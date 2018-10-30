/** @file dyn_feature_set.c
 */

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "util/debug_util.h"
#include "util/report_util.h"

#include "base/displays.h"
#include "base/feature_sets.h"

#include "vcp/vcp_feature_codes.h"
#include "dynvcp/dyn_feature_set.h"

static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_UDF;

#ifdef REF
#define DYN_FEATURE_SET_MARKER "DSET"
typedef struct {
   char                marker[4];
   VCP_Feature_Subset  subset;      // subset identifier
   DDCA_Display_Ref    dref;
   GPtrArray *         members;     // array of pointers to Internal_Feature_Metadata
} Dyn_Feature_Set;
#endif

void dbgrpt_dyn_feature_set(
      Dyn_Feature_Set * fset,
      int               depth)
{
   int d0 = depth;
   int d1 = depth+1;

   rpt_vstring(d0, "Subset: %d (%s)", fset->subset, feature_subset_name(fset->subset));
#ifndef DFM
   rpt_label  (d0, "Members:");
   for (int ndx=0; ndx < fset->members->len; ndx++) {
      Internal_Feature_Metadata * ifm = g_ptr_array_index(fset->members,ndx);
      dbgrpt_internal_feature_metadata(ifm, d1);
   }
#else
   rpt_label  (d0, "Members (dfm):");
   for (int ndx=0; ndx < fset->members_dfm->len; ndx++) {
      Display_Feature_Metadata * dfm = g_ptr_array_index(fset->members_dfm,ndx);
      dbgrpt_display_feature_metadata(dfm, d1);
   }
#endif
}

