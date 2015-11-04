/**************************************************************
 * FILE:               FifoDrv.c
 *
 * DESCRIPTION:
 * Driver Module which incorporates PI-layer of communication
 * infrastructure required by NIOS II MCAPI implementation.
 * This module or class contains the software necessary
 * to send or receive messages via the given physical
 * infrastructure, which in our case is a set of FIFOs.
 * There is a bidirectional FIFO-channel between any two nodes
 * (NIOSII-based systems).
 * PI_layer software provides the service to send a message
 * to any node or to receive a message from any node. This
 * service is implemented as an unconfirmed service, i.e.
 * we will not get a positive or negative acknowledgement
 * on message receipt. Therefore we have to assume a reliable,
 * error-free physical transmission channel.
 *
 * Service is provided by the following public interface
 * methods:
 * FifoDriver_open(): Initialization of PH_layer software
 *                    and related hardware
 * FifoDriver_write(): Request to send a message to another
 *                    node
 * FifoDriver_read(): Blocking read of a new message
 * FifoDriver_release(): Release and de-initialization of
 *                    any PI-layer related services and
 *                    hardware.
 *
 * Message receive operation is implemented using a
 * receive task named PH_FIFO_RcvTask() which has
 * to watch the FIFO and if the entire message has been
 * received, it has to inform the FifoDriver_read() function.
 * In order to do that each PH_FIFO_RcvTask() is accompanied
 * by an ISR named PH_FIFO_isr(). Each time the receive FIFO is
 * not empty, it will send an interrupt and CPU will execute
 * the corresponding PH_FIFO_isr(), which will handle the
 * interrupt request and will inform corresponding
 * PH_FIFO_RcvTask() to read out and process the received
 * data. If an entire message has been received,
 * FifoDriver_read() will be informed.
 *
 * PI-layer uses the following very simple packet format:
 *     ---------------------------------------
 *     | length | payload area               |
 *     ---------------------------------------
 *         |--> packet length in bytes (32 bit)
 *
 *
 * EDITION HISTORY:
 * 2014-01-31: Version 0.0 from Lukasz Malich        - ms
 * 2014-02-25: FifoDriver_init function debugging    - ms
 *             FifoDriver_release function debugging - ms
 *             FifoDriver_write function debugging   - ms
 * 2014-04-21: Version 0.4 release                   - ms
 *************************************************************/


#include <linux/module.h>	// symbols and functions needed by 
							// loadable modules
#include <linux/init.h>		// module_init and module_exit macros
#include <linux/kernel.h>	// printk()
#include <linux/fs.h>		// file_operations which allows user 
							// to open/close, read/write to device
#include <linux/cdev.h>		// it helps to register the driver 
							// to the Kernel, this is a char 
							// driver; makes cdev available
#include <linux/semaphore.h>	
#include <asm/uaccess.h>	// copy_to user; copy_from_user
#include <linux/sched.h>	// kernel API used by the driver, 
							// including functions for sleeping and 
							// nummerous variable declarations.
#include <linux/types.h>	// MINOR() macro etc
#include <linux/kthread.h>
#include <linux/interrupt.h>

#include <linux/ioport.h>
#include <linux/io.h>
#include <asm/io.h>

#include "../globals.h"		// MAX_PAYLOAD_SIZE is defined here
#include "cpu_fifo_bridge_regs.h" // FIFO registers

#include "hps.h"

// Driver specific definitions
#define DRIVER_NAME "FifoDriver"
#define DRIVER_MAJOR 244
#define DRIVER_MAX_MINOR_COUNT 2	// max number of devices using this driver

// FIFO bridge hardware related definitions
// FIFO QSys address + Bridge address
#define BASE_ADDRESS_FIFO_0 (0x00000000 + ALT_LWFPGASLVS_OFST)	// Bridge cpu_M cpu_0
// Interrupt level (calculation of interrupt level: 72 + QSys IRQ Number)
// Example: QSys IRQ Number 2 -> Interrupt level 74
#define IRQ_FIFO_0 74 
#define BASE_ADDRESS_FIFO_1 (0x00000400 + ALT_LWFPGASLVS_OFST) 	// Bridge cpu_M cpu_1
#define IRQ_FIFO_1 75 // Interrupt level

