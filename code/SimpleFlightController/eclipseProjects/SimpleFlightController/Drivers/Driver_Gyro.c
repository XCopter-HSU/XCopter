/*
 * This driver provides functions to connect to the gyroscope sensor
 * and read the values.
 */

//-----------------------Includes----------------------------------------------
#include <stdio.h>
#include <unistd.h>
#include "Driver_I2C.h"
#include "Driver_Gyro.h"
#include "../b_errorcodes.h"

//-----------------------Constants---------------------------------------------
char DLPF_CFG_0 = 1 << 0;
char DLPF_CFG_1 = 1 << 1;
char DLPF_CFG_2 = 1 << 2;
char DLPF_FS_SEL_0 = 1 << 3;
char DLPF_FS_SEL_1 = 1 << 4;

//-----------------------Attributes--------------------------------------------
enum GyroscopeState Gyroscope_state = GYRO_NOTAVAILABLE;

//-----------------------Method Implementation---------------------------------
int8_t Gyroscope_init() {
	if (Gyroscope_state == GYRO_INITIALIZED) {
		return NO_ERR;
	}
	I2CDriver_init();
	if (I2CDriver_open(I2C_400) == ERR_I2C_WRONG_STATE) {
		I2CDriver_close();
		return ERR_I2C_ACCESS_DENIED;
	}

	uint8_t id = 0;
	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, WHO_AM_I) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &id) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	//printf("Id: %d\n\n", id);

	// Write 1001 into Sample Rate Divider Register.
	// Sets sample Rate to Fsample = internalSample / 10 = 100 Hz.
	// Divides the internal sample rate by (9+1).
	if (I2CDriver_write2Bytes(ITG3200_ADDR_AD0_LOW, SMPLRT_DIV, 0x09) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	// Write 11001 (0x19) into DLPF Full Scale Register.
	// Sets Gyro Full Scale Range to +/- 2000 degree/sec,
	// digital low pass filter bandwidth to 188 Hz
	// and internal sample rate = 1 kHz.
	if (I2CDriver_write2Bytes(ITG3200_ADDR_AD0_LOW, DLPF_FS,
			(DLPF_FS_SEL_0 | DLPF_FS_SEL_1 | DLPF_CFG_0)) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	// Write 1 into CLK_SEL to use PLL with X Gyro reference.
	if (I2CDriver_write2Bytes(ITG3200_ADDR_AD0_LOW, PWR_MGM, 0x01) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	I2CDriver_close();
	Gyroscope_state = GYRO_INITIALIZED;
	usleep(80000);
	return NO_ERR;
}

int8_t getGyroAll(int16_t *dataTemp, int16_t *dataX, int16_t *dataY,
		int16_t *dataZ) {
	if (Gyroscope_state != GYRO_INITIALIZED) {
		return ERR_GYRO_WRONG_STATE;
	}
	if (I2CDriver_open(I2C_400) == ERR_I2C_WRONG_STATE) {
		I2CDriver_close();
		return ERR_I2C_ACCESS_DENIED;
	}

	*dataTemp = 0;
	*dataX = 0;
	*dataY = 0;
	*dataZ = 0;

	uint8_t t_msb = 0, t_lsb = 0;
	uint8_t x_msb = 0, x_lsb = 0;
	uint8_t y_msb = 0, y_lsb = 0;
	uint8_t z_msb = 0, z_lsb = 0;

	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, TEMP_OUT_H) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &t_msb) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, TEMP_OUT_L) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &t_lsb) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, GYRO_XOUT_H) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &x_msb) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, GYRO_XOUT_L) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &x_lsb) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, GYRO_YOUT_H) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &y_msb) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, GYRO_YOUT_L) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &y_lsb) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, GYRO_ZOUT_H) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &z_msb) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, GYRO_ZOUT_L) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &z_lsb) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	*dataTemp = t_msb;
	*dataTemp = (*dataTemp) << 8;
	*dataTemp = (*dataTemp) | t_lsb;

	*dataX = x_msb;
	*dataX = (*dataX) << 8;
	*dataX = (*dataX) | x_lsb;
	*dataX = (*dataX) * (-1);

	*dataY = y_msb;
	*dataY = (*dataY) << 8;
	*dataY = (*dataY) | y_lsb;
	*dataY = (*dataY) * (-1);

	*dataZ = z_msb;
	*dataZ = (*dataZ) << 8;
	*dataZ = (*dataZ) | z_lsb;

	I2CDriver_close();
	return NO_ERR;
}

