/** @file app_setvcp.c
 *
 *  Implement the SETVCP command
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <errno.h>
#include <string.h>
/** \endcond */

#include "public/ddcutil_types.h"

#include "util/error_info.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/feature_metadata.h"
#include "base/rtti.h"

#include "cmdline/parsed_cmd.h"

#include "ddc/ddc_vcp.h"
#include "ddc/ddc_packet_io.h"      // for alt_source_addr

#include "dynvcp/dyn_feature_codes.h"

#include "app_setvcp.h"

// Default trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_TOP;


/** Converts a VCP feature value from string form to internal form.
 *  Error messages are written to stderr.
 *
 *  @param   string_value
 *  @param   parsed_value    location where to return result
 *
 *  @return  true if conversion successful, false if not
 */
bool
parse_vcp_value(
      char * string_value,
      int * parsed_value_loc)
{
   assert(string_value);
   bool debug = false;
   DBGMSF(debug, "Starting. string_value = |%s|", string_value);

   FILE * errf = ferr();         // at app level will always be stderr
   char * canonical = canonicalize_possible_hex_value(string_value);
   bool ok = str_to_int(canonical, parsed_value_loc, 0);
   free(canonical);
   if (!ok) {
      f0printf(errf, "Not a number: \"%s\"\n", string_value);
      ok = false;
   }
   else if (*parsed_value_loc < 0 || *parsed_value_loc > 65535) {
      f0printf(errf, "Number must be in range 0..65535:  %d\n", *parsed_value_loc);
      ok = false;
   }

   DBGMSF(debug, "Done. *parsed_value_loc=%d, returning: %s", *parsed_value_loc, SBOOL(ok));
   return ok;
}


/** Parses the arguments passed for a single feature and sets the new value.
 *
 *   @param  dh          display handle
 *   @param  feature     feature code
 *   @param  value_type  indicates if a relative value
 *   @param  new_value   new feature value (as string)
 *   @param  force       attempt to set feature even if feature code unrecognized
 *   @return #Error_Info if error
 */
