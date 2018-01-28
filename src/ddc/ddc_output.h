/* ddc_output.h
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include <stdio.h>
#include <time.h>

#include "base/core.h"
#include "base/displays.h"
#include "base/status_code_mgt.h"

#include "vcp/vcp_feature_codes.h"
#include "vcp/vcp_feature_set.h"
#include "vcp/vcp_feature_values.h"

// TODO: Should probably be in a more general location
// Standard printf format strings for reporting feature codes values.
extern const char* FMT_CODE_NAME_DETAIL_W_NL;
extern const char* FMT_CODE_NAME_DETAIL_WO_NL;



#ifdef FUTURE
// not currently used
Public_Status_Code
check_valid_operation_by_feature_rec_and_version(
      VCP_Feature_Table_Entry * feature_rec,
      Version_Spec              vcp_version,
      Version_Feature_Flags     operation_flags);

// not currently used
Public_Status_Code
check_valid_operation_by_feature_id_and_dh(
      Byte                  feature_id,
      Display_Handle *      dh,
      Version_Feature_Flags operation_flags);
#endif


Public_Status_Code
collect_raw_subset_values(
      Display_Handle *    dh,
      VCP_Feature_Subset  subset,
      Vcp_Value_Set       vset,
      bool                ignore_unsupported,
      FILE *              msg_fh);

Public_Status_Code
get_formatted_value_for_feature_table_entry(
      Display_Handle *           dh,
      VCP_Feature_Table_Entry *  vcp_entry,
      bool                       suppress_unsupported,
      bool                       prefix_value_with_feature_code,
      char **                    pformatted_value,
      FILE *                     msg_fh);

Public_Status_Code
show_vcp_values(
      Display_Handle *    dh,
      VCP_Feature_Subset  subset,
      GPtrArray *         collector,
      Feature_Set_Flags   flags,
      Byte_Bit_Flags      features_seen);

#endif /* DDC_OUTPUT_H_ */
