/*
Copyright (c) 2008, The Multicore Association
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
(1) Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

(2) Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

(3) Neither the name of the Multicore Association nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/****************************************************************************
 File             : mcapi_trans_nios.c
 Author           : Eugen Harass
 Date             : 22.02.2013
 Decription       : MCAPI-transport-layer implementation for NIOS II
					(based on the MCA example shared memory implementation)

 Edition History:
 2013-02-22: MCAPI Implementation V0.1 based on MCAPI-specification V2.015
 2014-02-18: error in mcapi_trans_access_database_pre (database mutex
             wasn't really locked) found - ms (Manfred Strahnen)
 2014-03-20: optional database locked verification depending on macro
             DEBUG_LOCK_CHECK and modification in handling of locked
             variable (use of pthread_mutex_trylock) - ms
 2014-04-07: NS_layer_exit() included - ms
 2014-05-23: MCAPI_ERR_MEM_LIMIT debugging:
             changes in mcapi_trans_send_have_lock() which now returns
             the false reason. In case of MCAPI_ERR_MEM_LIMIT we could
             now handle this error by waiting until new memory will be
             available. This kind of error handling is now implemented in
             mcapi_trans_msg_send_i() and mcapi_trans_pktchan_send_i() - ms
 2014-06-05: NS_getRemoteEndpoint_request was used in a wrong way. It
             had to be checked against return code NS_OK - ms
 2014-06-16: home-brewed bug in mcapi_trans_endpoint_channel_isopen
             with introduction of PH_TCPSock_layer solved - ms
 2014-07-10: NS_init() parameters changed and debugging - ms
 2014-07-18: mcapi_trans_finalize() - DatabaseMutex destroyed - ms
 2014-07-29: NS_sendDataToRemote_request() call parameters changed,
             scalar value is now passed in former data field  - ms
 2014-07-31: scalar send/receive debugging - ms
 2014-08-05: v042 changes and ifdef adaptation to uC/OS-II - ms
 2014-08-06: changes in check_get_endpt_request_have_lock - ms
 2014-08-11: mcapi_env.h included - ms
 2014-09-30: Changes in mcapi_trans_pktchan_free() because of compiler
             dependent buggy pointer arithmetics - ms
***************************************************************************/

#ifdef __cplusplus
extern }
#endif /* __cplusplus */

#include "mcapi_trans.h" /* the transport API */
#include "mcapi_trans_nios.h"
#include "../NS_Layer/NS_layer.h"
#include "../MCAPI_Top/mcapi_env.h"

#include <errno.h>
#include <string.h> /* for memcpy */
#include <signal.h> /* for signals */
#include <unistd.h> /* for memset */
#include <assert.h> /* for assertions */
#include <stdio.h>

#ifdef UCOSII
	#include "includes.h"
#endif

#ifdef LINUX
	#include <pthread.h>
	#include <sched.h>
#endif

#define ms 10	// number of ms we have to wait until we try again

//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                   Constants and defines                                  //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
//#define	DEBUG_LOCK_CHECK	// if defined, at several places in code it will be
							// checked if data base is 'really' locked!

#define TICKS_TO_WAIT	1
#define MAGIC_NUM 0xdeadcafe
#define MCAPI_VALID_MASK 0x80000000
#define MSG_HEADER 1
#define MSG_PORT 0

//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                   Function prototypes (private)                          //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
mcapi_boolean_t mcapi_trans_access_database_pre (uint32_t handle,mcapi_boolean_t exclusive);

mcapi_boolean_t mcapi_trans_access_database_post (uint32_t handle,mcapi_boolean_t exclusive);

mcapi_boolean_t mcapi_trans_decode_request_handle(mcapi_request_t* request, uint16_t* r);

mcapi_boolean_t mcapi_trans_decode_handle (uint32_t handle, uint16_t* domain_index, uint16_t *node_index, uint16_t *endpoint_index);

uint32_t mcapi_trans_encode_handle (uint16_t domain_index, uint16_t node_index, uint16_t endpoint_index);

void mcapi_trans_signal_handler ( int sig );

//mcapi_boolean_t mcapi_trans_send (uint16_t sd, uint16_t sn,uint16_t se, uint16_t rd,uint16_t rn, uint16_t re, const char* buffer, size_t buffer_size, uint64_t scalar);
mcapi_boolean_t mcapi_trans_send (uint16_t sd, uint16_t sn,uint16_t se, uint16_t rd,uint16_t rn, uint16_t re, const char* buffer, size_t buffer_size);

int mcapi_trans_send_have_lock (uint16_t sd, uint16_t sn,uint16_t se, uint16_t rd, uint16_t rn,uint16_t re, const char* buffer, size_t buffer_size, uint64_t scalar);

mcapi_boolean_t mcapi_trans_recv_have_lock (uint16_t rd, uint16_t rn, uint16_t re, void** buffer, size_t buffer_size, size_t* received_size, mcapi_boolean_t blocking, uint64_t* scalar);

void mcapi_trans_recv_have_lock_ (uint16_t rd, uint16_t rn, uint16_t re, void** buffer, size_t buffer_size, size_t* received_size, int qindex, uint64_t* scalar);

mcapi_boolean_t mcapi_trans_endpoint_get_have_lock (mcapi_endpoint_t *e, mcapi_domain_t domain_id, mcapi_uint_t node_num, mcapi_uint_t port_num);

void mcapi_trans_open_channel_have_lock (uint16_t d,uint16_t n, uint16_t e);

void mcapi_trans_close_channel_have_lock (uint16_t d,uint16_t n, uint16_t e);

void mcapi_trans_connect_channel_have_lock (mcapi_endpoint_t send_endpoint, mcapi_endpoint_t receive_endpoint, channel_type type);

mcapi_boolean_t mcapi_trans_reserve_request_have_lock(int* r);

mcapi_boolean_t setup_request_have_lock (mcapi_endpoint_t* endpoint,
									     mcapi_request_t* request,
										 mcapi_status_t* mcapi_status,
										 mcapi_boolean_t completed,
										 size_t size,
										 void** buffer,
										 mcapi_request_type type,
										 mcapi_uint_t node_num,
										 mcapi_uint_t port_num,
										 mcapi_domain_t domain_num,
										 int r);

void check_receive_request_have_lock (mcapi_request_t *request);

void cancel_receive_request_have_lock (mcapi_request_t *request);

void check_get_endpt_request_have_lock (mcapi_request_t *request);

void check_open_channel_request_have_lock (mcapi_request_t *request);
void check_close_channel_request_have_lock (mcapi_request_t *request);
mcapi_boolean_t mcapi_trans_get_database_indices(uint32_t* d_index, uint32_t* n_index, mcapi_domain_t domain_id, mcapi_node_t node_id);

inline mcapi_boolean_t mcapi_trans_whoami (mcapi_node_t* node_id, uint32_t* n_index, mcapi_domain_t* domain_id, uint32_t* d_index);

void mcapi_trans_display_state_have_lock (void* handle);

void mcapi_trans_yield_have_lock();

mcapi_boolean_t mcapi_trans_add_domain_and_node (mcapi_domain_t domain_id, mcapi_node_t node_id, const mcapi_node_attributes_t* node_attrs);
mcapi_boolean_t mcapi_trans_valid_domain(mcapi_uint_t domain_num);

mcapi_boolean_t mcapi_trans_endpoint_exists_(mcapi_domain_t domain_id, uint32_t port_num);

void mcapi_trans_initialize_database();

/* queue management */
void print_queue (queue q);
int mcapi_trans_pop_queue (queue *q);
int mcapi_trans_push_queue (queue *q);
mcapi_boolean_t mcapi_trans_empty_queue (queue q);
mcapi_boolean_t mcapi_trans_full_queue (queue q);
mcapi_boolean_t mcapi_trans_full_queue_wait (queue q);
void mcapi_trans_compact_queue (queue* q);
mcapi_boolean_t mcapi_trans_endpoint_create_(mcapi_endpoint_t* ep, mcapi_domain_t domain_id, mcapi_node_t node_num, mcapi_uint_t port_num, mcapi_boolean_t anonymous);

#define mcapi_assert(x) MCAPI_ASSERT(x,__LINE__);
void MCAPI_ASSERT(mcapi_boolean_t condition,unsigned line)
{
	if (!condition) {
	  fprintf(stderr,"INTERNAL ERROR: MCAPI failed assertion (mcapi_trans_nios.c:%d) shutting down\n",line);
	  //mcapi_trans_signal_handler(SIGABRT);
	  exit(1);
	}
}

/***************************************************************************
Function: mca_dprintf

Description:

Parameters:

Returns: none
***************************************************************************/
#if MCA_WITH_DEBUG
inline void mcapi_dprintf(int level,const char *format, ...)
{
  if (MCA_WITH_DEBUG) {
    va_list ap;
    va_start(ap,format);
    if (level <= mcapi_debug){
      printf("MCAPI_DEBUG:");
      /* call variatic printf */
      vprintf(format,ap);
      printf("\n");
    }
    va_end(ap);
  }
}
#else
inline void mcapi_dprintf(int level,const char *format, ...) {}
#endif

//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                   Data                                                   //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

/* Database for store endpoint and channel information */
uint32_t global_rwl = 0; /* the global database lock */
#ifdef UCOSII
	OS_EVENT *DatabaseMutex;
#endif

#ifdef LINUX
	pthread_mutex_t DatabaseMutex;
#endif

mcapi_boolean_t locked = 0;

mcapi_database mcapi_db_impl;
mcapi_database* mcapi_db;

/* the debug level */
int mcapi_debug = 1;

/* global my node id number */
mcapi_uint_t my_node_id = 255;

/*global my domain id number */
mcapi_domain_t my_domain_id = 255;

/*global my database node index */
uint32_t my_mcapi_nindex = 0;

/*global my database domain index */
uint32_t my_mcapi_dindex = 0;

//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                   mcapi_trans API                                        //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
/***************************************************************************
  NAME: mcapi_trans_get_node_num
  DESCRIPTION: gets the node_num (not the transport's node index!)
  PARAMETERS: node_num: the node_num pointer to be filled in
  RETURN VALUE: boolean indicating success (the node num was found) or failure
   (couldn't find the node num).
***************************************************************************/
mcapi_boolean_t mcapi_trans_get_node_num(mcapi_uint_t* node)
{
	uint32_t d,n;
	mcapi_domain_t domain;

	return mcapi_trans_whoami(node,&n,&domain,&d);
}

/***************************************************************************
NAME: mcapi_trans_get_domain_num
DESCRIPTION: gets the domain_num (not the transport's node index!)
PARAMETERS: domain_num: the domain_num pointer to be filled in
RETURN VALUE: boolean indicating success (the node num was found) or failure
 (couldn't find the domain num).
***************************************************************************/
mcapi_boolean_t mcapi_trans_get_domain_num(mcapi_domain_t* domain)
{
  uint32_t d,n;
  mcapi_node_t node;

  return mcapi_trans_whoami(&node,&n,domain,&d);
}

/****************************_***********************************************
mcapi_trans_remove_request_have_lock
DESCRIPTION: Removes request from array
PARAMETERS:
r - request index
RETURN VALUE: TRUE/FALSE indicating if the request has removed.
***************************************************************************/
mcapi_boolean_t mcapi_trans_remove_request_have_lock(int r) {	/* by etem */
	int temp_empty_head_index;
	mcapi_boolean_t rc = MCAPI_FALSE;
	indexed_array_header *header = &mcapi_db->request_reserves_header;

	if (header->full_head_index != -1) {
		if (header->full_head_index == r) {
			header->full_head_index = header->array[header->full_head_index].next_index;
		}
		if (header->array[r].next_index != -1) {
			header->array[header->array[r].next_index].prev_index = header->array[r].prev_index;
		}
		if (header->array[r].prev_index != -1) {
			header->array[header->array[r].prev_index].next_index = header->array[r].next_index;
		}
		temp_empty_head_index = header->empty_head_index;
		header->empty_head_index = r;
		header->array[header->empty_head_index].next_index = temp_empty_head_index;
		if (temp_empty_head_index != -1) {
			header->array[temp_empty_head_index].prev_index = header->empty_head_index;
		}

		header->curr_count--;
		rc = MCAPI_TRUE;
	}
	return rc; // if rc=false, then there is no request available (array is empty)
}

/***************************************************************************
NAME: mcapi_trans_reserve_request
DESCRIPTION: Reserves an entry in the requests array
PARAMETERS: *r - request index pointer
RETURN VALUE: T/F
***************************************************************************/
mcapi_boolean_t mcapi_trans_reserve_request_have_lock(int *r) {	/* by etem */
	int temp_full_head_index;
	mcapi_boolean_t rc = MCAPI_FALSE;

	indexed_array_header *header = &mcapi_db->request_reserves_header;

	if (header->empty_head_index != -1) {
		*r = header->empty_head_index;
	  mcapi_db->requests[*r].valid = MCAPI_TRUE;
		temp_full_head_index = header->full_head_index;
		header->full_head_index = header->empty_head_index;
		header->empty_head_index = header->array[header->empty_head_index].next_index;
		header->array[header->empty_head_index].prev_index = -1;
		header->array[header->full_head_index].next_index = temp_full_head_index;
		header->array[header->full_head_index].prev_index = -1;
		if (temp_full_head_index != -1) {
			header->array[temp_full_head_index].prev_index = header->full_head_index;
		}
		header->curr_count++;
		rc = MCAPI_TRUE;
	}
	return rc;
}

/***************************************************************************
NAME: mcapi_trans_init_indexed_array_have_lock
DESCRIPTION: initializes indexed array
PARAMETERS:
RETURN VALUE: none
***************************************************************************/
void mcapi_trans_init_indexed_array_have_lock() {	/* by etem */
	int i;

	mcapi_db->request_reserves_header.curr_count = 0;
	mcapi_db->request_reserves_header.max_count = MCAPI_MAX_REQUESTS;
	mcapi_db->request_reserves_header.empty_head_index = 0;
	mcapi_db->request_reserves_header.full_head_index = -1;
	for (i = 0; i < MCAPI_MAX_REQUESTS; i++) {
		mcapi_db->request_reserves_header.array[i].next_index = i + 1;
		mcapi_db->request_reserves_header.array[i].prev_index = i - 1;
	}
	mcapi_db->request_reserves_header.array[MCAPI_MAX_REQUESTS - 1].next_index = -1;
	mcapi_db->request_reserves_header.array[0].prev_index = -1;
}

/***************************************************************************

  NAME: mcapi_trans_initialize    OK
  DESCRIPTION: initializes the transport layer
  PARAMETERS: node_num: the node number
  RETURN VALUE: boolean: success or failure
***************************************************************************/
mcapi_boolean_t mcapi_trans_initialize (mcapi_domain_t domain_id, mcapi_node_t node_id, const mcapi_node_attributes_t* node_attrs)
{
  mcapi_boolean_t rc = MCAPI_TRUE;
  locked = MCAPI_FALSE;

	// create mutex DatabaseMutex which ensures exclusive access to
	// database share among several threads
#ifdef UCOSII
  uint8_t	err;
  DatabaseMutex = OSMutexCreate(9, &err);
  if(err != OS_NO_ERR) {
	  printf("Error in mcapi_trans_nios: OSMutexCreate() failed");
	  return MCAPI_FALSE;
  }
#endif

#ifdef LINUX
  int err;
	if((err = pthread_mutex_init(&DatabaseMutex, NULL)) != 0) {
	  printf("Error in mcapi_trans_nios: pthread_mutex_init() failed");
	  return MCAPI_FALSE;
	}
#endif

  /* lock the database */
  if (!mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE)) { return MCAPI_FALSE; }

  /* initialize (invalidate) database entries */
  mcapi_trans_initialize_database();

  if (rc) {
    /* add the node */
    rc = mcapi_trans_add_domain_and_node(domain_id, node_id, node_attrs);
  }

  my_node_id = node_id;
  my_domain_id = domain_id;

  mcapi_assert(mcapi_trans_get_database_indices(&my_mcapi_dindex, &my_mcapi_nindex, my_domain_id, my_node_id) == MCAPI_TRUE);

  /* Network layer initialization */
  if(NS_init(my_domain_id, my_node_id) != NS_OK) {
	printf("mcapi_trans_initialize: NS_init() has reported an error!\n");
	fflush(stdout);
	rc = MCAPI_FALSE;	// indicate an error
  }

  /* unlock the database */
  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

  mcapi_dprintf(1, "mcapi_trans_initialize complete.  domain=%u, node=%u added", domain_id,node_id);
  return rc;
}

/***************************************************************************
  NAME: mcapi_trans_initialize_database
  DESCRIPTION: Initializes the global mcapi_database structure.
  PARAMETERS: -
  RETURN VALUE: -
***************************************************************************/
/**
 * NOTICE: The mcapi_database can be *huge* depending on the defines in
 * mcapi_config.h. If the processor's data memory is too small for the
 * database, this function *will* overwrite heap, stack and/or other
 * variables. You have been warned.
 */
void mcapi_trans_initialize_database() {

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	/* go through and set all nodes/domains/endpoints to invalid */
	int d, n, e = 0;

	mcapi_db = &mcapi_db_impl; //set database


	for (d = 0; d < MCA_MAX_DOMAINS; d++)
	{
		mcapi_db->domains[d].valid = MCAPI_FALSE;

		for (n = 0; n < MCA_MAX_NODES; n++)
		{
			mcapi_db->domains[d].nodes[n].valid = MCAPI_FALSE;

			for(e = 0; e < MCAPI_MAX_ENDPOINTS; e++)
			{
				mcapi_db->domains[d].nodes[n].node_d.endpoints[e].valid = MCAPI_FALSE;
			}
		}
	}
	/* init indexed array */
	mcapi_trans_init_indexed_array_have_lock();
}


/***************************************************************************
NAME:mcapi_trans_node_init_attributes
DESCRIPTION:
PARAMETERS:
RETURN VALUE:
***************************************************************************/
mcapi_boolean_t mcapi_trans_node_init_attributes(
                                      mcapi_node_attributes_t* mcapi_node_attributes,
                                      mcapi_status_t* mcapi_status
                                      ) {
  mcapi_boolean_t rc = MCAPI_TRUE;
  /* default values are all 0 */
  memset(mcapi_node_attributes,0,sizeof(mcapi_node_attributes));
  return rc;
}

