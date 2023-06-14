/** @file app_dumpload.c
 *
 *  Implement commands DUMPVCP and LOADVCP
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#ifdef TARGET_BSD
// what goes here?
#else
#include <linux/limits.h>    // PATH_MAX, NAME_MAX
#endif
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "util/error_info.h"
#include "util/file_util.h"
#include "util/glib_util.h"
#include "util/report_util.h"
#include "util/xdg_util.h"
/** \endcond */

#include "base/core.h"
#include "base/rtti.h"

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_dumpload.h"

#include "app_ddcutil/app_dumpload.h"

static const char TRACE_GROUP = DDCA_TRC_TOP;


// Filename creation

/** Uses the identifiers in an EDID and a timestamp to create a VCP filename.
 *
 * @param  edid         pointer to parsed edid
 * @param  time_millis  timestamp to use
 * @param  buf          buffer in which to return filename
 * @param  bufsz        buffer size
 */
static
void create_simple_vcp_fn_by_edid(
          Parsed_Edid * edid,
          time_t        time_millis,
          char *        buf,
          int           bufsz)
{
   assert(edid);
   assert(buf && bufsz > 80);

   char timestamp_text[30];
   format_timestamp(time_millis, timestamp_text, 30);
   g_snprintf(buf, bufsz, "%s-%s-%s.vcp",
              edid->model_name,
              edid->serial_ascii,
              timestamp_text
             );
   str_replace_char(buf, ' ', '_');     // convert blanks to underscores
}


/** Creates a VCP filename from a #Display_Handle and a timestamp.
 *
 * @param  dh           display handle
 * @param  time_millis  timestamp to use
 * @param  buf          buffer in which to return filename
 * @param  bufsz        buffer size
 */
static
void create_simple_vcp_fn_by_dh(
          Display_Handle * dh,
          time_t           time_millis,
          char *           buf,
          int              bufsz)
{
   Parsed_Edid * edid = dh->dref->pedid;
   assert(edid);
   create_simple_vcp_fn_by_edid(edid, time_millis, buf, bufsz);
}


/** Executes the DUMPVCP command, writing the output to a file.
 *
 *  @param  dh        display handle
 *  @param  filename  name of file to write to,
 *                    if NULL, the file name is generated
 *  @return status code
 *
 *  If the file name is generated, it is in the ddcutil subdirectory of the
 *  user's XDG home data directory, normally $HOME/.local/share/ddcutil/
 */
Status_Errno_DDC
app_dumpvcp_as_file(Display_Handle * dh, const char * filename)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s, filename=%p->%s", dh_repr(dh), filename, filename);

   char * actual_filename = NULL;
   FILE * fout = stdout;
   FILE * ferr = stderr;
   Dumpload_Data * data = NULL;
   Status_Errno_DDC ddcrc = dumpvcp_as_dumpload_data(dh, &data);
   if (ddcrc == 0) {
      GPtrArray * strings = convert_dumpload_data_to_string_array(data);
      FILE * output_fp = NULL;
      if (filename) {
         output_fp = fopen(filename, "w+");
         if (!output_fp) {
            ddcrc = -errno;
            f0printf(ferr, "Unable to open %s for writing: %s\n", filename, strerror(errno));
         }
         actual_filename = g_strdup(filename);
      }
      else {
         char simple_fn_buf[NAME_MAX+1];
         time_t time_millis = data->timestamp_millis;
         create_simple_vcp_fn_by_dh(
                               dh,
                               time_millis,
                               simple_fn_buf,
                               sizeof(simple_fn_buf));
         actual_filename = xdg_data_home_file("ddcutil",simple_fn_buf);
         // control with MsgLevel?
         f0printf(fout, "Writing file: %s\n", actual_filename);
         ddcrc = fopen_mkdir(actual_filename, "w+", ferr, &output_fp);
         ASSERT_IFF(output_fp, ddcrc == 0);
         if (ddcrc != 0) {
            f0printf(ferr, "Unable to create '%s', %s\n", actual_filename, strerror(-ddcrc));
         }
      }
      free_dumpload_data(data);

      if (output_fp) {
         int ct = strings->len;
         int ndx;
         for (ndx=0; ndx<ct; ndx++){
            char * nextval = g_ptr_array_index(strings, ndx);
            fprintf(output_fp, "%s\n", nextval);
         }
         fclose(output_fp);
      }
      else {
         ddcrc = -errno;
         f0printf(ferr, "Unable to open %s for writing: %s\n", actual_filename, strerror(errno));
      }

      g_ptr_array_free(strings, true);
      free(actual_filename);
   }
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "");
   return ddcrc;
}


