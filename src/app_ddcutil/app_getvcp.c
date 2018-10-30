/** @file app_getvcp.c
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <config.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "util/error_info.h"
#include "util/string_util.h"
#include "util/report_util.h"
/** \endcond */

#ifdef USE_USB
#include "usb_util/hiddev_reports.h"
#include "usb_util/hiddev_util.h"
#endif

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/sleep.h"
#include "base/vcp_version.h"

#include "vcp/vcp_feature_codes.h"

#include "dynvcp/dyn_feature_codes.h"

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"

#include "app_ddcutil/app_getvcp.h"


// Default trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_TOP;

#ifdef OLD
/* Shows a single VCP value specified by its feature table entry.
 *
 * Arguments:
 *    dh          handle of open display
 *    entry       hex feature id
 *
 * Returns:
 *    status code 0 = normal
 *                DDCRC_INVALID_OPERATION - feature is deprecated or write-only
 *                from get_formatted_value_for_feature_table_entry()
 */
Public_Status_Code
app_show_single_vcp_value_by_feature_table_entry(
      Display_Handle *           dh,
      VCP_Feature_Table_Entry *  entry)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,
         "Starting. Getting feature 0x%02x for %s", entry->code, dh_repr(dh) );

   DDCA_MCCS_Version_Spec vspec      = get_vcp_version_by_display_handle(dh);
   Public_Status_Code     psc        = 0;
   DDCA_Vcp_Feature_Code  feature_id = entry->code;

   if (!is_feature_readable_by_vcp_version(entry, vspec)) {
      char * feature_name =  get_version_sensitive_feature_name(entry, vspec);
      DDCA_Version_Feature_Flags vflags = get_version_sensitive_feature_flags(entry, vspec);
      if (vflags & DDCA_DEPRECATED)
         printf("Feature %02x (%s) is deprecated in MCCS %d.%d\n",
                feature_id, feature_name, vspec.major, vspec.minor);
      else
         printf("Feature %02x (%s) is not readable\n", feature_id, feature_name);
      psc = DDCRC_INVALID_OPERATION;
   }

   if (psc == 0) {
      char * formatted_value = NULL;
      psc = get_formatted_value_for_feature_table_entry(
               dh,
               entry,
               false,      /* suppress_unsupported */
               true,       /* prefix_value_with_feature_code */
               &formatted_value,
               stdout);    /* msg_fh */
      if (formatted_value) {
         printf("%s\n", formatted_value);
         free(formatted_value);
      }
   }

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", psc_desc(psc));
   return psc;
}
#endif


/* Shows a single VCP value specified by its #Internal_Feature_Metadata
 *
 * Arguments:
 *    dh          handle of open display
 *    meta        feature metadata
 *
 * Returns:
 *    status code 0 = normal
 *                DDCRC_INVALID_OPERATION - feature is deprecated or write-only
 *                from get_formatted_value_for_feature_table_entry()
 */
#ifdef IFM
DDCA_Status
app_show_single_vcp_value_by_internal_metadata(
      Display_Handle *             dh,
      Internal_Feature_Metadata *  meta)
{
   bool debug = true;
   DBGTRC(debug, TRACE_GROUP, "Starting. Getting feature 0x%02x for %s",
                               meta->external_metadata->feature_code, dh_repr(dh) );

   DDCA_Feature_Metadata * extmeta = meta->external_metadata;

   DDCA_MCCS_Version_Spec vspec      = get_vcp_version_by_display_handle(dh);
   DDCA_Status            ddcrc      = 0;
   DDCA_Vcp_Feature_Code  feature_id = extmeta->feature_code;

   if (!(extmeta->feature_flags & DDCA_READABLE)) {
      char * feature_name =  extmeta->feature_name;

      DDCA_Feature_Flags vflags = extmeta->feature_flags;
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
      ddcrc = get_formatted_value_for_internal_metadata(
               dh,
               meta,
               false,      /* suppress_unsupported */
               true,       /* prefix_value_with_feature_code */
               &formatted_value,
               stdout);    /* msg_fh */
      if (formatted_value) {
         printf("%s\n", formatted_value);
         free(formatted_value);
      }
   }

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", psc_desc(ddcrc));
   return ddcrc;
}
#endif



