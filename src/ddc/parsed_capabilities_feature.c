/* vcp_feature_record.c
 *
 * Created on: Nov 1, 2015
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

#include "parsed_capabilities_feature.h"

#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/debug_util.h"

#include "base/msg_control.h"

#include "ddc/vcp_feature_codes.h"



// Trace class for this file
// static TraceGroup TRACE_GROUP = TRC_DDC;   // currently unused, commented out to avoid warning


Capabilities_Feature_Record * new_Capabilities_Feature(
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
         (Capabilities_Feature_Record *) call_calloc(1,sizeof(Capabilities_Feature_Record), "new_VCP_Feature_Record");
   memcpy(vfr->marker, CAPABILITIES_FEATURE_MARKER, 4);
   vfr->feature_id = feature_id;
   // relying on calloc to 0 all other fields

   if (value_string_start) {
      vfr->value_string = (char *) malloc( value_string_len+1);
      memcpy(vfr->value_string, value_string_start, value_string_len);
      vfr->value_string[value_string_len] = '\0';

      // single digit values or true integer values in string?
#ifdef OLD
      vfr->values = parse_id_list(value_string_start, value_string_len);
      if (debug)
         report_bva_array(vfr->values, "Feature values (array):");
#endif

      Byte_Value_Array bva_values = bva_create();
      bool ok1 = store_bytehex_list(value_string_start, value_string_len, bva_values, bva_appender);
      if (!ok1) {
         fprintf(stderr, "Error processing VCP feature value list into bva_values: %.*s\n", value_string_len, value_string_start);
      }
      Byte_Bit_Flags bbf_values = bbf_create();
      bool ok2 = store_bytehex_list(value_string_start, value_string_len, bbf_values, bbf_appender);
      if (!ok2) {
          fprintf(stderr, "Error processing VCP feature value list into bbf_values: %.*s\n", value_string_len, value_string_start);
       }
      if (debug) {
          DBGMSG("store_bytehex_list for bva returned %d", ok1);
          DBGMSG("store_bytehex_list for bbf returned %d", ok2);
          //DBGMSG("Comparing Byte_value_Array vs ByteBitFlags");
      }

#ifdef OLD
      bool compok =  bva_bbf_same_values(vfr->values, bbf_values);
      if (compok) {
         DBGMSG("Byte_Value_Array and ByteBitFlags equivalent");
      }
      else {
         DBGMSG("Byte_Value_Array and ByteBitFlags DO NOT MATCH");
         report_bva_array(vfr->values, "Byte_Value_Array contents:");
         DBGMSG("ByteBitFlags as list: %s", bbf_to_string(bbf_values));
      }
#endif

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
      vfr->values = bva_values;
      if (debug)
         bva_report(vfr->values, "Feature values (array):");
      vfr->bbflags = bbf_values;
      if (debug) {
         char buf[768];
         DBGMSG("ByteBitFlags as list: %s", bbf_to_string(bbf_values,buf,768));
      }
   }

   return vfr;
}


void free_capabilities_feature(Capabilities_Feature_Record * pfeat) {
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

   call_free(pfeat, "free_vcp_feature");
   // DBGMSG("Done.");
}


void report_capabilities_feature(Capabilities_Feature_Record * vfr, Version_Spec vcp_version) {
   // DBGMSG("Starting. vfr=%p", vfr);
   printf("  Feature: %02X (%s)\n", vfr->feature_id,
          get_feature_name_by_id_and_vcp_version(vfr->feature_id, vcp_version));

   // hex_dump((Byte*) vfr, sizeof(VCP_Feature_Record));
   // if (vfr->values)
   //    report_id_array(vfr->values, "Feature values:");
   char * buf0 = NULL;

   if (vfr->value_string) {
      printf("    Values (unparsed): %s\n", vfr->value_string);
   }

   if (vfr->values) {

      Feature_Value_Entry * feature_values = find_feature_values_for_capabilities(vfr->feature_id, vcp_version);
      // if (feature_values)
      //    DBGMSG("Feature values found for feature 0x%02x", vfr->feature_id);
      // else
      //    DBGMSG("Feature values NOT found for feature 0x%02x", vfr->feature_id);

      int ct = bva_length(vfr->values);
      if (feature_values) {
         printf("    Values (  parsed):\n");
         int ndx = 0;
         for (; ndx < ct; ndx++) {
            Byte hval = bva_get(vfr->values, ndx);
            char *  value_name = get_feature_value_name(feature_values, hval);
            if (!value_name)
               value_name = "Unrecognized value!!";
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
         printf("    Values (  parsed): %s (interpretation unavailable)\n", buf0);
      }
   }

   // assert( streq(buf0, vfr->value_string));
   if (buf0)
      free(buf0);
}



