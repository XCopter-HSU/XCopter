/**************************************************************
 * FILE:              PH_layer.c
 *
 * DESCRIPTION:
 * This module or class contains the software necessary
 * to send or receive messages via the given physical
 * infrastructure, which in our case is a set of FIFOs.
 * There is a bidirectional FIFO-channel between any two nodes
 * (NIOSII-based systems).
 * PH_layer software provides the service to send a message
 * to any node or to receive a message from any node. This
 * service is implemented as an unconfirmed service, i.e.
 * we will not get a positive or negative acknowledgement
 * on message receipt. Therefore we have to assume a reliable,
 * error-free physical transmission channel.
 *
 * Service is provided by the following public interface
 * methods:
 * PH_Init():         Initialization of PH_layer software
 *                    and related hardware
 * PH_send_request(): Request to send a message to another
 *                    node
 * If PH_layer software receives a message from another
 * node, this message will be passed to upper layer by
 * calling upper layer's method NS_layer_receive().
 *
 * Message receive operation is implemented with one
 * receive task per FIFO. For example task PH_FIFO1_RcvTask()
 * has to watch FIFO1 and if the entire message has been
 * received, it has to pass the message to the upper layer.
 * In order to do that each PH_FIFOx_RcvTask() is accompanied
 * by an ISR named PH_FIFOx_isr(). Each time the receive FIFO is
 * not empty, it will send an interrupt and CPU will execute
 * the corresponding PH_FIFOx_isr(), which will handle the
 * interrupt request and will inform corresponding
 * PH_FIFOx_RcvTask() to read out and process the received
 * data.
 *
 * PH-layer uses the following very simple packet format:
 *     ---------------------------------------
 *     | length | payload area               |
 *     ---------------------------------------
 *         |--> packet length in bytes
 *
 * HAVE CARE!
 * For each node a node specific receive task will be
 * created. Task priorities 11, 12, 13, ... are assigned to
 * that tasks. Priority value 10 is in use by mutex. Therefore
 * we recommend that user tasks should use priority
 * values 20, 21, 22, ...
 * This should work until we have less than 9 NIOS nodes.
 *
 * EDITION HISTORY:
 * 2012-12-19: Version 0.2 integration of BA Blender - ms
 * 2012-12-31: RISC CPU save add and get macros/functions - ms
 * 2013-02-22: Implementation for MCAPI from Eugen Harass
 * 2013-06-05: PHY layer packet format changed:
 *             - START and STOPP delimiters deleted
 *             - length parameter preceeds packet data - ms
 * 2013-06-28: error detection during OSTaskCreateExt()
 *             and exit() on fatal errors - ms
 * 2014-02-25: debugging - ms
 *************************************************************/

#include <sys/alt_irq.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "includes.h"
#include "../NS_Layer/mapping.h"
#include "../NS_Layer/globals.h"
#include "cpu_fifo_bridge_regs.h" // located in IP core's INC directory
#include "io.h"
#include <stdint.h>	// necessary for intX_t and uintX_t data types

//#define	DEBUG_MODE_VERBOSE	// if defined more debug information
								// will be provided

// PH layer packet format (as passed to NS-layer)
typedef struct
{
	uint32_t bridge_base;
    uint32_t length;
    uint8_t  *data; // Daten ohne Escape-, Start- oder Endsequenz
} PH_pdu;

extern void NS_layer_receive(PH_pdu *data);

// PH layer may return following error/status codes:
#define PH_OK		 0	// no error, everything works fine
#define	PH_ERROR	-1	// an error occured

#define	MUTEX_SEND_PRIO					  10	// mutexSend priority
#define PH_FIFO_RCVTASK_PRIORITY          11
#define TASK_STACKSIZE                    2048	//4096

uint32_t bridge_base_fifo_1; // Adresse der CPU_FIFO_BRIDGE 1
uint32_t bridge_irq_fifo_1;  // IRQ der CPU_FIFO_BRIDGE 1