/***************************************************************************
NAME:mcapi_trans_node_get_attribute
DESCRIPTION:
PARAMETERS:
RETURN VALUE:
***************************************************************************/
mcapi_boolean_t mcapi_trans_node_get_attribute(
                                               mcapi_domain_t domain_id,
                                               mcapi_node_t node_id,
                                               mcapi_uint_t attribute_num,
                                               void* attribute,
                                               size_t attribute_size,
                                               mcapi_status_t* mcapi_status)
{
	mcapi_boolean_t rc = MCAPI_FALSE;
	mcapi_boolean_t found_node = MCAPI_FALSE;
	mcapi_boolean_t found_domain = MCAPI_FALSE;
	uint32_t d,n;
	mcapi_domain_t my_domain_id;
	mcapi_node_t my_node_id;
	size_t size;

	if(domain_id == my_domain_id && node_id == my_node_id)
	{
		if (!mcapi_trans_whoami(&my_node_id,&n,&my_domain_id,&d)) {
		  *mcapi_status = MCAPI_ERR_NODE_NOTINIT;
		} else if (attribute_num != MCAPI_NODE_ATTR_TYPE_REGULAR) {
		  /* only the node_attr_type attribute is currently supported */
		  *mcapi_status = MCAPI_ERR_ATTR_NOTSUPPORTED;
		} else {
		  /* lock the database */
		  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

		  // look for the <domain,node>
		  for (d = 0; ((d < MCA_MAX_DOMAINS) && (found_domain == MCAPI_FALSE)); d++) {
			if (mcapi_db->domains[d].domain_id == domain_id) {
			  found_domain = MCAPI_TRUE;
			  for (n = 0; ((n < MCA_MAX_NODES) &&  (found_node == MCAPI_FALSE)); n++) {
				if (mcapi_db->domains[d].nodes[n].node_num == node_id) {
				  found_node = MCAPI_TRUE;
				  if (!mcapi_db->domains[d].valid) {
					*mcapi_status = MCAPI_ERR_DOMAIN_INVALID;
				  } else if (!mcapi_db->domains[d].nodes[n].valid) {
					*mcapi_status = MCAPI_ERR_NODE_INVALID;
				  } else {
					size = mcapi_db->domains[d].nodes[n].attributes.entries[attribute_num].bytes;
					 if (size != attribute_size) {
					  *mcapi_status = MCAPI_ERR_ATTR_SIZE;
					 } else {
					   memcpy(attribute,
						   &mcapi_db->domains[d].nodes[n].attributes.entries[attribute_num].attribute_d,
						   size);
					   rc = MCAPI_TRUE;
					}
				  }
				}
			  }
			}
		  }
		  if (!found_domain) {
			*mcapi_status = MCAPI_ERR_DOMAIN_INVALID;
		  } else if (!found_node) {
			*mcapi_status = MCAPI_ERR_NODE_INVALID;
		  }
		  /* unlock the database */
		  mcapi_assert (mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
		}
	}
	else
	{
		*mcapi_status = MCAPI_ERR_GENERAL;
		//TODO
	}

	return rc;
}

/***************************************************************************
NAME:mcapi_trans_node_set_attribute
DESCRIPTION:
PARAMETERS:
RETURN VALUE:
***************************************************************************/
mcapi_boolean_t mcapi_trans_node_set_attribute(
                                    mcapi_node_attributes_t* mcapi_node_attributes,
                                    mcapi_uint_t attribute_num,
                                    const void* attribute,
                                    size_t attribute_size,
                                    mcapi_status_t* mcapi_status
                                    )
{
  mcapi_boolean_t rc = MCAPI_FALSE;

  if (attribute_num != MCAPI_NODE_ATTR_TYPE_REGULAR) {
    /* only the node_attr_type attribute is currently supported */
    *mcapi_status = MCAPI_ERR_ATTR_NOTSUPPORTED;
  } else if (attribute_size != sizeof(mcapi_node_attr_type_t) ) {
    *mcapi_status = MCAPI_ERR_ATTR_SIZE;
  } else {
    rc = MCAPI_TRUE;
    /* copy the attribute into the attributes data structure */
    memcpy(&mcapi_node_attributes->entries[attribute_num].attribute_d,
           attribute,
           attribute_size);
  }
  return rc;
}

/***************************************************************************
  NAME:mcapi_trans_finalize FOO
  DESCRIPTION: cleans up the semaphore and shared memory resources.
  PARAMETERS: none
  RETURN VALUE: boolean: success or failure
***************************************************************************/
mcapi_boolean_t mcapi_trans_finalize ()
{
	mcapi_boolean_t rc = MCAPI_FALSE;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	if(mcapi_db != NULL)
	{
		mcapi_db = NULL; /* set mcapi_db-pointer to NULL */
		rc = MCAPI_TRUE;
	}

	/* unlock the database */
	mcapi_assert (mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	// finalize lower layer protocols
	NS_layer_exit();

#ifdef LINUX
	// destroy mutex DatabaseMutex
	// HAVE CARE: mutex should be unlocked !!!
	int err;
	if((err = pthread_mutex_destroy(&DatabaseMutex)) != 0) {
	  printf("Error in mcapi_trans_nios: pthread_mutex_init() failed");
	  return MCAPI_FALSE;
	}
#endif

	return rc;
}

//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                   mcapi_trans API: error checking routines               //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
/***************************************************************************
  NAME: mcapi_trans_channel_type OK
  DESCRIPTION: Given an endpoint, returns the type of channel (if any)
   associated with it.
  PARAMETERS: endpoint: the endpoint to be checked
  RETURN VALUE: the type of the channel (pkt,scalar or none)
***************************************************************************/
channel_type mcapi_trans_channel_type (mcapi_endpoint_t endpoint)
{
	uint16_t d,n,e;
	int rc;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_assert(mcapi_trans_decode_handle(endpoint,&d,&n,&e));
	rc = mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.channel_type;

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	return rc;
}

/***************************************************************************
  NAME:mcapi_trans_send_endpoint OK
  DESCRIPTION:checks if the given endpoint is a send endpoint
  PARAMETERS: endpoint: the endpoint to be checked
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_send_endpoint (mcapi_endpoint_t endpoint)
{
	uint16_t d,n,e;
	int rc = MCAPI_TRUE;

	mcapi_dprintf(2,"mcapi_trans_send_endpoint(0x%x);",endpoint);
	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_assert(mcapi_trans_decode_handle(endpoint,&d,&n,&e));
	if ((mcapi_db->domains[d].nodes[n].node_d.endpoints[e].connected) &&
		(mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.recv_endpt == endpoint)) {
	  /* this endpoint has already been marked as a receive endpoint */
	  mcapi_dprintf(2,"mcapi_trans_send_endpoint ERROR: this endpoint (0x%x) has already been connected as a receive endpoint",
					endpoint);
	  rc = MCAPI_FALSE;
	}

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	return rc;
}

/***************************************************************************
  NAME: mcapi_trans_recv_endpoint OK
  DESCRIPTION:checks if the given endpoint can be or is already a receive endpoint
  PARAMETERS: endpoint: the endpoint to be checked
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_recv_endpoint (mcapi_endpoint_t endpoint)
{
	uint16_t d,n,e;
	int rc = MCAPI_TRUE;

	mcapi_dprintf(2,"mcapi_trans_recv_endpoint(0x%x);",endpoint);

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_assert(mcapi_trans_decode_handle(endpoint,&d,&n,&e));
	if ((mcapi_db->domains[d].nodes[n].node_d.endpoints[e].connected) &&
		(mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.send_endpt == endpoint)) {
	  /* this endpoint has already been marked as a send endpoint */
	  mcapi_dprintf(2,"mcapi_trans_recv_endpoint ERROR: this endpoint (0x%x) has already been connected as a send endpoint",
					endpoint);
	  rc = MCAPI_FALSE;
	}

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	return rc;
}

/***************************************************************************
  NAME:mcapi_trans_valid_port OK
  DESCRIPTION:checks if the given port_num is a valid port_num for this system
  PARAMETERS: port_num: the port num to be checked
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_port(mcapi_uint_t port_num)
{
	/* port-ID does not depend on the endpoint-index */
	return MCAPI_TRUE;
}

/***************************************************************************
  NAME:mcapi_trans_valid_node   OK
  DESCRIPTION: checks if the given node_num is a valid node_num for this system
  PARAMETERS: node_num: the node num to be checked
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_node(mcapi_uint_t node_num)
{
	/* node-ID depends on the node-index */
	return (node_num < MCA_MAX_NODES ? MCAPI_TRUE : MCAPI_FALSE);
}

/***************************************************************************
  NAME:mcapi_trans_valid_node   OK
  DESCRIPTION: checks if the given node_num is a valid node_num for this system
  PARAMETERS: node_num: the node num to be checked
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_domain(mcapi_uint_t domain_num)
{
	/* domain-ID depends on the domain-index */
	return (domain_num < MCA_MAX_DOMAINS ? MCAPI_TRUE : MCAPI_FALSE);
}

/***************************************************************************
  NAME: mcapi_trans_valid_endpoint  OK
  DESCRIPTION: checks if the given endpoint handle refers to a valid endpoint
  PARAMETERS: endpoint
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_endpoint (mcapi_endpoint_t endpoint)
{
	uint16_t d,n,e;
	mcapi_boolean_t rc = MCAPI_FALSE;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	if (mcapi_trans_decode_handle(endpoint,&d,&n,&e) &&
		mcapi_db->domains[d].nodes[n].node_d.endpoints[e].valid)
	{
		rc = MCAPI_TRUE;
	}

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	return rc;
}

/***************************************************************************
  NAME: mcapi_trans_endpoint_exists OK
  DESCRIPTION: checks if an endpoint has been created for this port id
  PARAMETERS: domain id, port id
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_endpoint_exists (mcapi_domain_t domain_id, uint32_t port_num)
{
	uint32_t d = 0;
	uint32_t n = 0;
	uint32_t i;
	mcapi_node_t node_id;
	int rc = MCAPI_FALSE;

	if (port_num == MCAPI_PORT_ANY) {
	  return rc;
	}

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_assert(mcapi_trans_whoami(&node_id,&n,&domain_id,&d));

	/* Note: we can't just iterate for i < num_endpoints because endpoints can
	   be deleted which would fragment the endpoints array. */
	for (i = 0; i < MCAPI_MAX_ENDPOINTS; i++) {
	  if (mcapi_db->domains[d].nodes[n].node_d.endpoints[i].valid &&
		  mcapi_db->domains[d].nodes[n].node_d.endpoints[i].port_num == port_num) {
		rc = MCAPI_TRUE;
		break;
	  }
	}

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	return rc;
}

/***************************************************************************
  NAME: mcapi_trans_endpoint_exists_ OK
  DESCRIPTION: checks if an endpoint has been created for this port id
  PARAMETERS: domain id, node id, port id
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_endpoint_exists_ (mcapi_domain_t domain_id, uint32_t port_num)
{
	uint32_t d = 0;
	uint32_t n = 0;
	uint32_t i;
	mcapi_node_t node_id;
	int rc = MCAPI_FALSE;

	if (port_num == MCAPI_PORT_ANY) {
	  return rc;
	}

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	mcapi_assert(mcapi_trans_whoami(&node_id,&n,&domain_id,&d));

	/* Note: we can't just iterate for i < num_endpoints because endpoints can
	   be deleted which would fragment the endpoints array. */
	for (i = 0; i < MCAPI_MAX_ENDPOINTS; i++) {
	  if (mcapi_db->domains[d].nodes[n].node_d.endpoints[i].valid &&
		  mcapi_db->domains[d].nodes[n].node_d.endpoints[i].port_num == port_num) {
		rc = MCAPI_TRUE;
		break;
	  }
	}

	return rc;
}

/***************************************************************************
  NAME: mcapi_trans_valid_endpoints OK
  DESCRIPTION: checks if the given endpoint handles refer to valid endpoints
  PARAMETERS: endpoint1, endpoint2
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_endpoints (mcapi_endpoint_t endpoint1,
                                             mcapi_endpoint_t endpoint2)
{
	uint16_t d1,n1,e1;
	uint16_t d2,n2,e2;
	mcapi_boolean_t rc = MCAPI_FALSE;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	/*FIXME should endpoint handles be
	 * checked if the refer to valid endpoints
	 * when sending a message?
	 */
	if (mcapi_trans_decode_handle(endpoint1,&d1,&n1,&e1) &&
		/* c_db->domains[d1].nodes[n1].node_d.endpoints[e1].valid && */
		mcapi_trans_decode_handle(endpoint2,&d2,&n2,&e2) /* &&
		c_db->domains[d2].nodes[n2].node_d.endpoints[e2].valid */) {
	  rc = MCAPI_TRUE;
	}

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	return rc;
}

/***************************************************************************
  NAME:mcapi_trans_pktchan_recv_isopen  OK
  DESCRIPTION:checks if the channel is open for a given handle
  PARAMETERS: receive_handle
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_pktchan_recv_isopen (mcapi_pktchan_recv_hndl_t receive_handle)
{
	return mcapi_trans_endpoint_channel_isopen(receive_handle);
}


/***************************************************************************
  NAME:mcapi_trans_pktchan_send_isopen OK
  DESCRIPTION:checks if the channel is open for a given handle
  PARAMETERS: send_handle
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_pktchan_send_isopen (mcapi_pktchan_send_hndl_t send_handle)
{
	return mcapi_trans_endpoint_channel_isopen(send_handle);
}

/***************************************************************************
  NAME:mcapi_trans_sclchan_recv_isopen OK
  DESCRIPTION:checks if the channel is open for a given handle
  PARAMETERS: receive_handle
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_sclchan_recv_isopen (mcapi_sclchan_recv_hndl_t receive_handle)
{
	return mcapi_trans_endpoint_channel_isopen(receive_handle);
}

/***************************************************************************
  NAME:mcapi_trans_sclchan_send_isopen OK
  DESCRIPTION:checks if the channel is open for a given handle
  PARAMETERS: send_handle
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_sclchan_send_isopen (mcapi_sclchan_send_hndl_t send_handle)
{
	return mcapi_trans_endpoint_channel_isopen(send_handle);
}

/***************************************************************************
  NAME:mcapi_trans_endpoint_channel_isopen  OK
  DESCRIPTION:checks if a channel is open for a given endpoint
  PARAMETERS: endpoint
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_endpoint_channel_isopen (mcapi_endpoint_t endpoint)
{
	uint16_t d,n,e;
	int rc = MCAPI_FALSE;

	mcapi_assert(mcapi_trans_decode_handle(endpoint,&d,&n,&e));

	/* check if the endpoint node is the local node */
	if(d == my_domain_id && n == my_node_id)
	{
		/* lock the database */
		mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

		rc = (mcapi_db->domains[d].nodes[n].node_d.endpoints[e].open);

		/* unlock the database */
		mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

		return rc;
	}
	else
	{
		/* endpoint node is not the local node -> go to next layer */
		rc = NS_endpointChannelIsopen_request(endpoint);
		if(rc == NS_OK)	return MCAPI_TRUE;
		else			return MCAPI_FALSE;
	}

//	return rc;
}

/***************************************************************************
  NAME:mcapi_trans_endpoint_isowner  OK
  DESCRIPTION:checks if the given endpoint is owned by the calling node
  PARAMETERS: endpoint
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_endpoint_isowner (mcapi_endpoint_t endpoint)
{
	uint16_t d,n,e;
	mcapi_node_t node_num=0;
	int rc = MCAPI_FALSE;

	mcapi_assert(mcapi_trans_get_node_num(&node_num));

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_assert(mcapi_trans_decode_handle(endpoint,&d,&n,&e));
	rc = ((mcapi_db->domains[d].nodes[n].node_d.endpoints[e].valid) &&
		  (mcapi_db->domains[d].nodes[n].node_num == node_num));

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
	return rc;
}

/***************************************************************************
  NAME:mcapi_trans_channel_connected   OK
  DESCRIPTION:checks if the given endpoint channel is connected
  PARAMETERS: endpoint
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_channel_connected (mcapi_endpoint_t endpoint)
{
	uint16_t d,n,e;
	int rc = MCAPI_FALSE;

	mcapi_assert(mcapi_trans_decode_handle(endpoint,&d,&n,&e));

	/* check if the endpoint node is the local node */
	/* check is only needed on the local side because
	 * the channel synchronization will be done during
	 * channel opening and not here
	 */
	if(d == my_domain_id && n == my_node_id)
	{
		/* lock the database */
		mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

		rc = ((mcapi_db->domains[d].nodes[n].node_d.endpoints[e].valid) &&
			  (mcapi_db->domains[d].nodes[n].node_d.endpoints[e].connected));

		/* unlock the database */
		mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
	}

	return rc;
}

/***************************************************************************
  NAME:mcapi_trans_compatible_endpoint_attributes OK
  DESCRIPTION:checks if the given endpoints have compatible attributes
  PARAMETERS: send_endpoint,recv_endpoint
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_compatible_endpoint_attributes (mcapi_endpoint_t send_endpoint,
                                                             mcapi_endpoint_t recv_endpoint)
{
  /* FIXME: (errata A3) currently un-implemented */
	return MCAPI_TRUE;
}

/***************************************************************************
  NAME:mcapi_trans_valid_pktchan_send_handle Ok
  DESCRIPTION:checks if the given pkt channel send handle is valid
  PARAMETERS: handle
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_pktchan_send_handle( mcapi_pktchan_send_hndl_t handle)
{
	uint16_t d,n,e;
	channel_type type;

	int rc = MCAPI_FALSE;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_dprintf (2,"mcapi_trans_valid_pktchan_send_handle (0x%x);",handle);

	type =MCAPI_PKT_CHAN;
	if (mcapi_trans_decode_handle(handle,&d,&n,&e)) {
	  if (mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.channel_type == type) {
		rc = MCAPI_TRUE;
	  } else {
		mcapi_dprintf(2,"mcapi_trans_valid_pktchan_send_handle node=%u,port=%u returning false channel_type != MCAPI_PKT_CHAN",
					  mcapi_db->domains[d].nodes[n].node_num,
					  mcapi_db->domains[d].nodes[n].node_d.endpoints[e].port_num);
	  }
	}

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
	return rc;
}

/***************************************************************************
  NAME:mcapi_trans_valid_pktchan_recv_handle OK
  DESCRIPTION:checks if the given pkt channel recv handle is valid
  PARAMETERS: handle
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_pktchan_recv_handle( mcapi_pktchan_recv_hndl_t handle)
{
	uint16_t d,n,e;
	channel_type type;
	int rc = MCAPI_FALSE;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_dprintf (2,"mcapi_trans_valid_pktchan_recv_handle (0x%x);",handle);

	type = MCAPI_PKT_CHAN;
	if (mcapi_trans_decode_handle(handle,&d,&n,&e)) {
	  if (mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.channel_type == type) {
		rc = MCAPI_TRUE;
	  } else {
		mcapi_dprintf(2,"mcapi_trans_valid_pktchan_recv_handle node=%u,port=%u returning false channel_type != MCAPI_PKT_CHAN",
					  mcapi_db->domains[d].nodes[n].node_num,
					  mcapi_db->domains[d].nodes[n].node_d.endpoints[e].port_num);
	  }
	}

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
	return rc;
}

/***************************************************************************
  NAME:mcapi_trans_valid_sclchan_send_handle OK
  DESCRIPTION: checks if the given scalar channel send handle is valid
  PARAMETERS: handle
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_sclchan_send_handle( mcapi_sclchan_send_hndl_t handle)
{
	uint16_t d,n,e;
	channel_type type;
	int rc = MCAPI_FALSE;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_dprintf (2,"mcapi_trans_valid_sclchan_send_handle (0x%x);",handle);

	type = MCAPI_SCL_CHAN;
	if (mcapi_trans_decode_handle(handle,&d,&n,&e)) {
	  if (mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.channel_type == type) {
		rc = MCAPI_TRUE;
	  } else {
		mcapi_dprintf(2,"mcapi_trans_valid_sclchan_send_handle node=%u,port=%u returning false channel_type != MCAPI_SCL_CHAN",
					  mcapi_db->domains[d].nodes[n].node_num,
					  mcapi_db->domains[d].nodes[n].node_d.endpoints[e].port_num);
	  }
	}

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	return rc;
}

/***************************************************************************
  NAME:mcapi_trans_valid_sclchan_recv_handle OK
  DESCRIPTION:checks if the given scalar channel recv handle is valid
  PARAMETERS:
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_sclchan_recv_handle( mcapi_sclchan_recv_hndl_t handle)
{
	uint16_t d,n,e;
	channel_type type;

	int rc = MCAPI_FALSE;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_dprintf (2,"mcapi_trans_valid_sclchan_recv_handle (0x%x);",handle);

	type= MCAPI_SCL_CHAN;
	if (mcapi_trans_decode_handle(handle,&d,&n,&e)) {
	  if (mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.channel_type == type) {
		rc = MCAPI_TRUE;
	  } else {
		mcapi_dprintf(2,"mcapi_trans_valid_sclchan_recv_handle node=%u,port=%u returning false channel_type != MCAPI_SCL_CHAN",
					  mcapi_db->domains[d].nodes[n].node_num,
					  mcapi_db->domains[d].nodes[n].node_d.endpoints[e].port_num);
	  }
	}

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
	return rc;
}

/***************************************************************************
  NAME: mcapi_trans_initialized OK
  DESCRIPTION: checks if the given node_id has called initialize
  PARAMETERS: node_id
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_initialized()
{
	uint32_t d,n;
	mcapi_node_t node;
	mcapi_domain_t domain;
	mcapi_boolean_t rc = MCAPI_FALSE;

	mcapi_dprintf (1,"mcapi_trans_initialized();");

	/* lock the database */
	if (!mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE)) { return MCAPI_FALSE; }

	rc = mcapi_trans_whoami(&node,&n,&domain,&d);

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	return rc;
}

