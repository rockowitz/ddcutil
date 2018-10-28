/** @file dyn_feature_set.h
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DYN_FEATURE_SET_H_
#define DYN_FEATURE_SET_H_

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

#include "dynvcp/dyn_feature_codes.h"


#define DYN_FEATURE_SET_MARKER "DSET"
typedef struct {
   char                marker[4];
   VCP_Feature_Subset  subset;      // subset identifier
   DDCA_Display_Ref    dref;
   GPtrArray *         members;     // array of pointers to Internal_Feature_Metadata
} Dyn_Feature_Set;

void dbgrpt_dyn_feature_set(Dyn_Feature_Set * fset, int depth);


VCP_Feature_Set
dyn_create_feature_set(
      VCP_Feature_Subset     subset,
      DDCA_Display_Ref       dref,
      // DDCA_MCCS_Version_Spec vcp_version,
      Feature_Set_Flags      flags);
   // bool                   exclude_table_features);

Dyn_Feature_Set *
dyn_create_feature_set2(
      VCP_Feature_Subset     subset,
      DDCA_Display_Ref       dref,
      Feature_Set_Flags      flags);

// VCP_Feature_Set
// ddc_create_single_feature_set_by_vcp_entry(VCP_Feature_Table_Entry * vcp_entry);

VCP_Feature_Set
dyn_create_single_feature_set_by_hexid(Byte id, DDCA_Display_Ref dref, bool force);

Dyn_Feature_Set *
dyn_create_single_feature_set_by_hexid2(
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_Display_Ref       dref,
      bool                   force);

// VCP_Feature_Table_Entry *
// get_feature_set_entry(VCP_Feature_Set feature_set, unsigned index);

Internal_Feature_Metadata *
dyn_get_feature_set_entry2(
      Dyn_Feature_Set * feature_set,
      unsigned          index);

// int
// get_feature_set_size(VCP_Feature_Set feature_set);

int
dyn_get_feature_set_size2(
      Dyn_Feature_Set * feature_set);

// VCP_Feature_Subset
// get_feature_set_subset_id(VCP_Feature_Set feature_set);

// void report_feature_set(VCP_Feature_Set feature_set, int depth);

VCP_Feature_Set
dyn_create_feature_set_from_feature_set_ref(
   Feature_Set_Ref *       fsref,
   // DDCA_MCCS_Version_Spec  vcp_version,
   DDCA_Display_Ref        dref,
   Feature_Set_Flags       flags);
 //  bool                    force);

Dyn_Feature_Set *
dyn_create_feature_set_from_feature_set_ref2(
   Feature_Set_Ref *       fsref,
   // DDCA_MCCS_Version_Spec  vcp_version,
   DDCA_Display_Ref        dref,
   Feature_Set_Flags       flags);

typedef bool (*Dyn_Feature_Set_Filter_Func)(Internal_Feature_Metadata * p_metadata);

void filter_feature_set2(Dyn_Feature_Set* fset, Dyn_Feature_Set_Filter_Func func);

DDCA_Feature_List feature_list_from_feature_set2(Dyn_Feature_Set * feature_set);

void dyn_free_feature_set(
      Dyn_Feature_Set * feature_set);


#endif /* DYN_FEATURE_SET_H_ */
