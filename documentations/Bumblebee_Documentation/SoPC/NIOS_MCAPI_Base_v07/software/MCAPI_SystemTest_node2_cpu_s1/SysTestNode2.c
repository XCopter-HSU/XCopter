/**************************************************************
 *                    file: SysTestNode2.c
 *
 * Program to test MCAPI messages with 3 NIOS CPUs.
 * Following 'messages transfer paths' will be installed and
 * used to transfer a well defined number of messages with
 * varying message size.
 *
 * Path1: node0 (cpu_m):  node0_sendMSG_to_node1_task   (node0_sendEP_to_node1)   --|
 *        node1 (cpu_s0): node1_recvMSG_from_node0_task (node1_recvEP_from_node0) <-|
 * Path2: node1 (cpu_s0): node1_sendMSG_to_node0_task   (node1_sendEP_to_node0)   --|
 *        node0 (cpu_m):  node0_recvMSG_from_node1_task (node0_recvEP_from_node1) <-|
 *
 * Path3: node0 (cpu_m):  node0_sendMSG_to_node2_task   (node0_sendEP_to_node2)   --|
 *        node2 (cpu_s1): node2_recvMSG_from_node0_task (node2_recvEP_from_node0) <-|
 * Path4: node2 (cpu_s1): node2_sendMSG_to_node0_task   (node2_sendEP_to_node0)   --|
 *        node0 (cpu_m):  node0_recvMSG_from_node2_task (node0_recvEP_from_node2) <-|
 *
 * Path5: node1 (cpu_s0): node1_sendMSG_to_node2_task   (node1_sendEP_to_node2)   --|
 *        node2 (cpu_s1): node2_recvMSG_from_node1_task (node2_recvEP_from_node1) <-|
 * Path6: node2 (cpu_s1): node2_sendMSG_to_node1_task   (node2_sendEP_to_node1)   --|
 *        node1 (cpu_s0): node1_recvMSG_from_node2_task (node1_recvEP_from_node2) <-|
 *
 * Any of the 6 paths must be enabled by setting the corresponding
 * 'C define'. E. g. when #define ENABLE_PATH1 is set, Path1 and the
 * corresponding send and receive tasks will be installed.
 *
 * Parameter MSG_SIZE_MAX specifies maximum message size.
 * Messages with message sizes of 1, 2, 3, ... MSG_SIZE_MAX -1
 * will be send. Sending this bunch of messages is called
 * a super-cycle. Parameter NUM_OF_CYCLES specifies how many
 * super-cycles are executed in order to test the system.
 *
 * Main function will initialize the system and
 * start the multitasking operating system.
 * Program is used to test MCAPI operation when messages
 * are bidirectionally exchanged with another node.
 * The receiving task has to check data correctness.
 *
 * This software should run on node2.
 *
 * According to file 'mapping.h':
 *    node0 has node id NIOS_0_NODE_ID and is given by cpu_m,
 *    node1 has node id NIOS_1_NODE_ID and is given by cpu_s0.
 *    node2 has node id NIOS_2_NODE_ID and is given by cpu_s1.
 *
 * History:
 * 2013-06-07: getting started - ms
 * 2013-06-19: extended to bidirectional data transfer - ms
 * 2013-07-01: debugging - ms
 * 2014-06-05: adaptation to Linux - ms
 * 2014-08-05: Additional support of uC/OS-II - ms
 * 2014-08-11: mcapi_env.h included via mcapi.h - ms
 * 2014-08-11: mcapi_env.h included via mcapi.h - ms
 *************************************************************/

// Depending on the used operating system and development
// environment, file mcapi_env.h must be included from
// different locations
//#include "../../MCAPI_Sys/MCAPI_Top/mcapi_env.h"	// in case of uCLinux/makefile
#include "MCAPI_Top/mcapi_env.h"				// in case of uC-OS-II/NIOS-IDE

#ifdef UCOSII
	// uC/OS-II specific includes
	#include "includes.h"
	#include "MCAPI_Top/mcapi.h"
	#include "NS_Layer/mapping.h"
#endif

#ifdef LINUX
	// uClinux specific includes
	#include <pthread.h>
	#include <sys/time.h>
	#include "../../MCAPI_Sys/MCAPI_Top/mcapi.h"
	#include "../../MCAPI_Sys/NS_Layer/mapping.h"
