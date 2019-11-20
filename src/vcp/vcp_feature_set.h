/** @file vcp_feature_set.h
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef VCP_FEATURE_SET_H_
#define VCP_FEATURE_SET_H_

#include "public/ddcutil_types.h"

/** \cond */
#include <glib.h>
#include <stdbool.h>

#include "util/coredefs.h"
/** \endcond */

#include "base/feature_sets.h"

#include "vcp/vcp_feature_codes.h"


#define VCP_FEATURE_SET_MARKER "FSET"
typedef struct vcp_feature_set {
   char                marker[4];
   VCP_Feature_Subset  subset;      // subset identifier
   GPtrArray *         members;     // array of pointers to VCP_Feature_Table_Entry
} VCP_Feature_Set;

typedef bool (*VCP_Feature_Set_Filter_Func)(VCP_Feature_Table_Entry * ventry);

void free_vcp_feature_set(VCP_Feature_Set * fset);

VCP_Feature_Set *
create_feature_set(
      VCP_Feature_Subset     subset,
      DDCA_MCCS_Version_Spec vcp_version,
      Feature_Set_Flags      flags);

#ifdef OLD
VCP_Feature_Set *
create_feature_set0(
      VCP_Feature_Subset   subset_id,
      GPtrArray *          members);
#endif


VCP_Feature_Set *
create_single_feature_set_by_vcp_entry(VCP_Feature_Table_Entry * vcp_entry);

VCP_Feature_Set *
create_single_feature_set_by_hexid(Byte id, bool force);

VCP_Feature_Table_Entry *
get_feature_set_entry(VCP_Feature_Set * feature_set, unsigned index);

void replace_feature_set_entry(
      VCP_Feature_Set *   feature_set,
      unsigned          index,
      VCP_Feature_Table_Entry * new_entry);

int
get_feature_set_size(VCP_Feature_Set * feature_set);

VCP_Feature_Subset
get_feature_set_subset_id(VCP_Feature_Set * feature_set);

void report_feature_set(VCP_Feature_Set * feature_set, int depth);
void dbgrpt_feature_set(VCP_Feature_Set * feature_set, int depth);


VCP_Feature_Set *
create_feature_set_from_feature_set_ref(
   Feature_Set_Ref *       fsref,
   DDCA_MCCS_Version_Spec  vcp_version,
   Feature_Set_Flags       flags);
 //  bool                    force);

void filter_feature_set(VCP_Feature_Set * fset, VCP_Feature_Set_Filter_Func func);

DDCA_Feature_List feature_list_from_feature_set(VCP_Feature_Set * feature_set);

#endif /* VCP_FEATURE_SET_H_ */
