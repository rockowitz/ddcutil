/* ddc_services.c
 *
 * Created on: Nov 15, 2015
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "ddc/ddc_services.h"

// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_DDC;


/* Master initialization function
 */
void init_ddc_services() {
   ddc_reset_write_only_stats();
   ddc_reset_write_read_stats();
   ddc_reset_multi_part_read_stats();
   init_sleep_stats();
   init_execution_stats();

   init_status_code_mgt();
   init_linux_errno();
   init_adl_errors();
   init_vcp_feature_codes();
   // adl_debug = true;      // turn on adl initialization tracing
   adlshim_initialize();
   init_ddc_packets();   // 11/2015: does nothing

   i2c_set_io_strategy(DEFAULT_I2C_IO_STRATEGY);
}


void ddc_show_max_tries(FILE * fh) {
   fprintf(fh, "Maximum Try Settings:\n");
   fprintf(fh, "Operation Type             Current  Default\n");
   fprintf(fh, "Write only exchange tries: %8d %8d\n",
               ddc_get_max_write_only_exchange_tries(),
               MAX_WRITE_ONLY_EXCHANGE_TRIES);
   fprintf(fh, "Write read exchange tries: %8d %8d\n",
               ddc_get_max_write_read_exchange_tries(),
               MAX_WRITE_READ_EXCHANGE_TRIES);
   fprintf(fh, "Multi-part exchange tries: %8d %8d\n",
               ddc_get_max_multi_part_read_tries(),
               MAX_MULTI_EXCHANGE_TRIES);
}

//
// Show VCP value
//

#ifdef OLD

// performs 3 functions:
// - gets vcp value
// - filters out values that should not be shown
// - if not OUTPUT_PROG, writes value, including error messages, to terminal
// returns Preparsed_Nontable_Vcp_Response for use when OUTPUT_PROG
Global_Status_Code get_and_check_nontable_value(
      Display_Handle *          dh,
      VCP_Feature_Table_Entry * vcp_entry,
      bool                      suppress_unsupported,
      // Output_Sink               msg_sink,
      Parsed_Nontable_Vcp_Response ** pcode_info
     )
{
   bool debug = false;
   DBGMSF(debug, "Starting.  feature code = 0x%02x", vcp_entry->code);

   FILE * data_fh = stdout;
   FILE * msg_fh  = stdout;
   Output_Level output_level = get_output_level();
   Byte vcp_code = vcp_entry->code;
   // char * feature_name = vcp_entry->name;
   Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   char * feature_name = get_version_sensitive_feature_name(vcp_entry, vspec);
   if (output_level >= OL_VERBOSE) {
      fprintf(msg_fh, "\nGetting data for VCP code 0x%02x - %s:\n", vcp_code, feature_name);
      // write_sink(msg_sink, "\nGetting data for VCP code 0x%02x - %s:", vcp_code, feature_name);
   }
   Parsed_Nontable_Vcp_Response * code_info = NULL;
   Global_Status_Code rc = get_nontable_vcp_value_by_display_handle(dh, vcp_code, &code_info);
   assert ( (rc==0 && code_info) || (rc!=0 && !code_info) );

      // DBGMSG("get_vcp_by_DisplayRef() returned %p", code_info);

   // if (code_info)
   //    DBGMSG("code_info->valid_response=%d", code_info->valid_response);
   // for unsupported features, some monitors return null response rather than a valid response
   // with unsupported feature indicator set
   if (output_level >= OL_NORMAL) {
      if (rc == DDCRC_NULL_RESPONSE && !suppress_unsupported) {
         fprintf(data_fh,
                 "VCP code 0x%02x (%-30s): Unsupported feature code (Null response)\n",
                 vcp_code, feature_name);
         // write_sink(msg_sink, "VCP code 0x%02x (%-30s): Unsupported feature code (Null response)",
         //                      vcp_code, feature_name);
      }
      else if (rc == DDCRC_INVALID_DATA) {
         fprintf(data_fh,
                 "VCP code 0x%02x (%-30s): Invalid response\n",
                 vcp_code, feature_name);
         // write_sink(msg_sink, "VCP code 0x%02x (%-30s): Invalid response",
         //                      vcp_code, feature_name);
      }
      else if (rc == DDCRC_REPORTED_UNSUPPORTED && !suppress_unsupported) {
         fprintf(data_fh,
                 "VCP code 0x%02x (%-30s): Unsupported feature code\n",
                 vcp_code, feature_name);
         // write_sink(msg_sink, "VCP code 0x%02x (%-30s): Unsupported feature code",
         //                      vcp_code, feature_name);
      }
   }

   else {
      if (output_level >= OL_VERBOSE) {
         rpt_push_output_dest(msg_fh);
         report_interpreted_nontable_vcp_response(code_info);
         rpt_pop_output_dest();
      }
   }

   if (rc == DDCRC_NULL_RESPONSE)
      rc = DDCRC_DETERMINED_UNSUPPORTED;

   *pcode_info = code_info;
   return rc;

// TO HERE
}


