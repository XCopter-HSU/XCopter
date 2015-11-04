/*
 * This is the I2C Driver that was written for an modified I2C Controller from
 * opencores.org.
 */

#ifndef B_I2CDRIVER_H_
#define B_I2CDRIVER_H_

//-----------------------Includes----------------------------------------------
#include "../stdint.h" // Include stdint.h for the use of Integers with a defined size

//-----------------------Driver States-----------------------------------------
/**
 * Enum that defines the three States of the Driver.
 * NOTAVAILABLE before the Driver is initialized.
 * INITIALIZED after initialization was executed.
 * ACTIVE when the driver is in use currently.
 */
enum DriverState { I2C_NOTAVAILABLE, I2C_INITIALIZED, I2C_ACTIVE };

//-----------------------Enum for Driver Speeds--------------------------------
enum ControllerSpeed { I2C_100, I2C_400 };

//-----------------------Method Declaration------------------------------------
/*
 * All methods use little Endian.
 * That means it will send data0 first.
 * If the function accepts a pointer and a count it
 * will send the bytes on the lowest address first.
 */

/**
 * Function init initializes the I2C Controller.
 * <p>
 * You have to call this Function before you first use the I2C Controller.
 *
 * @return Returns ERR_WRONG_STATE if the I2C Driver has already been initialized.
 */
extern int8_t I2CDriver_init();

/**
 * Function open opens the I2C Controller for Reading and Writing.
 * <p>
 * You have to call this Function before you call read or write functions.
 *
 * @param speed Defines the I2C Speed. Use I2C_400 for fast mode and I2C_100 for normal mode.
 * @return Returns ERR_WRONG_STATE if the Driver is already in use or not initialized.
 */
extern int8_t I2CDriver_open(enum ControllerSpeed speed);

/**
 * Function close closes the I2C Controller
 * <p>
 * You have to call this Function when you finished reading and writing to
 * release the Device.
 *
 * @return Returns always NO_ERR.
 */
extern int8_t I2CDriver_close();

/**
 * Function write1Byte writes 1 byte via the I2C Controller.
 *
 * @param 	address 	Is the address of the I2C slave you want to write on.
 * @param 	data 		Is the 1 byte of data you want to send via I2C.
 * @return 				Returns ERR_WRONG_STATE if driver state is not active.
 * 						Returns ERR_NO_ACK_ADDR if there was no acknowledge of a slave on this address.
 * 						Returns ERR_NO_ACK_DATA if the slave didn't send any acknowledge for the data.
 */
extern int8_t I2CDriver_write1Byte(uint8_t address, uint8_t data);

/**
 * Function write2Bytes writes 2 bytes via the I2C Controller.
 *
 * @param 	address 	Is the address of the I2C slave you want to write on.
 * @param 	data1 		Is the first byte of data you want to send via I2C.
 * @param 	data2 		Is the second byte of data you want to send via I2C.
 * @return 				Returns ERR_WRONG_STATE if driver state is not active.
 * 						Returns ERR_NO_ACK_ADDR if there was no acknowledge of a slave on this address.
 * 						Returns ERR_NO_ACK_DATA if the slave didn't send any acknowledge for the data.
 */
extern int8_t I2CDriver_write2Bytes(uint8_t address, uint8_t data0, uint8_t data1);

/**
 * Function writeBytes writes a variable count of Bytes via the I2C Controller.
 *
 * @param 	address Is the address of the I2C slave you want to write on.
 * @param 	data 	Is a pointer on a byte in memory.
 * @param 	count 	Is the count of bytes to transfer.
 * @return 				Returns ERR_WRONG_STATE if driver state is not active.
 * 						Returns ERR_NO_ACK_ADDR if there was no acknowledge of a slave on this address.
 * 						Returns ERR_NO_ACK_DATA if the slave didn't send any acknowledge for the data.
 */
