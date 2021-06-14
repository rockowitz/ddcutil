/** \file i2c_sysfs.c
 *
 *  Query /sys file system for information on I2C devices
 */

// Copyright (C) 2020-2021 Sanford Rockowitz <rockowitz@minsoft.com>
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
#ifdef ENABLE_UDEV
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
read_i2cN_device_node(
      const char *   device_path,
      I2C_Sys_Info * info,
      int            depth)
{
   assert(device_path);
   assert(info);
   bool debug = false;
   DBGMSF(debug, "Starting. device_path=%s", device_path);
   char * i2c_N = g_path_get_basename(device_path);
   int d0 = depth;
   if (debug && d0 < 0)
      d0 = 2;
   RPT2_ATTR_TEXT( d0, &info->device_name,    device_path, "name");
   RPT2_ATTR_TEXT( d0, &info->i2c_dev_dev,    device_path, "i2c-dev", i2c_N, "dev");
   RPT2_ATTR_TEXT( d0, &info->i2c_dev_name,   device_path, "i2c-dev", i2c_N, "name");
   free(i2c_N);
   DBGMSF(debug, "Done.");
}

#ifdef IN_PROGRESS
static void
read_drm_card_connector_node_common(
      const char *   dirname,
      const char *   connector;
      void *         accumulator,
      int            depth)
{
   bool debug = false;
   DBGMSF(debug, "connector_path=%s", connector_path);
   int d0 = depth;
   if (debug && d0 < 0)
      d0 = 2;
   I2C_Sys_Info * info = accumulator;
   char connector_path[PATH_MAX];
   g_snprintf(connector_path, PATH_MAX, "%s/%s", dirname, connector);

   char * drm_dp_aux_dir;
   RPT2_ATTR_SINGLE_SUBDIR(d0, &drm_dp_aux_dir, str_starts_with, "drm_dp_aux", connector_path);
   if (drm_dp_aux_dir) {
      RPT2_ATTR_TEXT(d0, &info->drm_dp_aux_name, connector_path, drm_dp_aux_dir, "name");
      RPT2_ATTR_TEXT(d0, &info->drm_dp_aux_dev,  connector_path, drm_dp_aux_dir, "dev");
   }

   char * ddc_path_fn;
   RPT2_ATTR_REALPATH(d0, &ddc_path_fn, connector_path, "ddc");
   if (ddc_path_fn) {
      info->ddc_path = ddc_path_fn;
      info->linked_ddc_filename = g_path_get_basename(ddc_path_fn);
      info->connector = g_path_get_basename(connector_path);  // == coonector
      RPT2_ATTR_TEXT(d0, &info->ddc_name,         ddc_path_fn, "name");
      RPT2_ATTR_TEXT(d0, &info->ddc_i2c_dev_name, ddc_path_fn, "i2c-dev", info->linked_ddc_filename, "name");
      RPT2_ATTR_TEXT(d0, &info->ddc_i2c_dev_dev,  ddc_path_fn, "i2c-dev", info->linked_ddc_filename, "dev");
   }


   RPT2_ATTR_EDID(d1, NULL, dirname, connector, "edid");
   RPT2_ATTR_TEXT(d1, NULL, dirname, connector, "enabled");
   RPT2_ATTR_TEXT(d1, NULL, dirname, connector, "status");
}
#endif


// Process <controller>/drm/cardN/cardN-<connector> for case that
// cardN-<connector> is a DisplayPort connector

static void
read_drm_dp_card_connector_node(
      const char *   connector_path,
      I2C_Sys_Info * info,
      int            depth)
{
   bool debug = false;
   DBGMSF(debug, "connector_path=%s", connector_path);
   int d0 = depth;
   if (debug && d0 < 0)
      d0 = 2;

   char * ddc_path_fn;
   RPT2_ATTR_REALPATH(d0, &ddc_path_fn, connector_path, "ddc");
   if (ddc_path_fn) {
      info->ddc_path = ddc_path_fn;
      info->linked_ddc_filename = g_path_get_basename(ddc_path_fn);
      info->connector = g_path_get_basename(connector_path);
      RPT2_ATTR_TEXT(d0, &info->ddc_name,         ddc_path_fn, "name");
      RPT2_ATTR_TEXT(d0, &info->ddc_i2c_dev_name, ddc_path_fn, "i2c-dev", info->linked_ddc_filename, "name");
      RPT2_ATTR_TEXT(d0, &info->ddc_i2c_dev_dev,  ddc_path_fn, "i2c-dev", info->linked_ddc_filename, "dev");
   }

   char * drm_dp_aux_dir;
   RPT2_ATTR_SINGLE_SUBDIR(d0, &drm_dp_aux_dir, str_starts_with, "drm_dp_aux", connector_path);
   if (drm_dp_aux_dir) {
      RPT2_ATTR_TEXT(d0, &info->drm_dp_aux_name, connector_path, drm_dp_aux_dir, "name");
      RPT2_ATTR_TEXT(d0, &info->drm_dp_aux_dev,  connector_path, drm_dp_aux_dir, "dev");
      free(drm_dp_aux_dir);
   }

   RPT2_ATTR_EDID(d0, NULL, connector_path, "edid");
   RPT2_ATTR_TEXT(d0, NULL, connector_path, "enabled");
   RPT2_ATTR_TEXT(d0, NULL, connector_path, "status");
}


