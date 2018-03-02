/* vcp_feature_set.h
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

/** \f
 *
 */

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


typedef void * VCP_Feature_Set;  // make underlying data structure opaque

typedef bool (*VCP_Feature_Set_Filter_Func)(VCP_Feature_Table_Entry * ventry);

void free_vcp_feature_set(VCP_Feature_Set fset);

VCP_Feature_Set
create_feature_set(
      VCP_Feature_Subset     subset,
      DDCA_MCCS_Version_Spec vcp_version,
      Feature_Set_Flags      flags);
   // bool                   exclude_table_features);

VCP_Feature_Set
create_single_feature_set_by_vcp_entry(VCP_Feature_Table_Entry * vcp_entry);

VCP_Feature_Set
create_single_feature_set_by_hexid(Byte id, bool force);

VCP_Feature_Table_Entry *
get_feature_set_entry(VCP_Feature_Set feature_set, unsigned index);

int
get_feature_set_size(VCP_Feature_Set feature_set);

VCP_Feature_Subset
get_feature_set_subset_id(VCP_Feature_Set feature_set);

void report_feature_set(VCP_Feature_Set feature_set, int depth);

VCP_Feature_Set
create_feature_set_from_feature_set_ref(
   Feature_Set_Ref *       fsref,
   DDCA_MCCS_Version_Spec  vcp_version,
   bool                    force);

void filter_feature_set(VCP_Feature_Set fset, VCP_Feature_Set_Filter_Func func);

DDCA_Feature_List feature_list_from_feature_set(VCP_Feature_Set feature_set);

#endif /* VCP_FEATURE_SET_H_ */