int8_t getGyroTemp(int16_t *dataTemp) {
	if (Gyroscope_state != GYRO_INITIALIZED) {
		return ERR_GYRO_WRONG_STATE;
	}
	if (I2CDriver_open(I2C_400) == ERR_I2C_WRONG_STATE) {
		I2CDriver_close();
		return ERR_I2C_ACCESS_DENIED;
	}

	*dataTemp = 0;

	uint8_t tempH = 0, tempL = 0;

	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, TEMP_OUT_H) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &tempH) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, TEMP_OUT_L) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &tempL) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	*dataTemp = tempH;
	*dataTemp = (*dataTemp) << 8;
	*dataTemp = (*dataTemp) | tempL;

	I2CDriver_close();
	return NO_ERR;
}

int8_t getGyroX(int16_t *dataX) {
	if (Gyroscope_state != GYRO_INITIALIZED) {
		return ERR_GYRO_WRONG_STATE;
	}
	if (I2CDriver_open(I2C_400) == ERR_I2C_WRONG_STATE) {
		I2CDriver_close();
		return ERR_I2C_ACCESS_DENIED;
	}

	*dataX = 0;
	uint8_t gyroXH = 0, gyroXL = 0;

	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, GYRO_XOUT_H) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &gyroXH) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, GYRO_XOUT_L) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &gyroXL) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	*dataX = gyroXH;
	*dataX = (*dataX) << 8;
	*dataX = (*dataX) | gyroXL;
	*dataX = (*dataX) * (-1);

	I2CDriver_close();
	return NO_ERR;
}

int8_t getGyroY(int16_t *dataY) {
	if (Gyroscope_state != GYRO_INITIALIZED) {
		return ERR_GYRO_WRONG_STATE;
	}
	if (I2CDriver_open(I2C_400) == ERR_I2C_WRONG_STATE) {
		I2CDriver_close();
		return ERR_I2C_ACCESS_DENIED;
	}

	*dataY = 0;
	uint8_t gyroYH = 0;
	uint8_t gyroYL = 0;

	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, GYRO_YOUT_H) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &gyroYH) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, GYRO_YOUT_L) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &gyroYL) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	*dataY = gyroYH;
	*dataY = (*dataY) << 8;
	*dataY = (*dataY) | gyroYL;
	*dataY = (*dataY) * (-1);

	I2CDriver_close();
	return NO_ERR;
}

int8_t getGyroZ(int16_t *dataZ) {
	if (Gyroscope_state != GYRO_INITIALIZED) {
		return ERR_GYRO_WRONG_STATE;
	}
	if (I2CDriver_open(I2C_400) == ERR_I2C_WRONG_STATE) {
		I2CDriver_close();
		return ERR_I2C_ACCESS_DENIED;
	}

	*dataZ = 0;
	uint8_t gyroZH = 0;
	uint8_t gyroZL = 0;

	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, GYRO_ZOUT_H) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &gyroZH) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	if (I2CDriver_write1Byte(ITG3200_ADDR_AD0_LOW, GYRO_ZOUT_L) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(ITG3200_ADDR_AD0_LOW, &gyroZL) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	*dataZ = gyroZH;
	*dataZ = (*dataZ) << 8;
	*dataZ = (*dataZ) | gyroZL;

	I2CDriver_close();
	return NO_ERR;
}
