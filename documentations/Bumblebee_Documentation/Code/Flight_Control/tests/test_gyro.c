/*
 * This module provides some functions to test the gyroscope.
 */

//-----------------------Includes----------------------------------------------
#include "../drivers/b_gyroscopedriver.h"
#include "../configuration/b_errorcodes.h"
#include "test_gyro.h"
#include <stdio.h>
#include <unistd.h>

//-----------------------Method Implementation---------------------------------
void test_getGyroAll() {
	printf("----- test_getGyroAll ------\n\n");

	if (Gyroscope_init() != NO_ERR) {
		printf("Error: Initialization of the gyroscope driver!\n");
	}

	int16_t temp, value_x, value_y, value_z;
	int i = 0;
	for (i = 0; i < 20; i++) {
		if (getGyroAll(&temp, &value_x, &value_y, &value_z) != NO_ERR) {
			printf("Error: Reading all values!\n");
		}
		printf("Temp: %d\nX: %d\nY: %d\nZ: %d\n\n", temp, value_x, value_y,
				value_z);
		usleep(1000000);
	}

	printf("\n-------- finished ----------\n");
}

void test_getGyroTemp() {
	printf("----- test_getGyroTemp -----\n\n");

	if (Gyroscope_init() != NO_ERR) {
		printf("Error: Initialization of the gyroscope driver!\n");
	}

	int16_t temp;
	int i = 0;
	for (i = 0; i < 20; i++) {
		if (getGyroTemp(&temp) != NO_ERR) {
			printf("Error: Reading temperature value failed!\n");
		}
		printf("Temp: %d\n", temp);
		usleep(1000000);
	}

	printf("\n-------- finished ----------\n");
}

void test_getGyroX() {
	printf("----- test_getGyroX --------\n\n");

	if (Gyroscope_init() != NO_ERR) {
		printf("Error: Initialization of the gyroscope driver!");
	}

	int16_t value_x;
	int i = 0;
	for (i = 0; i < 20; i++) {
		if (getGyroX(&value_x) != NO_ERR) {
			printf("Error: Reading x value failed!");
		}
		printf("X: %d\n", value_x);
		usleep(1000000);
	}

	printf("\n-------- finished ----------\n");
}

void test_getGyroY() {
	printf("----- test_getGyroY --------\n\n");

	if (Gyroscope_init() != NO_ERR) {
		printf("Error: Initialization of the gyroscope driver!");
	}

	int16_t value_y;
	int i = 0;
	for (i = 0; i < 20; i++) {
		if (getGyroY(&value_y) != NO_ERR) {
			printf("Error: Reading y value failed!");
		}
		printf("Y: %d\n", value_y);
		usleep(1000000);
	}

	printf("\n-------- finished ----------\n");
}

void test_getGyroZ() {
	printf("----- test_getGyroZ --------\n\n");

	if (Gyroscope_init() != NO_ERR) {
		printf("Error: Initialization of the gyroscope driver!");
	}

	int16_t value_z;
	int i = 0;
	for (i = 0; i < 20; i++) {
		if (getGyroZ(&value_z) != NO_ERR) {
			printf("Error: Reading z value failed!");
		}
		printf("Z: %d\n", value_z);
		usleep(1000000);
	}

	printf("\n-------- finished ----------\n");
}

void test_AllSingleMethods() {
	printf("----- test_AllSingleMethods --------\n\n");

	if (Gyroscope_init() != NO_ERR) {
		printf("Error: Initialization of the gyroscope driver!");
	}

	int16_t value_x, value_y, value_z;
	int i = 0;
	for (i = 0; i < 20; i++) {
		if (getGyroX(&value_x) != NO_ERR) {
			printf("Error: Reading x value failed!");
		}
		if (getGyroY(&value_y) != NO_ERR) {
			printf("Error: Reading y value failed!");
		}
		if (getGyroZ(&value_z) != NO_ERR) {
			printf("Error: Reading z value failed!");
		}
		printf("X: %d\nY: %d\nZ: %d\n\n", value_x, value_y, value_z);
		usleep(1000000);
	}
}
