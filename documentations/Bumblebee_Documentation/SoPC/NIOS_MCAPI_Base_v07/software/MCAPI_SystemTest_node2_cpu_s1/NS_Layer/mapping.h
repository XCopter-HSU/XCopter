/****************************************************************************
 File             : mapping.h
 Author           : Eugen Harass
 Date             : 22.02.2013
 Decription       : Mapping between MCAPI-nodes and CPU_FIFO_BRIDGE-modules

 * EDITION HISTORY:
 * 2013-12-17: debugging - ms (M. Strahnen)
****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef MAPPING_H
#define MAPPING_H

//#include "system.h"
#include <stdbool.h>

// Nios 0 Adressen
#define FIFO_BRIDGE_NIOS_0_NIOS_1_BASE 0x04000000
#define FIFO_BRIDGE_NIOS_0_NIOS_2_BASE 0x04000400
#define FIFO_BRIDGE_NIOS_0_NIOS_1_IRQ	10
#define FIFO_BRIDGE_NIOS_0_NIOS_2_IRQ	11

// Nios 1 Adressen
#define FIFO_BRIDGE_NIOS_1_NIOS_0_BASE 0x9004000
#define FIFO_BRIDGE_NIOS_1_NIOS_0_IRQ	1
#define FIFO_BRIDGE_NIOS_1_NIOS_2_BASE 0x9004400
#define FIFO_BRIDGE_NIOS_1_NIOS_2_IRQ	3

// Nios 2 Adressen
#define FIFO_BRIDGE_NIOS_2_NIOS_0_BASE 0x9004000
#define FIFO_BRIDGE_NIOS_2_NIOS_0_IRQ	1
#define FIFO_BRIDGE_NIOS_2_NIOS_1_BASE 0x9004400
#define FIFO_BRIDGE_NIOS_2_NIOS_1_IRQ	3

#define DOMAIN_ID 0
#define NIOS_0_NODE_ID 0
#define NIOS_1_NODE_ID 1
#define NIOS_2_NODE_ID 2

#define NUM_OF_NODES 3

// original nios_node_mapping_db
typedef struct {
	bool valid;
	unsigned int base;
	unsigned int irq;
} destination_node_type;

typedef struct {
	destination_node_type destination_nodes[NUM_OF_NODES];
} nios_node_mapping_db_type;

static const nios_node_mapping_db_type nios_node_mapping_db[NUM_OF_NODES] =
{
	//NIOS 0:
	{
		.destination_nodes[0] = { .valid = false, .base = 0, .irq = 0 },
		.destination_nodes[1] = { .valid = true, .base = FIFO_BRIDGE_NIOS_0_NIOS_1_BASE, .irq = FIFO_BRIDGE_NIOS_0_NIOS_1_IRQ },
		.destination_nodes[2] = { .valid = true, .base = FIFO_BRIDGE_NIOS_0_NIOS_2_BASE, .irq = FIFO_BRIDGE_NIOS_0_NIOS_2_IRQ }
	},
	//NIOS 1:
	{
		.destination_nodes[0] = { .valid = true, .base = FIFO_BRIDGE_NIOS_1_NIOS_0_BASE, .irq = FIFO_BRIDGE_NIOS_1_NIOS_0_IRQ },
		.destination_nodes[1] = { .valid = false, .base = 0, .irq = 0 },
		.destination_nodes[2] = { .valid = true, .base = FIFO_BRIDGE_NIOS_1_NIOS_2_BASE, .irq = FIFO_BRIDGE_NIOS_1_NIOS_2_IRQ }
	},
	//NIOS 2:
	{
		.destination_nodes[0] = { .valid = true, .base = FIFO_BRIDGE_NIOS_2_NIOS_0_BASE, .irq = FIFO_BRIDGE_NIOS_2_NIOS_0_IRQ },
		.destination_nodes[1] = { .valid = true, .base = FIFO_BRIDGE_NIOS_2_NIOS_1_BASE, .irq = FIFO_BRIDGE_NIOS_2_NIOS_1_IRQ },
		.destination_nodes[2] = { .valid = false, .base = 0, .irq = 0 }
	}
};

#endif // MAPPING_H

#ifdef __cplusplus
}
#endif /* __cplusplus */

