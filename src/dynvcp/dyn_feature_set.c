/** @file dyn_feature_set.c
 */

// Copyright (C) 2018-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string.h>

#include "util/debug_util.h"
#include "util/report_util.h"
#include "util/traced_function_stack.h"

#include "base/displays.h"
#include "base/feature_lists.h"
#include "base/feature_metadata.h"
#include "base/feature_set_ref.h"
#include "base/rtti.h"

#include "vcp/vcp_feature_codes.h"

#include "dynvcp/vcp_feature_set.h"

#include "dynvcp/dyn_feature_set.h"

static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_UDF;


void free_dyn_feature_set(Dyn_Feature_Set * fset) {
   if (fset) {
      assert( memcmp(fset->marker, DYN_FEATURE_SET_MARKER, 4) == 0);
      if (fset->members_dfm) {
         g_ptr_array_set_free_func(fset->members_dfm, (GDestroyNotify) dfm_free);
         g_ptr_array_free(fset->members_dfm, true);
      }
      free(fset);
   }
}


void report_dyn_feature_set(Dyn_Feature_Set * fset, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fset=%p", fset);

#ifdef TMI
   if (IS_DBGTRC(debug, TRACE_GROUP)) {
      if (fset)
         DBGMSG("marker = |%.4s| = %s", fset->marker, hexstring_t((unsigned char *)fset->marker, 4));
      show_backtrace(1);
      debug_current_traced_function_stack(true);
   }
#endif

   assert( fset && memcmp(fset->marker, DYN_FEATURE_SET_MARKER, 4) == 0);
   for (int ndx=0; ndx < fset->members_dfm->len; ndx++) {
      Display_Feature_Metadata * dfm_entry  = g_ptr_array_index(fset->members_dfm,ndx);
      rpt_vstring(depth,
                  "VCP code: %02X: %s",
                  dfm_entry->feature_code,
                  dfm_entry->feature_name);
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void dbgrpt_dyn_feature_set(
      Dyn_Feature_Set * fset,
      bool              verbose,
      int               depth)
{
   int d0 = depth;
   int d1 = depth+1;

   rpt_vstring(d0, "Subset: %d (%s)", fset->subset, feature_subset_name(fset->subset));
   rpt_label  (d0, "Members (dfm):");
   for (int ndx=0; ndx < fset->members_dfm->len; ndx++) {
      Display_Feature_Metadata * dfm = g_ptr_array_index(fset->members_dfm,ndx);
      if (verbose)
         dbgrpt_display_feature_metadata(dfm, d1);
      else
         rpt_vstring(d1, "0x%02x - %s", dfm->feature_code, dfm->feature_name);
   }
}


char * dyn_feature_set_repr_t(Dyn_Feature_Set * fset) {
   static GPrivate  dynfs_repr_key = G_PRIVATE_INIT(g_free);

   char * buf = get_thread_fixed_buffer(&dynfs_repr_key, 200);
   snprintf(buf, 100, "[%s,%s]",  feature_subset_name(fset->subset), dref_repr_t(fset->dref));
   return buf;
}


static Display_Feature_Metadata *
dyn_create_dynamic_feature_from_dfr_metadata(Dyn_Feature_Metadata * dfr_metadata)
{
   bool debug = false;
   DBGMSF(debug, "Starting. id=0x%02x", dfr_metadata->feature_code);
   Display_Feature_Metadata * dfm = dfm_from_dyn_feature_metadata(dfr_metadata);

   if (dfr_metadata->version_feature_flags & DDCA_SIMPLE_NC) {
      if (dfr_metadata->sl_values)
         dfm->nontable_formatter_sl = dyn_format_feature_detail_sl_lookup;  // HACK
      else
         dfm->nontable_formatter = format_feature_detail_sl_byte;
   }
   else if (dfr_metadata->version_feature_flags & DDCA_EXTENDED_NC) {
      if (dfr_metadata->sl_values)
         dfm->nontable_formatter_sl = dyn_format_feature_detail_sl_lookup_with_sh;  // HACK
      else
         dfm->nontable_formatter = format_feature_detail_sh_sl_bytes;
   }
   else if (dfr_metadata->version_feature_flags & DDCA_STD_CONT)
      dfm->nontable_formatter = format_feature_detail_standard_continuous;
   else if (dfr_metadata->version_feature_flags & DDCA_TABLE)
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

#ifdef UNUSED
static Dyn_Feature_Metadata *
dyn_create_feature_metadata_from_vcp_feature_table_entry(
      VCP_Feature_Table_Entry * pentry,
      DDCA_MCCS_Version_Spec    vspec)
{
   Display_Feature_Metadata * dfm =
     extract_version_feature_info_from_feature_table_entry(pentry, vspec, /*version_sensitive*/ true);

   if (pentry->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY)
      free_synthetic_vcp_entry(pentry);

   Dyn_Feature_Metadata * meta = dfm_to_ddca_feature_metadata(dfm);
   dfm_free(dfm);
   return meta;
}
#endif


#ifdef UNUSED
Display_Feature_Metadata *
dyn_create_dynamic_feature_from_vcp_feature_table_entry_dfm(
      VCP_Feature_Table_Entry * vfte, DDCA_MCCS_Version_Spec vspec)
{
   assert(vfte);
   bool debug = false;
   DBGMSF(debug, "Starting. id=0x%02x", vfte->code);
   // Internal_Feature_Metadata * ifm = calloc(1, sizeof(Internal_Feature_Metadata));
   Dyn_Feature_Metadata * meta = dyn_create_feature_metadata_from_vcp_feature_table_entry(vfte, vspec);
   Display_Feature_Metadata * dfm = dfm_from_dyn_feature_metadata(meta);
   free_ddca_feature_metadata(meta);
   free(meta);

   if (dfm->version_feature_flags & DDCA_SIMPLE_NC) {
      if (dfm->sl_values)
         dfm->nontable_formatter_sl = dyn_format_feature_detail_sl_lookup;
      else
         dfm->nontable_formatter = format_feature_detail_sl_byte;
   }
   else if (dfm->version_feature_flags & DDCA_STD_CONT)
      dfm->nontable_formatter = format_feature_detail_standard_continuous;
   else if (dfm->version_feature_flags & DDCA_TABLE)
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
#endif


static Dyn_Feature_Set *
dyn_create_feature_set0(
      VCP_Feature_Subset   subset_id,
      DDCA_Display_Ref     display_ref,
      GPtrArray *          members_dfm)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "subset_id=%d, number of members=%d",
                              subset_id, (members_dfm) ? members_dfm->len : -1);

   Dyn_Feature_Set * fset = calloc(1,sizeof(Dyn_Feature_Set));
   memcpy(fset->marker, DYN_FEATURE_SET_MARKER, 4);
   fset->subset = subset_id;
   fset->members_dfm = members_dfm;

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %p", fset);
   return fset;
}

static Dyn_Feature_Set *
dyn_create_feature_set1(
      VCP_Feature_Subset   subset_id,
      GPtrArray *          members_dfm)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "subset_id=%d, number of members=%d",
                              subset_id, (members_dfm) ? members_dfm->len : -1);

   Dyn_Feature_Set * fset = calloc(1,sizeof(Dyn_Feature_Set));
   memcpy(fset->marker, DYN_FEATURE_SET_MARKER, 4);
   fset->subset = subset_id;
   fset->members_dfm = members_dfm;

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %p", fset);
   return fset;
}



