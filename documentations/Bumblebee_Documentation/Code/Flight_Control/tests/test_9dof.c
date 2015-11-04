/*
 * Implementation of the tests for the 9DOF Stick
 */

//-----------------------Includes----------------------------------------------
#include "../configuration/stdint.h"
#include "../configuration/b_errorcodes.h"
#include "../drivers/b_accelerometerdriver.h"
#include "../drivers/b_CompassDriver.h"
#include "../drivers/b_gyroscopedriver.h"
#include <stdio.h>
#include <unistd.h>

//-----------------------Method Declaration------------------------------------
void test_9dofStick() {
	printf("Starting the 9DoF Stick test.\n\n");

	int16_t accX = 0, accY = 0, accZ = 0;
	int16_t gyroT = 0, gyroX = 0, gyroY = 0, gyroZ = 0;
	int16_t comX = 0, comY = 0, comZ = 0;
	int8_t errorcode = 0;

	errorcode = Accelerometer_init();
	if (errorcode != NO_ERR) {
		printf("GAME OVER! ACC ERROR: %d\n", errorcode);
		return;
	}
	errorcode = Gyroscope_init();
	if (errorcode != NO_ERR) {
		printf("GAME OVER! GYRO ERROR: %d\n", errorcode);
		return;
	}
	errorcode = Compass_init();
	if (errorcode != NO_ERR) {
		printf("GAME OVER! COM ERROR: %d\n", errorcode);
		return;
	}

	printf("Start measurement:\n");
	while (1) {
		getAccX(&accX);
		getAccY(&accY);
		getAccZ(&accZ);
		getGyroAll(&gyroT, &gyroX, &gyroY, &gyroZ);
		Compass_getRawValues(&comX, &comY, &comZ);
		printf("ACC: %d\t%d\t%d\tGYRO: %d\t%d\t%d\t%d\tCOMP: %d\t%d\t%d\n",
				accX, accY, accZ,
				gyroT, gyroX, gyroY, gyroZ,
				comX, comY, comZ);
		usleep(1000000);
	}
}
