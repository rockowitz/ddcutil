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
#include <errno.h>
#include <glib.h>
#include <string.h>
#include <time.h>

#include "util/report_util.h"

#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/linux_errno.h"
#include "base/msg_control.h"
#include "base/parms.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_do_io.h"

#include "adl/adl_errors.h"
#include "adl/adl_shim.h"

#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/vcp_feature_codes.h"

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



//
//  Set VCP value
//

/* Converts a VCP feature value from string form to internal form.
 *
 * Currently only handles values in range 0..255.
 *
 * Arguments:
 *    string_value
 *    parsed_value    location where to return result
 *
 * Returns:
 *    true if conversion successful, false if not
 */
bool parse_vcp_value(char * string_value, long* parsed_value) {
   bool ok = true;
   char * endptr = NULL;
   errno = 0;
   long longtemp = strtol(string_value, &endptr, 0 );  // allow 0xdd  for hex values
   int errsv = errno;
   // printf("errno=%d, new_value=|%s|, &new_value=%p, longtemp = %ld, endptr=0x%02x\n",
   //        errsv, new_value, &new_value, longtemp, *endptr);
   if (*endptr || errsv != 0) {
      printf("Not a number: %s", string_value);
      ok = false;
   }
   else if (longtemp < 0 || longtemp > 255) {
      printf("Number must be in range 0..255 (for now at least):  %ld\n", longtemp);
      ok = false;
   }
   else {
      *parsed_value = longtemp;
      ok = true;
   }
   return ok;
}


/* Parses the Set VCP arguments passed and sets the new value.
 *
 * Arguments:
 *   pdisp      display reference
 *   feature    feature id (as string)
 *   new_value  new feature value (as string)
 *
 * Returns:
 *   0          success
 *   -EINVAL (modulated)  invalid setvcp arguments, feature not writable
 *   from put_vcp_by_display_ref()
 */

// TODO: consider moving value parsing to command parser
Global_Status_Code set_vcp_value_top(Display_Ref * pdisp, char * feature, char * new_value) {
   Global_Status_Code my_errno = 0;
   long               longtemp;

   Version_Spec vspec = get_vcp_version_by_display_ref(pdisp);
   VCP_Feature_Table_Entry * entry = vcp_find_feature_by_charid(feature);
   if (entry) {

      // if ( !( (entry->flags) & VCP_WRITABLE ) ){
      if (!is_feature_writable_by_vcp_version(entry, vspec)) {
         // just go with non version specific name to avoid VCP version lookup
         // since we just have Display_Ref here
         char * feature_name =  get_version_specific_feature_name(entry, vspec);
         printf("Feature %s (%s) is not writable\n", feature, feature_name);
         my_errno = modulate_rc(-EINVAL, RR_ERRNO);    // TEMP - what is appropriate?
      }
      else {
         bool good_values = parse_vcp_value(new_value, &longtemp);
         if (!good_values) {
            my_errno = modulate_rc(-EINVAL, RR_ERRNO);
         }
      }
   }
   else {
      printf("Unrecognized VCP feature code: %s\n", feature);
      my_errno = modulate_rc(-EINVAL, RR_ERRNO);
   }

   if (my_errno == 0) {
      my_errno = put_vcp_by_display_ref(pdisp, entry, (int) longtemp);
   }

   return my_errno;
}


//
// Show VCP value
//

#ifdef NOT_USED
// What I really need are curried functions.

typedef int (*VCP_Emitter)(const char * format, ...);

static FILE * vcp_file_emitter_fp = NULL;

int vcp_file_emitter(const char * format, ...) {
   assert(vcp_file_emitter_fp);
   va_list(args);
   va_start(args, format);
   int rc = vfprintf(vcp_file_emitter_fp, format, args);
   return rc;
}

static GPtrArray* vcp_garray_emitter_array = NULL;

int vcp_garray_emitter(const char * format, ...) {
   assert(vcp_garray_emitter_array);
   va_list(args);
   va_start(args, format);
   char buf[400];
   vsnprintf(buf, 400, format, args);
   g_ptr_array_add(vcp_garray_emitter_array, strdup(buf));
}
#endif

