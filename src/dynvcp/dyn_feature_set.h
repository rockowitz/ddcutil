/** @file dyn_feature_set.h
 *
 *  Dyn_Feature_Set wrappers VCP_Feature_Set at the ddc level to incorporate
 *  user supplied feature information in feature metadata.
 */

// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DYN_FEATURE_SET_H_
#define DYN_FEATURE_SET_H_

#include "public/ddcutil_types.h"

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "util/coredefs.h"
/** \endcond */

#include "base/displays.h"
#include "base/feature_set_ref.h"

#include "vcp/vcp_feature_codes.h"
#include "vcp/vcp_feature_set.h"

#include "dynvcp/dyn_feature_codes.h"


#define DYN_FEATURE_SET_MARKER "DSET"
typedef struct {
   char                 marker[4];
   VCP_Feature_Subset   subset;      // subset identifier
   DDCA_Display_Ref     dref;
   GPtrArray *          members_dfm; // array of pointers to Display_Feature_Metadata - alt
} Dyn_Feature_Set;

void
dbgrpt_dyn_feature_set(
      Dyn_Feature_Set *   feature_set,
      bool                verbose,
      int                 depth);

char *
dyn_feature_set_repr_t(
      Dyn_Feature_Set * fset);

Dyn_Feature_Set *
dyn_create_feature_set(
      VCP_Feature_Subset  subset,
      DDCA_Display_Ref    dref,
      Feature_Set_Flags   flags);

#ifdef UNUSED
Dyn_Feature_Set *
dyn_create_single_feature_set_by_hexid2(
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_Display_Ref       dref,
      bool                   force);
#endif

Display_Feature_Metadata *
dyn_get_feature_set_entry(
      Dyn_Feature_Set * feature_set,
      unsigned          index);

int
dyn_get_feature_set_size(
      Dyn_Feature_Set * feature_set);

#ifdef UNUSED
Dyn_Feature_Set *
dyn_create_feature_set_from_feature_set_ref2(
   Feature_Set_Ref *       fsref,
   // DDCA_MCCS_Version_Spec  vcp_version,
   DDCA_Display_Ref        dref,
   Feature_Set_Flags       flags);
#endif

typedef bool (*Dyn_Feature_Set_Filter_Func)(Display_Feature_Metadata * p_metadata);

void
filter_feature_set(
      Dyn_Feature_Set*   feature_set,
      Dyn_Feature_Set_Filter_Func func);

void dyn_free_feature_set(
      Dyn_Feature_Set *  feature_set);

#endif /* DYN_FEATURE_SET_H_ */