/***************************************************************************
  NAME: mcapi_trans_num_endpoints OK
  DESCRIPTION: returns the current number of endpoints for the calling node
  PARAMETERS:  none
  RETURN VALUE: num_endpoints
***************************************************************************/
mcapi_uint32_t mcapi_trans_num_endpoints()
{
	uint32_t d = 0;
	uint32_t n = 0;
	uint32_t rc = 0;
	mcapi_node_t node_id=0;
	mcapi_domain_t domain_id=0;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_assert(mcapi_trans_whoami(&node_id,&n,&domain_id,&d));

	mcapi_dprintf (2,"mcapi_trans_num_endpoints (0x%x);",domain_id);

	rc = mcapi_db->domains[d].nodes[n].node_d.num_endpoints;

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	return rc;
}

/***************************************************************************
  NAME:mcapi_trans_valid_priority OK
  DESCRIPTION:checks if the given priority level is valid
  PARAMETERS: priority
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_priority(mcapi_priority_t priority)
{
  return ((priority >=0) && (priority <= MCAPI_MAX_PRIORITY));
}

/***************************************************************************
  NAME:mcapi_trans_connected OK
  DESCRIPTION: checks if the given endpoint is connected
  PARAMETERS: endpoint
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_connected(mcapi_endpoint_t endpoint)
{
	mcapi_boolean_t rc = MCAPI_FALSE;
	uint16_t d,n,e;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_dprintf (2,"mcapi_trans_connected (0x%x);",endpoint);

	rc = (mcapi_trans_decode_handle(endpoint,&d,&n,&e) &&
		 (mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.channel_type != MCAPI_NO_CHAN));
	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
	return rc;
}

/***************************************************************************
  NAME:valid_status_param
  DESCRIPTION: checks if the given status is a valid status parameter
  PARAMETERS: status
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_status_param (mcapi_status_t* mcapi_status)
{
  return (mcapi_status != NULL);
}

/***************************************************************************
  NAME:valid_version_param OK
  DESCRIPTION: checks if the given version is a valid version parameter
  PARAMETERS: version
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_version_param (mcapi_info_t* mcapi_version)
{
  return (mcapi_version != NULL);
}

/***************************************************************************
  NAME:valid_buffer_param OK
  DESCRIPTION:checks if the given buffer is a valid buffer parameter
  PARAMETERS: buffer
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_buffer_param (void* buffer)
{
  return (buffer != NULL);
}


/***************************************************************************
NAME: mcapi_trans_valid_request_handle
DESCRIPTION:checks if the given request handle is valid
      This is the parameter test/wait/cancel pass in to be processed.
PARAMETERS: request
RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_request_handle (mcapi_request_t* request)
{

  uint16_t r;
  return (mcapi_trans_decode_request_handle(request,&r));
}


/***************************************************************************
NAME:mcapi_trans_valid_size_param
DESCRIPTION: checks if the given size is a valid size parameter
PARAMETERS: size
RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE
***************************************************************************/
mcapi_boolean_t mcapi_trans_valid_size_param (size_t* size)
{
  return (size != NULL);
}

//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                   mcapi_trans API: endpoints                             //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
/***************************************************************************
NAME:mcapi_trans_endpoint_create
DESCRIPTION:create endpoint <node_num,port_num> and return it's handle
PARAMETERS:
     ep - the endpoint to be filled in
     port_num - the port id (only valid if !anonymous)
     anonymous - whether or not this should be an anonymous endpoint
RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE indicating success or failure
***************************************************************************/
mcapi_boolean_t mcapi_trans_endpoint_create (mcapi_endpoint_t* ep,
                                             mcapi_uint_t port_num,
                                             mcapi_boolean_t anonymous)
{

	uint32_t domain_index = 0;
	uint32_t node_index = 0;
	uint32_t i, endpoint_index;
	mcapi_boolean_t rc = MCAPI_FALSE;
	mcapi_domain_t domain_id2;
	mcapi_node_t node_num2;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_dprintf (1,"mcapi_trans_endpoint_create (0x%x,%u);",port_num,anonymous);

	mcapi_assert(mcapi_trans_whoami(&node_num2,&node_index,&domain_id2,&domain_index));


	/* make sure there's room - mcapi should have already checked this */
	mcapi_assert (mcapi_db->domains[domain_index].nodes[node_index].node_d.num_endpoints < MCAPI_MAX_ENDPOINTS);

	/* create the endpoint */
	/* find the first available endpoint index */
	endpoint_index = MCAPI_MAX_ENDPOINTS;
	for (i = 0; i < MCAPI_MAX_ENDPOINTS; i++) {
		if (! mcapi_db->domains[domain_index].nodes[node_index].node_d.endpoints[i].valid) {
			endpoint_index = i;
			//printf("found index: %i\n", endpoint_index);
			break;
	  }
	}

	if (endpoint_index < MCAPI_MAX_ENDPOINTS) {
	  /* initialize the endpoint entry*/
		mcapi_db->domains[domain_index].nodes[node_index].node_d.endpoints[endpoint_index].valid = MCAPI_TRUE;
		mcapi_db->domains[domain_index].nodes[node_index].node_d.endpoints[endpoint_index].port_num = port_num;
		mcapi_db->domains[domain_index].nodes[node_index].node_d.endpoints[endpoint_index].open = MCAPI_FALSE;
		mcapi_db->domains[domain_index].nodes[node_index].node_d.endpoints[endpoint_index].anonymous = anonymous;
		mcapi_db->domains[domain_index].nodes[node_index].node_d.endpoints[endpoint_index].num_attributes = 0;
		mcapi_db->domains[domain_index].nodes[node_index].node_d.num_endpoints++;

		/* set the handle */
		*ep = mcapi_trans_encode_handle (domain_index,node_index,endpoint_index);
		rc = MCAPI_TRUE;
	}

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	return rc;
}

/***************************************************************************
NAME:mcapi_trans_endpoint_create_
DESCRIPTION:create endpoint <domain_id,node_num,port_num> and return it's handle
PARAMETERS:
     ep - the endpoint to be filled in
     domain_id - the domain id
     node_num - the node id
     port_num - the port id
     anonymous - whether or not this should be an anonymous endpoint
RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE indicating success or failure
***************************************************************************/
mcapi_boolean_t mcapi_trans_endpoint_create_(mcapi_endpoint_t* ep, mcapi_domain_t domain_id, mcapi_node_t node_num, mcapi_uint_t port_num, mcapi_boolean_t anonymous)
{
	uint32_t i, endpoint_index;
	mcapi_boolean_t rc = MCAPI_FALSE;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	//mcapi_dprintf (1,"mcapi_trans_endpoint_create (0x%x,%u);",port_num,anonymous);

	mcapi_assert(mcapi_trans_valid_domain(domain_id) == MCAPI_TRUE);
	mcapi_assert(mcapi_trans_valid_node(node_num) == MCAPI_TRUE);

	/* make sure there's room - mcapi should have already checked this */
	mcapi_assert (mcapi_db->domains[domain_id].nodes[node_num].node_d.num_endpoints < MCAPI_MAX_ENDPOINTS);

	/* create the endpoint */
	/* find the first available endpoint index */
	endpoint_index = MCAPI_MAX_ENDPOINTS;
	for (i = 0; i < MCAPI_MAX_ENDPOINTS; i++) {
		if (! mcapi_db->domains[domain_id].nodes[node_num].node_d.endpoints[i].valid) {
			endpoint_index = i;
			break;
	  }
	}

	if (endpoint_index < MCAPI_MAX_ENDPOINTS) {
	  /* initialize the endpoint entry*/
		mcapi_db->domains[domain_id].nodes[node_num].node_d.endpoints[endpoint_index].valid = MCAPI_TRUE;
		mcapi_db->domains[domain_id].nodes[node_num].node_d.endpoints[endpoint_index].port_num = port_num;
		mcapi_db->domains[domain_id].nodes[node_num].node_d.endpoints[endpoint_index].open = MCAPI_FALSE;
		mcapi_db->domains[domain_id].nodes[node_num].node_d.endpoints[endpoint_index].anonymous = anonymous;
		mcapi_db->domains[domain_id].nodes[node_num].node_d.endpoints[endpoint_index].num_attributes = 0;
		mcapi_db->domains[domain_id].nodes[node_num].node_d.num_endpoints++;

		/* set the handle */
		*ep = mcapi_trans_encode_handle (domain_id,node_num,endpoint_index);

		rc = MCAPI_TRUE;
	}

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	return rc;
}