// performs 3 functions:
// - gets vcp value
// - filters out values that should not be shown
// - if not OUTPUT_PROG, writes value, including error messages, to terminal
// returns Interpreted_Vcp_Code for use when OUTPUT_PROG
Interpreted_Nontable_Vcp_Response * get_and_filter_vcp_value(
      Display_Handle *          dh,
      VCP_Feature_Table_Entry * vcp_entry,
      bool                      suppress_unsupported
     )
{
   bool debug = false;
   DBGMSF(debug, "Starting.  feature code = 0x%02x", vcp_entry->code);
   Output_Level output_level = get_output_level();
   Byte vcp_code = vcp_entry->code;
   // char * feature_name = vcp_entry->name;
   Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   char * feature_name = get_version_specific_feature_name(vcp_entry, vspec);
   if (output_level >= OL_VERBOSE)
      printf("\nGetting data for VCP code 0x%02x - %s:\n", vcp_code, feature_name);
   Interpreted_Nontable_Vcp_Response * code_info = NULL;
   Global_Status_Code rc = get_nontable_vcp_by_display_handle(dh, vcp_code, &code_info);
   // DBGMSG("get_vcp_by_DisplayRef() returned %p", code_info);

   // if (code_info)
   //    DBGMSG("code_info->valid_response=%d", code_info->valid_response);
   // for unsupported features, some monitors return null response rather than a valid response
   // with unsupported feature indicator set
   if (rc == DDCRC_NULL_RESPONSE) {
      if (output_level >= OL_NORMAL && !suppress_unsupported)
         printf("VCP code 0x%02x (%-30s): Unsupported feature code (Null response)\n", vcp_code, feature_name);
   }
   else if (!code_info) {
      if (output_level >= OL_NORMAL)
         printf("VCP code 0x%02x (%-30s): Unparsable response\n", vcp_code, feature_name);
   }
   else if (!code_info->valid_response) {
      if (output_level >= OL_NORMAL)
         printf("VCP code 0x%02x (%-30s): Invalid response\n", vcp_code, feature_name);
      code_info = NULL;
   }
   else if (!code_info->supported_opcode) {
      if (output_level >= OL_NORMAL && !suppress_unsupported)
         printf("VCP code 0x%02x (%-30s): Unsupported feature code\n", vcp_code, feature_name);
      code_info = NULL;
   }
   else {
      // if interpretation is version dependent and version not already set, get it
      // DBGMSG("vcp_entry->flags=0x%04x", vcp_entry->flags);
      // Version_Spec vcp_version = {0,0};
      // if ( (vcp_entry->flags & VCP_FUNC_VER) )
      Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);

      if (output_level != OL_PROGRAM) {
         char buf[100];
         vcp_format_nontable_feature_detail(vcp_entry, vcp_version, code_info, buf, 100);
         printf("VCP code 0x%02x (%-30s): %s\n", vcp_code, feature_name, buf);
      }   // OUTPUT_NORMAL
   }
   return code_info;
}


void dump_nontable_vcp(
      Display_Handle *          dh,
      VCP_Feature_Table_Entry * vcp_entry,
      GPtrArray * collector)
{
   bool debug = false;
   // Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   //  if (debug)
   //    printf("(%s) Starting. Getting value for feature 0x%02x, dh=%s, vspec=%d.%d\n",
   //           __func__, vcp_entry->code, display_handle_repr(dh), vspec.major, vspec.minor);
   DBGMSF(debug, "Starting. Getting value for feature 0x%02x, dh=%s",
                 vcp_entry->code, display_handle_repr(dh));

   Interpreted_Nontable_Vcp_Response * code_info = get_and_filter_vcp_value(dh, vcp_entry, true /* suppress_unsupported */ );
   if (code_info) {
      char buf[200];
      snprintf(buf, 200, "VCP %02X %5d", vcp_entry->code, code_info->cur_value);
      char * s = strdup(buf);
      g_ptr_array_add(collector, s);
      //   free(code_info);   // sometimes causes free failure, crash
   }
}


