/*
 * ddc_services.c
 *
 *  Created on: Nov 15, 2015
 *      Author: rock
 */

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <base/ddc_errno.h>
#include <base/ddc_packets.h>
#include <base/displays.h>
#include <base/linux_errno.h>

#include <i2c/i2c_bus_core.h>
#include <adl/adl_intf.h>

#include <ddc/ddc_multi_part_io.h>
#include <ddc/ddc_packet_io.h>
#include <ddc/ddc_vcp.h>
#include <ddc/vcp_feature_code_data.h>

#include <ddc/ddc_services.h>

// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_DDC;

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


// TODO: consider moving value parsing to cmd_parser_popt
Global_Status_Code set_vcp_value_top(Display_Ref * pdisp, char * feature, char * new_value) {
   Global_Status_Code my_errno = 0;
   long               longtemp;

   VCP_Feature_Table_Entry * entry = find_feature_by_charid(feature);
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

void show_vcp_for_nontable_vcp_code_table_entry_by_display_handle(
        Display_Handle *          dh,
        VCP_Feature_Table_Entry * vcp_entry,
        Version_Spec              vcp_version,   // will be set for scan operations, not set for single
        FILE *                    fp,   // where to write output
        bool                      suppress_unsupported)    // if set, do not output unsupported features
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. Getting value for feature 0x%02x, dh=%s, vcp_version=%d.%d\n",
             __func__, vcp_entry->code, display_handle_repr(dh), vcp_version.major, vcp_version.minor);


   Byte vcp_code = vcp_entry->code;
   char * feature_name = vcp_entry->name;
   // Msg_Level msgLevel = get_global_msg_level();
   Output_Level output_level = get_output_level();
   // if (msgLevel >= VERBOSE)
   if (output_level >= OL_VERBOSE)
      printf("\nGetting data for VCP code 0x%02x - %s:\n", vcp_code, feature_name);

   // Output_Format outputFormat = get_output_format();

   Interpreted_Vcp_Code * code_info = NULL;
   Global_Status_Code rc = get_vcp_by_display_handle(dh, vcp_code, &code_info);
   // printf("(%s) get_vcp_by_DisplayRef() returned %p\n", __func__, code_info);
   // if (code_info)
   //    printf("(%s) code_info->valid_response=%d\n", __func__, code_info->valid_response);
   // for unsupported features, some monitors return null response rather than a valid response
   // with unsupported feature indicator set
   if (rc == DDCRC_NULL_RESPONSE) {
      // if (msgLevel >= NORMAL && outputFormat == OUTPUT_NORMAL)
      if (output_level >= OL_NORMAL && !suppress_unsupported)
         printf("VCP code 0x%02x (%-30s): Unsupported feature code (Null response)\n", vcp_code, feature_name);
   }
   else if (!code_info) {
      // if (msgLevel >= NORMAL && outputFormat == OUTPUT_NORMAL)
      if (output_level >= OL_NORMAL)
         printf("VCP code 0x%02x (%-30s): Unparsable response\n", vcp_code, feature_name);
      // printf("Error retrieving VCP info.  Failed to interpret returned data.\n");
   }
   else if (!code_info->valid_response) {
      //if (msgLevel >= NORMAL && outputFormat == OUTPUT_NORMAL)
      if (output_level >= OL_NORMAL)
      printf("VCP code 0x%02x (%-30s): Invalid response\n", vcp_code, feature_name);
      // printf("Error retrieving VCP info\n");
   }
   else if (!code_info->supported_opcode) {
      // if (msgLevel >= NORMAL && outputFormat == OUTPUT_NORMAL)
      if (output_level >= OL_NORMAL && !suppress_unsupported)
      // printf("Invalid VCP code 0x%02x (%s)\n", vcp_code, feature_name);
      printf("VCP code 0x%02x (%-30s): Unsupported feature code\n", vcp_code, feature_name);
   }
   else {
      // if interpretation is version dependent and version not already set, get it
      // printf("(%s) vcp_entry->flags=0x%04x\n", __func__, vcp_entry->flags);
      if ( (vcp_entry->flags & VCP_FUNC_VER) && (vcp_version.major == 0) )
         vcp_version = get_vcp_version_by_display_handle(dh);

      Format_Feature_Detail_Function ffd_func = NULL;

      if (output_level != OL_PROGRAM) {
         // TODO: This is point at which need to know if response is version dependent, pass it to ffd_func

            ffd_func = get_feature_detail_function(vcp_entry);

            char buf[100];
            ffd_func(code_info, vcp_version,  buf, 100);
            printf("VCP code 0x%02x (%-30s): %s\n", vcp_code, feature_name, buf);

      }   // OUTPUT_NORNAL
      else {    // OUTPUT_PROG_VCP

            int cv = code_info->cur_value;
            // int mv = code_info->max_value;
            fprintf(fp, "VCP %02X %5d\n", vcp_code, cv);

      }
   }
   // if (code_info)
   //   free(code_info);   // sometimes causes free failure, crash
   if (debug)
      printf("(%s) Done\n", __func__);
   // TRCMSG("Done");
}





