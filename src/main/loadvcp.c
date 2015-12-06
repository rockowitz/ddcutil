/*  loadvcp.c
 *
 *  Created on: Aug 16, 2014
 *      Author: rock
 *
 *  Load/store VCP settings from/to file.
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>    // PATH_MAX, NAME_MAX
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <base/common.h>
#include <base/displays.h>
#include <util/report_util.h>
#include <base/util.h>

#include <i2c/i2c_bus_core.h>

#include <ddc/ddc_services.h>
#include <ddc/ddc_packet_io.h>
#include <ddc/ddc_vcp.h>

#include <main/loadvcp.h>

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


Loadvcp_Data * read_vcp_file(const char * fn) {
   // printf("(%s) Starting. fn=%s  \n", __func__, fn );
   Loadvcp_Data * data = NULL;
   bool validData = false;
   FILE * fp = fopen(fn, "r");
   if (!fp) {
      int errsv = errno;
      fprintf(stderr, "%s: %s\n", strerror(errsv), fn);
   }
   else {
      validData = true;
      data = calloc(1, sizeof(Loadvcp_Data));
      char * line = NULL;
      size_t len = 0;
      ssize_t read;
      int     ct;
      char    s0[32], s1[257], s2[16];
      char *  head;
      char *  rest;
      int     linectr = 0;

      while ((read = getline(&line, &len, fp)) != -1) {
         linectr++;
         // printf("Retrieved line of length %zu :\n", read);
         // printf("%s", line);
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
               // printf("(%s) rest=|%s|\n", __func__, rest );

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

      if (line)
         free(line);
   }

   // report_Loadvcp_Data(data, 0);

   if (!validData) {
      free(data);
      data = NULL;
   }

   // printf("(%s) Returning: %p  \n", __func__, data );
   return data;
}


bool loadvcp(const char * fn) {
   // Msg_Level msg_level = get_global_msg_level();
   Output_Level output_level = get_output_level();
   // printf("(%s) msgLevel=%d\n", __func__, msgLevel);
   // bool verbose = (msg_level >= VERBOSE);
   bool verbose = (output_level >= OL_VERBOSE);
   // printf("(%s) verbose=%d\n", __func__, verbose);
   bool ok = false;
   // printf("(%s) Starting. fn=%s  \n", __func__, fn );

   Loadvcp_Data * pdata = read_vcp_file(fn);
   if (!pdata) {
      fprintf(stderr, "Unable to load VCP data from file: %s\n", fn);
   }
   else {
      // printf("(%s) Searching for monitor  \n", __func__ );
      if (verbose) {
           printf("Loading VCP settings for monitor \"%s\", sn \"%s\" from file: %s\n",
                  pdata->model, pdata->serial_ascii, fn);
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
               // printf("(%s) feature_code=0x%02x, new_value=%d\n", __func__, feature_code, new_value );

               int rc =set_vcp_by_display_handle(dh, feature_code, new_value);
               if (rc != 0) {
                  printf("(%s) set_vcp_for_DisplayHandle() returned %d   \n", __func__, rc );
                  printf("(%s) Terminating.  \n", __func__ );
                  break;
               }
            }
            ok = true;
         }
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

   // printf("(%s) Returning %s\n", __func__, buf );
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
      // printf("(%s) simple_fn=%s\n", __func__, simple_fn );

      snprintf(fqfn, PATH_MAX, "/home/%s/%s/%s", getlogin(), USER_VCP_DATA_DIR, simple_fn);
      // printf("(%s) fqfn=%s   \n", __func__, fqfn );
      filename = fqfn;
      // control with MsgLevel?
      printf("Writing file: %s\n", filename);
   }


   FILE * output_fp = fopen(filename, "w+");
   // printf("(%s) output_fp=%p  \n", __func__, output_fp );
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
      printf("(%s) vals->len = %d\n", __func__, vals->len);
#ifdef FAILS
      int ndx = 0;
      char * nextval = NULL;
      nextval = g_ptr_array_index(vals, ndx);
      while (nextval != NULL) {
         fprintf(output_fp, "%s\n", nextval);
         ndx++;
         printf("(%s) ndx = %d\n", __func__, ndx);
         nextval = g_ptr_array_index(vals, ndx);
         printf("(%s) nextval = %p\n", __func__, nextval);
         printf("(%s) nextval = |%s|\n", __func__, nextval);
      }
#endif
      int ct = vals->len;
      int ndx;
      for (ndx=0; ndx<ct; ndx++){
         // printf("(%s) ndx = %d\n", __func__, ndx);
         char * nextval = g_ptr_array_index(vals, ndx);
         // printf("(%s) nextval = %p\n", __func__, nextval);
         // printf("(%s) strlen(nextval)=%ld, nextval = |%s|\n", __func__, strlen(nextval), nextval);
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
   // printf("(%s) ct = %d\n", __func__, ct);
   char ** pieces = calloc(ct, sizeof(char*));
   int ndx;
   for (ndx=0; ndx < ct; ndx++) {
      pieces[ndx] = g_ptr_array_index(vals,ndx);
      // printf("(%s) pieces[%d] = %s\n", __func__, ndx, pieces[ndx]);
   }
   char * catenated = strjoin((const char**) pieces, ct, ";");
   // printf("(%s) strlen(catenated)=%ld, catenated=%p, catenated=|%s|\n", __func__, strlen(catenated), catenated, catenated);
   // printf("(%s) returning %p\n", __func__, catenated);
   return catenated;
}



char * dumpvcp_to_string_by_display_ref(Display_Ref * dref) {
   Display_Handle* dh = ddc_open_display(dref, EXIT_IF_FAILURE);
   char * result = dumpvcp_to_string_by_display_ref(dref);
   ddc_close_display(dh);
   return result;
}