# endif

// other includes
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

// Enable/disable message transfer paths
//#define	ENABLE_PATH3	// node0 to node2
//#define	ENABLE_PATH4	// node2 to node0
#define	ENABLE_PATH5	// node1 to node2
#define	ENABLE_PATH6	// node2 to node1

#ifdef UCOSII
	/* uC/OS-II specific task definitions */
	/* Definition of Task Stacks */
	#define   INIT_TASK_STACKSIZE  2048
	#define   TASK_STACKSIZE       2048
	OS_STK    init_task_stk[INIT_TASK_STACKSIZE];
	OS_STK    node2_sendMSG_to_node0_task_stk[TASK_STACKSIZE];
	OS_STK	  node2_recvMSG_from_node0_task_stk[TASK_STACKSIZE];
	OS_STK    node2_sendMSG_to_node1_task_stk[TASK_STACKSIZE];
	OS_STK	  node2_recvMSG_from_node1_task_stk[TASK_STACKSIZE];

	/* HAVE CARE!
	 * In the communication layer for each node a node specific
	 * receive task will be created. Task priorities 11, 12, 13, ...
	 * are assigned to that tasks. Priority value 10 is also
	 * used by the communication layer as mutex priority. Therefore
	 * we recommend that user tasks should use priority
	 * values 20, 21, 22, ...
	 * By default MikroC/OS-II supports lowest priority 20.
	 * Therefore you have to adjust the following two
	 * operating system specific parameters with the help
	 * of the BSP editor:
	 *   os_lowest_prio = 40
	 *   os_max_tasks   = 30
	 * This should work until we have less than 9 NIOS nodes.
	 */
	#define INIT_TASK_PRIORITY  				20
	#define	NODE2_RECVMSG_FROM_NODE0_TASK_PRIO	21
	#define	NODE2_RECVMSG_FROM_NODE1_TASK_PRIO	22
	#define NODE2_SENDMSG_TO_NODE0_TASK_PRIO	23
	#define NODE2_SENDMSG_TO_NODE1_TASK_PRIO	24
#endif

#ifdef LINUX
/* uClinux specific task definitions */
	pthread_t node2_sendMSG_to_node0_thread;
	pthread_t node2_sendMSG_to_node1_thread;
	pthread_t node2_recvMSG_from_node0_thread;
	pthread_t node2_recvMSG_from_node1_thread;
	pthread_t init_thread;
#endif

// task-specific flags which indicate if thread is still
// alive. If a flag > 0 the corresponding task/thread is still
// alive.
	int node2_sendMSG_to_node0_thread_flag = 0;
	int node2_sendMSG_to_node1_thread_flag = 0;
	int node2_recvMSG_from_node0_thread_flag = 0;
	int node2_recvMSG_from_node1_thread_flag = 0;
	int init_thread_flag = 0;

/* Predefined Port ID numbers */
#define	NODE0_SENDPORT_TO_NODE1		1
#define	NODE0_SENDPORT_TO_NODE2		2
#define NODE0_RECVPORT_FROM_NODE1	3
#define	NODE0_RECVPORT_FROM_NODE2	4

#define	NODE1_SENDPORT_TO_NODE0		5
#define	NODE1_SENDPORT_TO_NODE2		6
#define NODE1_RECVPORT_FROM_NODE0	7
#define	NODE1_RECVPORT_FROM_NODE2	8

#define	NODE2_SENDPORT_TO_NODE0		9
#define	NODE2_SENDPORT_TO_NODE1		10
#define NODE2_RECVPORT_FROM_NODE0	11
#define	NODE2_RECVPORT_FROM_NODE1	12

// MCAPI DOMAINS
#define MY_DOMAIN 0

// Others
#define	NUM_OF_CYCLES	8	// number of message receive super-cycles
#define MSG_SIZE_MAX	64	// maximum message size
#define ISNONBLOCKING 	0	// if '1' the non-blocking version of MCAPI
						    // functions will be used

