/* ddc_output.c
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <config.h>

#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <string.h>
#include <time.h>

#include "util/output_sink.h"
#include "util/report_util.h"

#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/linux_errno.h"
#include "base/parms.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_do_io.h"

#include "adl/adl_errors.h"
#include "adl/adl_shim.h"

#ifdef USE_USB
#include "usb/usb_displays.h"
#include "usb/usb_vcp.h"
#endif

#include "ddc/ddc_edid.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_vcp_version.h"

#include <ddc/ddc_output.h>


// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_DDC;

//
// VCP Feature Table inquiry
//

/* Checks if a feature is a table type feature, given
 * the VCP version of a monitor.
 *
 * For a handful of features, whether the feature is of
 * type Table varies based on the the VCP version. This
 * function encapsulates that check.
 *
 * Arguments:
 *    frec    pointer to VCP feature table entry
 *    dh      display handle of monitor
 *
 * Returns:
 *    true if the specified feature is a table feature,
 *    false if not
 */
bool
is_table_feature_by_display_handle(
      VCP_Feature_Table_Entry *  frec,
      Display_Handle *           dh)
{
   // bool debug = false;
   bool result = false;
   Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   Version_Feature_Flags feature_flags = get_version_sensitive_feature_flags(frec, vcp_version);
   assert(feature_flags);
   result = (feature_flags & VCP2_ANY_TABLE);
   // DBGMSF(debug, "returning: %d", result);
   return result;
}


// For possible future use - currently unused
Global_Status_Code
check_valid_operation_by_feature_rec_and_version(
      VCP_Feature_Table_Entry * frec,
      Version_Spec              vcp_version,
      Version_Feature_Flags     operation_flags)
{
   Version_Feature_Flags feature_flags
      = get_version_sensitive_feature_flags(frec, vcp_version);
   assert(feature_flags);
   ushort rwflags   = operation_flags & VCP2_RW;
   ushort typeflags = operation_flags & (VCP2_TABLE | VCP2_CONT | VCP2_NC);
   Global_Status_Code result = DDCL_INVALID_OPERATION;
   if ( (feature_flags & rwflags) && (feature_flags & typeflags) )
      result = 0;
   return result;
}


Global_Status_Code
check_valid_operation_by_feature_id_and_dh(
      Byte                  feature_id,
      Display_Handle *      dh,
      Version_Feature_Flags operation_flags)
{
   Global_Status_Code result = 0;
   VCP_Feature_Table_Entry * frec = vcp_find_feature_by_hexid(feature_id);
   if (!frec)
      result = DDCL_UNKNOWN_FEATURE;
   else {
      Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
      result = check_valid_operation_by_feature_rec_and_version(frec, vcp_version, operation_flags);
   }
   return result;
}


//
// Get raw VCP feature values
//

/* Get the raw value (i.e. bytes) for a feature table entry.
 *
 * Convert and refine status codes, issue error messages.
 *
 * Arguments;
 *    dh                  display handle
 *    frec                pointer to VCP_Feature_Table_Entry for feature
 *    ignore_unsupported  if false, issue error message for unsupported feature
 *    pvalrec             location where to return pointer to feature value
 *    msg_fh              file handle for error messages
 *
 * Returns:
 *    status code
 */
