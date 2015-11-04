/**
 * Collection of error codes.
 */

#ifndef S_ERRORCODES_H_
#define S_ERRORCODES_H_

//-----------------------Defines-----------------------------------------------
// general errors
#define NO_ERR					  0	// if no error exist

// I2CDriver Module
#define ERR_I2C_WRONG_STATE 	-10	// The I2C driver is in the wrong state
#define ERR_NO_ACK_ADDR 		-11	// The I2C Controller received no ack from the slave after address transmission
#define ERR_NO_ACK_DATA 		-12	// The I2C Controller received no ack after he sent data
#define ERR_I2C_ACCESS_DENIED 	-13	// The I2C driver is already in use or not initialized
#define ERR_I2C_CONNECTION		-14 // Every error while connection to an device via I2C.

// Accelerometer Driver
#define ERR_ACC_WRONG_STATE 	-20	// The ACC driver is in the wrong state

// Gyroscope Driver
#define ERR_GYRO_WRONG_STATE 	-30	// The GYRO driver is in the wrong state

// Magnetometer Driver
#define ERR_WRONG_COMPASS_STATE -40	// The COMP driver is in the wrong state
#define ERR_CALIBRATING			-41 // Can be multiple errors while calibrating the compass.

// PWM Driver
#define ERR_PWM_ILLEGAL_RANGE 	-50	// The input value is not in the correct range.

//Motor Driver
#define ERR_MOTOR_ILLEGAL_RANGE -60	// The input value is not in the correct range.

#endif /* S_ERRORCODES_H_ */
