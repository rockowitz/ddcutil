/*
 * i2c_edid_tests.h
 *
 *  Created on: Jul 30, 2014
 *      Author: rock
 */

#ifndef I2C_EDID_TESTS_H_
#define I2C_EDID_TESTS_H_


// Exploratory programming functions.
// Just try to read the EDID and display the bytes.  Nothing returned.

void read_edid_ala_libxcm(int busno);

void probe_read_edid(int busno, char * write_mode, char * read_mode);

void test_read_edid_ala_libxcm();

void test_read_edid_for_bus(int busno);

#endif /* I2C_EDID_TESTS_H_ */
