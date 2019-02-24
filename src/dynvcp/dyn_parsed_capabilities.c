/** @file dyn_parsed_capabilities.c
 *
 * Report parsed capabilities, taking into account dynamic feature definitions.
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/report_util.h"
#include "util/string_util.h"
/** \endcond */

#include "base/core.h"
#include "base/displays.h"

#include "vcp/ddc_command_codes.h"
#include "vcp/parsed_capabilities_feature.h"
#include "vcp/vcp_feature_codes.h"

#include "vcp/parse_capabilities.h"
#include "vcp/parsed_capabilities_feature.h"

#include "dynvcp/dyn_feature_codes.h"

#include "dynvcp/dyn_parsed_capabilities.h"


/** Given a byte representing an absolute gamma value, as used in
 *  feature x72 (gamma), format a string representation of that value.
 *
 *  \param  buf    buffer in which to return formatted value
 *  \param  bufsz  size of buffer
 *  \param  bgamma byte representing gamma
 *  \return buf
 */
static char *
format_absolute_gamma(char * buf, int bufsz, Byte bgamma) {
   int i_gamma = bgamma + 100;
   char sgamma1[10];
   g_snprintf(sgamma1, 10, "%d", i_gamma);
   g_snprintf(buf, bufsz, "%s.%s",
                          lsub(sgamma1, strlen(sgamma1)-2),
                          substr(sgamma1, strlen(sgamma1)-2, 2));
   return buf;
}


/** Given a byte representing a relative gamma value, as used in
 *  feature x72 (gamma), return a string representation of that value.
 *
 *  \param  relative_gamma byte representing gamma
 *  \return string representation (do not free)
 */
static char *
format_relative_gamma(Byte relative_gamma)
{
   char * desc = NULL;
   switch(relative_gamma) {
   case 0x00:   desc = "Display default gamma";    break;
   case 0x01:   desc = "Default gamma - 0.1";      break;
   case 0x02:   desc = "Default gamma - 0.2";      break;
   case 0x03:   desc = "Default gamma - 0.3";      break;
   case 0x04:   desc = "Default gamma - 0.4";      break;
   case 0x05:   desc = "Default gamma - 0.5";      break;
   case 0x06:   desc = "Default gamma - 0.6";      break;
   case 0x07:   desc = "Default gamma - 0.7";      break;
   case 0x08:   desc = "Default gamma - 0.8";      break;
   case 0x09:   desc = "Default gamma - 0.9";      break;
   case 0x0a:   desc = "Default gamma - 1.0";      break;

   case 0x11:   desc = "Default gamma + 0.1";      break;
   case 0x12:   desc = "Default gamma + 0.2";      break;
   case 0x13:   desc = "Default gamma + 0.3";      break;
   case 0x14:   desc = "Default gamma + 0.4";      break;
   case 0x15:   desc = "Default gamma + 0.5";      break;
   case 0x16:   desc = "Default gamma + 0.6";      break;
   case 0x17:   desc = "Default gamma + 0.7";      break;
   case 0x18:   desc = "Default gamma + 0.8";      break;
   case 0x19:   desc = "Default gamma + 0.9";      break;
   case 0x1a:   desc = "Default gamma + 1.0";      break;

   default:     desc = "Invalid value";
   }
   return desc;
}


/** Special handling for interpreting the "value" bytes for feature x72 (gamma).
 *
 *  \param  feature_value_bytes  bytes to interpret
 *  \param  depth                logical indentation depth
 *
 *  \remark
 *  The bytes parm needs to be Byte_Value_Arrary, not a Bit_Bytes_Flag
 *  because the former returns the bytes in the order specified, whereas
 *  the latter effectively sorts them.
 */
