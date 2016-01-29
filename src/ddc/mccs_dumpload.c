/* loadvcp.c
 *
 * Created on: Aug 16, 2014
 *     Author: rock
 *
 * Load/store VCP settings from/to file.
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



#include "mccs_dumpload.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <linux/limits.h>    // PATH_MAX, NAME_MAX
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "util/file_util.h"
#include "util/glib_util.h"

#include "base/ddc_errno.h"
#include "base/common.h"
#include "base/displays.h"
#include "base/ddc_packets.h"
#include "base/msg_control.h"
#include "util/report_util.h"
#include "base/util.h"
#include "base/vcp_feature_values.h"

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_output.h"
#include "ddc/ddc_edid.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_displays.h"

#include "ddc/mccs_dumpload.h"


/* Report the contents of a Dumpload_Data struct
 *
 * Arguments:
 *    data     pointer to Dumpload_Data struct
 *    depth    logical indentation depth
 *
 * Returns:
 *    nothing
 */
void report_dumpload_data(Dumpload_Data * data, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Dumpload_Data", data, depth);
   // rptIval("busno", NULL, data->busno, d1);
   // TODO: show abbreviated edidstr
   rpt_str( "mfg_id",       NULL, data->mfg_id,       d1);
   rpt_str( "model",        NULL, data->model,        d1);
   rpt_str( "serial_ascii", NULL, data->serial_ascii, d1);
   rpt_str( "edid",         NULL, data->edidstr,      d1);
   rpt_int( "vcp_value_ct", NULL, data->vcp_value_ct, d1);
#ifdef OLD
   int ndx;
   for (ndx=0; ndx < data->vcp_value_ct; ndx++) {
      char buf[100];
      Single_Vcp_Value * curval = &data->vcp_value[ndx];
      snprintf(buf, 100, "0x%02x -> %d", curval->opcode, curval->value);
      rpt_str("VCP value", NULL, buf, d1);
   }
#endif
   rpt_structure_loc("vcp_values", data->vcp_values, d1);
   if (data->vcp_values)
      report_vcp_value_set(data->vcp_values, d1);
}


/* Given an array of strings stored in a GPtrArray,
 * convert it a Dumpload_Data structure.
 *
 * Arguments:
 *    garray      array of strings
 *
 * Returns:
 *
 */

