/** @file app_getvcp.c
 *  Implement command GETVCP
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "app_ddcutil/app_getvcp.h"
#include "config.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <stdio.h>

#include "util/data_structures.h"
#include "util/error_info.h"
#include "util/string_util.h"
#include "util/report_util.h"

#ifdef ENABLE_USB
#include "usb_util/hiddev_reports.h"
#include "usb_util/hiddev_util.h"
#endif
/** \endcond */

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/rtti.h"

#include "cmdline/parsed_cmd.h"

#include "vcp/vcp_feature_codes.h"

#include "dynvcp/dyn_feature_codes.h"

#include "ddc/ddc_output.h"
#include "ddc/ddc_vcp_version.h"


// Default trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_TOP;


/**  Shows a single VCP value specified by its #Display_Feature_Metadata
 *
 *   @param  dh           handle of open display
 *   @param  meta         feature metadata
 *   @return status code  0 = normal
 *                        DDCRC_INVALID_OPERATION - feature is deprecated or write-only
 *                        from get_formatted_value_for_feature_table_entry()
 */
DDCA_Status
app_show_single_vcp_value_by_dfm(
      Display_Handle *             dh,
      Display_Feature_Metadata *  dfm)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Getting feature 0x%02x for %s",
                               dfm->feature_code, dh_repr(dh) );

   DDCA_MCCS_Version_Spec vspec      = get_vcp_version_by_dh(dh);
   DDCA_Status            ddcrc      = 0;
   DDCA_Vcp_Feature_Code  feature_id = dfm->feature_code;

   if (!(dfm->feature_flags & DDCA_READABLE)) {
      char * feature_name =  dfm->feature_name;

      DDCA_Feature_Flags vflags = dfm->feature_flags;
      // should get vcp version from metadata
      if (vflags & DDCA_DEPRECATED)
         printf("Feature %02x (%s) is deprecated in MCCS %d.%d\n",
                feature_id, feature_name, vspec.major, vspec.minor);
      else
         printf("Feature %02x (%s) is not readable\n", feature_id, feature_name);
      ddcrc = DDCRC_INVALID_OPERATION;
   }

   if (ddcrc == 0) {
      char * formatted_value = NULL;
      ddcrc = ddc_get_formatted_value_for_dfm(
               dh,
               dfm,
               false,      /* suppress_unsupported */
               true,       /* prefix_value_with_feature_code */
               &formatted_value,
               stdout);    /* msg_fh */
      if (formatted_value) {
         printf("%s\n", formatted_value);
         free(formatted_value);
      }
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "");
   return ddcrc;
}


/**  Shows a single VCP value specified by its feature code
 *
 *   @param  dh           handle of open display
 *   @param  feature_id   feature code
 *   @param  force        generate default metadata if unknown feature id
 *   @return 0 - success
 *           DDCRC_UNKNOWN_FEATURE unrecognized feature id and **force** not specified
 *           from #app_show_single_vcp_value_by_dfm()
 *
 *   Looks up the #Display_Feature_Metadata record for the feature id and calls
 *   #app_show_single_vcp_value_by_dfm() to display the value.
 *   Generates a dummy #Display_Feature_Metadata record for features in the
 *   reserved manufacturer range (xE0..xFF).
 *   if #force is specified, also generates a dummy metadata record for
 *   unrecognized features.
 */
Status_Errno_DDC
app_show_single_vcp_value_by_feature_id(
      Display_Handle *      dh,
      DDCA_Vcp_Feature_Code feature_id,
      bool                  force)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Getting feature 0x%02x for %s, force=%s",
                              feature_id, dh_repr(dh), sbool(force) );

   Status_Errno_DDC         psc = 0;
   Display_Feature_Metadata * dfm = dyn_get_feature_metadata_by_dh(
                                       feature_id,
                                       dh,
                                       force || feature_id >= 0xe0);  // with_default
   if (!dfm) {
      printf("Unrecognized VCP feature code: x%02X\n", feature_id);
      psc = DDCRC_UNKNOWN_FEATURE;
   }
   else {
      psc = app_show_single_vcp_value_by_dfm(dh, dfm);
      dfm_free(dfm);
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, psc, "");
   return psc;
}


/** Shows the VCP values for all features in a VCP feature subset.
 *
 *  @param  dh                display handle
 *  @param  subset_id         feature subset
 *  @param  flags             option flags
 *  @param  features_seen     if non-null, collect list of features found
 *  @return from #show_vcp_values()
 */
