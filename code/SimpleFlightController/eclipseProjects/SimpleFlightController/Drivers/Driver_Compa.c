/*
 * This driver provides several methods to interact with
 * the HMC5883L Compass sensor.
 *
 *  Created on: 07.01.2015
 */

//-----------------------Includes----------------------------------------------
#include "Driver_Compa.h"
#include "Driver_I2C.h"
#include "../b_errorcodes.h"
#include <unistd.h>

//-----------------------Attributes--------------------------------------------
enum CompassState compassState = COMPASS_NOTAVAILABLE;

const int counts_per_milligauss[8] =
		{ 1370, 1090, 820, 660, 440, 390, 330, 230 };

float x_scale, y_scale, z_scale, x_max, y_max, z_max;

//-----------------------Defines-----------------------------------------------
#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

//-----------------------Method Implementation---------------------------------
int8_t Compass_init() {
	/* Check if module was already initialized */
	if (compassState == COMPASS_INITIALIZED) {
		return NO_ERR;
	}
	I2CDriver_init();

	if (I2CDriver_open(I2C_400) == ERR_I2C_WRONG_STATE) {
		I2CDriver_close();
		return ERR_I2C_ACCESS_DENIED;
	}

	usleep(5000); // wait 5ms to initialize the magnetometer

	// Set 8-average, 15 Hz default
	if (I2CDriver_write2Bytes(HMC5883L_DEVICE_ADDR,
			HMC5883L_CONFIGURATION_REGISTER_A, 0x70) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	// Set Gain = 5
	if (I2CDriver_write2Bytes(HMC5883L_DEVICE_ADDR,
			HMC5883L_CONFIGURATION_REGISTER_B, 0xA0) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	// continuous measurement mode
	if (I2CDriver_write2Bytes(HMC5883L_DEVICE_ADDR, HMC5883L_MODE_REGISTER,
			0x00) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	I2CDriver_close();
	usleep(100000);
	compassState = COMPASS_INITIALIZED;
	return NO_ERR;
}

int8_t Compass_getRawValues(int16_t *Xdata, int16_t *Ydata, int16_t *Zdata) {
	if (compassState != COMPASS_INITIALIZED) {
		return ERR_WRONG_COMPASS_STATE;
	}
	if (I2CDriver_open(I2C_400) == ERR_I2C_WRONG_STATE) {
		I2CDriver_close();
		return ERR_I2C_ACCESS_DENIED;
	}

	uint8_t x_MSB = 0, x_LSB = 0, y_MSB = 0, y_LSB = 0, z_MSB = 0, z_LSB = 0;

	// get x
	if (I2CDriver_write1Byte(HMC5883L_DEVICE_ADDR,
			HMC5883L_DATA_OUTPUT_X_MSB_REGISTER) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(HMC5883L_DEVICE_ADDR, &x_MSB) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_write1Byte(HMC5883L_DEVICE_ADDR,
			HMC5883L_DATA_OUTPUT_X_LSB_REGISTER) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(HMC5883L_DEVICE_ADDR, &x_LSB) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	// get Y
	if (I2CDriver_write1Byte(HMC5883L_DEVICE_ADDR,
			HMC5883L_DATE_OUTPUT_Y_MSB_REGISTER) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(HMC5883L_DEVICE_ADDR, &y_MSB) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_write1Byte(HMC5883L_DEVICE_ADDR,
			HMC5883L_DATA_OUTPUT_Y_LSB_REGISTER) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(HMC5883L_DEVICE_ADDR, &y_LSB) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	// get z
	if (I2CDriver_write1Byte(HMC5883L_DEVICE_ADDR,
			HMC5883L_DATA_OUTPUT_Z_MSB_REGISTER) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(HMC5883L_DEVICE_ADDR, &z_MSB) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_write1Byte(HMC5883L_DEVICE_ADDR,
			HMC5883L_DATA_OUTPUT_Z_LSB_REGISTER) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}
	if (I2CDriver_read1Byte(HMC5883L_DEVICE_ADDR, &z_LSB) != NO_ERR) {
		I2CDriver_close();
		return ERR_I2C_CONNECTION;
	}

	*Xdata = x_MSB;
	*Xdata = (*Xdata) << 8;
	*Xdata = (*Xdata) | x_LSB;

	*Ydata = y_MSB;
	*Ydata = (*Ydata) << 8;
	*Ydata = (*Ydata) | y_LSB;

	*Zdata = z_MSB;
	*Zdata = (*Zdata) << 8;
	*Zdata = (*Zdata) | z_LSB;

	I2CDriver_close();
	return NO_ERR;
}

int8_t Compass_getValues(float *x, float *y, float *z) {
	int16_t t_x, t_y, t_z;
	int8_t error = Compass_getRawValues(&t_x, &t_y, &t_z);
	*x = ((float) t_x) / (x_scale);
	*y = ((float) t_y) / (y_scale);
	*z = ((float) t_z) / (z_scale);
	return error;
}

int8_t Compass_calibrate(uint8_t gain, uint32_t n_samples) {
	if (compassState != COMPASS_INITIALIZED) {
		return ERR_WRONG_COMPASS_STATE;
	}
	if (I2CDriver_open(I2C_400) == ERR_I2C_WRONG_STATE) {
		I2CDriver_close();
		return ERR_I2C_ACCESS_DENIED;
	}

	int16_t xyz[3]; // 16 bit integer values for each axis.
	int32_t xyz_total[3] = { 0, 0, 0 }; // 32 bit totals so they won't overflow.
	int8_t bret = 1;
	int32_t low_limit, high_limit;

	if ((7 > gain) && (0 < n_samples)) {
		I2CDriver_write2Bytes(HMC5883L_DEVICE_ADDR,
				HMC5883L_CONFIGURATION_REGISTER_A, 0x010 + HMC_POS_BIAS);
		I2CDriver_write2Bytes(HMC5883L_DEVICE_ADDR,
				HMC5883L_CONFIGURATION_REGISTER_B, gain << 5);
		I2CDriver_write2Bytes(HMC5883L_DEVICE_ADDR, HMC5883L_MODE_REGISTER, 1); // Change to single measurement mode.
		usleep(100000);
		Compass_getRawValues(&xyz[0], &xyz[1], &xyz[2]);

		unsigned int i = 0;
		for (i = 0; i < n_samples; i++) {
			I2CDriver_write2Bytes(HMC5883L_DEVICE_ADDR, HMC5883L_MODE_REGISTER,
					1);
			usleep(100000);
			Compass_getRawValues(&xyz[0], &xyz[1], &xyz[2]); // Get the raw values in case the scales have already been changed.
			/*
			 Since the measurements are noisy, they should be averaged rather than taking the max.
			 */
			xyz_total[0] += xyz[0];
			xyz_total[1] += xyz[1];
			xyz_total[2] += xyz[2];
			/*
			 Detect saturation.
			 */
			if (-(1 << 12) >= min(xyz[0], min(xyz[1], xyz[2]))) {
				bret = 0;
				break;
			}
		}
		/*
		 Apply the negative bias. (Same gain)
		 */
		I2CDriver_write2Bytes(HMC5883L_DEVICE_ADDR,
				HMC5883L_CONFIGURATION_REGISTER_A, 0x010 + HMC_NEG_BIAS);
		for (i = 0; i < n_samples; i++) {
			I2CDriver_write2Bytes(HMC5883L_DEVICE_ADDR, HMC5883L_MODE_REGISTER,
					1);
			usleep(100000);
			Compass_getRawValues(&xyz[0], &xyz[1], &xyz[2]); // Get the raw values in case the scales have already been changed.
			/*
			 Since the measurements are noisy, they should be averaged.
			 */
			xyz_total[0] -= xyz[0];
			xyz_total[1] -= xyz[1];
			xyz_total[2] -= xyz[2];
			/*
			 Detect saturation.
			 */
			if (-(1 << 12) >= min(xyz[0], min(xyz[1], xyz[2]))) {
				bret = 0;
				break;
			}
		}
		/*
		 Compare the values against the expected self test bias gauss.
		 Notice, the same limits are applied to all axis.
		 */
		low_limit = SELF_TEST_LOW_LIMIT * counts_per_milligauss[gain] * 2
				* n_samples;
		high_limit = SELF_TEST_HIGH_LIMIT * counts_per_milligauss[gain] * 2
				* n_samples;

		if ((bret) && (low_limit <= xyz_total[0])
				&& (high_limit >= xyz_total[0]) && (low_limit <= xyz_total[1])
				&& (high_limit >= xyz_total[1]) && (low_limit <= xyz_total[2])
				&& (high_limit >= xyz_total[2])) {

			x_scale = (counts_per_milligauss[gain]
					* (HMC58X3_X_SELF_TEST_GAUSS * 2))
					/ (xyz_total[0] / n_samples);
			y_scale = (counts_per_milligauss[gain]
					* (HMC58X3_Y_SELF_TEST_GAUSS * 2))
					/ (xyz_total[1] / n_samples);
			z_scale = (counts_per_milligauss[gain]
					* (HMC58X3_Z_SELF_TEST_GAUSS * 2))
					/ (xyz_total[2] / n_samples);
		}
		I2CDriver_write2Bytes(HMC5883L_DEVICE_ADDR,
				HMC5883L_CONFIGURATION_REGISTER_A, 0x010); // set RegA/DOR back to default.
	} else {
		bret = 0;
	}
	if (bret == 0) {
		return ERR_CALIBRATING;
	}
	return NO_ERR;
}