void show_vcp_for_nontable_vcp_code_table_entry_by_display_handle(
        Display_Handle *          dh,
        VCP_Feature_Table_Entry * vcp_entry,
        // Version_Spec              vcp_version,   // will be set for scan operations, not set for single
        GPtrArray *               collector,   // where to write output
        bool                      suppress_unsupported)    // if set, do not output unsupported features
{
   bool debug = false;
   // DBGMSF(debug, "Starting. Getting value for feature 0x%02x, dh=%s, vcp_version=%d.%d\n",
   //               vcp_entry->code, display_handle_repr(dh), vcp_version.major, vcp_version.minor);
   DBGMSF(debug, "Starting. Getting value for feature 0x%02x, dh=%s",
                 vcp_entry->code, display_handle_repr(dh));


   Output_Level output_level = get_output_level();
   // hack for now:
   if (output_level == OL_PROGRAM) {
      dump_nontable_vcp(dh, vcp_entry, collector);
   }
   else {
      // no longer using vcp_version parm, picking it up from display_handle

    /*  Interpreted_Vcp_Code * code_info = */ get_and_filter_vcp_value(dh, vcp_entry, suppress_unsupported);

      // if (code_info)
      //   free(code_info);   // sometimes causes free failure, crash
   }
   DBGMSF(debug, "Done");
   // TRCMSG("Done");
}


// TODO split this out ala get_and_filter_vcp_value()

void show_vcp_for_table_vcp_code_table_entry_by_display_handle(
        Display_Handle *          dh,
        VCP_Feature_Table_Entry * vcp_entry,
        // Version_Spec              vcp_version,
        GPtrArray *               collector,   // where to write output
        bool                      suppress_unsupported)    // if set, do not output unsupported features
{
   bool debug = false;
   DBGMSF(debug, "Starting. Getting value for feature 0x%02x, dh=%s\n",
                 vcp_entry->code, display_handle_repr(dh));
   Byte vcp_code = vcp_entry->code;
   // char * feature_name = vcp_entry->name;
   Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   char * feature_name = get_version_specific_feature_name(vcp_entry, vspec);
   Output_Level output_level = get_output_level();
   if (output_level >= OL_VERBOSE)
      printf("\nGetting data for VCP code 0x%02x - %s:\n", vcp_code, feature_name);

   Buffer * accumulator = NULL;

   Global_Status_Code rc = get_table_vcp_by_display_handle(dh, vcp_code, &accumulator);
   if (rc == DDCRC_NULL_RESPONSE) {
      // if (msgLevel >= NORMAL && outputFormat == OUTPUT_NORMAL)
      if (output_level >= OL_NORMAL && !suppress_unsupported)
         printf("VCP code 0x%02x (%-30s): Unsupported feature code (Null response)\n", vcp_code, feature_name);
   }
   else if (rc == DDCRC_RETRIES) {
         printf("VCP code 0x%02x (%-30s): Maximum retries exceeded\n", vcp_code, feature_name);
      // printf("Error retrieving VCP info.  Failed to interpret returned data.\n");
   }
   else if (rc == DDCRC_REPORTED_UNSUPPORTED || rc == DDCRC_DETERMINED_UNSUPPORTED ) {
      if (!suppress_unsupported)
         printf("VCP code 0x%02x (%-30s): Unsupported feature code\n", vcp_code, feature_name);
   }

   // TODO: additional appropriate status codes
   else if (rc != 0) {
      //if (msgLevel >= NORMAL && outputFormat == OUTPUT_NORMAL)
      if (output_level >= OL_NORMAL)
      // TODO: status code name
      printf("VCP code 0x%02x (%-30s): Invalid response. status code=%s\n",
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
#ifdef REFERENCE
         char * hexstring2(
                   const unsigned char * bytes,      // bytes to convert
                   int                   len,        // number of bytes
                   const char *          sep,        // separator character (used how?)
                   bool                  uppercase,  // use upper case hex characters?
                   char *                buffer,     // buffer in which to return hex string
                   int                   bufsz);     // buffer size
#endif

         // int cv = code_info->cur_value;
         // int mv = code_info->max_value;
         char * workbuf = calloc(hexbufsize + 20, sizeof(char));
         snprintf(workbuf, hexbufsize+20, "VCP %02X %s\n", vcp_code, hexbuf);
         char * s = strdup(workbuf);
         g_ptr_array_add(collector, s);
         free(workbuf);
         free(hexbuf);

      }
   }
   if (accumulator)
      buffer_free(accumulator, __func__);
   // if (code_info)
   //   free(code_info);   // sometimes causes free failure, crash

   // TRCMSG("Done");
   if (debug)
      DBGMSG("Done.");
}



