/** \file parsed_capabilities_features.c
 * Describes one VCP feature in a capabilities string.
 *
 * The functions in this file are used only in parse_capabilities.c,
 * but were extracted for clarity.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "vcp/vcp_feature_codes.h"

#include "vcp/parsed_capabilities_feature.h"


// Trace class for this file
// static TraceGroup TRACE_GROUP = TRC_DDC;   // currently unused, commented out to avoid warning


void dbgrpt_capabilities_feature_record(
      Capabilities_Feature_Record * vfr, int depth)
{
   int d1 = depth+1;
   rpt_structure_loc("Capabilities_Feature_Record", vfr, depth);
   rpt_vstring(d1, "marker:       %.4s", vfr->marker);
   rpt_vstring(d1, "feature_ide:  0x%02x", vfr->feature_id);
   if (vfr->values) {
      char * s =  bva_as_string(vfr->values, true, " ");
      rpt_vstring(d1, "values:       %s", s);
      free(s);
   }
   else
      rpt_vstring(d1, "values:       None");
   rpt_vstring(d1, "value_string: %s", vfr->value_string);
   rpt_vstring(d1, "valid_values: %s", sbool(vfr->valid_values));
}


/** Given a feature code and the unparenthesized value string extracted
 *  from a capabilities string, parses the value string and creates
 *  a #Capabilities_Feature_Record.
 *
 *  \param  feature_id
 *  \param  value_string_start start of value string, NULL if no values string
 *  \param  value_string_len   length of value string
 *  \return newly allocated #Capabilities_Feature_Record
 */
Capabilities_Feature_Record * parse_capabilities_feature(
      Byte        feature_id,
      char *      value_string_start,
      int         value_string_len,
      GPtrArray * error_messages)
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

#if !defined(CFR_BVA) && !defined(CFR_BBF)     // sanity check
      assert(false);
#endif

#ifdef CFR_BVA
      Byte_Value_Array bva_values = bva_create();
      bool ok1 = store_bytehex_list(value_string_start, value_string_len, bva_values, bva_appender);
      if (!ok1) {
         char * s = g_strdup_printf("Invalid VCP value in list for feature x%02x: %.*s",
                    feature_id, value_string_len, value_string_start);
         g_ptr_array_add(error_messages, s);
      }
      if (debug) {
         DBGMSG("store_bytehex_list for bva returned %s", sbool(ok1));
         bva_report(vfr->values, "Feature values (array):");
      }
      vfr->valid_values = ok1;
      vfr->values = bva_values;
#endif

#ifdef CFR_BBF
      Byte_Bit_Flags bbf_values = bbf_create();
      bool ok2 = store_bytehex_list(value_string_start, value_string_len, bbf_values, bbf_appender);
      if (!ok2) {
          char * s = g_strdup_printf("Invalid VCP value in list for feature x%02x: %.*s",
                     feature_id, value_string_len, value_string_start);
          g_ptr_array_add(error_messages, s);
       }

      if (debug) {
         DBGMSG("store_bytehex_list for bbf returned %s", sbool(ok2));
         char buf[768];
         DBGMSG("ByteBitFlags as list: %s", bbf_to_string(bbf_values,buf,768));
      }
      vfr->bbflags = bbf_values;  // if not testing exactly 1 of CFR_BBF and CFR_BVA will be set
      vfr->valid_values = ok2;
#endif

#if defined(CFR_BVA) && defined(CFR_BBF)
      if (ok1 && ok2) {
         DBGMSF(debug, "Comparing Byte_value_Array vs ByteBitFlags");
         assert(bva_bbf_same_values(bva_values, bbf_values));
      }
#endif

   }   // value_string_start not NULL

   return vfr;
}


/** Frees a #Capabilities_Feature_Record instance.
 *
 * \param pfeat  pointer to #Capabilities_Feature_Record to free.\n
 *               If null, do nothing
 */
void free_capabilities_feature_record(
      Capabilities_Feature_Record * pfeat)
{
   // DBGMSG("Starting. pfeat=%p", pfeat);
   if (pfeat) {
      assert(memcmp(pfeat->marker, CAPABILITIES_FEATURE_MARKER, 4) == 0);

      if (pfeat->value_string)
         free(pfeat->value_string);

      if (pfeat->values)
         bva_free(pfeat->values);

#ifdef BBF
      if (pfeat->bbflags)
         bbf_free(pfeat->bbflags);
#endif
      pfeat->marker[3] = 'x';

      free(pfeat);
   }
   // DBGMSG("Done.");
}


