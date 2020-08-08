/** @file app_setvcp.c
 *
 *  Implement the SETVCP command
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <errno.h>
#include <string.h>
/** \endcond */

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/ddc_errno.h"

#include "vcp/vcp_feature_codes.h"

#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_vcp_version.h"

#include "dynvcp/dyn_feature_codes.h"

#include "app_setvcp.h"


/** Converts a VCP feature value from string form to internal form.
 *
 *  \param   string_value
 *  \param   parsed_value    location where to return result
 *
 *  \return  true if conversion successful, false if not
 */
bool
parse_vcp_value(
      char * string_value,
      long * parsed_value)
{
   bool debug = false;

   FILE * ferr = stderr;
   assert(string_value);
   bool ok = true;
   char buf[20];

   DBGMSF(debug, "Starting. string_value = |%s|", string_value);
   strupper(string_value);
   if (*string_value == 'X' ) {
      snprintf(buf, 20, "0%s", string_value);
      string_value = buf;
      DBGMSF(debug, "Adjusted value: |%s|", string_value);
   }
   else if (*(string_value + strlen(string_value)-1) == 'H') {
      int newlen = strlen(string_value)-1;
      snprintf(buf, 20, "0x%.*s", newlen, string_value);
      string_value = buf;
      DBGMSF(debug, "Adjusted value: |%s|", string_value);
   }

   char * endptr = NULL;
   errno = 0;
   long longtemp = strtol(string_value, &endptr, 0 );  // allow 0xdd  for hex values
   int errsv = errno;
   DBGMSF(debug, "errno=%d, string_value=|%s|, &string_value=%p, longtemp = %ld, endptr=0x%02x",
                 errsv, string_value, &string_value, longtemp, *endptr);
   if (*endptr || errsv != 0) {
      f0printf(ferr, "Not a number: %s\n", string_value);
      ok = false;
   }
   else if (longtemp < 0 || longtemp > 65535) {
      f0printf(ferr, "Number must be in range 0..65535:  %ld\n", longtemp);
      ok = false;
   }
   else {
      *parsed_value = longtemp;
      ok = true;
   }

   DBGMSF(debug, "Done. *parsed_value=%ld, returning: %s", *parsed_value, sbool(ok));
   return ok;
}


/** Parses the Set VCP arguments passed and sets the new value.
 *
 *   \param  dh         display handle
 *   \param  feature    feature code (as string)
 *   \param  new_value  new feature value (as string)
 *   \param  force      attempt to set feature even if feature code unrecognized
 *   \return NULL if success, Error_Info if error
 */
