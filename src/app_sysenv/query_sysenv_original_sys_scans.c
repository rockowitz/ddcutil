/** \file query_sysenv_original_sys_scans.c
 *
 *  This file contains the original /sys scans, one that starts at /sys/bus/drm
 *  and the second starting at /sys/bus/i2c/devices
 */

// Copyright (C) 2018-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "util/file_util.h"
#include "util/i2c_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"
#include "util/sysfs_i2c_util.h"

#include "base/core.h"

#include "query_sysenv_original_sys_scans.h"



// Directory Report Functions

// *** Detail for /sys/bus/i2c/devices (Initial Version)  ***

void one_bus_i2c_device(int busno, void * accumulator, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno);

   int d1 = depth+1;
   char pb1[PATH_MAX];
   // char pb2[PATH_MAX];

   char * dir_devices_i2cN = g_strdup_printf("/sys/bus/i2c/devices/i2c-%d", busno);
   char * real_device_dir = realpath(dir_devices_i2cN, pb1);
   rpt_vstring(depth, "Examining (5) %s -> %s", dir_devices_i2cN, real_device_dir);
   RPT_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device");
   RPT_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "name");
   char * device_class = NULL;
   if ( RPT_ATTR_TEXT(d1, &device_class, dir_devices_i2cN, "device/class") ) {
      if ( str_starts_with(device_class, "0x03") ){   // TODO: replace test
         RPT_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/boot_vga");
         RPT_ATTR_REALPATH_BASENAME( d1, NULL, dir_devices_i2cN, "device/driver");
         RPT_ATTR_REALPATH_BASENAME( d1, NULL, dir_devices_i2cN, "device/driver/module");
         RPT_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/enable");
         RPT_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/modalias");
         RPT_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/vendor");
         RPT_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/device");
         RPT_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/subsystem_vendor");
         RPT_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/subsystem_device");
         char * i2c_dev_subdir = NULL;
         RPT_ATTR_SINGLE_SUBDIR(d1, &i2c_dev_subdir,  NULL, NULL, dir_devices_i2cN, "i2c-dev");
         if (i2c_dev_subdir) {
            RPT_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "i2c-dev", i2c_dev_subdir, "dev");
            RPT_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "i2c-dev", i2c_dev_subdir, "name");
            RPT_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "i2c-dev", i2c_dev_subdir, "device");
            RPT_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "i2c-dev", i2c_dev_subdir, "subsystem");
            free(i2c_dev_subdir);
         }
      }
    }
    else {   // device/class not found
       g_snprintf(pb1, PATH_MAX, "%s/%s", dir_devices_i2cN, "device/class");
       rpt_attr_output(d1, pb1, ":", "Not found. (May be display port)");
       RPT_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "subsystem");
       bool ddc_subdir_found = RPT_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device/ddc");
       RPT_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device/device");
       RPT_ATTR_EDID(    d1, NULL, dir_devices_i2cN, "device/edid");
       RPT_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "device/status");
       RPT_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device/subsystem");

       char * realpath = NULL;
       RPT_ATTR_REALPATH(d1, &realpath, dir_devices_i2cN, "device/device");
       if (realpath) {
          rpt_attr_output(d1, "","","Skipping linked directory");
          free(realpath);
       }

       if (ddc_subdir_found) {
          RPT_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "device/ddc/name");
          RPT_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device/ddc/subsystem");

          char * i2c_dev_subdir = NULL;
          RPT_ATTR_SINGLE_SUBDIR(d1, &i2c_dev_subdir, NULL, NULL, dir_devices_i2cN, "device/ddc/i2c-dev");

          // /sys/bus/i2c/devices/i2c-N/device/ddc/i2c-dev/i2c-M
          //       dev
          //       device (link)
          //       name
          //       subsystem (link)

          RPT_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "device/ddc/i2c-dev", i2c_dev_subdir, "dev");
          RPT_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device/ddc/i2c-dev", i2c_dev_subdir, "device");
          RPT_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "device/ddc/i2c-dev", i2c_dev_subdir, "name");
          RPT_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device/ddc/i2c-dev", i2c_dev_subdir, "subsystem");
          free(i2c_dev_subdir);
       }
       //  /sys/bus/i2c/devices/i2c-N/device/drm_dp_auxNdump_sysfs_i2c

       char * drm_dp_aux_subdir = NULL;
       RPT_ATTR_SINGLE_SUBDIR(    d1, &drm_dp_aux_subdir,  str_starts_with, "drm_dp_aux", dir_devices_i2cN, "device");
       RPT_ATTR_REALPATH_BASENAME(d1, NULL, dir_devices_i2cN, "device/ddc/device/driver");
       RPT_ATTR_TEXT(             d1, NULL, dir_devices_i2cN, "device/enabled");

       if (drm_dp_aux_subdir) {
          RPT_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "device", drm_dp_aux_subdir, "dev");
          RPT_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device", drm_dp_aux_subdir, "device");
          RPT_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "device", drm_dp_aux_subdir, "name");
          RPT_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device", drm_dp_aux_subdir, "device/subsystem");
          free(drm_dp_aux_subdir);
       }
    }
   free(dir_devices_i2cN);
}

