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

#include "../app_ddctool/loadvcp.h"

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
#include <util/report_util.h>
#include <base/util.h>

#include <i2c/i2c_bus_core.h>

#include "ddc/ddc_services.h"
#include "ddc/ddc_edid.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_displays.h"



#define MAX_LOADVCP_VALUES  20

typedef struct {
   Byte   opcode;
   ushort value;
} Single_Vcp_Value;

typedef
struct {
   int    busno;
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

#ifdef USING_ITERATOR
Loadvcp_Data* loadvcp_data_from_iterator(Line_Iterator iter) {
#else
Loadvcp_Data* loadvcp_data_from_g_ptr_array(GPtrArray * garray) {
#endif
   bool debug = true;
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
               ct = sscanf(s1, "%d", &data->busno);
               if (ct == 0) {
                  fprintf(stderr, "Invalid bus number at line %d: %s\n", linectr, line);
                  validData = false;
               }
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


bool loadvcp_from_loadvcp_data(Loadvcp_Data* pdata) {
   bool debug = true;
   bool ok = false;
   // DBGMSG("Searching for monitor  " );
   if (debug) {
        printf("(%s) Loading VCP settings for monitor \"%s\", sn \"%s\" \n",
               __func__,
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

         int rc =set_nontable_vcp_value_by_dh(dh, feature_code, new_value);
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


GPtrArray * g_ptr_array_from_ntsa(Null_Terminated_String_Array ntsa) {
   int len = null_terminated_string_array_length(ntsa);
   GPtrArray * garray = g_ptr_array_sized_new(len);
   int ndx;
   for (ndx=0; ndx<len; ndx++) {
      g_ptr_array_add(garray, ntsa[ndx]);
   }
   return garray;
}




bool loadvcp_from_ntsa(Null_Terminated_String_Array ntsa) {
   bool debug = true;

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




// TODO: generalize, get default dir following XDG settings
#define USER_VCP_DATA_DIR ".local/share/icc"


char * create_simple_vcp_fn(Display_Ref * dref, time_t time_millis, char * buf, int bufsz) {
   if (bufsz == 0 || buf == NULL) {
      bufsz = 128;
      buf = calloc(1, bufsz);
   }

   char ts_buf[30];

   char * timestamp_text = format_timestamp(time_millis, ts_buf, 30);

   Parsed_Edid* edid = ddc_get_parsed_edid_by_display_ref(dref);

   // DisplayIdInfo* pIdInfo = get_display_id_info(dref);
   char buf2[64];
   if (!edid) {
      fprintf(stderr, "Display not found: %s\n", display_ref_short_name_r(dref, buf2, 64));
   }
   else {
      snprintf(buf, bufsz, "%s-%s-%s.vcp",
               edid->model_name,
               edid->serial_ascii,
               timestamp_text
              );

      // convert blanks to underscores
      char * p = buf;
      while (*p) {
         if (*p == ' ')
            *p = '_';
         p++;
      }
   }

   // DBGMSG("Returning %s", buf );
   return buf;
}





bool dumpvcp(Display_Ref * dref, char * filename) {
   bool ok = true;
   char fqfn[PATH_MAX] = {0};
   char timestamp_buf[30];
   time_t time_millis = time(NULL);
   // temporarily use same output format as filename, but format the date separately here
   // for flexibility
   format_timestamp(time_millis, timestamp_buf, sizeof(timestamp_buf));

   if (!filename) {
      char simple_fn_buf[NAME_MAX+1];
      char * simple_fn = create_simple_vcp_fn(dref, time_millis, simple_fn_buf, sizeof(simple_fn_buf));
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
#ifdef X
      fprintf(output_fp, "TIMESTAMP_TEXT %s\n", timestamp_buf );
      fprintf(output_fp, "TIMESTAMP_MILLIS %ld\n", time_millis);
#endif
#ifdef OLD
      set_output_format(OUTPUT_PROG_VCP);
      if (dref->ddc_io_mode == DDC_IO_DEVI2C)
         report_i2c_bus(dref->busno, output_fp);
      else {
         // report ADL
         // ADAPTER_INDEX, DISPLAY_INDEX, MFG_ID, MODEL, SN
      }
#endif
#ifdef X
      Parsed_Edid * edid = ddc_get_parsed_edid_by_display_ref(dref);
      fprintf(output_fp, "MFG_ID  %s\n",  edid->mfg_id);
      fprintf(output_fp, "MODEL   %s\n",  edid->model_name);
      fprintf(output_fp, "SN      %s\n",  edid->serial_ascii);

      char hexbuf[257];
      hexstring2(edid->bytes, 128,
                 NULL /* no separator */,
                 true /* uppercase */,
                 hexbuf, 257);
      fprintf(output_fp, "EDID    %s\n", hexbuf);
#endif
#ifdef OLD
      set_output_format(OUTPUT_PROG_VCP);
#endif
      GPtrArray * vals = get_profile_related_values_by_display_ref(dref);
      DBGMSG("vals->len = %d", vals->len);
#ifdef FAILS
      int ndx = 0;
      char * nextval = NULL;
      nextval = g_ptr_array_index(vals, ndx);
      while (nextval != NULL) {
         fprintf(output_fp, "%s\n", nextval);
         ndx++;
         DBGMSG("ndx = %d", ndx);
         nextval = g_ptr_array_index(vals, ndx);
         DBGMSG("nextval = %p", nextval);
         DBGMSG("nextval = |%s|", nextval);
      }
#endif
      int ct = vals->len;
      int ndx;
      for (ndx=0; ndx<ct; ndx++){
         // DBGMSG("ndx = %d", ndx);
         char * nextval = g_ptr_array_index(vals, ndx);
         // DBGMSG("nextval = %p", nextval);
         // DBGMSG("strlen(nextval)=%ld, nextval = |%s|", strlen(nextval), nextval);
         fprintf(output_fp, "%s\n", nextval);

      }
      g_ptr_array_free(vals, true);
      fclose(output_fp);

   }
   return ok;
}

char * dumpvcp_to_string_by_display_handle(Display_Handle * dh) {
   GPtrArray * vals = get_profile_related_values_by_display_handle(dh);
   int ct = vals->len;
   // DBGMSG("ct = %d", ct);
   char ** pieces = calloc(ct, sizeof(char*));
   int ndx;
   for (ndx=0; ndx < ct; ndx++) {
      pieces[ndx] = g_ptr_array_index(vals,ndx);
      // DBGMSG("pieces[%d] = %s", ndx, pieces[ndx]);
   }
   char * catenated = strjoin((const char**) pieces, ct, ";");
   // DBGMSG("strlen(catenated)=%ld, catenated=%p, catenated=|%s|", strlen(catenated), catenated, catenated);
   // DBGMSG("returning %p", catenated);
   return catenated;
}



char * dumpvcp_to_string_by_display_ref(Display_Ref * dref) {
   Display_Handle* dh = ddc_open_display(dref, EXIT_IF_FAILURE);
   char * result = dumpvcp_to_string_by_display_ref(dref);
   ddc_close_display(dh);
   return result;
}




Global_Status_Code loadvcp_from_string(char * catenated) {
   Null_Terminated_String_Array nta = strsplit(catenated, ";");
   int ct = null_terminated_string_array_length(nta);
   DBGMSG("split into %d lines", ct);
   int ndx = 0;
   for (; ndx < ct; ndx++) {
      DBGMSG("nta[ndx]=|%s|", nta[ndx]);
   }

   loadvcp_from_ntsa(nta);
   return 0;      // temp
}