static bool
starts_with_card(const char * val) {
   return str_starts_with(val, "card");
}


// Process a <controller>/drm/cardN/cardN-<connector> for case when
// cardN-<connector> is not a DisplayPort connector

static void
read_drm_nondp_card_connector_node(
      const char * dirname,                // e.g /sys/devices/pci.../card0
      const char * connector,              // e.g card0-DP-1
      void *       accumulator,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "dirname=%s, connector=%s", dirname, connector);
   int d1 = (depth < 0) ? -1 : depth + 1;
   if (debug && d1 < 0)
      d1 = 2;
   I2C_Sys_Info * info = accumulator;

   if (info->connector) {  // already handled by read_drm_dp_card_connector_node()
      DBGMSF(debug, "Connector already found, skipping");
      return;
   }

   bool is_dp = RPT2_ATTR_SINGLE_SUBDIR(depth, NULL, str_starts_with, "drm_dp_aux", dirname, connector);
   if (is_dp) {
      DBGMSF(debug, "Is display port connector, skipping");
      return;
   }

   char i2cN[20];
   g_snprintf(i2cN, 20, "i2c-%d", info->busno);
   bool found_i2c = RPT2_ATTR_SINGLE_SUBDIR(depth, NULL, streq, i2cN, dirname, connector, "ddc/i2c-dev");
   if (!found_i2c)
      return;
   info->connector = strdup(connector);
   RPT2_ATTR_TEXT(d1, NULL, dirname, connector, "ddc", "name");
   RPT2_ATTR_TEXT(d1, NULL, dirname, connector, "ddc/i2c-dev", i2cN, "dev");
   RPT2_ATTR_TEXT(d1, NULL, dirname, connector, "ddc/i2c-dev", i2cN, "name");
   RPT2_ATTR_EDID(d1, NULL, dirname, connector, "edid");
   RPT2_ATTR_TEXT(d1, NULL, dirname, connector, "enabled");
   RPT2_ATTR_TEXT(d1, NULL, dirname, connector, "status");
   return;
}


// Dir_Foreach_Func
// Process a <controller>/drm/cardN node

static void
one_drm_card(
      const char * dirname,     // e.g /sys/devices/pci
      const char * fn,          // card0, card1 ...
      void *       info,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, fn=%s", dirname, fn);
   char buf[PATH_MAX];
   g_snprintf(buf, PATH_MAX, "%s/%s", dirname, fn);
   dir_ordered_foreach(buf, starts_with_card, gaux_ptr_scomp, read_drm_nondp_card_connector_node, info, depth);
   DBGMSF(debug, "Done");
}


static void
read_controller_driver(
      const char *   controller_path,
      I2C_Sys_Info * info,
      int            depth)
{
   char * driver_path = NULL;
   RPT2_ATTR_REALPATH(depth, &driver_path, controller_path, "driver");
   if (driver_path) {
      info->driver = g_path_get_basename(driver_path);
      free(driver_path);
   }
}



// called only if not DisplayPort