// endpoint declarations
mcapi_endpoint_t node0_sendEP_to_node1;
mcapi_endpoint_t node0_sendEP_to_node2;
mcapi_endpoint_t node1_sendEP_to_node0;
mcapi_endpoint_t node1_sendEP_to_node2;
mcapi_endpoint_t node2_sendEP_to_node0;
mcapi_endpoint_t node2_sendEP_to_node1;

mcapi_endpoint_t node0_recvEP_from_node1;
mcapi_endpoint_t node0_recvEP_from_node2;
mcapi_endpoint_t node1_recvEP_from_node0;
mcapi_endpoint_t node1_recvEP_from_node2;
mcapi_endpoint_t node2_recvEP_from_node0;
mcapi_endpoint_t node2_recvEP_from_node1;

mcapi_priority_t prio;

/**************************************************************
 * function: sys_stop()
 * Simple stop the system and just wait().
 *************************************************************/
void sys_stop() {
	while(1) ;
}

/**************************************************************
 * function: check_status()
 * Prints MCAPI status.
 *************************************************************/
void check_status(mcapi_status_t status)
{
	char status_message[MCAPI_MAX_STATUS_MSG_LEN];
	if(status != MCAPI_SUCCESS) {
		printf("STATUS:%s\n", mcapi_display_status(status, status_message, sizeof(status_message)));
		fflush(stdout);
	}
}

/**************************************************************
 * task: node2_recvMSG_from_node0_task()
 * Task will receive messages from node0 and will check
 * message contents for correctness.
 *************************************************************/
void node2_recvMSG_from_node0_task(void* pdata)
{
	mcapi_timeout_t timeout = MCA_INFINITE;
	mcapi_status_t status = MCAPI_TRUE;
	unsigned char msg[MSG_SIZE_MAX];
	size_t tSize;
	mcapi_request_t r1;
	int	msg_size;
	int	cycles;			// number of receive super-cycles
	int i;
	int	msg_cnt = 0;	// number of messages received
	int	byte_cnt = 0;	// overall number of bytes received
	int	msg_err = 0;	// number of error during receipt

	// Execute NUM_OF_CYCLES message receive super-cycles.
	// In each super-cycles a defined number of messages with
	// changing message size will be received.
	for(cycles = 0; cycles < NUM_OF_CYCLES; cycles++) {
		// Message receive super-cycle: messages with ever increasing
		// sizes from 1 to MSG_SIZE_MAX will be received
		for(msg_size = 1; msg_size < MSG_SIZE_MAX; msg_size++) {
			if(ISNONBLOCKING) {
				mcapi_msg_recv_i(node2_recvEP_from_node0, msg, msg_size, &r1, &status);
				mcapi_wait(&r1, &tSize, timeout, &status);
				if(status != MCAPI_SUCCESS) {
					check_status(status);
					if(status == MCAPI_TIMEOUT)	break;
				}
			}
			else {
				mcapi_msg_recv(node2_recvEP_from_node0, msg, msg_size, &tSize, &status);
				if(status != MCAPI_SUCCESS) {
					check_status(status);
				}
			}
			msg_cnt++;
			byte_cnt = byte_cnt + tSize;

			// check message content
			for(i = 0; i < msg_size; i++) {
				if(msg[i] != (i % 256)) {  // 0 <= msg[i] <= 255
					msg_err++;
					printf("node2_recvMSG_from_node0_task: receive error on receiving message with size %d from node0\n", msg_size);
					printf("node2_recvMSG_from_node0_task:    - expected: 0x%x, received: 0x%x\n", i, msg[i]);
				}
			}

//			printf("node2_recvMSG_from_node0_task: message with %d bytes received from node0\n", msg_size);
		}
		printf("node2_recvMSG_from_node0_task: %d super-cycles executed\n", cycles);
	}
	printf("node2_recvMSG_from_node0_task: %d messages (%d bytes) received from node0 with %d errors\n", msg_cnt, byte_cnt, msg_err);

	// delete local endpoints
	mcapi_endpoint_delete(node2_recvEP_from_node0, &status);
	check_status(status);
//	printf("node2_recvMSG_from_node0_task: Receive endpoint node2_recvEP_from_node0 deleted\n");

	node2_recvMSG_from_node0_thread_flag = 0;
	printf("node2_recvMSG_from_node0_task will finish in 1 second\n"); fflush(stdout);
	usleep(1000000);

#ifdef UCOSII
	/* uC/OS-II specific code to stop this task*/
	while(1) OSTaskSuspend(OS_PRIO_SELF);
#endif

#ifdef LINUX
	/* uClinux specific code to stop this task */
	pthread_exit(NULL);
#endif
}

