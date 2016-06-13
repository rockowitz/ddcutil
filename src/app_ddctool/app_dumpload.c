/* app_dumpload.c
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
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <linux/limits.h>    // PATH_MAX, NAME_MAX
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "util/file_util.h"
#include "util/glib_util.h"
#include "util/report_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"

#include "vcp/vcp_feature_values.h"

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_dumpload.h"
#include "ddc/ddc_edid.h"
#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_vcp.h"

#include "app_ddctool/app_dumpload.h"


// Filename creation

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
Global_Status_Code
dumpvcp_as_file(Display_Handle * dh, char * filename) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   char               fqfn[PATH_MAX] = {0};

   Global_Status_Code gsc = 0;
   Dumpload_Data * data = NULL;
   gsc = dumpvcp_as_dumpload_data(dh, &data);
   if (gsc == 0) {
      GPtrArray * strings = convert_dumpload_data_to_string_array(data);

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
         f0printf(FOUT, "Writing file: %s\n", filename);
      }

      FILE * output_fp = fopen(filename, "w+");
      if (!output_fp) {
         int errsv = errno;
         f0printf(FERR, "Unable to open %s for writing: %s\n", fqfn, strerror(errno));
         gsc = modulate_rc(errsv, RR_ERRNO);
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
   }
   return gsc;
}




//
// Loadvcp
//

/* Read a file into a Dumpload_Data struct.
 */
Dumpload_Data * read_vcp_file(const char * fn) {
   // DBGMSG("Starting. fn=%s  ", fn );
   Dumpload_Data * data = NULL;
   GPtrArray * g_line_array = g_ptr_array_sized_new(100);
   // issues message if error:
   int rc = file_getlines(fn, g_line_array, false);
   if (rc < 0) {
      f0printf(FERR, "%s: %s\n", strerror(-rc), fn);
   }
   else {
      data = create_dumpload_data_from_g_ptr_array(g_line_array);
   }
   // DBGMSG("Returning: %p  ", data );
   return data;
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
bool loadvcp_by_file(const char * fn) {
   Output_Level output_level = get_output_level();
   bool verbose = (output_level >= OL_VERBOSE);
   bool ok = false;
   Global_Status_Code gsc = 0;
   // DBGMSG("Starting. fn=%s  ", fn );

   Dumpload_Data * pdata = read_vcp_file(fn);
   if (!pdata) {
      // Redundant, read_vcp_file() issues message:
      // f0printf(FERR, "Unable to load VCP data from file: %s\n", fn);
   }
   else {
      if (verbose) {
           f0printf(FOUT, "Loading VCP settings for monitor \"%s\", sn \"%s\" from file: %s\n",
                           pdata->model, pdata->serial_ascii, fn);
           rpt_push_output_dest(FOUT);
           report_dumpload_data(pdata, 0);
           rpt_pop_output_dest();
      }
      gsc = loadvcp_by_dumpload_data(pdata);
      ok = (gsc == 0);
   }
   return ok;
}