uint32_t bridge_base_fifo_2; // Adresse der CPU_FIFO_BRIDGE 2
uint32_t bridge_irq_fifo_2;  // IRQ der CPU_FIFO_BRIDGE 2

OS_STK  PH_FIFO1_RcvTask_stk[TASK_STACKSIZE];
OS_STK  PH_FIFO2_RcvTask_stk[TASK_STACKSIZE];

OS_EVENT *semAlmostFull_fifo_1;
OS_EVENT *semAlmostFull_fifo_2;

OS_EVENT *mutexSend;

/*************************************************************
 * ISR FUNCTION: PH_FIFOx_ISR()
 *
 * DESCRIPTION:
 * Waits for an interrupt from fifo_x device and sends
 * signal to waiting receive task PH_FIFOx_RcvTask() via
 * semaphore semAlMostFull_fifo_x.
 *************************************************************/
void PH_FIFO1_ISR(void *context, uint32_t id)
{
    // read FIFOs interrupt register to check for correct interrupt
    if(IORD_CPU_FIFO_BRIDGE_EVENT(bridge_base_fifo_1) & CPU_FIFO_BRIDGE_EVENT_ALMOSTFULL_MSK) {
    	// clear interrupt request
    	// HAVE CARE! It is necessary to re-enable interrupt
    	// before clearing it because of a bug in FIFO bridge
    	IOWR_CPU_FIFO_BRIDGE_IENABLE(bridge_base_fifo_1, CPU_FIFO_BRIDGE_IENABLE_ALMOSTFULL_MSK);
    	IOWR_CPU_FIFO_BRIDGE_EVENT(bridge_base_fifo_1, CPU_FIFO_BRIDGE_EVENT_ALMOSTFULL_MSK);

    	// inform waiting task about interrupt
        if(semAlmostFull_fifo_1->OSEventCnt == 0) {
            uint8_t err = OSSemPost(semAlmostFull_fifo_1);
            if(err != OS_NO_ERR) {
                printf("PH_FIFO1_ISR: error %d during OSSemPost()\n",err);
                exit(0);
            }
        }
    }
    else { // unexpected interrupt has occured
    	printf("PH_FIFO1_ISR: unexpected interrupt with int reg. = 0x%x occured\n", IORD_CPU_FIFO_BRIDGE_EVENT(bridge_base_fifo_1));
    	exit(0);
    }
}

/*************************************************************
 * ISR FUNCTION: PH_FIFOx_ISR()
 *
 * DESCRIPTION:
 * Waits for an interrupt from fifo_x device and sends
 * signal to waiting receive task PH_FIFOx_RcvTask() via
 * semaphore semAlMostFull_fifo_x.
 *************************************************************/
void PH_FIFO2_ISR(void *context, uint32_t id)
{
    // read FIFOs interrupt register to check for correct interrupt
    if(IORD_CPU_FIFO_BRIDGE_EVENT(bridge_base_fifo_2) & CPU_FIFO_BRIDGE_EVENT_ALMOSTFULL_MSK) {
    	// clear interrupt request
    	// HAVE CARE! It is necessary to re-enable interrupt
    	// before clearing it because of a bug in FIFO bridge
    	IOWR_CPU_FIFO_BRIDGE_IENABLE(bridge_base_fifo_2, CPU_FIFO_BRIDGE_IENABLE_ALMOSTFULL_MSK);
    	IOWR_CPU_FIFO_BRIDGE_EVENT(bridge_base_fifo_2, CPU_FIFO_BRIDGE_EVENT_ALMOSTFULL_MSK);

    	// inform waiting task about interrupt
        if(semAlmostFull_fifo_2->OSEventCnt == 0) {
            uint8_t err = OSSemPost(semAlmostFull_fifo_2);
            if(err != OS_NO_ERR) {
                printf("PH_FIFO2_ISR: error %d during OSSemPost()\n",err);
                exit(0);
            }
        }
    }
    else { // unexpected interrupt has occured
    	printf("PH_FIFO2_ISR: unexpected interrupt with int reg. = 0x%x occured\n",
    			IORD_CPU_FIFO_BRIDGE_EVENT(bridge_base_fifo_2));
    	exit(0);
    }
}

