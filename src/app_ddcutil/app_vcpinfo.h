// app_vcpinfo.h

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 

#ifndef APP_VCPINFO_H_
#define APP_VCPINFO_H_

#include <config.h>

#include "ddcutil_types.h"


char *
vcp_interpret_version_feature_flags(
      DDCA_Version_Feature_Flags flags,
      char *                     buf,
      int                        bufsz);

void
report_vcp_feature_table_entry(
      VCP_Feature_Table_Entry * vfte,
      int                       depth);

void
app_listvcp(FILE * fh);



bool
app_vcpinfo(
      Feature_Set_Ref *      fref,
      DDCA_MCCS_Version_Spec vspec,
      Feature_Set_Flags      fsflags);


#endif /* APP_VCPINFO_H_ */