/***************************************************************************
NAME:mcapi_trans_endpoint_get_attribute
DESCRIPTION:
PARAMETERS:
RETURN VALUE: none
***************************************************************************/
void mcapi_trans_endpoint_get_attribute(
                                        mcapi_endpoint_t endpoint,
                                        mcapi_uint_t attribute_num,
                                        void* attribute,
                                        size_t attribute_size,
                                        mcapi_status_t* mcapi_status)
{
	uint16_t d,n,e;
	int* attr = attribute;

	*mcapi_status = MCAPI_ERR_ATTR_NUM;

	mcapi_assert(mcapi_trans_decode_handle(endpoint,&d,&n,&e));

	if(d == my_domain_id && n == my_node_id)
	{
		/* lock the database */
		mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

		if (attribute_num == MCAPI_ENDP_ATTR_NUM_RECV_BUFFERS) {
		  *attr = MCAPI_MAX_QUEUE_ENTRIES -
				  mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.num_elements;
		  *mcapi_status = MCAPI_SUCCESS;
		}

		/* unlock the database */
		mcapi_assert (mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
	}
	else
	{
		*mcapi_status = MCAPI_ERR_GENERAL;
		//TODO
	}
}

/***************************************************************************
NAME:mcapi_trans_endpoint_set_attribute
DESCRIPTION:
PARAMETERS:
RETURN VALUE: none
***************************************************************************/
void mcapi_trans_endpoint_set_attribute(
                                        mcapi_endpoint_t endpoint,
                                        mcapi_uint_t attribute_num,
                                        const void* attribute,
                                        size_t attribute_size,
                                        mcapi_status_t* mcapi_status)
{

  fprintf(stderr,"WARNING: setting endpoint attributes not implemented");
}


/***************************************************************************
NAME:mcapi_trans_endpoint_get_i
DESCRIPTION:non-blocking get endpoint for the given <node_num,port_num>
PARAMETERS:
   endpoint - the endpoint handle to be filled in
   node_num - the node id
   port_num - the port id
   request - the request handle to be filled in when the task is complete
RETURN VALUE: none
***************************************************************************/
void mcapi_trans_endpoint_get_i( mcapi_endpoint_t* endpoint,
                                 mcapi_domain_t domain_id,
                                 mcapi_uint_t node_num,
                                 mcapi_uint_t port_num,
                                 mcapi_request_t* request,
                                 mcapi_status_t* mcapi_status)
{
	mcapi_boolean_t valid =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_TRUE : MCAPI_FALSE;
	mcapi_boolean_t completed = MCAPI_FALSE;
	int r;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	/* make sure we have an available request entry*/
	if ( mcapi_trans_reserve_request_have_lock(&r)) {
	  if (valid) {
		/* try to get the endpoint */
		/* check if the endpoint node is the local node */
		if(domain_id == my_domain_id && node_num == my_node_id)
		{
			if (mcapi_trans_endpoint_get_have_lock (endpoint,domain_id,node_num,port_num)) {
			  completed = MCAPI_TRUE; /* endpoint received -> reqeuest completed */
			}
		}
		else
		{
			/* endpoint node is not the local node -> go to the nex layer */
			/* unlock the database */
			mcapi_assert (mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

			if (NS_getRemoteEndpoint_request(endpoint,domain_id,node_num,port_num) == NS_OK) {
			  completed = MCAPI_TRUE; /* endpoint received -> request completed */
			}

			/* lock the database */
			mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));
		}
	  }

	  /* setup the reqeuest depend on the current state */
	  mcapi_assert(setup_request_have_lock(endpoint,request,mcapi_status,completed,0,NULL,GET_ENDPT,node_num,port_num,domain_id,r));
	}

	/* unlock the database */
	mcapi_assert (mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
}

/***************************************************************************
NAME:mcapi_trans_endpoint_get_have_lock
DESCRIPTION:get endpoint for the given <node_num,port_num>
PARAMETERS:
   endpoint - the endpoint handle to be filled in
   node_num - the node id
   port_num - the port id
RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE indicating success or failure
***************************************************************************/
mcapi_boolean_t mcapi_trans_endpoint_get_have_lock (mcapi_endpoint_t *ep,
                                                   mcapi_domain_t domain_id,
                                                   mcapi_uint_t node_num,
                                                   mcapi_uint_t port_num)
{
	uint32_t d,n,e;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	mcapi_dprintf(3,"mcapi_trans_endpoint_get_have_lock(&ep,%u,%u,%u);",domain_id,node_num,port_num);

	// look for the endpoint <domain,node,port>
	for (d = 0; d < MCA_MAX_DOMAINS; d++) {
	  if (mcapi_db->domains[d].valid &&
		  (mcapi_db->domains[d].domain_id == domain_id)) {
		for (n = 0; n < MCA_MAX_NODES; n++) {
		  if (mcapi_db->domains[d].nodes[n].valid &&
			  (mcapi_db->domains[d].nodes[n].node_num == node_num)) {
			// iterate over all the endpoints on this node
			for (e = 0; e < mcapi_db->domains[d].nodes[n].node_d.num_endpoints; e++) {
			  if (mcapi_db->domains[d].nodes[n].node_d.endpoints[e].valid &&
				  (mcapi_db->domains[d].nodes[n].node_d.endpoints[e].port_num == port_num)) {
				/* we found it, return the handle */
				*ep = mcapi_trans_encode_handle (d,n,e);
				return MCAPI_TRUE;
			  }
			}
		  }
		}
	  }
	}

	return MCAPI_FALSE;
}

/***************************************************************************
NAME:mcapi_trans_endpoint_get_
DESCRIPTION:get endpoint for the given <node_num,port_num>
PARAMETERS:
   endpoint - the endpoint handle to be filled in
   node_num - the node id
   port_num - the port id
RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE indicating success or failure
***************************************************************************/
mcapi_boolean_t mcapi_trans_endpoint_get_ (mcapi_endpoint_t *ep,
                                                   mcapi_domain_t domain_id,
                                                   mcapi_uint_t node_num,
                                                   mcapi_uint_t port_num)
{
	uint32_t d,n,e;
	mcapi_boolean_t rc = MCAPI_FALSE;

	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_dprintf(3,"mcapi_trans_endpoint_get_have_lock(&ep,%u,%u,%u);",domain_id,node_num,port_num);

	// look for the endpoint <domain,node,port>
	for (d = 0; d < MCA_MAX_DOMAINS && rc != MCAPI_TRUE; d++) {
	  if (mcapi_db->domains[d].valid &&
		  (mcapi_db->domains[d].domain_id == domain_id)) {
		for (n = 0; n < MCA_MAX_NODES && rc != MCAPI_TRUE; n++) {
		  if (mcapi_db->domains[d].nodes[n].valid &&
			  (mcapi_db->domains[d].nodes[n].node_num == node_num)) {
			// iterate over all the endpoints on this node
			for (e = 0; e < mcapi_db->domains[d].nodes[n].node_d.num_endpoints && rc != MCAPI_TRUE; e++) {
			  if (mcapi_db->domains[d].nodes[n].node_d.endpoints[e].valid &&
				  (mcapi_db->domains[d].nodes[n].node_d.endpoints[e].port_num == port_num)) {
				/* we found it, return the handle */

				*ep = mcapi_trans_encode_handle (d,n,e);

				rc = MCAPI_TRUE;
			  }
			}
		  }
		}
	  }
	}

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	return rc;
}

/***************************************************************************
NAME:mcapi_trans_endpoint_get
DESCRIPTION:blocking get endpoint for the given <node_num,port_num>
PARAMETERS:
   endpoint - the endpoint handle to be filled in
   node_num - the node id
   port_num - the port id
RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE indicating success or failure
***************************************************************************/
void mcapi_trans_endpoint_get(mcapi_endpoint_t *e,
                              mcapi_domain_t domain_id,
                              mcapi_uint_t node_num,
                              mcapi_uint_t port_num)
{
	mcapi_request_t request;
	mcapi_status_t status = MCAPI_SUCCESS;
	size_t size;

	/* use non-blocking followed by wait */

	mcapi_trans_endpoint_get_i(e, domain_id, node_num, port_num, &request, &status);
	mcapi_trans_wait (&request,&size,&status,MCA_INFINITE);
}

/***************************************************************************
NAME: mcapi_trans_endpoint_delete
DESCRIPTION:delete the given endpoint
PARAMETERS: endpoint
RETURN VALUE: none
***************************************************************************/
void mcapi_trans_endpoint_delete( mcapi_endpoint_t endpoint)
{
	uint16_t d,n,e;

	mcapi_dprintf(1,"mcapi_trans_endpoint_delete(0x%x);",endpoint);
	/* lock the database */
	mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	mcapi_assert(mcapi_trans_decode_handle(endpoint,&d,&n,&e));

	mcapi_dprintf(2,"mcapi_trans_endpoint_delete_have_lock node_num=%u, port_num=%u",
				  mcapi_db->domains[d].nodes[n].node_num,mcapi_db->domains[d].nodes[n].node_d.endpoints[e].port_num);

	/* remove the endpoint */
	mcapi_db->domains[d].nodes[n].node_d.num_endpoints--;
	/* zero out the old endpoint entry in the shared memory database */
	memset (&mcapi_db->domains[d].nodes[n].node_d.endpoints[e],0,sizeof(endpoint_entry));

	/* unlock the database */
	mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
}


//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                   mcapi_trans API: messages                              //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
/***************************************************************************
  NAME: mcapi_trans_msg_send
  DESCRIPTION: sends a connectionless message from one endpoint to another (blocking)
  PARAMETERS:
     send_endpoint - the sending endpoint's handle
     receive_endpoint - the receiving endpoint's handle
     buffer - the user supplied buffer
     buffer_size - the size in bytes of the buffer
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE indicating success or failure
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_msg_send( mcapi_endpoint_t  send_endpoint,
                                        mcapi_endpoint_t  receive_endpoint,
                                        const char* buffer,
                                        size_t buffer_size)
  {
	  mcapi_request_t request;
	  mcapi_status_t status = MCAPI_SUCCESS;
	  mcapi_status_t status1 = MCAPI_SUCCESS;
	  size_t size;

	  /* use non-blocking followed by wait */
	  do {
		status = MCAPI_SUCCESS;
		mcapi_trans_msg_send_i (send_endpoint,receive_endpoint,buffer,buffer_size,&request,&status);
//	  } while ( status == MCAPI_ERR_REQUEST_LIMIT);
	  } while ((status == MCAPI_ERR_REQUEST_LIMIT) || (status == MCAPI_ERR_MEM_LIMIT));

	  status1 = status;
	  mcapi_trans_wait (&request,&size,&status,MCA_INFINITE);

	  if (status == MCAPI_SUCCESS) {
		return MCAPI_TRUE;
	  }
	  printf("mcapi_trans_msg_send() return status = %d, %d\n", status1, status);
	  return MCAPI_FALSE;
  }



  /***************************************************************************
  NAME: mcapi_trans_msg_send_i
  DESCRIPTION: sends a connectionless message from one endpoint to another (non-blocking)
  PARAMETERS:
     send_endpoint - the sending endpoint's handle
     receive_endpoint - the receiving endpoint's handle
     buffer - the user supplied buffer
     buffer_size - the size in bytes of the buffer
     request - the request handle to be filled in when the task is complete
  RETURN VALUE:none
  ***************************************************************************/
  void mcapi_trans_msg_send_i( mcapi_endpoint_t  send_endpoint,
                               mcapi_endpoint_t  receive_endpoint,
                               const char* buffer,
                               size_t buffer_size,
                               mcapi_request_t* request,
                               mcapi_status_t* mcapi_status)
  {
	  int r;
	  uint16_t sd,sn,se;
	  uint16_t rd,rn,re;
	  int err;

	  /* if errors were found at the mcapi layer, then the request is considered complete */
	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;
	  mcapi_dprintf(1,"mcapi_trans_msg_send_i (0x%x,0x%x,buffer,%u,&request,&status);",send_endpoint,receive_endpoint,buffer_size);

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  /* make sure we have an available request entry*/
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {	// a request will be reserved!!!
		if (!completed) {
		  completed = MCAPI_TRUE; /* sends complete immediately */
		  mcapi_assert(mcapi_trans_decode_handle(send_endpoint,&sd,&sn,&se));
		  mcapi_assert(mcapi_trans_decode_handle(receive_endpoint,&rd,&rn,&re));

		  /* check if receive endpoint belongs to the current node */
		  if(rd == my_domain_id && rn == my_node_id)
		  {
			  mcapi_assert (mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.channel_type == MCAPI_NO_CHAN);
//			  if (!mcapi_trans_send_have_lock (sd,sn,se,rd,rn,re,buffer,buffer_size,0)) {
				/* assume couldn't get a buffer */
//				*mcapi_status = MCAPI_ERR_MEM_LIMIT;
//			  }

			  // Try to send the message. Only possible error case should be that
			  // there is not enough memory (send buffers or queue entries). In that
			  // case we will wait until memory will be available.
			  while ((err = mcapi_trans_send_have_lock (sd,sn,se,rd,rn,re,buffer,buffer_size,0)) != MCAPI_SUCCESS) {
				if(err == MCAPI_ERR_MEM_LIMIT) {
					// unlock data base, so that other threads could proceed
					mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
					// let other tasks do there job first
#ifdef UCOSII
					OSTimeDly(TICKS_TO_WAIT);
#endif

#ifdef LINUX
					sched_yield();
#endif
					// lock data base again before next try
					mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));
				}
				else {
					printf("mcapi_tans_msg_send_i(): unexpected error MCAPI_ERR_CHAN_NCNO\n");
					*mcapi_status = MCAPI_ERR_CHAN_NCNO;
					// unlock the data base
					mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
					return;
				}
			  }
			  *mcapi_status = MCAPI_SUCCESS;
		  }
		  else
		  {
			  mcapi_assert (mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.channel_type == MCAPI_NO_CHAN);

			  /* unlock the database */
			  	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

			  /* receive endpoint does not belong to the local node -> go to the next layer */
			  if (NS_sendDataToRemote_request(send_endpoint, receive_endpoint, (char *) buffer, buffer_size) != NS_OK) {
				  *mcapi_status = MCAPI_ERR_MEM_LIMIT;
			  }

			  /* lock the database */
			  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));
		  }
		}
		/* setup the reqeuest depend on the current state */
		mcapi_assert(setup_request_have_lock(&receive_endpoint,request,mcapi_status,completed,buffer_size,NULL,SEND,0,0,0,r));
		/* the non-blocking request succeeded, when they call test/wait they will see the status of the send */
		*mcapi_status = MCAPI_SUCCESS;
	  } else {
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }

  /***************************************************************************
  NAME:mcapi_trans_msg_recv
  DESCRIPTION:receives a message from this endpoints receive queue (blocking)
  PARAMETERS:
     receive_endpoint - the receiving endpoint
     buffer - the user supplied buffer to copy the message into
     buffer_size - the size of the user supplied buffer
     received_size - the actual size of the message received
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE indicating success or failure
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_msg_recv( mcapi_endpoint_t  receive_endpoint,
                                        char* buffer,
                                        size_t buffer_size,
                                        size_t* received_size)
  {
	  mcapi_request_t request;
	  mcapi_status_t status = MCAPI_SUCCESS;

	  mcapi_dprintf(2,"mcapi_trans_msg_recv re=0x%x",receive_endpoint);

	  /* use non-blocking followed by wait */
	  do {
		mcapi_trans_msg_recv_i (receive_endpoint,buffer,buffer_size,&request,&status);
	  } while (status == MCAPI_ERR_REQUEST_LIMIT);

	  mcapi_trans_wait (&request,received_size,&status,MCA_INFINITE);

	  if (status == MCAPI_SUCCESS) {
		return MCAPI_TRUE;
	  }
	  return MCAPI_FALSE;
  }

  /***************************************************************************
  NAME:mcapi_trans_msg_recv_i
  DESCRIPTION:receives a message from this endpoints receive queue (non-blocking)
  PARAMETERS:
     receive_endpoint - the receiving endpoint
     buffer - the user supplied buffer to copy the message into
     buffer_size - the size of the user supplied buffer
     received_size - the actual size of the message received
     request - the request to be filled in when the task is completed.
  RETURN VALUE:MCAPI_TRUE/MCAPI_FALSE indicating success or failure
  ***************************************************************************/
  void mcapi_trans_msg_recv_i( mcapi_endpoint_t  receive_endpoint,  char* buffer,
                               size_t buffer_size, mcapi_request_t* request,
                               mcapi_status_t* mcapi_status)
  {
	  uint16_t rd,rn,re;
	  int r;
	  size_t received_size = 0;
	  /* if errors were found at the mcapi layer, then the request is considered complete */
	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_dprintf(1,"mcapi_trans_msg_recv_i(0x%x,buffer,%u,&request,&status);",receive_endpoint,buffer_size);

	  /* make sure we have a request entry */
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {
		if (!completed) {
		  mcapi_assert(mcapi_trans_decode_handle(receive_endpoint,&rd,&rn,&re));

		  mcapi_assert (mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.channel_type == MCAPI_NO_CHAN);

		  if (mcapi_trans_recv_have_lock(rd,rn,re,(void*)&buffer,buffer_size,&received_size,MCAPI_FALSE,NULL)) {
			completed = MCAPI_TRUE;
			buffer_size = received_size;
		  }
		}
		/* setup the reqeuest depend on the current state */

		mcapi_assert(setup_request_have_lock(&receive_endpoint,request,mcapi_status,completed,buffer_size,(void**)((void*)&buffer),RECV,0,0,0,r));
	  } else {
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }
	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }

  /***************************************************************************
  NAME: mcapi_trans_msg_available
  DESCRIPTION: counts the number of messages in the endpoints receive queue
  PARAMETERS:  endpoint
  RETURN VALUE: the number of messages in the queue
  ***************************************************************************/
  mcapi_uint_t mcapi_trans_msg_available( mcapi_endpoint_t receive_endpoint)
  {
	  uint16_t rd,rn,re;
	  int rc = MCAPI_FALSE;

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_dprintf(1,"mcapi_trans_msg_available(0x%x);",receive_endpoint);
	  mcapi_assert(mcapi_trans_decode_handle(receive_endpoint,&rd,&rn,&re));
	  rc = mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.num_elements;

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	  return rc;
  }

  //////////////////////////////////////////////////////////////////////////////
  //                                                                          //
  //                   mcapi_trans API: packet channels                       //
  //                                                                          //
  //////////////////////////////////////////////////////////////////////////////
  /***************************************************************************
  NAME:mcapi_trans_pktchan_connect_i
  DESCRIPTION: connects a packet channel
  PARAMETERS:
    send_endpoint - the sending endpoint handle
    receive_endpoint - the receiving endpoint handle
    request - the request to be filled in when the task is complete
    mcapi_status -
  RETURN VALUE:none
  ***************************************************************************/
  void mcapi_trans_pktchan_connect_i( mcapi_endpoint_t  send_endpoint,
  									mcapi_endpoint_t  receive_endpoint,
  									mcapi_request_t* request,
  									mcapi_status_t* mcapi_status)
  {
	  int r;
	  /* if errors were found at the mcapi layer, then the request is considered complete */
	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  /* make sure we have a request entry */
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {
		if (!completed) {
		  mcapi_trans_connect_channel_have_lock (send_endpoint,receive_endpoint,MCAPI_PKT_CHAN);
		  completed = MCAPI_TRUE;
		}
		/* setup the reqeuest depend on the current state */
		mcapi_assert(setup_request_have_lock(&receive_endpoint,request,mcapi_status,completed,0,NULL,0,0,0,0,r));
	  } else {
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }

  /***************************************************************************
  NAME: mcapi_trans_pktchan_recv_open_i
  DESCRIPTION: opens the receive endpoint on a packet channel
  PARAMETERS:
    recv_handle - the receive channel handle to be filled in
    receive_endpoint - the receiving endpoint handle
    request - the request to be filled in when the task is complete
    mcapi_status
  RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_pktchan_recv_open_i( mcapi_pktchan_recv_hndl_t* recv_handle,
  									  mcapi_endpoint_t receive_endpoint,
  									  mcapi_request_t* request,
  									  mcapi_status_t* mcapi_status)
  {
	  int r;
	  uint16_t rd,rn,re;
	  /* if errors were found at the mcapi layer, then the request is considered complete */
	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;

	  mcapi_assert(mcapi_trans_decode_handle(receive_endpoint,&rd,&rn,&re));

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_endpoint_t send_endpoint = mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.send_endpt;

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	  /* get the open-state of the other side */
	  mcapi_boolean_t is_channel_open_on_send_side = mcapi_trans_endpoint_channel_isopen(send_endpoint);

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_dprintf(1,"mcapi_trans_pktchan_recv_open_i (recv_handle,0x%x,&request,&status);",receive_endpoint);

	  /* make sure we have a request entry */
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {
		if (!completed) {
		  /* mark the endpoint as open */
		  mcapi_trans_open_channel_have_lock (rd,rn,re);

		  /* fill in the channel handle */
		  *recv_handle = mcapi_trans_encode_handle(rd,rn,re);


		  /* has the channel been connected yet? */
		  if ( is_channel_open_on_send_side ) {
			completed = MCAPI_TRUE;
		  }

		  mcapi_dprintf(2,"mcapi_trans_pktchan_recv_open_i (node_num=%u,port_num=%u) handle=0x%x",
						  mcapi_db->domains[rd].nodes[rn].node_num,
						  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].port_num,
						*recv_handle);
		}

		/* setup the reqeuest depend on the current state */
		mcapi_assert(setup_request_have_lock(&receive_endpoint,request,mcapi_status,completed,0,NULL,OPEN_PKTCHAN,0,0,0,r));
	  } else {
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }


  /***************************************************************************
  NAME: mcapi_trans_pktchan_send_open_i
  DESCRIPTION: opens the send endpoint on a packet channel
  PARAMETERS:
    send_handle - the send channel handle to be filled in
    receive_endpoint - the receiving endpoint handle
    request - the request to be filled in when the task is complete
    mcapi_status
  RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_pktchan_send_open_i( mcapi_pktchan_send_hndl_t* send_handle,
  									  mcapi_endpoint_t send_endpoint,
  									  mcapi_request_t* request,
  									  mcapi_status_t* mcapi_status)
  {
	  int r;
	  uint16_t sd,sn,se;
	  /* if errors were found at the mcapi layer, then the request is considered complete */
	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;

	  mcapi_assert(mcapi_trans_decode_handle(send_endpoint,&sd,&sn,&se));

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_endpoint_t receive_endpoint = mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.recv_endpt;

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	  /* get the open-state of the other side */
	  mcapi_boolean_t is_channel_open_on_receive_side = mcapi_trans_endpoint_channel_isopen(receive_endpoint);

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_dprintf(1,"mcapi_trans_pktchan_send_open_i,send_handle,0x%x,&request,&status);",send_endpoint);

	  /* make sure we have a request entry */
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {
		if (!completed) {
		  /* mark the endpoint as open */
		  mcapi_trans_open_channel_have_lock (sd,sn,se);

		  /* fill in the channel handle */
		  *send_handle = mcapi_trans_encode_handle(sd,sn,se);

		  /* has the channel been connected yet? */
		  if ( is_channel_open_on_receive_side == MCAPI_TRUE ) {
			  completed = MCAPI_TRUE;
		  }

		  mcapi_dprintf(2," mcapi_trans_pktchan_send_open_i (node_num=%u,port_num=%u) handle=0x%x",
						mcapi_db->domains[sd].nodes[sn].node_num,
						mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].port_num,
						*send_handle);
		}
		/* setup the reqeuest depend on the current state */
		mcapi_assert(setup_request_have_lock(&send_endpoint,request,mcapi_status,completed,0,NULL,OPEN_PKTCHAN,0,0,0,r));
	  } else {
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }

  /***************************************************************************
  NAME:mcapi_trans_pktchan_send_i
  DESCRIPTION: sends a packet on a packet channel (non-blocking)
  PARAMETERS:
    send_handle - the send channel handle
    buffer - the buffer
    size - the size in bytes of the buffer
    request - the request handle to be filled in when the task is complete
    mcapi_status -

  RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_pktchan_send_i( mcapi_pktchan_send_hndl_t send_handle,
  								 const void* buffer, size_t size,
  								 mcapi_request_t* request,
  								 mcapi_status_t* mcapi_status)
  {
	  uint16_t sd,sn,se,rd,rn,re;
	  int r;
	  int err;

	  /* if errors were found at the mcapi layer, then the request is considered complete */
	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;

	  mcapi_dprintf(1,"mcapi_trans_pktchan_send_i(0x%x,buffer,%u,&request,&status);",send_handle,size);

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  /* make sure we have a request entry */
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {
		if (!completed) {
		  mcapi_assert(mcapi_trans_decode_handle(send_handle,&sd,&sn,&se));
		  mcapi_assert(mcapi_trans_decode_handle(mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.recv_endpt,&rd,&rn,&re));

		  /* check if receive endpoint belongs to the current node */
		  if(rd == my_domain_id && rn == my_node_id)
		  {
//			  if (mcapi_trans_send_have_lock (sd,sn,se,rd,rn,re,(char*)buffer,size,0) != MCAPI_SUCCESS) {
//				  *mcapi_status = MCAPI_ERR_MEM_LIMIT;
//			  }

			  // Try to send the message. Only possible error case should be that
			  // there is not enough memory (send buffers or queue entries). In that
			  // case we will wait until memory will be available.
			  while ((err = mcapi_trans_send_have_lock (sd,sn,se,rd,rn,re,(char*)buffer,size,0)) != MCAPI_SUCCESS) {
				if(err == MCAPI_ERR_MEM_LIMIT) {
					// unlock data base, so that other threads could proceed
					mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
					// let other tasks do there job first
#ifdef UCOSII
					OSTimeDly(TICKS_TO_WAIT);
#endif

#ifdef LINUX
					sched_yield();
#endif
					// lock data base again before next try
					mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));
				}
				else {
					printf("mcapi_tans_msg_send_i(): unexpected error MCAPI_ERR_CHAN_NCNO\n");
					*mcapi_status = MCAPI_ERR_CHAN_NCNO;
					// unlock the data base
					mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
					return;
				}
			  }
			  *mcapi_status = MCAPI_SUCCESS;
		  }
		  else
		  {
			  /* unlock the database */
			  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

			  /* receive endpoint does not belong to the current node -> go to the next layer */
			  if (NS_sendDataToRemote_request(send_handle, mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.recv_endpt, (char *) buffer,size) != NS_OK) {
				  *mcapi_status = MCAPI_ERR_MEM_LIMIT;
			  }

			  /* lock the database */
			  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));
		  }

		  completed = MCAPI_TRUE;
		}
		/* setup the reqeuest depend on the current state */
		mcapi_assert(setup_request_have_lock(&send_handle,request,mcapi_status,completed,size,NULL,SEND,0,0,0,r));
		/* the non-blocking request succeeded, when they call test/wait they will see the status of the send */
		*mcapi_status = MCAPI_SUCCESS;
	  } else{
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }

  /***************************************************************************
  NAME:mcapi_trans_pktchan_send
  DESCRIPTION: sends a packet on a packet channel (blocking)
  PARAMETERS:
    send_handle - the send channel handle
    buffer - the buffer
    size - the size in bytes of the buffer
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_pktchan_send( mcapi_pktchan_send_hndl_t send_handle,
  										  const void* buffer,
  										  size_t size)
  {
	  mcapi_request_t request;
	  mcapi_status_t status = MCAPI_SUCCESS;

	  /* use non-blocking followed by wait */
	  do {
		mcapi_trans_pktchan_send_i (send_handle,buffer,size,&request,&status);
	  } while (status == MCAPI_ERR_REQUEST_LIMIT);

	  mcapi_trans_wait (&request,&size,&status,MCA_INFINITE);

	  if (status == MCAPI_SUCCESS) {
		return MCAPI_TRUE;
	  }
	  return MCAPI_FALSE;
  }


  /***************************************************************************
  NAME:mcapi_trans_pktchan_recv_i
  DESCRIPTION: receives a packet on a packet channel (non-blocking)
  PARAMETERS:
    receive_handle - the send channel handle
    buffer - a pointer to a pointer to a buffer
    request - the request handle to be filled in when the task is complete
    mcapi_status -
  RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_pktchan_recv_i( mcapi_pktchan_recv_hndl_t receive_handle,
  								 void** buffer,
  								 mcapi_request_t* request,
  								 mcapi_status_t* mcapi_status)
  {
	  int r;
	  uint16_t rd,rn,re;
	  /* if errors were found at the mcapi layer, then the request is considered complete */
	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;

	  size_t size = MCAPI_MAX_PKT_SIZE;

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  /* make sure we have a request entry */
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {
		mcapi_dprintf(1,"mcapi_trans_pktchan_recv_i(0x%x,&buffer,&request,&status);",receive_handle,size);

		if (!completed) {
		  mcapi_assert(mcapi_trans_decode_handle(receive_handle,&rd,&rn,&re));

		  /* *buffer will be filled in the with a ptr to an mcapi buffer */
		  *buffer = NULL;
		  if (mcapi_trans_recv_have_lock (rd,rn,re,buffer,MCAPI_MAX_PKT_SIZE,&size,MCAPI_FALSE,NULL)) {
			completed = MCAPI_TRUE;
		  }
		}
		/* setup the reqeuest depend on the current state */
		mcapi_assert(setup_request_have_lock(&receive_handle,request,mcapi_status,completed,size,buffer,RECV,0,0,0,r));
	  } else{
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }

  /***************************************************************************
  NAME:mcapi_trans_pktchan_recv
  DESCRIPTION: receives a packet on a packet channel (blocking)
  PARAMETERS:
    send_handle - the send channel handle
    buffer - the buffer
    received_size - the size in bytes of the buffer
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE (only returns MCAPI_FALSE if it couldn't get a buffer)
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_pktchan_recv( mcapi_pktchan_recv_hndl_t receive_handle,
  										  void** buffer,
  										  size_t* received_size)
  {
	  mcapi_request_t request;
	  mcapi_status_t status = MCAPI_SUCCESS;

	  /* use non-blocking followed by wait */
	  do {
		mcapi_trans_pktchan_recv_i (receive_handle,buffer,&request,&status);
	  } while (status == MCAPI_ERR_REQUEST_LIMIT);

	  mcapi_trans_wait (&request,received_size,&status,MCA_INFINITE);

	  if (status == MCAPI_SUCCESS) {
		return MCAPI_TRUE;
	  }
	  return MCAPI_FALSE;
  }

  /***************************************************************************
  NAME: mcapi_trans_pktchan_available
  DESCRIPTION: counts the number of elements in the endpoint receive queue
    identified by the receive handle.
  PARAMETERS: receive_handle - the receive channel handle
  RETURN VALUE: the number of elements in the receive queue
  ***************************************************************************/
  mcapi_uint_t mcapi_trans_pktchan_available( mcapi_pktchan_recv_hndl_t receive_handle)
  {
	  uint16_t rd,rn,re;
	  int rc = MCAPI_FALSE;

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_dprintf(1,"mcapi_trans_pktchan_available(0x%x);",receive_handle);

	  mcapi_assert(mcapi_trans_decode_handle(receive_handle,&rd,&rn,&re));
	  rc = mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.num_elements;

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	  return rc;
  }

  /***************************************************************************
  NAME:mcapi_trans_pktchan_free
  DESCRIPTION: frees the given buffer
  PARAMETERS: buffer pointer
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE indicating success or failure (buffer not found)
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_pktchan_free( void* buffer)
  {
	  int rc = MCAPI_TRUE;
	  buffer_entry* b_e;

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_dprintf(1,"mcapi_trans_pktchan_free(buffer);");
	  /* optimization - just do pointer arithmetic on the buffer pointer to get
		 the base address of the buffer_entry structure. */
	  /* Old buffer_entry struct required following pointer arithmetics:
	  b_e = buffer-(sizeof(b_e->scalar)+sizeof(b_e->size)+sizeof(b_e->magic_num)+4);
	  which, in case of Ubuntu 14.1 environment results in an error!
      Therefore the ordering of the struct elements was changed so that
      not complex and erroneous buffer arithmetics will be necessary */
	  b_e = buffer;
	  if (b_e->magic_num == MAGIC_NUM) {
		memset(b_e,0,sizeof(buffer_entry));	// clear buffer entry
	  } else {
		/* didn't find the buffer */
		rc = MCAPI_FALSE;
	  }

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	  return rc;
  }


  /***************************************************************************
  NAME:mcapi_trans_pktchan_recv_close_i
  DESCRIPTION: non-blocking close of the receiving end of the packet channel
  PARAMETERS: receive_handle
  RETURN VALUE:none
  ***************************************************************************/
  void mcapi_trans_pktchan_recv_close_i( mcapi_pktchan_recv_hndl_t  receive_handle,
  									   mcapi_request_t* request, mcapi_status_t* mcapi_status)
  {
	  int r;
	  uint16_t rd,rn,re;
	  /* if errors were found at the mcapi layer, then the request is considered complete */

	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;

	  mcapi_assert(mcapi_trans_decode_handle(receive_handle,&rd,&rn,&re));

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_endpoint_t send_endpoint = mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.send_endpt;

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	  /* check the open-state of the other side */
	  mcapi_boolean_t is_channel_open_on_send_side = mcapi_trans_endpoint_channel_isopen(send_endpoint);

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  /* make sure we have a request entry */
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {
		mcapi_dprintf(1,"mcapi_trans_pktchan_recv_close_i (0x%x,&request,&status);",receive_handle);

		if (!completed) {
		  mcapi_trans_close_channel_have_lock (rd,rn,re);

		  /* all pending packets are discarded */
		  while(!mcapi_trans_empty_queue(mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue))
		  {
			 int qindex = mcapi_trans_pop_queue(&mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue);
			 int index = mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[qindex].buff_index &~ MCAPI_VALID_MASK;
			 mcapi_trans_pktchan_free(mcapi_db->buffers[index].buff);
			 mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[qindex].buff_index = 0;
		  }

		  /* has the channel been closed on the other side? */
		  if ( is_channel_open_on_send_side == MCAPI_FALSE ) {
			  /* channel is disconnected */
			  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].connected = MCAPI_FALSE;
			  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.channel_type = MCAPI_NO_CHAN;
			  completed = MCAPI_TRUE;
		  }
		}
		/* setup the reqeuest depend on the current state */
		mcapi_assert(setup_request_have_lock(&receive_handle,request,mcapi_status,completed,0,NULL,CLOSE_PKTCHAN,0,0,0,r));
	  } else {
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }


  /***************************************************************************
  NAME:mcapi_trans_pktchan_send_close_i
  DESCRIPTION: non-blocking close of the sending end of the packet channel
  PARAMETERS: receive_handle
  RETURN VALUE:none
  ***************************************************************************/
  void mcapi_trans_pktchan_send_close_i( mcapi_pktchan_send_hndl_t  send_handle,
  									   mcapi_request_t* request,mcapi_status_t* mcapi_status)
  {
	  int r;
	  uint16_t sd,sn,se;
	  /* if errors were found at the mcapi layer, then the request is considered complete */
	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;

	  mcapi_assert(mcapi_trans_decode_handle(send_handle,&sd,&sn,&se));

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_endpoint_t receive_endpoint = mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.recv_endpt;

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	  /* check the open-state of the other side */
	  mcapi_boolean_t is_channel_open_on_receive_side = mcapi_trans_endpoint_channel_isopen(receive_endpoint);

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  /* make sure we have a request entry */
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {
		mcapi_dprintf(1,"mcapi_trans_pktchan_send_close_i (0x%x,&request,&status);",send_handle);

		if (!completed) {
		  mcapi_trans_close_channel_have_lock (sd,sn,se);

		  /* has the channel been closed on the other side? */
		  if ( is_channel_open_on_receive_side == MCAPI_FALSE ) {
			  /* channel is disconnected */
			  mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].connected = MCAPI_FALSE;
			  mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.channel_type = MCAPI_NO_CHAN;
			  completed = MCAPI_TRUE;
		  }
		}
		/* setup the reqeuest depend on the current state */
		mcapi_assert(setup_request_have_lock(&send_handle,request,mcapi_status,completed,0,NULL,CLOSE_PKTCHAN,0,0,0,r));
	  } else{
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }

  //////////////////////////////////////////////////////////////////////////////
  //                                                                          //
  //                   mcapi_trans API: scalar channels                       //
  //                                                                          //
  //////////////////////////////////////////////////////////////////////////////

  /***************************************************************************
  NAME:mcapi_trans_sclchan_connect_i
  DESCRIPTION: connects a scalar channel between the given two endpoints
  PARAMETERS:
    send_endpoint - the sending endpoint
    receive_endpoint - the receiving endpoint
    request - the request to be filled in when the task is complete
    mcapi_status - the status
  RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_sclchan_connect_i( mcapi_endpoint_t  send_endpoint,
  								  mcapi_endpoint_t  receive_endpoint,
  								  mcapi_request_t* request,
  								  mcapi_status_t* mcapi_status)
  {
	  int r;
	  /* if errors were found at the mcapi layer, then the request is considered complete */
	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  /* make sure we have a request entry */
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {
		if (!completed) {
		  mcapi_trans_connect_channel_have_lock (send_endpoint,receive_endpoint,MCAPI_SCL_CHAN);
		  completed = MCAPI_TRUE;
		}
		/* setup the reqeuest depend on the current state */
		mcapi_assert(setup_request_have_lock(&receive_endpoint,request,mcapi_status,completed,0,NULL,0,0,0,0,r));
	  } else {
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }

  /***************************************************************************
  NAME: mcapi_trans_sclchan_recv_open_i
  DESCRIPTION: opens the receive endpoint on a packet channel
  PARAMETERS:
  recv_handle - the receive channel handle to be filled in
  receive_endpoint - the receiving endpoint handle
  request - the request to be filled in when the task is complete
  mcapi_status
  RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_sclchan_recv_open_i( mcapi_sclchan_recv_hndl_t* recv_handle,
  									mcapi_endpoint_t receive_endpoint,
  									mcapi_request_t* request,
  									mcapi_status_t* mcapi_status)
  {
	  int r;
	  uint16_t rd,rn,re;
	  /* if errors were found at the mcapi layer, then the request is considered complete */
	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;

	  mcapi_assert(mcapi_trans_decode_handle(receive_endpoint,&rd,&rn,&re));

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  /* check the open-state of the other side */
	  mcapi_endpoint_t send_endpoint = mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.send_endpt;

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	  mcapi_boolean_t is_channel_open_on_send_side = mcapi_trans_endpoint_channel_isopen(send_endpoint);

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_dprintf(1,"mcapi_trans_sclchan_recv_open_i(recv_handle,0x%x,&request,&status);",receive_endpoint);

	  /* make sure we have a request entry */
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {
		if (!completed) {
		  mcapi_trans_open_channel_have_lock (rd,rn,re);

		  /* fill in the channel handle */
		  *recv_handle = mcapi_trans_encode_handle(rd,rn,re);

		  /* has the channel been connected yet? */
		  if ( is_channel_open_on_send_side ) {
			completed = MCAPI_TRUE;
		  }

		  mcapi_dprintf(2,"mcapi_trans_sclchan_recv_open_i (node_num=%u,port_num=%u) handle=0x%x",
						mcapi_db->domains[rd].nodes[rn].node_num,
						mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].port_num,
						*recv_handle);
		}
		/* setup the reqeuest depend on the current state */
		mcapi_assert(setup_request_have_lock(&receive_endpoint,request,mcapi_status,completed,0,NULL,OPEN_SCLCHAN,0,0,0,r));
	  } else{
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }


  /***************************************************************************
  NAME: mcapi_trans_sclchan_send_open_i
  DESCRIPTION: opens the receive endpoint on a packet channel
  PARAMETERS:
  send_handle - the receive channel handle to be filled in
  receive_endpoint - the receiving endpoint handle
  request - the request to be filled in when the task is complete
  mcapi_status
  RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_sclchan_send_open_i( mcapi_sclchan_send_hndl_t* send_handle,
  									mcapi_endpoint_t  send_endpoint,
  									mcapi_request_t* request,
  									mcapi_status_t* mcapi_status)
  {
	  int r;
	  uint16_t sd,sn,se;
	  /* if errors were found at the mcapi layer, then the request is considered complete */
	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;

	  mcapi_assert(mcapi_trans_decode_handle(send_endpoint,&sd,&sn,&se));

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  /* check the open-state of the other side */
	  mcapi_endpoint_t receive_endpoint = mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.recv_endpt;

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	  mcapi_boolean_t is_channel_open_on_receive_side = mcapi_trans_endpoint_channel_isopen(receive_endpoint);

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_dprintf(1,"mcapi_trans_sclchan_send_open_i(send_handle,0x%x,&request,&status);",send_endpoint);

	  /* make sure we have a request entry */
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {
		if (!completed) {
		  /* mark the endpoint as open */
		  mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].open = MCAPI_TRUE;

		  /* fill in the channel handle */
		  *send_handle = mcapi_trans_encode_handle(sd,sn,se);

		  /* has the channel been connected yet? */
		  if ( is_channel_open_on_receive_side == MCAPI_TRUE ) {
			  completed = MCAPI_TRUE;
		  }

		  mcapi_dprintf(2," mcapi_trans_sclchan_send_open_i (node_num=%u,port_num=%u) handle=0x%x",
						mcapi_db->domains[sd].nodes[sn].node_num,
						mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].port_num,
						*send_handle);
		}
		/* setup the reqeuest depend on the current state */
		mcapi_assert(setup_request_have_lock(&send_endpoint,request,mcapi_status,completed,0,NULL,OPEN_SCLCHAN,0,0,0,r));
	  } else{
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }

  /***************************************************************************
  NAME:mcapi_trans_sclchan_send
  DESCRIPTION: sends a scalar on a scalar channel (blocking)
  PARAMETERS:
  send_handle - the send channel handle
  dataword - the scalar we should send
  size - the size in bytes of the scalar
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_sclchan_send( mcapi_sclchan_send_hndl_t send_handle,
  										uint64_t dataword,
  										uint32_t size)
  {
	  uint16_t sd,sn,se,rd,rn,re;
	  int rc = MCAPI_FALSE;
//	  printf("mcapi_trans_sclchan_send(0x%x, 0x%x, 0x%x);\n", send_handle, dataword, size); fflush(stdout);

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_assert(mcapi_trans_decode_handle(send_handle,&sd,&sn,&se));
	  mcapi_assert(mcapi_trans_decode_handle(mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.recv_endpt,&rd,&rn,&re));

	  /* check if receive endpoint belongs to the current node */
	  if(rd == my_domain_id && rn == my_node_id)
	  {
		  printf("current node\n");
		  rc = mcapi_trans_send_have_lock (sd,sn,se,rd,rn,re,NULL,size,dataword);
		  if(rc == MCAPI_SUCCESS)	rc = MCAPI_TRUE;
		  else						rc = MCAPI_FALSE;
		  /* unlock the database */
		  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
	  }
	  else
	  {
		  mcapi_endpoint_t receive_endpoint = mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.recv_endpt;

		  /* unlock the database */
		  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

		  /* receive endpoint does not belong to the current node -> go to the next layer */
		  rc = NS_sendDataToRemote_request(send_handle, receive_endpoint, (const int8_t *) &dataword, size);
	  }

	  if(rc == NS_OK)	return(MCAPI_TRUE);
	  else				return(MCAPI_FALSE);