Global_Status_Code
dump_nontable_vcp(
      Display_Handle *          dh,
      VCP_Feature_Table_Entry * vcp_entry,
      // Output_Sink               data_sink,
      // Output_Sink               msg_sink)
      GPtrArray * collector)
{
   bool debug = false;
   // Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   //  if (debug)
   //    printf("(%s) Starting. Getting value for feature 0x%02x, dh=%s, vspec=%d.%d\n",
   //           __func__, vcp_entry->code, display_handle_repr(dh), vspec.major, vspec.minor);
   DBGMSF(debug, "Starting. Getting value for feature 0x%02x, dh=%s",
                 vcp_entry->code, display_handle_repr(dh));

   Parsed_Nontable_Vcp_Response * code_info;
   Global_Status_Code gsc =
         get_and_check_nontable_value(
               dh,
               vcp_entry,
               true,       /* suppress_unsupported */
           //    msg_sink,
               &code_info
               );
   if (gsc == 0) {
      assert(code_info);
      char buf[200];
      snprintf(buf, 200, "VCP %02X %5d", vcp_entry->code, code_info->cur_value);
      char * s = strdup(buf);
      // write_sink(data_sink, "VCP %02X %5d", vcp_entry->code, code_info->cur_value);
      assert(collector);
      g_ptr_array_add(collector, s);
      //   free(code_info);   // sometimes causes free failure, crash
      free(code_info);
   }
   return gsc;
}



Global_Status_Code
show_value_for_nontable_feature_table_entry_by_display_handle(
        Display_Handle *          dh,
        VCP_Feature_Table_Entry * vcp_entry,
        GPtrArray *               collector,   // where to write output
        bool                      suppress_unsupported    // if set, do not output unsupported features
     // Output_Sink               data_sink,
     // Output_Sink               msg_sink
      )
{
   bool debug = false;
   // DBGMSF(debug, "Starting. Getting value for feature 0x%02x, dh=%s, vcp_version=%d.%d\n",
   //               vcp_entry->code, display_handle_repr(dh), vcp_version.major, vcp_version.minor);
   DBGMSF(debug, "Starting. Getting value for feature 0x%02x, dh=%s",
                 vcp_entry->code, display_handle_repr(dh));
   Global_Status_Code gsc = 0;

   Output_Level output_level = get_output_level();

   // hack for now:
   if (output_level == OL_PROGRAM) {
      // gsc = dump_nontable_vcp(dh, vcp_entry, data_sink, msg_sink);
      gsc = dump_nontable_vcp(dh, vcp_entry, collector);
   }
   else {
      Parsed_Nontable_Vcp_Response * code_info = NULL;
      gsc = get_and_check_nontable_value(
               dh,
               vcp_entry,
               suppress_unsupported,
         //      msg_sink,
               &code_info
      );

      if (gsc == 0) {
         assert(code_info);
         Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
         char buf[100];
         vcp_format_nontable_feature_detail(vcp_entry, vcp_version, code_info, buf, 100);
         // write_sink(data_sink, "VCP code 0x%02x (%-30s): %s\n",
         //                       vcp_code, feature_name, buf);
         // was this printf or append to collector?
         char * feature_name = get_version_sensitive_feature_name(vcp_entry, vcp_version);
         // if (output_level == OL_PROGRAM) {
         //    printf("VCP %02X %5d", vcp_entry->code, code_info->cur_value);
         // }
         // else {
         printf("VCP code 0x%02x (%-30s): %s\n",
                               vcp_entry->code, feature_name, buf);
         // }
         free(code_info);
      }
   // TO HERE
   }
   DBGMSF(debug, "Done.  Returning %d", gsc);
   // TRCMSG("Done");
   return gsc;
}



