/** \file i2c_sysfs.c
 *
 *  Query /sys file system for information on I2C devices
 */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 
#include "config.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/** \endcond */

#include "util/debug_util.h"
#include "util/edid.h"
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/i2c_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_util.h"
#ifdef USE_UDEV
#include "util/udev_i2c_util.h"
#endif
#include "util/utilrpt.h"

#include "base/core.h"

#include "i2c_sysfs.h"


void free_i2c_sys_info(I2C_Sys_Info * info) {
   if (info) {
      free(info->pci_device_path);
      free(info->drm_connector_path);
      free(info->connector);
      free(info->linked_ddc_filename);
      free(info->device_name);
      free(info->drm_dp_aux_name);
      free(info->drm_dp_aux_dev);
      free(info->i2c_dev_name);
      free(info->i2c_dev_dev);
      free(info->driver);
      free(info->ddc_path);
      free(info->ddc_name);
      free(info->ddc_i2c_dev_name);
      free(info->ddc_i2c_dev_dev);
      free(info);
   }
}


//  same whether displayport, non-displayport video, non-video
//    /sys/bus/i2c/devices/i2c-N
//    /sys/devices/pci0000:00/0000:00:02.0/0000:01:00.0/drm/card0/card0-DP-1/i2c-N

static void
read_i2cN_device_node(char * device_path, I2C_Sys_Info * info, int depth) {
   assert(device_path);
   assert(info);
   bool debug = false;
   DBGMSF(debug, "device_path=%s", device_path);
   char * i2c_N = g_strdup(g_path_get_basename(device_path));
   int d0 = depth;
   RPT2_ATTR_TEXT( d0, &info->device_name,    device_path, "name");
   RPT2_ATTR_TEXT( d0, &info->i2c_dev_dev,    device_path, "i2c-dev", i2c_N, "dev");
   RPT2_ATTR_TEXT( d0, &info->i2c_dev_name,   device_path, "i2c-dev", i2c_N, "name");
}


static void
read_drm_dp_card_connector_node(char * connector_path, I2C_Sys_Info * info, int depth) {
   bool debug = false;
   DBGMSF(debug, "connector_path=%s", connector_path);
   int d0 = depth;
   char * ddc_path_fn;
   RPT2_ATTR_REALPATH(d0, &ddc_path_fn, connector_path, "ddc");
   assert(ddc_path_fn);
   info->ddc_path = ddc_path_fn;
   info->linked_ddc_filename = g_path_get_basename(ddc_path_fn);
   info->connector = strdup(g_path_get_basename(connector_path));
   RPT2_ATTR_TEXT(d0, &info->ddc_name,         ddc_path_fn, "name");
   RPT2_ATTR_TEXT(d0, &info->ddc_i2c_dev_name, ddc_path_fn, "i2c-dev", info->linked_ddc_filename, "name");
   RPT2_ATTR_TEXT(d0, &info->ddc_i2c_dev_dev,  ddc_path_fn, "i2c-dev", info->linked_ddc_filename, "dev");

   char * drm_dp_aux_dir;
   RPT2_ATTR_SINGLE_SUBDIR(d0, &drm_dp_aux_dir, str_starts_with, "drm_dp_aux", connector_path);
   assert(drm_dp_aux_dir);
   RPT2_ATTR_TEXT(d0, &info->drm_dp_aux_name, connector_path, drm_dp_aux_dir, "name");
   RPT2_ATTR_TEXT(d0, &info->drm_dp_aux_dev,  connector_path, drm_dp_aux_dir, "dev");

   RPT2_ATTR_EDID(d0, NULL, connector_path, "edid");
   RPT2_ATTR_TEXT(d0, NULL, connector_path, "enabled");
   RPT2_ATTR_TEXT(d0, NULL, connector_path, "status");
}


static bool
starts_with_card(const char * val) {
   return str_starts_with(val, "card");
}