/* Shows a single VCP value specified by its #Display_Feature_Metadata
 *
 * Arguments:
 *    dh          handle of open display
 *    meta        feature metadata
 *
 * Returns:
 *    status code 0 = normal
 *                DDCRC_INVALID_OPERATION - feature is deprecated or write-only
 *                from get_formatted_value_for_feature_table_entry()
 */
DDCA_Status
app_show_single_vcp_value_by_dfm(
      Display_Handle *             dh,
      Display_Feature_Metadata *  dfm)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. Getting feature 0x%02x for %s",
                               dfm->feature_code, dh_repr(dh) );

   // DDCA_Feature_Metadata * extmeta = dfm_to_ddca_feature_metadata(meta);

   DDCA_MCCS_Version_Spec vspec      = get_vcp_version_by_display_handle(dh);
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
      ddcrc = get_formatted_value_for_display_feature_metadata(
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

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", psc_desc(ddcrc));
   return ddcrc;
}



#ifdef PRE_UDF
/* Shows a single VCP value specified by its feature id.
 *
 * Arguments:
 *    dh          handle of open display
 *    feature_id  hex feature id
 *    force       attempt to show value even if feature_id not in feature table
 *
 * Returns:
 *    status code 0 = success
 *                DDCRC_UNKNOWN_FEATURE  feature_id not in feature table and !force
 *                from app_show_single_vcp_value_by_feature_table_entry()
 */
Public_Status_Code
app_show_single_vcp_value_by_feature_id(
      Display_Handle *      dh,
      DDCA_Vcp_Feature_Code feature_id,
      bool                  force)
{
   bool debug = false;
   DBGMSF(debug, "Starting. Getting feature 0x%02x for %s, force=%s",
                 feature_id, dh_repr(dh), bool_repr(force) );

   Public_Status_Code         psc = 0;
   VCP_Feature_Table_Entry *  entry = NULL;

   entry = vcp_find_feature_by_hexid(feature_id);
   if (!entry && (force || feature_id >= 0xe0)) {    // don't require force if manufacturer specific code
      entry = vcp_create_dummy_feature_for_hexid(feature_id);
   }
   if (!entry) {
      printf("Unrecognized VCP feature code: 0x%02x\n", feature_id);
      psc = DDCRC_UNKNOWN_FEATURE;
   }
   else {
      psc = app_show_single_vcp_value_by_feature_table_entry(dh, entry);
   }

   DBGMSF(debug, "Done.  Returning: %s", psc_desc(psc));
   return psc;
}
#endif


#ifdef IFM
Public_Status_Code
app_show_single_vcp_value_by_feature_id_new(
      Display_Handle *      dh,
      DDCA_Vcp_Feature_Code feature_id,
      bool                  force)
{
   bool debug = true;
   DBGTRC(debug, TRACE_GROUP, "Starting. Getting feature 0x%02x for %s, force=%s",
                              feature_id, dh_repr(dh), bool_repr(force) );

   Public_Status_Code         psc = 0;

   Internal_Feature_Metadata * intmeta =
   dyn_get_feature_metadata_by_dh(
         feature_id,
         dh,
         force || feature_id >= 0xe0    // with_default
         );

   // VCP_Feature_Table_Entry *  entry = NULL;


   if (!intmeta) {
      printf("Unrecognized VCP feature code: 0x%02x\n", feature_id);
      psc = DDCRC_UNKNOWN_FEATURE;
   }
   else {
      psc = app_show_single_vcp_value_by_internal_metadata(dh, intmeta);
   }

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", psc_desc(psc));
   return psc;
}
#endif



