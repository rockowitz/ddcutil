/* vcp_base.c
 *
 * Created on: Jan 7, 2016
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

// Putting these definitions and functions into subdirectory base
// so that subdirectory cmdline is not dependent on ddc

#include <assert.h>
#include <stddef.h>

#include "util/report_util.h"

#include "base/vcp_feature_base.h"


// Standard format strings for reporting feature codes.
const char* FMT_CODE_NAME_DETAIL_WO_NL = "VCP code 0x%02x (%-30s): %s";
const char* FMT_CODE_NAME_DETAIL_W_NL  = "VCP code 0x%02x (%-30s): %s\n";


//
// VCP_Feature_Subset utilities
//

struct Vcp_Subset_Desc {
   VCP_Feature_Subset     subset_id;
   char *                 subset_id_name;
};

static struct Vcp_Subset_Desc subset_desc[] = {
      {VCP_SUBSET_PROFILE,         "VCP_SUBSET_PROFILE"},
      {VCP_SUBSET_COLOR,           "VCP_SUBSET_COLOR"},
      {VCP_SUBSET_LUT,             "VCP_SUBSET_LUT"},
      {VCP_SUBSET_CRT,             "VCP_SUBSET_CRT"},
      {VCP_SUBSET_TV,              "VCP_SUBSET_TV"},
      {VCP_SUBSET_AUDIO,           "VCP_SUBSET_AUDIO"},
      {VCP_SUBSET_WINDOW,          "VCP_SUBSET_WINDOW"},
      {VCP_SUBSET_DPVL,            "VCP_SUBSET_DPVL"},
      {VCP_SUBSET_SCAN,            "VCP_SUBSET_SCAN"},
      {VCP_SUBSET_ALL,             "VCP_SUBSET_ALL"},
      {VCP_SUBSET_SUPPORTED,       "VCP_SUBSET_SUPPORTED"},
      {VCP_SUBSET_SINGLE_FEATURE,  "VCP_SUBSET_SINGLE_FEATURE"},
      {VCP_SUBSET_NONE ,           "VCP_SUBSET_NONE"},
};
static int vcp_subset_count = sizeof(subset_desc)/sizeof(struct Vcp_Subset_Desc);

static struct Vcp_Subset_Desc * find_subset_desc(VCP_Feature_Subset subset_id) {
   struct Vcp_Subset_Desc * result = NULL;
   int ndx = 0;
   for(; ndx<vcp_subset_count; ndx++) {
     if (subset_id == subset_desc[ndx].subset_id) {
        result = &subset_desc[ndx];
        break;
     }
   }
   assert(result);
   return result;
}

char * feature_subset_name(VCP_Feature_Subset subset_id) {
   struct Vcp_Subset_Desc * desc = find_subset_desc(subset_id);
   return desc->subset_id_name;
}


#ifdef REFERENCE
   typedef struct {
      VCP_Feature_Subset  subset;
      Byte                specific_feature;
   } Feature_Set_Ref;
#endif

void report_feature_set_ref(Feature_Set_Ref * fsref, int depth) {
   rpt_vstring(depth, "subset: %s (%d)",  feature_subset_name(fsref->subset), fsref->subset);
   rpt_vstring(depth, "specific_feature:  0x%02x", fsref->specific_feature);
}
