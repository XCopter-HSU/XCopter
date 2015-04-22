#include "oc_i2c_master_regs.h"
#include "oc_i2c_master.h"
#include "alt_types.h"

void oc_i2c_master_init(alt_u32 base, alt_u32 freq)
{
    // Setup prescaler for a 100KHz I2C clock based on the frequency of the oc_i2c_master clock

    alt_u32 prescale = ((freq) / (5*100000)) - 1;                   // calculate the prescaler value

    IOWR_OC_I2C_MASTER_CTR(base, 0x00);                         // disable the I2C core

    IOWR_OC_I2C_MASTER_PRERLO(base, prescale & 0xff);           // write the lo prescaler register
    IOWR_OC_I2C_MASTER_PRERHI(base, (prescale & 0xff00)>>8);    // write the hi prescaler register

    IOWR_OC_I2C_MASTER_CTR(base, OC_I2C_MASTER_CTR_CORE_EN);    // enable the I2C core
}

void oc_i2c_master_write(oc_i2c_master_dev *i2c_dev, unsigned char address, unsigned char reg, unsigned char data)
{
  alt_u8 temp;
  
  do
  {
    temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
  } while((temp & OC_I2C_MASTER_SR_TIP));

  while(1)
  {
    temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
    while(temp & OC_I2C_MASTER_SR_BUSY)
    {
      i2c_dev->busy_on_entry++;

      temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
      if(temp & OC_I2C_MASTER_SR_BUSY)
      {
        IOWR_OC_I2C_MASTER_CR(i2c_dev->base, OC_I2C_MASTER_CR_STO | OC_I2C_MASTER_SR_IF);
  
        do
        {
          temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
        } while((temp & OC_I2C_MASTER_SR_TIP) || (!(temp & OC_I2C_MASTER_SR_IF)));
      }
    }

    // write address
    IOWR_OC_I2C_MASTER_TXR(i2c_dev->base, address<<1);
    IOWR_OC_I2C_MASTER_CR(i2c_dev->base, OC_I2C_MASTER_CR_STA | OC_I2C_MASTER_CR_WR | OC_I2C_MASTER_SR_IF);
    do
    {
      temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
    } while((temp & OC_I2C_MASTER_SR_TIP) || (!(temp & OC_I2C_MASTER_SR_IF)));
    if((temp & OC_I2C_MASTER_SR_RxACK) || (temp & OC_I2C_MASTER_SR_AL))
    {
      i2c_dev->bad_cycle_term++;
      continue;
    }
    
    // write register address
    IOWR_OC_I2C_MASTER_TXR(i2c_dev->base, reg);
    IOWR_OC_I2C_MASTER_CR(i2c_dev->base, OC_I2C_MASTER_CR_WR | OC_I2C_MASTER_SR_IF);
    do
    {
      temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
    } while((temp & OC_I2C_MASTER_SR_TIP) || (!(temp & OC_I2C_MASTER_SR_IF)));
    if((temp & OC_I2C_MASTER_SR_RxACK) || (temp & OC_I2C_MASTER_SR_AL))
    {
      i2c_dev->bad_cycle_term++;
      continue;
    }
    
    // write data
    IOWR_OC_I2C_MASTER_TXR(i2c_dev->base, data);
    IOWR_OC_I2C_MASTER_CR(i2c_dev->base, OC_I2C_MASTER_CR_STO | OC_I2C_MASTER_CR_WR | OC_I2C_MASTER_SR_IF);
    do
    {
      temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
    } while((temp & OC_I2C_MASTER_SR_TIP) || (!(temp & OC_I2C_MASTER_SR_IF)));
    if((temp & OC_I2C_MASTER_SR_RxACK) || (temp & OC_I2C_MASTER_SR_AL))
    {
      i2c_dev->bad_cycle_term++;
      continue;
    }
    
    break;
  }
}

unsigned char oc_i2c_master_read(oc_i2c_master_dev *i2c_dev, unsigned char address, unsigned char reg)
{
  alt_u8 temp;
  
  do
  {
    temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
  } while((temp & OC_I2C_MASTER_SR_TIP));

  while(1)
  {
    temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
    while(temp & OC_I2C_MASTER_SR_BUSY)
    {
      i2c_dev->busy_on_entry++;
      
      temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
      if(temp & OC_I2C_MASTER_SR_BUSY)
      {
        IOWR_OC_I2C_MASTER_CR(i2c_dev->base, OC_I2C_MASTER_CR_STO | OC_I2C_MASTER_SR_IF);
  
        do
        {
          temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
        } while((temp & OC_I2C_MASTER_SR_TIP) || (!(temp & OC_I2C_MASTER_SR_IF)));
      }
    }

    // write address
    IOWR_OC_I2C_MASTER_TXR(i2c_dev->base, address<<1);
    IOWR_OC_I2C_MASTER_CR(i2c_dev->base, OC_I2C_MASTER_CR_STA | OC_I2C_MASTER_CR_WR | OC_I2C_MASTER_SR_IF);
    do
    {
      temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
    } while((temp & OC_I2C_MASTER_SR_TIP) || (!(temp & OC_I2C_MASTER_SR_IF)));
    if((temp & OC_I2C_MASTER_SR_RxACK) || (temp & OC_I2C_MASTER_SR_AL))
    {
      i2c_dev->bad_cycle_term++;
      continue;
    }
    
    // write register address
    IOWR_OC_I2C_MASTER_TXR(i2c_dev->base, reg);
    IOWR_OC_I2C_MASTER_CR(i2c_dev->base, OC_I2C_MASTER_CR_WR | OC_I2C_MASTER_CR_STO | OC_I2C_MASTER_SR_IF);
    do
    {
      temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
    } while((temp & OC_I2C_MASTER_SR_TIP) || (!(temp & OC_I2C_MASTER_SR_IF)));
    if((temp & OC_I2C_MASTER_SR_RxACK) || (temp & OC_I2C_MASTER_SR_AL))
    {
      i2c_dev->bad_cycle_term++;
      continue;
    }
    
    // write address for reading
    IOWR_OC_I2C_MASTER_TXR(i2c_dev->base, (address<<1) | 1);
    IOWR_OC_I2C_MASTER_CR(i2c_dev->base, OC_I2C_MASTER_CR_STA | OC_I2C_MASTER_CR_WR | OC_I2C_MASTER_SR_IF);
    do
    {
      temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
    } while((temp & OC_I2C_MASTER_SR_TIP) || (!(temp & OC_I2C_MASTER_SR_IF)));
    if((temp & OC_I2C_MASTER_SR_RxACK) || (temp & OC_I2C_MASTER_SR_AL))
    {
      i2c_dev->bad_cycle_term++;
      continue;
    }
    
    // read data
    IOWR_OC_I2C_MASTER_CR(i2c_dev->base, OC_I2C_MASTER_CR_RD | OC_I2C_MASTER_CR_ACK | OC_I2C_MASTER_CR_STO | OC_I2C_MASTER_SR_IF);
    do
    {
      temp = IORD_OC_I2C_MASTER_SR(i2c_dev->base);
    } while((temp & OC_I2C_MASTER_SR_TIP) || (!(temp & OC_I2C_MASTER_SR_IF)));
    if(!(temp & OC_I2C_MASTER_SR_RxACK) || (temp & OC_I2C_MASTER_SR_AL))
    {
      i2c_dev->bad_cycle_term++;
      continue;
    }
    
    break;
  }

  return IORD_OC_I2C_MASTER_RXR(i2c_dev->base);
}