static void
report_gamma_capabilities(
      Byte_Value_Array               feature_value_bytes,
      int                            depth)
{
   bool debug = false;
   if (debug) {
      char * buf0 = bva_as_string(feature_value_bytes, true, " ");
      DBGMSG("feature_value_flags: %s", buf0);
      free(buf0);
   }

   int d0 = depth;
   Byte * bytes = bva_bytes(feature_value_bytes);
   int byte_ct = bva_length(feature_value_bytes);
   bool invalid_gamma_desc = false;
   bool relative_gamma = false;
   enum {gfull_range, glimited_range,gspecific_presets} gamma_mode;
   bool bypass_supported = false;
   char * absolute_tolerance_desc = "None";
   Byte specific_gammas[256];
   int  specific_gamma_ct = 0;

   if (byte_ct < 3) {
      // rpt_vstring(d0, "Invalid gamma values descriptor");
      invalid_gamma_desc = true;
      DBGMSF(debug, "Insufficient values, byte_ct=%d", byte_ct);
      goto bye;
   }

   // second value is always native gamma
   char s_native_gamma[10];
   format_absolute_gamma(s_native_gamma, 10, bytes[1]);
   DBGMSF(debug, "native gamma: %s (bytes[1] = 0x%02x)", s_native_gamma, bytes[1]);

   // third value describes range and how many specific values follow
   switch( bytes[2] ) {
   case 0xff:
   case 0xfe:
      gamma_mode = gfull_range;
      bypass_supported = (bytes[2] == 0xff) ? false : true;
      if (byte_ct != 3) {
         invalid_gamma_desc = true;
      }
      break;
   case 0xfd:
   case 0xfc:
      gamma_mode = glimited_range;
      bypass_supported = (bytes[2] == 0xfd) ? false : true;
      if (byte_ct != 5) {
         invalid_gamma_desc = true;
      }
      else {
         specific_gammas[0] = bytes[3];
         specific_gammas[1] = bytes[4];
         specific_gamma_ct = 2;
      }
      break;
   case 0xfb:
   case 0xfa:
      gamma_mode = gspecific_presets;
      bypass_supported = (bytes[2] == 0xfb) ? false : true;
      if (byte_ct < 4) {
         invalid_gamma_desc = true;
      }
      else {
         specific_gamma_ct = byte_ct - 3;
         memcpy(specific_gammas, bytes+3, specific_gamma_ct);
      }
      break;
   default:
      invalid_gamma_desc = true;

      // meaningless assignment to avoid subsequent -Werror=maybe-uninitialized error
      // when building for ppenSUSE_Leap_42.2 and OpenSUSE_Leap_42.3 under OBS
      // n. openSUSE_Leap_15.0 and openSUSE_Tumbleweed do not show this problem (2/2019)
      gamma_mode = gfull_range;
   }

   // first byte indicates relative vs absolute adjustment, and
   // for absolute adjustment the tolerance
   if (bytes[0] == 0xff) {   // relative adjustment
      DBGMSF(debug, "Relative adjustment (bytes[0] = 0x%02x)", bytes[0]);
      relative_gamma = true;
   }

   else {       // absolute adjustment
      if ( bytes[0] <= 0x0a ) {
         switch (bytes[0]) {
         case 0x00:  absolute_tolerance_desc = "ideal";   break;
         case 0x01:  absolute_tolerance_desc = "+/- 1%";  break;
         case 0x02:  absolute_tolerance_desc = "+/- 2%";  break;
         case 0x03:  absolute_tolerance_desc = "+/- 3%";  break;
         case 0x04:  absolute_tolerance_desc = "+/- 4%";  break;
         case 0x05:  absolute_tolerance_desc = "+/- 5%";  break;
         case 0x06:  absolute_tolerance_desc = "+/- 6%";  break;
         case 0x07:  absolute_tolerance_desc = "+/- 7%";  break;
         case 0x08:  absolute_tolerance_desc = "+/- 8%";  break;
         case 0x09:  absolute_tolerance_desc = "+/- 9%";  break;
         default:
            assert(bytes[0] == 0x0a);
            absolute_tolerance_desc = ">= 10%"; break;
         }
      }
      else {
         absolute_tolerance_desc = "None specified";
      }
      DBGMSF(debug, "absolute_tolerance: %s (bytes[0] = 0x%02x)", absolute_tolerance_desc, bytes[0]);
   }

   DBGMSF(debug, "Pre-output. invalid_gamma_desc=%s", sbool(invalid_gamma_desc));
   if (invalid_gamma_desc) {
      goto bye;
   }

   char * range_name = NULL;
   switch(gamma_mode) {
   case gfull_range:       range_name = "Full range";        break;
   case glimited_range:    range_name = "Limited range";     break;
   case gspecific_presets: range_name = "Specific presets";
   }
   rpt_vstring(d0, "%s of %s adjustment supported%s (%s0x%02x)",
         range_name,
         (relative_gamma) ? "relative" : "absolute",
               (bypass_supported)
               ? ", display has ability to bypass gamma correction"
                     : "",
         (debug) ? "bytes[2]=" : "",
         bytes[2]              );
   if (!relative_gamma) {   // absolute gamma
      rpt_vstring(d0, "Absolute tolerance: %s (%s=0x%02x)",
                       absolute_tolerance_desc,
                       (debug) ? "bytes[0]=" : "",
                       bytes[0]);
   }
   rpt_vstring(d0, "Native gamma: %s (0x%02x)", s_native_gamma, bytes[1]);
   if (gamma_mode == glimited_range) {
      if (relative_gamma) {
         rpt_vstring(d0, "Lower: %s (0x%02x), Upper: %s (0x%02x)",
               format_relative_gamma(bytes[3]), bytes[3],
               format_relative_gamma(bytes[4]), bytes[4]);
      }
      else {   // absolute gamma
         char sglower[10];
         char sgupper[10];
         rpt_vstring(d0, "Lower: %s (0x%02x), Upper: %s (0x%02x)",
               format_absolute_gamma(sglower, 10, bytes[3]),
               bytes[3],
               format_absolute_gamma(sgupper, 10, bytes[4]),
               bytes[4]
               );
      }
   }
   else if (gamma_mode == gspecific_presets) {
      // process specific_gammas
      char buf[300] = "\0";
      char bgamma[10];
      char * sgamma = NULL;
      for (int ndx = 0; ndx < specific_gamma_ct; ndx++) {
         Byte raw_gamma = specific_gammas[ndx];
         if (relative_gamma) {
            sgamma = format_relative_gamma(raw_gamma);
         }
         else {  // absolute gamma
            sgamma = format_absolute_gamma(bgamma, 10, raw_gamma);
         }
         char buf2[100];
         g_snprintf(buf2, 100, "%s %s (0x%02x)",
                               (ndx > 0) ? "," : "",
                               sgamma, raw_gamma);
         g_strlcat(buf, buf2, 300);
      }
      rpt_vstring(d0, "Specific presets: %s", buf);
   }   // g_specific_presets

bye:
   if (invalid_gamma_desc) {
      char * buf0 = bva_as_string(feature_value_bytes, true, " ");
      rpt_vstring(d0, "Invalid gamma descriptor: %s", buf0);
      free(buf0);
   }

}