Status_Errno_DDC
app_show_vcp_subset_values_by_dh(
        Display_Handle *    dh,
        VCP_Feature_Subset  subset_id,
        Feature_Set_Flags   flags,
        Bit_Set_256 *       features_seen)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
          "dh=%s, subset_id=%s, flags=%s, features_seen=%p",
          dh_repr(dh), feature_subset_name(subset_id), feature_set_flag_names_t(flags),
          features_seen );

   GPtrArray * collector = NULL;
   Status_Errno_DDC psc = ddc_show_vcp_values(dh, subset_id, collector, flags, features_seen);

   if (features_seen)
      DBGTRC_RET_DDCRC(debug, TRACE_GROUP, psc, "features_seen=%s",
                                           bs256_to_string_t(*features_seen, "x", ", ") );
   else
      DBGTRC_RET_DDCRC(debug, TRACE_GROUP, psc, "");
   return psc;
}


/**  Shows the VCP values for all features indicated by a #Feature_Set_Ref
 *
 *   @param  dh      display handle
 *   @param  fsref   feature set reference
 *   @param  flags   option flags
 *   @return status code from #app_show_single_vcp_value_by_feature_id_new_dfm() or
 *                            #app_show_subset_values_by_dh()
 */
Status_Errno_DDC
app_show_feature_set_values_by_dh(
      Display_Handle *     dh,
      Parsed_Cmd *         parsed_cmd)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh: %s. fsref: %s, flags: %s",
                                       dh_repr(dh), fsref_repr_t(parsed_cmd->fref),
                                       feature_set_flag_names_t(parsed_cmd->flags));
   if (debug || IS_TRACING())
      dbgrpt_feature_set_ref(parsed_cmd->fref,1);

   if (parsed_cmd->flags & CMD_FLAG_EXPLICIT_I2C_SOURCE_ADDR)
      alt_source_addr = parsed_cmd->explicit_i2c_source_addr;

   Feature_Set_Ref *    fsref = parsed_cmd->fref;

   // DBGMSG("parsed_cmd->flags: 0x%04x", parsed_cmd->flags);
   Feature_Set_Flags flags = 0x00;
   if (parsed_cmd->flags & CMD_FLAG_SHOW_UNSUPPORTED)
      flags |= FSF_SHOW_UNSUPPORTED;
   // if (parsed_cmd->flags & CMD_FLAG_FORCE)
   //    flags |= FSF_FORCE;                     // unused for getvcp, 11/18/2023
   if (parsed_cmd->flags & CMD_FLAG_NOTABLE)
      flags |= FSF_NOTABLE;
   if (parsed_cmd->flags & CMD_FLAG_RW_ONLY)
      flags |= FSF_RW_ONLY;
   if (parsed_cmd->flags & CMD_FLAG_RO_ONLY)
      flags |= FSF_RO_ONLY;
   // this is nonsense, getvcp on a WO feature should be caught by parser
   if (parsed_cmd->flags & CMD_FLAG_WO_ONLY) {
      // flags |= FSF_WO_ONLY;
      DBGMSG("Invalid: GETVCP for WO features");
      assert(false);
   }
   // char * s0 = feature_set_flag_names(flags);
   // DBGMSG("flags: 0x%04x - %s", flags, s0);
   // free(s0);

   Status_Errno_DDC psc = 0;
#ifdef OLD
   if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE) {
      psc = app_show_single_vcp_value_by_feature_id(
            dh, fsref->specific_feature, true);
   }
   else if (fsref->subset == VCP_SUBSET_MULTI_FEATURES) {
#endif
   if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE ||
       fsref->subset == VCP_SUBSET_MULTI_FEATURES)
   {
      int feature_ct = bs256_count(fsref->features);
      DBGMSF(debug, "VCP_SUBSET_MULTI_FEATURES, feature_ct=%d", feature_ct);
      psc = 0;
      Bit_Set_256_Iterator iter = bs256_iter_new(fsref->features);
      int bitno = bs256_iter_next(iter);
      while (bitno >= 0) {
         DBGMSF(debug, "bitno=0x%02x", bitno);
         int rc = app_show_single_vcp_value_by_feature_id(
               dh, bitno, true);
         if (rc < 0)
            psc = rc;
         bitno = bs256_iter_next(iter);
      }
      bs256_iter_free(iter);
   }
   else {
      psc = app_show_vcp_subset_values_by_dh(
            dh,
            fsref->subset,
            flags,
            NULL);
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, psc, "");
   return psc;
}


void init_app_getvcp() {
   RTTI_ADD_FUNC(app_show_feature_set_values_by_dh);
   RTTI_ADD_FUNC(app_show_vcp_subset_values_by_dh);
   RTTI_ADD_FUNC(app_show_single_vcp_value_by_feature_id);
   RTTI_ADD_FUNC(app_show_single_vcp_value_by_dfm);
}

