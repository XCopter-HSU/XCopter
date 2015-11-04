/*
 * This module provides some tests for the accelerometer driver.
 */

//-----------------------Includes----------------------------------------------
#include "../drivers/b_accelerometerdriver.h"
#include "../configuration/b_errorcodes.h"
#include "../drivers/b_i2cdriver.h"
#include "test_acc.h"
#include <stdio.h>
#include <unistd.h>

//-----------------------Method Implementation---------------------------------
void test_getAccX() {
	printf("----- test_getAccX ---------\n\n");

	if (Accelerometer_init() != NO_ERR) {
		printf("Error: Initialization of the accelerometer driver!");
	}

	int16_t value_x = 0;
	int i = 0;
	for (i = 0; i < 20; i++) {
		if (getAccX(&value_x) != NO_ERR) {
			printf("Error: Reading x value failed!");
		}
		printf("X: %d\n", value_x);
		usleep(1000000);
	}

	printf("\n-------- finished ----------\n");
}

void test_getAccY() {
	printf("----- test_getAccY ---------\n\n");

	if (Accelerometer_init() != NO_ERR) {
		printf("Error: Initialization of the accelerometer driver!");
	}

	int16_t value_y;
	int i = 0;
	for (i = 0; i < 20; i++) {
		if (getAccY(&value_y) != NO_ERR) {
			printf("Error: Reading y value failed!");
		}
		printf("Y: %d\n", value_y);
		usleep(1000000);
	}

	printf("\n-------- finished ----------\n");
}

void test_getAccZ() {
	printf("----- test_getAccZ ---------\n\n");

	if (Accelerometer_init() != NO_ERR) {
		printf("Error: Initialization of the accelerometer driver!");
	}

	int16_t value_z;
	int i = 0;
	for (i = 0; i < 20; i++) {
		if (getAccZ(&value_z) != NO_ERR) {
			printf("Error: Reading z value failed!");
		}
		printf("Z: %d\n", value_z);
		usleep(1000000);
	}

	printf("\n-------- finished ----------\n");
}

void test_getAllAcc() {
	printf("----- test_getAllAcc -------\n");

	if (Accelerometer_init() != NO_ERR) {
		printf("Error: Initialization of the accelerometer driver!");
	}

	int16_t value_x, value_y, value_z;
	int i = 0;
	for (i = 0; i < 20; i++) {
		if (getAccX(&value_x) != NO_ERR) {
			printf("Error: Reading x value failed!");
		}
		if (getAccY(&value_y) != NO_ERR) {
			printf("Error: Reading y value failed!");
		}
		if (getAccZ(&value_z) != NO_ERR) {
			printf("Error: Reading z value failed!");
		}
		printf("X: %d\nY: %d\nZ: %d\n\n", value_x, value_y, value_z);
		usleep(1000000);
	}

	printf("\n-------- finished ----------\n");
}
