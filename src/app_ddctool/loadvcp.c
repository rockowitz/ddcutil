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

#include <util/file_util.h>

#include <base/ddc_errno.h>
#include <base/common.h>
#include <base/displays.h>
#include <base/ddc_packets.h>
#include <util/report_util.h>
#include <base/util.h>

#include <i2c/i2c_bus_core.h>

#include "ddc/ddc_output.h"
#include "ddc/ddc_edid.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_displays.h"

#include "app_ddctool/loadvcp.h"


// Loadvcp_Data is the internal form data structure used to
// hold data being loaded.  Whatever the external form: a
// file or a string, it is converted to Loadvcp_Data and then
// written to the monitor.

#define MAX_LOADVCP_VALUES  20

typedef struct {
   Byte   opcode;
   ushort value;
} Single_Vcp_Value;

typedef
struct {
   time_t timestamp_millis;
 //  int    busno;
   Byte   edidbytes[128];
   char   edidstr[257];       // 128 byte edid as hex string (for future use)
   char   mfg_id[4];
   char   model[14];
   char   serial_ascii[14];
   int    vcp_value_ct;
   Single_Vcp_Value vcp_value[MAX_LOADVCP_VALUES];
} Loadvcp_Data;


void report_loadvcp_data(Loadvcp_Data * data, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Loadvcp_Data", data, depth);
   // rptIval("busno", NULL, data->busno, d1);
   // TODO: show abbreviated edidstr
   rpt_str( "mfg_id",       NULL, data->mfg_id,       d1);
   rpt_str( "model",        NULL, data->model,        d1);
   rpt_str( "serial_ascii", NULL, data->serial_ascii, d1);
   rpt_str( "edid",         NULL, data->edidstr,      d1);
   rpt_int( "vcp_value_ct", NULL, data->vcp_value_ct, d1);
   int ndx;
   for (ndx=0; ndx < data->vcp_value_ct; ndx++) {
      char buf[100];
      Single_Vcp_Value * curval = &data->vcp_value[ndx];
      snprintf(buf, 100, "0x%02x -> %d", curval->opcode, curval->value);
      rpt_str("VCP value", NULL, buf, d1);
   }
}



#ifdef USING_ITERATOR
typedef void   (*Func_Iter_Init)(void* object);
typedef char * (*Func_Next_Line)();
typedef bool   (*Func_Has_Next)();
typedef struct {
           Func_Iter_Init  func_init;
           Func_Next_Line  func_next;
           Func_Has_Next   func_has_next;
} Line_Iterator;

GPtrArray * iter_garray = NULL;
int         iter_garray_pos = 0;

void   iter_garray_init(void * pobj) {
   bool debug = true;
   GPtrArray * garray = (GPtrArray*) pobj;
   if (debug)
      DBGMSG("garray=%p", garray);
   iter_garray = garray;
   iter_garray_pos = 0;
}

char * iter_garray_next_line() {
   bool debug = true;
   if (debug)
      DBGMSG("Starting");

   char * result = g_ptr_array_index(iter_garray, iter_garray_pos++);

   if (debug) {
      DBGMSG("Returning %p", result);
      DBGMSG("Returning |%s|", result);
   }
   return result;
}

bool   iter_garray_has_next() {
   bool debug = true;
   if (debug)
      DBGMSG("Starting");

   bool result = (iter_garray_pos < iter_garray->len);

   return result;
}

Line_Iterator g_ptr_iter = {
        iter_garray_init,
        iter_garray_next_line,
        iter_garray_has_next
    };

Null_Terminated_String_Array  iter_ntsa     = NULL;
int                            iter_ntsa_pos = 0;
int                            iter_ntsa_len = 0;

void iter_ntsa_init(void* pobj) {
   bool debug = true;
   Null_Terminated_String_Array  ntsa = (Null_Terminated_String_Array) pobj;
   if (debug)
      DBGMSG("ntsa=%p", ntsa);
   iter_ntsa = ntsa;
   iter_ntsa_pos = 0;
   iter_ntsa_len = null_terminated_string_array_length(ntsa);
}