bool is_table_feature_by_display_handle(
        VCP_Feature_Table_Entry *  vcp_entry,
        Display_Handle *           dh)
{
   bool debug = false;

   bool result = false;

   // for now, just get the vcp_version even though its probably not
   // needed to test for table
   Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);

   Version_Feature_Flags feature_flags = get_version_specific_feature_flags(vcp_entry, vcp_version);
   assert(feature_flags);
#ifdef OLD
   if (feature_flags) {
      // DBGMSF(debug, "using result of get_feature_flags() to test for table");
      result = (feature_flags & VCP2_TABLE);
   }
   else {
      // fallback to old way
      DBGMSF(debug, "using old flags byte to test for table");
      if (vcp_entry->flags & VCP_TYPE_V2NC_V3T) {
         vcp_version = get_vcp_version_by_display_handle(dh);
         if (vcp_version.major >= 3)
            result = true;
      }
      else if (vcp_entry->flags & VCP_TABLE)
         result = true;
   }
#endif
   result = (feature_flags & VCP2_TABLE);
   // DBGMSF(debug, "returning: %d", result);
   return result;

}





void show_vcp_for_vcp_code_table_entry_by_display_handle(
        Display_Handle *           dh,
        VCP_Feature_Table_Entry *  vcp_entry,
        GPtrArray *                collector)   // where to write output
{
   bool debug = false;
   Trace_Group tg = TRACE_GROUP;
   if (debug)
      tg = 0xff;
   TRCMSGTG(tg, "Starting");
   // Version_Spec vcp_version = {0,0};

   bool use_table_function = is_table_feature_by_display_handle(vcp_entry, dh);

   if (use_table_function) {
      show_vcp_for_table_vcp_code_table_entry_by_display_handle(
         dh, vcp_entry, collector, false);
   }
   else {
      show_vcp_for_nontable_vcp_code_table_entry_by_display_handle(
         dh, vcp_entry,  collector, false);
   }

   TRCMSGTG(tg, "Done");
}



void show_vcp_for_vcp_code_table_entry_by_display_ref(
        Display_Ref *              dref,
        VCP_Feature_Table_Entry *  vcp_entry,
        GPtrArray *                collector)   // where to write output
{
   bool debug = false;
   DBGMSF(debug, "Starting");

   Display_Handle * dh = ddc_open_display(dref, EXIT_IF_FAILURE);
   show_vcp_for_vcp_code_table_entry_by_display_handle(dh, vcp_entry, collector);
   ddc_close_display(dh);

   DBGMSF(debug, "Done");
}


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
         show_vcp_for_nontable_vcp_code_table_entry_by_display_handle(phandle, entry, stdout, false);
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



