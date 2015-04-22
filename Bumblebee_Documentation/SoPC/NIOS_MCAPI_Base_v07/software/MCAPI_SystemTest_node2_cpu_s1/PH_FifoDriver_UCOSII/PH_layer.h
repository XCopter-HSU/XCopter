/**************************************************************
 * FILE:              PH_layer.h
 *
 * DESCRIPTION:
 * Everybody who wants to use the services of PH_layer.c
 * services has to include this file.
 *
 * Edition History:
 * 2012-12-19: Version 0.2 integration of BA Blender - ms
 * 2013-02-22: Implementation for MCAPI from Eugen Harass
 * 2013-12-17: totally new structure - ms
 *************************************************************/

#ifndef PH_LAYER_H_
#define PH_LAYER_H_

#include <stdint.h>	// necessary for intX_t and uintX_t data types

// PH layer may return following error/status codes:
#define PH_OK		 0	// no error, everything works fine
#define	PH_ERROR	-1	// an error occured

// PH layer packet format (as passed to NS-layer)
typedef struct
{
	uint32_t bridge_base;
    uint32_t length;
    uint8_t  *data; // Daten ohne Escape-, Start- oder Endsequenz
} PH_pdu;

// public function prototypes
extern uint8_t	PH_init(uint32_t nios_node_id);
extern uint32_t PH_send_request(uint32_t bridge_base, uint32_t *header, uint32_t header_length, uint32_t *payload, uint32_t payload_length);

#endif /*PH_LAYER_H_*/