/**
 *  Selection criteria:
 *  @param  subset_id
 *  @param  feature_set_flags
 *
 *  Feature code characteristics:
 *  @param  vcp_spec_groups     spec groups to which the feature belongs
 *  @param  feature_flags       feature code attributes
 *  @param  vcp_subsets         subsets to which the feature code belongs
 *
 *  @return  true/false depending on whether the feature code satisfies
 *           the selection criteria
 */
bool test_show_feature(
      VCP_Feature_Subset    subset_id,
      Feature_Set_Flags     feature_set_flags,

      gushort               vcp_spec_groups,
      DDCA_Feature_Flags    feature_flags,
      VCP_Feature_Subset    vcp_subsets)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "subset_id=%d - %s, feature_set_flags=0x%02x - %s",
                  subset_id,
                  feature_subset_name(subset_id),
                  feature_set_flags,
                  feature_set_flag_names_t(feature_set_flags));
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "vcp_spec_groups=0x%04x, feature_flags=%s, vcp_subsets=%s",
                   vcp_spec_groups, feature_flags, vcp_subsets);

   bool showit = true;
   bool exclude_table_features = feature_set_flags & FSF_NOTABLE;
    if ((feature_flags & DDCA_TABLE) && exclude_table_features)
       showit = false;
    else {
       showit = false;
        switch(subset_id) {
        case VCP_SUBSET_PRESET:
           showit = vcp_spec_groups & VCP_SPEC_PRESET;
           break;
        case VCP_SUBSET_TABLE:
           showit = feature_flags & DDCA_TABLE;
           break;
        case VCP_SUBSET_CCONT:
           showit = feature_flags & DDCA_COMPLEX_CONT;
           break;
        case VCP_SUBSET_SCONT:
           showit = feature_flags & DDCA_STD_CONT;
           break;
        case VCP_SUBSET_CONT:
           showit = feature_flags & DDCA_CONT;
           break;
        case VCP_SUBSET_SNC:
           showit = feature_flags & DDCA_SIMPLE_NC;
           break;
        case VCP_SUBSET_XNC:
           showit = feature_flags & DDCA_EXTENDED_NC;
           break;
        case VCP_SUBSET_CNC:
           showit = feature_flags & (DDCA_COMPLEX_NC);
           break;
        case VCP_SUBSET_NC_CONT:
           showit = feature_flags & (DDCA_NC_CONT);
           break;
        case VCP_SUBSET_NC_WO:
           showit = feature_flags & (DDCA_WO_NC);
           break;
        case VCP_SUBSET_NC:
           showit = feature_flags & DDCA_NC;
           break;
        case VCP_SUBSET_KNOWN:
//       case VCP_SUBSET_ALL:
//       case VCP_SUBSET_SUPPORTED:
           showit = true;
           break;
        case VCP_SUBSET_COLOR:
        case VCP_SUBSET_PROFILE:
        case VCP_SUBSET_LUT:
        case VCP_SUBSET_TV:
        case VCP_SUBSET_AUDIO:
        case VCP_SUBSET_WINDOW:
        case VCP_SUBSET_DPVL:
        case VCP_SUBSET_CRT:
           showit = vcp_subsets & subset_id;
           break;
        case VCP_SUBSET_SCAN:    // will never happen, inserted to avoid compiler warning
        case VCP_SUBSET_MFG:     // will never happen
        case VCP_SUBSET_UDF: // will never happen
        case VCP_SUBSET_SINGLE_FEATURE:
        case VCP_SUBSET_MULTI_FEATURES:
        case VCP_SUBSET_NONE:
           break;
        }  // switch
        if ( ( feature_set_flags & (FSF_RW_ONLY | FSF_RO_ONLY | FSF_WO_ONLY) ) &&
              subset_id != VCP_SUBSET_SINGLE_FEATURE && subset_id != VCP_SUBSET_NONE) {
           if (feature_set_flags &FSF_RW_ONLY) {
              if (! (feature_flags & DDCA_RW) )
                 showit = false;
           }
           else if (feature_set_flags & FSF_RO_ONLY) {
              if (! (feature_flags & DDCA_RO) )
                 showit = false;
           }
           else if (feature_set_flags & FSF_WO_ONLY) {
              if (! (feature_flags & DDCA_WO) )
                 showit = false;
           }
        }

        if ( feature_flags & DDCA_TABLE)  {
           // DBGMSF(debug, "Before final check for table feature.  showit=%s", bool_repr(showit));
           if (exclude_table_features)
              showit = false;
           // DBGMSF(debug, "After final check for table feature.  showit=%s", bool_repr(showit));
        }
        // if (showit) {
        //    DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding feature 0x%02x", feature_code);
        //    g_ptr_array_add(members_dfm, dfm);
        // }
     }
     // if ( !(feature_flags & DDCA_READABLE) )
    if (feature_set_flags & FSF_READABLE_ONLY) {
       if ( !(feature_flags & DDCA_READABLE) )
          showit = false;
    }

    DBGTRC_RET_BOOL(debug, TRACE_GROUP, showit, "");
   return showit;
}


