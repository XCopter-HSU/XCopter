/*
 * This driver provides functions to initialize the accelerometer
 * and read the values.
 */

#ifndef B_ACCELEROMETERDRIVER_H_
#define B_ACCELEROMETERDRIVER_H_

//-----------------------Includes----------------------------------------------
#include "../stdint.h" // Include stdint.h for the use of Integers with a defined size

//-----------------------Defines-----------------------------------------------
#define ADXL345_DEVICE_ADDR 	0x53
#define ADXL345_DEVID 			0x00
#define ADXL345_RESERVED1 		0x01
#define ADXL345_THRESH_TAP 		0x1d
#define ADXL345_OFSX 			0x1e
#define ADXL345_OFSY 			0x1f
#define ADXL345_OFSZ 			0x20
#define ADXL345_DUR 			0x21
#define ADXL345_LATENT 			0x22
#define ADXL345_WINDOW 			0x23
#define ADXL345_THRESH_ACT 		0x24
#define ADXL345_THRESH_INACT 	0x25
#define ADXL345_TIME_INACT 		0x26
#define ADXL345_ACT_INACT_CTL 	0x27
#define ADXL345_THRESH_FF 		0x28
#define ADXL345_TIME_FF 		0x29
#define ADXL345_TAP_AXES 		0x2a
#define ADXL345_ACT_TAP_STATUS 	0x2b
#define ADXL345_BW_RATE 		0x2c
#define ADXL345_POWER_CTL 		0x2d
#define ADXL345_INT_ENABLE 		0x2e
#define ADXL345_INT_MAP 		0x2f
#define ADXL345_INT_SOURCE 		0x30
#define ADXL345_DATA_FORMAT 	0x31
#define ADXL345_DATAX_LSB 		0x32
#define ADXL345_DATAX_MSB		0x33
#define ADXL345_DATAY_LSB 		0x34
#define ADXL345_DATAY_MSB 		0x35
#define ADXL345_DATAZ_LSB 		0x36
#define ADXL345_DATAZ_MSB 		0x37
#define ADXL345_FIFO_CTL 		0x38
#define ADXL345_FIFO_STATUS 	0x39

//-----------------------Attributes--------------------------------------------
enum AccelerometerState {
	ACC_NOTAVAILABLE, ACC_INITIALIZED
};

//-----------------------Method Declaration------------------------------------
/**
 * This function initializes the accelerometer.
 * <p>
 * You have to call this Function before you first use the accelerometer.
 *
 * @return 	ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_I2C_CONNECTION		If an error happened while a transaction.
 * 			NO_ERR					If everything is fine.
 */
int8_t Accelerometer_init();

/**
 * This function gets the value of the x axis.
 *
 * @param	dataX	There the value will be stored. It's in two's compliment.
 * @return 	ERR_ACC_WRONG_STATE		If the accelerometer is in the wrong state.
 * 			ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_I2C_CONNECTION		If an error happened while a transaction.
 * 			NO_ERR					If everything is fine.
 */
int8_t getAccX(int16_t *dataX);

/**
 * This function gets the value of the y axis.
 *
 * @param	dataY	There the value will be stored. It's in two's compliment.
 * @return 	ERR_ACC_WRONG_STATE		If the accelerometer is in the wrong state.
 * 			ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_I2C_CONNECTION		If an error happened while a transaction.
 * 			NO_ERR					If everything is fine.
 */
int8_t getAccY(int16_t *dataY);

/**
 * This function gets the value of the z axis.
 *
 * @param	dataZ	There the value will be stored. It's in two's compliment.
 * @return 	ERR_ACC_WRONG_STATE		If the accelerometer is in the wrong state.
 * 			ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_I2C_CONNECTION		If an error happened while a transaction.
 * 			NO_ERR					If everything is fine.
 */
int8_t getAccZ(int16_t *dataZ);

#endif /* B_ACCELEROMETERDRIVER_H_ */