Public_Status_Code
app_show_single_vcp_value_by_feature_id_new_dfm(
      Display_Handle *      dh,
      DDCA_Vcp_Feature_Code feature_id,
      bool                  force)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. Getting feature 0x%02x for %s, force=%s",
                              feature_id, dh_repr(dh), bool_repr(force) );

   Public_Status_Code         psc = 0;

   Display_Feature_Metadata * dfm =
   dyn_get_feature_metadata_by_dh_dfm(
         feature_id,
         dh,
         force || feature_id >= 0xe0    // with_default
         );

   // VCP_Feature_Table_Entry *  entry = NULL;


   if (!dfm) {
      printf("Unrecognized VCP feature code: 0x%02x\n", feature_id);
      psc = DDCRC_UNKNOWN_FEATURE;
   }
   else {
      psc = app_show_single_vcp_value_by_dfm(dh, dfm);
   }

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", psc_desc(psc));
   return psc;
}



#ifdef DUPLICATE
// swap this for app_show_single_vcp_value_by_feature_id_new() when time to cut over to dfm
Public_Status_Code
app_show_single_vcp_value_by_feature_id_new_dfm(
      Display_Handle *      dh,
      DDCA_Vcp_Feature_Code feature_id,
      bool                  force)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. Getting feature 0x%02x for %s, force=%s",
                              feature_id, dh_repr(dh), bool_repr(force) );

   Public_Status_Code         psc = 0;

   Display_Feature_Metadata * intmeta =
   dyn_get_feature_metadata_by_dh_dfm(
         feature_id,
         dh,
         force || feature_id >= 0xe0    // with_default
         );

   // VCP_Feature_Table_Entry *  entry = NULL;


   if (!intmeta) {
      printf("Unrecognized VCP feature code: 0x%02x\n", feature_id);
      psc = DDCRC_UNKNOWN_FEATURE;
   }
   else {
      psc = app_show_single_vcp_value_by_internal_metadata_dfm(dh, intmeta);
   }

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", psc_desc(psc));
   return psc;
}
#endif


/* Shows the VCP values for all features in a VCP feature subset.
 *
 * Arguments:
 *    dh                display handle
 *    subset_id         feature subset
 *    flags             option flags
 *    features_seen     if non-null, collect list of features found
 *
 * Returns:
 *    status code       from show_vcp_values()
 */
Public_Status_Code
app_show_vcp_subset_values_by_display_handle(
        Display_Handle *    dh,
        VCP_Feature_Subset  subset_id,
        Feature_Set_Flags   flags,
        Byte_Bit_Flags      features_seen)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,
          "Starting. dh=%s, subset_id=%s, flags=%s, features_seen=%p",
          dh_repr(dh), feature_subset_name(subset_id), feature_set_flag_names(flags), features_seen );

   GPtrArray * collector = NULL;
   Public_Status_Code psc = show_vcp_values(dh, subset_id, collector, flags, features_seen);

   if (debug || IS_TRACING()) {
      if (features_seen) {
        char * s = bbf_to_string(features_seen, NULL, 0);
        DBGMSG("Returning: %s. features_seen=%s",  psc_desc(psc), s);
        free(s);
      }
      else {
         DBGMSG("Returning: %s", psc_desc(psc));
      }
   }
   return psc;
}


#ifdef UNUSED
/* Shows the VCP values for all features in a VCP feature subset.
 *
 * Arguments:
 *    pdisp      display reference
 *    subset_id  feature subset
 *    collector  accumulates output
 *    show_unsupported
 *
 * Returns:
 *    nothing
 */
void app_show_vcp_subset_values_by_display_ref(
        Display_Ref *       dref,
        VCP_Feature_Subset  subset_id,
        bool                show_unsupported)
{
   // DBGMSG("Starting.  subset=%d   ", subset );
   // need to ensure that bus info initialized
   bool validDisp = true;
   if (dref->ddc_io_mode == DDC_IO_DEVI2C) {
      // Is this needed?  or checked by openDisplay?
      Bus_Info * bus_info = i2c_get_bus_info(dref->busno);
      if (!bus_info ||  !(bus_info->feature_flags & I2C_BUS_ADDR_0X37) ) {
         printf("Address 0x37 not detected on bus %d. I2C communication not available.\n", dref->busno );
         validDisp = false;
      }
   }
   else {
      validDisp = true;    // already checked
   }

   if (validDisp) {
      GPtrArray * collector = NULL;
      Display_Handle * pDispHandle = ddc_open_display(dref, EXIT_IF_FAILURE);
      show_vcp_values(pDispHandle, subset_id, collector, show_unsupported);
      ddc_close_display(pDispHandle);
   }
}
#endif