// #ifdef OLD
/** Given a feature set id for a named feature set (i.e. other than
 *  #VCP_Subset_Single_Feature), creates a #VCP_Feature_Set containing
 *  the features in the set.
 *
 *  @param   subset_id      feature subset id
 *  @param   vcp_version    vcp version, for obtaining most appropriate feature information,
 *                          e.g. feature type can vary by MCCS version
 *  @param   flags          flags to tailor execution
 *  @return  feature set listing the features in the set
 *
 *  @remark
 *  For #VCP_SUBSET_SCAN, whether Table type features are included is controlled
 *  by flag FSF_NOTABLE.
 *  @remark
 *  For remaining subset ids, the following flags apply:
 *  - FSF_NOTABLE - if set, ignore Table type features
 *    (Exception: For #VCP_SUBSET_TABLE and #VCP_SUBSET_LUT, flags #FSF_TABLE is ignored.)
 *  - FSF_RW_ONLY, FSF_RO_ONLY, FSF_WO_ONLy - filter feature ids by whether they are
 *    RW, RO, or WO
 */
VCP_Feature_Set *
create_vcp_feature_set(
      VCP_Feature_Subset     subset_id,
      DDCA_MCCS_Version_Spec vcp_version,
      Feature_Set_Flags      feature_setflags)
{
   assert(subset_id);
   assert(subset_id != VCP_SUBSET_SINGLE_FEATURE);

   bool debug = false;

   DBGTRC_STARTING(debug, TRACE_GROUP, "subset_id=%s(0x%04x), vcp_version=%d.%d, flags=%s",
                 feature_subset_name(subset_id), subset_id, vcp_version.major, vcp_version.minor,
                 feature_set_flag_names_t(feature_setflags));
   // if (IS_DBGTRC(debug, TRACE_GROUP)) {
   //    show_backtrace(2);
   // }

   bool exclude_table_features = feature_setflags & FSF_NOTABLE;

   struct vcp_feature_set * fset = calloc(1,sizeof(struct vcp_feature_set));
   memcpy(fset->marker, VCP_FEATURE_SET_MARKER, 4);
   fset->subset = subset_id;

   fset->members = g_ptr_array_sized_new(250);
   if (subset_id == VCP_SUBSET_SCAN || subset_id == VCP_SUBSET_MFG) {
      int ndx = 1;
      if (subset_id == VCP_SUBSET_MFG)
         ndx = 0xe0;
      for (; ndx < 256; ndx++) {
         Byte id = ndx;
         // DBGMSF(debug, "examining id 0x%02x", id);
         // n. this is a pointer into permanent data structures, should not be freed:
         VCP_Feature_Table_Entry* vcp_entry = vcp_find_feature_by_hexid(id);
         // original code looks at VCP2_READABLE, output level
         if (vcp_entry) {
            bool showit = true;
            if ( is_table_feature_by_vcp_version(vcp_entry, vcp_version) ) {
               if ( /* get_output_level() < DDCA_OL_VERBOSE || */
                    exclude_table_features  )
                  showit = false;
            }
            if (!is_feature_readable_by_vcp_version(vcp_entry, vcp_version)) {
               showit = false;
            }
            if (showit) {
               g_ptr_array_add(fset->members, vcp_entry);
            }
         }
         else {  // unknown feature or manufacturer specific feature
            g_ptr_array_add(fset->members, vcp_create_dummy_feature_for_hexid(id));
            if (ndx >= 0xe0 && (get_output_level() >= DDCA_OL_VERBOSE && !exclude_table_features) ) {
               // for manufacturer specific features, probe as both table and non-table
               // Only probe table if --verbose, output is confusing otherwise
               g_ptr_array_add(fset->members, vcp_create_table_dummy_feature_for_hexid(id));
            }
         }
      }
   }
   else {
      if (subset_id == VCP_SUBSET_TABLE || subset_id == VCP_SUBSET_LUT) {
         exclude_table_features = false;
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Reset exclude_table_features = %s",
                                             SBOOL(exclude_table_features));
      }
      int known_feature_ct = vcp_get_feature_code_count();
      int ndx = 0;
      for (ndx=0; ndx < known_feature_ct; ndx++) {
         VCP_Feature_Table_Entry * vcp_entry = vcp_get_feature_table_entry(ndx);
         assert(vcp_entry);
         DDCA_Version_Feature_Flags vflags =
               get_version_sensitive_feature_flags(vcp_entry, vcp_version);
         bool showit = test_show_feature(
               subset_id,
               feature_setflags,
               vcp_entry->vcp_spec_groups,
               vflags,
               vcp_entry->vcp_subsets);
         if (showit) {
            g_ptr_array_add(fset->members, vcp_entry);
         }
      }
   }

   assert(fset);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %p", fset);
   if (IS_DBGTRC(debug, TRACE_GROUP))
      dbgrpt_vcp_feature_set(fset, 1);
   return fset;
}
// #endif