char * iter_ntsa_next_line() {
   bool debug = true;
   if (debug)
      DBGMSG("Starting");

   char * result = iter_ntsa[iter_ntsa_pos++];
   if (debug) {
      DBGMSG("Returning %p", result);
      DBGMSG("Returning |%s|", result);
   }
   return result;
}

bool   iter_ntsa_has_next() {
   bool debug = true;
   if (debug)
      DBGMSG("Starting");

   bool result = (iter_ntsa_pos < iter_ntsa_len);

   if (debug)
      DBGMSG("Returning %d", result);
   return result;
}

Line_Iterator ntsa_iter = {
      iter_ntsa_init,
      iter_ntsa_next_line,
      iter_ntsa_has_next
};
#endif


/* Given an array of strings stored in a GPtrArray,
 * convert it a Loadvcp_Data structure.
 */
#ifdef USING_ITERATOR
Loadvcp_Data* loadvcp_data_from_iterator(Line_Iterator iter) {
#else
Loadvcp_Data* loadvcp_data_from_g_ptr_array(GPtrArray * garray) {
#endif
   bool debug = false;
   if (debug)
      DBGMSG("Starting.");
   Loadvcp_Data * data = calloc(1, sizeof(Loadvcp_Data));

   bool validData = true;
   data = calloc(1, sizeof(Loadvcp_Data));

   // size_t len = 0;
   // ssize_t read;
   int     ct;

   int     linectr = 0;

#ifdef USING_ITERATOR
   while ( (*iter.func_has_next)() ) {       // <---
#else
   while ( linectr < garray->len ) {
#endif
      char *  line = NULL;
      char    s0[32], s1[257], s2[16];
      char *  head;
      char *  rest;

#ifdef USING_ITERATOR
      line = (*iter.func_next)();           // <---
#else
      line = g_ptr_array_index(garray,linectr);
#endif
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
                  fprintf(stderr, "Invalid VCP data at line %d: %s\n", linectr, line);
                  validData = false;
               }
               else {
                  int ndx = data->vcp_value_ct;
                  Single_Vcp_Value * pval = &data->vcp_value[ndx];
                  bool ok = hhs_to_byte_in_buf(s1, &pval->opcode);
                  if (!ok) {
                     printf("Invalid opcode at line %d: %s", linectr, s1);
                     validData = false;
                  }
                  else {
                     ct = sscanf(s2, "%hd", &pval->value);
                     if (ct == 0) {
                        fprintf(stderr, "Invalid value for opcode at line %d: %s\n", linectr, line);
                        validData = false;
                     }
                     else {
                        data->vcp_value_ct++;
                     }
                  }
               }  // VCP

            }
            else {
               fprintf(stderr, "Unexpected field \"%s\" at line %d: %s\n", s0, linectr, line );
               validData = false;
            }
         }    // more than 1 field on line
      }       // non-comment line
   }          // one line of file

   if (!validData)
      data = NULL;
   return data;
}


/* Read a file into a Loadvcp_Data data structure.
 */
Loadvcp_Data * read_vcp_file(const char * fn) {
   // DBGMSG("Starting. fn=%s  ", fn );
   Loadvcp_Data * data = NULL;
   GPtrArray * g_line_array = g_ptr_array_sized_new(100);
   // issues message if error:
   int rc = file_getlines(fn, g_line_array);
   if (rc < 0) {
      fprintf(stderr, "%s: %s\n", strerror(-rc), fn);
   }
   else {
#ifdef USING_ITERATOR
      (g_ptr_iter.func_init)(g_line_array);
      data = loadvcp_data_from_iterator(g_ptr_iter);
#else
      data = loadvcp_data_from_g_ptr_array(g_line_array);
#endif
   }
   // DBGMSG("Returning: %p  ", data );
   return data;
}


/* Apply VCP settings from a Loadvcp_Data data structure to
 * the monitor specified in that data structure.
 */