#ifdef OLD
VCP_Feature_Set
dyn_create_feature_set(
      VCP_Feature_Subset     subset_id,
      DDCA_Display_Ref       dref,
     // DDCA_MCCS_Version_Spec vcp_version,
      Feature_Set_Flags      feature_flags)
   // bool                   exclude_table_features)
{
   bool debug = false;
   DBGMSF(debug, "Starting. subset_id=%d - %s, dref=%s, flags=0x%02x - %s",
                 subset_id,
                 feature_subset_name(subset_id),
                 dref_repr_t(dref),
                 feature_flags,
                 feature_set_flag_names(feature_flags));

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
            DFR_Flags                  feature_flags;
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
      result = create_feature_set(subset_id, dref2->vcp_version, feature_flags);
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
#endif

#ifdef IFM
Internal_Feature_Metadata *
dyn_create_dynamic_feature_from_dfr_metadata(DDCA_Feature_Metadata * dfr_metadata)
{
   bool debug = false;
   DBGMSF(debug, "Starting. id=0x%02x", dfr_metadata->feature_code);
   Internal_Feature_Metadata * ifm = calloc(1, sizeof(Internal_Feature_Metadata));
   ifm->external_metadata = dfr_metadata;

   if (dfr_metadata->feature_flags & DDCA_SIMPLE_NC) {
      if (dfr_metadata->sl_values)
         ifm->vcp_nontable_formatter = dyn_format_feature_detail_sl_lookup;  // HACK
      else
         ifm->nontable_formatter = format_feature_detail_sl_byte;
   }
   else if (dfr_metadata->feature_flags & DDCA_STD_CONT)
      ifm->nontable_formatter = format_feature_detail_standard_continuous;
   else if (dfr_metadata->feature_flags & DDCA_TABLE)
      ifm->table_formatter = default_table_feature_detail_function;
   else
      ifm->nontable_formatter = format_feature_detail_debug_bytes;

   // pentry->vcp_global_flags = DDCA_SYNTHETIC;   // indicates caller should free
   // pentry->vcp_global_flags |= DDCA_USER_DEFINED;
   assert(ifm);
   if (debug || IS_TRACING()) {
      DBGMSF(debug, "Done.  Returning: %p", ifm);
      dbgrpt_internal_feature_metadata(ifm, 1);
   }
   return ifm;
}
#endif

Display_Feature_Metadata *
dyn_create_dynamic_feature_from_dfr_metadata_dfm(DDCA_Feature_Metadata * dfr_metadata)
{
   bool debug = false;
   DBGMSF(debug, "Starting. id=0x%02x", dfr_metadata->feature_code);
   Display_Feature_Metadata * dfm = dfm_from_ddca_feature_metadata(dfr_metadata);

   if (dfr_metadata->feature_flags & DDCA_SIMPLE_NC) {
      if (dfr_metadata->sl_values)
         dfm->nontable_formatter_sl = dyn_format_feature_detail_sl_lookup;  // HACK
      else
         dfm->nontable_formatter = format_feature_detail_sl_byte;
   }
   else if (dfr_metadata->feature_flags & DDCA_STD_CONT)
      dfm->nontable_formatter = format_feature_detail_standard_continuous;
   else if (dfr_metadata->feature_flags & DDCA_TABLE)
      dfm->table_formatter = default_table_feature_detail_function;
   else
      dfm->nontable_formatter = format_feature_detail_debug_bytes;

   // pentry->vcp_global_flags = DDCA_SYNTHETIC;   // indicates caller should free
   // pentry->vcp_global_flags |= DDCA_USER_DEFINED;
   assert(dfm);
   if (debug || IS_TRACING()) {
      DBGMSF(debug, "Done.  Returning: %p", dfm);
      dbgrpt_display_feature_metadata(dfm, 1);
   }
   return dfm;
}

#ifndef DFM
DDCA_Feature_Metadata *
dyn_create_feature_metadata_from_vcp_feature_table_entry(
      VCP_Feature_Table_Entry * pentry, DDCA_MCCS_Version_Spec vspec)
{
   DDCA_Feature_Metadata * meta = calloc(1, sizeof(DDCA_Feature_Metadata));
   memcpy(meta->marker, DDCA_FEATURE_METADATA_MARKER, 4);

   DDCA_Version_Feature_Info *
   info = extract_version_feature_info(pentry, vspec, /*version_sensitive*/ true);

   if (pentry->vcp_global_flags & DDCA_SYNTHETIC)
      free_synthetic_vcp_entry(pentry);

   version_feature_info_to_metadata( info, meta);
   // free info
   return meta;
}
#else
DDCA_Feature_Metadata *
dyn_create_feature_metadata_from_vcp_feature_table_entry(
      VCP_Feature_Table_Entry * pentry, DDCA_MCCS_Version_Spec vspec)
{
   Display_Feature_Metadata * dfm =
     extract_version_feature_info_dfm(pentry, vspec, /*version_sensitive*/ true);

   if (pentry->vcp_global_flags & DDCA_SYNTHETIC)
      free_synthetic_vcp_entry(pentry);

   DDCA_Feature_Metadata * meta = dfm_to_ddca_feature_metadata(dfm);
   // free info
   return meta;
}
#endif

#ifdef IFM
Internal_Feature_Metadata *
dyn_create_dynamic_feature_from_vcp_feature_table_entry(
      VCP_Feature_Table_Entry * vfte, DDCA_MCCS_Version_Spec vspec)
{
   assert(vfte);
   bool debug = false;
   DBGMSF(debug, "Starting. id=0x%02x", vfte->code);
   Internal_Feature_Metadata * ifm = calloc(1, sizeof(Internal_Feature_Metadata));
   DDCA_Feature_Metadata * meta = dyn_create_feature_metadata_from_vcp_feature_table_entry(vfte, vspec);
   ifm->external_metadata = meta;

   if (meta->feature_flags & DDCA_SIMPLE_NC) {
      if (meta->sl_values)
         ifm->vcp_nontable_formatter = dyn_format_feature_detail_sl_lookup;
      else
         ifm->nontable_formatter = format_feature_detail_sl_byte;
   }
   else if (meta->feature_flags & DDCA_STD_CONT)
      ifm->nontable_formatter = format_feature_detail_standard_continuous;
   else if (meta->feature_flags & DDCA_TABLE)
      ifm->table_formatter = default_table_feature_detail_function;
   else
      ifm->nontable_formatter = format_feature_detail_debug_bytes;

   // pentry->vcp_global_flags = DDCA_SYNTHETIC;   // indicates caller should free
   // pentry->vcp_global_flags |= DDCA_USER_DEFINED;
   assert(ifm);
   if (debug || IS_TRACING()) {
      DBGMSF(debug, "Done.  Returning: %p", ifm);
      dbgrpt_internal_feature_metadata(ifm, 1);
   }
   return ifm;
}
#endif


Display_Feature_Metadata *
dyn_create_dynamic_feature_from_vcp_feature_table_entry_dfm(
      VCP_Feature_Table_Entry * vfte, DDCA_MCCS_Version_Spec vspec)
{
   assert(vfte);
   bool debug = false;
   DBGMSF(debug, "Starting. id=0x%02x", vfte->code);
   // Internal_Feature_Metadata * ifm = calloc(1, sizeof(Internal_Feature_Metadata));
   DDCA_Feature_Metadata * meta = dyn_create_feature_metadata_from_vcp_feature_table_entry(vfte, vspec);
   Display_Feature_Metadata * dfm = dfm_from_ddca_feature_metadata(meta);


   if (dfm->feature_flags & DDCA_SIMPLE_NC) {
      if (dfm->sl_values)
         dfm->nontable_formatter_sl = dyn_format_feature_detail_sl_lookup;
      else
         dfm->nontable_formatter = format_feature_detail_sl_byte;
   }
   else if (dfm->feature_flags & DDCA_STD_CONT)
      dfm->nontable_formatter = format_feature_detail_standard_continuous;
   else if (dfm->feature_flags & DDCA_TABLE)
      dfm->table_formatter = default_table_feature_detail_function;
   else
      dfm->nontable_formatter = format_feature_detail_debug_bytes;

   // pentry->vcp_global_flags = DDCA_SYNTHETIC;   // indicates caller should free
   // pentry->vcp_global_flags |= DDCA_USER_DEFINED;
   assert(dfm);
   if (debug || IS_TRACING()) {
      DBGMSF(debug, "Done.  Returning: %p", dfm);
      dbgrpt_display_feature_metadata(dfm, 1);
   }
   return dfm;
}



Dyn_Feature_Set *
dyn_create_feature_set0(
      VCP_Feature_Subset   subset_id,
      GPtrArray *          members,
      GPtrArray *          members_dfm)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. subset_id=%d, number of members=%d",
                              subset_id, (members) ? members->len : -1);

   Dyn_Feature_Set * fset = calloc(1,sizeof(Dyn_Feature_Set));
   memcpy(fset->marker, DYN_FEATURE_SET_MARKER, 4);
   fset->subset = subset_id;
   fset->members = members;
   fset->members_dfm = members_dfm;

   DBGTRC(debug, TRACE_GROUP, "Returning %p", fset);
   return fset;
}

