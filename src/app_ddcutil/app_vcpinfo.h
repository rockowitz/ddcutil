/** \file app_vcpinfo.h
 *
 *  vcpinfo and (deprecated) listvcp commands
 */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_VCPINFO_H_
#define APP_VCPINFO_H_

#include <config.h>

#include "ddcutil_types.h"

#include "vcp/vcp_feature_codes.h"
#include "vcp/vcp_feature_set.h"


void
app_listvcp(
      FILE * fh);

void
report_vcp_feature_table_entry(
      VCP_Feature_Table_Entry * vfte,
      int                       depth);

bool
app_vcpinfo(
      Feature_Set_Ref *       fref,
      DDCA_MCCS_Version_Spec  vspec,
      Feature_Set_Flags       fsflags);

#endif /* APP_VCPINFO_H_ */