/**************************************************************
 * TASK: PH_FIFOx_RcvTask()
 *
 * DESCRIPTION:
 * Receive task responsible for receiving data from a
 * dedicated CPU/FIFO-channel. Task will wait for an
 * interrupt from the corresponding FIFO and will then
 * read-out that FIFO. After receipt of an entire packet
 * this will be passed to the upper layer (NS-layer).
 * Format of packet (type PH_pdu) passed to NS-layer is:
 *
 * --------------------------------------------------------
 * | FIFO_bridge_address | length | payload area          |    |
 * --------------------------------------------------------
 *          |                |--> packet length in bytes
 *          |--> base address of FIFO channel
 *
 * ADAPTATION!!!
 * Parameters 'bridge_base_fifo_x' and 'semAlmostFull_fifo_x'
 * must be adjusted, to receive data from an other CPU.
 * Affected code lines are marked with // !!! adjust ...
 *************************************************************/
void PH_FIFO1_RcvTask(void *pdata)
{
    uint8_t err = OS_NO_ERR;
    int32_t rcvPacketSize = 0;	// receive packet size
    PH_pdu dataP;				// PH packet passed to NS-layer

    // alignment of receivebuffer[] forced because later 4 byte load
    // and store instructions may be used to copy buffer
    uint8_t receivebuffer[MAX_NS_PACK_SIZE + 4] __attribute__ ((aligned (4)));
    uint32_t *receivebuffer32 = (uint32_t *) receivebuffer;;
    dataP.bridge_base = bridge_base_fifo_1;		// !!! adjust bridge_base_fifo_X

    // endless receive loop
    while(1) {
      // wait until at least one word has been received
	  OSSemPend(semAlmostFull_fifo_1,0,&err);	// !!! adjust semAlmostFull_fifo_X
	  if(err != OS_NO_ERR) {
		printf("PH_FIFO1_RcvTask: Error in OSSemPend()\n");	// !!! adjust task name
	  }

	  // read FIFO until empty
	  while((IORD_CPU_FIFO_BRIDGE_STATUS(dataP.bridge_base) & CPU_FIFO_BRIDGE_RECVSTATUS_EMPTY_MSK) == 0) {
		  uint32_t data1 = IORD_CPU_FIFO_BRIDGE_DATA(dataP.bridge_base);

		  if(rcvPacketSize == 0) {				// first data word of packet?
			  rcvPacketSize = data1;			// rcvPacketSize contains packet size in bytes
			  receivebuffer32 = (uint32_t *) receivebuffer;	// set temporary pointer to
			  dataP.length = 0;
#ifdef DEBUG_MODE_VERBOSE
			  printf("PH_FIFO1_RcvTask: Packet with 0x%x bytes should be received\n", rcvPacketSize);	// !!! adjust FIFOToStubTaskX
			  fflush(stdout);
#endif
		  }
		  else {		// data word is part of payload area
			  *receivebuffer32++ = data1;
#ifdef DEBUG_MODE_VERBOSE
			  printf("PH_FIFO1_RcvTask:   - data word 0x%x received\n", data1);	// !!! adjust FIFOToStubTaskX
			  fflush(stdout);
#endif
			  dataP.length = dataP.length + 4;

			  if(dataP.length >= rcvPacketSize) {	// did we receive last data word of packet?
				  dataP.data = receivebuffer;
				  rcvPacketSize = 0;		// mark that current packet in complete
#ifdef DEBUG_MODE_VERBOSE
				  printf("PH_FIFO1_RcvTask: Packet with %d bytes has been received\n", dataP.length);	// !!! adjust FIFOToStubTaskX
				  fflush(stdout);
#endif

				  NS_layer_receive(&dataP);	// pass packet to upper layer
			  }
		  }
	  }

	  // normally IOWR_CPU_FIFO_BRIDGE_IENABLE() shouldn't be necessary
	  // here, must be because of a hardware bug in FIFO bridge module
	  IOWR_CPU_FIFO_BRIDGE_IENABLE(dataP.bridge_base, CPU_FIFO_BRIDGE_IENABLE_ALMOSTFULL_MSK);
    }	// end of endless receive loop
}

