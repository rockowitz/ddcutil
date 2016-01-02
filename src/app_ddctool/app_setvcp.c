/* app_setvcp.c
 *
 * Created on: Jan 1, 2016
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

//#include <assert.h>

#include <errno.h>
#include <string.h>

// #include "util/output_sink.h"
// #include "util/report_util.h"

#ifdef OLD

#include "base/ddc_packets.h"
#include "base/linux_errno.h"

#include "base/parms.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_do_io.h"

#include "adl/adl_errors.h"
#include "adl/adl_shim.h"

#include "ddc/ddc_edid.h"
#include "ddc/ddc_multi_part_io.h"

#include "ddc/vcp_feature_set.h"
#endif

#include "base/ddc_errno.h"
#include "base/msg_control.h"

#include "ddc/ddc_packet_io.h"
// #include "ddc/ddc_services.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/vcp_feature_codes.h"


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
 *
 * Returns:
 *   0          success
 *   -EINVAL (modulated)  invalid setvcp arguments, feature not writable
 *   from put_vcp_by_display_ref()
 */
// TODO: consider moving value parsing to command parser
Global_Status_Code
app_set_vcp_value_by_display_handle(
      Display_Handle * dh,
      char *           feature,
      char *           new_value,
      bool             force)
{
   Global_Status_Code         gsc = 0;
   long                       longtemp;
   Byte                       hexid;
   VCP_Feature_Table_Entry *  entry = NULL;
   bool                       good_value = false;

   Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   if ( hhs_to_byte_in_buf(feature, &hexid) )
   {
      entry = vcp_find_feature_by_hexid(hexid);
      if (!entry && force)
         entry = vcp_create_dummy_feature_for_hexid(hexid);
      if (entry) {
         if (!is_feature_writable_by_vcp_version(entry, vspec)) {
            char * feature_name =  get_version_sensitive_feature_name(entry, vspec);
            printf("Feature %s (%s) is not writable\n", feature, feature_name);
            // gsc = modulate_rc(-EINVAL, RR_ERRNO);    // TEMP - what is appropriate?
            gsc = DDCL_INVALID_OPERATION;
         }
         else {
            good_value = parse_vcp_value(new_value, &longtemp);
            if (!good_value) {
               gsc = modulate_rc(-EINVAL, RR_ERRNO);
            }
         }
      }
   }
   if (!entry) {
      printf("Unrecognized VCP feature code: %s\n", feature);
      // gsc = modulate_rc(-EINVAL, RR_ERRNO);
      gsc = DDCL_UNKNOWN_FEATURE;
   }

   if (entry && good_value) {
      gsc = set_nontable_vcp_value_by_dh(dh, entry->code, (int) longtemp);

      if (gsc != 0) {
         // Is this proper error message?
         printf("Setting value failed. rc=%d: %s\n", gsc , gsc_desc(gsc));
      }
   }

   return gsc;
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


Global_Status_Code
app_set_vcp_value_by_display_ref(
   Display_Ref * dref,
   char *        feature,
   char *        new_value,
   bool          force)
{
   Display_Handle * dh = ddc_open_display(dref, EXIT_IF_FAILURE);
   Global_Status_Code gsc = app_set_vcp_value_by_display_handle(dh, feature, new_value, force);
   ddc_close_display(dh);
   return gsc;
}