//	  return rc;
  }


  /***************************************************************************
  NAME:mcapi_trans_sclchan_recv
  DESCRIPTION: receives a packet on a packet channel (blocking)
  PARAMETERS:
  send_handle - the send channel handle
  buffer - the buffer
  received_size - the size in bytes of the buffer
  RETURN VALUE: MCAPI_TRUE/MCAPI_FALSE (only returns MCAPI_FALSE if it couldn't get a buffer)
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_sclchan_recv( mcapi_sclchan_recv_hndl_t receive_handle,
  										uint64_t *data,uint32_t size)
  {
	  uint16_t rd,rn,re;
	  size_t received_size;
	  int rc = MCAPI_FALSE;

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_dprintf(1,"uint64_t data;");
	  mcapi_dprintf(1,"mcapi_trans_sclchan_send(0x%x,&data,%u);",receive_handle,size);

	  mcapi_assert(mcapi_trans_decode_handle(receive_handle,&rd,&rn,&re));

	  if (mcapi_trans_recv_have_lock (rd,rn,re,NULL,size,&received_size,MCAPI_TRUE,data) &&
		  received_size == size) {
		rc = MCAPI_TRUE;
	  }

	  /* FIXME: (errata A2) if size != received_size then we shouldn't remove the item from the
		 endpoints receive queue */

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	  return rc;
  }

  /***************************************************************************
  NAME: mcapi_trans_sclchan_available
  DESCRIPTION: counts the number of elements in the endpoint receive queue
  identified by the receive handle.
  PARAMETERS: receive_handle - the receive channel handle
  RETURN VALUE: the number of elements in the receive queue
  ***************************************************************************/
  mcapi_uint_t mcapi_trans_sclchan_available_i( mcapi_sclchan_recv_hndl_t receive_handle)
  {
	  uint16_t rd,rn,re;
	  int rc = MCAPI_FALSE;

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_dprintf(1,"mcapi_trans_sclchan_available_i(0x%x);",receive_handle);

	  mcapi_assert(mcapi_trans_decode_handle(receive_handle,&rd,&rn,&re));
	  rc = mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.num_elements;

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	  return rc;
  }

  /***************************************************************************
  NAME:mcapi_trans_sclchan_recv_close_i
  DESCRIPTION: non-blocking close of the receiving end of the scalar channel
  PARAMETERS:
  receive_handle -
  request -
  mcapi_status -
  RETURN VALUE:none
  ***************************************************************************/
  void mcapi_trans_sclchan_recv_close_i( mcapi_sclchan_recv_hndl_t  recv_handle,
  									 mcapi_request_t* request,
  									 mcapi_status_t* mcapi_status)
  {
	  uint16_t rd,rn,re;
	  int r;

	  /* if errors were found at the mcapi layer, then the request is considered complete */
	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;

	  mcapi_assert(mcapi_trans_decode_handle(recv_handle,&rd,&rn,&re));

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_endpoint_t send_endpoint = mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.send_endpt;

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	  /* check the open-state depend on the other side */
	  mcapi_boolean_t is_channel_open_on_send_side = mcapi_trans_endpoint_channel_isopen(send_endpoint);

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_dprintf(1,"mcapi_trans_sclchan_recv_close_i(0x%x,&request,&status);",recv_handle);

	  /* make sure we have a request entry */
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {
		if (!completed) {
		  mcapi_trans_close_channel_have_lock (rd,rn,re);

		  /* all pending packets are discarded */
		  while(!mcapi_trans_empty_queue(mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue))
		  {
			 int qindex = mcapi_trans_pop_queue(&mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue);
			 int index = mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[qindex].buff_index &~ MCAPI_VALID_MASK;
			 /* free the mcapi buffer */
			 memset(&mcapi_db->buffers[index],0,sizeof(mcapi_db->buffers[index]));
			 mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[qindex].buff_index = 0;
		  }

		  /* has the channel been closed on the other side? */
		  if ( is_channel_open_on_send_side == MCAPI_FALSE ) {
			  /* channel is disconnected */
			  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].connected = MCAPI_FALSE;
			  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.channel_type = MCAPI_NO_CHAN;
			  completed = MCAPI_TRUE;
		  }
		}
		/* setup the request depend on the current state */
		mcapi_assert(setup_request_have_lock(&recv_handle,request,mcapi_status,completed,0,NULL,CLOSE_SCLCHAN,0,0,0,r));
	  } else{
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }
	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }


  /***************************************************************************
  NAME:mcapi_trans_sclchan_send_close_i
  DESCRIPTION: non-blocking close of the sending end of the scalar channel
  PARAMETERS:
  send_handle -
  request -
  mcapi_status -
  RETURN VALUE:none
  ***************************************************************************/
  void mcapi_trans_sclchan_send_close_i( mcapi_sclchan_send_hndl_t send_handle,
  									 mcapi_request_t* request,
  									 mcapi_status_t* mcapi_status)
  {
	  uint16_t sd,sn,se;
	  int r;

	  /* if errors were found at the mcapi layer, then the request is considered complete */
	  mcapi_boolean_t completed =  (*mcapi_status == MCAPI_SUCCESS) ? MCAPI_FALSE : MCAPI_TRUE;

	  mcapi_assert(mcapi_trans_decode_handle(send_handle,&sd,&sn,&se));

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_endpoint_t receive_endpoint = mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.recv_endpt;

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

	  /* check the open-state depend on the other side */
	  mcapi_boolean_t is_channel_open_on_receive_side = mcapi_trans_endpoint_channel_isopen(receive_endpoint);

	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

	  mcapi_dprintf(1,"mcapi_trans_sclchan_send_close_i(0x%x,&request,&status);",send_handle);

	  /* make sure we have a request entry */
	  if ( mcapi_trans_reserve_request_have_lock(&r)) {
		if (!completed) {
		  mcapi_trans_close_channel_have_lock (sd,sn,se);

		  /* has the channel been closed on the other side? */
		  if ( is_channel_open_on_receive_side == MCAPI_FALSE ) {
			  /* channel is disconnected */
			  mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].connected = MCAPI_FALSE;
			  mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.channel_type = MCAPI_NO_CHAN;
			  completed = MCAPI_TRUE;
		  }
		}
		/* setup the request depend on the current state */
		mcapi_assert(setup_request_have_lock(&send_handle,request,mcapi_status,completed,0,NULL,CLOSE_SCLCHAN,0,0,0,r));
	  } else{
		*mcapi_status = MCAPI_ERR_REQUEST_LIMIT;
	  }

	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }

  //////////////////////////////////////////////////////////////////////////////
  //                                                                          //
  //                   test and wait functions                                //
  //                                                                          //
  //////////////////////////////////////////////////////////////////////////////

  /***************************************************************************
  NAME:mcapi_trans_test_i
  DESCRIPTION: Tests if the request has completed yet (non-blocking).
  	It is called mcapi_test at the mcapi level even though it's a non-blocking function.
  PARAMETERS:
  request -
  size -
  mcapi_status -
  RETURN VALUE: TRUE/FALSE indicating if the request has completed.
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_test_i( mcapi_request_t* request,
  								  size_t* size,
  								  mcapi_status_t* mcapi_status)
  {
	  /* We return true if it's cancelled, invalid or completed.  We only return
	         false if the user should continue polling.
	  */
	  mcapi_boolean_t rc = MCAPI_FALSE;
	  uint16_t r;

	  mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE);

	  mcapi_dprintf(3,"mcapi_trans_test_i request handle:0x%lx",*request);

	  if (!mcapi_trans_decode_request_handle(request,&r) ||
		  (mcapi_db->requests[r].valid == MCAPI_FALSE)) {
		*mcapi_status = MCAPI_ERR_REQUEST_INVALID;
		rc = MCAPI_TRUE;
	  } else if (mcapi_db->requests[r].cancelled) {
		*mcapi_status = MCAPI_ERR_REQUEST_CANCELLED;
		rc = MCAPI_TRUE;
	  } else {
		if (!(mcapi_db->requests[r].completed)) {
		/* try to complete the request */
		/*  receives to an empty channel or get_endpt for an endpt that
			doesn't yet exist are the only two types of non-blocking functions
			that don't complete immediately for this implementation */
		switch (mcapi_db->requests[r].type) {
		case (RECV) :
		  check_receive_request_have_lock (request); break;
		case (GET_ENDPT) :
		  check_get_endpt_request_have_lock (request);break;
		case (OPEN_PKTCHAN) :
		case (OPEN_SCLCHAN) :
		  check_open_channel_request_have_lock (request);
		  break;
		case (CLOSE_PKTCHAN) :
		case (CLOSE_SCLCHAN) :
		  check_close_channel_request_have_lock (request);
		  break;
		default:
		  mcapi_assert(0);
		  break;
		};
	  }

	  /* query completed again because we may have just completed it */
	  if (mcapi_db->requests[r].completed) {
		mcapi_trans_remove_request_have_lock(r);	/* by etem */
		*size = mcapi_db->requests[r].size;
//		*mcapi_status = mcapi_db->requests[r].status;
		if((*mcapi_status = mcapi_db->requests[r].status) == MCAPI_ERR_MEM_LIMIT)
			printf("mcapi_trans_test_i() status = %d\n", *mcapi_status);

		/* clear the entry so that it can be reused */
		memset(&mcapi_db->requests[r],0,sizeof(mcapi_request_data));
		*request=0;

		rc = MCAPI_TRUE;
	  }
	 }

	  //mcapi_dprintf(2,"mcapi_trans_test_i returning rc=%u,status=%s",rc,mcapi_display_status(*mcapi_status));
	  mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE);

	  return rc;
  }

  /***************************************************************************
  NAME:mcapi_trans_wait
  DESCRIPTION:Tests if the request has completed yet (non-blocking).
  PARAMETERS:
  send_handle -
  request -
  mcapi_status -
  RETURN VALUE:  TRUE indicating the request has completed or FALSE
  indicating the request has been cancelled.
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_wait( mcapi_request_t* request,
  								size_t* size,
  								mcapi_status_t* mcapi_status,
  								mcapi_timeout_t timeout)
  {
	  mcapi_timeout_t time = 0;
	  uint16_t r;
	  mcapi_assert(mcapi_trans_decode_request_handle(request,&r));
	  mcapi_dprintf(1,"mcapi_trans_wait(&request,&size,&status,%u);",timeout);
	  while(1) {
		time++;
		if (mcapi_trans_test_i(request,size,mcapi_status)) {
		  return MCAPI_TRUE;
		}

		/* yield */
		mcapi_dprintf(5,"mcapi_trans_wait - attempting to yield");
		/* we don't have the lock, it's safe to just yield */