static void
one_drm_connector(
      const char * dirname,
      const char * connector,              // e.g card0-DP-1
      void *       accumulator,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "dirname=%s, connector=%s", dirname, connector);
   int d1 = (depth < 0) ? depth : depth + 1;
   I2C_Sys_Info * info = accumulator;
   if (info->connector) {
      DBGMSF(debug, "Connector already found, skipping");
      return;
   }
   bool is_dp = RPT2_ATTR_SINGLE_SUBDIR(depth, NULL, str_starts_with, "drm_dp_aux", dirname, connector);
   if (is_dp)
      return;
   char i2cname[20];
   g_snprintf(i2cname, 20, "i2c-%d", info->busno);
   bool found_i2c = RPT2_ATTR_SINGLE_SUBDIR(depth, NULL, streq, i2cname, dirname, connector, "ddc/i2c-dev");
   if (!found_i2c)
      return;
   info->connector = strdup(connector);
   RPT2_ATTR_TEXT(d1, NULL, dirname, connector, "ddc", "name");
   RPT2_ATTR_TEXT(d1, NULL, dirname, connector, "ddc/i2c-dev", i2cname, "dev");
   RPT2_ATTR_TEXT(d1, NULL, dirname, connector, "ddc/i2c-dev", i2cname, "name");
   RPT2_ATTR_EDID(d1, NULL, dirname, connector, "edid");
   RPT2_ATTR_TEXT(d1, NULL, dirname, connector, "enabled");
   RPT2_ATTR_TEXT(d1, NULL, dirname, connector, "status");
   return;
}


// Dir_Foreach_Func
static void
one_drm_card(
      const char * dirname,     //
      const char * fn,          // card0, card1 ...
      void *       info,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "dirname=%s, fn=%s", dirname, fn);
   char buf[PATH_MAX];
   g_snprintf(buf, PATH_MAX, "%s/%s", dirname, fn);
   dir_ordered_foreach(buf, starts_with_card, i2c_compare, one_drm_connector, info, depth);
}


static void
read_pci_something_node(
      char *         nodepath,
      int            busno,
      I2C_Sys_Info * info,
      int            depth)
{
   bool debug = false;
   DBGMSF(debug, "nodepath=%s, busno=%d", nodepath, busno);
   int d0 = depth;
   int d1 = (depth < 0) ? depth : depth + 1;
   char * class;
   RPT2_ATTR_TEXT(d0, &class, nodepath, "class");
   if (class && str_starts_with(class, "0x03")) {
      RPT2_ATTR_TEXT(d0, NULL, nodepath, "boot_vga");
      RPT2_ATTR_TEXT(d0, NULL, nodepath, "vendor");
      RPT2_ATTR_TEXT(d0, NULL, nodepath, "device");
      char * driver_path = NULL;
      RPT2_ATTR_REALPATH(d0, &driver_path, nodepath, "driver");
      RPT2_ATTR_TEXT(d0, NULL, nodepath, "fw_version");
      char buf[PATH_MAX];
      g_snprintf(buf, PATH_MAX, "%s/%s", nodepath, "drm");
      DBGMSF(debug, "Calling dir_ordered_foreach, buf=%s, predicate starts_with_card()", buf);
      dir_ordered_foreach(buf, starts_with_card, i2c_compare, one_drm_card, info, d1);
      if (info->connector)
         info->driver = g_path_get_basename(driver_path);
      else
         free(driver_path);
   }
}


