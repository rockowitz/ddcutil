// api_metadata.c

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>
#include <libmain/api_base_internal.h>
#include <libmain/api_displays_internal.h>

 
#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_c_api.h"

#include "util/report_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/feature_lists.h"
#include "base/feature_sets.h"

#include "vcp/vcp_feature_codes.h"
#include "vcp/vcp_feature_set.h"

#include "ddc/ddc_vcp_version.h"

#include "dynvcp/dyn_feature_codes.h"
#include "dynvcp/dyn_feature_set.h"
#include "dynvcp/dyn_dynamic_features.h"

#include "private/ddcutil_c_api_private.h"

#include "api_metadata_internal.h"



//
// Feature Lists
//
// TODO: Move most functions into directory src/base
//

const DDCA_Feature_List DDCA_EMPTY_FEATURE_LIST = {{0}};


void ddca_feature_list_clear(DDCA_Feature_List* vcplist) {
   feature_list_clear(vcplist);
}


void ddca_feature_list_add(DDCA_Feature_List * vcplist, uint8_t vcp_code) {
   feature_list_add(vcplist, vcp_code);
}


bool ddca_feature_list_contains(DDCA_Feature_List * vcplist, uint8_t vcp_code) {
   return feature_list_contains(vcplist, vcp_code);
}


const char *
ddca_feature_list_id_name(
      DDCA_Feature_Subset_Id  feature_subset_id)
{
   char * result = NULL;
   switch (feature_subset_id) {
   case DDCA_SUBSET_KNOWN:
      result = "VCP_SUBSET_KNOWN";
      break;
   case DDCA_SUBSET_COLOR:
      result = "VCP_SUBSET_COLOR";
      break;
   case DDCA_SUBSET_PROFILE:
      result = "VCP_SUBSET_PROFILE";
      break;
   case DDCA_SUBSET_MFG:
      result = "VCP_SUBSET_MFG";
      break;
   case DDCA_SUBSET_UNSET:
      result = "VCP_SUBSET_NONE";
      break;
   }
   return result;
}


#ifdef NEVER_PUBLISHED
DDCA_Status
ddca_get_feature_list(
      DDCA_Feature_Subset_Id  feature_subset_id,
      DDCA_MCCS_Version_Spec  vspec,
      bool                    include_table_features,
      DDCA_Feature_List*      p_feature_list)   // location to fill in
{
   bool debug = false;
   DBGMSF(debug, "Starting. feature_subset_id=%d, vcp_version=%d.%d, include_table_features=%s, p_feature_list=%p",
          feature_subset_id, vspec.major, vspec.minor, bool_repr(include_table_features), p_feature_list);

   DDCA_Status ddcrc = 0;
   // Whether a feature is a table feature can vary by version, so can't
   // specify VCP_SPEC_ANY to request feature ids in any version
   if (!vcp_version_is_valid(vspec, /* allow unknown */ false)) {
      ddcrc = -EINVAL;
      ddca_feature_list_clear(p_feature_list);
      goto bye;
   }
   VCP_Feature_Subset subset = VCP_SUBSET_NONE;  // pointless initialization to avoid compile warning
   switch (feature_subset_id) {
   case DDCA_SUBSET_KNOWN:
      subset = VCP_SUBSET_KNOWN;
      break;
   case DDCA_SUBSET_COLOR:
      subset = VCP_SUBSET_COLOR;
      break;
   case DDCA_SUBSET_PROFILE:
      subset = VCP_SUBSET_PROFILE;
      break;
   case DDCA_SUBSET_MFG:
      subset = VCP_SUBSET_MFG;
      break;
   case DDCA_SUBSET_UNSET:
      subset = VCP_SUBSET_NONE;
      break;
   }
   Feature_Set_Flags feature_flags = 0x00;
   if (!include_table_features)
      feature_flags |= FSF_NOTABLE;
   VCP_Feature_Set fset = create_feature_set(subset, vspec, feature_flags);
   // VCP_Feature_Set fset = create_feature_set(subset, vspec, !include_table_features);

   // TODO: function variant that takes result location as a parm, avoid memcpy
   DDCA_Feature_List result = feature_list_from_feature_set(fset);
   memcpy(p_feature_list, &result, 32);
   free_vcp_feature_set(fset);

#ifdef NO
   DBGMSG("feature_subset_id=%d, vspec=%s, returning:",
          feature_subset_id, format_vspec(vspec));
   rpt_hex_dump(result.bytes, 32, 1);
   for (int ndx = 0; ndx <= 255; ndx++) {
      uint8_t code = (uint8_t) ndx;
      if (ddca_feature_list_test(&result, code))
         printf("%02x ", code);
   }
   printf("\n");
#endif

bye:
   DBGMSF(debug, "Done. Returning: %s", psc_desc(ddcrc));
   if (debug)
      rpt_hex_dump((Byte*) p_feature_list, 32, 1);
   return ddcrc;

}
#endif


