/**************************************************************
 * FILE:           NS_layer.h
 *
 * DESCRIPTION:
 * Public function prototypes and as well as required
 * DEFINES and data structures, necessary to use
 * NS_layer.c software are defined here.
 * Everbody who wants to use NS_layer software has to
 * include this file.
 *
 * EDITION HISTORY:
 * 2013-02-25: Implementation for MCAPI from Eugen Harass
 * 2013-12-09: lots of debugging and coding cleaning - ms
 * 2014-01-22: synchronization between uClin and uCOS - ms
 * 2014-07-29: Changes in NS_sendDataToRemote_request(),
 *             scalar value now passed in 'normal' data
 *             filed - ms
 * 2014-08-05: v042 changes and ifdef adaptation to
 *             uC/OS-II - ms
 * 2014-08-11: extern decl. of NS_layer_exit() - ms
 *************************************************************/

#ifndef NSLAYER_H_
#define NSLAYER_H_

#include "../PH_FifoDriver_UCOSII/PH_layer.h"

// NS layer may return following error/status codes:
#define NS_OK				 0	// no error, everything works fine
#define	NS_ERROR			-1	// an error occured
#define NS_NO_FREE_SERVICEREQUEST_ID 	-2	// no free ID available
#define	NS_NO_FREE_CALLID		-3	// no free callID available

// public function prototypes
extern uint8_t NS_init(uint32_t domain_id, uint32_t node_id);
extern int NS_layer_exit();
extern uint8_t NS_getRemoteEndpoint_request(uint32_t* endpoint, uint32_t domain_id, uint32_t node_num, uint32_t port_num);
extern uint8_t NS_endpointChannelIsopen_request(uint32_t endpoint);
extern uint8_t NS_sendDataToRemote_request(uint32_t send_endpoint,
					uint32_t receive_endpoint,
					const int8_t* buffer,
					uint32_t buffer_size);
extern void NS_layer_receive(PH_pdu *data);

#endif /*NSLAYER_H_*/