Error_Info *
app_set_vcp_value(
      Display_Handle *  dh,
      Byte              feature_code,
      Setvcp_Value_Type value_type,
      char *            new_value,
      bool              force)
{
   assert(new_value && strlen(new_value) > 0);
   bool debug = false;
   DBGTRC_STARTING(debug,TRACE_GROUP, "feature=0x%02x, new_value=%s, value_type=%s, force=%s",
                feature_code, new_value, setvcp_value_type_name(value_type), sbool(force));

   DDCA_Status                ddcrc = 0;
   Error_Info *               ddc_excp = NULL;
   int                        itemp;
   Display_Feature_Metadata * dfm = NULL;
   bool                       good_value = false;
   DDCA_Any_Vcp_Value         vrec;

   dfm = dyn_get_feature_metadata_by_dh(feature_code,dh, (force || feature_code >= 0xe0) );
   if (!dfm) {
      ddc_excp = ERRINFO_NEW(DDCRC_UNKNOWN_FEATURE,
                              "Unrecognized VCP feature code: 0x%02x", feature_code);
      goto bye;
   }

   if (!(dfm->feature_flags & DDCA_WRITABLE)) {
      ddc_excp = ERRINFO_NEW(DDCRC_INVALID_OPERATION,
                 "Feature 0x%02x (%s) is not writable", feature_code, dfm->feature_name);
      goto bye;
   }

   if (dfm->feature_flags & DDCA_TABLE) {
      if (value_type != VALUE_TYPE_ABSOLUTE) {
         ddc_excp = ERRINFO_NEW(DDCRC_INVALID_OPERATION,
                                 "Relative VCP values valid only for Continuous VCP features");
         goto bye;
      }

      Byte * value_bytes;
      int bytect = hhs_to_byte_array(new_value, &value_bytes);
      if (bytect < 0) {    // bad hex string
         ddc_excp = ERRINFO_NEW(DDCRC_ARG, "Invalid hex value");
         goto bye;
      }

      vrec.opcode  = feature_code;
      vrec.value_type = DDCA_TABLE_VCP_VALUE;
      vrec.val.t.bytect = bytect;
      vrec.val.t.bytes  = value_bytes;
   }

   else {  // the usual non-table case
      good_value = parse_vcp_value(new_value, &itemp);
      if (!good_value) {
         // what is better status code?
         ddc_excp = ERRINFO_NEW(DDCRC_ARG, "Invalid VCP value: %s", new_value);
         goto bye;
      }

      if (value_type != VALUE_TYPE_ABSOLUTE) {
         if ( !(dfm->feature_flags & DDCA_CONT) ) {
            ddc_excp = ERRINFO_NEW(DDCRC_INVALID_OPERATION,
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
            ddc_excp = errinfo_new_with_cause(ddcrc, ddc_excp, __func__,
                          "Getting value failed for feature %02x, rc=%s",
                          feature_code, psc_desc(ddcrc));
            goto bye;
         }

         if ( value_type == VALUE_TYPE_RELATIVE_PLUS) {
            itemp = RESPONSE_CUR_VALUE(parsed_response) + itemp;
            if (itemp > RESPONSE_MAX_VALUE(parsed_response))
               itemp = RESPONSE_MAX_VALUE(parsed_response);
         }
         else {
            assert( value_type == VALUE_TYPE_RELATIVE_MINUS);
            itemp = RESPONSE_CUR_VALUE(parsed_response)  - itemp;
            if (itemp < 0)
               itemp = 0;
         }
         free(parsed_response);
      }

      vrec.opcode        = feature_code;
      vrec.value_type    = DDCA_NON_TABLE_VCP_VALUE;
      vrec.val.c_nc.sh = (itemp >> 8) & 0xff;
      vrec.val.c_nc.sl = itemp & 0xff;
   }

   ddc_excp = ddc_set_vcp_value(dh, &vrec, NULL);

   if (ddc_excp) {
      ddcrc = ERRINFO_STATUS(ddc_excp);
      if (ddcrc == DDCRC_VERIFY)
         ddc_excp = errinfo_new_with_cause(ddcrc, ddc_excp, __func__,
                       "Verification failed for feature %02x", feature_code);
      else
         ddc_excp = errinfo_new_with_cause(ddcrc, ddc_excp, __func__,
                       "Setting value failed for feature x%02X, rc=%s",
                       feature_code, psc_desc(ddcrc));
   }

bye:
   dfm_free(dfm);  // handles dfm == NULL

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, ddc_excp, "");
   return ddc_excp;
}


/** Execute command SETVCP
 *
 *  @param  parsed_cmd  parsed command
 *  @param  dh          display handle
 *  @return status code
 */
Status_Errno_DDC
app_setvcp(Parsed_Cmd * parsed_cmd, Display_Handle * dh)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s", dh_repr(dh));
   Error_Info * ddc_excp = NULL;
   Status_Errno_DDC ddcrc = 0;
   for (int ndx = 0; ndx < parsed_cmd->setvcp_values->len; ndx++) {
      Parsed_Setvcp_Args * cur =
            &g_array_index(parsed_cmd->setvcp_values, Parsed_Setvcp_Args, ndx);
      if (parsed_cmd->flags & CMD_FLAG_EXPLICIT_I2C_SOURCE_ADDR)
         alt_source_addr = parsed_cmd->explicit_i2c_source_addr;
      ddc_excp = app_set_vcp_value(
            dh,
            cur->feature_code,
            cur->feature_value_type,
            cur->feature_value,
            parsed_cmd->flags & CMD_FLAG_FORCE_UNRECOGNIZED_VCP_CODE);
      if (ddc_excp) {
         f0printf(ferr(), "%s\n", ddc_excp->detail);
         if (ddc_excp->status_code == DDCRC_RETRIES)
            f0printf(ferr(), "    Try errors: %s\n", errinfo_causes_string(ddc_excp));
         ddcrc = ERRINFO_STATUS(ddc_excp);
         BASE_ERRINFO_FREE_WITH_REPORT(ddc_excp, IS_DBGTRC(debug,TRACE_GROUP));
         break;
      }
   }
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc,"");
   return ddcrc;
}


void init_app_setvcp() {
   RTTI_ADD_FUNC(app_setvcp);
   RTTI_ADD_FUNC(app_set_vcp_value);
}
