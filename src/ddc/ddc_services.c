/*  ddc_services.c
 *
 *  Created on: Nov 15, 2015
 *      Author: rock
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
//  Display Specification
//

/* Converts the display identifiers passed on the command line to a logical
 * identifier for an I2C or ADL display.  If a bus number or ADL adapter.display
 * number is specified, the translation is direct.  If a model name/serial number
 * pair or an EDID is specified, the attached displays are searched.
 *
 * Arguments:
 *    pdid      display identifiers
 *    emit_error_msg
 * OLD   validate  if searching was not necessary, validate that that bus number or
 * OLD             ADL number does in fact reference an attached display
 *
 * Returns:
 *    DisplayRef instance specifying the display using either an I2C bus number
 *    or an ADL adapter.display number, NULL if display not found
 */
Display_Ref* get_display_ref_for_display_identifier(Display_Identifier* pdid, bool emit_error_msg) {
   Display_Ref* dref = NULL;
   bool validated = true;

   switch (pdid->id_type) {
   case DISP_ID_DISPNO:
      dref = ddc_find_display_by_dispno(pdid->dispno);
      if (!dref && emit_error_msg) {
         fprintf(stderr, "Invalid display number\n");
      }
      validated = false;
      break;
   case DISP_ID_BUSNO:
      dref = create_bus_display_ref(pdid->busno);
      validated = false;
      break;
   case DISP_ID_ADL:
      dref = create_adl_display_ref(pdid->iAdapterIndex, pdid->iDisplayIndex);
      validated = false;
      break;
   case DISP_ID_MONSER:
      dref = ddc_find_display_by_model_and_sn(pdid->model_name, pdid->serial_ascii);  // in ddc_packet_io
      if (!dref && emit_error_msg) {
         fprintf(stderr, "Unable to find monitor with the specified model and serial number\n");
      }
      break;
   case DISP_ID_EDID:
      dref = ddc_find_display_by_edid(pdid->edidbytes);
      if (!dref && emit_error_msg) {
         fprintf(stderr, "Unable to find monitor with the specified EDID\n" );
      }
      break;
   // no default case because switch is exhaustive, compiler warns if case missing
   }  // switch

   if (dref) {
      if (!validated)      // DISP_ID_BUSNO or DISP_ID_ADL
        validated = ddc_is_valid_display_ref(dref, emit_error_msg);
      if (!validated) {
         free(dref);
         dref = NULL;
      }
   }

   // printf("(%s) Returning: %s\n", __func__, (pdref)?"non-null": "NULL" );
   return dref;
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

   VCP_Feature_Table_Entry * entry = vcp_find_feature_by_charid(feature);
   if (entry) {
      if ( !( (entry->flags) & VCP_WRITABLE ) ){
         printf("Feature %s (%s) is not writable\n", feature, entry->name);
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
Interpreted_Vcp_Code * get_and_filter_vcp_value(
      Display_Handle *          dh,
      VCP_Feature_Table_Entry * vcp_entry,
      bool                      suppress_unsupported
     )
{
   Output_Level output_level = get_output_level();
   Byte vcp_code = vcp_entry->code;
   char * feature_name = vcp_entry->name;
   if (output_level >= OL_VERBOSE)
      printf("\nGetting data for VCP code 0x%02x - %s:\n", vcp_code, feature_name);
   Interpreted_Vcp_Code * code_info = NULL;
   Global_Status_Code rc = get_vcp_by_display_handle(dh, vcp_code, &code_info);
   // printf("(%s) get_vcp_by_DisplayRef() returned %p\n", __func__, code_info);

   // if (code_info)
   //    printf("(%s) code_info->valid_response=%d\n", __func__, code_info->valid_response);
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
      // printf("(%s) vcp_entry->flags=0x%04x\n", __func__, vcp_entry->flags);
      Version_Spec vcp_version = {0,0};
      if ( (vcp_entry->flags & VCP_FUNC_VER) )
           vcp_version = get_vcp_version_by_display_handle(dh);

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
   Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   if (debug)
      printf("(%s) Starting. Getting value for feature 0x%02x, dh=%s, vspec=%d.%d\n",
             __func__, vcp_entry->code, display_handle_repr(dh), vspec.major, vspec.minor);

   Interpreted_Vcp_Code * code_info = get_and_filter_vcp_value(dh, vcp_entry, true /* suppress_unsupported */ );
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
        Version_Spec              vcp_version,   // will be set for scan operations, not set for single
        GPtrArray *               collector,   // where to write output
        bool                      suppress_unsupported)    // if set, do not output unsupported features
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. Getting value for feature 0x%02x, dh=%s, vcp_version=%d.%d\n",
             __func__, vcp_entry->code, display_handle_repr(dh), vcp_version.major, vcp_version.minor);

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
   if (debug)
      printf("(%s) Done\n", __func__);
   // TRCMSG("Done");
}


// TODO split this out ala get_and_filter_vcp_value()

void show_vcp_for_table_vcp_code_table_entry_by_display_handle(
        Display_Handle *          dh,
        VCP_Feature_Table_Entry * vcp_entry,
        Version_Spec              vcp_version,
        GPtrArray *                  collector,   // where to write output
        bool                      suppress_unsupported)    // if set, do not output unsupported features
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. Getting value for feature 0x%02x, dh=%s\n",
             __func__, vcp_entry->code, display_handle_repr(dh));
   Byte vcp_code = vcp_entry->code;
   char * feature_name = vcp_entry->name;
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
      if ( (vcp_entry->flags & VCP_FUNC_VER) && (vcp_version.major == 0) )
         vcp_version = get_vcp_version_by_display_handle(dh);

      if (output_level != OL_PROGRAM) {

         char * formatted_data = NULL;
         bool ok = vcp_format_table_feature_detail(vcp_entry, vcp_version, accumulator, &formatted_data);

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
      printf("(%s) Done.\n", __func__);
}


