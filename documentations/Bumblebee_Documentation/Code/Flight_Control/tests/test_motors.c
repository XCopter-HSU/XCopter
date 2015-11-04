/*
 * This module provides some tests for b_motordriver.
 */

//-----------------------Includes----------------------------------------------
#include "../drivers/b_motordriver.h"
#include "../configuration/b_errorcodes.h"
#include "test_motors.h"
#include <stdio.h>

//-----------------------Method Implementation---------------------------------
/*
 * Tests the motors and the controllers.
 */
void test_runAllMotorsWithSpeedX(uint8_t speed) {
	printf("Starting motor test!\n");
	char c = 'w', b;
	printf("Initialize motors!\n");
	MotorDriver_init();

	while (1) {
		printf("Maximum\n");
		int8_t error = MotorDriver_setSpeedOfAllMotors(speed);
		if (error != NO_ERR) {
			printf("Error while setting the speed to %d: %d\n", speed, error);
		}
		scanf("%c", &b);
		printf("Minimum\n");
		error = MotorDriver_setSpeedOfAllMotors(1);
		if (error != NO_ERR) {
			printf("Error while setting the speed to 1: %d\n", error);
		}
		scanf("%c", &c);
		if (c == 'a') {
			break;
		}
	}
	printf("Motor test completed!\n");
}