// TODO split this out ala get_and_check_nontable_value()

void show_value_for_table_feature_table_entry_by_display_handle(
        Display_Handle *          dh,
        VCP_Feature_Table_Entry * vcp_entry,
        // Version_Spec              vcp_version,
        GPtrArray *               collector,   // where to write output
        bool                      suppress_unsupported    // if set, do not output unsupported features
        // Output_Sink               data_sink,
        // Output_Sink               msg_sink
       )
{
   bool debug = false;
   DBGMSF(debug, "Starting. Getting value for feature 0x%02x, dh=%s\n",
                 vcp_entry->code, display_handle_repr(dh));
   Byte vcp_code = vcp_entry->code;
   // char * feature_name = vcp_entry->name;
   Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   char * feature_name = get_version_sensitive_feature_name(vcp_entry, vspec);
   Output_Level output_level = get_output_level();
   if (output_level >= OL_VERBOSE) {
      printf("\nGetting data for VCP code 0x%02x - %s:\n", vcp_code, feature_name);
      // write_sink(msg_sink, "\nGetting data for VCP code 0x%02x - %s:",
      //                      vcp_code, feature_name);
   }

   Buffer * accumulator = NULL;
   Global_Status_Code rc = get_table_vcp_value_by_display_handle(dh, vcp_code, &accumulator);
   if (rc == DDCRC_NULL_RESPONSE) {
      // if (msgLevel >= NORMAL && outputFormat == OUTPUT_NORMAL)
      if (output_level >= OL_NORMAL && !suppress_unsupported) {
         printf("VCP code 0x%02x (%-30s): Unsupported feature code (Null response)\n", vcp_code, feature_name);
         // write_sink(msg_sink, "VCP code 0x%02x (%-30s): Unsupported feature code (Null response)",
         //                      vcp_code, feature_name);
      }
   }
   else if (rc == DDCRC_RETRIES) {
         printf("VCP code 0x%02x (%-30s): Maximum retries exceeded\n", vcp_code, feature_name);
         // write_sink(msg_sink, "VCP code 0x%02x (%-30s): Maximum retries exceeded",
         //                      vcp_code, feature_name);
      // printf("Error retrieving VCP info.  Failed to interpret returned data.\n");
   }
   else if (rc == DDCRC_REPORTED_UNSUPPORTED || rc == DDCRC_DETERMINED_UNSUPPORTED ) {
      if (!suppress_unsupported)
         printf("VCP code 0x%02x (%-30s): Unsupported feature code\n", vcp_code, feature_name);
         // write_sink(msg_sink, "VCP code 0x%02x (%-30s): Unsupported feature code",
         //                      vcp_code, feature_name);
   }

   // TODO: additional appropriate status codes
   else if (rc != 0) {
      //if (msgLevel >= NORMAL && outputFormat == OUTPUT_NORMAL)
      if (output_level >= OL_NORMAL)
      // TODO: status code name
      printf("VCP code 0x%02x (%-30s): Invalid response. status code=%s\n",
      // write_sink(msg_sink, "VCP code 0x%02x (%-30s): Invalid response. status code=%s",
             vcp_code, feature_name, gsc_desc(rc));
   }

   else {
      // if ( (vcp_entry->flags & VCP_FUNC_VER) && (vcp_version.major == 0) )
      // if ( is_version_unqueried(vcp_version) )
      //   vcp_version = get_vcp_version_by_display_handle(dh);

      if (output_level != OL_PROGRAM) {
         char * formatted_data = NULL;
         bool ok = vcp_format_table_feature_detail(vcp_entry, vspec, accumulator, &formatted_data);
         if (ok) {
            printf("VCP code 0x%02x (%-30s): %s\n",
            // write_sink(data_sink, "VCP code 0x%02x (%-30s): %s\n",
                                 vcp_code, feature_name, formatted_data);
            free(formatted_data);
         }
         else
            printf("VCP code 0x%02x (%-30s): !!! UNABLE TO FORMAT OUTPUT", vcp_code, feature_name);
      }   // OUTPUT_NORNAL
      else {    // OUTPUT_PROG_VCP
         // output VCP code  hex values of bytes
         int hexbufsize = buffer_length(accumulator) * 3;
         char * hexbuf = calloc(hexbufsize, sizeof(char));
         char space = ' ';
         hexstring2(accumulator->bytes, accumulator->len, &space, false /* upper case */, hexbuf, hexbufsize);
         char * workbuf = calloc(hexbufsize + 20, sizeof(char));
         snprintf(workbuf, hexbufsize+20, "VCP %02X %s\n", vcp_code, hexbuf);
         char * s = strdup(workbuf);
         g_ptr_array_add(collector, s);
         // write_sink(data_sink, "VCP %02X %s", vcp_code, hexbuf);
         free(workbuf);
         free(hexbuf);

      }
   }
 // TO HERE
   if (accumulator)
      buffer_free(accumulator, __func__);
   // if (code_info)
   //   free(code_info);   // sometimes causes free failure, crash

   // TRCMSG("Done");
   DBGMSGF(debug, "Done.");
}
#endif

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