Global_Status_Code
get_raw_value_for_feature_table_entry(
      Display_Handle *           dh,
      VCP_Feature_Table_Entry *  frec,
      bool                       ignore_unsupported,
      Single_Vcp_Value **        pvalrec,
      FILE *                     msg_fh)
{
   bool debug = false;
   // Trace_Group tg = (debug) ? 0xff : TRACE_GROUP;
   // TRCMSGTG(tg, "Starting");
   DBGTRC(debug, TRACE_GROUP, "Starting");

   Global_Status_Code gsc = 0;
   Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   char * feature_name = get_version_sensitive_feature_name(frec, vspec);

   Byte feature_code = frec->code;
   bool is_table_feature = is_table_feature_by_display_handle(frec, dh);
   Vcp_Value_Type feature_type = (is_table_feature) ? TABLE_VCP_VALUE : NON_TABLE_VCP_VALUE;
   Output_Level output_level = get_output_level();
   Single_Vcp_Value * valrec = NULL;
   if (dh->io_mode == USB_IO) {
#ifdef USE_USB
      gsc = usb_get_vcp_value(
              dh,
              feature_code,
              feature_type,
              &valrec);
#else
      PROGRAM_LOGIC_ERROR("ddcutil not build with USB support");
#endif
   }
   else {
      gsc = get_vcp_value(
              dh,
              feature_code,
              feature_type,
              &valrec);
   }
   assert ( (gsc==0 && valrec) || (gsc!=0 && !valrec) );

   switch(gsc) {
   case 0:
      break;

   case DDCRC_INVALID_DATA:
      if (output_level >= OL_NORMAL)
         f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                         feature_code, feature_name, "Invalid response");
      break;

   case DDCRC_NULL_RESPONSE:
      // for unsupported features, some monitors return null response rather than a valid response
      // with unsupported feature indicator set
      if (!ignore_unsupported) {
         f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                        feature_code, feature_name, "Unsupported feature code (Null response)");
      }
      COUNT_STATUS_CODE(DDCRC_DETERMINED_UNSUPPORTED);
      gsc = DDCRC_DETERMINED_UNSUPPORTED;
      break;

   case DDCRC_READ_ALL_ZERO:
      // treat as invalid response if not table type?
      if (!ignore_unsupported) {
         f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                        feature_code, feature_name, "Unsupported feature code (All zero response)");
      }
      gsc = DDCRC_DETERMINED_UNSUPPORTED;
      COUNT_STATUS_CODE(DDCRC_DETERMINED_UNSUPPORTED);
      break;

   case DDCRC_RETRIES:
      f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                      feature_code, feature_name, "Maximum retries exceeded");
      break;

   case DDCRC_REPORTED_UNSUPPORTED:
   case DDCRC_DETERMINED_UNSUPPORTED:
      if (!ignore_unsupported) {
         f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                         feature_code, feature_name, "Unsupported feature code");
      }
      break;

   default:
   {
      char buf[200];
      snprintf(buf, 200, "Invalid response. status code=%s", gsc_desc(gsc));
      f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                       feature_code, feature_name, buf);
   }
   }

   *pvalrec = valrec;
   // TRCMSGTG(tg, "Done.  Returning: %s, *pvalrec=%p", gsc_desc(gsc), *pvalrec);
   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s, *pvalrec=%p", gsc_desc(gsc), *pvalrec);
   assert( (gsc == 0 && *pvalrec) || (gsc != 0 && !*pvalrec) );
   return gsc;
}


/* Gather values for the features in a feature set.
 *
 * Arguments:
 *    dh                 display handle
 *    feature_set        feature set identifying features to be queried
 *    vset               append values retrieved to this value set
 *    ignore_unspported  unsupported features are not an error
 *    msg_fh             destination for error messages
 *
 * Returns:
 *    status code
 */
Global_Status_Code
collect_raw_feature_set_values(
      Display_Handle *      dh,
      VCP_Feature_Set       feature_set,
      Vcp_Value_Set         vset,
      bool                  ignore_unsupported,  // if false, is error if unsupported
      FILE *                msg_fh)
{
   Global_Status_Code master_status_code = 0;
   bool debug = false;
   DBGMSF(debug, "Starting.");
   // Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   int features_ct = get_feature_set_size(feature_set);
   int ndx;
   for (ndx=0; ndx< features_ct; ndx++) {
      VCP_Feature_Table_Entry * entry = get_feature_set_entry(feature_set, ndx);
      DBGMSF(debug,"ndx=%d, feature = 0x%02x", ndx, entry->code);
      Single_Vcp_Value *    pvalrec;
      Global_Status_Code cur_status_code =
       get_raw_value_for_feature_table_entry(
         dh,
         entry,
         ignore_unsupported,
         &pvalrec,
         msg_fh);
      if (cur_status_code == 0) {
         vcp_value_set_add(vset, pvalrec);
      }
      else if ( (cur_status_code == DDCRC_REPORTED_UNSUPPORTED ||
                 cur_status_code == DDCRC_DETERMINED_UNSUPPORTED
                ) && ignore_unsupported
              )
      {
         // no problem
      }
      else {
         master_status_code = cur_status_code;
         break;
      }
   }

   return master_status_code;
}


/* Gather values for the features in a named feature subset
 *
 * Arguments:
 *    dh                 display handle
 *    subset             feature set identifier
 *    vset               append values retrieved to this value set
 *    ignore_unspported  unsupported features are not an error
 *    msg_fh             destination for error messages
 *
 * Returns:
 *    status code
 */