I2C_Sys_Info *
get_i2c_sys_info(int busno, int depth) {
   bool debug = false;
   DBGMSF(debug, "busno=%d. depth=%d", busno, depth);
   I2C_Sys_Info * result = NULL;
  // depth = -1;
   int d1 = (depth < 0) ? -1 : depth+1;

   char i2c_N[20];
   g_snprintf(i2c_N, 20, "i2c-%d", busno);
                                               // Example:
   char   i2c_device_path[50];                 // /sys/bus/i2c/devices/i2c-13
   char * pci_i2c_device_path;                 // .../card0/card0-DP-1/i2c-13
   char * pci_i2c_device_parent = NULL;        // .../card0/card0-DP-1
// char * connector_path = NULL;               // .../card0/card0-DP-1
   char * drm_dp_aux_dir = NULL;               // .../card0/card0-DP-1/drm_dp_aux0
// char * ddc_path_fn = NULL;                  // .../card0/card0-DP-1/ddc
   g_snprintf(i2c_device_path, 50, "/sys/bus/i2c/devices/i2c-%d", busno);

   if (directory_exists(i2c_device_path)) {
      result = calloc(1, sizeof(I2C_Sys_Info));
      result->busno = busno;
      RPT2_ATTR_REALPATH(d1, &pci_i2c_device_path, i2c_device_path);
      result->pci_device_path = pci_i2c_device_path;
      read_i2cN_device_node(pci_i2c_device_path, result, d1);

      RPT2_ATTR_REALPATH(d1, &pci_i2c_device_parent, pci_i2c_device_path, "..");
      RPT2_ATTR_SINGLE_SUBDIR(d1, &drm_dp_aux_dir, str_starts_with, "drm_dp_aux", pci_i2c_device_parent);
      if (drm_dp_aux_dir) {
         result->is_display_port = true;
         read_drm_dp_card_connector_node(pci_i2c_device_parent, result, d1);
         // DBGMSG("A pci_i2c_device_parent=%s", pci_i2c_device_parent);
         char * driver_path = NULL;
         RPT2_ATTR_REALPATH(d1, &driver_path, pci_i2c_device_parent, "../../..", "driver");
         // if (info->connector)
         result->driver = g_path_get_basename(driver_path);
      }
      else {
         read_pci_something_node(pci_i2c_device_parent, busno, result, d1);
         char * driver_path = NULL;
         RPT2_ATTR_REALPATH(d1, &driver_path, pci_i2c_device_parent, "driver");
         // if (info->connector)
         result->driver = g_path_get_basename(driver_path);
      }
   }

   // ASSERT_IFF(drm_dp_aux_dir, ddc_path_fn);
   return result;
}


void report_i2c_sys_info(int busno, int depth) {
   bool debug = false;
   int d1 = (depth < 0) ? depth : depth + 1;
   int d2 = (depth < 0) ? depth : depth + 2;
   rpt_vstring(depth, "Extended information for /sys/bus/i2c/devices/i2c-%d...", busno);

   I2C_Sys_Info * dp_info =
         get_i2c_sys_info(busno, (debug) ? d2 : -1 );
   if (dp_info) {
      rpt_vstring(d1, "PCI device path:     %s", dp_info->pci_device_path);
      rpt_vstring(d1, "name:                %s", dp_info->device_name);
      rpt_vstring(d1, "i2c-dev/i2c-%d/dev:   %s", busno, dp_info->i2c_dev_dev);
      rpt_vstring(d1, "i2c-dev/i2c-%d/name:  %s", busno, dp_info->i2c_dev_name);
      rpt_vstring(d1, "Connector:           %s", dp_info->connector);
      rpt_vstring(d1, "Driver:              %s", dp_info->driver);

      if (dp_info->is_display_port) {
         rpt_vstring(d1, "DisplayPort only attributes:");
         rpt_vstring(d2, "ddc path:                %s", dp_info->ddc_path);
      // rpt_vstring(d2, "Linked ddc filename:     %s", dp_info->linked_ddc_filename);
         rpt_vstring(d2, "ddc name:                %s", dp_info->ddc_name);
         rpt_vstring(d2, "ddc i2c-dev/%s/dev:   %s", dp_info->linked_ddc_filename, dp_info->ddc_i2c_dev_dev);
         rpt_vstring(d2, "ddc i2c-dev/%s/name:  %s", dp_info->linked_ddc_filename, dp_info->ddc_i2c_dev_name);
         rpt_vstring(d2, "DP Aux channel dev:      %s", dp_info->drm_dp_aux_dev);
         rpt_vstring(d2, "DP Aux channel name:     %s", dp_info->drm_dp_aux_name);
      }
      else {
         rpt_vstring(d1, "Not a DisplayPort connection");
      }
      free_i2c_sys_info(dp_info);
   }
}

