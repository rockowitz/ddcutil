/* ddc_output.c
 *
 * Created on: Nov 15, 2015
 *     Author: rock
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

#include <assert.h>
#include <ddc/ddc_output.h>
#include <ddc/vcp_feature_set.h>
#include <errno.h>
#include <glib.h>
#include <string.h>
#include <time.h>

#include "util/output_sink.h"
#include "util/report_util.h"

#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/linux_errno.h"
#include "base/msg_control.h"
#include "base/parms.h"
#include "base/status_code_mgt.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_do_io.h"

#include "adl/adl_errors.h"
#include "adl/adl_shim.h"

#include "ddc/ddc_edid.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_vcp_version.h"


// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_DDC;

//
// Show VCP value
//

bool is_table_feature_by_display_handle(
        VCP_Feature_Table_Entry *  vcp_entry,
        Display_Handle *           dh)
{
   // bool debug = false;
   bool result = false;
   Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   Version_Feature_Flags feature_flags = get_version_sensitive_feature_flags(vcp_entry, vcp_version);
   assert(feature_flags);
   result = (feature_flags & VCP2_TABLE);
   // DBGMSF(debug, "returning: %d", result);
   return result;
}


// For possible future use - currently unused
Global_Status_Code
check_valid_operation_by_feature_rec_and_version(
      VCP_Feature_Table_Entry * feature_rec,
      Version_Spec              vcp_version,
      Version_Feature_Flags     operation_flags)
{
   Version_Feature_Flags feature_flags
      = get_version_specific_feature_flags(feature_rec, vcp_version);
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
   Trace_Group tg = (debug) ? 0xff : TRACE_GROUP;
   TRCMSGTG(tg, "Starting");

   Global_Status_Code gsc = 0;

   *pformatted_value = NULL;
   Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   Byte feature_code = vcp_entry->code;
   char * feature_name = get_version_sensitive_feature_name(vcp_entry, vspec);
   bool is_table_feature = is_table_feature_by_display_handle(vcp_entry, dh);
   VCP_Call_Type feature_type = (is_table_feature) ? TABLE_VCP_CALL : NON_TABLE_VCP_CALL;
   Output_Level output_level = get_output_level();
   if (output_level >= OL_VERBOSE) {
      fprintf(msg_fh, "\nGetting data for VCP code 0x%02x - %s:\n",
                            feature_code, feature_name);
   }
   Parsed_Vcp_Response * parsed_vcp_response;
   gsc = get_vcp_value_by_display_handle(
           dh,
           feature_code,
           feature_type,
           &parsed_vcp_response);
   // assert ( (gsc==0 && parsed_vcp_response) || (gsc!=0 && !parsed_vcp_response) );

   switch(gsc) {
   case 0:
      break;

   case DDCRC_INVALID_DATA:
      if (output_level >= OL_NORMAL)    // FMT_CODE_NAME_DETAIL_W_NL
         // fprintf(msg_fh, "VCP code 0x%02x (%-30s): Invalid response\n",
         //                 feature_code, feature_name);
         fprintf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                      feature_code, feature_name, "Invalid response");

      break;

   case DDCRC_NULL_RESPONSE:
      // for unsupported features, some monitors return null response rather than a valid response
      // with unsupported feature indicator set
      if (output_level >= OL_NORMAL && !suppress_unsupported)
         // fprintf(msg_fh, "VCP code 0x%02x (%-30s): Unsupported feature code (Null response)\n",
         //                 feature_code, feature_name);
         fprintf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                        feature_code, feature_name, "Unsupported feature code (Null response)");
      gsc = DDCRC_DETERMINED_UNSUPPORTED;
      break;

   case DDCRC_RETRIES:
      // fprintf(msg_fh, "VCP code 0x%02x (%-30s): Maximum retries exceeded\n",
      //                 feature_code, feature_name);
      fprintf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                      feature_code, feature_name, "Maximum retries exceeded");
      break;

   case DDCRC_REPORTED_UNSUPPORTED:
   case DDCRC_DETERMINED_UNSUPPORTED:
      if (output_level >= OL_NORMAL && !suppress_unsupported)
         // fprintf(msg_fh, "VCP code 0x%02x (%-30s): Unsupported feature code\n",
         //                 feature_code, feature_name);
         fprintf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                         feature_code, feature_name, "Unsupported feature code");
      break;

   default:
      if (output_level >= OL_NORMAL) {
         char buf[200];
         snprintf(buf, 200, "Invalid response. status code=%s", gsc_desc(gsc));
         // fprintf(msg_fh, "VCP code 0x%02x (%-30s): Invalid response. status code=%s\n",
         //     feature_code, feature_name, gsc_desc(gsc));
         fprintf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                         feature_code, feature_name, buf);
      }
   }

   // if (gsc == 0)
   //    report_parsed_vcp_response(parsed_vcp_response, 0);

   assert( (gsc==0 && (feature_type == parsed_vcp_response->response_type)) || (gsc!=0) );
   if (gsc == 0) {
      if (!is_table_feature && output_level >= OL_VERBOSE) {
         rpt_push_output_dest(msg_fh);
         Parsed_Nontable_Vcp_Response *  non_table_response = (*parsed_vcp_response).non_table_response;
         report_interpreted_nontable_vcp_response(non_table_response, 0);
         rpt_pop_output_dest();
      }

      if (output_level == OL_PROGRAM) {
         if (is_table_feature) {                // OL_PROGRAM, is table feature
            // output VCP code  hex values of bytes
            Buffer * accumulator = (*parsed_vcp_response).table_response;
            int hexbufsize = buffer_length(accumulator) * 3;
            char * hexbuf = calloc(hexbufsize, sizeof(char));
            char space = ' ';
            hexstring2(accumulator->bytes, accumulator->len, &space, false /* upper case */, hexbuf, hexbufsize);
            char * formatted = calloc(hexbufsize + 20, sizeof(char));
            snprintf(formatted, hexbufsize+20, "VCP %02X %s\n", feature_code, hexbuf);
            *pformatted_value = formatted;
            free(hexbuf);
         }
         else {                                // OL_PROGRAM, not table feature
            Parsed_Nontable_Vcp_Response * code_info = (*parsed_vcp_response).non_table_response;
            assert(code_info);
            char buf[200];
            snprintf(buf, 200, "VCP %02X %5d", vcp_entry->code, code_info->cur_value);
            *pformatted_value = strdup(buf);
         }
      }
      else  {          // normal (non OL_PROGRAM) output
         bool ok;
         char * formatted_data = NULL;

         ok = vcp_format_feature_detail(
                 vcp_entry,
                 vspec,
                 parsed_vcp_response,
                 &formatted_data);
         // DBGMSG("vcp_format_feature_detail set formatted_data=|%s|", formatted_data);
         if (!ok) {
            fprintf(msg_fh, FMT_CODE_NAME_DETAIL_WO_NL,
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

   // if (*parsed_vcp_response)
   // free_parsed_vcp_response(*parsed_vcp_response);     // TODO: implement

   TRCMSGTG(tg, "Done.  Returning: %s, *pformatted_value=|%s|", gsc_desc(gsc), *pformatted_value);
   return gsc;
}


#ifdef UNUSED
void show_value_for_feature_table_entry_by_display_ref(
        Display_Ref *              dref,
        VCP_Feature_Table_Entry *  vcp_entry,
        // Output_Sink                data_sink,
        // Output_Sink                msg_sink)
        GPtrArray *                collector)   // where to write output
{
   bool debug = false;
   DBGMSF(debug, "Starting");

   Display_Handle * dh = ddc_open_display(dref, EXIT_IF_FAILURE);
   show_value_for_feature_table_entry_by_display_handle(
         // dh, vcp_entry, data_sink, msg_sink);
         dh, vcp_entry, collector, false);
   ddc_close_display(dh);

   DBGMSF(debug, "Done");
}
#endif


Global_Status_Code
show_feature_set_values_by_display_handle(
      Display_Handle *      dh,
      VCP_Feature_Set       feature_set,
      GPtrArray *           collector,     // if null, write to stdout
      bool                  force_show_unsupported)
{
   Global_Status_Code master_status_code = 0;
   bool debug = false;
   DBGMSF(debug, "Starting.  collector=%p", collector);
   Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   int features_ct = get_feature_set_size(feature_set);
   VCP_Feature_Subset subset_id = get_feature_set_subset_id(feature_set);
   int ndx;
   Output_Level output_level = get_output_level();
   bool show_unsupported = false;
   if ( force_show_unsupported     ||
        output_level >= OL_VERBOSE ||
        subset_id == VCP_SUBSET_SINGLE_FEATURE
       )
       show_unsupported = true;

   bool suppress_unsupported = !show_unsupported;
   bool prefix_value_with_feature_code = true;    // TO FIX
   FILE * msg_fh = stdout;                        // TO FIX
   DBGMSF(debug, "features_ct=%d", features_ct);
   for (ndx=0; ndx< features_ct; ndx++) {
      VCP_Feature_Table_Entry * entry = get_feature_set_entry(feature_set, ndx);
      DBGMSF(debug,"ndx=%d, feature = 0x%02x", ndx, entry->code);
      if (!is_feature_readable_by_vcp_version(entry, vcp_version)) {
         // confuses the output if suppressing unsupported
         if (show_unsupported) {
            char * feature_name =  get_version_sensitive_feature_name(entry, vcp_version);
            Version_Feature_Flags vflags = get_version_sensitive_feature_flags(entry, vcp_version);
            char * msg = (vflags & VCP2_DEPRECATED) ? "Deprecated" : "Write-only feature";
            printf(FMT_CODE_NAME_DETAIL_W_NL,
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
               fprintf(stdout, "%s\n", formatted_value);
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

#ifdef EVEN_NEWER_OLD
         show_value_for_feature_table_entry_by_display_handle(
                 dh,
                 entry,
                 collector,   // where to write output
                 suppress_unsupported
                );
#endif

#ifdef THE_NEW_OLD
         bool is_table_feature = is_table_feature_by_display_handle(entry, dh);
         if (is_table_feature) {
            show_value_for_table_feature_table_entry_by_display_handle(
               dh,
               entry,
               // vcp_version,
               collector,
               suppress_unsupported);              // suppress unsupported features
         }
         else {
            show_value_for_nontable_feature_table_entry_by_display_handle(
               dh,
               entry,
               // vcp_version,
               collector,
               suppress_unsupported);   //  suppress unsupported features
         }
#endif
      }
   }   // loop over features

   DBGMSF(debug, "Returning: %s", gsc_desc(master_status_code));
   return master_status_code;
}

/* Shows the VCP values for all features in a VCP feature subset.
 *
 * Arguments:
 *    dh         display handle for open display
 *    subset     feature subset
 *    collector  accumulates output    // if null, write to stdout
 *
 * Returns:
 *    status code
 */
Global_Status_Code
show_vcp_values_by_display_handle(
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

   gsc = show_feature_set_values_by_display_handle(
            dh, feature_set, collector, force_show_unsupported);
   DBGMSF(debug, "Done");
   return gsc;
}


//
// Support for dumpvcp command and returning profile info as string in API
//


char * format_timestamp(time_t time_millis, char * buf, int bufsz) {
   if (bufsz == 0 || buf == NULL) {
      bufsz = 128;
      buf = calloc(1, bufsz);
   }
   struct tm tm = *localtime(&time_millis);
   snprintf(buf, bufsz, "%4d%02d%02d-%02d%02d%02d",
                  tm.tm_year+1900,
                  tm.tm_mon+1,
                  tm.tm_mday,
                  tm.tm_hour,
                  tm.tm_min,
                  tm.tm_sec
                 );
   return buf;
}


void collect_machine_readable_monitor_id(Display_Handle * dh, GPtrArray * vals) {
   char buf[400];
   int bufsz = sizeof(buf)/sizeof(char);

   Parsed_Edid * edid = ddc_get_parsed_edid_by_display_handle(dh);
   snprintf(buf, bufsz, "MFG_ID  %s",  edid->mfg_id);
   g_ptr_array_add(vals, strdup(buf));
   snprintf(buf, bufsz, "MODEL   %s",  edid->model_name);
   g_ptr_array_add(vals, strdup(buf));
   snprintf(buf, bufsz, "SN      %s",  edid->serial_ascii);
   g_ptr_array_add(vals, strdup(buf));

   char hexbuf[257];
   hexstring2(edid->bytes, 128,
              NULL /* no separator */,
              true /* uppercase */,
              hexbuf, 257);
   snprintf(buf, bufsz, "EDID    %s", hexbuf);
   g_ptr_array_add(vals, strdup(buf));
}


void collect_machine_readable_timestamp(time_t time_millis, GPtrArray* vals) {
   // temporarily use same output format as filename, but format the
   // date separately herefor flexibility
   char timestamp_buf[30];
   format_timestamp(time_millis, timestamp_buf, sizeof(timestamp_buf));
   char buf[400];
   int bufsz = sizeof(buf)/sizeof(char);
   snprintf(buf, bufsz, "TIMESTAMP_TEXT %s", timestamp_buf );
   g_ptr_array_add(vals, strdup(buf));

   snprintf(buf, bufsz, "TIMESTAMP_MILLIS %ld", time_millis);
   g_ptr_array_add(vals, strdup(buf));
}


Global_Status_Code
collect_profile_related_values_by_display_handle(
      Display_Handle*  dh,
      time_t           timestamp_millis,
      GPtrArray**      pvals)
{
   assert( get_output_level() == OL_PROGRAM);
   Global_Status_Code gsc = 0;
   GPtrArray * vals = g_ptr_array_sized_new(50);

   collect_machine_readable_timestamp(timestamp_millis, vals);
   collect_machine_readable_monitor_id(dh, vals);
   gsc = show_vcp_values_by_display_handle(
            dh,
            VCP_SUBSET_PROFILE,
            vals,
            false /* force_show_unsupported */);
   *pvals = vals;
   return gsc;
}