#ifdef OLD
void show_value_for_feature_table_entry_by_display_handle(
        Display_Handle *           dh,
        VCP_Feature_Table_Entry *  vcp_entry,
        GPtrArray *                collector,   // where to write output
        bool                       suppress_unsupported
        // Output_Sink                data_sink,
        // Output_Sink                msg_sink
       )
{
   bool debug = false;
   Trace_Group tg = (debug) ? 0xff : TRACE_GROUP;
   TRCMSGTG(tg, "Starting");

   bool use_table_function = is_table_feature_by_display_handle(vcp_entry, dh);

   if (use_table_function) {
      show_value_for_table_feature_table_entry_by_display_handle(
         // dh, vcp_entry, false, data_sink, msg_sink);
         dh, vcp_entry, collector, suppress_unsupported);
   }
   else {
      show_value_for_nontable_feature_table_entry_by_display_handle(
         // dh, vcp_entry,  false, data_sink, msg_sink);
            dh, vcp_entry,  collector, suppress_unsupported);
   }

   TRCMSGTG(tg, "Done");
}
#endif

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
      fprintf(msg_fh, "\nGetting data for VCP code 0x%02x - %s:",
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
      if (output_level >= OL_NORMAL)
         fprintf(msg_fh, "VCP code 0x%02x (%-30s): Invalid response\n",
                         feature_code, feature_name);
      break;

   case DDCRC_NULL_RESPONSE:
      // for unsupported features, some monitors return null response rather than a valid response
      // with unsupported feature indicator set
      if (output_level >= OL_NORMAL && !suppress_unsupported)
         fprintf(msg_fh, "VCP code 0x%02x (%-30s): Unsupported feature code (Null response)\n",
                         feature_code, feature_name);
      gsc = DDCRC_DETERMINED_UNSUPPORTED;
      break;

   case DDCRC_RETRIES:
      fprintf(msg_fh, "VCP code 0x%02x (%-30s): Maximum retries exceeded\n",
                      feature_code, feature_name);
      break;

   case DDCRC_REPORTED_UNSUPPORTED:
   case DDCRC_DETERMINED_UNSUPPORTED:
      if (output_level >= OL_NORMAL && !suppress_unsupported)
         fprintf(msg_fh, "VCP code 0x%02x (%-30s): Unsupported feature code\n",
                         feature_code, feature_name);
      break;

   default:
      if (output_level >= OL_NORMAL)
      fprintf(msg_fh, "VCP code 0x%02x (%-30s): Invalid response. status code=%s\n",
             feature_code, feature_name, gsc_desc(gsc));
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
         // TODO: IMPLEMENT unified vcp_format_feature_detail that takes Parsed_Vcp_Response as argument
         if (is_table_feature) {               // normal (non OL_PROGRAM) output, table
            ok = vcp_format_table_feature_detail(
                                     vcp_entry,
                                     vspec,
                                     parsed_vcp_response->table_response,
                                     &formatted_data);
            // DBGMSG("vcp_format_table_feature_detail set formatted_data=|%s|", formatted_data);
            if (!ok) {
               fprintf(msg_fh, "VCP code 0x%02x (%-30s): !!! UNABLE TO FORMAT OUTPUT",
                               feature_code, feature_name);
               gsc = DDCRC_INTERPRETATION_FAILED;
               // TODO: retry with default output function
            }
         }
         else {                                // normal (non OL_PROGRAM) output, non- table
            Parsed_Nontable_Vcp_Response * code_info = (*parsed_vcp_response).non_table_response;
            assert(code_info);
            formatted_data = calloc(1,100);
            ok = vcp_format_nontable_feature_detail(vcp_entry, vspec, code_info, formatted_data, 100);
            // DBGMSG("vcp_format_nontable_feature_detail set formatted_data=|%s|", formatted_data);
            if (!ok) {
               fprintf(msg_fh, "VCP code 0x%02x (%-30s): !!! UNABLE TO FORMAT OUTPUT",
                               feature_code, feature_name);
               gsc = DDCRC_INTERPRETATION_FAILED;
               // TODO: retry with default output function
            }
         }

         if (ok) {
            if (prefix_value_with_feature_code) {
               *pformatted_value = calloc(1, strlen(formatted_data) + 50);
               snprintf(*pformatted_value, strlen(formatted_data) + 49,
                        "VCP code 0x%02x (%-30s): %s",
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

// duplicate code, ugh!!!

#ifdef UNUSED
void show_single_vcp_value_by_display_handle(Display_Handle * phandle, char * feature, bool force) {
   // char buf[100];
   // DBGMSG("Starting. Getting feature %s for %s", feature,
   //        shortBasicDisplayRef(pdisp, buf, 100) );
   VCP_Feature_Table_Entry * entry = vcp_find_feature_by_charid(feature);
   if (entry) {
      if ( !( (entry->flags) & VCP_READABLE ) ){
         printf("Feature %s (%s) is not readable\n", feature, entry->name);
      }
      else {
         show_value_for_nontable_feature_table_entry_by_display_handle(phandle, entry, stdout, false);
      }
   }
   else if (force) {
      DBGMSG("force specified.  UNIMPLEMENTED" );
   }
   else {
      printf("Unrecognized VCP feature code: %s\n", feature);
   }
}
#endif





void show_feature_set_values_by_display_handle(
      Display_Handle *      dh,
      VCP_Feature_Set       feature_set,
      GPtrArray *           collector)
{
   bool debug = false;
   DBGMSF(debug, "Starting.  collector=%p", collector);
   Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   int features_ct = get_feature_set_size(feature_set);
   VCP_Feature_Subset subset_id = get_feature_set_subset_id(feature_set);  // in anticipation of refactoring
   int ndx;
   Output_Level output_level = get_output_level();
   bool suppress_unsupported = false;
   if (subset_id == SUBSET_SCAN) {
      suppress_unsupported = (output_level < OL_VERBOSE);
   }
   else if (subset_id == SUBSET_SUPPORTED) {
      suppress_unsupported = true;
   }
   bool prefix_value_with_feature_code = true;    // TO FIX
   FILE * msg_fh = stdout;                        // TO FIX
   DBGMSF(debug, "features_ct=%d", features_ct);
   for (ndx=0; ndx< features_ct; ndx++) {
      VCP_Feature_Table_Entry * entry = get_feature_set_entry(feature_set, ndx);
      DBGMSF(debug,"ndx=%d, feature = 0x%02x", ndx, entry->code);
      if (!is_feature_readable_by_vcp_version(entry, vcp_version)) {
         // confuses the output if suppressing unsupported
         if (!suppress_unsupported) {
            char * feature_name =  get_version_sensitive_feature_name(entry, vcp_version);
            printf(standard_feature_format_w_nl,
                   entry->code, feature_name, "Write-only feature");
         }
      }
      else {
         char * formatted_value = NULL;
         // Global_Status_Code gsc =       // unused
         get_formatted_value_for_feature_table_entry(
               dh,
               entry,
               suppress_unsupported,
               prefix_value_with_feature_code,
               &formatted_value,
               msg_fh);
         if (formatted_value) {
            if (collector)
               g_ptr_array_add(collector, formatted_value);
            else
               fprintf(stdout, "%s\n", formatted_value);
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

   DBGMSF(debug, "Done");
}

/* Shows the VCP values for all features in a VCP feature subset.
 *
 * Arguments:
 *    dh         display handle for open display
 *    subset     feature subset
 *    collector  accumulates output
 *
 * Returns:
 *    nothing
 */
void show_vcp_values_by_display_handle(
        Display_Handle *    dh,
        VCP_Feature_Subset  subset,
        GPtrArray *         collector)     // not used
{
   bool debug = false;
   DBGMSF(debug, "Starting.  subset=%d  dh=%s", subset, display_handle_repr(dh) );
   Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   // DBGMSG("VCP version = %d.%d", vcp_version.major, vcp_version.minor);
   VCP_Feature_Set feature_set = create_feature_set(subset, vcp_version);
   if (debug)
      report_feature_set(feature_set, 0);

   show_feature_set_values_by_display_handle(dh, feature_set, collector);
   DBGMSF(debug, "Done");
}



//
//
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


GPtrArray * get_profile_related_values_by_display_handle(Display_Handle* dh) {
   assert( get_output_level() == OL_PROGRAM);
   GPtrArray * vals = g_ptr_array_sized_new(50);

   // bool dumpvcp(Display_Ref * dref, char * filename) {

   char timestamp_buf[30];
   time_t time_millis = time(NULL);
   // temporarily use same output format as filename, but format the date separately here
   // for flexibility
   format_timestamp(time_millis, timestamp_buf, sizeof(timestamp_buf));

   char buf[400];
   int bufsz = sizeof(buf)/sizeof(char);
   char * s;
   snprintf(buf, bufsz, "TIMESTAMP_TEXT %s", timestamp_buf );
   s = strdup(buf);
   g_ptr_array_add(vals, s);
   snprintf(buf, bufsz, "TIMESTAMP_MILLIS %ld", time_millis);
   s = strdup(buf);
   g_ptr_array_add(vals, s);

   Parsed_Edid * edid = ddc_get_parsed_edid_by_display_handle(dh);
   // DBGMSG("strlen(mfg_id)=%ld", strlen(edid->mfg_id));
   snprintf(buf, bufsz, "MFG_ID  %s",  edid->mfg_id);
   // DBGMSG("strlen(buf)=%ld", strlen(buf));
   s = strdup(buf);
   // DBGMSG("strlen(s)=%ld", strlen(s));
   g_ptr_array_add(vals, s);
   snprintf(buf, bufsz, "MODEL   %s",  edid->model_name);
   s = strdup(buf);
   g_ptr_array_add(vals, s);
   snprintf(buf, bufsz, "SN      %s",  edid->serial_ascii);
   s = strdup(buf);
   g_ptr_array_add(vals, s);

   char hexbuf[257];
   hexstring2(edid->bytes, 128,
              NULL /* no separator */,
              true /* uppercase */,
              hexbuf, 257);
   snprintf(buf, bufsz, "EDID    %s", hexbuf);
   s = strdup(buf);
   g_ptr_array_add(vals, s);

   show_vcp_values_by_display_handle(dh, SUBSET_PROFILE, vals);

   return vals;
}