bool loadvcp_from_loadvcp_data(Loadvcp_Data* pdata) {
   bool debug = false;
   bool ok = false;
   // DBGMSG("Searching for monitor  " );
   if (debug) {
        DBGMSG("Loading VCP settings for monitor \"%s\", sn \"%s\" \n",
               pdata->model, pdata->serial_ascii);
        report_loadvcp_data(pdata, 0);
   }
   Display_Ref * dref = ddc_find_display_by_model_and_sn(pdata->model, pdata->serial_ascii);
   if (!dref) {
      fprintf(stderr, "Monitor not connected: %s - %s   \n", pdata->model, pdata->serial_ascii );
   }
   else {
      // reportDisplayRef(dref, 1);
      Display_Handle * dh = ddc_open_display(dref, EXIT_IF_FAILURE);
      int ndx;
      for (ndx=0; ndx < pdata->vcp_value_ct; ndx++) {
         Byte feature_code = pdata->vcp_value[ndx].opcode;
         int  new_value    = pdata->vcp_value[ndx].value;
         // DBGMSG("feature_code=0x%02x, new_value=%d", feature_code, new_value );

         int rc = set_nontable_vcp_value(dh, feature_code, new_value);
         if (rc != 0) {
            DBGMSG("set_vcp_for_DisplayHandle() returned %d   ", rc );
            DBGMSG("Terminating.  " );
            break;
         }
      }
      ok = true;
   }
   return ok;
}


/* Apply the VCP settings stored in a file to the monitor
 * indicated in that file.
 *
 * Arguments:
 *    fn          file name
 *
 * Returns:  true if load succeeded, false if not
 */
// TODO: convert to Global_Status_Code
bool loadvcp_from_file(const char * fn) {
   // Msg_Level msg_level = get_global_msg_level();
   Output_Level output_level = get_output_level();
   // DBGMSG("msgLevel=%d", msgLevel);
   // bool verbose = (msg_level >= VERBOSE);
   bool verbose = (output_level >= OL_VERBOSE);
   // DBGMSG("verbose=%d", verbose);
   bool ok = false;
   // DBGMSG("Starting. fn=%s  ", fn );

   Loadvcp_Data * pdata = read_vcp_file(fn);
   if (!pdata) {
      fprintf(stderr, "Unable to load VCP data from file: %s\n", fn);
   }
   else {
      if (verbose) {
           printf("Loading VCP settings for monitor \"%s\", sn \"%s\" from file: %s\n",
                  pdata->model, pdata->serial_ascii, fn);
           report_loadvcp_data(pdata, 0);
      }
      ok = loadvcp_from_loadvcp_data(pdata);
   }
   return ok;
}


/* Converts a Null_Terminated_String_Array to a GPtrArry.
 * The underlying strings are referenced, not duplicated.
 */
GPtrArray * g_ptr_array_from_ntsa(Null_Terminated_String_Array ntsa) {
   int len = null_terminated_string_array_length(ntsa);
   GPtrArray * garray = g_ptr_array_sized_new(len);
   int ndx;
   for (ndx=0; ndx<len; ndx++) {
      g_ptr_array_add(garray, ntsa[ndx]);
   }
   return garray;
}

/* Converts a GPtrArray to a Null_Terminated_String_Array.
 * The underlying strings are referenced, not duplicated.
 */
Null_Terminated_String_Array ntsa_from_g_ptr_array(GPtrArray * garray) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   assert(garray);
   Null_Terminated_String_Array ntsa = calloc(garray->len+1, sizeof(char *));
   int ndx = 0;
   for (;ndx < garray->len; ndx++) {
      // DBGMSF("&ntsa[ndx]=%p", &ntsa[ndx]);
      ntsa[ndx] = g_ptr_array_index(garray,ndx);
   }
   if (debug) {
      DBGMSG("Returning ntsa=%p", ntsa);
      null_terminated_string_array_show(ntsa);
   }
   return ntsa;
}