#ifdef UCOSII
		OSTimeDly(TICKS_TO_WAIT);
#endif

#ifdef LINUX
		sched_yield();
#endif
		if ((timeout !=  MCA_INFINITE) && (time >= timeout)) {
		  *mcapi_status = MCAPI_TIMEOUT;
		  return MCAPI_FALSE;
		}
	  }
  }

  /***************************************************************************
  NAME:mcapi_trans_wait_any
  DESCRIPTION:Tests if any of the requests have completed yet (blocking).
    Note: the request is now cleared if it has been completed or cancelled.
  PARAMETERS:
  send_handle -
  request -
  mcapi_status -
  RETURN VALUE:
  ***************************************************************************/
  unsigned mcapi_trans_wait_any(size_t number, mcapi_request_t** requests, size_t* size,
  								   mcapi_status_t* mcapi_status,
  								   mcapi_timeout_t timeout)
  {
	  mcapi_timeout_t time = 0;
	  int i;

	  mcapi_dprintf(1,"mcapi_trans_wait_any");
	  while(1) {
		time++;
		for (i = 0; i < number; i++) {
		  if (mcapi_trans_test_i(requests[i],size,mcapi_status)) {
			return i;
		  }
		  /* yield */
		  mcapi_dprintf(5,"mcapi_trans_wait_any - attempting to yield");
		  /* we don't have the lock, it's safe to just yield */
#ifdef UCOSII
		OSTimeDly(TICKS_TO_WAIT);
#endif

#ifdef LINUX
		sched_yield();
#endif
		  if ((timeout !=  MCA_INFINITE) && (time >= timeout)) {
			*mcapi_status = MCAPI_TIMEOUT;
			return MCA_RETURN_VALUE_INVALID;
		  }
		}
	  }
  }

  /***************************************************************************
  NAME:mcapi_trans_cancel
  DESCRIPTION: Cancels the given request
  PARAMETERS:
  request -
  mcapi_status -
  RETURN VALUE:none
  ***************************************************************************/
  void mcapi_trans_cancel(mcapi_request_t* request,mcapi_status_t* mcapi_status)
  {
	  uint16_t r;

	  mcapi_dprintf(1,"mcapi_trans_cancel");

	  /* lock the database */
	  mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE);

	  mcapi_assert(mcapi_trans_decode_request_handle(request,&r));

	  if (mcapi_db->requests[r].valid == MCAPI_FALSE) {
		*mcapi_status = MCAPI_ERR_REQUEST_INVALID;
	  } else if (mcapi_db->requests[r].cancelled) {
		/* this reqeust has already been cancelled */
		mcapi_dprintf(2,"mcapi_trans_cancel - request was already cancelled");
		*mcapi_status = MCAPI_ERR_REQUEST_CANCELLED;
	  } else if (!(mcapi_db->requests[r].completed)) {
		/* cancel the request */
		mcapi_db->requests[r].cancelled = MCAPI_TRUE;
		switch (mcapi_db->requests[r].type) {
		case (RECV) :
		  cancel_receive_request_have_lock (request);
		  break;
		case (GET_ENDPT) :
		  break;
		default:
		  mcapi_assert(0);
		  break;
		};
		/* clear the request so that it can be re-used */
		memset(&mcapi_db->requests[r],0,sizeof(mcapi_request_data));
		*mcapi_status = MCAPI_SUCCESS;
		/* invalidate the request handle */
		//*request = 0;
	  } else {
		/* it's too late, the request has already completed */
		mcapi_dprintf(2," mcapi_trans_cancel - Unable to cancel because request has already completed");
	  }

	  /* unlock the database */
	  mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE);
  }


  //////////////////////////////////////////////////////////////////////////////
  //                                                                          //
  //                   misc helper functions                                  //
  //                                                                          //
  //////////////////////////////////////////////////////////////////////////////

  /***************************************************************************
    NAME:mcapi_trans_signal_handler
    DESCRIPTION: The purpose of this function is to catch signals so that we
     can clean up our shared memory and sempaphore resources cleanly.
    PARAMETERS: the signal
    RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_signal_handler ( int sig )
  {
  }

  /***************************************************************************
   NAME: setup_request_have_lock
   DESCRIPTION: Sets up the request for a non-blocking function.
   PARAMETERS:
      handle -
      request -
      mcapi_status -
      completed - whether the request has already been completed or not (usually
        it has - receives to an empty queue or endpoint_get for endpoints that
        don't yet exist are the two main cases where completed will be false)
      size -
      buffer - the buffer
      type - the type of the request
   RETURN VALUE:
   ***************************************************************************/
   mcapi_boolean_t setup_request_have_lock (mcapi_endpoint_t* handle,
                                           mcapi_request_t* request,
                                           mcapi_status_t* mcapi_status,
                                           mcapi_boolean_t completed,
                                           size_t size,void** buffer,
                                           mcapi_request_type type,
                                           mcapi_uint_t node_num,
                                           mcapi_uint_t port_num,
                                           mcapi_domain_t domain_num,
                                           int r)
   {
	   int i,qindex;
	   uint16_t d,n,e;
	   mcapi_boolean_t rc = MCAPI_TRUE;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	   mcapi_db->requests[r].status = *mcapi_status;
	   mcapi_db->requests[r].size = size;
	   mcapi_db->requests[r].cancelled = MCAPI_FALSE;
	   mcapi_db->requests[r].completed = completed;

	   //encode the request handle (this is the only place in the code we do this)
	   *request = 0x80000000 | r;
	   mcapi_dprintf(1,"setup_request_have_lock handle=0x%x",*request);
	   /* this is hacky, there's probably a better way to do this */
	   if ((buffer != NULL) && (!completed)) {
		 mcapi_assert(mcapi_trans_decode_handle(*handle,&d,&n,&e));
		 if ( mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.channel_type == MCAPI_PKT_CHAN) {
		   /* packet buffer means system buffer, so save the users pointer to the buffer */
		   mcapi_db->requests[r].buffer_ptr = buffer;
		 } else {
		   /* message buffer means user buffer, so save the users buffer */
		   mcapi_db->requests[r].buffer = *buffer;
		 }
	   }
	   mcapi_db->requests[r].type = type;
	   mcapi_db->requests[r].handle = *handle;

	   /* save the pointer so that we can fill it in (the endpoint may not have been created yet)
		  an alternative is to make buffer a void* and use it for everything (messages, endpoints, etc.) */
	   if (  mcapi_db->requests[r].type == GET_ENDPT) {
		 mcapi_db->requests[r].ep_endpoint = handle;
		 mcapi_db->requests[r].ep_node_num = node_num;
		 mcapi_db->requests[r].ep_port_num = port_num;
		 mcapi_db->requests[r].ep_domain_num = domain_num;
	   }

	   /* if this was a non-blocking receive to an empty queue, then reserve the next buffer */
	   if ((type == RECV) && (!completed)) {
		 mcapi_assert(mcapi_trans_decode_handle(*handle,&d,&n,&e));
		 /*find the queue entry that doesn't already have a request associated with it */
		 for (i = 0; i < MCAPI_MAX_QUEUE_ENTRIES; i++) {
		   /* walk from head to tail */
		   qindex = (mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.head + i) % (MCAPI_MAX_QUEUE_ENTRIES);
		   if ((mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.elements[qindex].request==0) &&
			   (!mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.elements[qindex].invalid)) {
			 mcapi_dprintf(4,"receive request r=%u reserving qindex=%i",i,qindex);
			 mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.elements[qindex].request = *request;
			 break;
		   }
		 }
		 if (i == MCAPI_MAX_QUEUE_ENTRIES) {
		   mcapi_dprintf(1,"setup_request_have_lock: MCAPI_ERR_MEM_LIMIT all of this endpoint's buffers already have requests associated with them.  Your receives are outpacing your sends.  Either throttle this at the application layer or reconfigure with a larger endpoint receive queue.");
		   /* all of this endpoint's buffers already have requests associated with them */
		   mcapi_db->requests[r].status = MCAPI_ERR_MEM_LIMIT;
		   mcapi_db->requests[r].completed = MCAPI_TRUE;
		 }
	   }


	   return rc;
   }

   /***************************************************************************
   NAME: mcapi_trans_decode_request_handle
   DESCRIPTION:
   PARAMETER:
   RETURN VALUE:
   ***************************************************************************/
   mcapi_boolean_t mcapi_trans_decode_request_handle(mcapi_request_t* request,uint16_t* r)
   {
	   *r = *request;
	   if ((*r < MCAPI_MAX_REQUESTS) && (*request & 0x80000000)) {
		 return MCAPI_TRUE;
	   }
	   return MCAPI_FALSE;
   }

  /***************************************************************************
  NAME:mcapi_trans_display_state
  DESCRIPTION: This function is useful for debugging.  If the handle is null,
  we'll print out the state of the entire database.  Otherwise, we'll print out
  only the state of the endpoint that the handle refers to.
  PARAMETERS:
   handle
  RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_display_state (void* handle)
  {
	  /* lock the database */
	  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));
	  mcapi_trans_display_state_have_lock(handle);
	  /* unlock the database */
	  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));
  }

  /***************************************************************************
  NAME:mcapi_trans_display_state_have_lock
  DESCRIPTION: This function is useful for debugging.  If the handle is null,
  we'll print out the state of the entire database.  Otherwise, we'll print out
  only the state of the endpoint that the handle refers to.  Expects the database
  to be locked.
  PARAMETERS:
   handle
  RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_display_state_have_lock (void* handle)
  {
	  uint16_t d,n,e,a;
	  mcapi_endpoint_t* endpoint = (mcapi_endpoint_t*)handle;

	  printf("DISPLAY STATE:");


	  if (handle != NULL) {
		/* print the data for the given endpoint */
		mcapi_assert(mcapi_trans_decode_handle(*endpoint,&d,&n,&e));
		printf("\nnode: %u, port: %u, receive queue (num_elements=%i):",
			   (unsigned)mcapi_db->domains[d].nodes[n].node_num,(unsigned)mcapi_db->domains[d].nodes[n].node_d.endpoints[e].port_num,
			   (unsigned)mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.num_elements);

		printf("\n    endpoint: %u",e);
		printf("\n      valid:%u",mcapi_db->domains[d].nodes[n].node_d.endpoints[e].valid);
		printf("\n      anonymous:%u",mcapi_db->domains[d].nodes[n].node_d.endpoints[e].anonymous);
		printf("\n      open:%u",mcapi_db->domains[d].nodes[n].node_d.endpoints[e].open);
		printf("\n      connected:%u",mcapi_db->domains[d].nodes[n].node_d.endpoints[e].connected);
		printf("\n      num_attributes:%u",(unsigned)mcapi_db->domains[d].nodes[n].node_d.endpoints[e].num_attributes);
		for (a = 0; a < mcapi_db->domains[d].nodes[n].node_d.endpoints[e].num_attributes; a++) {
		  printf("\n        attribute:%u",a);
		  printf("\n          valid:%u",mcapi_db->domains[d].nodes[n].node_d.endpoints[e].attributes.entries[a].valid);
		  printf("\n          attribute_num:%u",mcapi_db->domains[d].nodes[n].node_d.endpoints[e].attributes.entries[a].attribute_num);
		  printf("\n          bytes:%i",(unsigned)mcapi_db->domains[d].nodes[n].node_d.endpoints[e].attributes.entries[a].bytes);
		}
		//print_queue(c_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue);
	  } else {
		/* print the whole database */
		for (d = 0; d < MCA_MAX_DOMAINS; d++) {
		  for (n = 0; n < MCA_MAX_NODES; n++) {
			if ((mcapi_db->domains[d].valid == MCAPI_TRUE) && (mcapi_db->domains[d].nodes[n].valid == MCAPI_TRUE)) {
			  printf("\nVALID NODE: d=%u, nindex=%u domain_id=%u,node_num=%u,",
					 d,n,mcapi_db->domains[d].domain_id,(unsigned)mcapi_db->domains[d].nodes[n].node_num);
			  printf("\n  num_endpoints:%u",mcapi_db->domains[d].nodes[n].node_d.num_endpoints);
			  for (e = 0; e < mcapi_db->domains[d].nodes[n].node_d.num_endpoints; e++) {
				if (mcapi_db->domains[d].nodes[n].node_d.endpoints[e].valid) {
				  printf("\n    VALID ENDPT: e=%u",e);
				  printf("\n    port_num: %u",(unsigned)mcapi_db->domains[d].nodes[n].node_d.endpoints[e].port_num);
				  printf("\n      anonymous:%u",mcapi_db->domains[d].nodes[n].node_d.endpoints[e].anonymous);
				  printf("\n      open:%u",mcapi_db->domains[d].nodes[n].node_d.endpoints[e].open);
				  printf("\n      connected:%u",mcapi_db->domains[d].nodes[n].node_d.endpoints[e].connected);
				  printf("\n      num_attributes:%u",(unsigned)mcapi_db->domains[d].nodes[n].node_d.endpoints[e].num_attributes);
				  for (a = 0; a < mcapi_db->domains[d].nodes[n].node_d.endpoints[e].num_attributes; a++) {
					printf("\n        a=%u",a);
					printf("\n        attribute:%u",a);
					printf("\n          valid:%u",mcapi_db->domains[d].nodes[n].node_d.endpoints[e].attributes.entries[a].valid);
					printf("\n          attribute_num:%u",mcapi_db->domains[d].nodes[n].node_d.endpoints[e].attributes.entries[a].attribute_num);
					printf("\n          bytes:%u",(unsigned)mcapi_db->domains[d].nodes[n].node_d.endpoints[e].attributes.entries[a].bytes);
				  }
				}
				//print_queue(c_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue);
			  }
			}
		  }
		}

	  }
	  printf("\n ");
  }

  /***************************************************************************
  NAME:check_open_channel_request
  DESCRIPTION: Checks if the endpoint has been connected yet.
  PARAMETERS: the request pointer (to be filled in)
  RETURN VALUE: none
  ***************************************************************************/
  void check_open_channel_request_have_lock (mcapi_request_t *request)
  {
	  uint16_t d,n,e,r;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  if (mcapi_trans_decode_request_handle(request,&r)) {

		mcapi_assert(mcapi_trans_decode_handle(mcapi_db->requests[r].handle,&d,&n,&e));

		/* has the channel been connected yet? */
		mcapi_endpoint_t send_endpoint = mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.send_endpt;
		mcapi_endpoint_t recv_endpoint = mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.recv_endpt;

		/* unlock the database */
		mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

		/* check if channel is open on both sides */
		mcapi_boolean_t is_channel_open = mcapi_trans_endpoint_channel_isopen(send_endpoint)
									   && mcapi_trans_endpoint_channel_isopen(recv_endpoint);

		/* lock the database */
		mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

		if ( is_channel_open == MCAPI_TRUE )
		{
			mcapi_db->requests[r].completed = MCAPI_TRUE;
		}
	  }
  }

  /***************************************************************************
  NAME:check_close_channel_request
  DESCRIPTION: Checks if the endpoint has been disconnected yet.
  PARAMETERS: the request pointer (to be filled in)
  RETURN VALUE: none
  ***************************************************************************/
  void check_close_channel_request_have_lock (mcapi_request_t *request)
  {
	  uint16_t d,n,e,r;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  if (mcapi_trans_decode_request_handle(request,&r)) {

		mcapi_assert(mcapi_trans_decode_handle(mcapi_db->requests[r].handle,&d,&n,&e));

		/* has the channel been connected yet? */
		mcapi_endpoint_t send_endpoint = mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.send_endpt;
		mcapi_endpoint_t recv_endpoint = mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.recv_endpt;

		/* unlock the database */
		mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

		/* check if channel is closed on both sides */
		mcapi_boolean_t is_channel_closed = !mcapi_trans_endpoint_channel_isopen(send_endpoint)
										 && !mcapi_trans_endpoint_channel_isopen(recv_endpoint);

		/* lock the database */
		mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));

		if ( is_channel_closed == MCAPI_TRUE )
		{
			/* channel is disconnected */
			mcapi_db->domains[d].nodes[n].node_d.endpoints[e].connected = MCAPI_FALSE;
			mcapi_db->domains[d].nodes[n].node_d.endpoints[e].recv_queue.channel_type = MCAPI_NO_CHAN;
			mcapi_db->requests[r].completed = MCAPI_TRUE;
		}
	  }
  }

  /***************************************************************************
  NAME:check_get_endpt_request
  DESCRIPTION: Checks if the request to get an endpoint has been completed or not.
  PARAMETERS: the request pointer (to be filled in)
  RETURN VALUE: none
  ***************************************************************************/
  void check_get_endpt_request_have_lock (mcapi_request_t *request)
  {
	  uint16_t r;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  mcapi_assert(mcapi_trans_decode_request_handle(request,&r));

	  /* check if endpoint belongs to the local node */
	  if(mcapi_db->requests[r].ep_domain_num == my_domain_id && mcapi_db->requests[r].ep_node_num == my_node_id)
	  {
		  if (mcapi_trans_endpoint_get_have_lock (mcapi_db->requests[r].ep_endpoint,
												  mcapi_db->requests[r].ep_domain_num,
												  mcapi_db->requests[r].ep_node_num,
												  mcapi_db->requests[r].ep_port_num)) {
			mcapi_db->requests[r].completed = MCAPI_TRUE;
			mcapi_db->requests[r].status = MCAPI_SUCCESS;
		  }
	  }
	  else
	  {
		  /* unlock the database */
		  mcapi_assert(mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE));

		  /* endpoint does not belong to the local node -> go to the next layer */
		  switch (mcapi_db->requests[r].type) {
		  case (RECV) :
			  printf("check_get_endpt_request_have_lock():  - request.type = RECV\n"); break;
		  case (GET_ENDPT) :
//				printf("check_get_endpt_request_have_lock():  - request.type = GET_ENDPT\n");
				// Because NS_getRemoteEndpoint() is a blocking function, the
				// only reason for this case could be that remote MCAPI system is
				// initialized but the requested remote endpoint wasn't installed
				// at previous request time
				if (NS_getRemoteEndpoint_request(mcapi_db->requests[r].ep_endpoint,
							mcapi_db->requests[r].ep_domain_num,
							mcapi_db->requests[r].ep_node_num,
							mcapi_db->requests[r].ep_port_num) == NS_OK) {
					mcapi_db->requests[r].completed = MCAPI_TRUE;
					mcapi_db->requests[r].status = MCAPI_SUCCESS;
				}
				break;
		  case (OPEN_PKTCHAN) :
			  printf("check_get_endpt_request_have_lock():  - request.type = OPEN_PKTCHAN\n"); break;
		  case (OPEN_SCLCHAN) :
			  printf("check_get_endpt_request_have_lock():  - request.type = OPEN_SCLCHAN\n"); break;
		  case (CLOSE_PKTCHAN) :
			  printf("check_get_endpt_request_have_lock():  - request.type = CLOSE_PKTCHAN\n"); break;
		  case (CLOSE_SCLCHAN) :
			  printf("check_get_endpt_request_have_lock():  - request.type = CLOSE_SCLCHAN\n"); break;
		  default :
			  printf("check_get_endpt_request_have_lock():  - request.type = unknown\n"); break;
		  }



		  /* lock the database */
		  mcapi_assert(mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE));
	  }
  }

  /***************************************************************************
  NAME: cancel_receive_request_have_lock
  DESCRIPTION: Cancels an outstanding receive request.  This is a little tricky
   because we have to preserve FIFO which means we have to shift all other
   outstanding receive requests down.
  PARAMETERS:
   request -
  RETURN VALUE: none
  ***************************************************************************/
  void cancel_receive_request_have_lock (mcapi_request_t *request)
  {
	  uint16_t rd,rn,re,r;
	  int i,last,start,curr;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  mcapi_assert(mcapi_trans_decode_request_handle(request,&r));
	  mcapi_assert(mcapi_trans_decode_handle(mcapi_db->requests[r].handle,&rd,&rn,&re));
	  for (i = 0; i < MCAPI_MAX_QUEUE_ENTRIES; i++) {
		if (mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[i].request == *request) {
		  /* we found the request, now clear the reservation */
		  mcapi_dprintf(5,"cancel_receive_request - cancelling request at index %i BEFORE:",i);
		  //print_queue(c_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue);
		  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[i].request = 0;
		  break;
		}
	  }

	  /* we should have found the outstanding request */
	  mcapi_assert (i != MCAPI_MAX_QUEUE_ENTRIES);

	  /* shift all pending reservations down*/
	  start = i;
	  last = start;
	  for (i = 0; i < MCAPI_MAX_QUEUE_ENTRIES; i++) {
		curr = (i+start)%MCAPI_MAX_QUEUE_ENTRIES;
		/* don't cross over the head or the tail */
		if ((curr == mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.tail) &&
			(curr != start)) {
		  break;
		}
		if ((curr == mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.head) &&
			(curr != start)) {
		  break;
		}
		if (mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[curr].request) {
		  mcapi_dprintf(5,"cancel_receive_request - shifting request at index %i to index %i",curr,last);
		  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[last].request =
			mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[curr].request;
		  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[curr].request = 0;
		  last = curr;
		}
	  }

	  mcapi_db->requests[r].cancelled = MCAPI_TRUE;
  }

  /***************************************************************************
  NAME: check_receive_request
  DESCRIPTION: Checks if the given non-blocking receive request has completed.
  	This is a little tricky because we can't just pop from the head of the
  	endpoints receive queue.  We have to locate the reservation that was
  	made in the queue (to preserve FIFO) at the time the request was made.
  PARAMETERS: the request pointer (to be filled in)
  RETURN VALUE: none
  ***************************************************************************/
  void check_receive_request_have_lock (mcapi_request_t *request)
  {
	  uint16_t rd,rn,re,r;
	  int i;
	  size_t size;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  int32_t index=-1;
	  mcapi_assert(mcapi_trans_decode_request_handle(request,&r));
	  mcapi_assert(mcapi_trans_decode_handle(mcapi_db->requests[r].handle,&rd,&rn,&re));
	  for (i = 0; i < MCAPI_MAX_QUEUE_ENTRIES; i++) {

		if (mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[i].request == *request) {
		  /* we found the request, check to see if there is valid data in the receive queue entry */
		  if (mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[i].buff_index ) {
			/* clear the request reservation */
			mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[i].request = 0;
			/* shared memory is zeroed, so we store our index as index+1 so that we can tell if it's valid or not*/
			index = mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[i].buff_index &~ MCAPI_VALID_MASK;
			/* update the request */
			mcapi_db->requests[r].completed = MCAPI_TRUE;
			mcapi_db->requests[r].status = MCAPI_SUCCESS;
			/* first take the entry out of the queue  this has the potential to fragment our
			   receive queue since we may not be removing from the head */
			if ( mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.channel_type == MCAPI_PKT_CHAN) {
			  /* packet buffer means system buffer, so save the users pointer to the buffer */
			  mcapi_trans_recv_have_lock_ (rd,rn,re,mcapi_db->requests[r].buffer_ptr,mcapi_db->requests[r].size,&mcapi_db->requests[r].size,i,NULL);
			} else {
			  /* message buffer means user buffer, so save the users buffer */
			  size = mcapi_db->requests[r].size;
			  mcapi_trans_recv_have_lock_ (rd,rn,re,&mcapi_db->requests[r].buffer,mcapi_db->requests[r].size,&mcapi_db->requests[r].size,i,NULL);
			  if (mcapi_db->requests[r].size > size) {
				mcapi_db->requests[r].size = size;
				mcapi_db->requests[r].status = MCAPI_ERR_MSG_TRUNCATED;
			  }
			}
			/* now update the receive queue state */
			mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.num_elements--;
			/* mark this entry as invalid so that the "bubble" won't be re-used */
			mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[i].invalid = MCAPI_TRUE;
			mcapi_trans_compact_queue (&mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue);
			mcapi_dprintf(4,"receive request (test/wait) popped from qindex=%i, num_elements=%i, head=%i, tail=%i",
						  i,mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.num_elements,
						  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.head,
						  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.tail);
		  }
		  break;
		}
	  }
	  /* we should have found the outstanding request */
	  mcapi_assert (i != MCAPI_MAX_QUEUE_ENTRIES);
  }



  /***************************************************************************
  NAME:mcapi_trans_connect_channel_have_lock
  DESCRIPTION: connects a channel
  PARAMETERS:
   send_endpoint
   receive_endpoint
   type
  RETURN VALUE:none
  ***************************************************************************/
  void mcapi_trans_connect_channel_have_lock (mcapi_endpoint_t send_endpoint,
  										 mcapi_endpoint_t receive_endpoint,
  										 channel_type type)
  {
	  uint16_t sd,sn,se;
	  uint16_t rd,rn,re;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  mcapi_assert(mcapi_trans_decode_handle(send_endpoint,&sd,&sn,&se));
	  mcapi_assert(mcapi_trans_decode_handle(receive_endpoint,&rd,&rn,&re));
	  mcapi_assert((sd == my_domain_id && sn == my_node_id) || (rd == my_domain_id && rn == my_node_id));

	  // both endpoints can also belong to the same node
	  if(sd == my_domain_id && sn == my_node_id)
	  {
		  /* update the send endpoint */
		  mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].connected = MCAPI_TRUE;
		  mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.recv_endpt = receive_endpoint;
		  mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.send_endpt = send_endpoint;
		  mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].recv_queue.channel_type = type;
	  }

	  if(rd == my_domain_id && rn == my_node_id)
	  {
		  /* update the receive endpoint */
		  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].connected = MCAPI_TRUE;
		  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.send_endpt = send_endpoint;
		  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.recv_endpt = receive_endpoint;
		  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.channel_type = type;
	  }

	  mcapi_dprintf(1,"channel_type=%u connected sender (node=%u,port=%u) to receiver (node=%u,port=%u)",
					type,mcapi_db->domains[sd].nodes[sn].node_num,
					mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].port_num,
					mcapi_db->domains[rd].nodes[rn].node_num,
					mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].port_num);
  }

  /***************************************************************************
  NAME:mcapi_trans_send
  DESCRIPTION: Attempts to send a message from one endpoint to another
  PARAMETERS:
  sn - the send node index (only used for verbose debug print)
  se - the send endpoint index (only used for verbose debug print)
  rn - the receive node index
  re - the receive endpoint index
  buffer -
  buffer_size -

  RETURN VALUE: true/false indicating success or failure
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_send (uint16_t sd, uint16_t sn,uint16_t se,
  				 uint16_t rd,uint16_t rn, uint16_t re,
  				 const char* buffer,
  				 size_t buffer_size)
  {
	  int qindex,i;
	  buffer_entry* db_buff = NULL;

	  // lock the database
	  mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE);

	  mcapi_dprintf(3,"mcapi_trans_send_have_lock sender (node=%u,port=%u) to receiver (node=%u,port=%u) ",
					mcapi_db->domains[sd].nodes[sn].node_num,
					mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].port_num,
					mcapi_db->domains[rd].nodes[rn].node_num,
					mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].port_num);

	  /* for packets or scalars, check if channel is connected and open */
	  if (mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.channel_type != MCAPI_NO_CHAN ) {
		  if(!(mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].connected)
		     || !(mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].open))
		  {
			  // unlock the database
			  mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE);

			  return MCAPI_FALSE;
		  }
	  }

	if (mcapi_trans_full_queue(mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue)) {
		/* we couldn't get space in the endpoints receive queue, try to compact the queue */
		mcapi_trans_compact_queue(&mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue);

		// unlock the database
		mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE);
		return MCAPI_FALSE;
	}

	/* find a free mcapi buffer (we only have to worry about this on the sending side) */
	for (i = 0; i < MCAPI_MAX_BUFFERS; i++) {
		if (!mcapi_db->buffers[i].magic_num) {
			mcapi_db->buffers[i].magic_num = MAGIC_NUM;
			db_buff = &mcapi_db->buffers[i];
			mcapi_dprintf(4,"using buffer index i=%u",i);
			break;
		}
	}
	if (i == MCAPI_MAX_BUFFERS) {
		/* we couldn't get a free buffer */
		mcapi_dprintf(2,"ERROR mcapi_trans_send_have_lock: No more buffers available - try freeing some buffers. ");

		// unlock the database
		mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE);

		return MCAPI_FALSE;
	}

	/* now go about updating buffer into the database... */
	/* find the next index in the circular queue */
	qindex = mcapi_trans_push_queue(&mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue);
	mcapi_dprintf(4,"send pushing %u byte buffer to qindex=%i, num_elements=%i, head=%i, tail=%i",
			buffer_size,qindex,mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.num_elements,
			mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.head,
			mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.tail);

	/* printf(" send pushing to qindex=%i",qindex); */
	if (mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.channel_type == MCAPI_SCL_CHAN ) {
		memcpy(&(db_buff->scalar), buffer, buffer_size);
	}
	else {
		/* copy the buffer parm into a mcapi buffer */
		memcpy (db_buff->buff, buffer, buffer_size);
	}
	/* set the size */
	db_buff->size = buffer_size;

	/* update the ptr in the receive_endpoints queue to point to our mcapi buffer */
	/* shared memory is zeroed, so we store our index as index with a valid bit */
	/* so that we can tell if it's valid or not*/
	mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[qindex].buff_index = i | MCAPI_VALID_MASK;

	// unlock the database
	mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE);

	return MCAPI_TRUE;
}


  /***************************************************************************
  NAME:mcapi_trans_send_have_lock
  DESCRIPTION: Attempts to send a message from one endpoint to another
  PARAMETERS:
  sn - the send node index (only used for verbose debug print)
  se - the send endpoint index (only used for verbose debug print)
  rn - the receive node index
  re - the receive endpoint index
  buffer -
  buffer_size -

  RETURN VALUE:
  - MCAPI_SUCCESS       in case of success
  - MCAPI_CHAN_NCNO     if channel is not connected or not open
  - MCAPI_ERR_MEM_LIMIT if receive queue is temporarilly full or if there
                        is no free buffer available
  ***************************************************************************/
  int mcapi_trans_send_have_lock (uint16_t sd, uint16_t sn, uint16_t se,
  										 uint16_t rd,uint16_t rn, uint16_t re,
  										 const char* buffer,
  										 size_t buffer_size,
  										 uint64_t scalar)
  {
	  int qindex,i;
	  buffer_entry* db_buff = NULL;

	  mcapi_dprintf(3,"mcapi_trans_send_have_lock sender (node=%u,port=%u) to receiver (node=%u,port=%u) ",
					mcapi_db->domains[sd].nodes[sn].node_num,
					mcapi_db->domains[sd].nodes[sn].node_d.endpoints[se].port_num,
					mcapi_db->domains[rd].nodes[rn].node_num,
					mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].port_num);

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  /* for packets or scalars, check if channel is connected and open */
	  if (mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.channel_type != MCAPI_NO_CHAN ) {
		  if(!(mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].connected)
		     || !(mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].open))
		  {
			  return MCAPI_ERR_CHAN_NCNO;
		  }
	  }

	  if (mcapi_trans_full_queue(mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue)) {
		/* we couldn't get space in the endpoints receive queue, try to compact the queue */
		mcapi_trans_compact_queue(&mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue);
		return MCAPI_ERR_MEM_LIMIT;
	  }

	  /* find a free mcapi buffer (we only have to worry about this on the sending side) */
	  for (i = 0; i < MCAPI_MAX_BUFFERS; i++) {
		if (!mcapi_db->buffers[i].magic_num) {
		  mcapi_db->buffers[i].magic_num = MAGIC_NUM;
		  db_buff = &mcapi_db->buffers[i];
		  mcapi_dprintf(4,"using buffer index i=%u",i);
		  break;
		}
	  }
	  if (i == MCAPI_MAX_BUFFERS) {
		/* we couldn't get a free buffer */
		mcapi_dprintf(2,"ERROR mcapi_trans_send_have_lock: No more buffers available - try freeing some buffers. ");
		return MCAPI_ERR_MEM_LIMIT;
	  }

	  /* now go about updating buffer into the database... */
	  /* find the next index in the circular queue */
	  qindex = mcapi_trans_push_queue(&mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue);
	  mcapi_dprintf(4,"send pushing %u byte buffer to qindex=%i, num_elements=%i, head=%i, tail=%i",
					buffer_size,qindex,mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.num_elements,
					mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.head,
					mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.tail);
	  /* printf(" send pushing to qindex=%i",qindex); */
	  if (mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.channel_type == MCAPI_SCL_CHAN ) {
		db_buff->scalar = scalar;
	  } else {
		/* copy the buffer parm into a mcapi buffer */
		memcpy (db_buff->buff,buffer,buffer_size);
	  }
	  /* set the size */
	  db_buff->size = buffer_size;
	  /* update the ptr in the receive_endpoints queue to point to our mcapi buffer */
	  /* shared memory is zeroed, so we store our index as index with a valid bit so that we can tell if it's valid or not*/
	  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[qindex].buff_index = i | MCAPI_VALID_MASK;

	  return MCAPI_SUCCESS;
  }

  /***************************************************************************
    NAME:  mcapi_trans_recv_have_lock_
    DESCRIPTION: Removes a message (at the given qindex) from the given
      receive endpoints queue.  This function is used both by check_receive_request
      and mcapi_trans_recv_have_lock.  We needed to separate the functionality
      because in order to preserve FIFO, if recv was called to an empty queue we
      had to set a reservation at the head of the queue.  Thus we can't always
      just pop from the head of the queue.
    PARAMETERS:
      rn - the receive node index
      re - the receive endpoint index
      buffer -
      buffer_size -
      received_size - the actual size (in bytes) of the data received
      qindex - index into the receive endpoints queue that we should remove from
    RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_recv_have_lock_ (uint16_t rd,uint16_t rn, uint16_t re, void** buffer, size_t buffer_size,
  							   size_t* received_size,int qindex,uint64_t* scalar)
  {
	  size_t size;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  /* shared memory is zeroed, so we store our index as index w/ a valid bit so that we can tell if it's valid or not*/
	  int index = mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[qindex].buff_index &~ MCAPI_VALID_MASK;
	  mcapi_assert (index >= 0);

	  mcapi_dprintf(3,"mcapi_trans_recv_have_lock_ for receiver (node=%u,port=%u)",
					mcapi_db->domains[rd].nodes[rn].node_num,
					mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].port_num);

	  /* printf(" recv popping from qindex=%i",qindex); */
	  /* first make sure buffer is big enough for the message */
	  if ((buffer_size) < mcapi_db->buffers[index].size) {
		fprintf(stderr,"ERROR: mcapi_trans_recv_have_lock buffer not big enough - loss of data: buffer_size=%i, element_size=%i",
				(int)buffer_size,
				(int)mcapi_db->buffers[index].size);
		/* NOTE: MCAPI_ETRUNCATED will be set by the calling functions by noticing that buffer_size < received_size */
	  }

	  /* set the size */
	  size = mcapi_db->buffers[index].size;

	  /* fill in the size */
	  *received_size = size;
	  if (buffer_size < size) {
		size = buffer_size;
	  }

	  /* copy the buffer out of the receive_endpoint's queue and into the buffer parm */
	  if (mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.channel_type == MCAPI_PKT_CHAN) {
		/* mcapi supplied buffer (pkt receive), so just update the pointer */
		*buffer = mcapi_db->buffers[index].buff;
	  } else {
		/* user supplied buffer, copy it in and free the mcapi buffer */
		if   (mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.channel_type == MCAPI_SCL_CHAN) {
		  /* scalar receive */
		  *scalar = mcapi_db->buffers[index].scalar;
		} else {
		  /* msg receive */
		  memcpy (*buffer,mcapi_db->buffers[index].buff,size);
		}
		/* free the mcapi  buffer */
		memset(&mcapi_db->buffers[index],0,sizeof(mcapi_db->buffers[index]));
	  }
	  mcapi_dprintf(4,"receive popping %u byte buffer from qindex=%i, num_elements=%i, head=%i, tail=%i buffer=[",
					size,qindex,mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.num_elements,
					mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.head,
					mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.tail);

	  /* clear the buffer pointer in the receive queue entry */
	  mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue.elements[qindex].buff_index = 0;
  }

  /***************************************************************************
   NAME: mcapi_trans_recv_have_lock
   DESCRIPTION: checks if a message is available, if so performs the pop (from
    the head of the queue) and sends the qindex to be used to mcapi_trans_recv_have_lock_
   PARAMETERS:
     rn - the receive node index
     re - the receive endpoint index
     buffer -
     buffer_size -
     received_size - the actual size (in bytes) of the data received
     blocking - whether or not this is a blocking receive
   RETURN VALUE: true/false indicating success or failure
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_recv_have_lock (uint16_t rd,uint16_t rn, uint16_t re, void** buffer,
  										size_t buffer_size, size_t* received_size,
  										mcapi_boolean_t blocking,uint64_t* scalar)
  {
	  int qindex;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  if ((!blocking) && (mcapi_trans_empty_queue(mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue))) {
		return MCAPI_FALSE;
	  }

	  while (mcapi_trans_empty_queue(mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue)) {
		mcapi_dprintf(5,"mcapi_trans_recv_have_lock to empty queue - attempting to yield");
		/* we have the lock, use this yield */
		mcapi_trans_yield_have_lock();
	  }

	  /* remove the element from the receive endpoints queue */
	  qindex = mcapi_trans_pop_queue(&mcapi_db->domains[rd].nodes[rn].node_d.endpoints[re].recv_queue);
	  mcapi_trans_recv_have_lock_ (rd,rn,re,buffer,buffer_size,received_size,qindex,scalar);

	  return MCAPI_TRUE;
  }

  /***************************************************************************
  NAME: mcapi_trans_open_channel_have_lock
  DESCRIPTION: marks the given endpoint as open
  PARAMETERS:
    n - the node index
    e - the endpoint index
  RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_open_channel_have_lock (uint16_t d,uint16_t n, uint16_t e)
  {

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  /* mark the endpoint as open */
	  mcapi_db->domains[d].nodes[n].node_d.endpoints[e].open = MCAPI_TRUE;
  }

  /***************************************************************************
  NAME:mcapi_trans_close_channel_have_lock
  DESCRIPTION: marks the given endpoint as closed
  PARAMETERS:
    n - the node index
    e - the endpoint index
  RETURN VALUE:none
  ***************************************************************************/
  void mcapi_trans_close_channel_have_lock (uint16_t d,uint16_t n, uint16_t e)
  {

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  /* mark the endpoint as closed */
	  mcapi_db->domains[d].nodes[n].node_d.endpoints[e].open = MCAPI_FALSE;
  }

  /***************************************************************************
  NAME:mcapi_trans_yield_have_lock
  DESCRIPTION: releases the lock, attempts to yield, re-acquires the lock.
  PARAMETERS: none
  RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_yield_have_lock ()
  {
	  // call this version of sched_yield when you have the lock
#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  // release the lock
	  mcapi_trans_access_database_post(global_rwl,MCAPI_TRUE);

	  /* yield */
