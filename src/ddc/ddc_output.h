/* \file ddc_output.h
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_OUTPUT_H_
#define DDC_OUTPUT_H_

#include <glib-2.0/glib.h>
#include <stdio.h>
#include <time.h>

#include "base/core.h"
#include "base/displays.h"
#include "base/status_code_mgt.h"

#include "vcp/vcp_feature_codes.h"
#include "vcp/vcp_feature_set.h"
#include "vcp/vcp_feature_values.h"

#include "dynvcp/dyn_feature_codes.h"


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
ddc_collect_raw_subset_values(
      Display_Handle *    dh,
      VCP_Feature_Subset  subset,
      Vcp_Value_Set       vset,
      bool                ignore_unsupported,
      FILE *              msg_fh);

Public_Status_Code
ddc_get_formatted_value_for_display_feature_metadata(
      Display_Handle *            dh,
      Display_Feature_Metadata *  dfm,
      bool                        suppress_unsupported,
      bool                        prefix_value_with_feature_code,
      char **                     formatted_value_loc,
      FILE *                      msg_fh);

Public_Status_Code
ddc_show_vcp_values(
      Display_Handle *    dh,
      VCP_Feature_Subset  subset,
      GPtrArray *         collector,
      Feature_Set_Flags   flags,
      Byte_Bit_Flags      features_seen);


void init_ddc_output();
#endif /* DDC_OUTPUT_H_ */