bool loadvcp_from_ntsa(Null_Terminated_String_Array ntsa) {
   bool debug = false;

   Output_Level output_level = get_output_level();
   bool verbose = (output_level >= OL_VERBOSE);
   // DBGMSG("output_level=%d, verbose=%d", output_level, verbose);
   if (debug) {
      DBGMSG("Starting.  ntsa=%p", ntsa);
      verbose = true;
   }
   bool ok = false;

   GPtrArray * garray = g_ptr_array_from_ntsa(ntsa);

#ifdef USING_ITERATOR
   (*ntsa_iter.func_init)(ntsa);
   (g_ptr_iter.func_init)(garray);
   if (debug)
      DBGMSG("after func_init");
// Both ways work.   Using loadvcp_from_ntsa is simpler, can change
// loadvcp_data_from_iteractor to not take iterator object
#ifdef WORKS
   Loadvcp_Data * pdata = loadvcp_data_from_iterator(ntsa_iter);
#endif
   Loadvcp_Data * pdata = loadvcp_data_from_iterator(g_ptr_iter);
#endif

   Loadvcp_Data * pdata = loadvcp_data_from_g_ptr_array(garray);

   if (debug)
      DBGMSG("loadvcp_data_from_iterator() returned %p", pdata);
   if (!pdata) {
      fprintf(stderr, "Unable to load VCP data from string\n");
   }
   else {
      if (verbose) {
           printf("Loading VCP settings for monitor \"%s\", sn \"%s\" \n",
                  pdata->model, pdata->serial_ascii);
           report_loadvcp_data(pdata, 0);
      }
      ok = loadvcp_from_loadvcp_data(pdata);
   }
   return ok;
}



// n. called from ddct_public:

Global_Status_Code loadvcp_from_string(char * catenated) {
   // bool debug = false;
   Null_Terminated_String_Array nta = strsplit(catenated, ";");
   // if (debug) {
   // int ct = null_terminated_string_array_length(nta);
   //    DBGMSG("split into %d lines", ct);
   //    int ndx = 0;
   //    for (; ndx < ct; ndx++) {
   //       DBGMSG("nta[%d]=|%s|", ndx, nta[ndx]);
   //    }
   // }
   loadvcp_from_ntsa(nta);
   null_terminated_string_array_free(nta);
   return 0;      // temp
}


//
// Dumpvcp
//

// TODO: generalize, get default dir following XDG settings
#define USER_VCP_DATA_DIR ".local/share/icc"


char * create_simple_vcp_fn_by_edid(
          Parsed_Edid * edid,
          time_t        time_millis,
          char *        buf,
          int           bufsz)
{
   assert(edid);
   if (bufsz == 0 || buf == NULL) {
      bufsz = 128;
      buf = calloc(1, bufsz);
   }

   char ts_buf[30];
   char * timestamp_text = format_timestamp(time_millis, ts_buf, 30);
   snprintf(buf, bufsz, "%s-%s-%s.vcp",
            edid->model_name,
            edid->serial_ascii,
            timestamp_text
           );
   str_replace_char(buf, ' ', '_');     // convert blanks to underscores

   // DBGMSG("Returning %s", buf );
   return buf;
}




char * create_simple_vcp_fn_by_display_handle(
          Display_Handle * dh,
          time_t           time_millis,
          char *           buf,
          int              bufsz)
{
   Parsed_Edid* edid = ddc_get_parsed_edid_by_display_handle(dh);
   assert(edid);
   return create_simple_vcp_fn_by_edid(edid, time_millis, buf, bufsz);
}