// From parsed_capabiliies_feature.c:

/** Displays the contents of a #Capabilities_Feature_Record as part
 *  of the **capabilities** command.
 *
 *  Output is written to the #FOUT device.
 *
 *  @param vfr         pointer to #Capabilities_Feature_Record
 *  @param dref        display reference
 *  @param vcp_version monitor VCP version, used in case feature
 *                     information is version specific, from parsed capabilities if possible
 *  @param depth       logical indentation depth
 */
static void
report_capabilities_feature(
      Capabilities_Feature_Record *  vfr,
      Display_Ref *                  dref,
      DDCA_MCCS_Version_Spec         vcp_version,
      int                            depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. vfr=%p, dref=%s, vcp_version=%d.%d",
                 vfr, dref_repr_t(dref), vcp_version.major, vcp_version.minor);
   assert(vfr && memcmp(vfr->marker, CAPABILITIES_FEATURE_MARKER, 4) == 0);

   int d0 = depth;
   int d1 = depth+1;
   int d2 = depth+2;

   rpt_vstring(d0, "Feature: %02X (%s)",
                  vfr->feature_id,
                  dyn_get_feature_name(vfr->feature_id, dref));  // n. handles dref == NULL
                  // get_feature_name_by_id_and_vcp_version(vfr->feature_id, vcp_version));

   DDCA_Output_Level ol = get_output_level();
   DBGMSF(debug,  "vfr->value_string=%p", vfr->value_string);
   if (ol >= DDCA_OL_VERBOSE && vfr->value_string) {
      rpt_vstring(d1, "Values (unparsed): %s", vfr->value_string);
   }

#ifdef OLD_BVA
   // hex_dump((Byte*) vfr, sizeof(VCP_Feature_Record));
   // if (vfr->values)
   //    report_id_array(vfr->values, "Feature values:");
   char * buf0 = NULL;
   DBGMSF(debug, "vfr->values=%p", vfr->values);
   if (vfr->values) {
      // Get the descriptions of the documented values for the feature
      DDCA_Feature_Value_Entry * feature_values =
            find_feature_values_for_capabilities(vfr->feature_id, vcp_version);

      DBGMSF(debug, "Feature values %sfound for feature 0x%02x",
                    (feature_values) ? "" : "NOT ",
                    vfr->feature_id);
      int ct = bva_length(vfr->values);
      if (feature_values) {  // did we find descriptions for the features?
         if (ol >= DDCA_OL_VERBOSE)
            f0printf(FOUT, "    Values (  parsed):\n");
         else
            f0printf(FOUT, "    Values:\n");
         int ndx = 0;
         for (; ndx < ct; ndx++) {
            Byte hval = bva_get(vfr->values, ndx);
            char *  value_name = sl_value_table_lookup(feature_values, hval);
            if (!value_name)
               value_name = "Unrecognized value";
            f0printf(FOUT, "       %02x: %s\n", hval, value_name);
         }
      }
      else {          // no interpretation available, just show the values
         int required_size = 3 * ct;
         buf0 = malloc(required_size);
         char * bufend = buf0+required_size;

         char * pos = buf0;
         int ndx = 0;
         for (; ndx < ct; ndx++) {
            Byte hval = bva_get(vfr->values, ndx);
            snprintf(pos, bufend-pos, "%02X ", hval);
            pos = pos+3;
         }
         *(pos-1) = '\0';
         if (ol >= DDCA_OL_VERBOSE)
            f0printf(FOUT, "    Values (  parsed): %s (interpretation unavailable)\n", buf0);
         else
            f0printf(FOUT, "    Values: %s (interpretation unavailable)\n", buf0);
      }
   }

   // assert( streq(buf0, vfr->value_string));
   if (buf0)
      free(buf0);
