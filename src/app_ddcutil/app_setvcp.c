/* app_setvcp.c
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

/** \f
 *
 */

/** \cond */
#include <assert.h>
#include <errno.h>
#include <string.h>
/** \endcond */

#include "base/core.h"
#include "base/ddc_errno.h"

#include "vcp/vcp_feature_codes.h"

#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_vcp_version.h"


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
   assert(string_value);
   bool ok = true;
   char buf[20];
   strupper(string_value);
   if (*string_value == 'X' ) {
      snprintf(buf, 20, "0%s", string_value);
      string_value = buf;
      // DBGMSG("Adjusted value: |%s|", string_value);
   }
   else if (*(string_value + strlen(string_value)-1) == 'H') {
      int newlen = strlen(string_value)-1;
      snprintf(buf, 20, "0x%.*s", newlen, string_value);
      string_value = buf;
      // DBGMSG("Adjusted value: |%s|", string_value);
   }

   char * endptr = NULL;
   errno = 0;
   long longtemp = strtol(string_value, &endptr, 0 );  // allow 0xdd  for hex values
   int errsv = errno;
   // printf("errno=%d, new_value=|%s|, &new_value=%p, longtemp = %ld, endptr=0x%02x\n",
   //        errsv, new_value, &new_value, longtemp, *endptr);
   if (*endptr || errsv != 0) {
      printf("Not a number: %s\n", string_value);
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
 *   dh         display handle
 *   feature    feature id (as string)
 *   new_value  new feature value (as string)
 *   force
 *   retry_history if non-null, collects retryable errors
 *
 * Returns:
 *   0          success
 *   -EINVAL    invalid setvcp arguments, feature not writable
 *   from put_vcp_by_display_ref()
 */
// TODO: consider moving value parsing to command parser
Error_Info *
app_set_vcp_value(
      Display_Handle * dh,
      char *           feature,
      char *           new_value,
      bool             force)
{
   bool debug = false;
   DBGMSF(debug,"Starting");
   assert(new_value && strlen(new_value) > 0);

   Public_Status_Code         psc = 0;
   Error_Info *               ddc_excp = NULL;
   long                       longtemp;
   Byte                       hexid;
   VCP_Feature_Table_Entry *  entry = NULL;
   bool                       good_value = false;
   Single_Vcp_Value           vrec;

   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   bool ok = any_one_byte_hex_string_to_byte_in_buf(feature, &hexid);
   if (!ok) {
      f0printf(FOUT, "Unrecognized VCP feature code: %s\n", feature);
      psc = DDCL_UNKNOWN_FEATURE;
      ddc_excp = errinfo_new(psc, __func__);
      goto bye;
   }
   entry = vcp_find_feature_by_hexid(hexid);
   if (!entry && ( force || hexid >= 0xe0) )  // assume force for mfg specific codes
      entry = vcp_create_dummy_feature_for_hexid(hexid);
   if (!entry) {
      f0printf(FOUT, "Unrecognized VCP feature code: %s\n", feature);
      psc = DDCL_UNKNOWN_FEATURE;
      ddc_excp = errinfo_new(psc, __func__);
      goto bye;
   }

   if (!is_feature_writable_by_vcp_version(entry, vspec)) {
      char * feature_name =  get_version_sensitive_feature_name(entry, vspec);
      f0printf(FOUT, "Feature %s (%s) is not writable\n", feature, feature_name);
      psc = DDCL_INVALID_OPERATION;
      ddc_excp = errinfo_new(psc, __func__);
      goto bye;
   }

   if (is_feature_table_by_vcp_version(entry, vspec)) {
      Byte * value_bytes;
      int bytect = hhs_to_byte_array(new_value, &value_bytes);
      if (bytect < 0) {    // bad hex string
         good_value = false;
      }
      else {
         good_value = true;
         vrec.opcode  = entry->code;
         vrec.value_type = DDCA_TABLE_VCP_VALUE;
         vrec.val.t.bytect = bytect;
         vrec.val.t.bytes  = value_bytes;
      }
   }
   else {  // the usual non-table case

      // Check for relative values
      char value_prefix = ' ';
      if (new_value[0] == '+' || new_value[0] == '-') {
         value_prefix = new_value[0];
         new_value = new_value+1;
      }
      good_value = parse_vcp_value(new_value, &longtemp);

      // Handle relative values
      if (value_prefix != ' ') {
         if ( !(get_version_sensitive_feature_flags(entry, vspec) & DDCA_CONT) ) {
            char * feature_name =  get_version_sensitive_feature_name(entry, vspec);
            f0printf(FOUT, "Feature %s (%s) is not continuous\n", feature, feature_name);
            psc = DDCL_INVALID_OPERATION;
            ddc_excp = errinfo_new(psc, __func__);
            goto bye;
         }

         Parsed_Nontable_Vcp_Response * parsed_response;
         ddc_excp = ddc_get_nontable_vcp_value(
                       dh,
                       entry->code,
                       &parsed_response);
         if (ddc_excp) {
            // is message needed?
            char * feature_name =  get_version_sensitive_feature_name(entry, vspec);
            f0printf(FOUT, "Error reading feature %s (%s)\n", feature, feature_name);
            goto bye;
         }

         if ( value_prefix == '+') {
            longtemp = parsed_response->cur_value + longtemp;
            if (longtemp > parsed_response->max_value)
               longtemp = parsed_response->max_value;
         }
         else {
            assert( value_prefix == '-');
            longtemp = parsed_response->cur_value - longtemp;
            if (longtemp < 0)
               longtemp = 0;
         }
         free(parsed_response);
      }

      if (good_value) {
         vrec.opcode        = entry->code;
         vrec.value_type    = DDCA_NON_TABLE_VCP_VALUE;
         vrec.val.c.cur_val = longtemp;
      }
   }

   if (!good_value) {
      f0printf(FOUT, "Invalid VCP value: %s\n", new_value);
      // what is better status code?
      psc = -EINVAL;
      ddc_excp = errinfo_new(psc, __func__);
      goto bye;
   }

   ddc_excp = ddc_set_vcp_value(dh, &vrec);
   psc = ERRINFO_STATUS(ddc_excp);

   // *** TEMP FOR TESTING ***
   // if (vrec.val.c.cur_val == 25) {
   //    DBGMSG("Forcing DDC_VERIFY");
   //    psc = DDCRC_VERIFY;
   // }

   if (psc != 0)  {
      switch(psc) {
      case DDCRC_VERIFY:
            f0printf(FOUT, "Verification failed for feature %02x\n", entry->code);
            break;
      default:
         // Is this proper error message?
         f0printf(FOUT, "Setting value failed for feature %02x. rc=%s\n", entry->code, psc_desc(psc));
         if (psc == DDCRC_RETRIES)
            f0printf(FOUT, "    Try errors: %s\n", errinfo_causes_string(ddc_excp));
      }
   }

bye:
   if (entry && (entry->vcp_global_flags & DDCA_SYNTHETIC) ) {
      free_synthetic_vcp_entry(entry);
   }

   DBGMSF(debug, "Returning: %s", psc_desc(psc));
   return ddc_excp;
}