Dyn_Feature_Set *
dyn_create_feature_set(
      VCP_Feature_Subset     subset_id,
      DDCA_Display_Ref       display_ref,
      Feature_Set_Flags      feature_set_flags)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "subset_id=%d - %s, dref=%s, feature_setflags=0x%02x - %s",
                  subset_id,
                  feature_subset_name(subset_id),
                  dref_repr_t(display_ref),
                  feature_set_flags,
                  feature_set_flag_names_t(feature_set_flags));

    Dyn_Feature_Set * result = NULL;
    Display_Ref * dref = NULL;
    if (display_ref) {
       dref = (Display_Ref *) display_ref;
       assert(memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
    }
    else {
       feature_set_flags &= ~FSF_CHECK_UDF;
    }

    GPtrArray * members_dfm = g_ptr_array_new();

    bool exclude_table_features = feature_set_flags & FSF_NOTABLE;

    if (subset_id == VCP_SUBSET_UDF) {  // all user defined features
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "VCP_SUBSET_UDF path");

       if ( (feature_set_flags&FSF_CHECK_UDF) && dref && dref->dfr) {
          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"dref->dfr is set");
          GHashTableIter iter;
          gpointer hash_key;
          gpointer hash_value;
          g_hash_table_iter_init(&iter, dref->dfr->features);
          bool found = g_hash_table_iter_next(&iter, &hash_key, &hash_value);
          while (found) {
             Dyn_Feature_Metadata * feature_metadata = hash_value;
             assert( memcmp(feature_metadata, DDCA_FEATURE_METADATA_MARKER, 4) == 0 );

             // Test Feature_Set_Flags other than FSF_SHOW_UNSUPPORTED,
             // which does not apply in this context
             bool include = true;
             //     Feature_Set_Flags                     //DDCA_Feature_Flags
             if ( ((feature_set_flags & FSF_NOTABLE) &&  (feature_metadata->version_feature_flags & DDCA_TABLE)) ||
                  ((feature_set_flags & FSF_RO_ONLY) && !(feature_metadata->version_feature_flags & DDCA_RO)   ) ||
                  ((feature_set_flags & FSF_RW_ONLY) && !(feature_metadata->version_feature_flags & DDCA_RW)   ) ||
                  ((feature_set_flags & FSF_WO_ONLY) && !(feature_metadata->version_feature_flags & DDCA_WO)   )
                )
                include = false;
             if (include) {
                Display_Feature_Metadata * dfm =
                   dyn_create_dynamic_feature_from_dfr_metadata(feature_metadata);
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding feature 0x%02x", dfm->feature_code);
                g_ptr_array_add(members_dfm, dfm);
             }

             found = g_hash_table_iter_next(&iter, &hash_key, &hash_value);
          }
       }   // if (dref->dfr)
       // result = dyn_create_feature_set0(subset_id, display_ref, members_dfm);
       result = dyn_create_feature_set1(subset_id, members_dfm);
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "VCP_SUBSET_UDF complete");
    }      // VCP_SUBSET_DYNAMIC

    else if (subset_id == VCP_SUBSET_SCAN || subset_id == VCP_SUBSET_MFG) {
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "VSP_SUBSET_SCAN or VCP_SUBSET_MFG");
       int ndx = 1;
       if (subset_id == VCP_SUBSET_MFG)
          ndx = 0xe0;
       for (; ndx < 256; ndx++) {
          Byte feature_code = ndx;
          // DBGMSF(debug, "examining id 0x%02x", id);
          // n. this is a pointer into permanent data structures, should not be freed:
          //VCP_Feature_Table_Entry* vcp_entry = vcp_find_feature_by_hexid(id);

          Display_Feature_Metadata * dfm = dyn_get_feature_metadata_by_dref(
                                              feature_code,
                                              dref,
                                              feature_set_flags & FSF_CHECK_UDF,
                                              true);    // with_default
          bool showit = true;
          if (!(dfm->version_feature_flags & DDCA_READABLE)) {
             showit = false;
          }
#ifdef OUT
           if (feature_set_flags & FSF_RO_ONLY) {
              if ( !(dfm->version_feature_flags & DDCA_READABLE) )
                 showit = false;
           }
#endif
          if ((dfm->version_feature_flags & DDCA_TABLE) && exclude_table_features) {
             //if ( /* get_output_level() < DDCA_OL_VERBOSE || */ exclude_table_features) {
             showit = false;
          }
          if (showit) {
             DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding feature 0x%02x", dfm->feature_code);
             g_ptr_array_add(members_dfm, dfm);
          }
          else
             dfm_free(dfm);

#ifdef NO
             else {  // unknown feature or manufacturer specific feature
                g_ptr_array_add(fset->members, vcp_create_dummy_feature_for_hexid(id));
                if (ndx >= 0xe0 && (get_output_level() >= DDCA_OL_VERBOSE && !exclude_table_features) ) {
                   // for manufacturer specific features, probe as both table and non-table
                   // Only probe table if --verbose, output is confusing otherwise
                   g_ptr_array_add(fset->members, vcp_create_table_dummy_feature_for_hexid(id));
                }
             }
#endif
       }  // for

       // result = dyn_create_feature_set0(subset_id, display_ref, members_dfm);
       result = dyn_create_feature_set1(subset_id, members_dfm);
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "VCP_SUBSET_SCAN or VCP_SUBSET_MFG complete");
    } // VCP_SUBSET_SCAN

    else {   // (subset_id != VCP_SUBSET_DYNAMIC, != VCP_SUBSET_SCAN
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "subset=id = %s", feature_subset_name(subset_id));
       if (subset_id == VCP_SUBSET_TABLE || subset_id == VCP_SUBSET_LUT) {
          exclude_table_features = false;
          DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Reset exclude_table_features = %s",
                                              SBOOL(exclude_table_features));
       }
       for (int feature_code = 0; feature_code < 256; feature_code++) {
          Display_Feature_Metadata * dfm = dyn_get_feature_metadata_by_dref(
                                              feature_code,
                                              dref,
                                              feature_set_flags & FSF_CHECK_UDF,
                                              false);    // with_default
          if (dfm) {
             bool showit = test_show_feature(
                              subset_id,
                              feature_set_flags,
                              dfm->vcp_spec_groups,
                              dfm->version_feature_flags,
                              dfm->vcp_subsets);
             if (showit) {
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding feature 0x%02x", dfm->feature_code);
                g_ptr_array_add(members_dfm, dfm);
             }
             else
                dfm_free(dfm);
          }

       }
       // result = dyn_create_feature_set0(subset_id, display_ref, members_dfm);
       result = dyn_create_feature_set1(subset_id, members_dfm);
    }

    DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %p", result);
    if (debug && result)
          dbgrpt_dyn_feature_set(result, false, 1);
    return result;
}