#ifdef UCOSII
		OSTimeDly(TICKS_TO_WAIT);
#endif

#ifdef LINUX
		sched_yield();
#endif
	  //re-acquire the lock
	  mcapi_trans_access_database_pre(global_rwl,MCAPI_TRUE);

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

  }

  /***************************************************************************
  NAME: mcapi_trans_access_database_pre
  DESCRIPTION: This function will lock the database related mutex in order
               to ensure exclusive database access
  PARAMETERS: none
  RETURN VALUE:none
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_access_database_pre (uint32_t handle, mcapi_boolean_t exclusive)
  {
	// acquire locking mutex
#ifdef UCOSII
	uint8_t	err;
	OSMutexPend(DatabaseMutex, 0, &err);
	if(err != OS_NO_ERR) {
		  if (err == OS_ERR_PEVENT_NULL) { return MCAPI_FALSE; }
			  locked = MCAPI_FALSE;
			  printf("Error in mcapi_trans_nios: OSMutexPend() failed");
			  return MCAPI_FALSE;
	}
#endif

#ifdef LINUX
	int err;
	err = pthread_mutex_lock(&DatabaseMutex);
  	if(err != 0) {
  		//if (err == OS_ERR_PEVENT_NULL) { return MCAPI_FALSE; }
		locked = MCAPI_FALSE;
		printf("Error in mcapi_trans_nios: pthread_mutex_lock() failed");
		return MCAPI_FALSE;
	}
#endif

	locked = MCAPI_TRUE;
  	return MCAPI_TRUE;
  }

  /***************************************************************************
  NAME:mcapi_trans_access_database_post
  DESCRIPTION: This function releases the semaphore.
  PARAMETERS: none
  RETURN VALUE: none
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_access_database_post (uint32_t handle,
                                                    mcapi_boolean_t exclusive)
  {
	// release locking mutex
#ifdef UCOSII
	uint8_t err;
	err = OSMutexPost(DatabaseMutex);
	if(err != OS_NO_ERR) {
		if (err == OS_ERR_PEVENT_NULL) { return MCAPI_FALSE; }
		locked = MCAPI_TRUE;
		printf("Error in mcapi_trans_nios: OSMutexPost() failed");
		return MCAPI_FALSE;
	}
#endif

#ifdef LINUX
	int err;
	err = pthread_mutex_unlock(&DatabaseMutex);
	if(err != 0) {
	  //if (err == OS_ERR_PEVENT_NULL) { return MCAPI_FALSE; }
	  locked = MCAPI_TRUE;
	  printf("Error in mcapi_trans_nios: pthread_mutex_unlock() failed");
	  return MCAPI_FALSE;
	}
#endif

	locked = MCAPI_FALSE;
	return MCAPI_TRUE;
  }

  /***************************************************************************
    NAME:mcapi_trans_add_node
    DESCRIPTION: Adds a node to the database (called by intialize)
    PARAMETERS: node_num
    RETURN VALUE: true/false indicating success or failure
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_add_domain_and_node (mcapi_domain_t domain_id, mcapi_node_t node_id, const mcapi_node_attributes_t* node_attrs)
  {
    mcapi_boolean_t domain_exists = MCAPI_TRUE;

    /* check if domain id is compliant */
    if (domain_id != MCA_MAX_DOMAINS)
	{
		/* check if this domain already exists */
		if (mcapi_db->domains[domain_id].valid == MCAPI_FALSE || mcapi_db->domains[domain_id].domain_id != domain_id)
		{
				/* it didn't exist so set the domain flag */
				domain_exists = MCAPI_FALSE;
		}
	}
    else
	{
		/* we didn't find an available domain index */
		  mcapi_dprintf(1,"You have hit MCA_MAX_DOMAINS, either use less domains or reconfigure with more domains");
		  return MCAPI_FALSE;
	}

    /* check if node id is compliant */
    if (node_id != MCA_MAX_NODES)
    {
		/* now see if this node already exists */
		if ((mcapi_db->domains[domain_id].nodes[node_id].valid == MCAPI_TRUE )&&
			(mcapi_db->domains[domain_id].nodes[node_id].node_num == node_id)) {
		  /* this node already exists for this domain */
		  mcapi_dprintf(1,"This node (%d) already exists for this domain(%d)",node_id,domain_id);
		  return MCAPI_FALSE;
		}
		else
		{
			/* add the caller to the database*/
			/* set the domain */
			if(domain_exists == MCAPI_FALSE)
			{
				mcapi_db->domains[domain_id].domain_id = domain_id;
				mcapi_db->domains[domain_id].valid = MCAPI_TRUE;
			}

			/* set the node */
			mcapi_db->domains[domain_id].nodes[node_id].valid = MCAPI_TRUE;
			mcapi_db->domains[domain_id].nodes[node_id].node_num = node_id;
			mcapi_db->domains[domain_id].num_nodes++;

			/* set the node attributes */
			if (node_attrs != NULL) {
			  memcpy(&mcapi_db->domains[domain_id].nodes[node_id].attributes,
					 node_attrs,
					 sizeof(mcapi_node_attributes_t));
			}

			/* initialize the attribute size for the only attribute we support */
			mcapi_db->domains[domain_id].nodes[node_id].attributes.entries[MCAPI_NODE_ATTR_TYPE_REGULAR].bytes = sizeof(mcapi_node_attr_type_t);

			int e = 0;
			for (e = 0; e < MCAPI_MAX_ENDPOINTS; e++) {
			  /* zero out all the endpoints */


			  memset (&mcapi_db->domains[domain_id].nodes[node_id].node_d.endpoints[e],0,sizeof(endpoint_entry));
			}
		}
    }
    else
    {
    	/* we didn't find an available node index */
		  mcapi_dprintf(1,"You have hit MCA_MAX_NODES, either use less nodes or reconfigure with more nodes.");
		  return MCAPI_FALSE;
    }

    return MCAPI_TRUE;
  }

  /***************************************************************************
  NAME:mcapi_trans_encode_handle
  DESCRIPTION:

  Our handles are very simple - a 32 bit integer is encoded with
  an index (16 bits gives us a range of 0:64K indices).
  Currently, we only have 3 indices for each of: domain array,
  node array, and endpoint array.
  PARAMETERS:
  node_index -
  endpoint_index -
  RETURN VALUE: the handle
  ***************************************************************************/
  uint32_t mcapi_trans_encode_handle (uint16_t domain_index,uint16_t node_index,uint16_t endpoint_index)
  {
	  uint32_t handle = 0;
	  uint8_t shift = 8;

	  mcapi_assert ((domain_index < MCA_MAX_DOMAINS) &&
				    (node_index < MCA_MAX_NODES) &&
			        (endpoint_index < MCAPI_MAX_ENDPOINTS));

	  handle = domain_index;
	  handle <<= shift;
	  handle |= node_index;
	  handle <<= shift;
	  handle |= endpoint_index;

	  return handle;
  }

  /***************************************************************************
  NAME:mcapi_trans_decode_handle
  DESCRIPTION: Decodes the given handle into it's database indices
  PARAMETERS:
  handle -
  node_index -
  endpoint_index -
  RETURN VALUE: true/false indicating success or failure
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_decode_handle (uint32_t handle, uint16_t *domain_index,uint16_t *node_index,
  												  uint16_t *endpoint_index)
  {
	  int rc = MCAPI_FALSE;
	  uint8_t shift = 8;

	  *domain_index            = (handle & 0x00ff0000) >> (shift * 2);
	  *node_index              = (handle & 0x0000ff00) >> shift;
	  *endpoint_index          = (handle & 0x000000ff);

	  if ((*domain_index < MCA_MAX_DOMAINS) &&
		  (*node_index < MCA_MAX_NODES) &&
		  (*endpoint_index < MCAPI_MAX_ENDPOINTS)) {
		  rc = MCAPI_TRUE;
	  }

	  return rc;
  }

  /***************************************************************************
	Function: mcapi_trans_whoami

	Description: looks up the corresponding node and domain indices in our database.

	Parameters:

	Returns: boolean indicating success or failure

	***************************************************************************/
  mcapi_boolean_t mcapi_trans_get_database_indices(uint32_t* d_index, uint32_t* n_index, mcapi_domain_t domain_id, mcapi_node_t node_id)
  {
	  mcapi_boolean_t rc = MCAPI_FALSE;
	  if (mcapi_db == NULL) { return rc;}

	  int d, n = 0;

	  for (d = 0; d < MCA_MAX_DOMAINS; d++)
	  {
		  if(mcapi_db->domains[d].valid == MCAPI_TRUE && domain_id == mcapi_db->domains[d].domain_id)
		  {
			  for (n = 0; n < MCA_MAX_NODES; n++)
			  {
				  if(mcapi_db->domains[d].nodes[n].valid == MCAPI_TRUE && node_id == mcapi_db->domains[d].nodes[n].node_num)
				  {
					  *d_index = d;
					  *n_index = n;
					  return MCAPI_TRUE;
				  }
			  }
		  }
	  }

	  return rc;
  }

  /***************************************************************************
  Function: mcapi_trans_whoami

  Description: looks up the corresponding node and domain info in our database.

  Parameters:

  Returns: boolean indicating success or failure

  ***************************************************************************/
  inline mcapi_boolean_t mcapi_trans_whoami (mcapi_node_t* node_id,uint32_t* n_index,
                                                      mcapi_domain_t* domain_id,uint32_t* d_index)
  {
	  if (mcapi_db == NULL) { return MCAPI_FALSE;}
	  *node_id = my_node_id;
	  *n_index = my_mcapi_nindex;
	  *domain_id = my_domain_id;
	  *d_index = my_mcapi_dindex;
	  return MCAPI_TRUE;
  }

  //////////////////////////////////////////////////////////////////////////////
  //                                                                          //
  //                   queue management                                       //
  //                                                                          //
  //////////////////////////////////////////////////////////////////////////////
  /***************************************************************************
    NAME: print_queue
    DESCRIPTION: Prints an endpoints receive queue (useful for debugging)
    PARAMETERS: q - the queue
    RETURN VALUE: none
  ***************************************************************************/
  void print_queue (queue q)
  {
	  int i,qindex,index;
	  uint16_t r;
	  /*print the recv queue from head to tail*/
	  printf("\n      recv_queue:");
	  for (i = 0; i < MCAPI_MAX_QUEUE_ENTRIES; i++) {
		/* walk the queue from the head to the tail */
		qindex = (q.head + i) % (MCAPI_MAX_QUEUE_ENTRIES);
		printf("\n          ----------------QINDEX: %i",qindex);
		if (q.head == qindex) { printf("\n           *** HEAD ***"); }
		if (q.tail == qindex) { printf("\n           *** TAIL ***"); }
		printf("\n          request:0x%lx",(long unsigned int)q.elements[qindex].request);
		if (q.elements[qindex].request) {
		  r = q.elements[qindex].request;
		  printf("\n             valid:%u",mcapi_db->requests[r].valid);
		  printf("\n             size:%u",(int)mcapi_db->requests[r].size);
		  switch (mcapi_db->requests[r].type) {
		  case (OTHER): printf("\n             type:OTHER"); break;
		  case (SEND): printf("\n             type:SEND"); break;
		  case (RECV): printf("\n             type:RECV"); break;
		  case (GET_ENDPT): printf("\n             type:GET_ENDPT"); break;
		  default:  printf("\n             type:UNKNOWN!!!"); break;
		  };
		  printf("\n             buffer:[%s]",(char*)mcapi_db->requests[r].buffer);
		  printf("\n             buffer_ptr:0x%lx",(long unsigned int)mcapi_db->requests[r].buffer_ptr);
		  printf("\n             completed:%u",mcapi_db->requests[r].completed);
		  printf("\n             cancelled:%u",mcapi_db->requests[r].cancelled);
		  printf("\n             handle:0x%i",(int)mcapi_db->requests[r].handle);
		  /*   printf("\n             status:%s",mcapi_display_status(c_db->requests[r].status)); */
		  printf("\n             status:%i",(int)mcapi_db->requests[r].status);
		  printf("\n             endpoint:0x%lx",(long unsigned int)mcapi_db->requests[r].ep_endpoint);
		}
		printf("\n          invalid:%u",q.elements[qindex].invalid);

		printf("\n          b:0x%lx",(long unsigned int)q.elements[qindex].buff_index);
		if (q.elements[qindex].buff_index) {
		  index = q.elements[qindex].buff_index &~ MCAPI_VALID_MASK;
		  printf("\n             size:%u",(unsigned)mcapi_db->buffers[index].size);
		  printf("\n             magic_num:%x",(unsigned)mcapi_db->buffers[index].magic_num);
		  printf("\n             buff:[%s]",(char*)mcapi_db->buffers[index].buff);
		}
	  }
  }

  /***************************************************************************
    NAME: push_queue
    DESCRIPTION: Returns the qindex that should be used for adding an element.
       Also updates the num_elements, and tail pointer.
    PARAMETERS: q - the queue pointer
    RETURN VALUE: the qindex to be used
  ***************************************************************************/
  int mcapi_trans_push_queue(queue* q)
  {
	  int i;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  if ( (q->tail + 1) % MCAPI_MAX_QUEUE_ENTRIES == q->head) {
		/* mcapi_assert (q->num_elements ==  MCAPI_MAX_QUEUE_ENTRIES);*/
		mcapi_assert(!"push_queue called on full queue");
	  }
	  q->num_elements++;
	  i = q->tail;
	  q->tail = ++q->tail % MCAPI_MAX_QUEUE_ENTRIES;
	  mcapi_assert (q->head != q->tail);
	  return i;
  }

  /***************************************************************************
    NAME: pop_queue
    DESCRIPTION: Returns the qindex that should be used for removing an element.
       Also updates the num_elements, and head pointer.
    PARAMETERS: q - the queue pointer
    RETURN VALUE: the qindex to be used
  ***************************************************************************/
  int mcapi_trans_pop_queue (queue* q)
  {
	  int i,qindex;
	  int x = 0;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  if (q->head == q->tail) {
		/*mcapi_assert (q->num_elements ==  0);*/
		mcapi_assert (!"pop_queue called on empty queue");
	  }

	  /* we can't just pop the first element off the head of the queue, because it
		 may be reserved for an earlier recv call, we need to take the first element
		 that doesn't already have a request associated with it.  This can fragment
		 our queue. */
	  for (i = 0; i < MCAPI_MAX_QUEUE_ENTRIES; i++) {
		/* walk the queue from the head to the tail */
		qindex = (q->head + i) % (MCAPI_MAX_QUEUE_ENTRIES);
		if ((!q->elements[qindex].request) &&
			(q->elements[qindex].buff_index)){
		  x = qindex;
		  break;
		}
	  }
	  if (i == MCAPI_MAX_QUEUE_ENTRIES) {
		/* all of this endpoint's buffers already have requests associated with them */
		mcapi_assert(0); /* mcapi_trans_empty_queue should have already checked for this case */
	  }

	  q->num_elements--;

	  /* if we are removing from the front of the queue, then move head */
	  if (x == q->head) {
		q->head = ++q->head % MCAPI_MAX_QUEUE_ENTRIES;
	  } else {
		/* we are fragmenting the queue, mark this entry as invalid */
		q->elements[qindex].invalid = MCAPI_TRUE;
	  }

	  if (q->num_elements > 0) {
		if (q->head == q->tail) { printf("num_elements=%d",q->num_elements); }
		mcapi_assert (q->head != q->tail);
	  }

	  mcapi_trans_compact_queue (q);

	  return x;
  }

  /***************************************************************************
    NAME: compact_queue
    DESCRIPTION: Attempts to compact the queue.  It can become fragmented based
       on the order that blocking/non-blocking sends/receives/tests come in
    PARAMETERS: q - the queue pointer
    RETURN VALUE: none
  ***************************************************************************/
  void mcapi_trans_compact_queue (queue* q)
  {
	  int i;
	  int qindex;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  mcapi_dprintf(7,"before mcapi_trans_compact_queue head=%i,tail=%i,num_elements=%i",q->head,q->tail,q->num_elements);
	  for (i = 0; i < MCAPI_MAX_QUEUE_ENTRIES; i++) {
	  qindex = (q->head + i) % (MCAPI_MAX_QUEUE_ENTRIES);
	  if ((qindex == q->tail) ||
		  (q->elements[qindex].request) ||
		  (q->elements[qindex].buff_index )){
		break;
	  } else {
		/* advance the head pointer */
		q->elements[qindex].invalid = MCAPI_FALSE;
		q->head = ++q->head % MCAPI_MAX_QUEUE_ENTRIES;
		i--;
	  }
	  }
	  mcapi_dprintf(7,"after mcapi_trans_compact_queue head=%i,tail=%i,num_elements=%i",q->head,q->tail,q->num_elements);
	  if (q->num_elements > 0) {
		mcapi_assert (q->head != q->tail);
	  }
  }

  /***************************************************************************
    NAME: mcapi_trans_empty_queue
    DESCRIPTION: Checks if the queue is empty or not
    PARAMETERS: q - the queue
    RETURN VALUE: true/false
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_empty_queue (queue q)
  {
	  int i,qindex;

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  if  (q.head == q.tail) {
		/* mcapi_assert (q.num_elements ==  0); */
		return MCAPI_TRUE;
	  }

	  /* if we have any buffers in our queue that don't have
		 reservations, then our queue is non-empty */
	  for (i = 0; i < MCAPI_MAX_QUEUE_ENTRIES; i++) {
		qindex = (q.head + i) % (MCAPI_MAX_QUEUE_ENTRIES);
		if ((!q.elements[qindex].request) &&
			(q.elements[qindex].buff_index )){
		  break;
		}
	  }
	  if (i == MCAPI_MAX_QUEUE_ENTRIES) {
		return MCAPI_TRUE;
	  }

	  return MCAPI_FALSE;
  }

  /***************************************************************************
    NAME: mcapi_trans_full_queue
    DESCRIPTION: Checks if the queue is full or not
    PARAMETERS: q - the queue
    RETURN VALUE: true/false
  ***************************************************************************/
  mcapi_boolean_t mcapi_trans_full_queue (queue q)
  {

#ifdef DEBUG_LOCK_CHECK
	/* database should be locked */
//	if(DatabaseMutex != 0) assert(locked == MCAPI_TRUE);
	if(pthread_mutex_trylock(&DatabaseMutex) != EBUSY) assert(locked == MCAPI_TRUE);
#endif

	  if ( (q.tail + 1) % MCAPI_MAX_QUEUE_ENTRIES == q.head) {
		/*  mcapi_assert (q.num_elements ==  (MCAPI_MAX_QUEUE_ENTRIES -1)); */
		return MCAPI_TRUE;
	  }
	  return MCAPI_FALSE;
  }

#ifdef __cplusplus
}
#endif /* __cplusplus */
