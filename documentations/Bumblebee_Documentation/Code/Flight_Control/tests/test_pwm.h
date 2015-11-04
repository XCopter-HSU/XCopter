/*
 * This module provides tests for the pwm controller.
 */

#ifndef TEST_PWM_H_
#define TEST_PWM_H_

#include "../drivers/b_pwmdriver.h"

//-----------------------Method Declaration------------------------------------
/**
 * This function tests the pwm controller. Initializing the motor controller
 *
 * @param	controller	Choose a controller which should be initialized.
 */
void testPWM(enum PwmController controller);

#endif /* TEST_PWM_H_ */
