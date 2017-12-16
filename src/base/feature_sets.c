/* features_sets.c
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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
 *  Feature set identifiers
 */

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <stddef.h>

#include "util/data_structures.h"
#include "util/report_util.h"
#include "util/string_util.h"
/** \endcond */

#include "base/core.h"

#include "base/feature_sets.h"


//
// VCP_Feature_Subset utilities
//

#ifdef OLD
Vcp_Subset_Desc vcp_subset_desc_old[] = {
      {VCP_SUBSET_PROFILE,         "VCP_SUBSET_PROFILE",     "PROFILE"},
      {VCP_SUBSET_COLOR,           "VCP_SUBSET_COLOR",       "COLOR"},
      {VCP_SUBSET_LUT,             "VCP_SUBSET_LUT",         "LUT"},
      {VCP_SUBSET_CRT,             "VCP_SUBSET_CRT",         "CRT"},
      {VCP_SUBSET_TV,              "VCP_SUBSET_TV",          "TV"},
      {VCP_SUBSET_AUDIO,           "VCP_SUBSET_AUDIO",       "AUDIO"},
      {VCP_SUBSET_WINDOW,          "VCP_SUBSET_WINDOW",      "WINDOW"},
      {VCP_SUBSET_DPVL,            "VCP_SUBSET_DPVL",        "DPVL"},
      {VCP_SUBSET_SCAN,            "VCP_SUBSET_SCAN",        "SCAN"},
      {VCP_SUBSET_ALL,             "VCP_SUBSET_ALL",         NULL},
      {VCP_SUBSET_SUPPORTED,       "VCP_SUBSET_SUPPORTED",   "SUPPORTED"},
      {VCP_SUBSET_KNOWN,           "VCP_SUBSET_KNOWN",       "KNOWN"},
      {VCP_SUBSET_PRESET,          "VCP_SUBSET_PRESET",      "PRESET"},
      {VCP_SUBSET_MFG,             "VCP_SUBSET_MFG",         "MFG"},
      {VCP_SUBSET_TABLE,           "VCP_SUBSET_TABLE",       "TABLE"},
      {VCP_SUBSET_SINGLE_FEATURE,  "VCP_SUBSET_SINGLE_FEATURE", NULL},
      {VCP_SUBSET_NONE ,           "VCP_SUBSET_NONE",        NULL},
};
const int vcp_subset_count_old = sizeof(vcp_subset_desc_old)/sizeof(struct _Vcp_Subset_Desc);
#endif


Value_Name_Table vcp_subset_table = {
      VNT(VCP_SUBSET_PROFILE,        "PROFILE"),
      VNT(VCP_SUBSET_COLOR,           "COLOR"),
      VNT(VCP_SUBSET_LUT,             "LUT"),
      VNT(VCP_SUBSET_CRT,             "CRT"),
      VNT(VCP_SUBSET_TV,              "TV"),
      VNT(VCP_SUBSET_AUDIO,           "AUDIO"),
      VNT(VCP_SUBSET_WINDOW,          "WINDOW"),
      VNT(VCP_SUBSET_DPVL,            "DPVL"),
      VNT(VCP_SUBSET_SCAN,            "SCAN"),
      VNT(VCP_SUBSET_ALL,             NULL),
      VNT(VCP_SUBSET_SUPPORTED,       "SUPPORTED"),
      VNT(VCP_SUBSET_KNOWN,           "KNOWN"),
      VNT(VCP_SUBSET_PRESET,          "PRESET"),
      VNT(VCP_SUBSET_MFG,             "MFG"),
      VNT(VCP_SUBSET_TABLE,           "TABLE"),
      VNT(VCP_SUBSET_SINGLE_FEATURE,   NULL),
      VNT(VCP_SUBSET_NONE,             NULL),
      VNT_END
};

// const int vcp_subset_count3 = (sizeof(vcp_subset_desc)/sizeof(Value_Name_Title) ) - 1;
const int vcp_subset_count = ARRAY_SIZE(vcp_subset_table) - 1;

#ifdef OLD
static struct _Vcp_Subset_Desc * find_subset_desc(VCP_Feature_Subset subset_id) {
   struct _Vcp_Subset_Desc * result = NULL;
   int ndx = 0;
   for(; ndx<vcp_subset_count_old; ndx++) {
     if (subset_id == vcp_subset_desc_old[ndx].subset_id) {
        result = &vcp_subset_desc_old[ndx];
        break;
     }
   }
   assert(result);
   return result;
}
#endif

char * feature_subset_name(VCP_Feature_Subset subset_id) {
   return vnt_name(vcp_subset_table, subset_id);
}

#ifdef OLD
char * feature_subset_name_old(VCP_Feature_Subset subset_id) {
   struct _Vcp_Subset_Desc * desc = find_subset_desc(subset_id);
   char * result =  desc->subset_id_name;
   return result;
}
#endif


char * feature_subset_names(VCP_Feature_Subset subset_ids) {
   GString * buf = g_string_sized_new(100);

   int kk = 0;
   bool first = true;
   for(;kk < vcp_subset_count; kk++) {
      Value_Name_Title cur_desc = vcp_subset_table[kk];
      if (subset_ids & cur_desc.value) {
         if (first)
            first = false;
         else
            g_string_append(buf, ", ");

         g_string_append(buf, (cur_desc.title) ? cur_desc.title : cur_desc.name);
      }
   }

   char * result = buf->str;
   g_string_free(buf, false);
   return result;
}


#ifdef REFERENCE
   typedef struct {
      VCP_Feature_Subset  subset;
      Byte                specific_feature;
   } Feature_Set_Ref;
#endif

void dbgrpt_feature_set_ref(Feature_Set_Ref * fsref, int depth) {
   rpt_vstring(depth, "subset: %s (%d)",  feature_subset_name(fsref->subset), fsref->subset);
   rpt_vstring(depth, "specific_feature:  0x%02x", fsref->specific_feature);
}

