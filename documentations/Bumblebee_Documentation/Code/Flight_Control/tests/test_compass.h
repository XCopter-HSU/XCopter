/*
 * This module provides some functions to test the magnetometer.
 */

#ifndef TEST_COMPASS_H_
#define TEST_COMPASS_H_

//-----------------------Method Declaration------------------------------------
/**
 * This function reads the raw values and prints them out.
 */
void test_getRawValues();

/**
 * This function calibrates the compass sensor and then prints the calibrated
 * values out.
 */
void test_getValues();

#endif /* TEST_COMPASS_H_ */