/**************************************************************
 * TASK: PH_FIFOx_RcvTask()
 *
 * DESCRIPTION:
 * Receive task responsible for receiving data from a
 * dedicated CPU/FIFO-channel. Task will wait for an
 * interrupt from the corresponding FIFO and will then
 * read-out that FIFO. After receipt of an entire packet
 * this will be passed to the upper layer (NS-layer).
 * Format of packet (type PH_pdu) passed to NS-layer is:
 *
 * --------------------------------------------------------
 * | FIFO_bridge_address | length | payload area          |    |
 * --------------------------------------------------------
 *          |                |--> packet length in bytes
 *          |--> base address of FIFO channel
 *
 * ADAPTATION!!!
 * Parameters 'bridge_base_fifo_x' and 'semAlmostFull_fifo_x'
 * must be adjusted, to receive data from an other CPU.
 * Affected code lines are marked with // !!! adjust ...
 *************************************************************/
void PH_FIFO2_RcvTask(void *pdata)
{
    uint8_t err = OS_NO_ERR;
    int32_t rcvPacketSize = 0;	// receive packet size
    PH_pdu dataP;				// PH packet passed to NS-layer

    // alignment of receivebuffer[] forced because later 4 byte load
    // and store instructions may be used to copy buffer
    uint8_t receivebuffer[MAX_NS_PACK_SIZE + 4] __attribute__ ((aligned (4)));
    uint32_t *receivebuffer32 = (uint32_t *) receivebuffer;;
    dataP.bridge_base = bridge_base_fifo_2;		// !!! adjust bridge_base_fifo_X

    // endless receive loop
    while(1) {
      // wait until at least one word has been received
	  OSSemPend(semAlmostFull_fifo_2,0,&err);	// !!! adjust semAlmostFull_fifo_X
	  if(err != OS_NO_ERR) {
		printf("PH_FIFO2_RcvTask: Error in OSSemPend()\n");	// !!! adjust task name
	  }

	  // read FIFO until empty
	  while((IORD_CPU_FIFO_BRIDGE_STATUS(dataP.bridge_base) & CPU_FIFO_BRIDGE_RECVSTATUS_EMPTY_MSK) == 0) {
		  uint32_t data1 = IORD_CPU_FIFO_BRIDGE_DATA(dataP.bridge_base);

		  if(rcvPacketSize == 0) {				// first data word of packet?
			  rcvPacketSize = data1;			// rcvPacketSize contains packet size in bytes
			  receivebuffer32 = (uint32_t *) receivebuffer;	// set temporary pointer to
			  dataP.length = 0;
#ifdef DEBUG_MODE_VERBOSE
			  printf("PH_FIFO2_RcvTask: Packet with 0x%x bytes should be received\n", rcvPacketSize);	// !!! adjust FIFOToStubTaskX
			  fflush(stdout);
#endif
		  }
		  else {		// data word is part of payload area
			  *receivebuffer32++ = data1;
#ifdef DEBUG_MODE_VERBOSE
			  printf("PH_FIFO2_RcvTask:   - data word 0x%x received\n", data1);	// !!! adjust FIFOToStubTaskX
			  fflush(stdout);
#endif
			  dataP.length = dataP.length + 4;

			  if(dataP.length >= rcvPacketSize) {	// did we receive last data word of packet?
				  dataP.data = receivebuffer;
				  rcvPacketSize = 0;		// mark that current packet in complete
#ifdef DEBUG_MODE_VERBOSE
				  printf("PH_FIFO2_RcvTask: Packet with %d bytes has been received\n", dataP.length);	// !!! adjust FIFOToStubTaskX
				  fflush(stdout);
#endif

				  NS_layer_receive(&dataP);	// pass packet to upper layer
			  }
		  }
	  }

	  // normally IOWR_CPU_FIFO_BRIDGE_IENABLE() shouldn't be necessary
	  // here, must be because of a hardware bug in FIFO bridge module
	  IOWR_CPU_FIFO_BRIDGE_IENABLE(dataP.bridge_base, CPU_FIFO_BRIDGE_IENABLE_ALMOSTFULL_MSK);
    }	// end of endless receive loop
}