#endif

// #ifdef NEW_BBF

   DBGMSF(debug, "vfr->bbflags=%p", vfr->bbflags);
   if (vfr->bbflags) {

      // Get the descriptions of the documented values for the feature
      DDCA_Feature_Value_Entry * feature_values = NULL;
      bool found_dynamic_feature = false;
      if (dref && dref->dfr) {
         DDCA_Feature_Metadata * dfr_metadata
                  = get_dynamic_feature_metadata(dref->dfr, vfr->feature_id);
         if (dfr_metadata) {
            found_dynamic_feature = true;
            feature_values = dfr_metadata->sl_values;
         }
      }
      if (!found_dynamic_feature) {
         feature_values =
            find_feature_values_for_capabilities(vfr->feature_id, vcp_version);
      }
      DBGMSF(debug, "Feature values %sfound for feature 0x%02x",
                    (feature_values) ? "" : "NOT ",
                    vfr->feature_id);

      if (feature_values || vfr->feature_id == 0x72) {  // did we find descriptions for the features?
         if (ol >= DDCA_OL_VERBOSE)
            rpt_vstring(d1, "Values (  parsed):");
         else
            rpt_vstring(d1, "Values:");

         char * dynamic_disclaimer = "";
         if (found_dynamic_feature)
            dynamic_disclaimer = " (from user defined feature definition)";

         if (vfr->feature_id == 0x72) {    // special handling for gamma
            report_gamma_capabilities(vfr->values, d2);
         }
         else {
            Byte_Bit_Flags iter = bbf_iter_new(vfr->bbflags);
            int nextval = -1;
            while ( (nextval = bbf_iter_next(iter)) >= 0) {
                  assert(nextval < 256);
                  char *  value_name = sl_value_table_lookup(feature_values, nextval);
                  if (!value_name)
                     value_name = "Unrecognized value";
                  rpt_vstring(d2, "%02x: %s%s", nextval, value_name, dynamic_disclaimer);
            }
            bbf_iter_free(iter);
         }
      }
      else {          // no interpretation available, just show the values
         char * buf1 = bbf_to_string(vfr->bbflags, NULL, 0);  // allocate buffer
         if (ol >= DDCA_OL_VERBOSE)
            rpt_vstring(d1, "Values (  parsed): %s (interpretation unavailable)", buf1);
         else
            rpt_vstring(d1, "Values: %s (interpretation unavailable)", buf1);
         free(buf1);
      }
   }
// #endif

   DBGMSF(debug, "Done.");
}


// from vcp/parse_capabilities.c:


//
// Report parsed data structures
//

static void
report_commands(Byte_Value_Array cmd_ids, int depth)
{
   rpt_label(depth, "Commands:");
   int ct = bva_length(cmd_ids);
   int ndx = 0;
   for (; ndx < ct; ndx++) {
      Byte hval = bva_get(cmd_ids, ndx);
      rpt_vstring(depth+1, "Command: %02x (%s)", hval, ddc_cmd_code_name(hval));
   }
}


static void
report_features(
      GPtrArray*             features,     // GPtrArray of Capabilities_Feature_Record
      Display_Ref *          dref,
      DDCA_MCCS_Version_Spec vcp_version)  // from parsed capabilities if possible
{
   bool debug = false;
   int d0 = 0;
   int d1 = 1;

   rpt_label(d0, "VCP Features:");
   int ct = features->len;
   int ndx;
   for (ndx=0; ndx < ct; ndx++) {
      Capabilities_Feature_Record * vfr = g_ptr_array_index(features, ndx);
      DBGMSF(debug, "vfr = %p", vfr);
      report_capabilities_feature(vfr, dref, vcp_version, d1);
   }
}


