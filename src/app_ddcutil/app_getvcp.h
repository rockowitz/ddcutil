/** \file app_getvcp.h
 */

 //Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
 // SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_GETVCP_H_
#define APP_GETVCP_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

#include "util/error_info.h"
#include "base/displays.h"
#include "base/feature_sets.h"
#include "base/status_code_mgt.h"

Public_Status_Code
app_show_vcp_subset_values_by_dh(
      Display_Handle *      dh,
      VCP_Feature_Subset    subset,
      Feature_Set_Flags     flags,
      Byte_Bit_Flags        features_seen);

Public_Status_Code
app_show_feature_set_values_by_dh(
      Display_Handle *      dh,
      Feature_Set_Ref *     fsref,
      Feature_Set_Flags     flags);

void
app_read_changes_forever(
      Display_Handle *      dh,
      bool                  force_no_fifo);

#endif /* APP_GETVCP_H_ */
