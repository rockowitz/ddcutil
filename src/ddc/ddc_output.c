/* ddc_output.c
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \file
 */

#include <config.h>

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <string.h>
#include <time.h>

#include "util/error_info.h"
#include "util/report_util.h"

#include "base/adl_errors.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/linux_errno.h"
#include "base/parms.h"
#include "base/sleep.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_do_io.h"

#include "adl/adl_shim.h"

#ifdef USE_USB
#include "usb/usb_displays.h"
#include "usb/usb_vcp.h"
#endif

#include "vcp/parse_capabilities.h"

#include "ddc/ddc_edid.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_vcp_version.h"

#include <ddc/ddc_output.h>


// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_DDC;


// Standard format strings for reporting feature codes.
const char* FMT_CODE_NAME_DETAIL_WO_NL = "VCP code 0x%02x (%-30s): %s";
const char* FMT_CODE_NAME_DETAIL_W_NL  = "VCP code 0x%02x (%-30s): %s\n";

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
   DDCA_MCCS_Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   DDCA_Version_Feature_Flags feature_flags = get_version_sensitive_feature_flags(frec, vcp_version);
   assert(feature_flags);
   result = (feature_flags & DDCA_TABLE);
   // DBGMSF(debug, "returning: %d", result);
   return result;
}


// For possible future use - currently unused
Public_Status_Code
check_valid_operation_by_feature_rec_and_version(
      VCP_Feature_Table_Entry *   frec,
      DDCA_MCCS_Version_Spec      vcp_version,
      DDCA_Version_Feature_Flags  operation_flags)
{
   DDCA_Version_Feature_Flags feature_flags
      = get_version_sensitive_feature_flags(frec, vcp_version);
   assert(feature_flags);
   ushort rwflags   = operation_flags & DDCA_RW;
   ushort typeflags = operation_flags & (DDCA_NORMAL_TABLE | DDCA_CONT | DDCA_NC);
   Public_Status_Code result = DDCL_INVALID_OPERATION;
   if ( (feature_flags & rwflags) && (feature_flags & typeflags) )
      result = 0;
   return result;
}


Public_Status_Code
check_valid_operation_by_feature_id_and_dh(
      Byte                  feature_id,
      Display_Handle *      dh,
      DDCA_Version_Feature_Flags operation_flags)
{
   Public_Status_Code result = 0;
   VCP_Feature_Table_Entry * frec = vcp_find_feature_by_hexid(feature_id);
   if (!frec)
      result = DDCL_UNKNOWN_FEATURE;
   else {
      DDCA_MCCS_Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
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
Public_Status_Code
get_raw_value_for_feature_table_entry(
      Display_Handle *           dh,
      VCP_Feature_Table_Entry *  frec,
      bool                       ignore_unsupported,
      DDCA_Single_Vcp_Value **   pvalrec,
      FILE *                     msg_fh)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting", NULL);

   assert(dh);
   assert(dh->dref);

   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;

   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   char * feature_name = get_version_sensitive_feature_name(frec, vspec);

   Byte feature_code = frec->code;
   bool is_table_feature = is_table_feature_by_display_handle(frec, dh);
   DDCA_Vcp_Value_Type feature_type = (is_table_feature) ? DDCA_TABLE_VCP_VALUE : DDCA_NON_TABLE_VCP_VALUE;
   DDCA_Output_Level output_level = get_output_level();
   DDCA_Single_Vcp_Value * valrec = NULL;
   if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
#ifdef USE_USB
     psc = usb_get_vcp_value(
              dh,
              feature_code,
              feature_type,
              &valrec);
     if (psc != 0)
        ddc_excp = errinfo_new(psc, __func__);
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
   }
   else {
      ddc_excp = get_vcp_value(
              dh,
              feature_code,
              feature_type,
              &valrec);
      psc = ERRINFO_STATUS(ddc_excp);
   }
   assert ( (psc==0 && valrec) || (psc!=0 && !valrec) );

   switch(psc) {
   case 0:
      break;

   case DDCRC_INVALID_DATA:
      if (output_level >= DDCA_OL_NORMAL)
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
      psc = DDCRC_DETERMINED_UNSUPPORTED;
      break;

   case DDCRC_READ_ALL_ZERO:
      // treat as invalid response if not table type?
      if (!ignore_unsupported) {
         f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                        feature_code, feature_name, "Unsupported feature code (All zero response)");
      }
      psc = DDCRC_DETERMINED_UNSUPPORTED;
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
      snprintf(buf, 200, "Invalid response. status code=%s, %s", psc_desc(psc), dh_repr_t(dh));
      f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                       feature_code, feature_name, buf);
   }
   }

   *pvalrec = valrec;
   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s, *pvalrec=%p", psc_desc(psc), *pvalrec);
   assert( (psc == 0 && *pvalrec) || (psc != 0 && !*pvalrec) );
   if (*pvalrec && (debug || IS_TRACING())) {
      dbgrpt_ddca_single_vcp_value(*pvalrec, 1);
   }
   if (ddc_excp) {
#ifdef OLD
      if (debug || IS_TRACING() || report_freed_exceptions) {
         DBGMSG("Freeing exception:");
         errinfo_report(ddc_excp, 1);
      }
      errinfo_free(ddc_excp);
#endif
      ERRINFO_FREE_WITH_REPORT(ddc_excp, debug || IS_TRACING() || report_freed_exceptions);
   }
   return psc;
}