static void
read_pci_display_controller_node(
      const char *   nodepath,
      int            busno,
      I2C_Sys_Info * info,
      int            depth)
{
   bool debug = false;
   DBGMSF(debug, "busno=%d, nodepath=%s", busno, nodepath);
   int d0 = depth;                              // for this function
   if (debug && d0 < 0)
      d0 = 2;
   int depth1 = (depth < 0) ? -1 : depth + 1;   // for called functions

   char * class;
   RPT2_ATTR_TEXT(d0, &class, nodepath, "class");
   if (class && str_starts_with(class, "0x03")) {
      // this is indeed a display controller node
      RPT2_ATTR_TEXT(d0, NULL, nodepath, "boot_vga");
      RPT2_ATTR_TEXT(d0, NULL, nodepath, "vendor");
      RPT2_ATTR_TEXT(d0, NULL, nodepath, "device");

      // RPT2_ATTR_TEXT(d0, NULL, nodepath, "fw_version");
#ifdef OLD
      char * driver_path = NULL;
      RPT2_ATTR_REALPATH(d0, &driver_path, nodepath, "driver");
      if (driver_path && info->connector)   // why the info->connector test?
         info->driver = g_path_get_basename(driver_path);
      free(driver_path);
#endif
      read_controller_driver(nodepath, info, depth);

      // examine all drm/cardN subnodes
      char buf[PATH_MAX];
      g_snprintf(buf, PATH_MAX, "%s/%s", nodepath, "drm");
      DBGMSF(debug, "Calling dir_ordered_foreach, buf=%s, predicate starts_with_card()", buf);
      dir_ordered_foreach(buf, starts_with_card, i2c_compare, one_drm_card, info, depth1);
   }
   free(class);
}


I2C_Sys_Info *
get_i2c_sys_info(
      int busno,
      int depth)
{
   bool debug = false;
   DBGMSF(debug, "busno=%d. depth=%d", busno, depth);
   I2C_Sys_Info * result = NULL;
   int d1 = (depth < 0) ? -1 : depth+1;

   char i2c_N[20];
   g_snprintf(i2c_N, 20, "i2c-%d", busno);
                                               // Example:
   char   i2c_device_path[50];                 // /sys/bus/i2c/devices/i2c-13
   char * pci_i2c_device_path = NULL;          // /sys/devices/../card0/card0-DP-1/i2c-13
   char * pci_i2c_device_parent = NULL;        // /sys/devices/.../card0/card0-DP-1
// char * connector_path = NULL;               // .../card0/card0-DP-1
// char * drm_dp_aux_dir = NULL;               // .../card0/card0-DP-1/drm_dp_aux0
// char * ddc_path_fn = NULL;                  // .../card0/card0-DP-1/ddc
   g_snprintf(i2c_device_path, 50, "/sys/bus/i2c/devices/i2c-%d", busno);

   if (directory_exists(i2c_device_path)) {
      result = calloc(1, sizeof(I2C_Sys_Info));
      result->busno = busno;
      // real path is in /sys/devices tree
      RPT2_ATTR_REALPATH(d1, &pci_i2c_device_path, i2c_device_path);
      result->pci_device_path = pci_i2c_device_path;
      DBGMSF(debug, "pci_i2c_device_path=%s", pci_i2c_device_path);
      read_i2cN_device_node(pci_i2c_device_path, result, d1);

      RPT2_ATTR_REALPATH(d1, &pci_i2c_device_parent, pci_i2c_device_path, "..");
      DBGMSF(debug, "pci_i2c_device_parent=%s", pci_i2c_device_parent);

      bool has_drm_dp_aux_dir =
              RPT2_ATTR_SINGLE_SUBDIR(d1, NULL, str_starts_with, "drm_dp_aux", pci_i2c_device_parent);
      // RPT2_ATTR_SINGLE_SUBDIR(d1, &drm_dp_aux_dir, str_starts_with, "drm_dp_aux", pci_i2c_device_parent);
      // if (drm_dp_aux_dir) {
      if (has_drm_dp_aux_dir) {
         // pci_i2c_device_parent is a drm connector node
         result->is_display_port = true;
         read_drm_dp_card_connector_node(pci_i2c_device_parent, result, d1);

         char controller_path[PATH_MAX];
         g_snprintf(controller_path, PATH_MAX, "%s/../../..", pci_i2c_device_parent);
         read_controller_driver(controller_path, result, d1);

#ifdef OLD
         char * driver_path = NULL;
         // look in controller node:
         RPT2_ATTR_REALPATH(d1, &driver_path, pci_i2c_device_parent, "../../..", "driver");
         result->driver = g_path_get_basename(driver_path);
         free(driver_path);
#endif

         // free(drm_dp_aux_dir);
      }
      else {
         // pci_i2c_device_parent is a display controller node
         read_pci_display_controller_node(pci_i2c_device_parent, busno, result, d1);


#ifdef OLD
         char * driver_path = NULL;
         RPT2_ATTR_REALPATH(d1, &driver_path, pci_i2c_device_parent, "driver");
         result->driver = g_path_get_basename(driver_path);
         free(driver_path);
#endif
      }
      free(pci_i2c_device_parent);
   }

   // ASSERT_IFF(drm_dp_aux_dir, ddc_path_fn);
   return result;
}


