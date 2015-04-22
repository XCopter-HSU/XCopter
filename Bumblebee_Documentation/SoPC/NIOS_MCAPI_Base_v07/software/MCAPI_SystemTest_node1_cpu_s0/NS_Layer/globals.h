/**************************************************************
 * FILE:              globals.h
 *
 * DESCRIPTION:
 * Global definitions which are required in several files.
 *
 * Edition History
 * 2012-12-17: changes for CPC_Tool v0.2 (BA Blender) - ms
 * 2012-12-31: RISC CPU save add and get macros/functions - ms
 * 2013-02-22: Implementation for MCAPI from Eugen Harass
 * 2013-12-17: debugging - ms
 *************************************************************/

#ifndef GLOBALS_H
#define GLOBALS_H

#include "../MCAPI_Top/mca_config.h"

// max. size of a MCAPI message or packet
#define MAX_MCAPI_MSGPKT_SIZE (MCAPI_MAX_MSG_SIZE > MCAPI_MAX_PKT_SIZE ? MCAPI_MAX_MSG_SIZE : MCAPI_MAX_PKT_SIZE)

// max. header size of an NS packet
#define MAX_NS_HEADER_SIZE	32

// max. total size of a NS packet
#define MAX_NS_PACK_SIZE	(MAX_NS_HEADER_SIZE + MAX_MCAPI_MSGPKT_SIZE)

// max. size of the payload area of a NS response packet
#define MAX_NS_RESP_PAYLOAD_SIZE	32

#endif /*GLOBALS_H*/
