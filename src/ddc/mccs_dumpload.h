/* loadvcp.h
 *
 * Created on: Aug 16, 2014
 *     Author: rock
 *
 * Load/store VCP settings from/to file.
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

#ifndef LOADVCP_H_
#define LOADVCP_H_

#include <stdio.h>

#include <base/status_code_mgt.h>
#include <base/displays.h>

#include "base/vcp_feature_values.h"


// Dumpload_Data is the internal form data structure used to
// hold data being loaded.  Whatever the external form: a
// file or a string, it is converted to Dumpload_Data and then
// written to the monitor.

#define MAX_LOADVCP_VALUES  20





typedef
struct {
   time_t timestamp_millis;
 //  int    busno;
   Byte   edidbytes[128];
   char   edidstr[257];       // 128 byte edid as hex string (for future use)
   char   mfg_id[4];
   char   model[14];
   char   serial_ascii[14];
   int    vcp_value_ct;
#ifdef OLD
   Single_Vcp_Value vcp_value[MAX_LOADVCP_VALUES];
#endif
   // new way:
   Vcp_Value_Set vcp_values;
} Dumpload_Data;

void report_dumpload_data(Dumpload_Data * data, int depth);

// TODO: implement:
// free_dupload_data(Dumpload_Data * pdata);

Global_Status_Code loadvcp_by_dumpload_data(Dumpload_Data* pdata);
Global_Status_Code loadvcp_by_string(char * catenated);

Dumpload_Data* create_dumpload_data_from_g_ptr_array(GPtrArray * garray);
GPtrArray * convert_dumpload_data_to_string_array(Dumpload_Data * data);

Global_Status_Code
dumpvcp_as_dumpload_data(
      Display_Handle * dh,
      Dumpload_Data** pdumpload_data);


Global_Status_Code dumpvcp_as_string(Display_Handle * dh, char** result);

#ifdef OLD
Global_Status_Code dumpvcp_as_string_old(Display_Handle * dh, char** result);
#endif

#endif /* LOADVCP_H_ */
