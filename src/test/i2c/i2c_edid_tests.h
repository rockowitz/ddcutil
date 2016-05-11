/* i2c_edid_tests.h
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
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
