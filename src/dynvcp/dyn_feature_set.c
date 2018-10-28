/** @file dyn_feature_set.c
 */

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "base/displays.h"
#include "base/feature_sets.h"

#include "dynvcp/dyn_feature_set.h"

static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_UDF;

VCP_Feature_Set
dyn_create_feature_set(
      VCP_Feature_Subset     subset_id,
      DDCA_Display_Ref       dref,
     // DDCA_MCCS_Version_Spec vcp_version,
      Feature_Set_Flags      flags)
   // bool                   exclude_table_features)
{
   bool debug = true;
   DBGMSF(debug, "Starting. subset_id=%d - %s, dref=%s, flags=0x%02x - %s",
                 subset_id,
                 feature_subset_name(subset_id),
                 dref_repr_t(dref),
                 flags,
                 feature_set_flag_names(flags));

   VCP_Feature_Set result = NULL;

   Display_Ref * dref2 = (Display_Ref *) dref;
   assert( dref2 && memcmp(dref2->marker, DISPLAY_REF_MARKER, 4) == 0);

   if (subset_id == VCP_SUBSET_DYNAMIC) {  // all user defined features
      if (dref2->dfr) {
#ifdef REF
         typedef enum {
            DFR_FLAGS_NONE      = 0,
            DFR_FLAGS_NOT_FOUND = 1
         } DFR_Flags;


         #define DYNAMIC_FEATURES_REC_MARKER "DFRC"
         typedef struct {
            char                       marker[4];
            char *                     mfg_id;       // [EDID_MFG_ID_FIELD_SIZE];
            char *                     model_name;   // [EDID_MODEL_NAME_FIELD_SIZE];
            uint16_t                   product_code;
            char *                     filename;     // source filename, if applicable
            DDCA_MCCS_Version_Spec     vspec;
            DFR_Flags                  flags;
            GHashTable *               features;     // array of DDCA_Feature_Metadata
         } Dynamic_Features_Rec;
#endif

         GPtrArray * members = g_ptr_array_new();

         GHashTableIter iter;
         gpointer hash_key;
         gpointer hash_value;
         g_hash_table_iter_init(&iter, dref2->dfr->features);
         bool found = g_hash_table_iter_next(&iter, &hash_key, &hash_value);
         while (found) {
            DDCA_Feature_Metadata * feature_metadata = hash_value;
            assert( memcmp(feature_metadata, DDCA_FEATURE_METADATA_MARKER, 4) == 0 );

            VCP_Feature_Table_Entry * pfte =
            vcp_create_dynamic_feature(feature_metadata->feature_code, feature_metadata);
            g_ptr_array_add(members, pfte);

            found = g_hash_table_iter_next(&iter, &hash_key, &hash_value);
         }

         result = create_feature_set0(subset_id, members);

      }
   }
   else {
      // TODO:  insert DFR records if necessary
      result = create_feature_set(subset_id, dref2->vcp_version, flags);
      assert(result);

      // For those features for which user defined metadata exists, replace
      // the feature set entry with one reflecting the user defined metadata
      int ct = get_feature_set_size(result);
      for (int ndx = 0; ndx < ct; ndx++) {
         VCP_Feature_Table_Entry * cur_entry = get_feature_set_entry(result, ndx);
         DDCA_Vcp_Feature_Code feature_code =  cur_entry->code;

         //  GHashTable * feature_hash = dref2->dfr->features;
         DDCA_Feature_Metadata * feature_metadata = get_dynamic_feature_metadata(dref2->dfr, feature_code);
         if (feature_metadata) {
            DBGMSG("Replacing feature set entry for feature 0x%02x with user defined metadata", feature_code);
            dbgrpt_feature_metadata(feature_metadata, 1);
            VCP_Feature_Table_Entry * pfte =
              vcp_create_dynamic_feature(feature_metadata->feature_code, feature_metadata);
            dbgrpt_vcp_entry(pfte, 1);
            replace_feature_set_entry(result, ndx,pfte);
         }
      }
   }

   if (debug) {
      DBGMSG("Returning: %p", result);
      if (result)
         dbgrpt_feature_set(result, 1);
   }
   return result;
}


VCP_Feature_Set
dyn_create_single_feature_set_by_hexid(Byte feature_code, DDCA_Display_Ref dref, bool force) {
   VCP_Feature_Set result = NULL;

    Display_Ref * dref2 = (Display_Ref *) dref;
    assert( dref2 && memcmp(dref2->marker, DISPLAY_REF_MARKER, 4) == 0);
    if (dref2->dfr) {
       DDCA_Feature_Metadata * feature_metadata  =
          get_dynamic_feature_metadata(dref2->dfr, feature_code);
       if (feature_metadata) {
          VCP_Feature_Table_Entry * vcp_entry =
             vcp_create_dynamic_feature(feature_metadata->feature_code, feature_metadata);
          result = create_single_feature_set_by_vcp_entry(vcp_entry);
       }
    }
    if (!result)
       result = create_single_feature_set_by_hexid(feature_code, force);

   return result;
}


VCP_Feature_Set
dyn_create_feature_set_from_feature_set_ref(
   Feature_Set_Ref *       fsref,
   // DDCA_MCCS_Version_Spec  vcp_version,
   DDCA_Display_Ref        dref,
   Feature_Set_Flags       flags)
 //  bool                    force);
{
   bool debug = true;
   DBGTRC(debug, TRACE_GROUP, "Starting. fsref=%s, dref=%s, flags=%s",
          fsref_repr(fsref), dref_repr_t(dref), interpret_ddca_feature_flags(flags));

   VCP_Feature_Set result = NULL;
   if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE) {
      result = dyn_create_single_feature_set_by_hexid(fsref->specific_feature, dref, flags & FSF_FORCE);
   }
   else {
      result = dyn_create_feature_set(fsref->subset, dref, flags);
   }

   if (debug || IS_TRACING()) {
      DBGMSG("Returning VCP_Feature_Set %p",  result);
      if (result)
         dbgrpt_feature_set(result, 1);
   }
   return result;
}