/**************************************************************
 * task: node2_recvMSG_from_node1_task()
 * Task will receive messages from node1 and will check
 * message contents for correctness.
 *************************************************************/
void node2_recvMSG_from_node1_task(void* pdata)
{
	mcapi_timeout_t timeout = MCA_INFINITE;
	mcapi_status_t status = MCAPI_TRUE;
	unsigned char msg[MSG_SIZE_MAX];
	size_t tSize;
	mcapi_request_t r1;
	int	msg_size;
	int	cycles;			// number of receive super-cycles
	int i;
	int	msg_cnt = 0;	// number of messages received
	int	byte_cnt = 0;	// overall number of bytes received
	int	msg_err = 0;	// number of error during receipt

	// Execute NUM_OF_CYCLES message receive super-cycles.
	// In each super-cycles a defined number of messages with
	// changing message size will be received.
	for(cycles = 0; cycles < NUM_OF_CYCLES; cycles++) {
		// Message receive super-cycle: messages with ever increasing
		// sizes from 1 to MSG_SIZE_MAX will be received
		for(msg_size = 1; msg_size < MSG_SIZE_MAX; msg_size++) {
			if(ISNONBLOCKING) {
				mcapi_msg_recv_i(node2_recvEP_from_node1, msg, msg_size, &r1, &status);
				mcapi_wait(&r1, &tSize, timeout, &status);
				if(status != MCAPI_SUCCESS) {
					check_status(status);
					if(status == MCAPI_TIMEOUT)	break;
				}
			}
			else {
				mcapi_msg_recv(node2_recvEP_from_node1, msg, msg_size, &tSize, &status);
				if(status != MCAPI_SUCCESS) {
					check_status(status);
				}
			}
			msg_cnt++;
			byte_cnt = byte_cnt + tSize;

			// check message content
			for(i = 0; i < msg_size; i++) {
				if(msg[i] != (i % 256)) {  // 0 <= msg[i] <= 255
					msg_err++;
					printf("node2_recvMSG_from_node1_task: receive error on receiving message with size %d from node1\n", msg_size);
					printf("node2_recvMSG_from_node1_task:    - expected: 0x%x, received: 0x%x\n", i, msg[i]);
				}
			}

//			printf("node2_recvMSG_from_node1_task: message with %d bytes received from node1\n", msg_size);
		}
		printf("node2_recvMSG_from_node1_task: %d super-cycles executed\n", cycles);
	}
	printf("node2_recvMSG_from_node1_task: %d messages (%d bytes) received from node1 with %d errors\n", msg_cnt, byte_cnt, msg_err);

	// delete local endpoints
	mcapi_endpoint_delete(node2_recvEP_from_node1, &status);
	check_status(status);
//	printf("node2_recvMSG_from_node1_task: Receive endpoint node2_recvEP_from_node1 deleted\n");

	node2_recvMSG_from_node1_thread_flag = 0;
	printf("node2_recvMSG_from_node1_task will finish in 1 second\n"); fflush(stdout);
	usleep(1000000);

#ifdef UCOSII
	/* uC/OS-II specific code to stop this task */
	while(1) OSTaskSuspend(OS_PRIO_SELF);
#endif

#ifdef LINUX
	/* uClinux specific code to stop this task*/
	pthread_exit(NULL);
#endif
}

/**************************************************************
 * task: node2_sendMSG_to_node0_task()
 * Sends messages with different message sizes to node0.
 **************************************************************/