DDCA_Status
ddca_get_feature_list_by_dref(
      DDCA_Feature_Subset_Id  feature_set_id,
      DDCA_Display_Ref        ddca_dref,
      bool                    include_table_features,
      DDCA_Feature_List*      p_feature_list)
{
   WITH_DR(
         ddca_dref,
         {
#ifdef OLD
               DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_ref(ddca_dref);

               psc = ddca_get_feature_list(
                     feature_set_id,
                     vspec,               // dref->vcp_version,
                     include_table_features,
                     p_feature_list);
#endif
               bool debug = false;
               DBGMSF(debug, "Starting. feature_subset_id=%d, dref=%p=%s, include_table_features=%s, p_feature_list=%p",
                      feature_set_id, dref, dref_repr_t(dref), bool_repr(include_table_features), p_feature_list);

               DDCA_MCCS_Version_Spec vspec = dref->vcp_version;
               // DBGMSF(debug, "vspec=%p=%s=%d.%d", &dref->vcp_version, format_vspec(dref->vcp_version), dref->vcp_version.major, dref->vcp_version.minor);
               // Whether a feature is a table feature can vary by version, so can't
               // specify VCP_SPEC_ANY to request feature ids in any version
               if (!vcp_version_is_valid(vspec, /* allow unknown */ false)) {
                  psc = -EINVAL;
                  ddca_feature_list_clear(p_feature_list);
                  goto bye;
               }
               VCP_Feature_Subset subset = VCP_SUBSET_NONE;  // pointless initialization to avoid compile warning
               switch (feature_set_id) {
               case DDCA_SUBSET_KNOWN:
                  subset = VCP_SUBSET_KNOWN;
                  break;
               case DDCA_SUBSET_COLOR:
                  subset = VCP_SUBSET_COLOR;
                  break;
               case DDCA_SUBSET_PROFILE:
                  subset = VCP_SUBSET_PROFILE;
                  break;
               case DDCA_SUBSET_MFG:
                  subset = VCP_SUBSET_MFG;
                  break;
               case DDCA_SUBSET_UNSET:
                  subset = VCP_SUBSET_NONE;
                  break;
               }
               Feature_Set_Flags flags = 0x00;
               if (!include_table_features)
                  flags |= FSF_NOTABLE;
               VCP_Feature_Set fset = dyn_create_feature_set2_dfm(subset, dref, flags);
               // VCP_Feature_Set fset = create_feature_set(subset, vspec, !include_table_features);

               // TODO: function variant that takes result location as a parm, avoid memcpy
               DDCA_Feature_List result = feature_list_from_feature_set(fset);
               memcpy(p_feature_list, &result, 32);
               free_vcp_feature_set(fset);

            bye:
               DBGMSF(debug, "Done. Returning: %s", psc_desc(psc));
               if (debug)
                  rpt_hex_dump((Byte*) p_feature_list, 32, 1);

         }
      );
}


DDCA_Feature_List
ddca_feature_list_or(
      DDCA_Feature_List* vcplist1,
      DDCA_Feature_List* vcplist2)
{
   return feature_list_or(vcplist1, vcplist2);
}


DDCA_Feature_List
ddca_feature_list_and(
      DDCA_Feature_List* vcplist1,
      DDCA_Feature_List* vcplist2)
{
   return feature_list_and(vcplist1, vcplist2);
}


DDCA_Feature_List
ddca_feature_list_and_not(
      DDCA_Feature_List* vcplist1,
      DDCA_Feature_List* vcplist2)
{
   return feature_list_and_not(vcplist1, vcplist2);
}


