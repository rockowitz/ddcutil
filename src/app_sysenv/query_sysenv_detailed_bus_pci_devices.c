/** \file query_sysenv_detailed_bus_pci_devices.c
 *
 *  This file contains a variant of the scan of /sys/bus/pci/devices that
 *  that performs minimal filtering of attributes.
 */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 /** \cond */
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>

#include "util/data_structures.h"
#include "util/device_id_util.h"
#include "util/file_util.h"
#include "util/i2c_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_filter_functions.h"
#include "util/sysfs_i2c_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"
#include "base/linux_errno.h"
#include "base/rtti.h"
/** \endcond */

#include "i2c/i2c_sysfs.h"

#include "query_sysenv_base.h"
#include "query_sysenv_sysfs_common.h"

#include "query_sysenv_detailed_bus_pci_devices.h"




// Directory Report Functions

void sysfs_dir_cardN_cardNconnector(
      const char * dirname,
      const char * filename,
      void *       accumulator,
      int          depth)
{
   rpt_nl();
   char dirname_fn[PATH_MAX];
   g_snprintf(dirname_fn, PATH_MAX, "%s/%s", dirname, filename);
   // DBGMSG("dirname=%s, filename=%s, dirname_fn=%s", dirname, filename, dirname_fn);
   int d0 = depth;
   // int d1 = depth+1;
   // int d2 = depth+2;

   RPT_ATTR_REALPATH( d0, NULL, dirname_fn, "device");
   RPT_ATTR_REALPATH( d0, NULL, dirname_fn, "ddc");
   RPT_ATTR_EDID(     d0, NULL, dirname_fn, "edid");
   RPT_ATTR_TEXT(     d0, NULL, dirname_fn, "enabled");
   RPT_ATTR_TEXT(     d0, NULL, dirname_fn, "status");
   RPT_ATTR_REALPATH( d0, NULL, dirname_fn, "subsystem");

   // for DP, also:
   //    drm_dp_auxN
   //    i2c-N

   char * dir_drm_dp_aux = NULL;
   RPT_ATTR_SINGLE_SUBDIR(d0, &dir_drm_dp_aux, str_starts_with, "drm_dp_aux", dirname_fn);
   if (dir_drm_dp_aux) {
      RPT_ATTR_REALPATH(d0, NULL, dirname_fn, dir_drm_dp_aux, "device");
      RPT_ATTR_TEXT(    d0, NULL, dirname_fn, dir_drm_dp_aux, "dev");


      RPT_ATTR_TEXT(    d0, NULL, dirname_fn, dir_drm_dp_aux, "name");
      RPT_ATTR_REALPATH(d0, NULL, dirname_fn, dir_drm_dp_aux, "subsystem");
      free(dir_drm_dp_aux);
   }
   char * dir_i2cN = NULL;
   RPT_ATTR_SINGLE_SUBDIR(d0, &dir_i2cN, str_starts_with, "i2c-",dirname_fn);
   if (dir_i2cN) {
      char pb1[PATH_MAX];
      g_snprintf(pb1, PATH_MAX, "%s/%s", dirname_fn, dir_i2cN);
      // sysfs_dir_i2c_dev(fqfn, dir_i2cN, accumulator, d0);
      char * dir_i2cN_i2cdev_i2cN = NULL;
      RPT_ATTR_SINGLE_SUBDIR(d0, &dir_i2cN_i2cdev_i2cN, str_starts_with, "i2c-", dirname_fn, dir_i2cN, "i2c-dev");
      if (dir_i2cN_i2cdev_i2cN) {
         RPT_ATTR_REALPATH(d0, NULL, dirname_fn, dir_i2cN, "i2c-dev", dir_i2cN_i2cdev_i2cN, "device");
         RPT_ATTR_TEXT(    d0, NULL, dirname_fn, dir_i2cN, "i2c-dev", dir_i2cN_i2cdev_i2cN, "dev");
         RPT_ATTR_TEXT(    d0, NULL, dirname_fn, dir_i2cN, "i2c-dev", dir_i2cN_i2cdev_i2cN, "name");
         RPT_ATTR_REALPATH(d0, NULL, dirname_fn, dir_i2cN, "i2c-dev", dir_i2cN_i2cdev_i2cN, "subsystem");
         free(dir_i2cN_i2cdev_i2cN);
      }
      RPT_ATTR_REALPATH( d0, NULL, dirname_fn, dir_i2cN, "device");
      RPT_ATTR_TEXT(     d0, NULL, dirname_fn, dir_i2cN, "name");
      RPT_ATTR_REALPATH( d0, NULL, dirname_fn, dir_i2cN, "subsystem");
      free(dir_i2cN);
   }
}