// TODO: return Global_Status_Code rather than ok
bool dumpvcp_to_file(Display_Handle * dh, char * filename) {
   bool               ok             = true;
   Global_Status_Code gsc            = 0;
   char               fqfn[PATH_MAX] = {0};
   time_t             time_millis    = time(NULL);

   if (!filename) {
      char simple_fn_buf[NAME_MAX+1];
      char * simple_fn = create_simple_vcp_fn_by_display_handle(
                            dh,
                            time_millis,
                            simple_fn_buf,
                            sizeof(simple_fn_buf));
      // DBGMSG("simple_fn=%s", simple_fn );

      snprintf(fqfn, PATH_MAX, "/home/%s/%s/%s", getlogin(), USER_VCP_DATA_DIR, simple_fn);
      // DBGMSG("fqfn=%s   ", fqfn );
      filename = fqfn;
      // control with MsgLevel?
      printf("Writing file: %s\n", filename);
   }

   FILE * output_fp = fopen(filename, "w+");
   // DBGMSG("output_fp=%p  ", output_fp );
   if (!output_fp) {
      fprintf(stderr, "(%s) Unable to open %s for writing: %s\n", __func__, fqfn, strerror(errno)  );
      ok = false;
   }
   else {
      // TODO: return status codes up the call chain to here,
      // look for DDCRC_MULTI_FEATURE_ERROR
      GPtrArray * vals = NULL;
      gsc = collect_profile_related_values(dh, time_millis, &vals);
      // DBGMSG("vals->len = %d", vals->len);
      if (gsc != 0) {
         fprintf(stderr, "Error reading at least one feature value.  File not written.\n");
         ok = false;
      }
      else {
         int ct = vals->len;
         int ndx;
         for (ndx=0; ndx<ct; ndx++){
            // DBGMSG("ndx = %d", ndx);
            char * nextval = g_ptr_array_index(vals, ndx);
            // DBGMSG("nextval = %p", nextval);
            // DBGMSG("strlen(nextval)=%ld, nextval = |%s|", strlen(nextval), nextval);
            fprintf(output_fp, "%s\n", nextval);
         }
      }
      if (vals)
         g_ptr_array_free(vals, true);
      fclose(output_fp);
   }
   return ok;
}


// n. called from ddct_public.c
Global_Status_Code
dumpvcp_to_string(Display_Handle * dh, char ** pstring) {
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
      Null_Terminated_String_Array ntsa_pieces = ntsa_from_g_ptr_array(vals);
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

// Under construction:
Global_Status_Code
dumpvcp_to_loadvcp_data(Display_Handle * dh, Loadvcp_Data** ploadvcp_data, FILE * msg_fh) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   Global_Status_Code gsc = 0;
   Loadvcp_Data * dumped_data = calloc(1, sizeof(Loadvcp_Data));
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

   GPtrArray* collector = g_ptr_array_sized_new(50);
     gsc = collect_raw_subset_values(
             dh,
             VCP_SUBSET_PROFILE,
             collector,
             true,                //  ignore_unsupported
             msg_fh);
   if (gsc == 0) {
      // hack for now, TODO: redo properly
      DBGMSG("collector->len=%d", collector->len);
      assert(collector->len <= 20);
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
   }


   if (gsc != 0 && dumped_data)
      free(dumped_data);
   else
      *ploadvcp_data = dumped_data;
   if (debug) {
      DBGMSG("Returning: %s, *ploadvcp_data=%p", gsc_desc(gsc), *ploadvcp_data);
      report_loadvcp_data(*ploadvcp_data, 1);
   }
   return gsc;
}



GPtrArray * loadvcp_data_to_string_array(Loadvcp_Data * data) {
   bool debug = false;
   DBGMSF(debug, "Starting. data=%p", data);
   assert(data);
   if (debug)
      report_loadvcp_data(data, 1);

   GPtrArray * vals = g_ptr_array_sized_new(30);

   collect_machine_readable_timestamp(data->timestamp_millis, vals);

   char buf[300];
   int bufsz = sizeof(buf)/sizeof(char);
   snprintf(buf, bufsz, "MFG_ID  %s",  data->mfg_id);
   g_ptr_array_add(vals, strdup(buf));
   snprintf(buf, bufsz, "MODEL   %s",  data->model);
   g_ptr_array_add(vals, strdup(buf));
   snprintf(buf, bufsz, "SN      %s",  data->serial_ascii);
   g_ptr_array_add(vals, strdup(buf));

   char hexbuf[257];
   hexstring2(data->edidbytes, 128,
              NULL /* no separator */,
              true /* uppercase */,
              hexbuf, 257);
   snprintf(buf, bufsz, "EDID    %s", hexbuf);
   g_ptr_array_add(vals, strdup(buf));

   int ndx = 0;
   for (;ndx < data->vcp_value_ct; ndx++) {
      // n. get_formatted_value_for_feature_table_entry() also has code for table type values
      char buf[200];
      snprintf(buf, 200, "VCP %02X %5d", data->vcp_value[ndx].opcode, data->vcp_value[ndx].value);
      g_ptr_array_add(vals, strdup(buf));


   }
   return vals;
}