void show_vcp_for_vcp_code_table_entry_by_display_ref(
        Display_Ref *              dref,
        VCP_Feature_Table_Entry *  vcp_entry,
        GPtrArray *                   collector)   // where to write output
{
   bool debug = false;
   Trace_Group tg = TRACE_GROUP;
   if (debug)
      tg = 0xff;
   TRCMSGTG(tg, "Starting");
   Version_Spec vcp_version = {0,0};

   Display_Handle * dh = ddc_open_display(dref, EXIT_IF_FAILURE);

   bool use_table_function = false;
   if (vcp_entry->flags & VCP_TYPE_V2NC_V3T) {
      vcp_version = get_vcp_version_by_display_handle(dh);
      if (vcp_version.major >= 3)
         use_table_function = true;
   }
   else if (vcp_entry->flags & VCP_TABLE)
      use_table_function = true;


   if (use_table_function) {
      show_vcp_for_table_vcp_code_table_entry_by_display_handle(dh, vcp_entry, vcp_version, collector, false);
   }
   else {
      show_vcp_for_nontable_vcp_code_table_entry_by_display_handle(dh, vcp_entry, vcp_version, collector, false);
   }
   ddc_close_display(dh);

   TRCMSGTG(tg, "Done");
}


// duplicate code, ugh!!!