/** Emit detailed /sys report
 *
 *  \param  info   pointer to struct with relevant /sys information
 *  \param  depth  logical indentation depth, if < 0 perform no indentation
 *
 *  \remark
 *  This function is used by the DETECT command.
 */
// report intended for detect command

void report_i2c_sys_info(I2C_Sys_Info * info, int depth) {
   int d1 = (depth < 0) ? depth : depth + 1;
   int d2 = (depth < 0) ? depth : depth + 2;

   if (info) {
      rpt_vstring(depth, "Extended information for /sys/bus/i2c/devices/i2c-%d...", info->busno);
      char * busno_pad = (info->busno < 10) ? " " : "";
      rpt_vstring(d1, "PCI device path:     %s", info->pci_device_path);
      rpt_vstring(d1, "name:                %s", info->device_name);
      rpt_vstring(d1, "i2c-dev/i2c-%d/dev: %s %s",
                      info->busno, busno_pad, info->i2c_dev_dev);
      rpt_vstring(d1, "i2c-dev/i2c-%d/name:%s %s",
                      info->busno, busno_pad, info->i2c_dev_name);
      rpt_vstring(d1, "Connector:           %s", info->connector);
      rpt_vstring(d1, "Driver:              %s", info->driver);

      if (info->is_display_port) {
         rpt_vstring(d1, "DisplayPort only attributes:");
         rpt_vstring(d2, "ddc path:                %s", info->ddc_path);
      // rpt_vstring(d2, "Linked ddc filename:     %s", dp_info->linked_ddc_filename);
         rpt_vstring(d2, "ddc name:                %s", info->ddc_name);
         rpt_vstring(d2, "ddc i2c-dev/%s/dev:  %s %s",
                         info->linked_ddc_filename, busno_pad, info->ddc_i2c_dev_dev);
         rpt_vstring(d2, "ddc i2c-dev/%s/name: %s %s",
                         info->linked_ddc_filename, busno_pad, info->ddc_i2c_dev_name);
         rpt_vstring(d2, "DP Aux channel dev:      %s", info->drm_dp_aux_dev);
         rpt_vstring(d2, "DP Aux channel name:     %s", info->drm_dp_aux_name);
      }
      else {
         rpt_vstring(d1, "Not a DisplayPort connection");
      }
   }
}


void report_one_bus_i2c(
      const char * dirname,     //
      const char * fn,          // i2c-1, i2c-2, etc., possibly 1-0037, 1-0023, 1-0050 etc
      void *       data,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "dirname=%s, fn=%s", dirname, fn);
   rpt_nl();
   int busno = i2c_name_to_busno(fn);  //   catches non-i2cN names
   if (busno < 0) {
      rpt_vstring(depth, "Ignoring (A) %s/%s", dirname, fn);
   }
   else {
      rpt_vstring(depth, "Examining (A) /sys/bus/i2c/devices/i2c-%d...", busno);
      int d1 = (debug) ? -1 : depth+1;
      I2C_Sys_Info * info = get_i2c_sys_info(busno, d1);
      report_i2c_sys_info(info, depth+1);
      free_i2c_sys_info(info);
   }
}


void dbgrpt_sys_bus_i2c(int depth) {
   rpt_label(depth, "Examining (B) /sys/bus/i2c/devices for MST, duplicate EDIDs:");
   rpt_nl();
   dir_ordered_foreach("/sys/bus/i2c/devices", NULL, i2c_compare, report_one_bus_i2c, NULL, depth);
}

//
//  Pruned Scan
//

//  Filter functions