Global_Status_Code
collect_raw_subset_values(
        Display_Handle *    dh,
        VCP_Feature_Subset  subset,
        Vcp_Value_Set       vset,
        bool                ignore_unsupported,
        FILE *              msg_fh)
{
   Global_Status_Code gsc = 0;
   bool debug = false;
   DBGMSF(debug, "Starting.  subset=%d  dh=%s", subset, display_handle_repr(dh) );
   Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   // DBGMSG("VCP version = %d.%d", vcp_version.major, vcp_version.minor);
   VCP_Feature_Set feature_set = create_feature_set(subset, vcp_version);
   if (debug)
      report_feature_set(feature_set, 0);

   gsc = collect_raw_feature_set_values(
            dh, feature_set, vset,
            ignore_unsupported, msg_fh);
   DBGMSF(debug, "Done");
   return gsc;
}


//
// Get formatted feature values
//

Global_Status_Code
get_formatted_value_for_feature_table_entry(
      Display_Handle *           dh,
      VCP_Feature_Table_Entry *  vcp_entry,
      bool                       suppress_unsupported,
      bool                       prefix_value_with_feature_code,
      char **                    pformatted_value,
      FILE *                     msg_fh)
{
   bool debug = false;
   // Trace_Group tg = (debug) ? 0xff : TRACE_GROUP;
   // TRCMSGTG(tg, "Starting");
   DBGTRC(debug, TRACE_GROUP, "Starting");

   Global_Status_Code gsc = 0;
   *pformatted_value = NULL;

   Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   Byte feature_code = vcp_entry->code;
   char * feature_name = get_version_sensitive_feature_name(vcp_entry, vspec);
   bool is_table_feature = is_table_feature_by_display_handle(vcp_entry, dh);
   Vcp_Value_Type feature_type = (is_table_feature) ? TABLE_VCP_VALUE : NON_TABLE_VCP_VALUE;
   Output_Level output_level = get_output_level();
   if (output_level >= OL_VERBOSE) {
      fprintf(msg_fh, "\nGetting data for %s VCP code 0x%02x - %s:\n",
                            (is_table_feature) ? "table" : "non-table",
                            feature_code,
                            feature_name);
   }

   Single_Vcp_Value *    pvalrec = NULL;
   bool ignore_unsupported = !(output_level >= OL_NORMAL && !suppress_unsupported);
   gsc = get_raw_value_for_feature_table_entry(
            dh,
            vcp_entry,
            ignore_unsupported,
            &pvalrec,
            msg_fh);
   assert( (gsc==0 && (feature_type == pvalrec->value_type)) || (gsc!=0 && !pvalrec) );
   if (gsc == 0) {
      if (!is_table_feature && output_level >= OL_VERBOSE) {
         rpt_push_output_dest(msg_fh);
         report_single_vcp_value(pvalrec, 0);
         rpt_pop_output_dest();
      }

      if (output_level == OL_PROGRAM) {
         if (is_table_feature) {                // OL_PROGRAM, is table feature
            // output VCP code  hex values of bytes
            int bytect = pvalrec->val.t.bytect;
            int hexbufsize = bytect * 3;
            char * hexbuf = calloc(hexbufsize, sizeof(char));
            char space = ' ';
            hexstring2(pvalrec->val.t.bytes, bytect, &space, false /* upper case */, hexbuf, hexbufsize);
            char * formatted = calloc(hexbufsize + 20, sizeof(char));
            snprintf(formatted, hexbufsize+20, "VCP %02X %s\n", feature_code, hexbuf);
            *pformatted_value = formatted;
            free(hexbuf);
         }
         else {                                // OL_PROGRAM, not table feature
            char buf[200];
            snprintf(buf, 200, "VCP %02X %5d", vcp_entry->code, pvalrec->val.c.cur_val);
            *pformatted_value = strdup(buf);
         }
      }
      else  {          // normal (non OL_PROGRAM) output
         bool ok;
         char * formatted_data = NULL;

         ok = vcp_format_feature_detail(
                 vcp_entry,
                 vspec,
                 pvalrec,
                 &formatted_data);
         // DBGMSG("vcp_format_feature_detail set formatted_data=|%s|", formatted_data);
         if (!ok) {
            f0printf(msg_fh, FMT_CODE_NAME_DETAIL_WO_NL,
                            feature_code, feature_name, "!!! UNABLE TO FORMAT OUTPUT");
            gsc = DDCRC_INTERPRETATION_FAILED;
            // TODO: retry with default output function
         }

         if (ok) {
            if (prefix_value_with_feature_code) {
               *pformatted_value = calloc(1, strlen(formatted_data) + 50);
               snprintf(*pformatted_value, strlen(formatted_data) + 49,
                        FMT_CODE_NAME_DETAIL_WO_NL,
                        feature_code, feature_name, formatted_data);
               free(formatted_data);
            }
            else {
                *pformatted_value = formatted_data;
             }
         }
      }     // normal (non OL_PROGRAM) output

   }

   if (pvalrec)
      free_single_vcp_value(pvalrec);

   // TRCMSGTG(tg, "Done.  Returning: %s, *pformatted_value=|%s|", gsc_desc(gsc), *pformatted_value);
   DBGTRC(debug, TRACE_GROUP,
          "Done.  Returning: %s, *pformatted_value=|%s|",
          gsc_desc(gsc), *pformatted_value);
   return gsc;
}