/* Shows the VCP values for all features indicated by a Feature_Set_Ref
 *
 * Arguments:
 *    dh                display handle
 *    fsref             feature set reference
 *    flags             option flags
 *
 * Returns:
 *    status code       from app_show_single_vcp_value_by_feature_id() or
 *                           app_show_subset_values_by_display_handle()
 */
Public_Status_Code
app_show_feature_set_values_by_display_handle(
      Display_Handle *     dh,
      Feature_Set_Ref *    fsref,
      Feature_Set_Flags    flags)
{
   bool debug = false;
   if (debug || IS_TRACING()) {
      char * s0 = feature_set_flag_names(flags);
      DBGMSG("Starting. dh: %s. fsref: %s, flags: %s", dh_repr(dh), fsref_repr(fsref), s0);
      // dbgrpt_feature_set_ref(fsref,1);
      free(s0);
   }

   Public_Status_Code psc = 0;
   if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE) {
#ifndef DFM
      psc = app_show_single_vcp_value_by_feature_id_new(
            dh, fsref->specific_feature, feature_flags&FSF_FORCE);
#else
      psc = app_show_single_vcp_value_by_feature_id_new_dfm(
            dh, fsref->specific_feature, flags&FSF_FORCE);
#endif
   }
   else {
      psc = app_show_vcp_subset_values_by_display_handle(
            dh,
            fsref->subset,
            flags,
            NULL);
   }

   DBGTRC(debug, TRACE_GROUP, "Returning: %s", psc_desc(psc));
   return psc;
}


//
// Watch for changed VCP values
//

void reset_vcp_x02(Display_Handle * dh) {
   Error_Info * ddc_excp = ddc_set_nontable_vcp_value(dh, 0x02, 0x01);
   if (ddc_excp) {
      DBGMSG("set_nontable_vcp_value_by_display_handle() returned %s", errinfo_summary(ddc_excp) );
      errinfo_free(ddc_excp);
   }
   else
      DBGMSG("reset feature x02 (new control value) successful");
}

bool new_control_values_exist(Display_Handle * dh) {
   bool debug = false;
   Parsed_Nontable_Vcp_Response * p_nontable_response = NULL;
   // DBGMSF(debug, "VCP version: %d.%d", vspec.major, vspec.minor);
   bool result = false;
    Error_Info * ddc_excp = ddc_get_nontable_vcp_value(
             dh,
             0x02,
             &p_nontable_response);
    if (ddc_excp) {
       DBGMSG("get_nontable_vcp_value() returned %s", errinfo_summary(ddc_excp) );
       errinfo_free(ddc_excp);
    }

    else if (p_nontable_response->sl == 0x01) {
       DBGMSF(debug, "No new control values found");
       free(p_nontable_response);
    }
    else {
       DBGMSG("New control values exist. x02 value: 0x%02x", p_nontable_response->sl);
       free(p_nontable_response);
       p_nontable_response = NULL;
       result = true;
    }
    return result;
}


/** Gets the ID of the next changed feature from VCP feature x52,
 *  then reads and displays the value of that feature.
 *
 *  \param   dh  #Display_Handle
 *  \return  id of changed feature, 0x00 if none
 */

Byte show_changed_feature(Display_Handle * dh) {
   Parsed_Nontable_Vcp_Response * p_nontable_response = NULL;
   Byte changed_feature = 0x00;
   Error_Info * ddc_excp = ddc_get_nontable_vcp_value(
              dh,
              0x52,
              &p_nontable_response);
  // psc = (ddc_excp) ? ddc_excp->psc : 0;
  if (ddc_excp) {
     DBGMSG("get_nontable_vcp_value() for VCP feature x52 returned %s", errinfo_summary(ddc_excp) );
     errinfo_free(ddc_excp);
  }
  else {
     changed_feature = p_nontable_response->sl;
     free(p_nontable_response);
     if (changed_feature)
#ifndef DFM
        app_show_single_vcp_value_by_feature_id_new(dh, changed_feature, false);
#else
        app_show_single_vcp_value_by_feature_id_new_dfm(dh, changed_feature, false);
#endif
  }
  return changed_feature;
}