void show_single_vcp_value_by_display_ref(Display_Ref * dref, char * feature, bool force) {
   bool debug = false;
   if (debug) {
      printf("(%s) Starting. Getting feature %s for %s\n",
             __func__, feature,
             display_ref_short_name(dref) );
   }
   VCP_Feature_Table_Entry * entry = vcp_find_feature_by_charid(feature);
   bool showit = true;

   Version_Spec vspec = get_vcp_version_by_display_ref(dref);
   if (entry) {
      // if ( !( (entry->flags) & VCP_READABLE ) ){
      if (!is_feature_readable_by_vcp_version(entry, vspec)) {
         char * feature_name =  get_version_specific_feature_name(entry, vspec);
         printf("Feature %s (%s) is not readable\n", feature, feature_name);
         showit = false;
      }
   }
   else if (force) {
      // DBGMSG("force specified.  UNIMPLEMENTED" );
      entry = vcp_create_dummy_feature_for_charid(feature);    // issues error message if invalid hex
      if (!entry) {
         showit = false;
         printf("Invalid feature code: %s\n", feature);  // i.e. invalid hex value
      }
   }
   else {
      printf("Unrecognized VCP feature code: %s\n", feature);
      showit = false;
   }

   // DBGMSG("showit=%d", showit);
   if (showit) {
      // DBGMSG("calling show_vcp_for_vcp_code_table_entry_by_display_ref()");
      show_vcp_for_vcp_code_table_entry_by_display_ref(dref, entry, NULL);
   }

   if (debug)
      DBGMSG("Done");
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
        GPtrArray *         collector)
{
   bool debug = false;
   DBGMSF(debug, "Starting.  subset=%d  dh=%s", subset, display_handle_repr(dh) );

   // For collections of feature codes, just assume that at least one of them
   // will need the version number for proper interpretation.
   // TODO: verify lookup always occurs in called functions and eliminate parm
   Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   // DBGMSG("VCP version = %d.%d", vcp_version.major, vcp_version.minor);

   if (subset == SUBSET_SCAN) {
      int ndx = 0;
      for (ndx=0; ndx <= 255; ndx++) {
         Byte id = ndx;
         // DBGMSG("ndx=%d, id=0x%02x", ndx, id);
         VCP_Feature_Table_Entry * entry = vcp_find_feature_by_hexid_w_default(id);
         bool suppress_unsupported = (get_output_level() < OL_VERBOSE);
         // if ( !( (entry->flags) & VCP_READABLE ) ){
         if (!is_feature_readable_by_vcp_version(entry, vcp_version)) {
            // confuses the output if suppressing unsupported
            if (!suppress_unsupported) {
               char * feature_name =  get_version_specific_feature_name(entry, vcp_version);
               printf("Feature 0x%02x (%s) is not readable\n", ndx, feature_name);
            }
         }
         else {
            bool use_table_function = is_table_feature_by_display_handle(entry, dh);
            if (use_table_function) {
               show_vcp_for_table_vcp_code_table_entry_by_display_handle(
                  dh,
                  entry,
                  // vcp_version,
                  collector,
                  suppress_unsupported);              // suppress unsupported features
            }
            else {
               show_vcp_for_nontable_vcp_code_table_entry_by_display_handle(
                  dh,
                  entry,
                  // vcp_version,
                  collector,
                  suppress_unsupported);   //  suppress unsupported features
            }
         }
      }
   }
   else {
      int ndx = 0;
      int vcp_feature_code_count = vcp_get_feature_code_count();
      for (ndx=0; ndx < vcp_feature_code_count; ndx++) {
         VCP_Feature_Table_Entry * vcp_entry = vcp_get_feature_table_entry(ndx);
         assert(vcp_entry != NULL);
         Version_Feature_Flags vflags = get_version_specific_feature_flags(vcp_entry, vcp_version);
         // if (vcp_entry->flags & VCP_READABLE) {
         if (vflags & VCP2_READABLE) {
            bool showIt = true;      //
            switch (subset) {
            case SUBSET_ALL:       showIt = true;                              break;
            case SUBSET_SUPPORTED: showIt = true;                              break;
            // case SUBSET_COLORMGT:  showIt = vcp_entry->flags & VCP_COLORMGT;   break;
            // case SUBSET_PROFILE:   showIt = vcp_entry->flags & VCP_PROFILE;    break;
            case SUBSET_COLORMGT:  showIt = vflags & VCP2_COLORMGT;   break;
            case SUBSET_PROFILE:   showIt = vflags & VCP2_PROFILE;    break;
            case SUBSET_SCAN:  // will never happen, inserted to avoid compiler warning
            default: PROGRAM_LOGIC_ERROR("subset=%d", subset);
            };

           if (showIt) {
              bool is_table_feature = is_table_feature_by_display_handle(vcp_entry, dh);
              if (is_table_feature) {
                 show_vcp_for_table_vcp_code_table_entry_by_display_handle(
                    dh,
                    vcp_entry,
                    // vcp_version,
                    collector,
                    (subset==SUBSET_SUPPORTED) );    // suppress unsupported features

              }
              else {
                 show_vcp_for_nontable_vcp_code_table_entry_by_display_handle(
                     dh,
                     vcp_entry,
                     // vcp_version,
                     collector,
                     (subset==SUBSET_SUPPORTED));    // suppress_unsupported
              }
           }
         }
      }
   }

   if (debug)
      DBGMSG("Done");
}


/* Shows the VCP values for all features in a VCP feature subset.
 *
 * Arguments:
 *    pdisp      display reference
 *    subset     feature subset
 *    collector  accumulates output
 *
 * Returns:
 *    nothing
 */
void show_vcp_values_by_display_ref(Display_Ref * dref, VCP_Feature_Subset subset, GPtrArray * collector) {
   // DBGMSG("Starting.  subset=%d   ", subset );
   // need to ensure that bus info initialized
   bool validDisp = true;
   if (dref->ddc_io_mode == DDC_IO_DEVI2C) {
      // Is this needed?  or checked by openDisplay?
      Bus_Info * bus_info = i2c_get_bus_info(dref->busno);
      if (!bus_info ||  !(bus_info->flags & I2C_BUS_ADDR_0X37) ) {
         printf("Address 0x37 not detected on bus %d. I2C communication not available.\n", dref->busno );
         validDisp = false;
      }
   }
   else {
      validDisp = true;    // already checked
   }

   if (validDisp) {
      Display_Handle * pDispHandle = ddc_open_display(dref, EXIT_IF_FAILURE);
      show_vcp_values_by_display_handle(pDispHandle, subset, collector);
      ddc_close_display(pDispHandle);
   }
}



