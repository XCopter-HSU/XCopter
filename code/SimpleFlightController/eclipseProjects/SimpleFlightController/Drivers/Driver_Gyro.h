/*
 * This driver provides functions to connect to the gyroscope sensor
 * and read the values.
 */

#ifndef B_GYROSCOPEDRIVER_H_
#define B_GYROSCOPEDRIVER_H_

//-----------------------Includes----------------------------------------------
#include "../stdint.h" // Include stdint.h for the use of Integers with a defined size
//-----------------------Defines-----------------------------------------------
#define ITG3200_ADDR_AD0_HIGH  0x69   //AD0=1 0x69 I2C address when AD0 is connected to HIGH (VCC) - default for sparkfun breakout
#define ITG3200_ADDR_AD0_LOW   0x68   //AD0=0 0x68 I2C address when AD0 is connected to LOW (GND)
/* ---- Registers ---- */
#define WHO_AM_I           0x00  // RW   SETUP: I2C address
#define SMPLRT_DIV         0x15  // RW   SETUP: Sample Rate Divider
#define DLPF_FS            0x16  // RW   SETUP: Digital Low Pass Filter/ Full Scale range
#define INT_CFG            0x17  // RW   Interrupt: Configuration
#define INT_STATUS         0x1A  // R	Interrupt: Status
#define TEMP_OUT_H         0x1B
#define TEMP_OUT_L         0x1C
#define GYRO_XOUT_H        0x1D
#define GYRO_XOUT_L        0x1E
#define GYRO_YOUT_H        0x1F
#define GYRO_YOUT_L        0x20
#define GYRO_ZOUT_H        0x21
#define GYRO_ZOUT_L        0x22

#define PWR_MGM            0x3E  // RW	Power Management
//-----------------------Attributes--------------------------------------------
enum GyroscopeState {
	GYRO_NOTAVAILABLE, GYRO_INITIALIZED
};

//-----------------------Method Declaration------------------------------------
/**
 * This function initializes the gyroscope sensor.
 * <p>
 * It has to be called before reading the values.
 *
 * @return	ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_I2C_CONNECTION		If an error happened while a transaction.
 * 			NO_ERR					If everything is fine.
 */
int8_t Gyroscope_init();

/**
 * This function reads all values of the gyroscope.
 *
 * @param	dataTemp	Stores the temperature value.
 * 			dataX		Stores the x axis value.
 * 			dataY		Stores the y axis value.
 * 			dataZ		Stores the z axis value.
 * @return	ERR_GYRO_WRONG_STATE	If the gyroscope is not initialized.
 * 			ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_I2C_CONNECTION		If an error happened while a transaction.
 * 			NO_ERR					If everything is fine.
 */
int8_t getGyroAll(int16_t *dataTemp, int16_t *dataX, int16_t *dataY,
		int16_t *dataZ);

/**
 * This function gets the temperature value.
 *
 * @param	dataTemp	There the temperature value gets stored.
 * @return	ERR_GYRO_WRONG_STATE	If the gyroscope is not initialized.
 * 			ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_I2C_CONNECTION		If an error happened while a transaction.
 * 			NO_ERR					If everything is fine.
 */
int8_t getGyroTemp(int16_t *dataTemp);

/**
 * This function gets the x axis value.
 *
 * @param	dataX	There the x axis value gets stored.
 * @return	ERR_GYRO_WRONG_STATE	If the gyroscope is not initialized.
 * 			ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_I2C_CONNECTION		If an error happened while a transaction.
 * 			NO_ERR					If everything is fine.
 */
int8_t getGyroX(int16_t *dataX);

/**
 * This function gets the y axis value.
 *
 * @param	dataY	There the y axis value gets stored.
 * @return	ERR_GYRO_WRONG_STATE	If the gyroscope is not initialized.
 * 			ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_I2C_CONNECTION		If an error happened while a transaction.
 * 			NO_ERR					If everything is fine.
 */
int8_t getGyroY(int16_t *dataY);

/**
 * This function gets the z axis value.
 *
 * @param	dataZ	There the z axis value gets stored.
 * @return	ERR_GYRO_WRONG_STATE	If the gyroscope is not initialized.
 * 			ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_I2C_CONNECTION		If an error happened while a transaction.
 * 			NO_ERR					If everything is fine.
 */
int8_t getGyroZ(int16_t *dataZ);

#endif /* B_GYROSCOPEDRIVER_H_ */