/** Creates a VCP_Feature_Set from a feature set reference.
 *
 *  \param  fsref         external feature set reference
 *  \param  vcp_version
 *  \param  flags         checks only FSF_FORCE
 *
 *  \return feature set, NULL if not found
 *
 *  @remark
 *  If creating a #VCP_Feature_Set containing a single specified feature,
 *  flag #FSF_FORCE controls whether a feature set is created for an
 *  unrecognized feature.
 *  @remark
 *  If creating a named feature set, see called function #create_feature_set_ref()
 *  for the effect of #FSF_FORCE and other flags.
 *  @remark
 *  Used only for VCPINFO
 */
Dyn_Feature_Set *
create_dyn_feature_set_from_feature_set_ref(
   Feature_Set_Ref *         fsref,
   DDCA_MCCS_Version_Spec    vcp_version,
   Feature_Set_Flags         flags)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,"fsref=%s, vcp_version=%d.%d. flags=%s",
                             fsref_repr_t(fsref),
                             vcp_version.major, vcp_version.minor,
                             feature_set_flag_names_t(flags));

    Dyn_Feature_Set * fset = NULL;

    assert(!(flags & FSF_CHECK_UDF));

    if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE ||
        fsref->subset == VCP_SUBSET_MULTI_FEATURES)
    {
       fset = calloc(1,sizeof(Dyn_Feature_Set));
       assert(fset);     // avoid coverity "Dereference before null check" warning
       memcpy(fset->marker, DYN_FEATURE_SET_MARKER, 4);
       fset->members_dfm = g_ptr_array_sized_new(1);
       fset->subset = fsref->subset;
       Bit_Set_256_Iterator iter = bs256_iter_new(fsref->features);
       int feature_code = -1;
       while ( (feature_code = bs256_iter_next(iter)) >= 0 ) {
          Byte hexid = (Byte) feature_code;
          // Display_Feature_Metadata * vcp_entry = vcp_find_feature_by_hexid_w_default(hexid);
          Display_Feature_Metadata * dfm_entry = dyn_get_feature_metadata_by_dref(
                                              hexid,
                                              NULL,
                                              flags & FSF_CHECK_UDF,
                                              false);    // with_default
          g_ptr_array_add(fset->members_dfm, dfm_entry);
       }
       bs256_iter_free(iter);
    }
    else {
       fset = dyn_create_feature_set(fsref->subset, NULL, flags);
    }

    DBGTRC_DONE(debug, TRACE_GROUP, "Returning fset %p", fset);
    if (IS_DBGTRC(debug, TRACE_GROUP))
       dbgrpt_dyn_feature_set(fset, false, 1);
    return fset;
}