// TODO: consider moving value parsing to command parser
Error_Info *
app_set_vcp_value(
      Display_Handle * dh,
      char *           feature,
      char *           new_value,
      bool             force)
{
   FILE * ferr = stderr;
   bool debug = false;
   DBGTRC(debug,DDCA_TRC_TOP, "Starting. feature=%s, new_value=%s, force=%s", feature, new_value, sbool(force));
   assert(new_value && strlen(new_value) > 0);

   DDCA_Status                ddcrc = 0;
   Error_Info *               ddc_excp = NULL;
   long                       longtemp;
   Byte                       feature_code;
   Display_Feature_Metadata * dfm = NULL;
   bool                       good_value = false;
   DDCA_Any_Vcp_Value         vrec;

   bool ok = any_one_byte_hex_string_to_byte_in_buf(feature, &feature_code);
   if (!ok) {
      f0printf(ferr, "Invalid VCP feature code: %s\n", feature);
      ddc_excp = errinfo_new2(DDCRC_ARG, __func__, "Invalid VCP feature code: %s\n", feature);
      goto bye;
   }

   dfm = dyn_get_feature_metadata_by_dh_dfm(feature_code,dh, (force || feature_code >= 0xe0) );
   if (!dfm) {
      f0printf(ferr, "Unrecognized VCP feature code: %s\n", feature);
      ddc_excp = errinfo_new2(DDCRC_UNKNOWN_FEATURE, __func__,
                              "Unrecognized VCP feature code: %s", feature);
      goto bye;
   }

   if (!(dfm->feature_flags & DDCA_WRITABLE)) {
      f0printf(ferr, "Feature %s (%s) is not writable\n", feature, dfm->feature_name);
      ddc_excp = errinfo_new2(DDCRC_INVALID_OPERATION, __func__,
                 "Feature %s (%s) is not writable", feature, dfm->feature_name);
      goto bye;
   }

   // Check for relative values
   char value_prefix = ' ';
   if (new_value[0] == '+' || new_value[0] == '-') {
      assert(strlen(new_value) > 1);
      value_prefix = new_value[0];
      new_value = new_value+1;
   }

   if (dfm->feature_flags & DDCA_TABLE) {
      if (value_prefix != ' ') {
         f0printf(ferr, "Relative VCP values valid only for Continuous VCP features\n");
         ddc_excp = errinfo_new2(DDCRC_INVALID_OPERATION, __func__,
                                 "Relative VCP values valid only for Continuous VCP features");
         goto bye;
      }

      Byte * value_bytes;
      int bytect = hhs_to_byte_array(new_value, &value_bytes);
      if (bytect < 0) {    // bad hex string
         f0printf(ferr, "Invalid hex value\n");
         ddc_excp = errinfo_new2(DDCRC_ARG, __func__, "Invalid hex value");
         goto bye;
      }

      vrec.opcode  = feature_code;
      vrec.value_type = DDCA_TABLE_VCP_VALUE;
      vrec.val.t.bytect = bytect;
      vrec.val.t.bytes  = value_bytes;
   }

   else {  // the usual non-table case
      good_value = parse_vcp_value(new_value, &longtemp);
      if (!good_value) {
         f0printf(ferr, "Invalid VCP value: %s\n", new_value);
         // what is better status code?
         ddc_excp = errinfo_new2(DDCRC_ARG, __func__,  "Invalid VCP value: %s", new_value);
         goto bye;
      }

      if ( value_prefix != ' ') {
         if ( !(dfm->feature_flags & DDCA_CONT) ) {
            f0printf(ferr, "Relative VCP values valid only for Continuous VCP features\n");
            // char * feature_name =  get_version_sensitive_feature_name(entry, vspec);
            // f0printf(ferr, "Feature %s (%s) is not continuous\n", feature, feature_name);
            ddc_excp = errinfo_new2(DDCRC_INVALID_OPERATION, __func__,
                           "Relative VCP values valid only for Continuous VCP features");
            goto bye;
         }

         // Handle relative values

         Parsed_Nontable_Vcp_Response * parsed_response;
         ddc_excp = ddc_get_nontable_vcp_value(
                       dh,
                       feature_code,
                       &parsed_response);
         if (ddc_excp) {
            ddcrc = ERRINFO_STATUS(ddc_excp);
            // is message needed?
            // char * feature_name =  get_version_sensitive_feature_name(entry, vspec);
            // f0printf(ferr, "Error reading feature %s (%s)\n", feature, feature_name);
            f0printf(ferr, "Getting value failed for feature %02x. rc=%s\n", feature_code, psc_desc(ddcrc));
            if (ddcrc == DDCRC_RETRIES)
               f0printf(ferr, "    Try errors: %s\n", errinfo_causes_string(ddc_excp));
            ddc_excp = errinfo_new_with_cause3(ddcrc, ddc_excp, __func__,
                                               "Getting value failed for feature %02x, rc=%s", feature_code, psc_desc(ddcrc));
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

      vrec.opcode        = feature_code;
      vrec.value_type    = DDCA_NON_TABLE_VCP_VALUE;
      vrec.val.c_nc.sh = (longtemp >> 8) & 0xff;
      // assert(vrec.val.c_nc.sh == 0);
      vrec.val.c_nc.sl = longtemp & 0xff;
   }

   ddc_excp = ddc_set_vcp_value(dh, &vrec, NULL);

   if (ddc_excp) {
      ddcrc = ERRINFO_STATUS(ddc_excp);
      switch(ddcrc) {
      case DDCRC_VERIFY:
            f0printf(ferr, "Verification failed for feature %02x\n", feature_code);
            ddc_excp = errinfo_new_with_cause3(ddcrc, ddc_excp, __func__,
                                               "Verification failed for feature %02x", feature_code);
            break;
      default:
         // Is this proper error message?
         f0printf(ferr, "Setting value failed for feature %02x. rc=%s\n", feature_code, psc_desc(ddcrc));
         if (ddcrc == DDCRC_RETRIES)
            f0printf(ferr, "    Try errors: %s\n", errinfo_causes_string(ddc_excp));
         ddc_excp = errinfo_new_with_cause3(ddcrc, ddc_excp, __func__,
                                            "Setting value failed for feature %02x", feature_code);
      }
   }

bye:
   dfm_free(dfm);  // handles dfm == NULL

   DBGTRC(debug, DDCA_TRC_TOP, "Done.     Returning: %s", errinfo_summary(ddc_excp));
   return ddc_excp;
}

