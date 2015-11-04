/*
 * This module provides some functions to test the magnetometer.
 */

//-----------------------Includes----------------------------------------------
#include "../configuration/b_errorcodes.h"
#include "../drivers/b_CompassDriver.h"
#include "test_compass.h"
#include <stdio.h>
#include <unistd.h>

//-----------------------Method Implementation---------------------------------
void test_getRawValues() {
	printf("----- test_getRawValues ---------\n\n");

	if (Compass_init() != NO_ERR) {
		printf("Error: Initialization of the magnetometer driver!");
	}

	int16_t value_x, value_y, value_z;
	int i = 0;
	for (i = 0; i < 20; i++) {
		if (Compass_getRawValues(&value_x, &value_y, &value_z) != NO_ERR) {
			printf("Error: Reading the raw values failed!");
		}
		printf("X: %d\tY: %d\tZ: %d\n", value_x, value_y, value_z);
		usleep(1000000);
	}

	printf("\n-------- finished ----------\n");
}

void test_getValues() {
	printf("----- test_getValues ---------\n\n");

	if (Compass_init() != NO_ERR) {
		printf("Error: Initialization of the magnetometer driver!");
	}

	float value_x, value_y, value_z;
	int i = 0;
	for (i = 0; i < 20; i++) {
		if (Compass_getValues(&value_x, &value_y, &value_z) != NO_ERR) {
			printf("Error: Reading the values failed!");
		}
		printf("X: %f\tY: %f\tZ: %f\n", value_x, value_y, value_z);
		usleep(1000000);
	}

	printf("\n-------- finished ----------\n");
}