Dumpload_Data* create_dumpload_data_from_g_ptr_array(GPtrArray * garray) {
   bool debug = false;
   DBGMSF(debug, "Starting.");

   Dumpload_Data * data = calloc(1, sizeof(Dumpload_Data));
   bool validData = true;
   data->vcp_values = vcp_value_set_new(15);      // 15 = initial size

   int     ct;
   int     linectr = 0;

   while ( linectr < garray->len ) {
      char *  line = NULL;
      char    s0[32], s1[257], s2[16];
      char *  head;
      char *  rest;

      line = g_ptr_array_index(garray,linectr);
      linectr++;

      *s0 = '\0'; *s1 = '\0'; *s2 = '\0';
      head = line;
      while (*head == ' ') head++;
      ct = sscanf(head, "%31s %256s %15s", s0, s1, s2);
      if (ct > 0 && *s0 != '*' && *s0 != '#') {
         if (ct == 1) {
            printf("Invalid data at line %d: %s\n", linectr, line);
            validData = false;
         }
         else {
            rest = head + strlen(s0);;
            while (*rest == ' ') rest++;
            char * last = rest + strlen(rest) - 1;
            // we already parsed a second token, so don't need to worry that last becomes < head
            while (*last == ' ' || *last == '\n') {
               *last-- = '\0';
            }
            // DBGMSG("rest=|%s|", rest );

            if (streq(s0, "BUS")) {
               // ignore
               // ct = sscanf(s1, "%d", &data->busno);
               // if (ct == 0) {
               //    fprintf(stderr, "Invalid bus number at line %d: %s\n", linectr, line);
               //    validData = false;
               // }
            }
            else if (streq(s0, "EDID") || streq(s0, "EDIDSTR")) {
               strncpy(data->edidstr, s1, sizeof(data->edidstr));
            }
            else if (streq(s0, "MFG_ID")) {
               strncpy(data->mfg_id, s1, sizeof(data->mfg_id));
            }
            else if (streq(s0, "MODEL")) {
               strncpy(data->model, rest, sizeof(data->model));
            }
            else if (streq(s0, "SN")) {
               strncpy(data->serial_ascii, rest, sizeof(data->serial_ascii));
            }
            else if (streq(s0, "TIMESTAMP_TEXT")   ||
                     streq(s0, "TIMESTAMP_MILLIS")

                    ) {
               // do nothing, just recognize valid field
            }
            else if (streq(s0, "VCP")) {
               if (ct != 3) {
                  f0printf(FERR, "Invalid VCP data at line %d: %s\n", linectr, line);
                  validData = false;
               }
               else {
#ifdef OLD
                  int ndx = data->vcp_value_ct;
                  Single_Vcp_Value * pval = &data->vcp_value[ndx];
#endif
                  Byte feature_id;
                  bool ok = hhs_to_byte_in_buf(s1, &feature_id);
                  if (!ok) {
                     f0printf(FERR, "Invalid opcode at line %d: %s", linectr, s1);
                     validData = false;
                  }
                  else {

                     ushort feature_value;
                     ct = sscanf(s2, "%hd", &feature_value);
                     if (ct == 0) {
                        f0printf(FERR, "Invalid value for opcode at line %d: %s\n", linectr, line);
                        validData = false;
                     }
                     else {
#ifdef OLD
                     pval->opcode = feature_id;
                     pval->value  = value;
#endif

                        data->vcp_value_ct++;

                        // new way:
                        // assume non-table for now
                        // TODO: opcode and value should be saved in local vars
                        Single_Vcp_Value * valrec = create_cont_vcp_value(
                              feature_id,
                              0,   // max_val, unused for LOADVCP
                              feature_value);
                        vcp_value_set_add(data->vcp_values, valrec);
                     }
                  }
               }  // VCP
            }
            else {
               f0printf(FERR, "Unexpected field \"%s\" at line %d: %s\n", s0, linectr, line );
               validData = false;
            }
         }    // more than 1 field on line
      }       // non-comment line
   }          // one line of file

   if (!validData)
      // if (data)
      //    free_dumpload_data(data);   // UNIMPLEMENTED
      data = NULL;
   return data;
}


/* Apply VCP settings from a Dumpload_Data struct to
 * the monitor specified in that data structure.
 *
 * Arguments:
 *    pdata      pointer to Dumpload_Data instance
 *
 * Returns:
 *    TO CONVERT TO GSC
 */
Global_Status_Code loadvcp_by_dumpload_data(Dumpload_Data* pdata) {
   bool debug = false;
   if (debug) {
        DBGMSG("Loading VCP settings for monitor \"%s\", sn \"%s\" \n",
               pdata->model, pdata->serial_ascii);
        report_dumpload_data(pdata, 0);
   }

   Global_Status_Code gsc = 0;

   Display_Ref * dref = ddc_find_display_by_model_and_sn(pdata->model, pdata->serial_ascii);
   if (!dref) {
      f0printf(FERR, "Monitor not connected: %s - %s   \n", pdata->model, pdata->serial_ascii );
      gsc = DDCRC_INVALID_DISPLAY;
      goto bye;
   }

   Display_Handle * dh = ddc_open_display(dref, RETURN_ERROR_IF_FAILURE);
   if (!dh) {
      gsc = DDCRC_INVALID_DISPLAY;
      goto bye;
   }

   int ndx;
   for (ndx=0; ndx < pdata->vcp_value_ct; ndx++) {
      // new way
      Single_Vcp_Value * vrec = vcp_value_set_get(pdata->vcp_values, ndx);
#ifdef OLD
         // old way:
         Byte feature_code = pdata->vcp_value[ndx].opcode;
         int  new_value    = pdata->vcp_value[ndx].value;
         // DBGMSG("feature_code=0x%02x, new_value=%d", feature_code, new_value );
         assert(vrec->val.c.cur_val == new_value);
         assert(vrec->opcode == feature_code);
#endif
      Byte   feature_code = vrec->opcode;
      ushort new_value    = vrec->val.c.cur_val;
      gsc = set_nontable_vcp_value(dh, feature_code, new_value);
      if (gsc != 0) {
         f0printf(FERR, "Error setting value %d for VCP feature code 0x%02x: %s",
                         new_value, feature_code, gsc_desc(gsc) );
         f0printf(FERR, "Terminating.");
         break;
      }
   } // for loop
   ddc_close_display(dh);

bye:
   return gsc;
}


