/* ddc_output.h
 *
 * Created on: Nov 15, 2015
 *     Author: rock
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

#ifndef DDC_OUTPUT_H_
#define DDC_OUTPUT_H_

#include <glib.h>
// #include <stdio.h>
// #include <time.h>

#include "base/common.h"
// #include "base/ddc_base_defs.h"     // for Version_Spec
#include "base/displays.h"
#include "base/status_code_mgt.h"

#include "ddc/vcp_feature_codes.h"
#include "ddc/vcp_feature_set.h"



#ifdef FUTURE
// not currently used
Global_Status_Code
check_valid_operation_by_feature_rec_and_version(
      VCP_Feature_Table_Entry * feature_rec,
      Version_Spec              vcp_version,
      Version_Feature_Flags     operation_flags);

// not currently used
Global_Status_Code
check_valid_operation_by_feature_id_and_dh(
      Byte                  feature_id,
      Display_Handle *      dh,
      Version_Feature_Flags operation_flags);
#endif

#ifdef OLD
void show_value_for_feature_table_entry_by_display_handle(
        Display_Handle *           dh,
        VCP_Feature_Table_Entry *  vcp_entry,
        GPtrArray *                collector,   // where to write output
        bool                       suppress_unsupported
        // Output_Sink                data_sink,
        // Output_Sink                msg_sink
       );
#endif

Global_Status_Code
get_formatted_value_for_feature_table_entry(
      Display_Handle *           dh,
      VCP_Feature_Table_Entry *  vcp_entry,
      bool                       suppress_unsupported,
      bool                       prefix_value_with_feature_code,
      char **                    pformatted_value,
      FILE *                     msg_fh);


void show_vcp_values_by_display_handle(
        Display_Handle *    dh,
        VCP_Feature_Subset  subset,
        GPtrArray *         collector);

char * format_timestamp(time_t time_millis, char * buf, int bufsz);

GPtrArray * collect_profile_related_values_by_display_handle(Display_Handle * dh, time_t timestamp_millis);


#endif /* DDC_OUTPUT_H_ */
