/*
 * This module provides some functions to test the gyroscope.
 */

#ifndef TEST_GYRO_H_
#define TEST_GYRO_H_

//-----------------------Method Declaration------------------------------------
/**
 * This test reads all values with the getGyroAll method and prints them out.
 */
void test_getGyroAll();

/**
 * This test reads the temperature value and prints it out.
 */
void test_getGyroTemp();

/**
 * This test reads the x axis value and prints it out.
 */
void test_getGyroX();

/**
 * This test reads the y axis value and prints it out.
 */
void test_getGyroY();

/**
 * This test reads the z axis value and prints it out.
 */
void test_getGyroZ();

/**
 * This test reads all values with the single methods and prints them out.
 */
void test_AllSingleMethods();

#endif /* TEST_GYRO_H_ */
