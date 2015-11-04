/*
 * This driver provides functions to initialize the accelerometer
 * and read the values.
 */

//-----------------------Includes----------------------------------------------
#include <stdio.h>
#include "Driver_I2C.h"
#include "Driver_Accl.h"
#include "../b_errorcodes.h"

//-----------------------Attributes--------------------------------------------
enum AccelerometerState Accelerometer_state = ACC_NOTAVAILABLE;

//-----------------------Method Implementation---------------------------------
int8_t Accelerometer_init() {
	/* Check if module was already initialized */
	if (Accelerometer_state == ACC_INITIALIZED) {
		return NO_ERR;
	}
	I2CDriver_init();

	if (I2CDriver_open(I2C_400) == ERR_I2C_WRONG_STATE) {
		I2CDriver_close();
		return ERR_I2C_ACCESS_DENIED;
	}

	int8_t error = I2CDriver_write2Bytes(ADXL345_DEVICE_ADDR, ADXL345_POWER_CTL,
			8);
	if (error != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	I2CDriver_close();
	Accelerometer_state = ACC_INITIALIZED;
	return NO_ERR;
}

int8_t getAccX(int16_t *dataX) {
	if (Accelerometer_state != ACC_INITIALIZED) {
		return ERR_ACC_WRONG_STATE;
	}

	if (I2CDriver_open(I2C_400) == ERR_I2C_WRONG_STATE) {
		I2CDriver_close();
		return ERR_I2C_ACCESS_DENIED;
	}

	*dataX = 0;
	uint8_t x_lsb = 0;
	uint8_t x_msb = 0;

	int8_t error = I2CDriver_write1Byte(ADXL345_DEVICE_ADDR, ADXL345_DATAX_LSB);
	if (error != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	error = I2CDriver_read1Byte(ADXL345_DEVICE_ADDR, &x_lsb);
	if (error != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	error = I2CDriver_write1Byte(ADXL345_DEVICE_ADDR, ADXL345_DATAX_MSB);
	if (error != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	error = I2CDriver_read1Byte(ADXL345_DEVICE_ADDR, &x_msb);
	if (error != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	*dataX = x_msb;
	*dataX = (*dataX) << 8;
	*dataX = (*dataX) | x_lsb;
	*dataX = (*dataX) * (-1);

	I2CDriver_close();
	return NO_ERR;
}

int8_t getAccY(int16_t *dataY) {
	if (Accelerometer_state != ACC_INITIALIZED) {
		return ERR_ACC_WRONG_STATE;
	}
	if (I2CDriver_open(I2C_400) == ERR_I2C_WRONG_STATE) {
		I2CDriver_close();
		return ERR_I2C_ACCESS_DENIED;
	}

	*dataY = 0;
	uint8_t y_lsb = 0;
	uint8_t y_msb = 0;

	int8_t error = I2CDriver_write1Byte(ADXL345_DEVICE_ADDR, ADXL345_DATAY_LSB);
	if (error != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	error = I2CDriver_read1Byte(ADXL345_DEVICE_ADDR, &y_lsb);
	if (error != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	error = I2CDriver_write1Byte(ADXL345_DEVICE_ADDR, ADXL345_DATAY_MSB);
	if (error != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	error = I2CDriver_read1Byte(ADXL345_DEVICE_ADDR, &y_msb);
	if (error != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	*dataY = y_msb;
	*dataY = (*dataY) << 8;
	*dataY = (*dataY) | y_lsb;

	I2CDriver_close();
	return NO_ERR;
}

int8_t getAccZ(int16_t *dataZ) {
	if (Accelerometer_state != ACC_INITIALIZED) {
		return ERR_ACC_WRONG_STATE;
	}
	if (I2CDriver_open(I2C_400) == ERR_I2C_WRONG_STATE) {
		I2CDriver_close();
		return ERR_I2C_ACCESS_DENIED;
	}

	*dataZ = 0;
	uint8_t z_lsb = 0;
	uint8_t z_msb = 0;

	int8_t error = I2CDriver_write1Byte(ADXL345_DEVICE_ADDR, ADXL345_DATAZ_LSB);
	if (error != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	error = I2CDriver_read1Byte(ADXL345_DEVICE_ADDR, &z_lsb);
	if (error != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	error = I2CDriver_write1Byte(ADXL345_DEVICE_ADDR, ADXL345_DATAZ_MSB);
	if (error != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	error = I2CDriver_read1Byte(ADXL345_DEVICE_ADDR, &z_msb);
	if (error != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	*dataZ = z_msb;
	*dataZ = (*dataZ) << 8;
	*dataZ = (*dataZ) | z_lsb;
	*dataZ = (*dataZ) * (-1);

	I2CDriver_close();
	return NO_ERR;
}
