/* parsed_capabilities_feature.c
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "util/data_structures.h"

#include "vcp/vcp_feature_codes.h"

#include "vcp/parsed_capabilities_feature.h"


// Trace class for this file
// static TraceGroup TRACE_GROUP = TRC_DDC;   // currently unused, commented out to avoid warning


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

// #ifdef OLD_WAY
      Byte_Value_Array bva_values = bva_create();
      bool ok1 = store_bytehex_list(value_string_start, value_string_len, bva_values, bva_appender);
      if (!ok1) {
         fprintf(stderr,
                 "Error processing VCP feature value list into bva_values: %.*s\n",
                 value_string_len, value_string_start);
      }
// #endif
      Byte_Bit_Flags bbf_values = bbf_create();
      bool ok2 = store_bytehex_list(value_string_start, value_string_len, bbf_values, bbf_appender);
      if (!ok2) {
          fprintf(stderr,
                  "Error processing VCP feature value list into bbf_values: %.*s\n",
                  value_string_len, value_string_start);
       }
      if (debug) {
// #ifdef OLD_WAY
          DBGMSG("store_bytehex_list for bva returned %d", ok1);
// #endif
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

// #ifdef OLD_WAY
      vfr->values = bva_values;
      if (debug)
         bva_report(vfr->values, "Feature values (array):");
// #endif
      vfr->bbflags = bbf_values;
      if (debug) {
         char buf[768];
         DBGMSG("ByteBitFlags as list: %s", bbf_to_string(bbf_values,buf,768));
      }
   }

   return vfr;
}


void
free_capabilities_feature(
      Capabilities_Feature_Record * pfeat)
{
   // DBGMSG("Starting. pfeat=%p", pfeat);
   assert(pfeat);
   assert(memcmp(pfeat->marker, CAPABILITIES_FEATURE_MARKER, 4) == 0);

   if (pfeat->value_string)
      free(pfeat->value_string);

   if (pfeat->values)
      bva_free(pfeat->values);

   if (pfeat->bbflags)
      bbf_free(pfeat->bbflags);

   pfeat->marker[3] = 'x';

   free(pfeat);
   // DBGMSG("Done.");
}


void
show_capabilities_feature(
      Capabilities_Feature_Record *  vfr,
      DDCA_MCCS_Version_Spec         vcp_version)
{
   bool debug = false;
   DBGMSF(debug, "Starting. vfr=%p, vcp_version=%d.%d", vfr, vcp_version.major, vcp_version.minor);
   assert(vfr);
   assert(memcmp(vfr->marker, CAPABILITIES_FEATURE_MARKER, 4) == 0);
   printf("  Feature: %02X (%s)\n", vfr->feature_id,
          get_feature_name_by_id_and_vcp_version(vfr->feature_id, vcp_version));

   // hex_dump((Byte*) vfr, sizeof(VCP_Feature_Record));
   // if (vfr->values)
   //    report_id_array(vfr->values, "Feature values:");
   char * buf0 = NULL;
   DDCA_Output_Level ol = get_output_level();
   DBGMSF(debug,  "vfr->value_string=%p", vfr->value_string);
   if (ol >= OL_VERBOSE && vfr->value_string) {

      printf("    Values (unparsed): %s\n", vfr->value_string);
   }

   DBGMSF(debug, "vfr->values=%p", vfr->values);
   // TODO: convert to use vfr->bbflags (Byte_Bit_Flags)
   if (vfr->values) {
      DDCA_Feature_Value_Entry * feature_values =
            find_feature_values_for_capabilities(vfr->feature_id, vcp_version);

      DBGMSF(debug, "Feature values %sfound for feature 0x%02x",
                    (feature_values) ? "" : "NOT ",
                    vfr->feature_id);

      int ct = bva_length(vfr->values);
      if (feature_values) {
         if (ol >= OL_VERBOSE)
            printf("    Values (  parsed):\n");
         else
            printf("    Values:\n");
         int ndx = 0;
         for (; ndx < ct; ndx++) {
            Byte hval = bva_get(vfr->values, ndx);
            char *  value_name = get_feature_value_name(feature_values, hval);
            if (!value_name)
               value_name = "Unrecognized value";
            printf("       %02x: %s\n", hval, value_name);
         }
      }
      else {
         int required_size = 3 * ct;
         buf0 = malloc(required_size);
         char * bufend = buf0+required_size;

         int ndx = 0;
         char * pos = buf0;
         for (; ndx < ct; ndx++) {
            Byte hval = bva_get(vfr->values, ndx);
            snprintf(pos, bufend-pos, "%02X ", hval);
            pos = pos+3;
         }
         *(pos-1) = '\0';
         if (ol >= OL_VERBOSE)
            printf("    Values (  parsed): %s (interpretation unavailable)\n", buf0);
         else
            printf("    Values: %s (interpretation unavailable)\n", buf0);
      }
   }

   // assert( streq(buf0, vfr->value_string));
   if (buf0)
      free(buf0);
   DBGMSF(debug, "Done.");
}