Global_Status_Code
show_feature_set_values(
      Display_Handle *      dh,
      VCP_Feature_Set       feature_set,
      GPtrArray *           collector,     // if null, write to FOUT
      bool                  force_show_unsupported)
{
   Global_Status_Code master_status_code = 0;
   bool debug = false;
   DBGMSF(debug, "Starting.  collector=%p", collector);

   VCP_Feature_Subset subset_id = get_feature_set_subset_id(feature_set);
   Output_Level output_level = get_output_level();
   bool show_unsupported = false;
   if ( force_show_unsupported     ||
        output_level >= OL_VERBOSE ||
        subset_id == VCP_SUBSET_SINGLE_FEATURE
       )
       show_unsupported = true;
   bool suppress_unsupported = !show_unsupported;

   Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   bool prefix_value_with_feature_code = true;    // TO FIX
   FILE * msg_fh = FOUT;                        // TO FIX
   int features_ct = get_feature_set_size(feature_set);
   DBGMSF(debug, "features_ct=%d", features_ct);
   int ndx;
   for (ndx=0; ndx< features_ct; ndx++) {
      VCP_Feature_Table_Entry * entry = get_feature_set_entry(feature_set, ndx);
      DBGMSF(debug,"ndx=%d, feature = 0x%02x", ndx, entry->code);
      if (!is_feature_readable_by_vcp_version(entry, vcp_version)) {
         // confuses the output if suppressing unsupported
         if (show_unsupported) {
            char * feature_name =  get_version_sensitive_feature_name(entry, vcp_version);
            Version_Feature_Flags vflags = get_version_sensitive_feature_flags(entry, vcp_version);
            char * msg = (vflags & VCP2_DEPRECATED) ? "Deprecated" : "Write-only feature";
            fprintf(FOUT, FMT_CODE_NAME_DETAIL_W_NL,
                          entry->code, feature_name, msg);
         }
      }
      else {
         char * formatted_value = NULL;
         Global_Status_Code gsc =
         get_formatted_value_for_feature_table_entry(
               dh,
               entry,
               suppress_unsupported,
               prefix_value_with_feature_code,
               &formatted_value,
               msg_fh);
         assert( (gsc==0 && formatted_value) || (gsc!=0 && !formatted_value) );
         if (gsc == 0) {
            if (collector)
               g_ptr_array_add(collector, formatted_value);
            else
               fprintf(FOUT, "%s\n", formatted_value);
         }
         else {
            // or should I check features_ct == 1?
            VCP_Feature_Subset subset_id = get_feature_set_subset_id(feature_set);
            if (subset_id == VCP_SUBSET_SINGLE_FEATURE)
               master_status_code = gsc;
            else {
               if ( (gsc != DDCRC_REPORTED_UNSUPPORTED) && (gsc != DDCRC_DETERMINED_UNSUPPORTED) ) {
                  if (master_status_code == 0)
                     master_status_code = gsc;
               }
            }
         }
      }
   }   // loop over features

   DBGMSF(debug, "Returning: %s", gsc_desc(master_status_code));
   return master_status_code;
}


/* Shows the VCP values for all features in a VCP feature subset.
 *
 * Arguments:
 *    dh         display handle for open display
 *    subset     feature subset id
 *    collector  accumulates output    // if null, write to stdout
 *    force_show_unsupported
 *
 * Returns:
 *    status code
 */
Global_Status_Code
show_vcp_values(
        Display_Handle *    dh,
        VCP_Feature_Subset  subset,
        GPtrArray *         collector,    // not used
        bool                force_show_unsupported)
{
   Global_Status_Code gsc = 0;
   bool debug = false;
   DBGMSF(debug, "Starting.  subset=%d  dh=%s", subset, display_handle_repr(dh) );
   Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   // DBGMSG("VCP version = %d.%d", vcp_version.major, vcp_version.minor);
   VCP_Feature_Set feature_set = create_feature_set(subset, vcp_version);
   if (debug)
      report_feature_set(feature_set, 0);

   gsc = show_feature_set_values(
            dh, feature_set, collector, force_show_unsupported);
   DBGMSF(debug, "Done");
   return gsc;
}