/**  Process all /sys/bus/pci/devices/<pci-device>/cardN directories
 *
 *  These directories exist for DisplayPort connectors
 *
 *  Note the realpath for these directories is one of
 *          /sys/bus/devices/NNNN:NN:NN.N/cardN
 *          /sys/bus/devices/NNNN:NN:NN.N/NNNN:NN:nn.N/cardN
 *  Include subdirectory i2c-dev/i2c-N
 *
 *  @param dirname      name of device directory
 *  @param filename     i2c-N
 *  @param accumulator
 *  @param depth        logical indentation depth
 */
void sysfs_dir_cardN(
      const char * dirname,
      const char * filename,
      void *       accumulator,
      int          depth)
{
   char fqfn[PATH_MAX];
   g_snprintf(fqfn, PATH_MAX, "%s/%s", dirname, filename);

   dir_ordered_foreach(
         fqfn,
         predicate_cardN,
         gaux_ptr_scomp,    // GCompareFunc
         sysfs_dir_cardN_cardNconnector,
         accumulator,
         depth);
}


/**  Process /sys/bus/pci/devices/<pci-device>/i2c-N directory
 *
 *  These directories exist for non-DP connectors
 *
 *  Note the realpath for these directories is one of
 *          /sys/bus/devices/NNNN:NN:NN.N/i2c-N
 *          /sys/bus/devices/NNNN:NN:NN.N/NNNN:NN:nn.N/i2c-N
 *  Include subdirectory i2c-dev/i2c-N
 *
 *  @param dirname      name of device directory
 *  @param filename     i2c-N
 *  @param accumulator
 *  @param depth        logical indentation depth
 */
void sysfs_dir_i2cN(
      const char * dirname,
      const char * filename,
      void *       accumulator,
      int          depth)
{
   rpt_nl();
   char fqfn[PATH_MAX];
   g_snprintf(fqfn, PATH_MAX, "%s/%s", dirname, filename);
   int d0 = depth;

   RPT_ATTR_REALPATH(d0,  NULL,     fqfn, "device");
   RPT_ATTR_TEXT(    d0,  NULL,     fqfn, "name");
   RPT_ATTR_REALPATH(d0,  NULL,     fqfn, "subsystem");
   char * i2c_dev_fn = NULL;
   RPT_ATTR_SINGLE_SUBDIR(d0, &i2c_dev_fn, streq, "i2c-dev", fqfn);
   if (i2c_dev_fn) {
      char * i2cN = NULL;
      RPT_ATTR_SINGLE_SUBDIR(d0, &i2cN,NULL, NULL, fqfn, "i2c-dev");
      RPT_ATTR_REALPATH(     d0, NULL, fqfn, "i2c-dev", i2cN, "device");
      RPT_ATTR_TEXT(         d0, NULL, fqfn, "i2c-dev", i2cN, "dev");
      RPT_ATTR_TEXT(         d0, NULL, fqfn, "i2c-dev", i2cN, "name");
      RPT_ATTR_REALPATH(     d0, NULL, fqfn, "i2c-dev", i2cN, "subsystem");
      free(i2cN);
      free(i2c_dev_fn);
   }
}


/**  Process a single /sys/bus/pci/devices/<pci-device>
 *
 *   Returns immediately if class is not a display device or docking station
 *
 *   PPPP:BB:DD:F
 *      PPPP     PCI domain
 *      BB       bus number
 *      DD       device number
 *      F        device function
 *
 *   Note the realpath for these directories is one of
 *          /sys/bus/devices/PPPP:BB:DD.F
 *          /sys/bus/devices/NNNN:NN:NN.N/PPPP:BB:DD:F
 */
