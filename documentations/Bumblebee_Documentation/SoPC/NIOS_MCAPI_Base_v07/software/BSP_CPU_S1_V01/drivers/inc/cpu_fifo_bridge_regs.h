#ifndef __CPU_FIFO_BRIDGE_REGS_H__
#define __CPU_FIFO_BRIDGE_REGS_H__

#include <io.h>

#define CPU_FIFO_BRIDGE_DATA_REG							0
#define CPU_FIFO_BRIDGE_IENABLE_REG							1
#define CPU_FIFO_BRIDGE_EVENT_REG							2
#define CPU_FIFO_BRIDGE_STATUS_REG							3
#define CPU_FIFO_BRIDGE_RECVLEVEL_REG						4
#define CPU_FIFO_BRIDGE_SENDLEVEL_REG						5

// Read slave
#define IORD_CPU_FIFO_BRIDGE_DATA(base)                   	\
        IORD(base, CPU_FIFO_BRIDGE_DATA_REG)
        
// Write slave
#define IOWR_CPU_FIFO_BRIDGE_DATA(base, data)            	\
        IOWR(base, CPU_FIFO_BRIDGE_DATA_REG, data)

// Control slave
#define IORD_CPU_FIFO_BRIDGE_IENABLE(base)                  \
        IORD(base, CPU_FIFO_BRIDGE_IENABLE_REG)

#define IORD_CPU_FIFO_BRIDGE_EVENT(base)                  	\
        IORD(base, CPU_FIFO_BRIDGE_EVENT_REG)
		
#define IORD_CPU_FIFO_BRIDGE_STATUS(base)                  	\
        IORD(base, CPU_FIFO_BRIDGE_STATUS_REG)
		
#define IORD_CPU_FIFO_BRIDGE_RECVLEVEL(base)                \
        IORD(base, CPU_FIFO_BRIDGE_RECVLEVEL_REG)
		
#define IORD_CPU_FIFO_BRIDGE_SENDLEVEL(base)                \
        IORD(base, CPU_FIFO_BRIDGE_SENDLEVEL_REG)

#define IOWR_CPU_FIFO_BRIDGE_IENABLE(base, data)            \
        IOWR(base, CPU_FIFO_BRIDGE_IENABLE_REG, data)

#define IOWR_CPU_FIFO_BRIDGE_EVENT(base, data)            	\
        IOWR(base, CPU_FIFO_BRIDGE_EVENT_REG, data)
		
#define IOWR_CPU_FIFO_BRIDGE_CLEAR(base)            		\
        IOWR(base, CPU_FIFO_BRIDGE_STATUS_REG, 0x1)

#define CPU_FIFO_BRIDGE_IENABLE_ALMOSTEMPTY_MSK			(0x01)
#define CPU_FIFO_BRIDGE_IENABLE_ALMOSTFULL_MSK			(0x02)
#define CPU_FIFO_BRIDGE_IENABLE_ALL_MSK					(0x03)

#define CPU_FIFO_BRIDGE_EVENT_ALMOSTEMPTY_MSK			(0x01)
#define CPU_FIFO_BRIDGE_EVENT_ALMOSTFULL_MSK			(0x02)
#define CPU_FIFO_BRIDGE_EVENT_ALL_MSK					(0x03)

#define CPU_FIFO_BRIDGE_SENDSTATUS_EMPTY_MSK			(0x01)
#define CPU_FIFO_BRIDGE_SENDSTATUS_ALMOSTEMPTY_MSK		(0x02)
#define CPU_FIFO_BRIDGE_SENDSTATUS_ALMOSTFULL_MSK		(0x04)
#define CPU_FIFO_BRIDGE_SENDSTATUS_FULL_MSK				(0x08)
#define CPU_FIFO_BRIDGE_RECVSTATUS_EMPTY_MSK			(0x10)
#define CPU_FIFO_BRIDGE_RECVSTATUS_ALMOSTEMPTY_MSK		(0x20)
#define CPU_FIFO_BRIDGE_RECVSTATUS_ALMOSTFULL_MSK		(0x40)
#define CPU_FIFO_BRIDGE_RECVSTATUS_FULL_MSK				(0x80)
#define CPU_FIFO_BRIDGE_STATUS_ALL_MSK					(0xFF)

#endif /* __CPU_FIFO_BRIDGE_REGS_H__ */
