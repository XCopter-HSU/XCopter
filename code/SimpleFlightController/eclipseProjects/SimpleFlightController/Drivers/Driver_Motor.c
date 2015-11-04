/*
 * Implementation of the motor driver.
 */

//-----------------------Includes----------------------------------------------
#include "Driver_Motor.h"
#include "Driver_PWM.h"
#include "../b_errorcodes.h"
#include <stdio.h>

//-----------------------Method Implementation---------------------------------
/*
 * Function init initializes the Motordriver.
 */
int8_t MotorDriver_init() {
	// Set the speed of all motors to the highest value
	printf("Maximal\n");
	MotorDriver_setSpeedOfAllMotors(254);
	// wait for a signal of all controllers
	scanf("%c");

	// Set the speed of all motors to lowest value
	printf("Minimal\n");
	MotorDriver_setSpeedOfAllMotors(1);
	// wait for a signal of all controllers
	scanf("%c");

	return NO_ERR;
}

/*
 * Sets the motor speed in a range of 1 to 254.
 */
int8_t MotorDriver_setSpeed(uint8_t speed, enum Motor motor) {
	if (speed > 254 || speed < 1) {
		return ERR_MOTOR_ILLEGAL_RANGE;
	}
	switch (motor) {
	case Motor_Front_Left:
		PWMDriver_setSignalWidth(speed, PWM_1);
		break;
	case Motor_Front_Right:
		PWMDriver_setSignalWidth(speed, PWM_2);
		break;
	case Motor_Middle_Left:
		PWMDriver_setSignalWidth(speed, PWM_3);
		break;
	case Motor_Middle_Right:
		PWMDriver_setSignalWidth(speed, PWM_4);
		break;
	case Motor_Back_Left:
		PWMDriver_setSignalWidth(speed, PWM_5);
		break;
	case Motor_Back_Right:
		PWMDriver_setSignalWidth(speed, PWM_6);
		break;
	}
	return NO_ERR;
}

/*
 * Sets the speed of all motors.
 */
int8_t MotorDriver_setSpeedOfAllMotors(uint8_t speed) {
	if (speed > 254 || speed < 1) {
		return ERR_MOTOR_ILLEGAL_RANGE;
	}
	PWMDriver_setSignalWidth(speed, PWM_1);
	PWMDriver_setSignalWidth(speed, PWM_2);
	PWMDriver_setSignalWidth(speed, PWM_3);
	PWMDriver_setSignalWidth(speed, PWM_4);
	PWMDriver_setSignalWidth(speed, PWM_5);
	PWMDriver_setSignalWidth(speed, PWM_6);
	PWMDriver_setSignalWidth(speed, PWM_7);
	PWMDriver_setSignalWidth(speed, PWM_8);
	return NO_ERR;
}

/*
 * Sets the motor speed in percentage.
 */
int8_t MotorDriver_setSpeedPercent(uint8_t speed, enum Motor motor) {
	if (speed > 100 || speed < 0) {
		return ERR_MOTOR_ILLEGAL_RANGE;
	}
	speed = speed * ((float) 255 / 100);
	MotorDriver_setSpeed(speed, motor);

	return NO_ERR;
}