#ifdef UNUSED
void show_single_vcp_value_by_display_handle(Display_Handle * phandle, char * feature, bool force) {
   // char buf[100];
   // printf("(%s) Starting. Getting feature %s for %s\n", __func__, feature,
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
      printf("(%s) force specified.  UNIMPLEMENTED\n", __func__ );
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

   if (entry) {
      if ( !( (entry->flags) & VCP_READABLE ) ){
         printf("Feature %s (%s) is not readable\n", feature, entry->name);
         showit = false;
      }
   }
   else if (force) {
      // printf("(%s) force specified.  UNIMPLEMENTED\n", __func__ );
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

   // printf("(%s) showit=%d\n", __func__, showit);
   if (showit) {
      // printf("(%s) calling show_vcp_for_vcp_code_table_entry_by_display_ref()\n", __func__);
      show_vcp_for_vcp_code_table_entry_by_display_ref(dref, entry, NULL);
   }

   if (debug)
      printf("(%s) Done\n", __func__);
}


/* Shows the VCP values for all features in a VCP feature subset.
 *
 * Arguments:
 *    dh      display handle for open display
 *    subset  feature subset
 *    fp      where to write output
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
   if (debug)
      printf("(%s) Starting.  subset=%d  dh=%s\n", __func__, subset, display_handle_repr(dh) );

   // For collections of feature codes, just assume that at least one of them
   // will need the version number for proper interpretation.
   // TODO: verify lookup always occurs in called functions and eliminate parm
   Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   // printf("(%s) VCP version = %d.%d\n", __func__, vcp_version.major, vcp_version.minor);

   if (subset == SUBSET_SCAN) {
      int ndx = 0;
      for (ndx=0; ndx <= 255; ndx++) {
         Byte id = ndx;
         // printf("(%s) ndx=%d, id=0x%02x\n", __func__, ndx, id);
         VCP_Feature_Table_Entry * entry = vcp_find_feature_by_hexid_w_default(id);
         bool suppress_unsupported = (get_output_level() < OL_VERBOSE);
         if ( !( (entry->flags) & VCP_READABLE ) ){
            // confuses the output if suppressing unsupported
            if (!suppress_unsupported)
               printf("Feature 0x%02x (%s) is not readable\n", ndx, entry->name);
         }
         else {
            if (entry->flags & VCP_TABLE) {
               show_vcp_for_table_vcp_code_table_entry_by_display_handle(
                  dh,
                  entry,
                  vcp_version,
                  collector,
                  suppress_unsupported);              // suppress unsupported features
            }
            else {
               show_vcp_for_nontable_vcp_code_table_entry_by_display_handle(
                  dh,
                  entry,
                  vcp_version,
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
         if (vcp_entry->flags & VCP_READABLE) {
            bool showIt = true;      //
            switch (subset) {
            case SUBSET_ALL:       showIt = true;                              break;
            case SUBSET_SUPPORTED: showIt = true;                              break;
            case SUBSET_COLORMGT:  showIt = vcp_entry->flags & VCP_COLORMGT;   break;
            case SUBSET_PROFILE:   showIt = vcp_entry->flags & VCP_PROFILE;    break;
            case SUBSET_SCAN:  // will never happen, inserted to avoid compiler warning
            default: PROGRAM_LOGIC_ERROR("subset=%d", subset);
            };

           if (showIt) {
              if (vcp_entry->flags & VCP_TABLE) {
                 show_vcp_for_table_vcp_code_table_entry_by_display_handle(
                    dh,
                    vcp_entry,
                    vcp_version,
                    collector,
                    (subset==SUBSET_SUPPORTED) );    // suppress unsupported features

              }
              else {
                 show_vcp_for_nontable_vcp_code_table_entry_by_display_handle(
                     dh,
                     vcp_entry,
                     vcp_version,
                     collector,
                     (subset==SUBSET_SUPPORTED));    // suppress_unsupported
              }
           }
         }
      }
   }

   if (debug)
      printf("(%s) Done\n", __func__);
}


/* Shows the VCP values for all features in a VCP feature subset.
 *
 * Arguments:
 *    pdisp   display reference
 *    subset  feature subset
 *    fp      where to write output
 *
 * Returns:
 *    nothing
 */
void show_vcp_values_by_display_ref(Display_Ref * dref, VCP_Feature_Subset subset, GPtrArray * collector) {
   // printf("(%s) Starting.  subset=%d   \n", __func__, subset );
   // need to ensure that bus info initialized
   bool validDisp = true;
   if (dref->ddc_io_mode == DDC_IO_DEVI2C) {
      // Is this needed?  or checked by openDisplay?
      Bus_Info * bus_info = i2c_get_bus_info(dref->busno);
      if (!bus_info ||  !(bus_info->flags & I2C_BUS_ADDR_0X37) ) {
         fprintf(stderr, "(%s) Address 0x37 not detected on bus %d. I2C communication not available.\n",
                __func__, dref->busno );
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


/* Creates a list of all displays found.  The list first contains any displays
 * on /dev/i2c-n busses, then any ADL displays.
 *
 * The displays are assigned a display number (starting from 1) based on the
 * above order.
 *
 * Arguments: none
 *
 * Returns:
 *    Display_Info_list struct
 */
Display_Info_List * ddc_get_valid_displays() {
   int ndx;

      Display_Info_List i2c_displays = i2c_get_valid_displays();
      Display_Info_List adl_displays = adlshim_get_valid_displays();

      // merge the lists
      int displayct = i2c_displays.ct + adl_displays.ct;
      Display_Info_List * all_displays = calloc(1, sizeof(Display_Info_List));
      all_displays->info_recs = calloc(displayct, sizeof(Display_Info));
      all_displays->ct = displayct;
      memcpy(all_displays->info_recs,
             i2c_displays.info_recs,
             i2c_displays.ct * sizeof(Display_Info));
      memcpy(all_displays->info_recs + i2c_displays.ct*sizeof(Display_Info),
             adl_displays.info_recs,
             adl_displays.ct * sizeof(Display_Info));
      if (i2c_displays.info_recs)
         free(i2c_displays.info_recs);
      if (adl_displays.info_recs)
         free(adl_displays.info_recs);
      for (ndx = 0; ndx < displayct; ndx++) {
         all_displays->info_recs[ndx].dispno = ndx+1;      // displays are numbered from 0, not 1
      }
      // printf("(%s) all_displays in main.c:\n", __func__);
      // report_display_info_list(all_displays, 0);
      return all_displays;
}


/* Show information about a display.
 *
 * Arguments:
 *    curinfo   pointer to display information
 *    depth     logical indentation depth
 */
void ddc_show_active_display(Display_Info * curinfo, int depth) {
   if (curinfo->dref->ddc_io_mode == DDC_IO_DEVI2C)
      i2c_show_active_display_by_busno(curinfo->dref->busno, depth);
   else {
      adlshim_show_active_display_by_display_ref(curinfo->dref, depth);
     // adl_show_active_display_by_adlno(curinfo->dref->iAdapterIndex, curinfo->dref->iDisplayIndex, depth);
   }

   Output_Level output_level = get_output_level();
   if (output_level >= OL_NORMAL) {
      // char * short_name = display_ref_short_name(curinfo->dref);
          // printf("Display:       %s\n", short_name);
          // works, but TMI
          // printf("Mfg:           %s\n", cur_info->edid->mfg_id);
      // don't want debugging  output if OL_VERBOSE
      if (output_level >= OL_VERBOSE)
         set_output_level(OL_NORMAL);

      Version_Spec vspec = get_vcp_version_by_display_ref(curinfo->dref);

      // printf("VCP version:   %d.%d\n", vspec.major, vspec.minor);
      if (vspec.major == 0)
         rpt_vstring(depth, "VCP version: detection failed");
      else
         rpt_vstring(depth, "VCP version:         %d.%d", vspec.major, vspec.minor);

      if (output_level >= OL_VERBOSE) {
         // display controller mfg, firmware version
         Interpreted_Vcp_Code* code_info;

         Global_Status_Code gsc = get_vcp_by_display_ref(
                curinfo->dref,
                0xc8,         // controller manufacturer
                &code_info);
         if (gsc != 0) {
            printf("(%s) get_vcp_by_display_ref() returned %s\n", __func__, gsc_desc(gsc));
         }
         else {
            Feature_Value_Entry * vals = pxc8_display_controller_type_values;
            char * mfg_name =  find_value_name_new(
                                  vals,
                                  code_info->sl);
            rpt_vstring(depth, "Controller mfg:      %s", (mfg_name) ? mfg_name : "not set");
            if (mfg_name) {
               Global_Status_Code gsc = get_vcp_by_display_ref(
                        curinfo->dref,
                        0xc9,         // firmware version
                        &code_info);
               if (gsc != 0) {
                  printf("(%s) get_vcp_by_display_ref() returned %s\n", __func__, gsc_desc(gsc));
               }
               else {
                  rpt_vstring(depth, "Firmware version:    %d.%d", code_info->sh, code_info->sl);
               }
            }
         }
      }
      if (output_level >= OL_VERBOSE)
         set_output_level(output_level);
   }
}


/* Reports all displays found.
 *
 * Arguments:
 *    depth       logical indentation depth
 *
 * Returns:
 *    number of displays
 */
int ddc_show_active_displays(int depth) {
   Display_Info_List * display_list = ddc_get_valid_displays();
   int ndx;
   for (ndx=0; ndx<display_list->ct; ndx++) {
      rpt_vstring(depth, "Display %d", ndx+1);
      Display_Info * curinfo = &display_list->info_recs[ndx];
      ddc_show_active_display(curinfo, depth+1);
      puts("");
   }
   return display_list->ct;
}


/* Returns a Display_Ref for the nth display.
 *
 * Arguments:
 *    dispno     display number
 *
 * Returns:
 *    Display_Ref for the dispno'th display, NULL if
 *    dispno < 1 or dispno > number of displays
 */
Display_Ref* ddc_find_display_by_dispno(int dispno) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting.  dispno=%d\n", __func__, dispno);
   Display_Ref * result = NULL;
   Display_Info_List * all_displays = ddc_get_valid_displays();
   if (dispno >= 1 && dispno <= all_displays->ct) {
      Display_Info chosen = all_displays->info_recs[dispno-1];
      result = chosen.dref;
   }

   if (debug) {
      printf("(%s) Returning: %p  \n", __func__, result );
      if (result)
         report_display_ref(result, 0);
   }
   return result;
}


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
      Interpreted_Vcp_Code * pinterpreted_code;

      // verbose output is distracting since this function is called when
      // querying for other things
      Output_Level olev = get_output_level();
      if (olev == OL_VERBOSE)
         set_output_level(OL_NORMAL);
      Global_Status_Code  gsc = get_vcp_by_display_handle(dh, 0xdf, &pinterpreted_code);
      if (olev == OL_VERBOSE)
         set_output_level(olev);
      if (gsc == 0) {
         dh->vcp_version.major = pinterpreted_code->sh;
         dh->vcp_version.minor = pinterpreted_code->sl;
      }
      else {
         // happens for pre MCCS v2 monitors
         if (debug)
            printf("(%s) Error detecting VCP version. gsc=%s\n",
                    __func__, gsc_desc(gsc) );
      }
   }
   // printf("(%s) Returning: %d.%d\n", __func__, dh->vcp_version.major, dh->vcp_version.minor);
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

   // printf("(%s) Returning: %d.%d\n", __func__, dref->vcp_version.major, vspec.minor);
   return dref->vcp_version;
}


/* Executes the VCP Get Capabilities command to obtain the
 * capabilities string.  The string is returned in null terminated
 * form in a Buffer struct.  It is the responsibility of the caller to
 * free this struct.
 *
 * Arguments:
 *    dh                    pointer to display handle
 *    ppCapabilitiesBuffer  address at which to return pointer to allocated Buffer
 *
 * Returns:
 *   status code
 */
Global_Status_Code get_capabilities_buffer_by_display_handle(
                      Display_Handle * dh,
                      Buffer**         ppCapabilitiesBuffer) {
   int rc;

   rc = multi_part_read_with_retry(
           dh,
           DDC_PACKET_TYPE_CAPABILITIES_REQUEST,
           0x00,                       // no subtype for capabilities
           ppCapabilitiesBuffer);

   Buffer * cap_buffer = *ppCapabilitiesBuffer;
   if (rc >= 0) {
      // trim trailing blanks and nulls
      int len = buffer_length(*ppCapabilitiesBuffer);
      while ( len > 0) {
         Byte ch = cap_buffer->bytes[len-1];
         if (ch == ' ' || ch == '\0')
            len--;
         else
            break;
      }
      // since buffer contains a string, put a single null at end
      buffer_set_byte(cap_buffer, len, '\0');
      buffer_set_length(cap_buffer, len+1);
   }
   return rc;
}


Global_Status_Code get_capabilities_string_by_display_handle(Display_Handle * dh, char** pcaps) {
   Global_Status_Code gsc = 0;
   if (!dh->capabilities_string) {
      Buffer * pcaps_buffer;
      gsc = get_capabilities_buffer_by_display_handle(dh, &pcaps_buffer);
      if (gsc == 0) {
         dh->capabilities_string = strdup((char *) pcaps_buffer->bytes);
         buffer_free(pcaps_buffer,__func__);
      }
   }
   *pcaps = dh->capabilities_string;
   return gsc;
}


/* Executes the VCP Get Capabilities command to obtain the
 * capabilities string.  The string is returned in null terminated
 * form in a Buffer struct.  It is the responsibility of the caller to
 * free this struct.
 *
 * Arguments:
 *    dref                  pointer to display reference
 *    ppCapabilitiesBuffer  address at which to return pointer to allocated Buffer
 *
 * Returns:
 *   status code
 */
Global_Status_Code get_capabilities_buffer_by_display_ref(Display_Ref * dref, Buffer** ppCapabilitiesBuffer) {
   int rc;
   Display_Handle* dh = ddc_open_display(dref, EXIT_IF_FAILURE);
   rc = get_capabilities_buffer_by_display_handle(dh, ppCapabilitiesBuffer);
   ddc_close_display(dh);
   return rc;
}


Global_Status_Code get_capabilities_string_by_display_ref(Display_Ref * dref, char** pcaps) {
   int rc;
   Display_Handle* dh = ddc_open_display(dref, EXIT_IF_FAILURE);
   rc = get_capabilities_string_by_display_handle(dh, pcaps);
   ddc_close_display(dh);
   return rc;
}


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
   // printf("(%s) strlen(mfg_id)=%ld\n", __func__, strlen(edid->mfg_id));
   snprintf(buf, bufsz, "MFG_ID  %s",  edid->mfg_id);
   // printf("(%s) strlen(buf)=%ld\n", __func__, strlen(buf));
   s = strdup(buf);
   // printf("(%s) strlen(s)=%ld\n", __func__, strlen(s));
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
