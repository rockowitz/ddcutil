/* adl_tests.h
 *
 * Created on: Jul 27, 2014
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef ADL_TESTS_H_
#define ADL_TESTS_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LINUX
#include <adl_sdk.h>


void adl_testmain();

void diddle_adl_brightness(int iAdapterIndex, int iDisplayIndex);

void exercise_ad_calls(int iAdapterIndex, int iDisplayIndex);

void run_adapter_display_tests();

#endif /* ADL_TESTS_H_ */