Global_Status_Code loadvcp_by_ntsa(Null_Terminated_String_Array ntsa) {
   bool debug = false;

   Output_Level output_level = get_output_level();
   bool verbose = (output_level >= OL_VERBOSE);
   // DBGMSG("output_level=%d, verbose=%d", output_level, verbose);
   if (debug) {
      DBGMSG("Starting.  ntsa=%p", ntsa);
      verbose = true;
   }
   Global_Status_Code gsc = 0;

   GPtrArray * garray = ntsa_to_g_ptr_array(ntsa);

   Dumpload_Data * pdata = create_dumpload_data_from_g_ptr_array(garray);
   DBGMSGF(debug, "create_dumpload_data_from_g_ptr_array() returned %p", pdata);
   if (!pdata) {
      f0printf(FERR, "Unable to load VCP data from string\n");
      gsc = DDCRC_INVALID_DATA;
   }
   else {
      if (verbose) {
           f0printf(FOUT, "Loading VCP settings for monitor \"%s\", sn \"%s\" \n",
                           pdata->model, pdata->serial_ascii);
           rpt_push_output_dest(FOUT);
           report_dumpload_data(pdata, 0);
           rpt_pop_output_dest();
      }
      gsc = loadvcp_by_dumpload_data(pdata);
   }
   return gsc;
}


// n. called from ddct_public:

Global_Status_Code loadvcp_by_string(char * catenated) {
   Null_Terminated_String_Array nta = strsplit(catenated, ";");
   Global_Status_Code gsc = loadvcp_by_ntsa(nta);
   null_terminated_string_array_free(nta);
   return gsc;
}


//
// Dumpvcp
//

#ifdef OLD
// n. called from ddct_public.c
Global_Status_Code
dumpvcp_as_string_old(Display_Handle * dh, char ** pstring) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   GPtrArray * vals = NULL;
   *pstring = NULL;
   Global_Status_Code gsc = collect_profile_related_values(dh, time(NULL), &vals);
   if (gsc == 0) {
#ifdef OLD
      int ct = vals->len;
      DBGMSG("ct = %d", ct);
      char ** pieces = calloc(ct, sizeof(char*));
      int ndx;
      for (ndx=0; ndx < ct; ndx++) {
         pieces[ndx] = g_ptr_array_index(vals,ndx);
         DBGMSG("pieces[%d] = %s", ndx, pieces[ndx]);
      }
      char * catenated = strjoin((const char**) pieces, ct, ";");
      DBGMSF(debug, "strlen(catenated)=%ld, catenated=%p, catenated=|%s|", strlen(catenated), catenated, catenated);
      *pstring = catenated;
      DBGMSF(debug, "*pstring=%p", *pstring);
#endif
      // Alternative implementation using glib:
      Null_Terminated_String_Array ntsa_pieces = g_ptr_array_to_ntsa(vals);
      // n. our Null_Terminated_String_Array is identical to glib's GStrv
      gchar sepchar = ';';
      gchar * catenated2 = g_strjoinv(&sepchar, ntsa_pieces);
      DBGMSF(debug, "catenated2=%p", catenated2);
#ifdef old
      DBGMSF(debug, "catenated2=|%s|", catenated2);
      assert(strcmp(catenated, catenated2) == 0);
#endif
      *pstring = catenated2;

      g_ptr_array_free(vals, true);
   }
   DBGMSF(debug, "Returning: %s", gsc_desc(gsc));

   return gsc;
}
#endif


/* Primary function for the DUMPVCP command.
 *
 * Writes DUMPVCP data to the in-core Dumpload_Data structure
 *
 * Arguments:
 *    dh              display handle for connected display
 *    pdumpload_data  address as which to return pointer to newly allocated
 *                    Dumpload_Data struct.  It is the responsibility of the
 *                    caller to free this data structure.
 *
 * Returns:
 *    status code
 */