/* Gather values for the features in a feature set.
 *
 * Arguments:
 *    dh                  display handle
 *    feature_set         feature set identifying features to be queried
 *    vset                append values retrieved to this value set
 *    ignore_unsupported  unsupported features are not an error
 *    msg_fh              destination for error messages
 *
 * Returns:
 *    status code
 */
Public_Status_Code
collect_raw_feature_set_values(
      Display_Handle *      dh,
      VCP_Feature_Set       feature_set,
      Vcp_Value_Set         vset,
      bool                  ignore_unsupported,  // if false, is error if unsupported
      FILE *                msg_fh)
{
   Public_Status_Code master_status_code = 0;
   bool debug = false;
   DBGMSF0(debug, "Starting.");
   int features_ct = get_feature_set_size(feature_set);
   // needed when called from C API, o.w. get get NULL response for first feature
   // DBGMSG("Inserting sleep() before first call to get_raw_value_for_feature_table_entry()");
   // sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, "initial");
   int ndx;
   for (ndx=0; ndx< features_ct; ndx++) {
      VCP_Feature_Table_Entry * entry = get_feature_set_entry(feature_set, ndx);
      DBGMSF(debug,"ndx=%d, feature = 0x%02x", ndx, entry->code);
      DDCA_Single_Vcp_Value *    pvalrec;
      Public_Status_Code cur_status_code =
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
 *    ignore_unsupported  unsupported features are not an error
 *    msg_fh             destination for error messages
 *
 * Returns:
 *    status code
 */
Public_Status_Code
collect_raw_subset_values(
        Display_Handle *    dh,
        VCP_Feature_Subset  subset,
        Vcp_Value_Set       vset,
        bool                ignore_unsupported,
        FILE *              msg_fh)
{
   Public_Status_Code psc = 0;
   bool debug = false;
   DBGMSF(debug, "Starting.  subset=%d  dh=%s", subset, dh_repr(dh) );
   DDCA_MCCS_Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   // DBGMSG("VCP version = %d.%d", vcp_version.major, vcp_version.minor);
   VCP_Feature_Set feature_set = create_feature_set(subset, vcp_version);
   if (debug)
      report_feature_set(feature_set, 0);

   psc = collect_raw_feature_set_values(
            dh, feature_set, vset,
            ignore_unsupported, msg_fh);
   free_vcp_feature_set(feature_set);
   DBGMSF0(debug, "Done");
   return psc;
}


//
// Get formatted feature values
//

Public_Status_Code
get_formatted_value_for_feature_table_entry(
      Display_Handle *           dh,
      VCP_Feature_Table_Entry *  vcp_entry,
      bool                       suppress_unsupported,
      bool                       prefix_value_with_feature_code,
      char **                    pformatted_value,
      FILE *                     msg_fh)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. suppress_unsupported=%s", bool_repr(suppress_unsupported));

   Public_Status_Code psc = 0;
   *pformatted_value = NULL;

   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   Byte feature_code = vcp_entry->code;
   char * feature_name = get_version_sensitive_feature_name(vcp_entry, vspec);
   bool is_table_feature = is_table_feature_by_display_handle(vcp_entry, dh);
   DDCA_Vcp_Value_Type feature_type = (is_table_feature) ? DDCA_TABLE_VCP_VALUE : DDCA_NON_TABLE_VCP_VALUE;
   DDCA_Output_Level output_level = get_output_level();
   if (output_level >= DDCA_OL_VERBOSE) {
      fprintf(msg_fh, "\nGetting data for %s VCP code 0x%02x - %s:\n",
                            (is_table_feature) ? "table" : "non-table",
                            feature_code,
                            feature_name);
   }

   DDCA_Single_Vcp_Value *    pvalrec = NULL;

   // bool ignore_unsupported = !(output_level >= DDCA_OL_NORMAL && !suppress_unsupported);
   bool ignore_unsupported = suppress_unsupported;

   psc = get_raw_value_for_feature_table_entry(
            dh,
            vcp_entry,
            ignore_unsupported,
            &pvalrec,
            (output_level == DDCA_OL_TERSE) ? NULL : msg_fh);
            // msg_fh);
   assert( (psc==0 && (feature_type == pvalrec->value_type)) || (psc!=0 && !pvalrec) );
   if (psc == 0) {
      // if (!is_table_feature && output_level >= OL_VERBOSE) {
      // if (!is_table_feature && debug) {
      if (output_level >= DDCA_OL_VERBOSE || debug) {
         rpt_push_output_dest(msg_fh);
         // report_single_vcp_value(pvalrec, 0);
         rpt_vstring(0, "Raw value: %s", summarize_single_vcp_value(pvalrec));
         rpt_pop_output_dest();
      }

      if (output_level == DDCA_OL_TERSE) {
         if (is_table_feature) {
            // output VCP code  hex values of bytes
            int bytect = pvalrec->val.t.bytect;
            int hexbufsize = bytect * 3;
            char * hexbuf = calloc(hexbufsize, sizeof(char));
            char space = ' ';
            // n. buffer passed to hexstring2(), so no allocation
            hexstring2(pvalrec->val.t.bytes, bytect, &space, false /* upper case */, hexbuf, hexbufsize);
            char * formatted = calloc(hexbufsize + 20, sizeof(char));
            snprintf(formatted, hexbufsize+20, "VCP %02X T x%s\n", feature_code, hexbuf);
            *pformatted_value = formatted;
            free(hexbuf);
         }
         else {                                // OL_PROGRAM, not table feature
            DDCA_Version_Feature_Flags vflags =
               get_version_sensitive_feature_flags(vcp_entry, vspec);
            char buf[200];
            assert(vflags & (DDCA_CONT | DDCA_SIMPLE_NC | DDCA_COMPLEX_NC));
            if (vflags & DDCA_CONT) {
               snprintf(buf, 200, "VCP %02X C %d %d",
                                  vcp_entry->code,
                                  pvalrec->val.c.cur_val, pvalrec->val.c.max_val);
            }
            else if (vflags & DDCA_SIMPLE_NC) {
               snprintf(buf, 200, "VCP %02X SNC x%02x",
                                   vcp_entry->code, pvalrec->val.nc.sl);
            }
            else {
               assert(vflags & DDCA_COMPLEX_NC);
               snprintf(buf, 200, "VCP %02X CNC x%02x x%02x x%02x x%02x",
                                  vcp_entry->code,
                                  pvalrec->val.nc.mh,
                                  pvalrec->val.nc.ml,
                                  pvalrec->val.nc.sh,
                                  pvalrec->val.nc.sl
                                  );
            }
            *pformatted_value = strdup(buf);
         }
      }

      else  {
         bool ok;
         char * formatted_data = NULL;

         ok = vcp_format_feature_detail(
                 vcp_entry,
                 vspec,
                 pvalrec,
                 &formatted_data);
         // DBGMSG("vcp_format_feature_detail set formatted_data=|%s|", formatted_data);
         if (!ok) {
            f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                            feature_code, feature_name, "!!! UNABLE TO FORMAT OUTPUT");
            psc = DDCRC_INTERPRETATION_FAILED;
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
      }         // normal (non OL_PROGRAM) output
   }

   else {   // error
      // if output_level >= DDCA_OL_NORMAL, get_raw_value_for_feature_table_entry() already issued message
      if (output_level == DDCA_OL_TERSE && !suppress_unsupported) {
         f0printf(msg_fh, "VCP %02X ERR\n", vcp_entry->code);
      }
   }

   if (pvalrec)
      free_single_vcp_value(pvalrec);

   DBGTRC(debug, TRACE_GROUP,
          "Done.  Returning: %s, *pformatted_value=|%s|",
          psc_desc(psc), *pformatted_value);
   return psc;
}


Public_Status_Code
show_feature_set_values(
      Display_Handle *      dh,
      VCP_Feature_Set       feature_set,
      GPtrArray *           collector,     // if null, write to FOUT
      Feature_Set_Flags     flags,
      Byte_Value_Array      features_seen)     // if non-null, collect list of features seen
{
   Public_Status_Code master_status_code = 0;
   bool debug = false;
   char * s0 = feature_set_flag_names(flags);
   DBGMSF(debug, "Starting.  flags=%s, collector=%p", s0, collector);
   free(s0);

   VCP_Feature_Subset subset_id = get_feature_set_subset_id(feature_set);
   DDCA_Output_Level output_level = get_output_level();
   bool show_unsupported = false;
   if ( (flags & FSF_SHOW_UNSUPPORTED)  ||
        output_level >= DDCA_OL_VERBOSE ||
        subset_id == VCP_SUBSET_SINGLE_FEATURE
       )
       show_unsupported = true;
   bool suppress_unsupported = !show_unsupported;

   DDCA_MCCS_Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
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
            DDCA_Version_Feature_Flags vflags = get_version_sensitive_feature_flags(entry, vcp_version);
            char * msg = (vflags & DDCA_DEPRECATED) ? "Deprecated" : "Write-only feature";
            f0printf(FOUT, FMT_CODE_NAME_DETAIL_W_NL,
                          entry->code, feature_name, msg);
         }
      }
      else {
         bool skip_feature = false;
         if (subset_id != VCP_SUBSET_SINGLE_FEATURE &&
             is_feature_table_by_vcp_version(entry, vcp_version) &&
             (flags & FSF_NOTABLE) )
         {
            skip_feature = true;
         }
         if (!skip_feature) {

            char * formatted_value = NULL;
            Public_Status_Code psc =
            get_formatted_value_for_feature_table_entry(
                  dh,
                  entry,
                  suppress_unsupported,
                  prefix_value_with_feature_code,
                  &formatted_value,
                  msg_fh);
            assert( (psc==0 && formatted_value) || (psc!=0 && !formatted_value) );
            if (psc == 0) {
               if (collector)
                  g_ptr_array_add(collector, formatted_value);
               else
                  f0printf(FOUT, "%s\n", formatted_value);
               free(formatted_value);
               if (features_seen)
                  bbf_set(features_seen, entry->code);  // note that feature was read
            }
            else {
               // or should I check features_ct == 1?
               VCP_Feature_Subset subset_id = get_feature_set_subset_id(feature_set);
               if (subset_id == VCP_SUBSET_SINGLE_FEATURE)
                  master_status_code = psc;
               else {
                  if ( (psc != DDCRC_REPORTED_UNSUPPORTED) && (psc != DDCRC_DETERMINED_UNSUPPORTED) ) {
                     if (master_status_code == 0)
                        master_status_code = psc;
                  }
               }
            }
         }   // !skip_feature
      }
      DBGMSF(debug,"ndx=%d, feature = 0x%02x Done", ndx, entry->code);
   }   // loop over features

   DBGMSF(debug, "Returning: %s", psc_desc(master_status_code));
   return master_status_code;
}

