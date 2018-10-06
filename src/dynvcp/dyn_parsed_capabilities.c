/** @file ddc_parsed_capabilities.c
 *
 *  Parse the capabilities string returned by DDC, query the parsed data structure.
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <dynvcp/dyn_parsed_capabilities.h>

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
/** \endcond */

#include "util/data_structures.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/displays.h"

#include "vcp/ddc_command_codes.h"
#include "vcp/parsed_capabilities_feature.h"
#include "vcp/vcp_feature_codes.h"

#include "vcp/parse_capabilities.h"
#include "vcp/parsed_capabilities_feature.h"



// From parsed_capabiliies_feature.c:

/** Displays the contents of a #Capabilities_Feature_Record as part
 *  of the **capabilities** command.
 *
 *  Output is written to the #FOUT device.
 *
 *  @param vfr         pointer to #Capabilities_Feature_Record
 *  @param vcp_version monitor VCP version, used in case feature
 *                     information is version specific
 *  @param depth       logical indentation depth
 */
static void report_capabilities_feature(
      Capabilities_Feature_Record *  vfr,
      DDCA_MCCS_Version_Spec         vcp_version,
      int                            depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. vfr=%p, vcp_version=%d.%d", vfr, vcp_version.major, vcp_version.minor);
   assert(vfr && memcmp(vfr->marker, CAPABILITIES_FEATURE_MARKER, 4) == 0);

   int d0 = depth;
   int d1 = depth+1;
   int d2 = depth+2;

   rpt_vstring(d0, "Feature: %02X (%s)",
                  vfr->feature_id,
                  get_feature_name_by_id_and_vcp_version(vfr->feature_id, vcp_version));

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
            char *  value_name = get_feature_value_name(feature_values, hval);
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
      DDCA_Feature_Value_Entry * feature_values =
            find_feature_values_for_capabilities(vfr->feature_id, vcp_version);
      DBGMSF(debug, "Feature values %sfound for feature 0x%02x",
                    (feature_values) ? "" : "NOT ",
                    vfr->feature_id);

      if (feature_values) {  // did we find descriptions for the features?
         if (ol >= DDCA_OL_VERBOSE)
            rpt_vstring(d1, "Values (  parsed):");
         else
            rpt_vstring(d1, "Values:");

         Byte_Bit_Flags iter = bbf_iter_new(vfr->bbflags);
         int nextval = -1;
         while ( (nextval = bbf_iter_next(iter)) >= 0) {
            assert(nextval < 256);
            char *  value_name = get_feature_value_name(feature_values, nextval);
            if (!value_name)
               value_name = "Unrecognized value";
            rpt_vstring(d2, "%02x: %s", nextval, value_name);
         }
         bbf_iter_free(iter);
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



   DBGMSF0(debug, "Done.");
}


// from vcp/parse_capabilities.c:


//
// Report parsed data structures
//

static void report_commands(Byte_Value_Array cmd_ids, int depth) {
   rpt_label(depth, "Commands:");
   int ct = bva_length(cmd_ids);
   int ndx = 0;
   for (; ndx < ct; ndx++) {
      Byte hval = bva_get(cmd_ids, ndx);
      rpt_vstring(depth+1, "Command: %02x (%s)", hval, ddc_cmd_code_name(hval));
   }
}


static void report_features(
      GPtrArray*             features,     // GPtrArray of Capabilities_Feature_Record
      DDCA_MCCS_Version_Spec vcp_version)
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
      report_capabilities_feature(vfr, vcp_version, d1);
   }
}


/** Reports the Parsed_Capabilities struct for human consumption.
 *
 * @param pcaps pointer to ***Parsed_Capabilities***
 *
 * Output is written to the current stdout device.
 */
void report_parsed_capabilities(
      Parsed_Capabilities*     pcaps,
      DDCA_Monitor_Model_Key * mmid,    // not currently used
      int                      depth)
{
   bool debug = false;
   assert(pcaps && memcmp(pcaps->marker, PARSED_CAPABILITIES_MARKER, 4) == 0);
   DBGMSF(debug, "Starting. pcaps->raw_cmds_segment_seen=%s, pcaps->commands=%p, pcaps->vcp_features=%p",
          bool_repr(pcaps->raw_cmds_segment_seen), pcaps->commands, pcaps->vcp_features);

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
   if (pcaps->vcp_features)
      report_features(pcaps->vcp_features, pcaps->parsed_mccs_version);
   else {
      // handle pathological case of 0 length capabilities string, e.g. Samsung S32D850T
      if (pcaps->raw_vcp_features_seen)
         damaged = true;
   }
   if (damaged)
      rpt_label(d0, "Capabilities string not completely parsed");
}


