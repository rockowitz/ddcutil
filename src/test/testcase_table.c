/* testcase_table.c
 *
 * <copyright>
 * Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "config.h"

#include "testcase_table.h"

Testcase_Descriptor testcase_catalog[] = {
 //   {"get_luminosity_sample_code",        DisplayRefBus,  NULL, get_luminosity_sample_code, NULL, NULL},
 //     {"demo_p2411_problem",                DisplayRefBus,  NULL, demo_p2411_problem, NULL, NULL}
};
int testcase_catalog_ct = sizeof(testcase_catalog)/sizeof(Testcase_Descriptor);