#ifdef UNPUBLISHED
// no real savings in client code
// sample use:
// int codect;
//  uint8_t feature_codes[256];
// ddca_feature_list_to_codes(&vcplist2, &codect, feature_codes);
// printf("\nFeatures in feature set COLOR:  ");
// for (int ndx = 0; ndx < codect; ndx++) {
//       printf(" 0x%02x", feature_codes[ndx]);
// }
// puts("");

/** Converts a feature list into an array of feature codes.
 *
 *  @param[in]  vcplist   pointer to feature list
 *  @param[out] p_codect  address where to return count of feature codes
 *  @param[out] vcp_codes address of 256 byte buffer to receive codes
 */

void ddca_feature_list_to_codes(
      DDCA_Feature_List* vcplist,
      int*               codect,
      uint8_t            vcp_codes[256])
{
   int ctr = 0;
   for (int ndx = 0; ndx < 256; ndx++) {
      if (ddca_feature_list_contains(vcplist, ndx)) {
         vcp_codes[ctr++] = ndx;
      }
   }
   *codect = ctr;
}
#endif


int
ddca_feature_list_count(
      DDCA_Feature_List * feature_list)
{
   return feature_list_count(feature_list);
}


char *
ddca_feature_list_string(
      DDCA_Feature_List * feature_list,
      char * value_prefix,
      char * sepstr)
{
   return feature_list_string(feature_list, value_prefix, sepstr);
}


//
// Feature Metadata
//

#ifdef OLD
// or return a struct?
DDCA_Status ddca_get_feature_flags_by_vcp_version(
      DDCA_Vcp_Feature_Code         feature_code,
      DDCA_MCCS_Version_Id          mccs_version_id,
      DDCA_Version_Feature_Flags *  feature_flags)
{
   DDCA_Status rc = 0;
   DDCA_MCCS_Version_Spec vspec = mccs_version_id_to_spec(mccs_version_id);

   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
   if (!pentry) {
      *feature_flags = 0;
      rc = DDCRC_ARG;
   }
   else {
      DDCA_Version_Feature_Flags vflags = get_version_specific_feature_flags(pentry, vspec);
      *feature_flags = 0;
      // TODO handle subvariants REWORK
      if (vflags & VCP2_RO)
         *feature_flags |= DDCA_RO;
      if (vflags & VCP2_WO)
         *feature_flags |= DDCA_WO;
      if (vflags & VCP2_RW)
         *feature_flags |= DDCA_RW;
      if (vflags & VCP2_CONT)
         *feature_flags |= DDCA_CONTINUOUS;
#ifdef OLD
      if (pentry->feature_flags & VCP_TYPE_V2NC_V3T) {
         if (vspec.major < 3)
            *feature_flags |= DDCA_SIMPLE_NC;
         else
            *feature_flags |= DDCA_TABLE;
      }
#endif
      else if (vflags & DDCA_TABLE)
         *feature_flags |= DDCA_TABLE;
      else if (vflags & VCP2_NC) {
         if (vspec.major < 3)
            *feature_flags |= DDCA_SIMPLE_NC;
         else {
            // TODO: In V3, some features use combination of high and low bytes
            // for now, mark all as simple
            *feature_flags |= DDCA_SIMPLE_NC;
            // alt: DDCT_COMPLEX_NC
         }
      }
   }
   return rc;
}
#endif

#ifdef DVFI
// deprecated
DDCA_Status
ddca_get_feature_info_by_vcp_version(
      DDCA_Vcp_Feature_Code       feature_code,
   // DDCT_MCCS_Version_Spec      vspec,
      DDCA_MCCS_Version_Id        mccs_version_id,
      DDCA_Version_Feature_Info** p_info)
{
   bool debug = false;
   DBGMSF(debug, "Starting. feature_code=0x%02x, mccs_version_id=%d", feature_code, mccs_version_id);

   DDCA_Status psc = 0;
   *p_info = NULL;
   // DDCA_MCCS_Version_Spec vspec = mccs_version_id_to_spec(mccs_version_id);

   // or should this be a version sensitive call?
   DDCA_Version_Feature_Info * info =  get_version_feature_info_by_version_id(
         feature_code,
         mccs_version_id,
         false,                       // with_default
         true);                       // false => version specific, true=> version sensitive
   if (!info)
      psc = DDCRC_ARG;
   else
      *p_info = info;

   DBGMSF(debug, "Returning:%d, *p_info=%p", psc, *p_info);
   return psc;

}
#endif