/**************************************************************
 * FUNCTION: PH_send_request(...)
 *
 * DESCRIPTION:
 * Function will send a packet consisting of a header and a
 * payload area. length = header_length + payload_length bytes
 * have to be sent over the FIFO channel.
 * Header data specifies the requested NS services whereas
 * payload data is optional.
 * HAVE CARE!!! If header and payload data should be send,
 * header_length has to be a multiple of four!
 *
 * FIFO-channel's base address is 'bridge_base',
 * 'header' points to the location where the header is stored in
 * memory. 'payload' points to the location where payload data
 * is stored in memory.
 * (Passing header and payload areas separately has the great
 * advantage that in the calling function both areas do not need
 * to be merged. Merging both areas means that payload area has
 * to copied in a common packet. Time consuming copying
 * therefore will be avoided.)
 *
 * Send packet format is:
 *     ------------------------------------------------------
 *     | length | NS header data | NS optional payload data |
 *     ------------------------------------------------------
 *       32 bit    length byte
 *
 * INPUT PARAMETERS:
 *   - see description above
 *
 * RETRUN VALUE:
 * - PH_OK in case of success, otherwise PH_ERROR
 *************************************************************/
uint32_t PH_send_request(uint32_t bridge_base, uint32_t *header, uint32_t header_length,
		uint32_t *payload, uint32_t payload_length)
{
	int i;
	int	num32 = 0;
    uint8_t err;

	// Mutex handling necessary because PH_send_request() could be used
	// in different threads
    OSMutexPend(mutexSend, 0, &err);
	if(err != OS_NO_ERR) {
		printf("PH_send_request: OSMutexPend() failed, error code = %d\n", err);
		return(PH_ERROR);
	}

#ifdef DEBUG_MODE_VERBOSE
	printf("PH_send_request(): packet with %d bytes should be send\n", header_length + payload_length);
	fflush(stdout);
#endif

	/* 1. Send packet length parameter ***************************/
	// wait until there in enough free space in FIFO
	while(IORD_CPU_FIFO_BRIDGE_SENDLEVEL(bridge_base) > 31) {
//		OSTimeDly(0);	// even OSTimDly(0) will
						// reduce performance significantly
		;
	}
	IOWR_CPU_FIFO_BRIDGE_DATA(bridge_base, header_length + payload_length);	// send data word to FIFO

	/* 2. Send header area ***************************************/
	if((header_length % 4) == 0)
		num32 = header_length/4;
	else {
		if(payload_length == 0)
			num32 = header_length/4 + 1;
		else {
			printf("PH_send_request(): header_length has to be a multiple of 4!\n");
			fflush(stdout);
			err = OSMutexPost(mutexSend);
			if(err != OS_NO_ERR)
			  printf("PH_send_request: OSMutexPost() failed, error code = %d\n", err);
			fflush(stdout);
			return(PH_ERROR);
		}
	}

	for(i = 0; i < num32; i++) {
#ifdef DEBUG_MODE_VERBOSE
		printf("PH_send_request(): header word %d: 0x%x will be sent\n", i, *header);
		fflush(stdout);
#endif
		// wait until there in enough free space in FIFO
		while(IORD_CPU_FIFO_BRIDGE_SENDLEVEL(bridge_base) > 31) {
//			OSTimeDly(0);
			;
		}
		IOWR_CPU_FIFO_BRIDGE_DATA(bridge_base, *header++);	// send data word to FIFO
	}

	/* 3. Send payload area **************************************/
	if((payload_length % 4) == 0)
		num32 = payload_length/4;
	else {
		num32 = payload_length/4 + 1;
	}

	for(i = 0; i < num32; i++) {
#ifdef DEBUG_MODE_VERBOSE
		printf("PH_send_request(): payload word %d: 0x%x will be sent\n", i, *payload);
		fflush(stdout);
#endif
		// wait until there in enough free space in FIFO
		while(IORD_CPU_FIFO_BRIDGE_SENDLEVEL(bridge_base) > 31) {
//			OSTimeDly(0);
			;
		}
		IOWR_CPU_FIFO_BRIDGE_DATA(bridge_base, *payload++);	// send data word to FIFO
	}

	// release mutex
	err = OSMutexPost(mutexSend);
	if(err != OS_NO_ERR) {
	  printf("PH_send_request: OSMutexPost() failed, error code = %d\n", err);
	  fflush(stdout);
	  return(PH_ERROR);
	}

	return(PH_OK);
}