#ifdef FUTURE
//typedef bool (*VCP_Feature_Set_Filter_Func)(VCP_Feature_Table_Entry * ventry);
bool hack42(VCP_Feature_Table_Entry * ventry) {
   bool debug = false;
   bool result = true;

   // if (ventry->code >= 0xe0)  {     // is everything promoted to int before comparison?
   if ( (ventry->vcp_global_flags & DDCA_SYNTHETIC) &&
        (ventry->v20_flags & DDCA_NORMAL_TABLE)
      )
   {
      result = false;
      DBGMSF(debug, "Returning false for vcp code 0x%02x", ventry->code);
   }
   return result;
}
#endif


/* Shows the VCP values for all features in a VCP feature subset.
 *
 * Arguments:
 *    dh         display handle for open display
 *    subset     feature subset id
 *    collector  accumulates output    // if null, write to FOUT
 *    flags      feature set flags
 *    features_seen   if non-null, collect ids of features that exist
 *
 * Returns:
 *    status code
 */
Public_Status_Code
show_vcp_values(
        Display_Handle *    dh,
        VCP_Feature_Subset  subset,
        GPtrArray *         collector,    // not used
        Feature_Set_Flags   flags,
        Byte_Bit_Flags      features_seen)
{
   Public_Status_Code psc = 0;
   bool debug = false;
   if (debug) {
      char * s0 = feature_set_flag_names(flags);
      DBGMSG("Starting.  subset=%d, flags=%s,  dh=%s", subset, s0, dh_repr(dh) );
      free(s0);
   }

   DDCA_MCCS_Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   // DBGMSG("VCP version = %d.%d", vcp_version.major, vcp_version.minor);
   VCP_Feature_Set feature_set = create_feature_set(subset, vcp_version);
#ifdef FUTURE
   Parsed_Capabilities * pcaps = NULL;   // TODO: HOW TO GET Parsed_Capabilities?, will only be set for probe/interrogate
   // special case, if scanning, don't try to do a table read of manufacturer specific
   // features if it's clear that table read commands are unavailable

   // convoluted solution to avoid passing additional argument to create_feature_set()
   if (subset == VCP_SUBSET_SCAN && !parsed_capabilities_may_support_table_commands(pcaps)) {
      filter_feature_set(feature_set, hack42);
   }
#endif
   if (debug)
      report_feature_set(feature_set, 0);

   psc = show_feature_set_values(
            dh, feature_set, collector, flags, features_seen);
   free_vcp_feature_set(feature_set);
   DBGMSF0(debug, "Done");
   return psc;
}