Global_Status_Code
dumpvcp_as_dumpload_data(
      Display_Handle * dh,
      Dumpload_Data** pdumpload_data)
{
   bool debug = false;
   DBGMSF(debug, "Starting");
   Global_Status_Code gsc = 0;
   Dumpload_Data * dumped_data = calloc(1, sizeof(Dumpload_Data));

   // timestamp:
   dumped_data->timestamp_millis = time(NULL);

   // identification information from edid:
   Parsed_Edid * edid = ddc_get_parsed_edid_by_display_handle(dh);
   memcpy(dumped_data->mfg_id, edid->mfg_id, sizeof(dumped_data->mfg_id));
   memcpy(dumped_data->model,  edid->model_name, sizeof(dumped_data->model));
   memcpy(dumped_data->serial_ascii, edid->serial_ascii, sizeof(dumped_data->serial_ascii));
   memcpy(dumped_data->edidbytes, edid->bytes, 128);
   assert(sizeof(dumped_data->edidstr) == 257);
   hexstring2(edid->bytes, 128,
              NULL /* no separator */,
              true /* uppercase */,
              dumped_data->edidstr, 257);

   // VCP values
#ifdef OLD
   GPtrArray* collector = g_ptr_array_sized_new(50);
#endif
   Vcp_Value_Set vset = vcp_value_set_new(50);
   gsc = collect_raw_subset_values(
             dh,
             VCP_SUBSET_PROFILE,
             vset,
#ifdef OLD
             collector,
#endif
             true,               //  ignore_unsupported
             FERR);
   if (gsc == 0) {
#ifdef OLD
      // hack for now, TODO: redo properly
      DBGMSF(debug, "collector->len=%d", collector->len);
      assert(collector->len <= 20);
      assert(collector->len == vset->len);
      int ndx = 0;

      for (;ndx < collector->len; ndx++) {
         Parsed_Vcp_Response *  val =  g_ptr_array_index(collector,ndx);
         if (val->response_type != NON_TABLE_VCP_CALL) {
            gsc = DDCL_UNIMPLEMENTED;
         }
         else {
            dumped_data->vcp_value[ndx].opcode = val->non_table_response->vcp_code;
            dumped_data->vcp_value[ndx].value = val->non_table_response->sh << 8 |
                                                val->non_table_response->sl;
         }
      }
      dumped_data->vcp_value_ct = collector->len;
      // TODO: free collector
#endif

      dumped_data->vcp_values = vset;             // NOTE REFERENCE, BE CAREFUL WHEN FREE
      dumped_data->vcp_value_ct = vcp_value_set_size(vset);
#ifdef OLD
      int ndx;
      for (ndx=0; ndx < dumped_data->vcp_value_ct; ndx++) {
         Single_Vcp_Value * vrec = vcp_value_set_get(dumped_data->vcp_values,ndx);
         assert(dumped_data->vcp_value[ndx].opcode == vrec->opcode);
         assert(dumped_data->vcp_value[ndx].value == vrec->val.c.cur_val);
      }
#endif
   }


   if (gsc != 0 && dumped_data)
      free(dumped_data);
   else
      *pdumpload_data = dumped_data;
   if (debug) {
      DBGMSG("Returning: %s, *pdumpload_data=%p", gsc_desc(gsc), *pdumpload_data);
      report_dumpload_data(*pdumpload_data, 1);
   }
   return gsc;
}


/* Converts a Dumpload_Data structure to an array of strings
 *
 * Arguments:
 *    data     pointer to Dumpload_Data instance
 *
 * Returns:
 *    array of strings
 */