#ifdef NEVER_RELEASED
DDCA_Status
ddca_get_simplified_feature_info(
      DDCA_Vcp_Feature_Code         feature_code,
      DDCA_MCCS_Version_Spec        vspec,
 //   DDCA_MCCS_Version_Id          mccs_version_id,
      DDCA_Feature_Metadata *   info)
{
   DDCA_Status psc = DDCRC_ARG;
   DDCA_Version_Feature_Info * full_info =  get_version_feature_info_by_vspec(
         feature_code,
         vspec,
         false,                       // with_default
         true);                       // false => version specific, true=> version sensitive
   if (full_info) {
      info->feature_code  = feature_code;
      info->vspec         = vspec;
      info->version_id    = full_info->version_id;    // keep?
      info->feature_flags = full_info->feature_flags;

      free_version_feature_info(full_info);
      psc = 0;
   }
   return psc;
}
#endif



// UNPUBLISHED
/**
 * Gets characteristics of a VCP feature.
 *
 * VCP characteristics (C vs NC, RW vs RO, etc) can vary by MCCS version.
 *
 * @param[in]  feature_code     VCP feature code
 * @param[in]  vspec            MCCS version (may be DDCA_VSPEC_UNKNOWN)
 * @param[out] p_feature_flags  address of flag field to fill in
 * @return     status code
 * @retval     DDCRC_ARG        invalid MCCS version
 * @retval     DDCRC_UNKNOWN_FEATURE  unrecognized feature
 *
 * @since 0.9.0
 */
DDCA_Status
ddca_get_feature_flags_by_vspec(
      DDCA_Vcp_Feature_Code         feature_code,
      DDCA_MCCS_Version_Spec        vspec,
      DDCA_Feature_Flags *          feature_flags)
{
   DDCA_Status psc = DDCRC_ARG;
   if (vcp_version_is_valid(vspec, /*unknown_ok*/ true)) {
//    DDCA_Version_Feature_Info * full_info =  get_version_feature_info_by_vspec(
      Display_Feature_Metadata * dfm =  get_version_feature_info_by_vspec_dfm(
            feature_code,
            vspec,
            false,                       // with_default
            true);                       // false => version specific, true=> version sensitive
      if (dfm) {
         *feature_flags = dfm->feature_flags;
//          free_version_feature_info(full_info);
         free_display_feature_metadata(dfm);
         psc = 0;
      }
      else {
         psc = DDCRC_UNKNOWN_FEATURE;
      }
   }
   return psc;
}


#ifdef NEVER_RELEASED
DDCA_Status
ddca_get_feature_flags_by_version_id(
      DDCA_Vcp_Feature_Code         feature_code,
 //   DDCA_MCCS_Version_Spec        vspec,
      DDCA_MCCS_Version_Id          mccs_version_id,
      DDCA_Feature_Flags *          feature_flags)
{
   DDCA_Status psc = DDCRC_ARG;
   DDCA_Version_Feature_Info * full_info =  get_version_feature_info_by_version_id(
         feature_code,
         mccs_version_id,
         false,                       // with_default
         true);                       // false => version specific, true=> version sensitive
   if (full_info) {
      *feature_flags = full_info->feature_flags;
      free_version_feature_info(full_info);
      psc = 0;
   }
   return psc;
}
#endif


#ifdef DVFI
// deprecated
DDCA_Status
ddca_get_feature_info_by_display(
      DDCA_Display_Handle           ddca_dh,    // needed because in rare cases feature info is MCCS version dependent
      DDCA_Vcp_Feature_Code         feature_code,
      DDCA_Version_Feature_Info **  p_info)
{
   WITH_DH(
      ddca_dh,
      {
         DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(ddca_dh);
         //DDCA_MCCS_Version_Id   version_id = mccs_version_spec_to_id(vspec);
         //psc = ddca_get_feature_info_by_vcp_version(feature_code, version_id, p_info);

         *p_info = get_version_feature_info_by_vspec(
                      feature_code,
                      vspec,
                      false,      //   with_default,
                      true);      //   version_sensitive
         if (!*p_info)
            psc = DDCRC_ARG;
      }
   );
}
#endif


