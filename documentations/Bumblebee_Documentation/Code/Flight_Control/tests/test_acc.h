/*
 * This module provides some tests for the accelerometer driver.
 */

#ifndef TEST_ACC_H_
#define TEST_ACC_H_

//-----------------------Method Declaration------------------------------------
/**
 * This test reads the x axis value and prints it out.
 */
void test_getAccX();

/**
 * This test reads the y axis value and prints it out.
 */
void test_getAccY();

/**
 * This test reads the z axis value and prints it out.
 */
void test_getAccZ();

/**
 * This test reads all values and prints them out.
 */
void test_getAllAcc();

#endif /* TEST_ACC_H_ */