/*************************************************************
 * FUNCTION: cpu_fifo_bridge_init()
 *
 * DESCRIOTION:
 * Initialize FIFO device.
 *
 * INPUT PARAMETERS:
 * - address: FIFO's base address
 * - ienable: interrupts to enable
 *
 * RETURN VALUE:
 * - none
 *************************************************************/
void cpu_fifo_bridge_init(alt_u32 address, alt_u32 ienable)
{
	IOWR_CPU_FIFO_BRIDGE_CLEAR(address);	// clear send FIFO

	// read out receive FIFO until empty
	// Not a good idea because if one CPU has started before
	// another CPU and has send a request to that CPU, the other
	// CPU will delete this request during it's initialization
	// phase.
//	while(!(IORD_CPU_FIFO_BRIDGE_STATUS(address) & CPU_FIFO_BRIDGE_RECVSTATUS_EMPTY_MSK)) {
//		IORD_CPU_FIFO_BRIDGE_DATA(address);
//	}

	// enable interrupts specified by parameter ienable
    alt_u32 ret = (IORD_CPU_FIFO_BRIDGE_IENABLE(address) & CPU_FIFO_BRIDGE_IENABLE_ALL_MSK);
    IOWR_CPU_FIFO_BRIDGE_IENABLE(address, (ret ^ ienable));

    // clear any pending interrupt requests
    IOWR_CPU_FIFO_BRIDGE_EVENT(address, CPU_FIFO_BRIDGE_EVENT_ALL_MSK);
}

/*************************************************************
 * FUNCTION: PH_init()
 *
 * DESCRIPTION:
 * Initialization of PH layer components.
 *
 * INPUT PARAMETERS:
 *  - local node ID
 *
 * RETURN VALUE:
 * Returns error/status code. Following codes might be returned:
 * - PH_OK      everything works fine
 * - PH_ERROR   error occured
 *************************************************************/