void one_pci_device(
      const char * dirname,
      const char * filename,
      void *       accumulator,
      int          depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dirname=%s, filename=%s", dirname, filename);
   int d0 = depth;
   int d1 = depth+1;

   char dir_fn[PATH_MAX];
   g_snprintf(dir_fn, PATH_MAX, "%s/%s", dirname, filename);

   char * device_class = read_sysfs_attr(dir_fn, "class", false);
   if (!device_class) {
      DBGTRC_DONE(debug, DDCA_TRC_NONE, "no device_class");
      return;
   }
   unsigned class_id = h2uint(device_class);
   // DBGMSF(debug, "class_id: 0x%08x", class_id);
   //   if (str_starts_with(device_class, "0x03")) {
   if (class_id >> 16 != 0x03 &&     // Display controller
       class_id >> 16 != 0x0a)       // Docking station
   {
       DBGTRC_DONE(debug, DDCA_TRC_NONE, "class not display or docking station");
       return;
   }
   free(device_class);

   char rpath[PATH_MAX];
   // without assignment, get warning that return value of realpath() is not used
   // causes compilation failures since all warnings treated as errors
   _Pragma("GCC diagnostic push")
   _Pragma("GCC diagnostic ignored \"-Wunused-result\"")
   realpath(dir_fn, rpath);
   _Pragma("GCC diagnostic pop")

   // DBGMSG("dirname=%s, filename=%s, pb1=%s, rpath=%s", dirname, filename, pb1, rpath);
   rpt_nl();
    rpt_vstring(       d0, "Examining (7) %s/%s -> %s", dirname, filename, rpath);
    RPT_ATTR_REALPATH(d1, NULL, dirname, filename, "device");
    RPT_ATTR_TEXT(    d1, NULL, dirname, filename, "class");
    RPT_ATTR_TEXT(    d1, NULL, dirname, filename, "boot_vga");
    RPT_ATTR_REALPATH_BASENAME(d1, NULL, dirname, filename, "driver");
    RPT_ATTR_TEXT(    d1, NULL, dirname, filename, "enable");
    RPT_ATTR_TEXT(    d1, NULL, dirname, filename, "modalias");
    RPT_ATTR_TEXT(    d1, NULL, dirname, filename, "vendor");
    RPT_ATTR_TEXT(    d1, NULL, dirname, filename, "device");
    RPT_ATTR_TEXT(    d1, NULL, dirname, filename, "subsystem_vendor");
    RPT_ATTR_TEXT(    d1, NULL, dirname, filename, "subsystem_device");
    RPT_ATTR_REALPATH(d1, NULL, dirname, filename, "subsystem");
    rpt_nl();

    // Process drm subdirectory
    char * drm_fn = NULL;
    bool has_drm_dir = RPT_ATTR_SINGLE_SUBDIR(d1, &drm_fn, streq, "drm", dir_fn);
    if (has_drm_dir) {
       char dir_fn_drm[PATH_MAX];
       g_snprintf(dir_fn_drm, PATH_MAX, "%s/%s", dir_fn, "drm");
       dir_ordered_foreach(
             dir_fn_drm,
             predicate_cardN,   // only subdirectories named drm/cardN
             gaux_ptr_scomp,    // GCompareFunc
             sysfs_dir_cardN,
             accumulator,
             d1);
       free(drm_fn);
    }

    // Process i2c-N subdirectories:
    dir_ordered_foreach(
          dir_fn,
          startswith_i2c,       // only subdirectories named i2c-N
          i2c_compare,          // order by i2c device number, handles names not of form "i2c-N"
          sysfs_dir_i2cN,
          accumulator,
          d1);

    DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


void
dump_detailed_sys_bus_pci(int depth) {
   rpt_nl();
   rpt_nl();
   rpt_label(depth, "*** Detailed /sys/bus/pci/devices scan ***");
   rpt_nl();
   dir_filtered_ordered_foreach("/sys/bus/pci/devices",
                       has_class_display_or_docking_station,      // filter function
                       NULL,                    // ordering function
                       one_pci_device,
                       NULL,                    // accumulator
                       depth);
}


void init_query_detailed_bus_pci_devices() {
   RTTI_ADD_FUNC(one_pci_device);
}