#ifdef IFM
Dyn_Feature_Set *
dyn_create_feature_set2(
      VCP_Feature_Subset     subset_id,
      DDCA_Display_Ref       display_ref,
      Feature_Set_Flags      feature_flags)
{
   Dyn_Feature_Set * result = NULL;
   bool debug = true;
   DBGMSF(debug, "Starting. subset_id=%d - %s, dref=%s, flags=0x%02x - %s",
                  subset_id,
                  feature_subset_name(subset_id),
                  dref_repr_t(display_ref),
                  feature_flags,
                  feature_set_flag_names(feature_flags));

    Display_Ref * dref = (Display_Ref *) display_ref;
    assert( dref && memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);

    GPtrArray * members = g_ptr_array_new();
    GPtrArray * members_dfm = g_ptr_array_new();

    if (subset_id == VCP_SUBSET_DYNAMIC) {  // all user defined features
       DBGMSF(debug, "VCP_SUBSET_DYNAMIC path");

       if (dref->dfr) {
          DBGMSF(debug, "dref->dfr is set");
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
             DFR_Flags                  feature_flags;
             GHashTable *               features;     // array of DDCA_Feature_Metadata
          } Dynamic_Features_Rec;
 #endif

          GHashTableIter iter;
          gpointer hash_key;
          gpointer hash_value;
          g_hash_table_iter_init(&iter, dref->dfr->features);
          bool found = g_hash_table_iter_next(&iter, &hash_key, &hash_value);
          while (found) {
             DDCA_Feature_Metadata * feature_metadata = hash_value;
             assert( memcmp(feature_metadata, DDCA_FEATURE_METADATA_MARKER, 4) == 0 );

             Internal_Feature_Metadata * ifm =
                dyn_create_dynamic_feature_from_dfr_metadata(feature_metadata);
             g_ptr_array_add(members, ifm);


//             Display_Feature_Metadata * dfm =
//                dyn_create_dynamic_feature_from_dfr_metadata_dfm(feature_metadata);
//             g_ptr_array_add(members_dfm, dfm);

             found = g_hash_table_iter_next(&iter, &hash_key, &hash_value);
          }
       }   // if (dref->dfr)

    }
    else {   // (subset_id != VCP_SUBSET_DYNAMIC
       // TODO:  insert DFR records if necessary
       result = create_feature_set(subset_id, dref->vcp_version, feature_flags);
       assert(result);

       // For those features for which user defined metadata exists, replace
       // the feature set entry with one reflecting the user defined metadata
       int ct = get_feature_set_size(result);
       for (int ndx = 0; ndx < ct; ndx++) {
          VCP_Feature_Table_Entry * cur_entry = get_feature_set_entry(result, ndx);
          DDCA_Vcp_Feature_Code feature_code =  cur_entry->code;

          //  GHashTable * feature_hash = dref->dfr->features;
          DDCA_Feature_Metadata * feature_metadata = get_dynamic_feature_metadata(dref->dfr, feature_code);
          if (feature_metadata) {
             Internal_Feature_Metadata * ifm =
                dyn_create_dynamic_feature_from_dfr_metadata(feature_metadata);
             g_ptr_array_add(members, ifm);
//
//             Display_Feature_Metadata * dfm =
//                dyn_create_dynamic_feature_from_dfr_metadata_dfm(feature_metadata);
//             g_ptr_array_add(members_dfm, dfm);

          }
          else {
             // create Internal_Feature_Metadata from VCP_Feature_Table_Entry
             Internal_Feature_Metadata * ifm =
                   dyn_create_dynamic_feature_from_vcp_feature_table_entry(cur_entry, dref->vcp_version);
             g_ptr_array_add(members, ifm);

//             Display_Feature_Metadata * dfm =
//                   dyn_create_dynamic_feature_from_vcp_feature_table_entry_dfm(cur_entry, dref->vcp_version);
//             g_ptr_array_add(members_dfm, dfm);

          }
       }
    }

    result = dyn_create_feature_set0(subset_id, members, members_dfm);

    if (debug) {
       DBGMSG("Returning: %p", result);
       if (result)
          dbgrpt_dyn_feature_set(result, 1);
    }
    return result;
}
#endif