GPtrArray * convert_dumpload_data_to_string_array(Dumpload_Data * data) {
   bool debug = false;
   DBGMSF(debug, "Starting. data=%p", data);
   assert(data);
   if (debug)
      report_dumpload_data(data, 1);

   GPtrArray * strings = g_ptr_array_sized_new(30);

   collect_machine_readable_timestamp(data->timestamp_millis, strings);

   char buf[300];
   int bufsz = sizeof(buf)/sizeof(char);
   snprintf(buf, bufsz, "MFG_ID  %s",  data->mfg_id);
   g_ptr_array_add(strings, strdup(buf));
   snprintf(buf, bufsz, "MODEL   %s",  data->model);
   g_ptr_array_add(strings, strdup(buf));
   snprintf(buf, bufsz, "SN      %s",  data->serial_ascii);
   g_ptr_array_add(strings, strdup(buf));

   char hexbuf[257];
   hexstring2(data->edidbytes, 128,
              NULL /* no separator */,
              true /* uppercase */,
              hexbuf, 257);
   snprintf(buf, bufsz, "EDID    %s", hexbuf);
   g_ptr_array_add(strings, strdup(buf));

   int ndx = 0;
#ifdef OLD
   for (;ndx < data->vcp_value_ct; ndx++) {
      // n. get_formatted_value_for_feature_table_entry() also has code for table type values
      char buf[200];
      snprintf(buf, 200, "VCP %02X %5d", data->vcp_value[ndx].opcode, data->vcp_value[ndx].value);
      g_ptr_array_add(strings, strdup(buf));
   }
#endif
   for (ndx=0;ndx < data->vcp_values->len; ndx++) {
      // n. get_formatted_value_for_feature_table_entry() also has code for table type values
      Single_Vcp_Value * vrec = vcp_value_set_get(data->vcp_values,ndx);
      char buf[200];
      snprintf(buf, 200, "VCP %02X %5d",
                         vrec->opcode, vrec->val.c.cur_val);
      g_ptr_array_add(strings, strdup(buf));
   }
   return strings;
}


/** Joins a GPtrArray containing pointers to character strings
 *  into a single string,
 *
 *  Arguments:
 *     string   GPtrArray of strings
 *     sepstr   if non-null, separator to insert between joined strings
 *
 *  Returns:
 *     joined string
 */
char * join_string_g_ptr_array(GPtrArray* strings, char * sepstr) {
   bool debug = true;

   int ct = strings->len;
   DBGMSF(debug, "ct = %d", ct);
   char ** pieces = calloc(ct, sizeof(char*));
   int ndx;
   for (ndx=0; ndx < ct; ndx++) {
      pieces[ndx] = g_ptr_array_index(strings,ndx);
      DBGMSF(debug, "pieces[%d] = %s", ndx, pieces[ndx]);
   }
   char * catenated = strjoin((const char**) pieces, ct, sepstr);
   DBGMSF(debug, "strlen(catenated)=%ld, catenated=%p, catenated=|%s|", strlen(catenated), catenated, catenated);

#ifdef GLIB_VARIANT
   // GLIB variant failing when used with file.  why?
   Null_Terminated_String_Array ntsa_pieces = g_ptr_array_to_ntsa(strings);
   if (debug) {
      DBGMSG("ntsa_pieces before call to g_strjoinv():");
      null_terminated_string_array_show(ntsa_pieces);
   }
   // n. our Null_Terminated_String_Array is identical to glib's GStrv
   gchar sepchar = ';';
   gchar * catenated2 = g_strjoinv(&sepchar, ntsa_pieces);
   DBGMSF(debug, "catenated2=%p", catenated2);
   *pstring = catenated2;
   assert(strcmp(catenated, catenated2) == 0);
#endif

   return catenated;
}


/* Returns the output of the DUMPVCP command a single string.
 * Each field is separated by a semicolon.
 *
 * The caller is responsible for freeing the returned string.
 *
 * Arguments:
 *    dh       display handle of open monnitor
 *    pstring  location at which to return string
 *
 * Returns:
 *    status code
 */
// n. called from ddct_public.c
// move to glib_util.c?
Global_Status_Code
dumpvcp_as_string(Display_Handle * dh, char ** pstring) {
   bool debug = false;
   DBGMSF(debug, "Starting");

   Global_Status_Code gsc    = 0;
   Dumpload_Data *    data   = NULL;
   *pstring = NULL;

   gsc = dumpvcp_as_dumpload_data(dh, &data);
   if (gsc == 0) {
      GPtrArray * strings = convert_dumpload_data_to_string_array(data);
      *pstring = join_string_g_ptr_array(strings, ";");
   }
   DBGMSF(debug, "Returning: %s, *pstring=|%s|", gsc_desc(gsc), *pstring);
   return gsc;
}
