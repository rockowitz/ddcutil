/* ddc_vcp_tests.h
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

#ifndef DDC_VCP_TESTS_H_
#define DDC_VCP_TESTS_H_

#include "test/ddc/ddc_vcp_tests.h"

void get_luminosity_sample_code(int busno);
void demo_nvidia_bug_sample_code(int busno);
void get_luminosity_using_single_ioctl(int busno);
void probe_get_luminosity(int busno, char * write_mode, char * read_mode);

void test_get_luminosity_for_bus(int busno);

void demo_p2411_problem(int busno);

#endif /* DDC_VCP_TESTS_H_ */