void node2_sendMSG_to_node0_task(void* pdata)
{
	mcapi_status_t status = MCAPI_TRUE;
	char msg[MSG_SIZE_MAX];	// message buffer
	int	msg_size;			// current message size
	mcapi_request_t r1;
	size_t tSize1;
	int	cycles;			// number of receive super-cycles
	int	msg_cnt = 0;	// number of messages received
	int	byte_cnt = 0;	// overall number of bytes received
	int i;
	uint16_t del = 0;			// delay parameter

	// Execute NUM_OF_CYCLES message send super-cycles.
	// In each super-cycles a defined number of messages with
	// changing message size will be send.
	for(cycles = 0; cycles < NUM_OF_CYCLES; cycles++) {
		// Message send super-cycle: messages with ever increasing
		// sizes from 1 to MSG_SIZE_MAX will be send

		// introduce a random delay to make things really asynchronous
		del = rand() % 20 + 1;	// generates a random between 0 and 20
								// i. e. an random delay between
//		OSTimeDly(del);		// 0*10ms and 20*10ms
		usleep(10000 * del);	// wait del * 10 ms

		for(msg_size = 1; msg_size < MSG_SIZE_MAX; msg_size++) {
			// prepare message content
			for(i = 0; i < msg_size; i++) {
				msg[i] = i % 256;	// 0 <= msg[i] <= 255
//				if(msg_size == 27) msg[13] = 0xfe;	// insert an error
													// for testing purposes
			}

			// send message to node1
			if(ISNONBLOCKING) {
				// non-blocking send method will be used
				mcapi_msg_send_i(node2_sendEP_to_node0, node0_recvEP_from_node2, msg, msg_size, prio, &r1, &status);
				while(!(mcapi_test(&r1, &tSize1, &status))) {
					if(status != MCAPI_SUCCESS)
						check_status(status);
					OSTimeDly(1);
				}
			}
			else {
				// blocking send method will be used
				mcapi_msg_send(node2_sendEP_to_node0, node0_recvEP_from_node2, msg, msg_size, prio, &status);
				if(status != MCAPI_SUCCESS)
					printf("node2_sendMSG_to_node0_task: Error in mcapi_msg_send() with msg_size = %d\n", msg_size); fflush(stdout);
					check_status(status);
			}

//			OSTimeDly(1);	// 10 ms delay only for debugging
			msg_cnt++;
			byte_cnt = byte_cnt + msg_size;

//			printf("node2_sendMSG_to_node0_task: Message with %d bytes sent to node0\n", msg_size);
		}
		printf("node2_sendMSG_to_node0_task: %d super-cycles executed\n", cycles);
	}
	printf("node2_sendMSG_to_node0_task: %d messages (%d bytes) sent to node0\n", msg_cnt, byte_cnt);

	// delete local endpoints
	mcapi_endpoint_delete(node2_sendEP_to_node0, &status);
	check_status(status);
//	printf("node2_sendMSG_to_node0_task: Send endpoint node2_sendEP_to_node0 deleted\n");

	node2_sendMSG_to_node0_thread_flag = 0;
	printf("node2_sendMSG_to_node0_task will finish in 1 second\n"); fflush(stdout);
	usleep(1000000);

#ifdef UCOSII
	/* uC/OS-II specific code to stop this task */
	while(1) OSTaskSuspend(OS_PRIO_SELF);
#endif

#ifdef LINUX
	/* uClinux specific code to stop this task */
	pthread_exit(NULL);
#endif
}

/**************************************************************
 * task: node2_sendMSG_to_node1_task()
 * Sends messages with different message sizes to node1.
 **************************************************************/
