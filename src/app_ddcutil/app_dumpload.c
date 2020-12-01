/** @file app_dumpload.c
 *
 *  Primary file for the DUMPVCP and LOADVCP commands
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#ifndef TARGET_BSD
#include <linux/limits.h>    // PATH_MAX, NAME_MAX
#endif
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "util/error_info.h"
#include "util/file_util.h"
#include "util/glib_util.h"
#include "util/report_util.h"
/** \endcond */

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "vcp/vcp_feature_values.h"

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_dumpload.h"
#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_vcp.h"

#include "app_ddcutil/app_dumpload.h"


// Filename creation

// TODO: generalize, get default dir following XDG settings
#define USER_VCP_DATA_DIR ".local/share/icc"


/** Uses the identifiers in an EDID and a timestamp to create a VCP filename.
 *
 * \param  edid         pointer to parsed edid
 * \param  time_millis  timestamp to use
 * \param  buf          buffer in which to return filename
 * \param  bufsz        buffer size
 */
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

   char timestamp_text[30];
   format_timestamp(time_millis, timestamp_text, 30);
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
   // Parsed_Edid* edid = ddc_get_parsed_edid_by_display_handle(dh);
   Parsed_Edid * edid = dh->dref->pedid;
   assert(edid);
   return create_simple_vcp_fn_by_edid(edid, time_millis, buf, bufsz);
}


/* Executes the DUMPVCP command, writing the output to a file.
 *
 * Arguments:
 *    dh        display handle
 *    filename  name of file to write to,
 *              if NULL, the file name is generated
 *
 * Returns:
 *    status code
 */
Public_Status_Code
dumpvcp_as_file(Display_Handle * dh, char * filename) {
   FILE * fout = stdout;
   FILE * ferr = stderr;
   bool debug = false;
   DBGMSF(debug, "Starting");
   char               fqfn[PATH_MAX] = {0};

   Public_Status_Code psc = 0;
   Dumpload_Data * data = NULL;
   psc = dumpvcp_as_dumpload_data(dh, &data);
   if (psc == 0) {
      GPtrArray * strings = convert_dumpload_data_to_string_array(data);

      if (!filename) {
         time_t time_millis = data->timestamp_millis;
         char simple_fn_buf[NAME_MAX+1];
         create_simple_vcp_fn_by_display_handle(
                               dh,
                               time_millis,
                               simple_fn_buf,
                               sizeof(simple_fn_buf));
         struct passwd * pw = getpwuid(getuid());
         const char * homedir = pw->pw_dir;

         snprintf(fqfn, PATH_MAX, "%s/%s/%s", homedir, USER_VCP_DATA_DIR, simple_fn_buf);
         // DBGMSG("fqfn=%s   ", fqfn );
         filename = fqfn;
         // control with MsgLevel?
         f0printf(fout, "Writing file: %s\n", filename);
      }
      free_dumpload_data(data);

      FILE * output_fp = fopen(filename, "w+");
      if (!output_fp) {
         int errsv = errno;
         f0printf(ferr, "Unable to open %s for writing: %s\n", fqfn, strerror(errno));
         psc = -errsv;
      }
      else {
         int ct = strings->len;
         int ndx;
         for (ndx=0; ndx<ct; ndx++){
            char * nextval = g_ptr_array_index(strings, ndx);
            fprintf(output_fp, "%s\n", nextval);
         }
         fclose(output_fp);
      }
      g_ptr_array_free(strings, true);
   }
   return psc;
}


//
// Loadvcp
//

/* Read a file into a newly allocated Dumpload_Data struct.
 *
 * \param  fn  file name
 * \return pointer to newly allocated #Dumpload_Data struct.
 *         Caller is responsible for freeing
 */
Dumpload_Data * read_vcp_file(const char * fn) {
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
            f0printf(ferr, "Invalid data:\n");
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


/* Apply the VCP settings stored in a file to the monitor
 * indicated in that file.
 *
 * Arguments:
 *    fn          file name
 *    dh          handle for open display
 *
 * Returns:  true if load succeeded, false if not
 */
// TODO: convert to Public_Status_Code
bool loadvcp_by_file(const char * fn, Display_Handle * dh) {
   FILE * fout = stdout;
   // FILE * ferr = stderr;
   bool debug = false;
   DBGMSF(debug, "Starting. fn=%s, dh=%p %s", fn, dh, (dh) ? dh_repr(dh):"");

   DDCA_Output_Level output_level = get_output_level();
   bool verbose = (output_level >= DDCA_OL_VERBOSE);
   bool ok = false;
   Public_Status_Code psc = 0;
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
         psc = ddc_excp->status_code;
         // errinfo_free(ddc_excp);
         ERRINFO_FREE_WITH_REPORT(ddc_excp, debug || report_freed_exceptions);
      }
      free_dumpload_data(pdata);
      ok = (psc == 0);
   }

   DBGMSF(debug, "Returning: %s", sbool(ok));
   return ok;
}