void each_i2c_device_new(const char * dirname, const char * fn, void * accumulator, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, fn=%s", dirname, fn);
   rpt_nl();
   int busno = i2c_name_to_busno(fn);
   if (busno < 0)
      rpt_vstring(1, "Unexpected I2C device name: %s", fn);
   else {
      one_bus_i2c_device(busno, NULL, 1);
   }
}


// *** Detail for /sys/class/drm (initial version) ***

void each_drm_device(const char * dirname, const char * fn, void * accumulator, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, fn=%s", dirname, fn);
   rpt_nl();
   int d1 = depth+1;

   char * drm_cardX_dir = g_strdup_printf("/sys/class/drm/%s", fn);
   char * real_cardX_dir = realpath(drm_cardX_dir, NULL);
   rpt_vstring(depth, "Examining (6) %s -> %s", drm_cardX_dir, real_cardX_dir);

   // e.g. /sys/class/drm/card0-DP-1
   RPT_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, "ddc");
   RPT_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, "device");
   char * drm_dp_aux_subdir = NULL;   // exists only if DP
   RPT_ATTR_SINGLE_SUBDIR(d1, &drm_dp_aux_subdir, str_starts_with, "drm_dp_aux", drm_cardX_dir);
   RPT_ATTR_EDID(         d1, NULL, drm_cardX_dir, "edid");
   RPT_ATTR_TEXT(         d1, NULL, drm_cardX_dir, "enabled");
   char * i2cN_subdir = NULL;  // exists only if DP
   RPT_ATTR_SINGLE_SUBDIR(d1, &i2cN_subdir, str_starts_with, "i2c-", drm_cardX_dir);
   RPT_ATTR_TEXT(         d1, NULL, drm_cardX_dir, "status");
   RPT_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, "subsystem");

   // messages subdirectories of card0/DP-1
   // e.g. /sys/class/drm/card0-DP-1/drm_dp_aux0
   //      does not exist for non-DP
   if (drm_dp_aux_subdir) {
      rpt_nl();
      RPT_ATTR_TEXT(         d1, NULL, drm_cardX_dir, drm_dp_aux_subdir, "dev");
      RPT_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, drm_dp_aux_subdir, "device");
      RPT_ATTR_TEXT(         d1, NULL, drm_cardX_dir, drm_dp_aux_subdir, "name");
   // RPT_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, drm_dp_aux_subdir, "subsystem")
      free(drm_dp_aux_subdir);
   }

   // e.g. /sys/class/drm/card0-DP-1/i2c-13
   // does not exist for non-DP

   if (i2cN_subdir) {
      rpt_nl();
      RPT_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, i2cN_subdir, "device");
      RPT_ATTR_NOTE_SUBDIR(  d1, NULL, drm_cardX_dir, i2cN_subdir, "i2c-dev");
      RPT_ATTR_TEXT(         d1, NULL, drm_cardX_dir, i2cN_subdir, "name");
      RPT_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, i2cN_subdir, "subsystem");

      rpt_nl();
      // e.g. /sys/class/drm-card0-DP-1/i2c-13/i2c-dev
      RPT_ATTR_NOTE_SUBDIR(  d1, NULL, drm_cardX_dir, i2cN_subdir, "i2c-dev", i2cN_subdir);   // or can subdir name vary?

      // e.g. /sys/class/drm-card0-DP-1/i2c-13/i2c-dev/i2c-13
      RPT_ATTR_TEXT(        d1, NULL, drm_cardX_dir, i2cN_subdir, "i2c-dev", i2cN_subdir, "dev");
      RPT_ATTR_REALPATH(    d1, NULL, drm_cardX_dir, i2cN_subdir, "i2c-dev", i2cN_subdir, "device");
      RPT_ATTR_TEXT(        d1, NULL, drm_cardX_dir, i2cN_subdir, "i2c-dev", i2cN_subdir, "name");
      RPT_ATTR_REALPATH(    d1, NULL, drm_cardX_dir, i2cN_subdir, "i2c-dev", i2cN_subdir, "subsystem");
      free(i2cN_subdir);
   }

   free(drm_cardX_dir);
   free(real_cardX_dir);
}



void dump_original_sys_scans() {
   if (get_output_level() >= DDCA_OL_VV) {
      rpt_nl();
      rpt_label(0, "*** Detail for /sys/bus/i2c/devices (Initial Version) ***");
      dir_ordered_foreach(
            "/sys/bus/i2c/devices",
            NULL,                 // fn_filter
            i2c_compare,
            each_i2c_device_new,
            NULL,                 // accumulator
            0);                   // depth

      rpt_nl();
      rpt_label(0, "*** Detail for /sys/class/drm  (Initial Version) ***");
      dir_ordered_foreach(
            "/sys/class/drm",
            drm_filter                ,
            gaux_ptr_scomp,    // GCompareFunc
            each_drm_device,    //
            NULL,                 // accumulator
            0);                   // depth

#ifdef TMI
      GPtrArray *  video_devices =   execute_shell_cmd_collect(
            "find /sys/devices -name class | xargs grep -il 0x03 | xargs dirname | xargs ls -lR");
      rpt_nl();

      rpt_vstring(0, "Display devices: (class 0x03nnnn)");
      for (int ndx = 0; ndx < video_devices->len; ndx++) {
         char * dirname = g_ptr_array_index(video_devices, ndx);
         rpt_vstring(2, "%s", dirname);
      }
#endif

   }
}
