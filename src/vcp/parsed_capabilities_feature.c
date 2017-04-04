/* parsed_capabilities_feature.c
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \file
 * Describes one VCP feature in a capabilities string.
 *
 * The functions in this file are used only in parse_capabilities.c,
 * but were extracted for clarity.
 */

/** \cond */
#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
/** \endcond */

#include "util/data_structures.h"

#include "vcp/vcp_feature_codes.h"

#include "vcp/parsed_capabilities_feature.h"


// Trace class for this file
// static TraceGroup TRACE_GROUP = TRC_DDC;   // currently unused, commented out to avoid warning

/** Given a feature code and the unparenthesized value string extracted
 *  from a capabilities string, parses the value string and creates
 *  a #Capabilities_Feature_Record.
 *
 *  \param  feature_id
 *  \param  value_string_start start of value string, may be NULL
 *  \param  value_string_len   length of value string
 */
Capabilities_Feature_Record *
new_capabilities_feature(
      Byte   feature_id,
      char * value_string_start,
      int    value_string_len)
{
   bool debug = false;
   if (debug) {
      DBGMSG("Starting. Feature: 0x%02x", feature_id);
      if (value_string_start)
         DBGMSG("value string: |%.*s|", value_string_len, value_string_start);
      else
         DBGMSG("value_string_start = NULL");
   }

   Capabilities_Feature_Record * vfr =
         (Capabilities_Feature_Record *) calloc(1,sizeof(Capabilities_Feature_Record));
   memcpy(vfr->marker, CAPABILITIES_FEATURE_MARKER, 4);
   vfr->feature_id = feature_id;
   // relying on calloc to 0 all other fields

   if (value_string_start) {
      vfr->value_string = (char *) malloc( value_string_len+1);
      memcpy(vfr->value_string, value_string_start, value_string_len);
      vfr->value_string[value_string_len] = '\0';

#ifdef OLD_BVA
      Byte_Value_Array bva_values = bva_create();
      bool ok1 = store_bytehex_list(value_string_start, value_string_len, bva_values, bva_appender);
      if (!ok1) {
         f0printf(FOUT,
                 "Error processing VCP feature value list into bva_values: %.*s\n",
                 value_string_len, value_string_start);
      }
#endif
      Byte_Bit_Flags bbf_values = bbf_create();
      bool ok2 = store_bytehex_list(value_string_start, value_string_len, bbf_values, bbf_appender);
      if (!ok2) {
          f0printf(FOUT,
                  "Error processing VCP feature value list into bbf_values: %.*s\n",
                  value_string_len, value_string_start);
       }
      if (debug) {
#ifdef OLD_WAY
          DBGMSG("store_bytehex_list for bva returned %d", ok1);
#endif
          DBGMSG("store_bytehex_list for bbf returned %d", ok2);
          //DBGMSG("Comparing Byte_value_Array vs ByteBitFlags");
      }

#ifdef OLD_WAY
      bool compok =  bva_bbf_same_values(bva_values, bbf_values);
      if (compok) {
         if (debug)
            DBGMSG("Byte_Value_Array and ByteBitFlags equivalent");
      }
      else {
         DBGMSG("Byte_Value_Array and ByteBitFlags DO NOT MATCH");
         bva_report(bva_values, "Byte_Value_Array contents:");
         char buf[768];
         DBGMSG("ByteBitFlags as list: %s", bbf_to_string(bbf_values, buf, 768));
      }
#endif

#ifdef OLD_BVA
      vfr->values = bva_values;
      if (debug)
         bva_report(vfr->values, "Feature values (array):");
#endif
      vfr->bbflags = bbf_values;
      if (debug) {
         char buf[768];
         DBGMSG("ByteBitFlags as list: %s", bbf_to_string(bbf_values,buf,768));
      }
   }

   return vfr;
}


/** Frees a #Capabilities_Feature_Record instance.
 *
 * \param pfeat  pointer to #Capabilities_Feature_Record to free.\n
 *               If null, do nothing
 */
void
free_capabilities_feature(
      Capabilities_Feature_Record * pfeat)
{
   // DBGMSG("Starting. pfeat=%p", pfeat);
   if (pfeat) {
      assert(memcmp(pfeat->marker, CAPABILITIES_FEATURE_MARKER, 4) == 0);

      if (pfeat->value_string)
         free(pfeat->value_string);

#ifdef OLD_BVA
      // TODO: prune one implementation
      if (pfeat->values)
         bva_free(pfeat->values);
#endif

      if (pfeat->bbflags)
         bbf_free(pfeat->bbflags);

      pfeat->marker[3] = 'x';

      free(pfeat);
   }
   // DBGMSG("Done.");
}


/** Displays the contents of #Capabilities_Feature_Record as part
 *  of the **capabilities** command.
 *
 *  Output is written to the #FOUT device.
 *
 *  @param vfr         pointer to #Capabilities_Feature_Record
 *  @param vcp_version monitor VCP version, used in case feature
 *                     information is version specific
 */
void
show_capabilities_feature(
      Capabilities_Feature_Record *  vfr,
      DDCA_MCCS_Version_Spec         vcp_version)
{
   bool debug = false;
   DBGMSF(debug, "Starting. vfr=%p, vcp_version=%d.%d", vfr, vcp_version.major, vcp_version.minor);
   assert(vfr && memcmp(vfr->marker, CAPABILITIES_FEATURE_MARKER, 4) == 0);
   f0printf(FOUT, "  Feature: %02X (%s)\n",
                  vfr->feature_id,
                  get_feature_name_by_id_and_vcp_version(vfr->feature_id, vcp_version));

   DDCA_Output_Level ol = get_output_level();
   DBGMSF(debug,  "vfr->value_string=%p", vfr->value_string);
   if (ol >= DDCA_OL_VERBOSE && vfr->value_string) {
      f0printf(FOUT, "    Values (unparsed): %s\n", vfr->value_string);
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
            f0printf(FOUT, "    Values (  parsed):\n");
         else
            f0printf(FOUT, "    Values:\n");

         Byte_Bit_Flags iter = bbf_iter_new(vfr->bbflags);
         int nextval = -1;
         while ( (nextval = bbf_iter_next(iter)) >= 0) {
            assert(nextval < 256);
            char *  value_name = get_feature_value_name(feature_values, nextval);
            if (!value_name)
               value_name = "Unrecognized value";
            f0printf(FOUT, "       %02x: %s\n", nextval, value_name);
         }
         bbf_iter_free(iter);
      }
      else {          // no interpretation available, just show the values
         char * buf1 = bbf_to_string(vfr->bbflags, NULL, 0);  // allocate buffer
         if (ol >= DDCA_OL_VERBOSE)
            f0printf(FOUT, "    Values (  parsed): %s (interpretation unavailable)\n", buf1);
         else
            f0printf(FOUT, "    Values: %s (interpretation unavailable)\n", buf1);
         free(buf1);
      }
   }
// #endif



   DBGMSF(debug, "Done.");
}