#ifdef DVFI
DDCA_Status
ddca_get_feature_metadata_by_vspec(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_MCCS_Version_Spec      vspec,
      bool                        create_default_if_not_found,
      DDCA_Feature_Metadata *     info) //   change to **?
{
   // DBGMSG("vspec=%d.%d", vspec.major, vspec.minor);
   DDCA_Status psc = DDCRC_ARG;
   memset(info, 0, sizeof(DDCA_Feature_Metadata));
   memcpy(info->marker, DDCA_FEATURE_METADATA_MARKER, 4);
   DDCA_Version_Feature_Info * full_info =
         get_version_feature_info_by_vspec(
               feature_code,
               vspec,
               create_default_if_not_found,
               true);                      // false => version specific, true=> version sensitive
   if (full_info) {
      // DBGMSG("Reading full_info");
      info->feature_code  = feature_code;
 //   info->vspec         = vspec;
      info->feature_flags = full_info->feature_flags;
      if (info->feature_flags & DDCA_SIMPLE_NC)
         info->sl_values = full_info->sl_values;
      if (info->feature_flags & DDCA_SYNTHETIC) {
         // strdup so that don't have to worry about synthesized entries when free
         if (full_info->feature_name)
            info->feature_name  = strdup(full_info->feature_name);
         if (full_info->desc)
            info->feature_desc  = strdup(full_info->desc);
      }
      else {
         info->feature_name  = full_info->feature_name;
         info->feature_desc  = full_info->desc;
      }
      // DBGMSG("Reading full_info done");

      free_version_feature_info(full_info);
      psc = 0;
   }
   return psc;
}
#endif

DDCA_Status
ddca_get_feature_metadata_by_vspec(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_MCCS_Version_Spec      vspec,
      bool                        create_default_if_not_found,
      DDCA_Feature_Metadata *     info) //   change to **?
{
   // DBGMSG("vspec=%d.%d", vspec.major, vspec.minor);
   DDCA_Status psc = DDCRC_ARG;
   memset(info, 0, sizeof(DDCA_Feature_Metadata));
   memcpy(info->marker, DDCA_FEATURE_METADATA_MARKER, 4);
   Display_Feature_Metadata * dfm =
         get_version_feature_info_by_vspec_dfm(
               feature_code,
               vspec,
               create_default_if_not_found,
               true);                      // false => version specific, true=> version sensitive
   if (dfm) {
      // DBGMSG("Reading full_info");
      info->feature_code  = feature_code;
 //   info->vspec         = vspec;
      info->feature_flags = dfm->feature_flags;
      if (info->feature_flags & DDCA_SIMPLE_NC)
         info->sl_values = dfm->sl_values;
      if (info->feature_flags & DDCA_SYNTHETIC) {
         // strdup so that don't have to worry about synthesized entries when free
         if (dfm->feature_name)
            dfm->feature_name  = strdup(dfm->feature_name);
         if (dfm->feature_desc)
            info->feature_desc  = strdup(dfm->feature_desc);
      }
      else {
         info->feature_name  = dfm->feature_name;
         info->feature_desc  = dfm->feature_desc;
      }
      // DBGMSG("Reading full_info done");

      // free_version_feature_info(full_info);
      free_display_feature_metadata(dfm);
      psc = 0;
   }
   return psc;
}


DDCA_Status
ddca_get_feature_metadata_by_dref(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Display_Ref            ddca_dref,
      bool                        create_default_if_not_found,
      DDCA_Feature_Metadata *     info)
{
   WITH_DR(
         ddca_dref,
         {
#ifdef OLD
               // DBGMSG("Starting");
               // dbgrpt_display_ref(dref, 1);

               // returns dref->vcp_version if already cached, queries monitor if not
               DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_ref(ddca_dref);

               psc = ddca_get_feature_metadata_by_vspec(
                     feature_code,
                     vspec,               // dref->vcp_version,
                     create_default_if_not_found,
                     info);
#endif

#ifdef IFM
               Internal_Feature_Metadata * intmeta =
                  dyn_get_feature_metadata_by_dref(feature_code, dref, create_default_if_not_found);
               if (!intmeta) {
                  psc = DDCRC_NOT_FOUND;
               }
               else {
                  memcpy(info, intmeta->external_metadata, sizeof(DDCA_Feature_Metadata));
               }
#endif
#ifdef DFM
               Display_Feature_Metadata * intmeta =
                  dyn_get_feature_metadata_by_dref_dfm(feature_code, dref, create_default_if_not_found);
               if (!intmeta) {
                  psc = DDCRC_NOT_FOUND;
               }
               else {
                  DDCA_Feature_Metadata * meta = dfm_to_ddca_feature_metadata(intmeta);
                  memcpy(info, meta, sizeof(DDCA_Feature_Metadata));
                  // MEMORY MANAGEMENT !!!
               }
#endif
         }
      );
}


