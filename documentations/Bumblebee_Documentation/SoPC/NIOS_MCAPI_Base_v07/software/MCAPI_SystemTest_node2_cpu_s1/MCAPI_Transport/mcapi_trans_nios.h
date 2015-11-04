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
 File             : mcapi_trans_nios.h
 Author           : Eugen Harass
 Date             : 22.02.2013
 Decription       : MCAPI-transport-layer implementation for NIOS II
					(based on the MCA example shared memory implementation)

 Edition History:
 2013-02-22: MCAPI Implementation V0.1 based on MCAPI-specification V2.015
 2014-09-30: Changes in buffer_entry structure because of compiler
             dependent buggy pointer arithmetics in function
             mcapi_trans_pktchan_free() - ms
****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef _TRANSPORT_NIOS_H_
#define _TRANSPORT_NIOS_H_

#include <stdarg.h> /* for va_list */
#include <stdio.h> /* for the inlined dprintf routine */
#include <stdlib.h>

#include "../MCAPI_Top/mcapi.h"
#include "../MCAPI_Top/mca_config.h"
#include "../MCAPI_Top/mcapi_impl_spec.h"

//#include "../../LowerLayerProtocols/NS_layer.h"
//#include "includes.h"

/*******************************************************************
  definitions and constants
*******************************************************************/    
/* the debug level */
extern int mcapi_debug;
//#define OS_NO_ERR 0
/* we leave one empty element so that the array implementation 
   can tell the difference between empty and full */
#define MCAPI_MAX_QUEUE_ENTRIES (MCAPI_MAX_QUEUE_ELEMENTS + 1)

#define MCAPI_MAX_REQUESTS MCA_MAX_REQUESTS

#define mcapi_dprintf mca_dprintf

#define MCAPI_MAX(X,Y) ((X) > (Y) ? (X) : (Y))

/*******************************************************************
  mcapi_trans data types
*******************************************************************/

/* buffer entry is used for msgs, pkts and scalars */
/* NOTE: if you change the buffer_entry data structure then you also
   need to update the pointer arithmetic in mcapi_trans_pktchan_free */

typedef struct {
  char buff [MCAPI_MAX(MCAPI_MAX_PKT_SIZE,MCAPI_MAX_MSG_SIZE)]; // the buffer 
											// is used for both pkts and msgs
  uint32_t magic_num;
  size_t size; /* size (in bytes) of the buffer */
  mcapi_boolean_t in_use;
  uint64_t scalar;
} buffer_entry;

typedef enum {
  OTHER,
  OPEN_PKTCHAN,
  OPEN_SCLCHAN,
  SEND,
  RECV,
  GET_ENDPT,
  CLOSE_PKTCHAN,
  CLOSE_SCLCHAN
} mcapi_request_type;

typedef struct {
  mca_boolean_t valid;
  size_t size;
  mcapi_request_type type;
  void* buffer;
  void** buffer_ptr;
  uint32_t ep_node_num; /* used only for get_endpoint */
  uint32_t ep_port_num; /* used only for get_endpoint */
  mca_domain_t ep_domain_num; /* used only for get_endpoint */
  mca_boolean_t completed;
  mca_boolean_t cancelled;
  mcapi_endpoint_t handle;
  mca_status_t status;
  mcapi_endpoint_t* ep_endpoint;
} mcapi_request_data;

typedef struct  {
	int next_index;
	int prev_index;
} indexed_array_node;

typedef struct  {
	indexed_array_node array[MCAPI_MAX_REQUESTS];
	int curr_count;
	int max_count;
	int empty_head_index;
	int full_head_index;
} indexed_array_header;

typedef struct {
  mcapi_request_t request; //angepasst /* holds a reservation for an outstanding receive request */
  int32_t buff_index; //angepasst    /* the pointer to the actual buffer entry in the buffer pool */
  mcapi_boolean_t invalid;
} buffer_descriptor;

typedef struct {  
  /* the next 3 data members are only valid for channels */
  mcapi_endpoint_t send_endpt;
  mcapi_endpoint_t recv_endpt;
  uint8_t channel_type;

  uint32_t num_elements;
  uint16_t head;
  uint16_t tail;
  buffer_descriptor elements[MCAPI_MAX_QUEUE_ENTRIES+1];
}queue;

typedef struct {
  uint32_t port_num;
  mcapi_boolean_t valid;
  mcapi_boolean_t anonymous;
  mcapi_boolean_t open;
  mcapi_boolean_t connected;
  uint32_t num_attributes;
  mcapi_endpt_attributes_t attributes; // angepasst
  queue recv_queue;
} endpoint_entry;

typedef struct {
  uint16_t num_endpoints;
  endpoint_entry endpoints[MCAPI_MAX_ENDPOINTS];
} node_descriptor;

typedef struct {
  //struct sigaction signals[MCA_MAX_SIGNALS];
  uint32_t node_num;
  mcapi_boolean_t valid;
  uint32_t num_attributes;
  mcapi_node_attributes_t attributes;
  node_descriptor node_d;
  //pid_t pid;
  //pthread_t tid;
} node_entry;

typedef struct {
  uint16_t num_nodes;
  mca_domain_t domain_id;
  mcapi_boolean_t valid;
  node_entry nodes[MCA_MAX_NODES];
} domain_entry;

typedef struct {
  domain_entry domains[MCA_MAX_DOMAINS];
  buffer_entry buffers[MCAPI_MAX_BUFFERS];
  mcapi_request_data requests[MCAPI_MAX_REQUESTS];
  // global header and array that we keep all requests
  indexed_array_node request_reserves[MCAPI_MAX_REQUESTS];
  indexed_array_header request_reserves_header;
  uint16_t num_domains;
} mcapi_database;

inline void mcapi_dprintf(int level,const char *format, ...);

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
