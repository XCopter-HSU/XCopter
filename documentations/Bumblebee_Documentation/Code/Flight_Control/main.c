/**
 * This is the main file. Here starts the program.
 */

//-----------------------Includes----------------------------------------------
#include <stdio.h>
#include "drivers/b_i2cdriver.h"
#include "drivers/b_pwmdriver.h"
#include "tests/test_acc.h"
#include "tests/test_gyro.h"
#include "tests/test_pwm.h"
#include "tests/test_compass.h"
#include "tests/test_motors.h"
#include "tests/test_9dof.h"

//-----------------------Method Implementation---------------------------------
/*
 * Include here the needed tests.
 */
void tests() {
	// 9DoF stick
//	test_9dofStick();

	// accelerometer
//	test_getAccX();
//	test_getAccY();
//	test_getAccZ();

	// magnetometer
//	test_getRawValues();
//	test_getValues();

	// gyroscope
//	test_getGyroAll();
//	test_getGyroTemp();
//	test_getGyroX();
//	test_getGyroY();
//	test_getGyroZ();
//	test_AllSingleMethods();

	// motors
//	test_runAllMotorsWithSpeedX(10);

	// pwm
//	testPWM(PWM_1);
}

//-----------------------MAIN--------------------------------------------------
int main(int argc, char* argv[]) {
	printf("---- main ----\n");

	tests();

	printf("\n---- end ----\n");
	return 0;
}