DDCA_Status
ddca_get_feature_metadata_by_dh(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Display_Handle         ddca_dh,
      bool                        create_default_if_not_found,
      DDCA_Feature_Metadata *     info)
{
   WITH_DH(
         ddca_dh,
         {
               bool debug = false;
               DBGMSF(debug, "Starting.  feature_code=0x%02x, ddca_dh=%s, create_default_if_not_found=%s, info=%p",
                             feature_code, ddca_dh_repr(ddca_dh), sbool(create_default_if_not_found), info);
               if (debug)
                  dbgrpt_display_ref(dh->dref, 1);

#ifdef OLD
               // Note:  dh->dref->vcp_version may be Unqueried (255,255)
               // Query vcp version here instead of calling
               // ddca_get_feature_metadata_by_dref() because we already have
               // display handle, don't need to open display.

                DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(ddca_dh);
                // DDCA_Monitor_Model_Key* p_mmid = dh->dref->mmid;

                psc = ddca_get_feature_metadata_by_vspec(
                      feature_code,
                      vspec,               // dref->vcp_version,
                      create_default_if_not_found,
                      info);
#endif

#ifdef FAILS_GET_VERSION
                psc = ddca_get_feature_metadata_by_dref(
                     feature_code,
                     dh->dref,
                     create_default_if_not_found,
                     info);
#endif

#ifdef IFM
                Internal_Feature_Metadata * intmeta =
                   dyn_get_feature_metadata_by_dh(feature_code, dh, create_default_if_not_found);
                if (!intmeta) {
                   psc = DDCRC_NOT_FOUND;
                }
                else {
                   memcpy(info, intmeta->external_metadata, sizeof(DDCA_Feature_Metadata));
                }
#endif
#ifdef DFM
               Display_Feature_Metadata * intmeta =
                  dyn_get_feature_metadata_by_dh_dfm(feature_code, dh, create_default_if_not_found);
               if (!intmeta) {
                  psc = DDCRC_NOT_FOUND;
               }
               else {
                  DDCA_Feature_Metadata * meta = dfm_to_ddca_feature_metadata(intmeta);
                  memcpy(info, meta, sizeof(DDCA_Feature_Metadata));
                  // MEMORY MANAGEMENT !!!
               }
#endif

                DBGMSF(debug, "Done.  Returning: %s", ddca_rc_desc(psc));
                // if (psc == 0) {
                //    dbgrpt_internal_feature_metadata(intmeta, 3);
                //    dbgrpt_ddca_feature_metadata(info, 5);
                // }
         }
      );
}

// frees the contents of info, not info itself
DDCA_Status
ddca_free_feature_metadata_contents(DDCA_Feature_Metadata info) {
   if ( memcmp(info.marker, DDCA_FEATURE_METADATA_MARKER, 4) == 0) {
      if (info.feature_flags & DDCA_SYNTHETIC) {
         free(info.feature_name);
         free(info.feature_desc);
      }
      info.marker[3] = 'x';
   }
   return 0;
}

#ifdef DVFI
DDCA_Status
ddca_free_feature_info(
      DDCA_Version_Feature_Info * info)
{
   DDCA_Status rc = 0;
   if (info) {
      if (memcmp(info->marker, VCP_VERSION_SPECIFIC_FEATURE_INFO_MARKER, 4) != 0 )  {
        rc = DDCRC_ARG;
      }
      else {
         free_version_feature_info(info);
      }
   }
   return rc;
}
#endif

