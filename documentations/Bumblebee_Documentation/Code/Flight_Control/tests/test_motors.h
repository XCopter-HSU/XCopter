/*
 * This module provides some tests for b_motordriver.
 */

#ifndef TEST_MOTORS_H_
#define TEST_MOTORS_H_

//-----------------------Includes----------------------------------------------
#include "../configuration/stdint.h"

//-----------------------Method Declaration------------------------------------
/**
 * This function tests the motors. They all get initialized and then the
 * motor speed is set to x. With "return" can switch between on and off.
 *
 * @param	speed	This is the speed of the motors.
 */
void test_runAllMotorsWithSpeedX(uint8_t speed);

#endif /* TEST_MOTORS_H_ */
