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

      VNT(VCP_SUBSET_DYNAMIC,         "UDF"),
      VNT(VCP_SUBSET_SINGLE_FEATURE,  NULL),
      VNT(VCP_SUBSET_NONE,            NULL),
      VNT_END
};
const int vcp_subset_count = ARRAY_SIZE(vcp_subset_table) - 1;


/** Given a #VCP_Feature_Subset id, return its symbolic name.
 *
 *  @param subset_id
 *  @param subset symbolic name, do not free
 */
char * feature_subset_name(VCP_Feature_Subset subset_id) {
   return vnt_name(vcp_subset_table, subset_id);
}


/** Returns a comma separated list of external subset names for a set of subset ids.
 *  The returned value is valid until the next call to this
 *  function in the current thread.
 *
 *  @param subset_ids
 *  @return comma delimited list, do not free
 */
char * feature_subset_names(VCP_Feature_Subset subset_ids) {
   GString * buf = g_string_sized_new(100);

   bool first = true;
   for(int kk=0; kk < vcp_subset_count; kk++) {
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


/** Outputs a debug report of #Feature_Set_Ref instance
 *
 *  @param  fsref  feature set reference
 *  @param  depth  logical indentation depth
 */
void dbgrpt_feature_set_ref(Feature_Set_Ref * fsref, int depth) {
   rpt_vstring(depth, "subset: %s (%d)",  feature_subset_name(fsref->subset), fsref->subset);
   rpt_vstring(depth, "specific_feature:  0x%02x", fsref->specific_feature);
}


/** Returns a representation of #Feature_Set_Ref.
 *  The returned value is valid until the next call to this function in the current thread.
 *
 *  @param  fsref  feature set reference
 *  @return string description, do not free
 */
char * fsref_repr_t(Feature_Set_Ref * fsref) {
   static GPrivate  fsref_repr_key = G_PRIVATE_INIT(g_free);

   char * buf = get_thread_fixed_buffer(&fsref_repr_key, 100);
   if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE)
      snprintf(buf, 100, "[VCP_SUBSET_SINGLE_FEATURE, 0x%02x]", fsref->specific_feature);
   else
      snprintf(buf, 100, "[%s]",  feature_subset_name(fsref->subset));
   return buf;
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


/* Returns a string representation containing the symbolic names of the
 * flags in a #Feature_Set_Flags instance.
 * The returned value is valid until the next call to this function in the current thread.
 *
 * @param  flags  feature set flags
 * @return string description, do not free
 */
char * feature_set_flag_names_t(Feature_Set_Flags flags) {
   static GPrivate  feature_set_flag_names_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&feature_set_flag_names_key, 100);
   char * s = vnt_interpret_flags(
             flags,
             feature_set_flag_table,
             false,                      // use value name, not description
             "|");                      // sepstr
   g_strlcpy(buf, s, 100);
   free(s);
   return buf;
}

