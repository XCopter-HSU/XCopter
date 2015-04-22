#ifndef __OC_I2C_MASTER_H__
#define __OC_I2C_MASTER_H__

#include "alt_types.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

// This device stucture is passed into the read and write routines, it is used
// to provide the base address of the oc_i2c_master peripheral as well as some
// storage for some error counters that can be used to debug errant I2C
// operation if desired.
typedef volatile struct oc_i2c_master_dev
{
  volatile void*     base;               // The base address of the device
  volatile alt_u32   busy_on_entry;      // incremented if I2C controller is busy at entry of read or write routines
  volatile alt_u32   bad_cycle_term;     // incremented if slave ack is not proper during any transfer phase
} oc_i2c_master_dev;

void oc_i2c_master_init(alt_u32 base, alt_u32 freq);
void oc_i2c_master_write(oc_i2c_master_dev *i2c_dev, unsigned char address, unsigned char reg, unsigned char data);
unsigned char oc_i2c_master_read(oc_i2c_master_dev *i2c_dev, unsigned char address, unsigned char reg);

#define OC_I2C_MASTER_INSTANCE(name, dev) extern int alt_no_storage
#define OC_I2C_MASTER_INIT(name, dev) oc_i2c_master_init(name##_BASE, name##_FREQ);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __OC_I2C_MASTER_H__ */