void show_vcp_for_table_vcp_code_table_entry_by_display_handle(
        Display_Handle *          dh,
        VCP_Feature_Table_Entry * vcp_entry,
        Version_Spec              vcp_version,
        FILE *                    fp,   // where to write output
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
   else if (rc == DDCRC_UNSUPPORTED) {
      if (!suppress_unsupported)
         printf("VCP code 0x%02x (%-30s): Unsupported feature code\n", vcp_code, feature_name);
   }

   // TODO: additional appropriate status codes
   else if (rc != 0) {
      //if (msgLevel >= NORMAL && outputFormat == OUTPUT_NORMAL)
      if (output_level >= OL_NORMAL)
      // TODO: status code name
      printf("VCP code 0x%02x (%-30s): Invalid response. status code=%s\n",
             vcp_code, feature_name, global_status_code_description(rc));
   }

   else {
      if ( (vcp_entry->flags & VCP_FUNC_VER) && (vcp_version.major == 0) )
         vcp_version = get_vcp_version_by_display_handle(dh);

      if (output_level != OL_PROGRAM) {
         Format_Table_Feature_Detail_Function ffd_func =
             get_table_feature_detail_function(vcp_entry);
         Buffer * formatted_data = NULL;
         bool ok = ffd_func(vcp_version, accumulator, &formatted_data);
         if (ok)
            printf("VCP code 0x%02x (%-30s): %.*s\n", vcp_code, feature_name, formatted_data->len, formatted_data->bytes);
         else
            printf("VCP code 0x%02x (%-30s): !!! UNABLE TO FORMAT OUTPUT", vcp_code, feature_name);
      }   // OUTPUT_NORNAL
      else {    // OUTPUT_PROG_VCP
         // output VCP code  hex values of bytes
         int hexbufsize = buffer_length(accumulator) * 3;
         char * hexbuf = calloc(1,hexbufsize);
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
         fprintf(fp, "VCP %02X %s\n", vcp_code, hexbuf);
      }
   }
   // if (code_info)
   //   free(code_info);   // sometimes causes free failure, crash

   // TRCMSG("Done");
   if (debug)
      printf("(%s) Done.\n", __func__);
}





