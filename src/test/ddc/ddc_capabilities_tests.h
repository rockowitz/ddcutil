/*
 * ddc_capabilities_tests.h
 *
 *  Created on: Jul 30, 2014
 *      Author: rock
 */

#ifndef DDC_CAPABILITIES_TESTS_H_
#define DDC_CAPABILITIES_TESTS_H_

#include <base/util.h>

// Exploratory programming and tests

void probe_get_capabilities(int busno, char* write_mode, char* read_mode, Byte addr);

void test_get_capabilities_for_bus(int busno);

#endif /* DDC_CAPABILITIES_TESTS_H_ */