// returns pointer into permanent internal data structure, caller should not free
char *
ddca_get_feature_name(DDCA_Vcp_Feature_Code feature_code) {
   // do we want get_feature_name()'s handling of mfg specific and unrecognized codes?
   char * result = get_feature_name_by_id_only(feature_code);
   return result;
}

// deprecated
char *
ddca_feature_name_by_vspec(
      DDCA_Vcp_Feature_Code    feature_code,
      DDCA_MCCS_Version_Spec   vspec,
      DDCA_Monitor_Model_Key * p_mmid)  // currently ignored
{
   char * result = get_feature_name_by_id_and_vcp_version(feature_code, vspec);
   return result;
}

#ifdef NEVER_RELEASED
/** \deprecated */
char *
ddca_feature_name_by_version_id(
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_MCCS_Version_Id   mccs_version_id)
{
   DDCA_MCCS_Version_Spec vspec = mccs_version_id_to_spec(mccs_version_id);
   char * result = get_feature_name_by_id_and_vcp_version(feature_code, vspec);
   return result;
}
#endif


// deprecated
DDCA_Status
ddca_get_feature_name_by_dref(
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_Display_Ref       ddca_dref,
      char **                name_loc)
{
   WITH_DR(ddca_dref,
         {
               //*name_loc = ddca_feature_name_by_vspec(feature_code, dref->vcp_version, dref->mmid);
               *name_loc = get_feature_name_by_id_and_vcp_version(feature_code, dref->vcp_version);
               if (!*name_loc)
                  psc = -EINVAL;
         }
   )
}


//
// Display Inquiry
//

// unpublished
DDCA_Status
ddca_get_simple_sl_value_table_by_vspec(
      DDCA_Vcp_Feature_Code      feature_code,
      DDCA_MCCS_Version_Spec     vspec,
      const DDCA_Monitor_Model_Key *   p_mmid,   // currently ignored
      DDCA_Feature_Value_Entry** value_table_loc)
{
   bool debug = false;
   DDCA_Status rc = 0;
   *value_table_loc = NULL;
   DBGMSF(debug, "feature_code = 0x%02x, vspec=%d.%d",
                 feature_code, vspec.major, vspec.minor);

   if (!vcp_version_is_valid(vspec, /* unknown_ok */ true)) {
      rc = DDCRC_ARG;
      goto bye;
   }

   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
   if (!pentry) {
        *value_table_loc = NULL;
        rc = DDCRC_UNKNOWN_FEATURE;
  }
  else {
     DDCA_Version_Feature_Flags vflags = get_version_sensitive_feature_flags(pentry, vspec);
     if (!(vflags & DDCA_SIMPLE_NC)) {
        *value_table_loc = NULL;
        rc = DDCRC_INVALID_OPERATION;
     }
     else  {
        DDCA_Feature_Value_Entry * table = get_version_sensitive_sl_values(pentry, vspec);
        DDCA_Feature_Value_Entry * table2 = (DDCA_Feature_Value_Entry*) table;    // identical definitions
        *value_table_loc = table2;
        rc = 0;
        DDCA_Feature_Value_Entry * cur = table2;
        if (debug) {
           while (cur->value_name) {
              DBGMSG("   0x%02x - %s", cur->value_code, cur->value_name);
              cur++;
           }
        }
     }
  }

bye:
  DBGMSF(debug, "Done. *pvalue_table=%p, returning %s", *value_table_loc, psc_desc(rc));

   return rc;
}


// for now, just gets SL value table based on the vspec of the display ref,
// eventually handle dynamically assigned monitor specs
DDCA_Status
ddca_get_simple_sl_value_table_by_dref(
      DDCA_Vcp_Feature_Code      feature_code,
      DDCA_Display_Ref           ddca_dref,
      DDCA_Feature_Value_Entry** value_table_loc)
{
   WITH_DR(ddca_dref,
      {
         psc = ddca_get_simple_sl_value_table_by_vspec(
                  feature_code, dref->vcp_version, dref->mmid, value_table_loc);
      }
   )
}


