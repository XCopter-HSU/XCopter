#ifndef _OC_I2C_MASTER_REGS_H_
#define _OC_I2C_MASTER_REGS_H_

#include <io.h>

// scl prescale register lo
#define IOWR_OC_I2C_MASTER_PRERLO(base, data)  IOWR(base, 0, data)
#define IORD_OC_I2C_MASTER_PRERLO(base)        IORD(base, 0)

// scl prescale register hi
#define IOWR_OC_I2C_MASTER_PRERHI(base, data)  IOWR(base, 1, data)
#define IORD_OC_I2C_MASTER_PRERHI(base)        IORD(base, 1)

// control register
#define IOWR_OC_I2C_MASTER_CTR(base, data)     IOWR(base, 2, data)
#define IORD_OC_I2C_MASTER_CTR(base)           IORD(base, 2)

#define OC_I2C_MASTER_CTR_CORE_EN   (0x80)
#define OC_I2C_MASTER_CTR_IRQ_EN    (0x40)

// tx and rx registers
#define IOWR_OC_I2C_MASTER_TXR(base, data)     IOWR(base, 3, data)
#define IORD_OC_I2C_MASTER_RXR(base)           IORD(base, 3)

// command and status register
#define IOWR_OC_I2C_MASTER_CR(base, data)      IOWR(base, 4, data)
#define IORD_OC_I2C_MASTER_SR(base)            IORD(base, 4)

#define OC_I2C_MASTER_CR_STA   (0x80)
#define OC_I2C_MASTER_CR_STO   (0x40)
#define OC_I2C_MASTER_CR_RD    (0x20)
#define OC_I2C_MASTER_CR_WR    (0x10)
#define OC_I2C_MASTER_CR_ACK   (0x08)
#define OC_I2C_MASTER_CR_IACK  (0x01)

#define OC_I2C_MASTER_SR_RxACK (0x80)
#define OC_I2C_MASTER_SR_BUSY  (0x40)
#define OC_I2C_MASTER_SR_AL    (0x20)
#define OC_I2C_MASTER_SR_TIP   (0x02)
#define OC_I2C_MASTER_SR_IF    (0x01)
 
#endif /* _OC_I2C_MASTER_REGS_H_ */