Dyn_Feature_Set *
dyn_create_feature_set2_dfm(
      VCP_Feature_Subset     subset_id,
      DDCA_Display_Ref       display_ref,
      Feature_Set_Flags      feature_set_flags)
{
   bool debug = false;
   DBGMSF(debug, "Starting. subset_id=%d - %s, dref=%s, feature_setflags=0x%02x - %s",
                  subset_id,
                  feature_subset_name(subset_id),
                  dref_repr_t(display_ref),
                  feature_set_flags,
                  feature_set_flag_names(feature_set_flags));

    Dyn_Feature_Set * result = NULL;
    Display_Ref * dref = (Display_Ref *) display_ref;
    assert( dref && memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);

    GPtrArray * members = g_ptr_array_new();
    GPtrArray * members_dfm = g_ptr_array_new();

    if (subset_id == VCP_SUBSET_DYNAMIC) {  // all user defined features
       DBGMSF(debug, "VCP_SUBSET_DYNAMIC path");

       if (dref->dfr) {
          DBGMSF(debug, "dref->dfr is set");
          GHashTableIter iter;
          gpointer hash_key;
          gpointer hash_value;
          g_hash_table_iter_init(&iter, dref->dfr->features);
          bool found = g_hash_table_iter_next(&iter, &hash_key, &hash_value);
          while (found) {
             DDCA_Feature_Metadata * feature_metadata = hash_value;
             assert( memcmp(feature_metadata, DDCA_FEATURE_METADATA_MARKER, 4) == 0 );

             // Test Feature_Set_Flags other than FSF_SHOW_UNSUPPORTED, FSF_FORCE,
             // which do not apply in this context
             bool include = true;
             if ( ((feature_set_flags & FSF_NOTABLE) &&  (feature_metadata->feature_flags & DDCA_TABLE)) ||
                  ((feature_set_flags & FSF_RO_ONLY) && !(feature_metadata->feature_flags & DDCA_RO)   ) ||
                  ((feature_set_flags & FSF_RW_ONLY) && !(feature_metadata->feature_flags & DDCA_RW)   ) ||
                  ((feature_set_flags & FSF_WO_ONLY) && !(feature_metadata->feature_flags & DDCA_WO)   )
                )
                include = false;
             if (include) {
                Display_Feature_Metadata * dfm =
                   dyn_create_dynamic_feature_from_dfr_metadata_dfm(feature_metadata);
                g_ptr_array_add(members_dfm, dfm);
             }

             found = g_hash_table_iter_next(&iter, &hash_key, &hash_value);
          }
       }   // if (dref->dfr)
       result = dyn_create_feature_set0(subset_id, members, members_dfm);
    }      // VCP_SUBSET_DYNAMIC

    else {   // (subset_id != VCP_SUBSET_DYNAMIC
       VCP_Feature_Set * vcp_feature_set =
       create_feature_set(
             subset_id,
             dref->vcp_version,
             feature_set_flags);
       int ct = get_feature_set_size(vcp_feature_set);
       for (int ndx = 0; ndx < ct; ndx++) {
          VCP_Feature_Table_Entry * vfte = get_feature_set_entry(vcp_feature_set, ndx);
          DDCA_Vcp_Feature_Code feature_code = vfte->code;
          Display_Feature_Metadata * dfm =
                dyn_get_feature_metadata_by_dref_dfm(
                      feature_code,
                      dref,
                      feature_set_flags & FSF_SHOW_UNSUPPORTED);   // with_default
          if (dfm)
             g_ptr_array_add(members_dfm, dfm);
       }
       result = dyn_create_feature_set0(subset_id, members, members_dfm);
    }

    if (debug) {
       DBGMSG("Returning: %p", result);
       if (result)
          dbgrpt_dyn_feature_set(result, 1);
    }
    return result;
}


