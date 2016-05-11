/* testcase_table.h
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

#ifndef TESTCASE_TABLE_H_
#define TESTCASE_TABLE_H_

#include "base/displays.h"

// type of display reference required/supported by the command
typedef enum {DisplayRefNone, DisplayRefAny, DisplayRefBus, DisplayRefAdl} DisplayRefType;

typedef void (*NoArgFunction)();
typedef void (*BusArgFunction)(int busno);
typedef void (*AdlArgFunction)(int iAdapterIndex, int iDisplayIndex);
typedef void (*DisplayRefArgFunction)(Display_Ref * dref);

typedef
struct {
   char *                name;      // testcase description
   DisplayRefType        drefType;
   // should really be a union
   NoArgFunction         fp_noarg;
   BusArgFunction        fp_bus;
   AdlArgFunction        fp_adl;
   DisplayRefArgFunction fp_dr;
} Testcase_Descriptor;

extern Testcase_Descriptor testcase_catalog[];
extern int testcase_catalog_ct;

// Testcase_Descriptor ** get_testcase_catalog();
// int get_testcase_catalog_ct();

#endif /* TESTCASE_TABLE_H_ */
