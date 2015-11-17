/*
 * adl_tests.h
 *
 *  Created on: Jul 27, 2014
 *      Author: rock
 */

#ifndef ADL_TESTS_H_
#define ADL_TESTS_H_

// #include <assert.h>
#include <stdlib.h>
#include <string.h>
// #include <unistd.h>
#include <stdio.h>

#define LINUX
#include <adl_sdk/adl_sdk.h>


void adl_testmain();

void diddle_adl_brightness(int iAdapterIndex, int iDisplayIndex);

void exercise_ad_calls(int iAdapterIndex, int iDisplayIndex);

void run_adapter_display_tests();

#endif /* ADL_TESTS_H_ */