void show_vcp_for_vcp_code_table_entry_by_display_ref(
        Display_Ref *              dref,
        VCP_Feature_Table_Entry *  vcp_entry,
        FILE *                     fp)   // where to write output
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
      show_vcp_for_table_vcp_code_table_entry_by_display_handle(dh, vcp_entry, vcp_version, fp, false);
   }
   else {
      show_vcp_for_nontable_vcp_code_table_entry_by_display_handle(dh, vcp_entry, vcp_version, fp, false);
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
   VCP_Feature_Table_Entry * entry = find_feature_by_charid(feature);
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
   if (debug)
      printf("(%s) Starting. Getting feature %s for %s\n", __func__, feature,
           display_ref_short_name(dref) );
   VCP_Feature_Table_Entry * entry = find_feature_by_charid(feature);
   bool showit = true;
   if (entry) {
      if ( !( (entry->flags) & VCP_READABLE ) ){
         printf("Feature %s (%s) is not readable\n", feature, entry->name);
         showit = false;
      }
   }
   else if (force) {
      // printf("(%s) force specified.  UNIMPLEMENTED\n", __func__ );
      entry = create_dummy_feature_for_charid(feature);    // issues error message if invalid hex
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
      show_vcp_for_vcp_code_table_entry_by_display_ref(dref, entry, stdout);
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
        FILE *              fp)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting.  subset=%d  dh=%s\n", __func__, subset, display_handle_repr(dh) );

   // For collections of feature codes, just assume that at least one of them
   // will need the version number for proper interpretation.
   Version_Spec vcp_version = get_vcp_version_by_display_handle(dh);
   // printf("(%s) VCP version = %d.%d\n", __func__, vcp_version.major, vcp_version.minor);

   if (subset == SUBSET_SCAN) {
      int ndx = 0;
      for (ndx=0; ndx <= 255; ndx++) {
         Byte id = ndx;
         // printf("(%s) ndx=%d, id=0x%02x\n", __func__, ndx, id);
         VCP_Feature_Table_Entry * entry = find_feature_by_hexid_w_default(id);
         if ( !( (entry->flags) & VCP_READABLE ) ){
            // confuses the output, since we're suppressing unsupported
            // printf("Feature 0x%02x (%s) is not readable\n", ndx, entry->name);
         }
         else {
            if (entry->flags & VCP_TABLE) {
               show_vcp_for_table_vcp_code_table_entry_by_display_handle(
                  dh,
                  entry,
                  vcp_version,
                  stdout,
                  true);              // suppress unsupported features
            }
            else {
               show_vcp_for_nontable_vcp_code_table_entry_by_display_handle(
                  dh,
                  entry,
                  vcp_version,
                  stdout,
                  true);   //  suppress unsupported features
            }
         }
      }
   }
   else {
      int ndx = 0;
      for (ndx=0; ndx < vcp_feature_code_count; ndx++) {
         VCP_Feature_Table_Entry * vcp_entry = get_vcp_feature_table_entry(ndx);
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
                    fp,
                    (subset==SUBSET_SUPPORTED) );    // suppress unsupported features

              }
              else {
                 show_vcp_for_nontable_vcp_code_table_entry_by_display_handle(
                     dh,
                     vcp_entry,
                     vcp_version,
                     fp,
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
void show_vcp_values_by_display_ref(Display_Ref * dref, VCP_Feature_Subset subset, FILE * fp) {
   // printf("(%s) Starting.  subset=%d   \n", __func__, subset );
   // need to ensure that bus info initialized
   bool validDisp = true;
   if (dref->ddc_io_mode == DDC_IO_DEVI2C) {
      // Is this needed?  or checked by openDisplay?
      Bus_Info * bus_info = get_bus_info(dref->busno);
      if (!bus_info ||  !(bus_info->flags & I2C_BUS_ADDR_0X37) ) {
         printf("(%s) Address 0x37 not detected on bus %d. I2C communication not available.\n",
                __func__, dref->busno );
         validDisp = false;
      }
   }
   else {
      validDisp = true;    // already checked
   }

   if (validDisp) {
      Display_Handle * pDispHandle = ddc_open_display(dref, EXIT_IF_FAILURE);
      show_vcp_values_by_display_handle(pDispHandle, subset, fp);
      ddc_close_display(pDispHandle);
   }
}



Display_Info_List * get_valid_ddc_displays() {

      Display_Info_List i2c_displays = get_valid_i2c_displays();
      Display_Info_List adl_displays = get_valid_adl_displays();

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
      // printf("(%s) all_displays in main.c:\n", __func__);
      // report_display_info_list(all_displays, 0);
      return all_displays;
}



Version_Spec get_vcp_version_by_display_handle(Display_Handle * dh) {
   // printf("(%s) Starting. dh=%p, dh->vcp_version =  %d.%d\n",
   //        __func__, dh, dh->vcp_version.major, dh->vcp_version.minor);
   if (is_version_unqueried(dh->vcp_version)) {
      dh->vcp_version.major = 0;
      dh->vcp_version.minor = 0;
      Interpreted_Vcp_Code * pinterpreted_code;

      Global_Status_Code  gsc = get_vcp_by_display_handle(dh, 0xdf, &pinterpreted_code);
      if (gsc == 0) {
         dh->vcp_version.major = pinterpreted_code->sh;
         dh->vcp_version.minor = pinterpreted_code->sl;
      }
      else {
         printf("(%s) Error detecting VCP version. gsc=%s\n",
                __func__, global_status_code_description(gsc) );
      }
   }
   // printf("(%s) Returning: %d.%d\n", __func__, dh->vcp_version.major, dh->vcp_version.minor);
   return dh->vcp_version;
}


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
 *    dref                  pointer to display reference
 *    ppCapabilitiesBuffer  address at which to return pointer to allocated Buffer
 *
 * Returns:
 *   status code
 */
Global_Status_Code get_capabilities(Display_Ref * dref, Buffer** ppCapabilitiesBuffer) {
   int rc;

   Display_Handle* dh = ddc_open_display(dref, EXIT_IF_FAILURE);

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
   ddc_close_display(dh);
   return rc;
}