/** Reports the Parsed_Capabilities struct for human consumption.
 *
 * @param pcaps parsed capabilities
 * @param dh    display handle, may be null
 * @param dref  display reference, may be null
 * @param depth logical indentation depth
 *
 * Output is written to the current stdout device.
 *
 * @remark
 * dh/dref alternatives are needed to avoid double open of already opened device
 * @remark
 * If **dh** is non-null, dref is set to dh->dref.
 */
void dyn_report_parsed_capabilities(
      Parsed_Capabilities *    pcaps,
      Display_Handle *         dh,
      Display_Ref *            dref,
      int                      depth)
{
   bool debug = false;
   assert(pcaps && memcmp(pcaps->marker, PARSED_CAPABILITIES_MARKER, 4) == 0);
   DBGMSF(debug, "Starting. dh-%s, dref=%s, pcaps->raw_cmds_segment_seen=%s, "
                 "pcaps->commands=%p, pcaps->vcp_features=%p",
                 dh_repr_t(dh), dref_repr_t(dref), sbool(pcaps->raw_cmds_segment_seen),
                 pcaps->commands, pcaps->vcp_features);

   if (dh)
      dref = dh->dref;

   int d0 = depth;
   // int d1 = d0+1;
   DDCA_Output_Level output_level = get_output_level();
   if (output_level >= DDCA_OL_VERBOSE) {
      rpt_vstring(d0, "%s capabilities string: %s",
                      (pcaps->raw_value_synthesized) ? "Synthesized unparsed" : "Unparsed",
                      pcaps->raw_value);
   }
   bool damaged = false;
   rpt_vstring(d0, "MCCS version: %s",
                   (pcaps->mccs_version_string) ? pcaps->mccs_version_string : "not present");

   if (pcaps->commands)
      report_commands(pcaps->commands, d0);
   else {
      // not an error in the case of USB_IO, as the capabilities string was
      // synthesized and does not include a commands segment
      // also, HP LP2480zx does not have cmds segment
      if (pcaps->raw_cmds_segment_seen)
         damaged = true;
   }

   // if vcp version unspecified in capabilities string, use queried value
   DDCA_MCCS_Version_Spec vspec = pcaps->parsed_mccs_version;
   // if (true) {
   if ( vcp_version_eq(vspec, DDCA_VSPEC_UNKNOWN) || vcp_version_eq(vspec, DDCA_VSPEC_UNQUERIED)) {
      if (dh)
         vspec = get_vcp_version_by_display_handle(dh);
      else if (dref)
         vspec = get_vcp_version_by_display_ref(dref);
   }

   if (pcaps->vcp_features) {

#ifdef CAPABILITIES_TEST_CASES
      // for testing feature x72 gamma
      char * vstring = NULL;
      Capabilities_Feature_Record * cfr = NULL;

      vstring = "05 78 FB 50 64 78 8C";
       cfr = new_capabilities_feature(0x72, vstring, strlen(vstring));
      g_ptr_array_add(pcaps->vcp_features, cfr);
      vstring = "02 96 fe 50 a0";
       cfr = new_capabilities_feature(0x72, vstring, strlen(vstring));
      g_ptr_array_add(pcaps->vcp_features, cfr);
      vstring = "00 78 ff";
       cfr = new_capabilities_feature(0x72, vstring, strlen(vstring));
      g_ptr_array_add(pcaps->vcp_features, cfr);
      // invalid example in spec, inserted 3rd byte FB
      vstring = "FF 00 FB 01 03 05 07 09 11 13 15 17 19";
       cfr = new_capabilities_feature(0x72, vstring, strlen(vstring));
      g_ptr_array_add(pcaps->vcp_features, cfr);
      // invalid example in spec, 3rd byte should be bd or fc
      vstring = "FF 01 FC 05 15";
       cfr = new_capabilities_feature(0x72, vstring, strlen(vstring));
      g_ptr_array_add(pcaps->vcp_features, cfr);
      vstring = "FF 01 FE";
       cfr = new_capabilities_feature(0x72, vstring, strlen(vstring));
      g_ptr_array_add(pcaps->vcp_features, cfr);
#endif

      report_features(pcaps->vcp_features, dref, vspec);
   }
   else {
      // handle pathological case of 0 length capabilities string, e.g. Samsung S32D850T
      if (pcaps->raw_vcp_features_seen)
         damaged = true;
   }
   if (damaged)
      rpt_label(d0, "Capabilities string not completely parsed");
}