void node2_sendMSG_to_node1_task(void* pdata)
{
	mcapi_status_t status = MCAPI_TRUE;
	char msg[MSG_SIZE_MAX];	// message buffer
	int	msg_size;			// current message size
	mcapi_request_t r1;
	size_t tSize1;
	int	cycles;			// number of receive super-cycles
	int	msg_cnt = 0;	// number of messages received
	int	byte_cnt = 0;	// overall number of bytes received
	int i;
	uint16_t del = 0;			// delay parameter

	// Execute NUM_OF_CYCLES message send super-cycles.
	// In each super-cycles a defined number of messages with
	// changing message size will be send.
	for(cycles = 0; cycles < NUM_OF_CYCLES; cycles++) {
		// Message send super-cycle: messages with ever increasing
		// sizes from 1 to MSG_SIZE_MAX will be send

		// introduce a random delay to make things really asynchronous
		del = rand() % 20 + 1;	// generates a random between 0 and 20
								// i. e. an random delay between
//		OSTimeDly(del);			// 0*10ms and 20*10ms
		usleep(10000 * del);	// wait del * 10 ms

		for(msg_size = 1; msg_size < MSG_SIZE_MAX; msg_size++) {
			// prepare message content
			for(i = 0; i < msg_size; i++) {
				msg[i] = i % 256;	// 0 <= msg[i] <= 255
//				if(msg_size == 27) msg[13] = 0xfe;	// insert an error
													// for testing purposes
			}

			// send message to node1
			if(ISNONBLOCKING) {
				// non-blocking send method will be used
				mcapi_msg_send_i(node2_sendEP_to_node1, node1_recvEP_from_node2, msg, msg_size, prio, &r1, &status);
				while(!(mcapi_test(&r1, &tSize1, &status))) {
					if(status != MCAPI_SUCCESS)
						check_status(status);
					OSTimeDly(1);
				}
			}
			else {
				// blocking send method will be used
				mcapi_msg_send(node2_sendEP_to_node1, node1_recvEP_from_node2, msg, msg_size, prio, &status);
				if(status != MCAPI_SUCCESS) {
					printf("node2_sendMSG_to_node1_task: Error in mcapi_msg_send() with msg_size = %d\n", msg_size); fflush(stdout);
					check_status(status);
				}
			}

//			OSTimeDly(1);	// 10 ms delay only for debugging
			msg_cnt++;
			byte_cnt = byte_cnt + msg_size;

//			printf("node2_sendMSG_to_node1_task: Message with %d bytes sent to node1\n", msg_size);
		}
//		printf("node2_sendMSG_to_node1_task: %d super-cycles executed\n", cycles);
	}
	printf("node2_sendMSG_to_node1_task: %d messages (%d bytes) sent to node1\n", msg_cnt, byte_cnt);

	// delete local endpoints
	mcapi_endpoint_delete(node2_sendEP_to_node1, &status);
	check_status(status);
//	printf("node2_sendMSG_to_node1_task: Send endpoint node2_sendEP_to_node1 deleted\n");

	node2_sendMSG_to_node1_thread_flag = 0;
	printf("node2_sendMSG_to_node1_task will finish in 1 second!\n"); fflush(stdout);
	usleep(1000000);

#ifdef UCOSII
	/* uC/OS-II specific code to stop this task */
	while(1) OSTaskSuspend(OS_PRIO_SELF);
#endif

#ifdef LINUX
	/* uClinux specific code to stop this task */
	pthread_exit(NULL);
#endif
}

/**************************************************************
 * task: init_task()
 * Initialization task which initializes MCAPI, creates
 * endpoints and starts the necessary sending and receiving
 * tasks.
 *************************************************************/