DDCA_Status
ddca_get_simple_sl_value_table(
      DDCA_Vcp_Feature_Code      feature_code,
      DDCA_MCCS_Version_Id       mccs_version_id,
      DDCA_Feature_Value_Entry** value_table_loc)
{
   bool debug = false;
   DDCA_Status rc = 0;
   *value_table_loc = NULL;
   DDCA_MCCS_Version_Spec vspec = mccs_version_id_to_spec(mccs_version_id);
   DBGMSF(debug, "feature_code = 0x%02x, mccs_version_id=%d, vspec=%d.%d",
                 feature_code, mccs_version_id, vspec.major, vspec.minor);

   rc = ddca_get_simple_sl_value_table_by_vspec(
           feature_code, vspec, &DDCA_UNDEFINED_MONITOR_MODEL_KEY,  value_table_loc);

   DBGMSF(debug, "Done. *pvalue_table=%p, returning %s", *value_table_loc, psc_desc(rc));
   return rc;
}


// typedef void * Feature_Value_Table;   // temp

DDCA_Status
ddca_get_simple_nc_feature_value_name_by_table(
      DDCA_Feature_Value_Entry *  feature_value_table,
      uint8_t                     feature_value,
      char**                      value_name_loc)
{
   // DBGMSG("feature_value_table=%p", feature_value_table);
   // DBGMSG("*feature_value_table=%p", *feature_value_table);
   DDCA_Status rc = 0;
   DDCA_Feature_Value_Entry * feature_value_entries = feature_value_table;
   *value_name_loc = vcp_get_feature_value_name(feature_value_entries, feature_value);
   if (!*value_name_loc)
      rc = DDCRC_NOT_FOUND;               // correct handling for value not found?
   return rc;
}


DDCA_Status
ddca_get_simple_nc_feature_value_name_by_vspec(
      DDCA_Vcp_Feature_Code    feature_code,
      DDCA_MCCS_Version_Spec   vspec,    // needed because value lookup mccs version dependent
      const DDCA_Monitor_Model_Key * p_mmid,
      uint8_t                  feature_value,
      char**                   feature_name_loc)
{
   DDCA_Feature_Value_Entry * feature_value_entries = NULL;

   // this should be a function in vcp_feature_codes:
   DDCA_Status rc = ddca_get_simple_sl_value_table_by_vspec(
                      feature_code, vspec, p_mmid, &feature_value_entries);
   if (rc == 0) {
      // DBGMSG("&feature_value_entries = %p", &feature_value_entries);
      rc = ddca_get_simple_nc_feature_value_name_by_table(feature_value_entries, feature_value, feature_name_loc);
   }
   return rc;
}


// deprecated
DDCA_Status
ddca_get_simple_nc_feature_value_name_by_display(
      DDCA_Display_Handle    ddca_dh,    // needed because value lookup mccs version dependent
      DDCA_Vcp_Feature_Code  feature_code,
      uint8_t                feature_value,
      char**                 feature_name_loc)
{
   WITH_DH(ddca_dh,  {
         DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
         DDCA_Monitor_Model_Key * p_mmid = dh->dref->mmid;
         return ddca_get_simple_nc_feature_value_name_by_vspec(
                   feature_code, vspec, p_mmid, feature_value, feature_name_loc);
      }
   );
}


//
//  Dynamic
//

DDCA_Status
ddca_dfr_check_by_dref(DDCA_Display_Ref ddca_dref)
{
   WITH_DR(ddca_dref,
      {
            bool debug = false;
            DBGMSF(debug, "dref=%s", dref_repr_t(dref));

            free_thread_error_detail();
            Error_Info * ddc_excp = dfr_check_by_dref(dref);
            if (ddc_excp) {
               psc = ddc_excp->status_code;
               // DBGMSF(debug, "excp->status_code=%s, psc=%s", ddca_rc_name(ddc_excp->status_code), ddca_rc_name(psc));
               save_thread_error_detail(error_info_to_ddca_detail(ddc_excp));
               errinfo_free(ddc_excp);
            }
            DBGMSF(debug, "Returning: %s", ddca_rc_name(psc));
      }
   );
}

DDCA_Status
ddca_dfr_check_by_dh(DDCA_Display_Handle ddca_dh)
{
   WITH_DH(ddca_dh,
      {
            bool debug = false;
            DBGMSF(debug, "dref=%s", dh_repr_t(dh));

            psc = ddca_dfr_check_by_dref(dh->dref);

            DBGMSF(debug, "Returning: %s", ddca_rc_name(psc));
      }
   );
}