/* Checks for VCP feature changes by:
 *   - reading feature x02 to check if changes exist,
 *   - querying feature x52 for the id of a changed feature
 *   - reading the value of the changed feature.
 *
 * If the VCP version is 2.1 or less a single feature is
 * read from x52.  For VCP version 3.0 and 2.2, x52 is a
 * FIFO queue of changed features.
 *
 * Finally, 1 is written to feature x02 as a reset.
 *
 * Arguments:
 *    dh      display handle
 */
void
app_read_changes(Display_Handle * dh) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   int MAX_CHANGES = 20;

   // read 02h
   // xff: no user controls
   // x01: no new control values
   // x02: new control values exist

   /* Per the 3.0 and 2.2 specs, feature x52 is a FIFO to be read until value x00 indicates empty
    * What apparently happens on 2.1 (U3011) is that each time feature x02 is reset with value x01
    * the subsequent read of feature x02 returns x02 (new control values exists) until the queue
    * of changes is flushed
    */

   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);

   if (new_control_values_exist(dh)) {
      if ( vcp_version_le(vspec, DDCA_VSPEC_V21) ) {
         show_changed_feature(dh);
      }
      else {  // x52 is a FIFO
         int ctr = 0;
         for (;ctr < MAX_CHANGES; ctr++) {
            Byte cur_feature = show_changed_feature(dh);
            if (cur_feature == 0x00) {
               DBGMSG("No more changed features found");
               break;
            }
         }
         if (ctr == MAX_CHANGES) {
            DBGMSG("Reached loop guard value MAX_CHANGES (%d)", MAX_CHANGES);
         }
      }
      reset_vcp_x02(dh);
   }
}


#ifdef USE_USB
void
app_read_changes_usb(Display_Handle * dh) {
   bool debug = true;
   DBGMSF(debug, "Starting");
   // bool new_values_found = false;

   assert(dh->dref->io_path.io_mode == DDCA_IO_USB);
   int fd = dh->fh;
   int flaguref = HIDDEV_FLAG_UREF;
   struct hiddev_usage_ref uref;
   int rc = ioctl(fd, HIDIOCSFLAG, &flaguref);
   if (rc < 0) {
      REPORT_IOCTL_ERROR("HIDIOCSFLAG", errno);
      return;
   }

   ssize_t ct = read(fd, &uref, sizeof(uref));
   if (ct < 0) {
      int errsv = errno;
      // report the error
      printf("(%s) read failed, errno=%d\n", __func__, errsv);
   }
   else if (ct > 0) {
      rpt_vstring(1, "Read new value:");
      if (ct < sizeof(uref)) {
         rpt_vstring(1, "Short read");
      }
      else {
         report_hiddev_usage_ref(&uref, 1);
         rpt_vstring(1, "New value: 0x%04x (%d)", uref.value, uref.value);
      }
   }
   else {
      DBGMSF(debug, "tick");
   }
}
#endif



/* Infinite loop watching for VCP feature changes reported by the display.
 *
 * Arguments:
 *    dh        display handle
 *
 * Returns:
 *    does not return - halts with program termination
 */
void
app_read_changes_forever(Display_Handle * dh) {
   bool debug = false;

   printf("Watching for VCP feature changes on display %s\n", dh_repr(dh));
   printf("Type ^C to exit...\n");
   // show version here instead of in called function to declutter debug output:
   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   DBGMSF(debug, "VCP version: %d.%d", vspec.major, vspec.minor);
   while(true) {
#ifdef USE_USB
      if (dh->dref->io_path.io_mode == DDCA_IO_USB)
         app_read_changes_usb(dh);
      else
#endif
         app_read_changes(dh);

      sleep_millis( 2500);
   }
}