#ifdef MAYBE_FUTURE
bool dirname_starts_with(const char * dirname, const char * val) {
   bool debug = true;
   DBGMSF(debug, "dirname=%s, val_fn=%s", dirname, val);
   bool result = str_starts_with(dirname, val);
   DBGMSF(debug, "Returning %s", sbool(result));
   return result;
#endif

// for e.g. i2c-3
bool is_i2cN(const char * dirname, const char * val) {
   bool debug = false;
   DBGMSF(debug, "dirname=%s, val_fn=%s", dirname, val);
   bool result = str_starts_with(dirname, "i2c-");
   DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

bool is_drm_dp_aux_subdir(const char * dirname, const char * val) {
   bool debug = false;
   DBGMSF(debug, "dirname=%s, val=%s", dirname, val);
   bool result = str_starts_with(dirname, "drm_dp_aux");
   DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

// for e.g. card0-DP-1
bool is_card_connector_dir(const char * dirname, const char * simple_fn) {
   bool debug = false;
   DBGMSF(debug, "dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = str_starts_with(simple_fn, "card");
   DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

// for e.g. card0
bool is_cardN_dir(const char * dirname, const char * simple_fn) {
   bool debug = false;
   DBGMSF(debug, "dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = str_starts_with(simple_fn, "card");
   DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

bool is_drm_dir(const char * dirname, const char * simple_fn) {
   bool debug = false;
   DBGMSF(debug, "dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = streq(simple_fn, "drm");
   DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

bool is_i2cN_dir(const char * dirname, const char * simple_fn) {
   bool debug = false;
   DBGMSF(debug, "dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = str_starts_with(simple_fn, "i2c-");
   DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

// does dirname/simple_fn have attribute class with value display controller or docking station?
bool has_class_display_or_docking_station(
      const char * dirname, const char * simple_fn)
{
   bool debug = false;
   bool result = false;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);
   char * class_val = NULL;
   int    iclass = 0;
   int    top_byte = 0;
   if ( GET_ATTR_TEXT(&class_val, dirname, simple_fn, "class") ) {
      if (str_to_int(class_val, &iclass, 16) ) {
         top_byte = iclass >> 16;
         if (top_byte == 0x03 || top_byte == 0x0a)  // display controller or docking station
            result = true;
      }
   }
   DBGMSF(debug, "class_val = %s, top_byte = 0x%02x, result=%s",
                 class_val, top_byte, sbool(result) );
   return result;
}


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
   bool found_name = RPT2_ATTR_TEXT(0, &name_val, dirname, fn, "name");
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
   char * name_val;
   bool found_name = RPT2_ATTR_TEXT(0, &name_val, dirname, fn, "name");
   DBGMSF(debug, "RPT_ATTR_TEXT returned %s, name_val -> %s", sbool(found_name), name_val);
}

void report_one_connector(
      const char * dirname,     // <device>/drm/cardN
      const char * simple_fn,   // card0-HDMI-1 etc
      void *       data,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);

   char * i2c_subdir_name   = NULL;
   char * drm_dp_aux_subdir = NULL;
   char * i2c_subdir        = NULL;
   RPT2_ATTR_SINGLE_SUBDIR(depth, &i2c_subdir_name, is_i2cN, "i2c-", dirname, simple_fn);
   RPT2_ATTR_SINGLE_SUBDIR(depth, &drm_dp_aux_subdir, is_drm_dp_aux_subdir, "drm_dp_aux", dirname, simple_fn);
   RPT2_ATTR_REALPATH(depth, NULL, dirname, simple_fn, "ddc");
   RPT2_ATTR_TEXT(depth, NULL, dirname, simple_fn, "dpms");
   RPT2_ATTR_EDID(depth, NULL, dirname, simple_fn, "edid");
   RPT2_ATTR_TEXT(depth, NULL, dirname, simple_fn, "enabled");
   RPT2_ATTR_TEXT(depth, NULL, dirname, simple_fn, "status");
   if (drm_dp_aux_subdir) {     // DisplayPort
      RPT2_ATTR_TEXT(0, NULL, dirname, simple_fn, drm_dp_aux_subdir, "name");
      RPT2_ATTR_TEXT(0, NULL, dirname, simple_fn, drm_dp_aux_subdir, "dev");
   }

   RPT2_ATTR_SINGLE_SUBDIR(0, &i2c_subdir, is_i2cN, "i2c-", dirname, simple_fn);
   if (i2c_subdir) {
      RPT2_ATTR_TEXT(depth, NULL, dirname, simple_fn, i2c_subdir, "name");
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
   RPT2_ATTR_TEXT(    depth, NULL, dirname, fn, "class");
   RPT2_ATTR_REALPATH(depth, NULL, dirname, fn, "driver");

   char * thisdir = g_strdup_printf("%s/%s", dirname, fn);
   dir_filtered_ordered_foreach(
         thisdir,
         is_i2cN_dir,
         i2c_compare,
         report_one_i2c_dir,
         NULL,
         depth);

   if ( RPT2_ATTR_NOTE_SUBDIR(0, NULL, dirname, fn, "drm"))  {
         char * drmdir = g_strdup_printf("%s/%s/drm", dirname, fn);
         dir_filtered_ordered_foreach(
               drmdir, is_cardN_dir, NULL, report_one_cardN, NULL, depth);
   }

   rpt_nl();
}


void
dbgrpt_refined_sys(int depth) {
   rpt_nl();
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

