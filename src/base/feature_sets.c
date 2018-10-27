/* \file feature_sets.c
 *
 * Feature set identifiers
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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

Value_Name_Table vcp_subset_table = {
      // ddcutil defined groups
      VNT(VCP_SUBSET_PROFILE,         "PROFILE"),
      VNT(VCP_SUBSET_COLOR,           "COLOR"),
      VNT(VCP_SUBSET_LUT,             "LUT"),

      // MCCS defined groups
      VNT(VCP_SUBSET_CRT,             "CRT"),
      VNT(VCP_SUBSET_TV,              "TV"),
      VNT(VCP_SUBSET_AUDIO,           "AUDIO"),
      VNT(VCP_SUBSET_WINDOW,          "WINDOW"),
      VNT(VCP_SUBSET_DPVL,            "DPVL"),
      VNT(VCP_SUBSET_PRESET,          "PRESET"),

      // by feature type
      VNT(VCP_SUBSET_TABLE,           "TABLE"),
      VNT(VCP_SUBSET_SCONT,           "SCONT"),
      VNT(VCP_SUBSET_CCONT,           "CCONT"),
      VNT(VCP_SUBSET_CONT,            "CONT"),
      VNT(VCP_SUBSET_SNC,             "SNC"),
      VNT(VCP_SUBSET_CNC,             "CNC"),
      VNT(VCP_SUBSET_NC,              "NC"),
      VNT(VCP_SUBSET_NC_WO,           "NC_WO"),
      VNT(VCP_SUBSET_NC_CONT,         "NC_CONT"),

      // special handling
      VNT(VCP_SUBSET_SCAN,            "SCAN"),
//    VNT(VCP_SUBSET_ALL,             NULL),
//    VNT(VCP_SUBSET_SUPPORTED,       "SUPPORTED"),
      VNT(VCP_SUBSET_KNOWN,           "KNOWN"),

      VNT(VCP_SUBSET_MFG,             "MFG"),

      VNT(VCP_SUBSET_DYNAMIC,         "DYNAMIC"),
      VNT(VCP_SUBSET_SINGLE_FEATURE,  NULL),
      VNT(VCP_SUBSET_NONE,            NULL),
      VNT_END
};

// const int vcp_subset_count3 = (sizeof(vcp_subset_desc)/sizeof(Value_Name_Title) ) - 1;
const int vcp_subset_count = ARRAY_SIZE(vcp_subset_table) - 1;

char * feature_subset_name(VCP_Feature_Subset subset_id) {
   return vnt_name(vcp_subset_table, subset_id);
}

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


static
Value_Name_Title_Table feature_set_flag_table = {
      VNT(FSF_FORCE,            "force"),
      VNT(FSF_SHOW_UNSUPPORTED, "report unsupported features"),
      VNT(FSF_NOTABLE,          "do not report table features"),
      VNT(FSF_RW_ONLY,          "include only RW features"),
      VNT(FSF_RO_ONLY,          "include only RO features"),
      VNT(FSF_WO_ONLY,          "include only WO features"),
      VNT_END
};
const int feature_set_flag_ct = ARRAY_SIZE(feature_set_flag_table)-1;

char * feature_set_flag_names(Feature_Set_Flags flags) {
   return vnt_interpret_flags(
             flags,
             feature_set_flag_table,
             false,                      // use value name, not description
             "|");                      // sepstr
}