/** Creates a VCP_Feature_Set from a feature set reference.
 *
 *  \param  fsref         external feature set reference
 *  \param  vcp_version
 *  \param  flags         checks only FSF_FORCE
 *
 *  \return feature set, NULL if not found
 *
 *  @remark
 *  If creating a #VCP_Feature_Set containing a single specified feature,
 *  flag #FSF_FORCE controls whether a feature set is created for an
 *  unrecognized feature.
 *  @remark
 *  If creating a named feature set, see called function #create_feature_set_ref()
 *  for the effect of #FSF_FORCE and other flags.
 *  @remark
 *  Used only for VCPINFO
 */
VCP_Feature_Set *
create_vcp_feature_set_from_feature_set_ref(
   Feature_Set_Ref *         fsref,
   DDCA_MCCS_Version_Spec    vcp_version,
   Feature_Set_Flags         flags)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,"fsref=%s, vcp_version=%d.%d. flags=%s",
                             fsref_repr_t(fsref),
                             vcp_version.major, vcp_version.minor,
                             feature_set_flag_names_t(flags));

    struct vcp_feature_set * fset = NULL;
#ifdef OLD
    if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE) {
       // fset = create_single_feature_set_by_hexid(fsref->specific_feature, flags & FSF_FORCE);
       int feature_code = bs256_first_bit_set(fsref->features);
       assert(feature_code >= 0);
       fset = create_single_feature_set_by_hexid((Byte)feature_code, flags & FSF_FORCE);
    }
    else if (fsref->subset == VCP_SUBSET_MULTI_FEATURES) {
#endif
    if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE ||
        fsref->subset == VCP_SUBSET_MULTI_FEATURES)
    {
       fset = calloc(1,sizeof(struct vcp_feature_set));
       assert(fset);     // avoid coverity "Dereference before null check" warning
       memcpy(fset->marker, VCP_FEATURE_SET_MARKER, 4);
       fset->members = g_ptr_array_sized_new(1);
       fset->subset = fsref->subset;
       Bit_Set_256_Iterator iter = bs256_iter_new(fsref->features);
       int feature_code = -1;
       while ( (feature_code = bs256_iter_next(iter)) >= 0 ) {
          Byte hexid = (Byte) feature_code;
          VCP_Feature_Table_Entry* vcp_entry = vcp_find_feature_by_hexid_w_default(hexid);
          g_ptr_array_add(fset->members, vcp_entry);
       }
       bs256_iter_free(iter);
    }
    else {
       fset = create_vcp_feature_set(fsref->subset, vcp_version, flags);
    }

    DBGTRC_RET_STRUCT(debug, TRACE_GROUP, "Vcp_Feature_Set", dbgrpt_vcp_feature_set, fset);
    return fset;
}