#define BRIDGE_OFFSET 0x400 //offset between single bridges

//fifo memory
#define FIFO_DEV(X) (unsigned int*)(fifo_mem + X*BRIDGE_OFFSET)

#define DEBUG_MODE_VERBOSE	// if defined detailled debug
								// information will be presented

/**************************************************************
 * minors[] contains the necessary hardware related informations
 * of each FIFO communication channel. FIFO channel with
 * base address BASE_ADDRESS_FIFO_0 and interrupt level 
 * IRQ_FIFO_0 will be associated with the driver instantiation
 * with minor number 0, and so on.
 *************************************************************/
typedef struct {
	int minor;				// slave's minor number
	unsigned int address;	// slave FIFO base address
	unsigned int irq;		// slave IRQ level
} sMinor;

static sMinor minors[] = {
	{0, BASE_ADDRESS_FIFO_0, IRQ_FIFO_0},
	{1, BASE_ADDRESS_FIFO_1, IRQ_FIFO_1}
};

// PH layer packet format
// in contrast to ucosii software, PH_pdu does not contain the
// bridge_base parameter, therefore it was renamed to PH_pduB
typedef struct {
    uint32_t length;	// length of data packet
    uint8_t  *data;		// pointer to data
} PH_pduB;

// FIFO channel related data structure
typedef struct {
	volatile uint8_t init;			// indicates driver's state 
									//(opened/closed)
	struct semaphore readyForData;	// used by read() function to 
									// signal receive task that 
									// last message has been read 
									// and corresponding buffer 
									// could be re-used
	struct task_struct *rcvThread;	// receive thread responsible
									// FIFO channel
	struct semaphore dataAvailable;	// used by receive task to 
									// signal read() function that 
									// a new message is available
	struct semaphore semTask; 		// used by ISR to signal 
									// corresponding receive task 
									// that FIFO is NOT empty
	PH_pduB receivedPacket;	
} sDataBuffer;

static sDataBuffer recvBuffer[DRIVER_MAX_MINOR_COUNT];

struct semaphore mutexSend;	// mutex to guarantee exclusive access to
							// send environment

// cdev object and some other variables
static struct cdev *FifoDriver_cdev = NULL;	// it represents our 
											// character device driver
int result = 0;								// for checking errors
dev_t dev_num = MKDEV(DRIVER_MAJOR, 0);		// calculate device number

//virtual base pointer to access FIFO 
void *fifo_mem;

/*************************************************************
 * ISR FUNCTION: PH_FIFOx_ISR()
 *
 * DESCRIPTION:
 * Waits for an interrupt from fifo_x device and sends
 * signal to waiting receive task PH_FIFOx_RcvTask() via
 * semaphore semTask.
 *************************************************************/
irqreturn_t PH_FIFO_isr(int irq, void *dev_id/*, struct pt_regs *regs*/)
{
	int minor = ((sMinor*)dev_id)->minor;

    // read FIFOs interrupt register to check for correct interrupt
    if(ioread32( FIFO_DEV(minor) + CPU_FIFO_BRIDGE_EVENT_REG ) & CPU_FIFO_BRIDGE_EVENT_ALMOSTFULL_MSK) {
    	// clear interrupt request1
    	// HAVE CARE! It is necessary to re-enable interrupt
    	// before clearing it because of a bug in FIFO bridge
    	
			iowrite32(CPU_FIFO_BRIDGE_IENABLE_ALMOSTFULL_MSK,FIFO_DEV(minor) + CPU_FIFO_BRIDGE_IENABLE_REG);
			
    	iowrite32(CPU_FIFO_BRIDGE_EVENT_ALMOSTFULL_MSK,FIFO_DEV(minor) + CPU_FIFO_BRIDGE_EVENT_REG);
    	// inform waiting task about interrupt
        if(recvBuffer[minor].semTask.count == 0) {
            up(&recvBuffer[minor].semTask);
        }
    }
    else { // unexpected interrupt has occured
    	printk(KERN_WARNING 
				"FifoDriver: PH_FIFO_ISR: unexpected interrupt with int reg. = 0x%x occured\n", 
				ioread32(FIFO_DEV(minor) + CPU_FIFO_BRIDGE_EVENT_REG));
    	return IRQ_NONE;
    }

    return IRQ_HANDLED;
}