//
// Functions for VCP (MCCS) version
//

/* Gets the VCP version.
 *
 * Because the VCP version is used repeatedly for interpreting other
 * VCP feature values, it is cached.
 *
 * Arguments:
 *    dh     display handle
 *
 * Returns:
 *    Version_Spec struct containing version, contains 0.0 if version
 *    could not be retrieved (pre MCCS v2)
 */
Version_Spec get_vcp_version_by_display_handle(Display_Handle * dh) {
   bool debug = false;
   // printf("(%s) Starting. dh=%p, dh->vcp_version =  %d.%d\n",
   //        __func__, dh, dh->vcp_version.major, dh->vcp_version.minor);
   if (is_version_unqueried(dh->vcp_version)) {
      dh->vcp_version.major = 0;
      dh->vcp_version.minor = 0;
      Interpreted_Nontable_Vcp_Response * pinterpreted_code;

      // verbose output is distracting since this function is called when
      // querying for other things
      Output_Level olev = get_output_level();
      if (olev == OL_VERBOSE)
         set_output_level(OL_NORMAL);
      Global_Status_Code  gsc = get_nontable_vcp_by_display_handle(dh, 0xdf, &pinterpreted_code);
      if (olev == OL_VERBOSE)
         set_output_level(olev);
      if (gsc == 0) {
         dh->vcp_version.major = pinterpreted_code->sh;
         dh->vcp_version.minor = pinterpreted_code->sl;
      }
      else {
         // happens for pre MCCS v2 monitors
         DBGMSF(debug, "Error detecting VCP version. gsc=%s\n", gsc_desc(gsc) );
      }
   }
   // DBGMSG("Returning: %d.%d", dh->vcp_version.major, dh->vcp_version.minor);
   return dh->vcp_version;
}


/* Gets the VCP version.
 *
 * Because the VCP version is used repeatedly for interpreting other
 * VCP feature values, it is cached.
 *
 * Arguments:
 *    dref     display reference
 *
 * Returns:
 *    Version_Spec struct containing version, contains 0.0 if version
 *    could not be retrieved (pre MCCS v2)
 */
Version_Spec get_vcp_version_by_display_ref(Display_Ref * dref) {
   // printf("(%s) Starting. dref=%p, dref->vcp_version =  %d.%d\n",
   //        __func__, dref, dref->vcp_version.major, dref->vcp_version.minor);

   if (is_version_unqueried(dref->vcp_version)) {
      Display_Handle * dh = ddc_open_display(dref, EXIT_IF_FAILURE);
      dref->vcp_version = get_vcp_version_by_display_handle(dh);
      ddc_close_display(dh);
   }

   // DBGMSG("Returning: %d.%d", dref->vcp_version.major, vspec.minor);
   return dref->vcp_version;
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


GPtrArray * get_profile_related_values_by_display_ref(Display_Ref * dref) {
   Display_Handle* dh = ddc_open_display(dref, EXIT_IF_FAILURE);
   GPtrArray * vals = get_profile_related_values_by_display_handle(dh);
   ddc_close_display(dh);
   return vals;
}


void ddc_show_max_tries() {
   printf("Maximum Try Settings:\n");
   printf("Operation Type             Current  Default\n");
   printf("Write only exchange tries: %8d %8d\n",
          ddc_get_max_write_only_exchange_tries(),
          MAX_WRITE_ONLY_EXCHANGE_TRIES);
   printf("Write read exchange tries: %8d %8d\n",
          ddc_get_max_write_read_exchange_tries(),
          MAX_WRITE_READ_EXCHANGE_TRIES);
   printf("Multi-part exchange tries: %8d %8d\n",
          ddc_get_max_multi_part_read_tries(),
          MAX_MULTI_EXCHANGE_TRIES);
   printf("\nMaximum value allowed for any setting: %d\n", MAX_MAX_TRIES);
}
