/** @file ddc_feature_set.h
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_FEATURE_SET_H_
#define DDC_FEATURE_SET_H_

#include "public/ddcutil_types.h"

/** \cond */
#include <assert.h>
#include <glib.h>
#include <stdbool.h>

#include "util/coredefs.h"
/** \endcond */

#include "base/displays.h"
#include "base/feature_sets.h"

#include "vcp/vcp_feature_codes.h"

#include "vcp/vcp_feature_set.h"


VCP_Feature_Set
ddc_create_feature_set(
      VCP_Feature_Subset     subset,
      DDCA_Display_Ref       dref,
      // DDCA_MCCS_Version_Spec vcp_version,
      Feature_Set_Flags      flags);
   // bool                   exclude_table_features);

// VCP_Feature_Set
// ddc_create_single_feature_set_by_vcp_entry(VCP_Feature_Table_Entry * vcp_entry);

VCP_Feature_Set
ddc_create_single_feature_set_by_hexid(Byte id, DDCA_Display_Ref dref, bool force);

// VCP_Feature_Table_Entry *
// get_feature_set_entry(VCP_Feature_Set feature_set, unsigned index);

// int
// get_feature_set_size(VCP_Feature_Set feature_set);

// VCP_Feature_Subset
// get_feature_set_subset_id(VCP_Feature_Set feature_set);

// void report_feature_set(VCP_Feature_Set feature_set, int depth);

VCP_Feature_Set
ddc_create_feature_set_from_feature_set_ref(
   Feature_Set_Ref *       fsref,
   // DDCA_MCCS_Version_Spec  vcp_version,
   DDCA_Display_Ref        dref,
   Feature_Set_Flags       flags);
 //  bool                    force);

// void filter_feature_set(VCP_Feature_Set fset, VCP_Feature_Set_Filter_Func func);

// DDCA_Feature_List feature_list_from_feature_set(VCP_Feature_Set feature_set);


#endif /* DDC_FEATURE_SET_H_ */