/**************************************************************
 * TASK: PH_FIFO_RcvTask()
 *
 * DESCRIPTION:
 * Receive task responsible for receiving data from a
 * dedicated CPU/FIFO-channel. Task will wait for an
 * interrupt from the corresponding FIFO and will then
 * read-out that FIFO. After receipt of an entire packet
 * this will be passed to the upper layer (NS-layer).
 *
 * Received packet will be store in a buffer and could
 * be read out using drivers read() function.
 *
 * Format of packet (type PH_pduB) passed to NS-layer is:
 *
 * ----------------------------------
 * | length | payload area          |
 * ----------------------------------
 *     |                |--> payload data
 *     |--> packet length in bytes (32 bit)
 *
 *************************************************************/
static int PH_FIFO_RcvTask(void *pdata)
{
	int minor = ((sMinor*)pdata)->minor;
	uint32_t rcvPacketSize = 0;
	PH_pduB dataP;

    // alignment of receivebuffer[] forced because later 4 byte load
    // and store instructions may be used to copy buffer
	uint8_t receivebuffer[MAX_NS_PACK_SIZE + 4] __attribute__ ((aligned (4)));
	uint32_t *receivebuffer32 = NULL;
	
	// endless receive loop	
	while(!(recvBuffer[minor].init == 2)) {
		// wait until at least one word has been received
		if(down_interruptible(&recvBuffer[minor].semTask)) //semAlmostFull_Fifo
			printk(KERN_INFO "FifoDriver.PH_FIFO_RcvTask: could not get semTask \n");

		// check if task should be stopped		
		if(recvBuffer[minor].init == 2) {
			printk("FifoDriver.PH_FIFO_RcvTask: Thread Exit\n");
			return 0;	// stop task
		}
	
		//read Fifo until empty
		while((ioread32(FIFO_DEV(minor) + CPU_FIFO_BRIDGE_STATUS_REG) 
							& CPU_FIFO_BRIDGE_RECVSTATUS_EMPTY_MSK) == 0) {
			uint32_t data1 = ioread32(FIFO_DEV(minor) + CPU_FIFO_BRIDGE_DATA_REG);

			if(rcvPacketSize == 0) {		// first data word of packet?
				rcvPacketSize = data1;		// number of bytes to read

				// set temporary pointer to beginning of receive buffer
				receivebuffer32 = (uint32_t *) receivebuffer;
				dataP.length = 0;	  
			
#ifdef DEBUG_MODE_VERBOSE
printk(KERN_INFO "FifoDriver.PH_FIFO_RcvTask: Packet with 0x%x bytes should be received\n", 
		rcvPacketSize);
#endif

			}
			else {		//data word is not first data word of packet
				*receivebuffer32++ = data1;

#ifdef DEBUG_MODE_VERBOSE
printk(KERN_INFO "FifoDriver.PH_FIFO_RcvTask:   - data word 0x%x received\n", data1);
#endif

				dataP.length = dataP.length + 4;

				if(dataP.length >= rcvPacketSize) { // did we receive last 
													// data word of packet?
					dataP.data = receivebuffer;
					rcvPacketSize = 0;				// mark that current 
													// packet as complete

#ifdef DEBUG_MODE_VERBOSE
printk(KERN_INFO "FifoDriver.PH_FIFO_RcvTask: Packet with %d bytes has been received", dataP.length);
int i;
for(i=0; i < dataP.length; i++) {
	printk(KERN_INFO "0x%x, ", dataP.data[i]);
}
printk("\n");
#endif

					recvBuffer[minor].receivedPacket = dataP;

					// mark that a new packet has been received and could be read
					up(&recvBuffer[minor].dataAvailable);

					// wait until new packet has been read out and recvBuffer could
					// be used
					down_interruptible(&recvBuffer[minor].readyForData);

					// check if task should be stopped
					if(recvBuffer[minor].init == 2) {
						printk("FifoDriver.PH_FIFO_RcvTask: Thread Exit\n");
						return 0;	// stop task
					}
				}
			}
		}

		// normally IOWR_CPU_FIFO_BRIDGE_IENABLE() shouldn't be necessary
		iowrite32(CPU_FIFO_BRIDGE_IENABLE_ALMOSTFULL_MSK, FIFO_DEV(minor) + CPU_FIFO_BRIDGE_IENABLE_REG);
		// here, must be because of a hardware bug in FIFO bridge module
	}

	return 0;	// stop task
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
void cpu_fifo_bridge_init(uint32_t address, uint32_t ienable)
{
	uint32_t ret;
	int fifo_index = 0;

	int i;
	for(i=0;i < DRIVER_MAX_MINOR_COUNT;i++) {
		if(address == minors[i].address) {
			fifo_index = i;
			break;
		}
	}
	
	//clear send FIFO
	iowrite32(0x1, FIFO_DEV(fifo_index) + CPU_FIFO_BRIDGE_STATUS_REG);
	
	//enable interrupts specified by parameters ienable
	ret = (ioread32( FIFO_DEV(fifo_index) + CPU_FIFO_BRIDGE_IENABLE_REG) & CPU_FIFO_BRIDGE_IENABLE_ALL_MSK);
 	iowrite32((ret ^ ienable), FIFO_DEV(fifo_index) + CPU_FIFO_BRIDGE_IENABLE_REG);
	
	//clear any pending interrupt requests
 	iowrite32(CPU_FIFO_BRIDGE_EVENT_ALL_MSK, FIFO_DEV(fifo_index) + CPU_FIFO_BRIDGE_EVENT_REG);
	
}

/**************************************************************
 * FUNCTION: FifoDriver_open()
 *
 * DESCRIPTION:
 * Function is called any time the device is opened. Here any
 * kind of device initialization should be done.
 * (see PH_init() function in uC/OS-II implementation)
 * Furthermore exclusive access to the driver should be 
 * handled here.
 *
 * INPUT PARAMETERS:
 * inode: reference to the device representing file on disk, 
 *        contains information about that file
 * struct file: represents an abstract open file
 *
 * RETURN VALUE:
 * -
 *************************************************************/
static int FifoDriver_open(struct inode *inode, struct file *flip)
{
	int minor;
	minor = MINOR(inode->i_rdev);	// get my minor number

	printk(KERN_INFO "Try to open MCAPI FifoDriver with minor = %d\n", minor);
	
	// check if maximum number of minors will be exceeded
	if(minor >= DRIVER_MAX_MINOR_COUNT) {
		printk(KERN_ALERT "FifoDriver_open(): Max-Minor has been reached!\n");
		return -ENODEV;
	}
	
	// check if driver is already open
	if(recvBuffer[minor].init != 0) {
		printk(KERN_ALERT "FifoDriver_open(): Device is already open!\n");
		return -ENODEV;
	}
	
	// create synchronization semaphores and mutex
	sema_init(&recvBuffer[minor].readyForData, 0);
	sema_init(&recvBuffer[minor].dataAvailable, 0);
	sema_init(&recvBuffer[minor].semTask, 0);
	sema_init(&mutexSend,1);

	// initialize FIFO bridge hardware
	cpu_fifo_bridge_init(minors[minor].address, 0);
	
	// IRQ initialization!
	if((result = request_irq(minors[minor].irq, PH_FIFO_isr, (unsigned long) 0, DRIVER_NAME, (void *) &(minors[minor]))) < 0)	{
		printk(KERN_WARNING "FifoDriver_open(): request_irq failure (%d)\n",result);
		return -ENODEV;
	}

	// create receive thread
	switch(minor) {
		case 0:
			recvBuffer[minor].rcvThread = kthread_run(&PH_FIFO_RcvTask, 
								&(minors[minor]), "PH_FIFO0_RcvTask");
			break;
		case 1:
			recvBuffer[minor].rcvThread = kthread_run(&PH_FIFO_RcvTask, 
								&(minors[minor]), "PH_FIFO1_RcvTask");
			break;
		default:
     		printk(KERN_ALERT "FifoDriver_open(): Unknown minor number!\n");
      		return -ENODEV;
			break;
	}

	// check for thread creation error
	if (IS_ERR(recvBuffer[minor].rcvThread)) {
     		printk(KERN_WARNING 
				"FifoDriver_open(): thread PH_FIFOxRcvTask creation error!\n");
			recvBuffer[minor].init = 0;	// mark driver as closed
      		return -EAGAIN;
    	}

	// enable FIFO receive interrupts
 	iowrite32(CPU_FIFO_BRIDGE_IENABLE_ALMOSTFULL_MSK, FIFO_DEV(minor) + CPU_FIFO_BRIDGE_IENABLE_REG);

	recvBuffer[minor].init = 1;	// mark driver as open
	printk(KERN_INFO 
			"FifoDriver with minor = %d successfully opened!\n", minor);
	return 0;
}

/**************************************************************
 * FUNCTION: FifoDriver_release()
 *
 * DESCRIPTION:
 * Function is called if the device is closed. Here any
 * kind of device de-initialization should be done.
 * Furthermore device should be marked as 'free'.
 *
 * INPUT PARAMETERS:
 * inode: reference to the device representing file on disk, 
 *        contains information about that file
 * struct file: represents an abstract open file
 *
 * RETURN VALUE:
 * -
 *************************************************************/
static int FifoDriver_release(struct inode *inode, struct file *flip)
{
	int minor;
	minor = MINOR(inode->i_rdev);

	printk(KERN_INFO "Try to close MCAPI FifoDriver with minor = %d\n", minor);
	
	// disable and de-register FIFO interrupt
	iowrite32(0, FIFO_DEV(minor) + CPU_FIFO_BRIDGE_IENABLE_REG);
	free_irq(minors[minor].irq, (void *)&(minors[minor]));
	
	// stop receive thread
	if (recvBuffer[minor].rcvThread != NULL) {
		printk(KERN_INFO "FifoDriver_release: stop receive thread\n");
		recvBuffer[minor].init = 2;       
		if(recvBuffer[minor].semTask.count == 0)	// maybe receive is just
			up(&recvBuffer[minor].semTask); 		// waiting for an interrupt
													// and first has to be waked up
		kthread_stop(recvBuffer[minor].rcvThread);

		up(&recvBuffer[minor].readyForData);	// wake up user process
												// waiting in driver_read()
  	}
	
	recvBuffer[minor].init = 0;	// mark driver as closed
	printk(KERN_INFO 
			"FifoDriver with minor = %d successfully closed!\n", minor);
	return 0;
}

/**************************************************************
 * FUNCTION: FifoDriver_read(...)
 *
 * DESCRIPTION:
 * Blocking read() function. If a new packet is available
 * function will copy data to user space area pointed to by
 * bufStoreData. bufCount tells us the size of bufStoreData
 * in bytes.
 *
 * INPUT PARAMETERS:
 * - *filp:      device file pointer, required to get FIFO
 *               FIFO channel's base address
 * - *bufStoreData:   points to location in user space where
 *               packet data should be copied to
 * - bufCount:   size of bufStoreData
 * - *curOffset: unused
 *
 * RETRUN VALUE:
 * - number of data bytes read or one of the following
 *   error codes
 * - EFAULT: user buffer to small
 *           error in copy_to_user
 *************************************************************/
static ssize_t FifoDriver_read(struct file* filp, char* bufStoreData, 
							size_t bufCount, loff_t* curOffset)
{
	int minor = iminor(filp->f_path.dentry->d_inode); //(flip->private_data)->minor;

	// check size of user buffer
	if(bufCount < recvBuffer[minor].receivedPacket.length) {
		printk(KERN_WARNING "FifoDriver_read(): user buffer to small\n");
		return -EFAULT;
	}

	// wait until new packet has been received
	down_interruptible(&recvBuffer[minor].dataAvailable);

	if (copy_to_user(bufStoreData, recvBuffer[minor].receivedPacket.data, 
								recvBuffer[minor].receivedPacket.length)) {
		printk(KERN_WARNING "FifoDriver_read(): error in copy_to_user\n");
  		return -EFAULT;
 	}
	
	// tell receive task that receive buffer could be used
	// for next data packet
	up(&recvBuffer[minor].readyForData);

	return recvBuffer[minor].receivedPacket.length;	
}

/**************************************************************
 * FUNCTION: FifoDriver_write(...)
 *
 * DESCRIPTION:
 * Function will send a packet consisting with length bytes.
 * Packet payload data is located in user space and pointed
 * to by *payload.
 * HAVE CARE!!! If header and payload data should be send,
 * header_length has to be a multiple of four!
 *
 * Send packet format is:
 * ----------------------------------
 * | length | payload area          |
 * ----------------------------------
 *     |                |--> payload data
 *     |--> packet length in bytes (32 bit)
 *
 * INPUT PARAMETERS:
 * - *filp:      device file pointer, required to get FIFO
 *               FIFO channel's base address
 * - *payload:   points to location in user space where the
 *               data we have to write is stored
 * - length:     number of bytes we have to write
 * - *curOffset: unused
 *
 * RETRUN VALUE:
 * - number of data bytes written or one of the following
 *   error codes
 * - EFAULT: message length exceeds max. size
 *           error in copy_from_user()
 *************************************************************/
static ssize_t FifoDriver_write(struct file* filp, const char* payload, 
							size_t length, loff_t* curOffset)
{
	int dummy = 0;
	int i = 0;
	int num32;
	uint8_t   databuf[MAX_NS_PACK_SIZE + 4];
	uint32_t *pdata32;
	int minor = iminor(filp->f_path.dentry->d_inode); //(flip->private_data)->minor;

	// check maximum packet length
	if(length > MAX_NS_PACK_SIZE + 4) {
		printk(KERN_WARNING "FifoDriver_write(): message length exceeds max. size\n");
		return -EFAULT;
	}

	// copy data from user to kernel space
	if(copy_from_user(databuf, payload, length)) {
		printk(KERN_WARNING "FifoDriver_write(): could not copy from user\n");
		return -EFAULT;
	}

#ifdef DEBUG_MODE_VERBOSE
printk(KERN_INFO "FifoDriver_write(): packet with %d bytes should be send:\n", length);
for(i=0; i< length; i++) {
	printk(KERN_INFO "0x%x, ", databuf[i]);
}
printk(KERN_INFO "\n");
#endif
	
	// lock send environment because FifoDriver_write() could be used
	// in different threads
	if(down_interruptible(&mutexSend))
		printk(KERN_WARNING "FifoDriver_write() could not aquire mutexSend-Semaphore\n");
	
	// SEND PACKET LENGTH TO FIFO
	// wait until there in enough free space in FIFO
	// cpu_fifo_bridge_read_sendlevel() replaced by a more direct access
	while(ioread32(FIFO_DEV(minor) + CPU_FIFO_BRIDGE_SENDLEVEL_REG) > 31) {
		dummy++;
	}

	// write packet length parameter to FIFO
	iowrite32(length,FIFO_DEV(minor) + CPU_FIFO_BRIDGE_DATA_REG);

	//	SEND ORIGINAL DATA PACKET + SOME OPTIONAL PADDING BYTES
	if((length % 4) == 0)
		num32 = length/4;
	else
		num32 = length/4 + 1;	// number of 32 bit data words to sent

#ifdef DEBUG_MODE_VERBOSE
printk(KERN_INFO "FifoDriver_write(): %d 32-bit data words have to be sent\n", num32);
printk(KERN_INFO "FifoDriver_write(): to device with  minor number      = %d\n", minor);
printk(KERN_INFO "FifoDriver_write():           and   FIFO base address = 0x%x\n", minors[minor].address); 
#endif

	pdata32 = (uint32_t *) databuf;
	for(i = 0; i < num32; i++) {

#ifdef DEBUG_MODE_VERBOSE
printk(KERN_INFO"FifoDriver_write(): word %d: 0x%x will be sent\n", i, *pdata32);
#endif

		// wait until there in enough free space in FIFO
		while(ioread32(FIFO_DEV(minor) + CPU_FIFO_BRIDGE_SENDLEVEL_REG) > 31) {
			dummy++;
		}

		// send data to FIFO
		iowrite32(*pdata32++,FIFO_DEV(minor) + CPU_FIFO_BRIDGE_DATA_REG);
	}

	up(&mutexSend);	// release mutex

	return length;
}

// Tell the kernel which functions to call when user operates on our device file

static struct file_operations fops = {
	.owner   = THIS_MODULE,
	.open    = FifoDriver_open,
	.release = FifoDriver_release,
	.read	 = FifoDriver_read,
	.write   = FifoDriver_write
};


/**************************************************************
 * FUNCTION cleanup()
 * Called when module is deleted using rmmod command.
 *************************************************************/
static void __exit cleanup(void)
{
	printk(KERN_ALERT "FifoDriver.cleanup: unload module\n");
	if(FifoDriver_cdev)
		cdev_del(FifoDriver_cdev);

	unregister_chrdev_region(dev_num, DRIVER_MAX_MINOR_COUNT);
//	cdev_del(FifoDriver_cdev);

	// Device file need to be deleted as well
	
	//release memory
	
	release_mem_region(BASE_ADDRESS_FIFO_0, PAGE_SIZE);
	iounmap(fifo_mem);
}

/**************************************************************
 * FUNCTION initialization()
 * Called when module is inserted using insmod command.
 *************************************************************/
static int __init initialization(void) // __init info for kernel
{	// register module as character I/O driver (Major-number registrieren)
	result = register_chrdev_region(dev_num, DRIVER_MAX_MINOR_COUNT, DRIVER_NAME);
	if(result < 0) {
		printk(KERN_ALERT "FifoDriver.initialization: register_chrdev_region failed.\n");
		return result;
	}

	printk(KERN_INFO "\nuse mknod /dev/%s0 c %d 0 \n", DRIVER_NAME, DRIVER_MAJOR);

	FifoDriver_cdev = cdev_alloc();	//allocate Kernel space for Kernel Object
	if(FifoDriver_cdev == NULL) {
		printk(KERN_ALERT "FifoDriver.initialization: cdev_alloc() failed.\n");
		goto free_device_number;
	}
	
	cdev_init(FifoDriver_cdev, &fops);
	FifoDriver_cdev->owner = THIS_MODULE;
	
	result = cdev_add(FifoDriver_cdev, dev_num, DRIVER_MAX_MINOR_COUNT);
	if(result < 0) {
		printk(KERN_ALERT "FifoDriver.initialization: unable to add cdev to kernel. \n");
		goto free_cdev;
	}
	
		//get access to the system's physical memory
		struct resource *res;
		
		res = request_mem_region(BASE_ADDRESS_FIFO_0,PAGE_SIZE,DRIVER_NAME);
		if(res == NULL) {
				printk(KERN_ALERT "FifoDriver.initialization: request_mem_region() failed.\n");
				goto failed_mem_region;
		}
		
		fifo_mem = ioremap(BASE_ADDRESS_FIFO_0,PAGE_SIZE);
		if(fifo_mem == NULL) {
			printk(KERN_ALERT "FifoDriver.initialization: ioremap() failed.\n");
			goto failed_mapping;
		}
	
	return 0;

failed_mapping:
	release_mem_region(BASE_ADDRESS_FIFO_0,PAGE_SIZE);
failed_mem_region:
free_cdev:
	kobject_put(&FifoDriver_cdev->kobj);
	FifoDriver_cdev = NULL;
free_device_number:
	unregister_chrdev_region(dev_num, DRIVER_MAX_MINOR_COUNT);
	return -EIO;
}

module_init(initialization);
module_exit(cleanup);

MODULE_AUTHOR("Malich, Strahnen");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MCAPI FIFO channel PI-layer driver");
MODULE_SUPPORTED_DEVICE("FIFO communication channel");
