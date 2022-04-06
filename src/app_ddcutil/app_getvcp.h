/** @file app_getvcp.h
 *  Implement the GETVCP command
 */

 //Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
 // SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_GETVCP_H_
#define APP_GETVCP_H_

#include "public/ddcutil_types.h"

/** \cond */
#include <stdbool.h>

#include "util/data_structures.h"
#include "util/error_info.h"
#include "util/string_util.h"
#include "util/report_util.h"
/** \endcond */

#include "base/displays.h"
#include "base/feature_set_ref.h"
#include "base/status_code_mgt.h"

#include "cmdline/parsed_cmd.h"

Status_Errno_DDC
app_show_single_vcp_value_by_feature_id(
      Display_Handle *      dh,
      DDCA_Vcp_Feature_Code feature_id,
      bool                  force);

Status_Errno_DDC
app_show_vcp_subset_values_by_dh(
      Display_Handle *      dh,
      VCP_Feature_Subset    subset,
      Feature_Set_Flags     flags,
      Byte_Bit_Flags        features_seen);

Status_Errno_DDC
app_show_feature_set_values_by_dh(
      Display_Handle *      dh,
      Parsed_Cmd *          parsed_cmd);

#endif /* APP_GETVCP_H_ */
