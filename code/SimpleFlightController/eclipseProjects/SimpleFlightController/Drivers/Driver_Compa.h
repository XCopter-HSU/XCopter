/*
 * This driver provides several methods to interact with
 * the HMC5883L Compass sensor.
 *
 *  Created on: 07.01.2015
 */

#ifndef B_COMPASSDRIVER_H_
#define B_COMPASSDRIVER_H_

//-----------------------Includes----------------------------------------------
#include "../stdint.h"

//-----------------------Defines-----------------------------------------------
#define HMC5883L_DEVICE_ADDR 				0x1E 	// Base address of the device
#define HMC5883L_CONFIGURATION_REGISTER_A 	0x00	// Read/Write
#define HMC5883L_CONFIGURATION_REGISTER_B 	0x01	// Read/Write
#define HMC5883L_MODE_REGISTER 				0x02	// Read/Write
#define HMC5883L_DATA_OUTPUT_X_MSB_REGISTER	0x03	// Read
#define HMC5883L_DATA_OUTPUT_X_LSB_REGISTER 0x04	// Read
#define HMC5883L_DATA_OUTPUT_Z_MSB_REGISTER	0x05	// Read
#define HMC5883L_DATA_OUTPUT_Z_LSB_REGISTER	0x06	// Read
#define HMC5883L_DATE_OUTPUT_Y_MSB_REGISTER	0x07	// Read
#define HMC5883L_DATA_OUTPUT_Y_LSB_REGISTER	0x08	// Read
#define HMC5883L_STATUS_REGISTER			0x09	// Read
#define HMC5883L_IDENTIFICATION_REGISTER_A	0x0A	// Read
#define HMC5883L_IDENTIFICATION_REGISTER_B	0x0B	// Read
#define HMC5883L_IDENTIFICATION_REGISTER_C	0x0C	// Read
// Some values for the self test
#define HMC58X3_X_SELF_TEST_GAUSS (+1.16)
#define HMC58X3_Y_SELF_TEST_GAUSS (HMC58X3_X_SELF_TEST_GAUSS)
#define HMC58X3_Z_SELF_TEST_GAUSS (+1.08)

#define SELF_TEST_LOW_LIMIT  (243.0/390.0)   // Low limit when gain is 5.
#define SELF_TEST_HIGH_LIMIT (575.0/390.0)   // High limit when gain is 5.
#define HMC_POS_BIAS 1
#define HMC_NEG_BIAS 2

//-----------------------Attributes--------------------------------------------
enum CompassState {
	COMPASS_NOTAVAILABLE, COMPASS_INITIALIZED
};

//-----------------------Method Declaration------------------------------------
/**
 * This function initialize the compass sensor and all needed other components.
 * <p>
 * You have to call this Function before you first use the Compass Sensor.
 *
 * @return  ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_I2C_CONNECTION		If an error happened while a transaction.
 * 			NO_ERR					If everything is fine.
 */
int8_t Compass_init();

/**
 * Call this function to get all raw values of the compass sensor.
 *
 * @param	Xdata	There the x axis value is stored.
 * 			Ydata	There the y axis value is stored.
 * 			Zdata	There the z axis value is stored.
 * @return	ERR_WRONG_COMPASS_STATE	If the magnetometer is not initialized.
 * 			ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_I2C_CONNECTION		If an error happened while a transaction.
 * 			NO_ERR					If everything is fine.
 */
int8_t Compass_getRawValues(int16_t *Xdata, int16_t *Ydata, int16_t *Zdata);

/**
 * Call this function to get the calibrated values in float.
 * <p>
 * The Compass_calibrate method should be run before.
 *
 * @param	x	This is the value of the x axis.
 * 			y	This is the value of the y axis.
 * 			z	This is the value of the z axis.
 * @return	ERR_WRONG_COMPASS_STATE	If the magnetometer is not initialized.
 * 			ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_I2C_CONNECTION		If an error happened while a transaction.
 * 			NO_ERR					If everything is fine.
 */
int8_t Compass_getValues(float *x, float *y, float *z);

/**
 * This function calibrates the sensor. After running this the
 * values can be get calibrated by the getValues function.
 *
 * @param	gain		This is the starting gain value.
 * 			n_samples	This is the samples rate.
 * @return	ERR_WRONG_COMPASS_STATE	If the magnetometer is not initialized.
 * 			ERR_I2C_ACCESS_DENIED	If the I2C driver could not be opened.
 * 			ERR_CALIBRATING			If an error happens while the calibration.
 * 			NO_ERR					If everything is fine.
 */
int8_t Compass_calibrate(uint8_t gain, uint32_t n_samples);

#endif /* B_COMPASSDRIVER_H_ */