void init_task(void* pdata)
{
	uint8_t err;

	mcapi_param_t mcapi_parameters;
	mcapi_info_t mcapi_info;
	mcapi_status_t status = MCAPI_FALSE;

	/* Initialize MCAPI **************************************/
	// MY_DOMAIN and NIOS_2_NODE_ID are defined in file mappings.h
	mcapi_initialize(MY_DOMAIN, NIOS_2_NODE_ID, NULL, &mcapi_parameters, &mcapi_info, &status);
	check_status(status);
	printf("init_task: MCAPI initialized\n"); fflush(stdout);

	mcapi_domain_t mydomain = mcapi_domain_id_get(&status);
	check_status(status);
	printf("init_task:   - My domain: %i\n", mydomain); fflush(stdout);

	mcapi_node_t mynode = mcapi_node_id_get(&status);
	check_status(status);
	printf("init_task:   - My node: %i\n", mynode); fflush(stdout);

/* PATH3 related initialization **********************************/
#ifdef	ENABLE_PATH3
	// create local receive endpoint
	node2_recvEP_from_node0 = mcapi_endpoint_create(NODE2_RECVPORT_FROM_NODE0, &status);
	check_status(status);
	printf("init_task:   - Local endpoint on node2 (used to receive data from node0) created\n"); fflush(stdout);

	// create receive task receiving data from node0
#ifdef UCOSII
	err = OSTaskCreateExt(node2_recvMSG_from_node0_task,
						  NULL,
						  (void *)&node2_recvMSG_from_node0_task_stk[TASK_STACKSIZE-1],
						  NODE2_RECVMSG_FROM_NODE0_TASK_PRIO,
						  NODE2_RECVMSG_FROM_NODE0_TASK_PRIO,
						  node2_recvMSG_from_node0_task_stk,
						  TASK_STACKSIZE,
						  NULL,
						  0);
	if(err != OS_NO_ERR) {
		printf("init_task: Error in OSTaskCreateExt(): %i\n",err);
	}
#endif // UCOSII

#ifdef LINUX
	/* uClinux specific code */
	err = pthread_create(&node2_recvMSG_from_node0_thread, NULL, (void*)&node1_recvMSG_from_node0_task, NULL);
	if(err != 0) {
		printf("init_task: Error in pthread_create\n");
	}
#endif // LINUX
	node2_recvMSG_from_node0_thread_flag = 1; 	// mark thread as alive
#endif // PATH3

/* PATH5 (node1 -> node2) related initialization **********************************/
#ifdef	ENABLE_PATH5
	// create local receive endpoint
	node2_recvEP_from_node1 = mcapi_endpoint_create(NODE2_RECVPORT_FROM_NODE1, &status);
	check_status(status);
	printf("init_task:   - Local endpoint on node2 (used to receive data from node1) created\n"); fflush(stdout);

	// create receive task receiving data from node1
#ifdef UCOSII
	err = OSTaskCreateExt(node2_recvMSG_from_node1_task,
						  NULL,
						  (void *)&node2_recvMSG_from_node1_task_stk[TASK_STACKSIZE-1],
						  NODE2_RECVMSG_FROM_NODE1_TASK_PRIO,
						  NODE2_RECVMSG_FROM_NODE1_TASK_PRIO,
						  node2_recvMSG_from_node1_task_stk,
						  TASK_STACKSIZE,
						  NULL,
						  0);
	if(err != OS_NO_ERR) {
		printf("init_task: Error in OSTaskCreateExt(): %i\n",err);
	}
#endif // UCOSII

#ifdef LINUX
/* uClinux specific code */
	err = pthread_create(&node2_recvMSG_from_node1_thread, NULL, (void*)&node1_recvMSG_from_node2_task, NULL);
	if(err != 0) {
		printf("init_task: Error in pthread_create\n");
	}
#endif // LINUX
	node2_recvMSG_from_node1_thread_flag = 1;	// mark thread as alive
#endif // PATH5

/* PATH4 related initialization **********************************/
#ifdef	ENABLE_PATH4
	// create local send endpoint
	node2_sendEP_to_node0 = mcapi_endpoint_create(NODE2_SENDPORT_TO_NODE0, &status);
	check_status(status);
	printf("init_task:   - Local endpoint on node2 (used to send data to node0) created\n"); fflush(stdout);

	/* get remote receive endpoints **********************************/
	node0_recvEP_from_node2 = mcapi_endpoint_get(MY_DOMAIN, NIOS_0_NODE_ID, NODE0_RECVPORT_FROM_NODE2, MCAPI_TIMEOUT_INFINITE, &status);
	check_status(status);
	printf("init_task:   - Remote endpoint on node0 (used to receive data from node2) received\n"); fflush(stdout);

	// create send task sending data to node0
#ifdef UCOSII
	/* uC/OS-II specific code */
	err = OSTaskCreateExt(node2_sendMSG_to_node0_task,
						  NULL,
						  (void *)&node2_sendMSG_to_node0_task_stk[TASK_STACKSIZE-1],
						  NODE2_SENDMSG_TO_NODE0_TASK_PRIO,
						  NODE2_SENDMSG_TO_NODE0_TASK_PRIO,
						  node2_sendMSG_to_node0_task_stk,
						  TASK_STACKSIZE,
						  NULL,
						  0);
	if(err != OS_NO_ERR) {
		printf("init_task: Error in OSTaskCreateExt(): %i\n",err);
	}
#endif //UCOSII

#ifdef LINUX
	/* uClinux specific code */
	err = pthread_create(&node2_sendMSG_to_node0_thread, NULL, (void*)&node1_sendMSG_to_node0_task, NULL);
	if(err != 0) {
		printf("init_task: Error in pthread_create\n");
	}
#endif // LINUX
	node2_sendMSG_to_node0_thread_flag = 1;	// mark thread as alive
#endif //PATH4

/* PATH6 (node2 -> node1) related initialization **********************************/
#ifdef	ENABLE_PATH6
	// create local send endpoint
	node2_sendEP_to_node1 = mcapi_endpoint_create(NODE2_SENDPORT_TO_NODE1, &status);
	check_status(status);
	printf("init_task:   - Local endpoint on node2 (used to send data to node1) created\n"); fflush(stdout);

	/* get remote receive endpoints **********************************/
	node1_recvEP_from_node2 = mcapi_endpoint_get(MY_DOMAIN, NIOS_1_NODE_ID, NODE1_RECVPORT_FROM_NODE2, MCAPI_TIMEOUT_INFINITE, &status);
	check_status(status);
	printf("init_task:   - Remote endpoint on node1 (used to receive data from node2) received\n"); fflush(stdout);

	// create send task sending data to node1
#ifdef UCOSII
	/* uC/OS-II specific code */
	err = OSTaskCreateExt(node2_sendMSG_to_node1_task,
						  NULL,
						  (void *)&node2_sendMSG_to_node1_task_stk[TASK_STACKSIZE-1],
						  NODE2_SENDMSG_TO_NODE1_TASK_PRIO,
						  NODE2_SENDMSG_TO_NODE1_TASK_PRIO,
						  node2_sendMSG_to_node1_task_stk,
						  TASK_STACKSIZE,
						  NULL,
						  0);
	if(err != OS_NO_ERR) {
		printf("init_task: Error in OSTaskCreateExt(): %i\n",err);
	}
#endif // UCOSII

#ifdef LINUX
	/* uClinux specific code */
	err = pthread_create(&node2_sendMSG_to_node1_thread, NULL, (void*)&node1_sendMSG_to_node2_task, NULL);
	if(err != 0) {
		printf("init_task: Error in pthread_create\n");
	}
#endif // LINUX
	node2_sendMSG_to_node1_thread_flag = 1;	// mark thread as alive
#endif // PATH6

	// wait till the created threads have been finished.
	while((node2_sendMSG_to_node0_thread_flag +
		   node2_sendMSG_to_node1_thread_flag +
		   node2_recvMSG_from_node0_thread_flag +
		   node2_recvMSG_from_node1_thread_flag) != 0)
		usleep(1000000);

	// finalize MCAPI system
	init_thread_flag = 0;
	printf("init_task will finish in 1 second!\n"); fflush(stdout);
	mcapi_finalize(&status);
	check_status(status);
	usleep(1000000);

#ifdef UCOSII
	/* uC/OS-II specific code to stop this task */
	while(1) OSTaskSuspend(OS_PRIO_SELF);
#endif

#ifdef LINUX
	/* LINUX specific code to stop this task */
	pthread_exit(NULL);
#endif
}

