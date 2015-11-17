/*
 * ddc_vcp_tests.h
 *
 *  Created on: Jul 28, 2014
 *      Author: rock
 */

#ifndef DDC_VCP_TESTS_H_
#define DDC_VCP_TESTS_H_

// #include <common.h>
// #include <vcp_feature_codes.h>
// #include <ddc_vcp.h>

#include <test/ddc/ddc_vcp_tests.h>

void get_luminosity_sample_code(int busno);
void demo_nvidia_bug_sample_code(int busno);
void get_luminosity_using_single_ioctl(int busno);
void probe_get_luminosity(int busno, char * write_mode, char * read_mode);

void test_get_luminosity_for_bus(int busno);

void demo_p2411_problem(int busno);

#endif /* DDC_VCP_TESTS_H_ */