#ifdef OLD
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
#endif

Dyn_Feature_Set *
dyn_create_single_feature_set_by_hexid2(
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_Display_Ref       display_ref,
      bool                   force)
{
   Display_Ref * dref = (Display_Ref *) display_ref;
   assert( dref && memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);

   Dyn_Feature_Set * result = calloc(1, sizeof(Dyn_Feature_Set));
   memcpy(result->marker, DYN_FEATURE_SET_MARKER, 4);
   result->dref = dref;
   result->subset = VCP_SUBSET_SINGLE_FEATURE;
   result->members = g_ptr_array_new();
   result->members_dfm = g_ptr_array_new();
#ifdef IFM
   Internal_Feature_Metadata * ifm = NULL;
#endif
   Display_Feature_Metadata *  dfm = NULL;
   if (dref->dfr) {
      DDCA_Feature_Metadata * feature_metadata  =
         get_dynamic_feature_metadata(dref->dfr, feature_code);
      if (feature_metadata) {
#ifdef IFM
         ifm = dyn_create_dynamic_feature_from_dfr_metadata(feature_metadata);
#endif
         dfm = dyn_create_dynamic_feature_from_dfr_metadata_dfm(feature_metadata);
      }
   }
#ifdef IFM
   if (!ifm) {
      VCP_Feature_Table_Entry* vcp_entry = NULL;
       if (force)
          vcp_entry = vcp_find_feature_by_hexid_w_default(feature_code);
       else
          vcp_entry = vcp_find_feature_by_hexid(feature_code);
       if (vcp_entry)
          ifm = dyn_create_dynamic_feature_from_vcp_feature_table_entry(vcp_entry, dref->vcp_version);
   }
#endif
   if (!dfm) {
      VCP_Feature_Table_Entry* vcp_entry = NULL;
       if (force)
          vcp_entry = vcp_find_feature_by_hexid_w_default(feature_code);
       else
          vcp_entry = vcp_find_feature_by_hexid(feature_code);
       if (vcp_entry)
          dfm = dyn_create_dynamic_feature_from_vcp_feature_table_entry_dfm(vcp_entry, dref->vcp_version);
   }
#ifdef IFM
   if (ifm)
      g_ptr_array_add(result->members, ifm);
#endif

   if (dfm)
      g_ptr_array_add(result->members_dfm, dfm);

   else {
      // free_dyn_feature_set(result)   ??
      // result = NULL  ??
   }

   return result;
}

#ifdef IFM
Internal_Feature_Metadata *
dyn_get_feature_set_entry2(
      Dyn_Feature_Set * feature_set,
      unsigned          index)
{
   assert(feature_set && feature_set->members);
   Internal_Feature_Metadata * result = NULL;
   if (index < feature_set->members->len)
      result = g_ptr_array_index(feature_set->members, index);
   return result;
}
#endif

Display_Feature_Metadata *
dyn_get_feature_set_entry2_dfm(
      Dyn_Feature_Set * feature_set,
      unsigned          index)
{
   assert(feature_set && feature_set->members_dfm);
   Display_Feature_Metadata * result = NULL;
   if (index < feature_set->members_dfm->len)
      result = g_ptr_array_index(feature_set->members_dfm, index);
   return result;
}