/**************************************************************
 * function: main()
 *
 * Simply creates initialization task and starts operating
 * system.
 *************************************************************/
int main(void)
{
	uint8_t err;

#ifdef UCOSII
	/* uC/OS-II specific code */
	err = OSTaskCreateExt(init_task,
						  NULL,
						  (void *)&init_task_stk[INIT_TASK_STACKSIZE-1],
						  INIT_TASK_PRIORITY,
						  INIT_TASK_PRIORITY,
						  init_task_stk,
						  INIT_TASK_STACKSIZE,
						  NULL,
						  0);
    if(err != OS_NO_ERR) {
		printf("main: Error in OSTaskCreateExt(): %i\n",err);
	}
    OSStart();	// start operating system operation
#endif

#ifdef LINUX
/* uClinux specific code */
	err = pthread_create(&init_thread, NULL, (void*)&init_task, NULL);
	if(err != 0) {
		printf("main: Error in pthread_create\n");
		exit(0);
	}
#endif

	init_thread_flag = 1;	// indicate that init_thread is alive

	// wait until all tasks have been finished
	while(init_thread_flag != 0)
		usleep(1000000);

	printf("main: main task will end now\n"); fflush(stdout);
#ifdef UCOSII
	/* uC/OS-II specific code */
    return 0;
#endif

#ifdef LINUX
	exit(errno);
#endif
}
