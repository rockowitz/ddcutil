/* vcp_feature_groups.h
 *
 * Created on: Dec 29, 2015
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef SRC_DDC_VCP_FEATURE_SET_H_
#define SRC_DDC_VCP_FEATURE_SET_H_

#include <glib.h>
#include <stdbool.h>

#include "util/coredefs.h"

#include "base/ddc_base_defs.h"

#include "ddc/vcp_feature_codes.h"

// #include "ddc/ddc_services.h"      // TEMP, circular,  VCP_Feature_Subset defined here



typedef void * VCP_Feature_Set;




VCP_Feature_Set create_feature_set(VCP_Feature_Subset subset, Version_Spec vcp_version);
VCP_Feature_Set create_single_feature_set_by_vcp_entry(VCP_Feature_Table_Entry * vcp_entry);
VCP_Feature_Set create_single_feature_set_by_hexid(Byte id, bool force);
VCP_Feature_Set create_single_feature_set_by_charid(Byte id, bool force);
VCP_Feature_Table_Entry * get_feature_set_entry(VCP_Feature_Set feature_set, int index);
int get_feature_set_size(VCP_Feature_Set feature_set);
VCP_Feature_Subset get_feature_set_subset_id(VCP_Feature_Set feature_set);

void report_feature_set(VCP_Feature_Set feature_set, int depth);

typedef struct {
   VCP_Feature_Subset  subset;
   Byte                specific_feature;
} Feature_Set_Ref;


VCP_Feature_Set
create_feature_set_from_feature_set_ref(
   Feature_Set_Ref * fsref,
   Version_Spec      vcp_version,
   bool              force);




#endif /* SRC_DDC_VCP_FEATURE_SET_H_ */
