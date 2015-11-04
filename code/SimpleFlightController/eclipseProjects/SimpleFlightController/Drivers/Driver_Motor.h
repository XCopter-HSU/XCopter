/*
 * This is a driver to control the motors. You have to initialize all motor
 * controllers before start to set the motor speed.
 * <p>
 * This is an abstraction of the pwmdriver.
 */

#ifndef B_MOTORDRIVER_H_
#define B_MOTORDRIVER_H_

//-----------------------Includes----------------------------------------------
#include "../stdint.h"

//-----------------------Attributes--------------------------------------------
enum Motor {
	Motor_Front_Left,
	Motor_Front_Right,
	Motor_Middle_Left,
	Motor_Middle_Right,
	Motor_Back_Left,
	Motor_Back_Right
};

//-----------------------Method Declaration------------------------------------
/**
 * Function init initializes the Motordriver.
 * <p>
 * You should call this function before you first use the motors. It initialize
 * the controllers for the motor. Read the how-to for instructions.
 *
 * @return	NO_ERR	If everything is fine.
 */
int8_t MotorDriver_init();

/**
 * Function setSpeed.
 * <p>
 * Sets the motor speed in a range of 1 to 254.
 *
 * @param 	speed 	The speed value whos range is 1 to 254
 * @param 	motor 	The motor that shall be set.
 * @return	ERR_MOTOR_ILLEGAL_RANGE	If the range does not fit.
 * 			NO_ERR					If everything is fine.
 */
int8_t MotorDriver_setSpeed(uint8_t speed, enum Motor motor);

/**
 * Function setSpeedOfAllMotors.
 * <p>
 * Sets the motor speed of all motors in a range of 1 to 254.
 *
 * @param	speed	The speed value in a range of 1 to 254.
 * @return	ERR_MOTOR_ILLEGAL_RANGE	If the range does not fit.
 * 			NO_ERR					If everything is fine.
 */
int8_t MotorDriver_setSpeedOfAllMotors(uint8_t speed);

/**
 * Function setSpeedPercent.
 * <p>
 * Sets the motor speed in a range of 0 to 100 percentage.
 *
 * @param 	speed 	The speed value whos range is 0 to 100 in percent.
 * @param 	motor 	The motor that shall be set.
 * @return	ERR_MOTOR_ILLEGAL_RANGE	If the range does not fit.
 * 			NO_ERR					If everything is fine.
 */
int8_t MotorDriver_setSpeedPercent(uint8_t speed, enum Motor motor);

#endif