extern int8_t I2CDriver_writeBytes(uint8_t address, uint8_t *data, uint32_t count);

/**
 * Function writeBytes writes a variable count of Bytes via the I2C Controller.
 *
 * @param 	address 	Is the address of the I2C slave you want to write on.
 * @param 	data 		Is an Array of Bytes you want to send via I2C.
 * @return 				Returns ERR_WRONG_STATE if driver state is not active.
 * 						Returns ERR_NO_ACK_ADDR if there was no acknowledge of a slave on this address.
 * 						Returns ERR_NO_ACK_DATA if the slave didn't send any acknowledge for the data.
 */
extern int8_t I2CDriver_writeByteArray(uint8_t address, uint8_t data[]);

/**
 * Function read1Byte reads one byte via the I2C Controller.
 *
 * @param 	address 	Is the address of the I2C slave you want to read off.
 * @param 	data 		Is a pointer on the destination of the 1 byte.
 * @return 				Returns ERR_WRONG_STATE if driver state is not active.
 * 						Returns ERR_NO_ACK_ADDR if there was no acknowledge of a slave on this address.
 */
extern int8_t I2CDriver_read1Byte(uint8_t address, uint8_t *data);

/**
 * Function that reads 2 Bytes via the I2C Controller.
 *
 * @param 	address 	Is the address of the I2C slave you want to read off.
 * @param 	data1 		Is a pointer on the destination of the first byte.
 * @param 	data2 		Is a pointer on the destination of the second byte.
 * @return 				Returns ERR_WRONG_STATE if driver state is not active.
 * 						Returns ERR_NO_ACK_ADDR if there was no acknowledge of a slave on this address.
 */
extern int8_t I2CDriver_read2Bytes(uint8_t address, uint8_t *data0, uint8_t *data1);

/**
 * Function that reads a variable length of bytes from the I2C Controller.
 *
 * @param 	address 	Is the address of the I2C slave you want to read off.
 * @param 	data1 		Is a pointer on the destination of the first byte.
 * @param 	count 		Is the count of bytes to receive.
 * @return 				Returns ERR_WRONG_STATE if driver state is not active.
 * 						Returns ERR_NO_ACK_ADDR if there was no acknowledge of a slave on this address.
 */
extern int8_t I2CDriver_readBytes(uint8_t address, uint8_t *data, uint32_t count);

/**
 * Function that reads a variable length of bytes from the I2C Controller.
 *
 * @param 	address 	Is the address of the I2C slave you want to read off.
 * @param 	data 		Is the Array in wich the received bytes will be written.
 * @return 				Returns ERR_WRONG_STATE if driver state is not active.
 * 						Returns ERR_NO_ACK_ADDR if there was no acknowledge of a slave on this address.
 */
extern int8_t I2CDriver_readByteArray(uint8_t address, uint8_t data[]);

/**
 * Function that writes and reads a variable length of bytes to and from the I2C Controller.
 * <p>
 * No stop condition is generated between the write and the read. A repeated start is generated.
 *
 * @param 	address 	Is the address of the I2C slave you want to write on and read off.
 * @param 	dataWrite 	Is the Pointer on the data wich has to be written.
 * @param 	dataRead 	Is the Pointer on the address from wich the read data has to be saved.
 * @param 	countWrite 	Is the count of the data bytes that have to be written.
 * @param 	countRead 	Is the count of the data bytes that have to be read.
 * @return 				Returns ERR_WRONG_STATE if driver state is not active.
 * 						Returns ERR_NO_ACK_ADDR if there was no acknowledge of a slave on this address.
 * 						Returns ERR_NO_ACK_DATA if the slave didn't send any acknowledge for the data.
 */
extern int8_t I2CDriver_writeReadBytes(uint8_t address, uint8_t *dataWrite, uint8_t *dataRead, uint32_t countWrite, uint32_t countRead);

#endif /* B_I2CDRIVER_H_ */
