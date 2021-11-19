/** \file query_sysenv_sys_bus_pci_devices.c
 *
 *  This file contains a variant of the scan of /sys/bus/pci/devices that
 *  that focuses on attributes determined to be of significance.
 */

// Copyright (C) 2018-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <strings.h>

#include "config.h"
#include "public/ddcutil_c_api.h"
#include "public/ddcutil_types.h"

#include "util/file_util.h"
#include "util/i2c_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_filter_functions.h"
#include "util/sysfs_i2c_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"
#include "query_sysenv_simplified_sys_bus_pci_devices.h"

#include "query_sysenv_sysfs_common.h"


// Default trace class for this file
// static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_ENV;


//
//  Pruned Scan
//

//  Directory report functions

// report <device>/drm
void report_drm_dir(
      const char * dirname,   // <device>
      const char * fn,        // drm
      void *       data,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "dirname=%s, fn=%s", dirname, fn);
   char * name_val;
   bool found_name = RPT_ATTR_TEXT(0, &name_val, dirname, fn, "name");
   DBGMSF(debug, "RPT_ATTR_TEXT returned %s, name_val -> %s", sbool(found_name), name_val);
}

// report <device>/i2c-N
void report_one_i2c_dir(
      const char * dirname,     // <device>
      const char * fn,          // i2c-1, i2c-2, etc., possibly 1-0037, 1-0023, 1-0050 etc
      void *       data,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "dirname=%s, fn=%s", dirname, fn);
   rpt_nl();
   char * name_val;
   bool found_name = RPT_ATTR_TEXT(0, &name_val, dirname, fn, "name");
   RPT_ATTR_TEXT(0, NULL, dirname, fn, "i2c-dev", fn, "name");
   RPT_ATTR_TEXT(0, NULL, dirname, fn, "i2c-dev", fn, "dev");
   DBGMSF(debug, "RPT_ATTR_TEXT returned %s, name_val -> %s", sbool(found_name), name_val);
}

static
void report_one_connector(
      const char * dirname,     // <device>/drm/cardN
      const char * simple_fn,   // card0-HDMI-1 etc
      void *       data,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);

   rpt_nl();
   char * drm_dp_aux_subdir = NULL;
   char * i2c_subdir        = NULL;
   RPT_ATTR_TEXT(depth, NULL, dirname, simple_fn, "enabled");
   RPT_ATTR_TEXT(depth, NULL, dirname, simple_fn, "status");
   RPT_ATTR_TEXT(depth, NULL, dirname, simple_fn, "dpms");

   RPT_ATTR_EDID(depth, NULL, dirname, simple_fn, "edid");
   RPT_ATTR_REALPATH(depth, NULL, dirname, simple_fn, "ddc");

   RPT_ATTR_SINGLE_SUBDIR(depth, &drm_dp_aux_subdir, is_drm_dp_aux_subdir, "drm_dp_aux", dirname, simple_fn);
   if (drm_dp_aux_subdir) {     // DisplayPort
      RPT_ATTR_TEXT(0, NULL, dirname, simple_fn, drm_dp_aux_subdir, "name");
      RPT_ATTR_TEXT(0, NULL, dirname, simple_fn, drm_dp_aux_subdir, "dev");
      free(drm_dp_aux_subdir);
   }

   RPT_ATTR_SINGLE_SUBDIR(depth, &i2c_subdir, is_i2cN, "i2c-", dirname, simple_fn);
   if (i2c_subdir) {
      RPT_ATTR_TEXT(depth, NULL, dirname, simple_fn, i2c_subdir, "name");
      RPT_ATTR_TEXT(depth, NULL, dirname, simple_fn, i2c_subdir, "dev");
      free(i2c_subdir);
   }

   DBGMSF(debug, "Done");
}

void report_one_cardN(
      const char * dirname,
      const char * simple_fn,
      void *       accum,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);
   char * thisdir = g_strdup_printf("%s/%s", dirname, simple_fn);
   dir_filtered_ordered_foreach(
         thisdir,
         is_card_connector_dir,
         NULL,
         report_one_connector,
         NULL,
         depth);
   DBGMSF(debug, "Done.");
}

void report_one_pci_device(
      const char * dirname,     //
      const char * fn,          // i2c-1, i2c-2, etc., possibly 1-0037, 1-0023, 1-0050 etc
      void *       data,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "dirname=%s, fn=%s", dirname, fn);
   RPT_ATTR_TEXT(    depth, NULL, dirname, fn, "class");
   RPT_ATTR_REALPATH(depth, NULL, dirname, fn, "driver");

   char * thisdir = g_strdup_printf("%s/%s", dirname, fn);
   dir_filtered_ordered_foreach(
         thisdir,
         is_i2cN_dir,
         i2c_compare,
         report_one_i2c_dir,
         NULL,
         depth);

   if ( RPT_ATTR_NOTE_SUBDIR(0, NULL, dirname, fn, "drm"))  {
         char * drmdir = g_strdup_printf("%s/%s/drm", dirname, fn);
         dir_filtered_ordered_foreach(
               drmdir,
               is_cardN_dir,
               gaux_ptr_scomp,  // fails if card-11 etc.exist, but chance of that is vanishingly small
               report_one_cardN,
               NULL,
               depth);
   }

   rpt_nl();
}


 
void
dump_simplified_sys_bus_pci(int depth) {
   rpt_nl();
   // rpt_nl();
   // show_top_level_sys_entries(depth);
   rpt_nl();
   rpt_label(depth, "*** Simplified /sys/bus/pci/devices scan ***");
   rpt_nl();
   dir_filtered_ordered_foreach("/sys/bus/pci/devices",
                       has_class_display_or_docking_station,      // filter function
                       NULL,                    // ordering function
                       report_one_pci_device,
                       NULL,                    // accumulator
                       depth);
}