uint8_t	PH_init(uint32_t nios_node_id)
{
	uint8_t err;

	// create semphores used to synchronize PH_FIFOx_ISR() and
	// corresponding PH_FIFOx_RcvTask()
    semAlmostFull_fifo_1 = OSSemCreate(0);
    semAlmostFull_fifo_2 = OSSemCreate(0);
    if((semAlmostFull_fifo_1 == NULL) || (semAlmostFull_fifo_2 == NULL)) {
    	printf("PH_init: error on creating semAlmostFull_fifoX semaphores\n");
    	return(PH_ERROR);
    }

    // mutexSend is used for exclusive access to PH_send_request() method
    mutexSend = OSMutexCreate(MUTEX_SEND_PRIO, &err);
    if(err != OS_NO_ERR) {
		printf("PH_init: error %d during OSMutexCreate()\n", err);
		return(PH_ERROR);
	}

    int n, i;
	for(n = 0, i = 0; n < NUM_OF_NODES; n++) {
		// check if node is valid
		if(nios_node_mapping_db[nios_node_id].destination_nodes[n].valid == true) {
			// Index i is necessary because we use absolute names for rcvTask, isr,
			// base address and irq level. First legal destination node will be
			// handled by PH_FIFO1_RcvTask and PH_FIFO1_ISR,
			// second by PH_FIFO2_RcvTask and PH_FIFO2_ISR and so on.

			/**********************************************************/
			if(i == 0) { // initialize first valid destination node
						 // found in nios_node_mapping_db
				bridge_base_fifo_1 = nios_node_mapping_db[nios_node_id].destination_nodes[n].base;
				bridge_irq_fifo_1  = nios_node_mapping_db[nios_node_id].destination_nodes[n].irq;

				// Initialize FIFO bridge
				cpu_fifo_bridge_init(bridge_base_fifo_1,0);

				// enable FIFO receive interrupt and register corresponding ISR
				alt_irq_register(bridge_irq_fifo_1, 0, PH_FIFO1_ISR);
				alt_irq_enable(bridge_irq_fifo_1);
				IOWR_CPU_FIFO_BRIDGE_IENABLE(bridge_base_fifo_1, CPU_FIFO_BRIDGE_IENABLE_ALMOSTFULL_MSK);

				// create corresponding receive task
				if((err = OSTaskCreateExt(PH_FIFO1_RcvTask,
								  NULL,
								  (void *)&PH_FIFO1_RcvTask_stk[TASK_STACKSIZE-1],
								  PH_FIFO_RCVTASK_PRIORITY + i,
								  PH_FIFO_RCVTASK_PRIORITY + i,
								  PH_FIFO1_RcvTask_stk,
								  TASK_STACKSIZE,
								  NULL,
								  0)) != OS_NO_ERR) {
					printf("PH_init: error %d during OSTaskCreateExt() execution\n", err);
					return(PH_ERROR);
				}
			}

			/**********************************************************/
			if(i == 1) { // initialize second valid destination node
						 // found in nios_node_mapping_db
				bridge_base_fifo_2 = nios_node_mapping_db[nios_node_id].destination_nodes[n].base;
				bridge_irq_fifo_2 = nios_node_mapping_db[nios_node_id].destination_nodes[n].irq;

				// Initialize FIFO bridge
				cpu_fifo_bridge_init(bridge_base_fifo_2,0);

				// enable FIFO receive interrupt and register corresponding ISR
				alt_irq_register(bridge_irq_fifo_2, 0, PH_FIFO2_ISR);
				alt_irq_enable(bridge_irq_fifo_2);
				IOWR_CPU_FIFO_BRIDGE_IENABLE(bridge_base_fifo_2, CPU_FIFO_BRIDGE_IENABLE_ALMOSTFULL_MSK);

				if((err = OSTaskCreateExt(PH_FIFO2_RcvTask,
								  NULL,
								  (void *)&PH_FIFO2_RcvTask_stk[TASK_STACKSIZE-1],
								  PH_FIFO_RCVTASK_PRIORITY + i,
								  PH_FIFO_RCVTASK_PRIORITY + i,
								  PH_FIFO2_RcvTask_stk,
								  TASK_STACKSIZE,
								  NULL,
								  0)) != OS_NO_ERR) {
					printf("PH_init: error %d during OSTaskCreateExt() execution\n", err);
					return(PH_ERROR);
				}
			}

			i++;
		}
    }
	return(PH_OK);
}