#ifdef UNUSED
Dyn_Feature_Set *
dyn_create_single_feature_set_by_hexid2(
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_Display_Ref       display_ref,
      bool                  force)
{
   bool debug = false;
   DBGMSF(debug, "feature_code=0x%02x, display_ref=%s, force=%s",
                 feature_code, dref_repr_t(display_ref), sbool(force));
   Display_Ref * dref = (Display_Ref *) display_ref;
   assert( dref && memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);

   Dyn_Feature_Set * result = calloc(1, sizeof(Dyn_Feature_Set));
   memcpy(result->marker, DYN_FEATURE_SET_MARKER, 4);
   result->dref = dref;
   result->subset = VCP_SUBSET_SINGLE_FEATURE;
   result->members_dfm = g_ptr_array_new();
   Display_Feature_Metadata *  dfm = NULL;
   if (dref->dfr) {
      DDCA_Featurfree_dfm_funce_Metadata * feature_metadata  =
         get_dynamic_feature_metadata(dref->dfr, feature_code);
      if (feature_metadata) {
         dfm = dyn_create_dynamic_feature_from_dfr_metadata(feature_metadata);
      }
   }

   if (!dfm) {
      VCP_Feature_Table_Entry* vcp_entry = NULL;
       if (force)
          vcp_entry = vcp_find_feature_by_hexid_w_default(feature_code);
       else
          vcp_entry = vcp_find_feature_by_hexid(feature_code);
       if (vcp_entry)
          dfm = dyn_create_dynamic_feature_from_vcp_feature_table_entry_dfm(
                   vcp_entry,
                   get_vcp_version_by_dref(dref) );
                   // dref->vcp_version);
   }

   if (dfm)
      g_ptr_array_add(result->members_dfm, dfm);

   else {
      // free_dyn_feature_set(result)   ??
      // result = NULL  ??
   }

   DBGMSF(debug, "Done. Returning %p", result);
   return result;
}
#endif