int
dyn_get_feature_set_size2(
      Dyn_Feature_Set * feature_set)
{
   assert(feature_set && feature_set->members);
   int result = feature_set->members->len;
   return result;
}

int
dyn_get_feature_set_size2_dfm(
      Dyn_Feature_Set * feature_set)
{
   // show_backtrace(2);
   assert(feature_set);
   assert(feature_set->members_dfm);
   int result = feature_set->members_dfm->len;
   return result;
}


#ifdef OLD
VCP_Feature_Set
dyn_create_feature_set_from_feature_set_ref(
   Feature_Set_Ref *       fsref,
   // DDCA_MCCS_Version_Spec  vcp_version,
   DDCA_Display_Ref        dref,
   Feature_Set_Flags       feature_flags)
 //  bool                    force);
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. fsref=%s, dref=%s, flags=%s",
          fsref_repr(fsref), dref_repr_t(dref), interpret_ddca_feature_flags(feature_flags));

   VCP_Feature_Set result = NULL;
   if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE) {
      result = dyn_create_single_feature_set_by_hexid(fsref->specific_feature, dref, feature_flags & FSF_FORCE);
   }
   else {
      result = dyn_create_feature_set(fsref->subset, dref, feature_flags);
   }

   if (debug || IS_TRACING()) {
      DBGMSG("Returning VCP_Feature_Set %p",  result);
      if (result)
         dbgrpt_feature_set(result, 1);
   }
   return result;
}
#endif


Dyn_Feature_Set *
dyn_create_feature_set_from_feature_set_ref2(
   Feature_Set_Ref *       fsref,
   DDCA_Display_Ref        dref,
   Feature_Set_Flags       flags)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. fsref=%s, dref=%s, flags=%s",
          fsref_repr(fsref), dref_repr_t(dref), interpret_ddca_feature_flags(flags));

   Dyn_Feature_Set* result = NULL;
   if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE) {
      // iftests within function
      result = dyn_create_single_feature_set_by_hexid2(fsref->specific_feature, dref, flags & FSF_FORCE);
   }
   else {
      result = dyn_create_feature_set2_dfm(fsref->subset, dref, flags);
   }

   if (debug || IS_TRACING()) {
      DBGMSG("Returning VCP_Feature_Set %p",  result);
      if (result)
         dbgrpt_dyn_feature_set(result, 1);
   }
   return result;
}


void dyn_free_feature_set(
      Dyn_Feature_Set * feature_set)
{
   DBGMSG("Unimplemented");
}



// or, take DDCA_Feature_List address as parm
DDCA_Feature_List
feature_list_from_dyn_feature_set(Dyn_Feature_Set * fset)
{
   bool debug = true;
   if (debug || IS_TRACING()) {
      DBGMSG("Starting. feature_set = %p", fset);
      show_backtrace(2);
      dbgrpt_dyn_feature_set(fset, 1);
   }

   DDCA_Feature_List vcplist = {{0}};
   assert( fset && memcmp(fset->marker, DYN_FEATURE_SET_MARKER, 4) == 0);
   int ndx = 0;
   for (; ndx < fset->members_dfm->len; ndx++) {
      Display_Feature_Metadata * vcp_entry = NULL;
      vcp_entry = g_ptr_array_index(fset->members_dfm,ndx);

      uint8_t vcp_code = vcp_entry->feature_code;
      DBGMSG("Setting feature: 0x%02x", vcp_code);
      int flagndx   = vcp_code >> 3;
      int shiftct   = vcp_code & 0x07;
      Byte flagbit  = 0x01 << shiftct;
      // printf("(%s) vcp_code=0x%02x, flagndx=%d, shiftct=%d, flagbit=0x%02x\n",
      //        __func__, vcp_code, flagndx, shiftct, flagbit);
      vcplist.bytes[flagndx] |= flagbit;
      // uint8_t bval = vcplist.bytes[flagndx];
      // printf("(%s) vcplist.bytes[%d] = 0x%02x\n",  __func__, flagndx, bval);
   }

   if (debug || IS_TRACING()) {
      DBGMSG("Returning: ");
      rpt_hex_dump(vcplist.bytes, 32, 1);
   }

   return vcplist;
}


