/** \file parsed_capabilities_features.c
 * Describes one VCP feature in a capabilities string.
 *
 * The functions in this file are used only in parse_capabilities.c,
 * but were extracted for clarity.
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
/** \endcond */

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
Capabilities_Feature_Record * new_capabilities_feature(
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
         DBGMSG0("value_string_start = NULL");
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

// #ifdef OLD_BVA
      Byte_Value_Array bva_values = bva_create();
      bool ok1 = store_bytehex_list(value_string_start, value_string_len, bva_values, bva_appender);
      if (!ok1) {
         SEVEREMSG(
                 "Error processing VCP feature value list into bva_values: %.*s\n",
                 value_string_len, value_string_start);
      }
// #endif
      Byte_Bit_Flags bbf_values = bbf_create();
      bool ok2 = store_bytehex_list(value_string_start, value_string_len, bbf_values, bbf_appender);
      if (!ok2) {
          SEVEREMSG("Error processing VCP feature value list into bbf_values: %.*s\n",
                     value_string_len, value_string_start);
       }
      if (debug) {
// #ifdef OLD_BVA
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

 // #ifdef OLD_BVA
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


/** Frees a #Capabilities_Feature_Record instance.
 *
 * \param pfeat  pointer to #Capabilities_Feature_Record to free.\n
 *               If null, do nothing
 */
void free_capabilities_feature(
      Capabilities_Feature_Record * pfeat)
{
   // DBGMSG("Starting. pfeat=%p", pfeat);
   if (pfeat) {
      assert(memcmp(pfeat->marker, CAPABILITIES_FEATURE_MARKER, 4) == 0);

      if (pfeat->value_string)
         free(pfeat->value_string);

// #ifdef OLD_BVA
      // TODO: prune one implementation
      if (pfeat->values)
         bva_free(pfeat->values);
// #endif

      if (pfeat->bbflags)
         bbf_free(pfeat->bbflags);

      pfeat->marker[3] = 'x';

      free(pfeat);
   }
   // DBGMSG("Done.");
}