// n. called from ddct_public.c
Global_Status_Code
dumpvcp_to_string_new(Display_Handle * dh, char ** pstring) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   // GPtrArray * vals = NULL;
   *pstring = NULL;
   // Under construction:
   Global_Status_Code gsc = 0;
   Loadvcp_Data * data = NULL;
   FILE * msg_fh = stdout;   // temp
   gsc = dumpvcp_to_loadvcp_data(dh, &data, msg_fh);
   if (gsc == 0) {
      GPtrArray * strings = loadvcp_data_to_string_array(data);

      int ct = strings->len;
      DBGMSG("ct = %d", ct);
      char ** pieces = calloc(ct, sizeof(char*));
      int ndx;
      for (ndx=0; ndx < ct; ndx++) {
         pieces[ndx] = g_ptr_array_index(strings,ndx);
         DBGMSG("pieces[%d] = %s", ndx, pieces[ndx]);
      }
      char * catenated = strjoin((const char**) pieces, ct, ";");
      DBGMSF(debug, "strlen(catenated)=%ld, catenated=%p, catenated=|%s|", strlen(catenated), catenated, catenated);
      *pstring = catenated;


#ifdef GLIB_VARIANT
      // GLIB variant failing when used with file.  why?
      Null_Terminated_String_Array ntsa_pieces = ntsa_from_g_ptr_array(strings);
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
      DBGMSF(debug, "*pstring = |%s|", *pstring);
   }
   DBGMSF(debug, "Returning: %s", gsc_desc(gsc));
   return gsc;
}


// Global_Status_Code
bool
dumpvcp_to_file_new(Display_Handle * dh, char * filename) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   char               fqfn[PATH_MAX] = {0};

   Global_Status_Code gsc = 0;
   Loadvcp_Data * data = NULL;
   FILE * msg_fh = stdout;   // temp
   gsc = dumpvcp_to_loadvcp_data(dh, &data, msg_fh);
   if (gsc == 0) {
      GPtrArray * strings = loadvcp_data_to_string_array(data);

      if (!filename) {
         time_t time_millis = data->timestamp_millis;
         char simple_fn_buf[NAME_MAX+1];
         char * simple_fn = create_simple_vcp_fn_by_display_handle(
                               dh,
                               time_millis,
                               simple_fn_buf,
                               sizeof(simple_fn_buf));
         // DBGMSG("simple_fn=%s", simple_fn );

         snprintf(fqfn, PATH_MAX, "/home/%s/%s/%s", getlogin(), USER_VCP_DATA_DIR, simple_fn);
         // DBGMSG("fqfn=%s   ", fqfn );
         filename = fqfn;
         // control with MsgLevel?
         printf("Writing file: %s\n", filename);
      }

      FILE * output_fp = fopen(filename, "w+");
      // DBGMSG("output_fp=%p  ", output_fp );
      if (!output_fp) {
         fprintf(stderr, "(%s) Unable to open %s for writing: %s\n", __func__, fqfn, strerror(errno)  );
         gsc =  -1;     /* TEMP FAILURE VALUE */
      }
      else {
         int ct = strings->len;
                int ndx;
                for (ndx=0; ndx<ct; ndx++){
                   // DBGMSG("ndx = %d", ndx);
                   char * nextval = g_ptr_array_index(strings, ndx);
                   // DBGMSG("nextval = %p", nextval);
                   // DBGMSG("strlen(nextval)=%ld, nextval = |%s|", strlen(nextval), nextval);
                   fprintf(output_fp, "%s\n", nextval);
                }

         fclose(output_fp);

      }

   }
   return (gsc == 0);
}