//
// LOADVCP
//

/** Reads a file into a newly allocated Dumpload_Data struct.
 *
 *  @param  fn  file name
 *  @return pointer to newly allocated #Dumpload_Data struct.
 *          Caller is responsible for freeing
 */
static Dumpload_Data *
read_vcp_file(const char * fn)
{
   FILE * ferr = stderr;
   bool debug = false;
   DBGMSF(debug, "Starting. fn=%s  ", fn );

   Dumpload_Data * data = NULL;
   GPtrArray * g_line_array = g_ptr_array_sized_new(100);
   g_ptr_array_set_free_func(g_line_array, g_free);
   int rc = file_getlines(fn, g_line_array, false);
   if (rc < 0) {
      f0printf(ferr, "%s: %s\n", strerror(-rc), fn);
   }
   else {
      Error_Info * err = create_dumpload_data_from_g_ptr_array(g_line_array, &data);
      if (err) {
         if (err->status_code == DDCRC_BAD_DATA) {
            f0printf(ferr, "Invalid data in file %s:\n", fn);
            for (int ndx = 0; ndx < err->cause_ct; ndx++) {
               f0printf(ferr, "   %s\n", err->causes[ndx]->detail);
            }
         }
         else {
            // should never occur
            // errinfo report goes to fout, so send initial msg there as well
            f0printf(fout(), "Unexpected error reading data:\n");
            errinfo_report(err, 1);
         }
      }
      errinfo_free(err);
      g_ptr_array_free(g_line_array, true);
   }

   DBGMSF(debug, "Returning: %p  ", data );
   return data;
}


/** Apply the VCP settings stored in a file to the monitor
 *  indicated in that file.
 *
 *  @param   fn          file name
 *  @param   dh          handle for open display
 *  @return  status code
 */
Status_Errno_DDC
app_loadvcp_by_file(const char * fn, Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fn=%s, dh=%p %s", fn, dh, (dh) ? dh_repr(dh):"");

   FILE * fout = stdout;
   DDCA_Output_Level output_level = get_output_level();
   bool verbose = (output_level >= DDCA_OL_VERBOSE);
   Status_Errno_DDC ddcrc = 0;
   Error_Info * ddc_excp = NULL;

   Dumpload_Data * pdata = read_vcp_file(fn);
   if (!pdata) {
      // Redundant, read_vcp_file() issues message:
      // f0printf(ferr, "Unable to load VCP data from file: %s\n", fn);
   }
   else {
      if (verbose || debug) {
           f0printf(fout, "Loading VCP settings for monitor \"%s\", sn \"%s\" from file: %s\n",
                           pdata->model, pdata->serial_ascii, fn);
           if (debug) {
              rpt_push_output_dest(fout);
              dbgrpt_dumpload_data(pdata, 0);
              rpt_pop_output_dest();
           }
      }
      ddc_excp = loadvcp_by_dumpload_data(pdata, dh);
      if (ddc_excp) {
         ddcrc = ddc_excp->status_code;
         f0printf(ferr(),  "%s\n", ddc_excp->detail);
         errinfo_free(ddc_excp);
      }
      free_dumpload_data(pdata);
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "");
   return ddcrc;
}


#ifdef UNUSED
bool app_loadvcp(const char * fn, Display_Identifier * pdid) {
   bool debug = false;
   DBGMSF(debug, "Starting. fn = |%s|, pdid = %s", fn, did_repr(pdid));

   bool loadvcp_ok = true;
   Display_Handle * dh   = NULL;
   Display_Ref * dref = NULL;
   if (pdid) {
       dref = get_display_ref_for_display_identifier(pdid, CALLOPT_ERR_MSG);
       if (!dref)
          loadvcp_ok = false;
       else {
          ddc_open_display(dref, CALLOPT_ERR_MSG, &dh);  // rc == 0 iff dh
          if (!dh)
             loadvcp_ok = false;
       }
   }

   if (loadvcp_ok)
      loadvcp_ok = app_loadvcp_by_file(fn, dh);

   // if we opened the display, close it
   if (dh)
      ddc_close_display(dh);
   if (dref)
      free_display_ref(dref);

   DBGMSF(debug, "Done. Returning %s", sbool(loadvcp_ok));
   return loadvcp_ok;
}
#endif


void init_app_dumpload() {
   RTTI_ADD_FUNC(app_dumpvcp_as_file);
   RTTI_ADD_FUNC(app_loadvcp_by_file);
}