// replaces
// VCP_Feature_Table_Entry * get_feature_set_entry(VCP_Feature_Set feature_set, unsigned index);

Display_Feature_Metadata *
dyn_get_feature_set_entry(
      Dyn_Feature_Set * feature_set,
      unsigned          index)
{
   assert(feature_set && feature_set->members_dfm);
   Display_Feature_Metadata * result = NULL;
   if (index < feature_set->members_dfm->len)
      result = g_ptr_array_index(feature_set->members_dfm, index);
   return result;
}


// replaces
// int get_feature_set_size(VCP_Feature_Set feature_set);

int
dyn_get_feature_set_size(
      Dyn_Feature_Set * feature_set)
{
   // show_backtrace(2);
   assert(feature_set);
   assert(feature_set->members_dfm);
   int result = feature_set->members_dfm->len;
   return result;
}

#ifdef UNUSED
Dyn_Feature_Set *
dyn_create_feature_set_from_feature_set_ref2(
   Feature_Set_Ref *       fsref,
   DDCA_Display_Ref        dref,
   Feature_Set_Flags       flags)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. fsref=%s, dref=%s, flags=%s",
          fsref_repr_t(fsref), dref_repr_t(dref), interpret_feature_flags_t(flags));

   Dyn_Feature_Set* result = NULL;
   if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE) {
      // iftests within function
      result = dyn_create_single_feature_set_by_hexid2(fsref->specific_feature, dref, flags & FSF_FORCE);
   }
   else {
      result = dyn_create_feature_set(fsref->subset, dref, flags);
   }

   if (debug || IS_TRACING()) {
      DBGMSG("Returning VCP_Feature_Set %p",  result);
      if (result)
         dbgrpt_dyn_feature_set(result, true, 1);
   }
   return result;
}
#endif


void dyn_free_feature_set(
      Dyn_Feature_Set * feature_set)
{
   bool debug = false;
   DBGMSF(debug, "Starting. feature_set=%s", dyn_feature_set_repr_t(feature_set));
   if (feature_set->members_dfm) {
      g_ptr_array_set_free_func(feature_set->members_dfm, (GDestroyNotify) dfm_free);
      g_ptr_array_free(feature_set->members_dfm,true);
   }
   free(feature_set);
   DBGMSF(debug, "Done");
}

void init_dyn_feature_set() {
   RTTI_ADD_FUNC(dyn_create_feature_set0);
   RTTI_ADD_FUNC(dyn_create_feature_set);
  //  RTTI_ADD_FUNC(create_vcp_feature_set);
   RTTI_ADD_FUNC(create_vcp_feature_set_from_feature_set_ref);
   RTTI_ADD_FUNC(report_dyn_feature_set);
}

