/** \file i2c_sysfs.c
 *
 *  Query /sys file system for information on I2C devices
 */

// Copyright (C) 2020-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 
#include "config.h"

/** \cond */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xf86drm.h>
// #include <xf86drmMode.h>
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
#include "util/sysfs_filter_functions.h"
#include "util/sysfs_i2c_util.h"
#ifdef ENABLE_UDEV
#include "util/udev_i2c_util.h"
#endif
#include "util/utilrpt.h"

#include "base/core.h"

#include "i2c_sysfs.h"

//
// *** General Utilities ***

void gaux_string_ptr_array_include(GPtrArray * arry, char * new_value) {
   bool debug = false;
   if (debug)
      printf("(%s) new_value=|%s|\n", __func__, new_value);
   assert(arry);
   int ndx = 0;
   for (; ndx < arry->len; ndx++) {
      char * old_value = g_ptr_array_index(arry, ndx);
      if (streq(new_value, old_value) ) {
         if (debug) printf("(%s) Found. ndx=%d\n", __func__, ndx);
         break;
      }
   }
   if (ndx == arry->len) {
      if (debug)
         printf("(%s) Appending new value\n", __func__);
      g_ptr_array_add(arry, strdup(new_value));
   }
}


//
// *** Common Functions
//

/** Given a sysfs node, walk up the driver chain until
 *  an adapter node is found.
 *
 *  \param  path   e.g. /sys/bus/i2c/drivers/i2c-5
 *  \param  depth  logical indentation depth
 *  \return sysfs path to adapter
 *
 *  Parameter **depth** behaves as usual for sysfs RPT_... functions.
 *  If depth >= 0, sysfs attributes reported.
 *  If depth < -9, there is no output
 *
 *  Caller is responsible for freeing the returned value
 */
char * find_adapter(char * path, int depth) {
   char * devpath = NULL;
   if ( RPT_ATTR_NOTE_SUBDIR(depth, NULL, path, "device") ) {
       if ( RPT_ATTR_TEXT(depth, NULL, path, "device", "class") ) {
          RPT_ATTR_REALPATH(depth, &devpath, path, "device");
       }
       else {
          char p2[PATH_MAX];
           g_snprintf(p2, PATH_MAX, "%s/device", path);
           devpath = find_adapter(p2, depth);
       }
   }
   return devpath;
}


char * get_driver_for_adapter(char * adapter_path, int depth) {
   char * basename = NULL;
   RPT_ATTR_REALPATH_BASENAME(depth, &basename, adapter_path, "driver", "module");
   char * retval = (basename) ? basename : NULL;
   return retval;
}


char * find_and_get_adapter_driver(char * path, int depth) {
   char * result = NULL;
   char * adapter_path = find_adapter(path, depth);
   if (adapter_path) {
      result = get_driver_for_adapter(adapter_path, depth);
      free(adapter_path);
   }
   return result;
}


/** Returns the name of the video driver for an I2C bus.
 *
 * \param  busno   I2 bus number
 * \return driver name, NULL if can't determine
 *
 * Caller is responsible for freeing the returned string.
 */
char * get_driver_for_busno(int busno) {
   char path[PATH_MAX];
   g_snprintf(path, PATH_MAX, "/sys/bus/i2c/devices/i2c-%d", busno);
   char * result = find_and_get_adapter_driver(path, -1);
   return result;
}


//
// Predicate functions
//

// typedef Dir_Filter_Func
bool is_drm_connector(const char * dirname, const char * simple_fn) {
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = false;
   if (str_starts_with(simple_fn, "card")) {
      char * s0 = strdup( simple_fn + 4);   // work around const char *
      char * s = s0;
      while (isdigit(*s)) s++;
      if (*s == '-')
         result = true;
      free(s0);
   }
   DBGMSF(debug, "Done.     Returning %s", SBOOL(result));
   return result;
}


bool fn_starts_with(const char * filename, const char * val) {
   return str_starts_with(filename, val);
}


bool is_n_nnnn(const char * dirname, const char * simple_fn) {
   bool result = predicate_any_D_00hh(simple_fn);
   DBGMSF(false,"dirname=%s. simple_fn=%s, returning %s", dirname, simple_fn, SBOOL(result));
   return result;
}

#ifdef NOT_NEEDED   // just set func arg to NULL
bool fn_any(const char * filename, const char * ignore) {
   DBGMSF(true, "filename=%s, ignore=%s: Returning true", filename, ignore);
   return true;
}
#endif


//
// *** I2C_Sys_Info ***
//

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
   RPT_ATTR_TEXT( d0, &info->device_name,    device_path, "name");
   RPT_ATTR_TEXT( d0, &info->i2c_dev_dev,    device_path, "i2c-dev", i2c_N, "dev");
   RPT_ATTR_TEXT( d0, &info->i2c_dev_name,   device_path, "i2c-dev", i2c_N, "name");
   free(i2c_N);
   DBGMSF(debug, "Done.");
}

#ifdef IN_PROGRESS
static void
read_drm_card_connector_node_common(
      const char *   dirname,
      const char *   connector;      // e.g. card0-DP-1
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
   RPT_ATTR_SINGLE_SUBDIR(d0, &drm_dp_aux_dir, str_starts_with, "drm_dp_aux", connector_path);
   if (drm_dp_aux_dir) {
      RPT_ATTR_TEXT(d0, &info->drm_dp_aux_name, connector_path, drm_dp_aux_dir, "name");
      RPT_ATTR_TEXT(d0, &info->drm_dp_aux_dev,  connector_path, drm_dp_aux_dir, "dev");
   }

   char * ddc_path_fn;
   RPT_ATTR_REALPATH(d0, &ddc_path_fn, connector_path, "ddc");
   if (ddc_path_fn) {
      info->ddc_path = ddc_path_fn;
      info->linked_ddc_filename = g_path_get_basename(ddc_path_fn);
      info->connector = g_path_get_basename(connector_path);  // == coonector
      RPT_ATTR_TEXT(d0, &info->ddc_name,         ddc_path_fn, "name");
      RPT_ATTR_TEXT(d0, &info->ddc_i2c_dev_name, ddc_path_fn, "i2c-dev", info->linked_ddc_filename, "name");
      RPT_ATTR_TEXT(d0, &info->ddc_i2c_dev_dev,  ddc_path_fn, "i2c-dev", info->linked_ddc_filename, "dev");
   }


   RPT_ATTR_EDID(d1, NULL, dirname, connector, "edid");
   RPT_ATTR_TEXT(d1, NULL, dirname, connector, "enabled");
   RPT_ATTR_TEXT(d1, NULL, dirname, connector, "status");
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
   RPT_ATTR_REALPATH(d0, &ddc_path_fn, connector_path, "ddc");
   if (ddc_path_fn) {
      info->ddc_path = ddc_path_fn;
      info->linked_ddc_filename = g_path_get_basename(ddc_path_fn);
      info->connector = g_path_get_basename(connector_path);
      RPT_ATTR_TEXT(d0, &info->ddc_name,         ddc_path_fn, "name");
      RPT_ATTR_TEXT(d0, &info->ddc_i2c_dev_name, ddc_path_fn, "i2c-dev", info->linked_ddc_filename, "name");
      RPT_ATTR_TEXT(d0, &info->ddc_i2c_dev_dev,  ddc_path_fn, "i2c-dev", info->linked_ddc_filename, "dev");
   }

   char * drm_dp_aux_dir;
   RPT_ATTR_SINGLE_SUBDIR(d0, &drm_dp_aux_dir, str_starts_with, "drm_dp_aux", connector_path);
   if (drm_dp_aux_dir) {
      RPT_ATTR_TEXT(d0, &info->drm_dp_aux_name, connector_path, drm_dp_aux_dir, "name");
      RPT_ATTR_TEXT(d0, &info->drm_dp_aux_dev,  connector_path, drm_dp_aux_dir, "dev");
      free(drm_dp_aux_dir);
   }

   RPT_ATTR_EDID(d0, NULL, connector_path, "edid");
   RPT_ATTR_TEXT(d0, NULL, connector_path, "enabled");
   RPT_ATTR_TEXT(d0, NULL, connector_path, "status");
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

   bool is_dp = RPT_ATTR_SINGLE_SUBDIR(depth, NULL, str_starts_with, "drm_dp_aux", dirname, connector);
   if (is_dp) {
      DBGMSF(debug, "Is display port connector, skipping");
      return;
   }

   char i2cN[20];
   g_snprintf(i2cN, 20, "i2c-%d", info->busno);
   bool found_i2c = RPT_ATTR_SINGLE_SUBDIR(depth, NULL, streq, i2cN, dirname, connector, "ddc/i2c-dev");
   if (!found_i2c)
      return;
   info->connector = strdup(connector);
   RPT_ATTR_TEXT(d1, NULL, dirname, connector, "ddc", "name");
   RPT_ATTR_TEXT(d1, NULL, dirname, connector, "ddc/i2c-dev", i2cN, "dev");
   RPT_ATTR_TEXT(d1, NULL, dirname, connector, "ddc/i2c-dev", i2cN, "name");
   RPT_ATTR_EDID(d1, NULL, dirname, connector, "edid");
   RPT_ATTR_TEXT(d1, NULL, dirname, connector, "enabled");
   RPT_ATTR_TEXT(d1, NULL, dirname, connector, "status");
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
   dir_ordered_foreach(
         buf,
         predicate_cardN_connector,
         gaux_ptr_scomp,
         read_drm_nondp_card_connector_node,
         info,
         depth);
   DBGMSF(debug, "Done");
}


static void
read_controller_driver(
      const char *   controller_path,
      I2C_Sys_Info * info,
      int            depth)
{
   char * driver_path = NULL;
   RPT_ATTR_REALPATH(depth, &driver_path, controller_path, "driver");
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
   RPT_ATTR_TEXT(d0, &class, nodepath, "class");
   if (class && str_starts_with(class, "0x03")) {
      // this is indeed a display controller node
      RPT_ATTR_TEXT(d0, NULL, nodepath, "boot_vga");
      RPT_ATTR_TEXT(d0, NULL, nodepath, "vendor");
      RPT_ATTR_TEXT(d0, NULL, nodepath, "device");

      // RPT_ATTR_TEXT(d0, NULL, nodepath, "fw_version");
#ifdef OLD
      char * driver_path = NULL;
      RPT_ATTR_REALPATH(d0, &driver_path, nodepath, "driver");
      if (driver_path && info->connector)   // why the info->connector test?
         info->driver = g_path_get_basename(driver_path);
      free(driver_path);
#endif
      read_controller_driver(nodepath, info, depth);

      // examine all drm/cardN subnodes
      char buf[PATH_MAX];
      g_snprintf(buf, PATH_MAX, "%s/%s", nodepath, "drm");
      DBGMSF(debug, "Calling dir_ordered_foreach, buf=%s, predicate predicate_cardN_connector()", buf);
      dir_ordered_foreach(buf, predicate_cardN_connector, i2c_compare, one_drm_card, info, depth1);
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
      RPT_ATTR_REALPATH(d1, &pci_i2c_device_path, i2c_device_path);
      result->pci_device_path = pci_i2c_device_path;
      DBGMSF(debug, "pci_i2c_device_path=%s", pci_i2c_device_path);
      read_i2cN_device_node(pci_i2c_device_path, result, d1);

      RPT_ATTR_REALPATH(d1, &pci_i2c_device_parent, pci_i2c_device_path, "..");
      DBGMSF(debug, "pci_i2c_device_parent=%s", pci_i2c_device_parent);

      bool has_drm_dp_aux_dir =
              RPT_ATTR_SINGLE_SUBDIR(d1, NULL, str_starts_with, "drm_dp_aux", pci_i2c_device_parent);
      // RPT_ATTR_SINGLE_SUBDIR(d1, &drm_dp_aux_dir, str_starts_with, "drm_dp_aux", pci_i2c_device_parent);
      // if (drm_dp_aux_dir) {
      if (has_drm_dp_aux_dir) {
         // pci_i2c_device_parent is a drm connector node
         result->is_amdgpu_display_port = true;
         read_drm_dp_card_connector_node(pci_i2c_device_parent, result, d1);

         char controller_path[PATH_MAX];
         g_snprintf(controller_path, PATH_MAX, "%s/../../..", pci_i2c_device_parent);
         read_controller_driver(controller_path, result, d1);

#ifdef OLD
         char * driver_path = NULL;
         // look in controller node:
         RPT_ATTR_REALPATH(d1, &driver_path, pci_i2c_device_parent, "../../..", "driver");
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
         RPT_ATTR_REALPATH(d1, &driver_path, pci_i2c_device_parent, "driver");
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

      if (info->is_amdgpu_display_port) {
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
      // else {
      //    rpt_vstring(d1, "Not a DisplayPort connection");
      // }
   }
}


static void report_one_bus_i2c(
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
      // int d1 = (debug) ? -1 : depth+1;
      // d1 > 0 => reports as collects, no need to call report_i2c_sys_info()
      int d1 = depth+1;
      I2C_Sys_Info * info = get_i2c_sys_info(busno, d1);
      // report_i2c_sys_info(info, depth+1);
      free_i2c_sys_info(info);
   }
}


void dbgrpt_sys_bus_i2c(int depth) {
   rpt_label(depth, "Examining (B) /sys/bus/i2c/devices:");
   rpt_nl();
   dir_ordered_foreach("/sys/bus/i2c/devices", NULL, i2c_compare, report_one_bus_i2c, NULL, depth);
}


//
//  *** Scan /sys by drm connector - uses struct Sys_Drm_Connector ***
//

static GPtrArray * sys_drm_connectors = NULL;  // Sys_Drm_Display;

// from query_sysenv_sysfs
// 9/28/2021 Requires hardening, testing on other than amdgpu, MST etc


void free_sys_drm_display(void * display) {
   if (display) {
      Sys_Drm_Connector * disp = display;
      free(disp->connector_name);
      free(disp->ddc_dir_path);
      free(disp->connector_path);
      free(disp->edid_bytes);
      free(disp->name);
      free(disp->status);
      free(disp->base_name);
      free(disp);
   }
}


void free_sys_drm_connectors() {
   if (sys_drm_connectors)
      g_ptr_array_free(sys_drm_connectors, true);
   sys_drm_connectors = NULL;
}


void report_one_sys_drm_display(int depth, Sys_Drm_Connector * cur)
{
   int d0 = depth;
   int d1 = depth+1;

   rpt_vstring(d0, "Connector:   %s", cur->connector_name);
   rpt_vstring(d1, "i2c_busno:   %d", cur->i2c_busno);
   rpt_vstring(d1, "name:        %s", cur->name);
   rpt_vstring(d1, "dev:         %s", cur->dev);
   rpt_vstring(d1, "enabled:     %s", cur->enabled);
   rpt_vstring(d1, "status:      %s", cur->status);

   if (cur->is_aux_channel) {
      rpt_vstring(d1, "base_busno:  %d", cur->base_busno);
      rpt_vstring(d1, "base_name:   %s", cur->base_name);
      rpt_vstring(d1, "base dev:    %s", cur->base_dev);
   }
   if (cur->edid_size > 0) {
      rpt_label(d1,   "edid:");
      rpt_hex_dump(cur->edid_bytes, cur->edid_size, d1);
   }
   else
      rpt_label(d1,"edid:        None");
}


// typedef Dir_Foreach_Func
void one_drm_connector(
      const char *  dirname,      // /sys/class/drm
      const char *  fn,           // e.g. card0-DP-1
      void *        accumulator,
      int           depth)
{
   bool debug = false;
   DBGMSF(debug, "dirname=%s, fn=%s, depth=%d", dirname, fn, depth);
   GPtrArray * drm_displays = accumulator;

   Sys_Drm_Connector * cur = calloc(1, sizeof(Sys_Drm_Connector));
   cur->i2c_busno = -1;      // 0 is valid bus number
   cur->base_busno = -1;
   g_ptr_array_add(drm_displays, cur);
   cur->connector_name = strdup(fn);   // e.g. card0-DP-1
   RPT_ATTR_REALPATH(depth, &cur->connector_path,
                                       dirname, fn);
   RPT_ATTR_TEXT(depth, &cur->enabled, dirname, fn, "enabled");   // e.g. /sys/class/drm/card0-DP-1/enabled
   RPT_ATTR_TEXT(depth, &cur->status,  dirname, fn, "status"); // e.g. /sys/class/drm/card0-DP-1/status

   GByteArray * edid_byte_array = NULL;
   RPT_ATTR_EDID(depth, &edid_byte_array, dirname, fn, "edid");   // e.g. /sys/class/drm/card0-DP-1/edid
   // DBGMSG("edid_byte_array=%p", (void*)edid_byte_array);
   if (edid_byte_array) {
     cur->edid_size = edid_byte_array->len;
     cur->edid_bytes = g_byte_array_free(edid_byte_array, false);
     // DBGMSF("Setting cur->edid_bytes = %p", (void*)cur->edid_bytes);
   }

   // returns 1 if the DRM driver is loaded, 0 otherwise
   int drm_available = drmAvailable();
   DBGMSF(debug, "Has a DRM kernel driver been loaded? (drmAvailable()): %s",
                     sbool(drm_available));

   char * driver = find_and_get_adapter_driver( cur->connector_path, -1);
   assert( !(streq(driver, "nvidia") && drm_available) );
   free(driver);
   if (drm_available) {     //  ???
      bool has_drm_dp_aux_subdir =          // does is exist? /sys/class/drm/card0-DP-1/drm_dp_aux0
            RPT_ATTR_SINGLE_SUBDIR(
                  depth,
                  NULL,     // char **      value_loc,
                  fn_starts_with,
                  "drm_dp_aux",
                  dirname,
                  fn);

      // does e.g. /sys/class/drm/card0-DP-1/i2c-6 exist?
      // *** BAD TEST, Nvida driver does not have drm_dp_aux subdir for DP

      char * i2cN_buf = NULL;   // i2c-N
      bool has_i2c_subdir =
            RPT_ATTR_SINGLE_SUBDIR(depth, &i2cN_buf, fn_starts_with,"i2c-",
                                   dirname, fn);

      ASSERT_IFF(has_drm_dp_aux_subdir, has_i2c_subdir);
      cur->is_aux_channel = has_drm_dp_aux_subdir;
      DBGMSF(debug, "has_i2c_subdir = cur->is_aux_channel = %s", SBOOL(has_i2c_subdir));
      if (has_i2c_subdir) {  // DP
         cur->i2c_busno = i2c_name_to_busno(i2cN_buf);

         // e.g. /sys/class/drm/card0-DP-1/i2c-6/name:
         char * buf = NULL;
         RPT_ATTR_TEXT(depth, &cur->name, dirname, fn, i2cN_buf, "name");
         RPT_ATTR_TEXT(depth, &buf,       dirname, fn, i2cN_buf, "i2c-dev", i2cN_buf, "name");
         // DBGMSG("name = |%s|", cur->name);
         // DBGMSG("buf  = |%s|", buf);
         assert(streq(cur->name, buf));
         free(buf);

         RPT_ATTR_TEXT(depth, &cur->dev,  dirname, fn, i2cN_buf, "i2c-dev", i2cN_buf, "dev");
         free(i2cN_buf);
         if (depth >= 0)
            rpt_nl();

         // Examine ddc subdirectory - does not exist on Nvidia driver
         bool has_ddc_subdir = RPT_ATTR_NOTE_SUBDIR(-1, NULL, dirname, fn, "ddc");
         if (has_ddc_subdir) {
            RPT_ATTR_REALPATH(-1, &cur->ddc_dir_path,    dirname, fn, "ddc");
            // e.g. /sys/class/drm/card0-DP-1/ddc/name:
            RPT_ATTR_TEXT(depth, &cur->base_name, dirname, fn, "ddc", "name");

            bool has_i2c_dev_subdir = RPT_ATTR_NOTE_SUBDIR(-1, NULL, dirname, fn, "ddc", "i2c-dev");
            if (has_i2c_dev_subdir) {
               // looking for e.g. /sys/bus/drm/card0-DP-1/ddc/i2c-dev/i2c-1
               has_i2c_subdir =
                  RPT_ATTR_SINGLE_SUBDIR(depth, &i2cN_buf, fn_starts_with, "i2c-",
                                         dirname, fn, "ddc", "i2c-dev");
               if (has_i2c_subdir) {
                  cur->base_busno = i2c_name_to_busno(i2cN_buf);
                  char * buf = NULL;
                  RPT_ATTR_TEXT(depth, &buf, dirname, fn, "ddc", "i2c-dev", i2cN_buf, "name");
                  assert (streq(buf, cur->base_name));
                  free(buf);
                  RPT_ATTR_TEXT(depth, &cur->base_dev, dirname, fn, "ddc", "i2c-dev", i2cN_buf, "dev");
                  free(i2cN_buf);
               }
            }
         }
      } // has_i2c_subdir
      else {   // not DP
         if (depth >= 0)
            rpt_nl();

         bool found_ddc = RPT_ATTR_REALPATH(depth, &cur->ddc_dir_path,    dirname, fn, "ddc");
         ASSERT_IFF(found_ddc, cur->ddc_dir_path);
         if (cur->ddc_dir_path) {
            // No ddc directory on Nvidia?
            // Examine ddc subdirectory
            // e.g. /sys/class/drm/card0-DP-1/ddc/name:
            RPT_ATTR_TEXT(depth, &cur->name,    dirname, fn, "ddc", "name");

            char * i2cN_buf = NULL;
            // looking for e.g. /sys/bus/drm/card0-DVI-D-1/ddc/i2c-dev/i2c-1
            has_i2c_subdir =
                RPT_ATTR_SINGLE_SUBDIR(depth, &i2cN_buf, fn_starts_with, "i2c-",
                                                dirname, fn, "ddc", "i2c-dev");
            if (has_i2c_subdir) {
                cur->i2c_busno = i2c_name_to_busno(i2cN_buf);
               char * buf = NULL;
               RPT_ATTR_TEXT(depth, &buf,       dirname, fn, "ddc", "i2c-dev", i2cN_buf, "name");
               assert (streq(buf, cur->name));
               free(buf);
               RPT_ATTR_TEXT(depth, &cur->base_dev,
                                                dirname, fn, "ddc", "i2c-dev", i2cN_buf, "dev");
               free(i2cN_buf);
            }
         }  // had ddc subdirectory
      }   // not DP

   }  // not Nvidia
   if (depth >= 0)
      rpt_nl();
   DBGMSF(debug, "Done.");
}


/**
 *
 *  \param  depth  logical indentation depth, if < 0 do not emit report
 *  \return array of #Sys_Drm_Connector structs, one for each connector found
 */

GPtrArray * scan_sys_drm_connectors(int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "depth=%d", depth);
   if (sys_drm_connectors) {
      g_ptr_array_free(sys_drm_connectors, true);
      sys_drm_connectors = NULL;
   }
   GPtrArray * sys_drm_connectors = g_ptr_array_new_with_free_func(free_sys_drm_display);

   dir_filtered_ordered_foreach(
         "/sys/class/drm",
         is_drm_connector,      // filter function
         NULL,                  // ordering function
         one_drm_connector,
         sys_drm_connectors,         // accumulator
         depth);
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "size of sys_drm_connectors: %d", sys_drm_connectors->len);
   return sys_drm_connectors;
}


GPtrArray* get_sys_drm_connectors(bool rescan) {
   if (sys_drm_connectors && rescan) {
      g_ptr_array_free(sys_drm_connectors, true);
      sys_drm_connectors = NULL;
   }
   if (!sys_drm_connectors)
      sys_drm_connectors = scan_sys_drm_connectors(-1);
   return sys_drm_connectors;
}


void report_sys_drm_connectors(int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "depth=%d", depth);
   int d0 = depth;
   int d1 = (debug) ? 2 : -1;
   rpt_nl();
   rpt_label(d0, "Display connectors reported by DRM:");
   if (!sys_drm_connectors)
     sys_drm_connectors = scan_sys_drm_connectors(d1);
   GPtrArray * displays = sys_drm_connectors;
   if (!displays || displays->len == 0) {
      rpt_label(d1, "None");
   }
   else {
      for (int ndx = 0; ndx < displays->len; ndx++) {
         Sys_Drm_Connector * cur = g_ptr_array_index(displays, ndx);
         report_one_sys_drm_display(depth, cur);
         rpt_nl();
      }
   }
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "");
}


Sys_Drm_Connector *
find_sys_drm_connector_by_busno_or_edid(int busno, Byte * edid) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "busno=%d, edid=%p", busno, (void*)edid);
   if (!sys_drm_connectors)
     sys_drm_connectors = scan_sys_drm_connectors(-1);
   Sys_Drm_Connector * result = NULL;
   // DBGTRC_NOPREFIX(debug, DDCA_TRC_I2C, "After scan_sys_drm_connectors(), sys_drm_displays=%p",
   //                                     (void*) sys_drm_displays);
   if (sys_drm_connectors) {
      for (int ndx = 0; ndx < sys_drm_connectors->len; ndx++) {
         Sys_Drm_Connector * cur = g_ptr_array_index(sys_drm_connectors, ndx);
         // DBGMSG("cur->busno = %d", cur->i2c_busno);
         if (busno >= 0 && cur->i2c_busno == busno) {
            // DBGMSG("Matched");
            result = cur;
            break;
         }
         if (edid && cur->edid_size >= 128 && (memcmp(edid, cur->edid_bytes,128) == 0)) {
            DBGMSF(debug, "Matched by edid");
            result = cur;
            break;
         }
      }
   }
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p", (void*) result);
   return result;
}


Sys_Drm_Connector * find_sys_drm_connector_by_busno(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "busno=%d", busno);
   Sys_Drm_Connector * result = find_sys_drm_connector_by_busno_or_edid(busno, NULL);
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p", (void*) result);
   return result;
}


Sys_Drm_Connector * find_sys_drm_connector_by_edid(Byte * raw_edid) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "edid=%p", (void*) raw_edid);
   Sys_Drm_Connector * result = find_sys_drm_connector_by_busno_or_edid(-1, raw_edid);
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p", (void*) result);
   return result;
}


//
//  Scan for conflicting modules/drivers: Struct Sys_Conflicting_Driver
//

void free_sys_conflicting_driver(Sys_Conflicting_Driver * rec) {
   bool debug = true;
   DBGMSF(debug, "rec=%p", (void*)rec);
   if (rec) {
      free(rec->n_nnnn);
      free(rec->name);
      free(rec->driver_module);
      free(rec->modalias);
      free(rec->eeprom_edid_bytes);
      free(rec);
   }
}


static
void free_sys_conflicting_driver0(void * rec) {
   free_sys_conflicting_driver((Sys_Conflicting_Driver*) rec);
}


char * best_conflicting_driver_name(Sys_Conflicting_Driver* rec) {
   char * result = NULL;
   if (rec->name)
      result = rec->name;
   else if (rec->driver_module)
      result = rec->driver_module;
   else
      result = rec->modalias;
   return result;
}


void dbgrpt_conflicting_driver(Sys_Conflicting_Driver * conflict, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Sys_Conflicting_Driver", conflict, depth);
   rpt_vstring(d1, "i2c_busno:     %d", conflict->i2c_busno);
   rpt_vstring(d1, "n_nnnn:        %s", conflict->n_nnnn);
   rpt_vstring(d1, "name:          %s", conflict->name);
   rpt_vstring(d1, "driver/module: %s", conflict->driver_module);
   rpt_vstring(d1, "modalias:      %s", conflict->modalias);
   rpt_vstring(d1, "best conflicting driver name: %s", best_conflicting_driver_name(conflict));
   if (conflict->eeprom_edid_bytes) {
      rpt_vstring(d1, "eeprom_edid_bytes:");
      rpt_hex_dump(conflict->eeprom_edid_bytes, conflict->eeprom_edid_size, d1);
   }
}


// typedef Dir_Foreach_Func
void one_n_nnnn(
      const char * dir_name,  // e.g. /sys/bus/i2c/devices/i2c-4
      const char * fn,        // e.g. 4-0037
      void *       accumulator,
      int          depth)
{
   bool debug = true;
   DBGMSF(debug, "dirname=%s, fn=%s, depth=%d", dir_name, fn, depth);

   GPtrArray* conflicting_drivers= accumulator;
   Sys_Conflicting_Driver * conflicting_driver = calloc(1, sizeof(Sys_Conflicting_Driver));
   DBGMSF(debug, "Allocated Sys_Conflicting_Driver %p", (void*) conflicting_driver);
   conflicting_driver->n_nnnn = strdup(fn);

   RPT_ATTR_TEXT(depth, &conflicting_driver->name, dir_name, fn, "name");

   if (str_ends_with(fn, "0050")) {
      GByteArray * edid_byte_array = NULL;
      RPT_ATTR_EDID(depth, &edid_byte_array, dir_name, fn, "eeprom");
      if (edid_byte_array) {
         conflicting_driver->eeprom_edid_size = edid_byte_array->len;
         conflicting_driver->eeprom_edid_bytes = g_byte_array_steal(edid_byte_array, &conflicting_driver->eeprom_edid_size);
      }
   }

   // N.  subdirectory driver does not always exist, e.g. for ddcci - N-0037
   RPT_ATTR_REALPATH_BASENAME(depth, &conflicting_driver->driver_module, dir_name, fn, "driver/module");
   RPT_ATTR_TEXT(depth, &conflicting_driver->modalias, dir_name, fn, "modalias");

   g_ptr_array_add(conflicting_drivers, conflicting_driver);
   if (depth >= 0)
      rpt_nl();
   DBGMSF(debug, "Done");
}


static
void collect_conflicting_drivers0(GPtrArray * conflicting_drivers, int busno, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d, conflicting_drivers=%p", busno, (void*)conflicting_drivers);

   char i2c_bus_path[PATH_MAX];
   g_snprintf(i2c_bus_path, sizeof(i2c_bus_path), "/sys/bus/i2c/devices/i2c-%d", busno);
   char sbusno[4];
   g_snprintf(sbusno, 4, "%d", busno);

   int old_ct = conflicting_drivers->len;
   dir_ordered_foreach_with_arg(
                         i2c_bus_path,           // directory
                         predicate_exact_D_00hh, // filter function
                         sbusno,                 // filter function argument
                         NULL,                   // ordering function
                         one_n_nnnn,             // process dir named like 4-0050
                         conflicting_drivers,    // accumulator
                         depth);

   for (int ndx=old_ct; ndx < conflicting_drivers->len; ndx++) {
      Sys_Conflicting_Driver * cur = g_ptr_array_index(conflicting_drivers, ndx);
      cur->i2c_busno = busno;
   }

   DBGMSF(debug, "Done." );
}


GPtrArray * collect_conflicting_drivers(int busno, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d, depth=%d", busno, depth);

   GPtrArray * conflicting_drivers = g_ptr_array_new_with_free_func(free_sys_conflicting_driver0);
   collect_conflicting_drivers0(conflicting_drivers, busno, depth);

   DBGMSF(debug, "Done.     Returning: %p", (void*)conflicting_drivers);
   return conflicting_drivers;
}


GPtrArray * collect_conflicting_drivers_for_any_bus(int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting.");
   GPtrArray* all_connectors = get_sys_drm_connectors(false);
   GPtrArray * conflicting_drivers = g_ptr_array_new_with_free_func(free_sys_conflicting_driver0);
   for (int ndx = 0; ndx < all_connectors->len; ndx++) {
      Sys_Drm_Connector * cur = g_ptr_array_index(all_connectors, ndx);
      DBGMSF(debug, "cur->i2c_busno=%d", cur->i2c_busno);
      if (cur->i2c_busno >= 0)   // may not have been set
         collect_conflicting_drivers0(conflicting_drivers, cur->i2c_busno, depth);
   }
   DBGMSF(debug, "Done. Returning %p", (void*) conflicting_drivers);
   return conflicting_drivers;
}


void report_conflicting_drivers(GPtrArray * conflicts, int depth) {
   for (int ndx=0; ndx < conflicts->len; ndx++) {
      Sys_Conflicting_Driver * cur = g_ptr_array_index(conflicts, ndx);
      dbgrpt_conflicting_driver(cur, depth);
   }
}


GPtrArray * conflicting_driver_names(GPtrArray * conflicts) {
   bool debug = false;
   DBGMSF(debug, "Starting.  conflicts=%p", (void*)conflicts);
   GPtrArray * result = g_ptr_array_new_with_free_func(g_free);
   for (int ndx = 0; ndx < conflicts->len; ndx++) {
      Sys_Conflicting_Driver * cur = g_ptr_array_index(conflicts, ndx);
      gaux_string_ptr_array_include(result, best_conflicting_driver_name(cur));
   }
   DBGMSF(debug, "Returning: %s", join_string_g_ptr_array_t(result, " + ") );
   return result;
}


char * conflicting_driver_names_string_t(GPtrArray * conflicts) {
   GPtrArray * driver_names = conflicting_driver_names(conflicts);
   char * result = join_string_g_ptr_array_t(driver_names, ". ");
   g_ptr_array_free(driver_names, true);
   return result;
}


void free_conflicting_drivers(GPtrArray* conflicts) {
   g_ptr_array_free(conflicts, true);
}


//
// *** Collect basic /dev/i2c-N information into Sysfs_I2C_Info records ***
//

void free_sysfs_i2c_info(Sysfs_I2C_Info * info) {
   if (info) {
      free(info->name);
      free(info->adapter_path);
      free(info->adapter_class);
      free(info->driver);
      free(info->driver_version);
      g_ptr_array_free(info->conflicting_driver_names, true);
      free(info);
   }
}


// GDestroyNotify
void destroy_sysfs_i2c_info(void * info) {
   free_sysfs_i2c_info( (Sysfs_I2C_Info*) info);
}


void dbgrpt_sysfs_i2c_info(Sysfs_I2C_Info * info, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Sysfs_I2C_Info", info, depth);
   rpt_vstring(d1, "busno:                     %d", info->busno);
   rpt_vstring(d1, "name:                      %s", info->name);
   rpt_vstring(d1, "adapter_path:              %s", info->adapter_path);
   rpt_vstring(d1, "adapter_class:             %s", info->adapter_class);
   rpt_vstring(d1, "driver:                    %s", info->driver);
   rpt_vstring(d1, "driver_version:            %s", info->driver_version);
   rpt_vstring(d1, "conflicting_driver_names:  %s",
         join_string_g_ptr_array_t(info->conflicting_driver_names, ", ") );
}


char * best_driver_name_for_n_nnnn(const char * dirname, const char * fn, int depth) {
   bool debug = false;

   char * best_name = NULL;
   char * attr = "name";
   RPT_ATTR_TEXT(depth, &best_name, dirname, fn, attr);
   if (!best_name) {
      // N.  subdirectory driver does not always exist, e.g. for ddcci N-0037
      attr = "driver/module";
      RPT_ATTR_REALPATH_BASENAME(depth, &best_name, dirname, fn, attr);
      if (!best_name) {
         attr = "modalias";
         RPT_ATTR_TEXT(depth, &best_name, dirname, fn, attr);
      }
   }

   DBGMSF(debug, "dirname=%s, fn=%s, using attr=%s, returning: %s",
                 dirname, fn, attr, best_name);
   return best_name;
}


// typedef Dir_Foreach_Func
void simple_one_n_nnnn(
      const char * dir_name,  // e.g. /sys/bus/i2c/devices/i2c-4
      const char * fn,        // e.g. 4-0037
      void *       accumulator,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "dirname=%s, fn=%s, depth=%d", dir_name, fn, depth);

   char * best_name = best_driver_name_for_n_nnnn(dir_name, fn, depth);
   if (best_name) {
      gaux_string_ptr_array_include(accumulator,best_name );
      DBGMSF(debug, "appending: |%s|", best_name);
   }
   DBGMSF(debug, "Done");
}


Sysfs_I2C_Info *  get_i2c_info(int busno, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d, depth=%d", busno, depth);
   char bus_path[40];
   g_snprintf(bus_path, 40, "/sys/bus/i2c/devices/i2c-%d", busno);
   Sysfs_I2C_Info * result = calloc(1, sizeof(Sysfs_I2C_Info));
   result->busno = busno;
   RPT_ATTR_TEXT(depth, &result->name, bus_path, "name");
   char * adapter_path  = find_adapter(bus_path, depth);
   if (adapter_path) {
      result->adapter_path = adapter_path;
      RPT_ATTR_TEXT(             depth, &result->adapter_class,  adapter_path, "class");
      RPT_ATTR_REALPATH_BASENAME(depth, &result->driver,         adapter_path, "driver");
      RPT_ATTR_TEXT(             depth, &result->driver_version, adapter_path, "driver/module/version");
   }

   result->conflicting_driver_names = g_ptr_array_new_with_free_func(g_free);

   DBGMSF(debug, "Looking for D-00hh match");
   char sbusno[4];
   g_snprintf(sbusno, 4, "%d",busno);
   dir_ordered_foreach_with_arg(
         "/sys/bus/i2c/devices",
         predicate_exact_D_00hh, sbusno,
         NULL,               // compare func
         simple_one_n_nnnn,
         result->conflicting_driver_names,
         depth);
   DBGMSF(debug, "After collecting /sys/bus/i2c/devices subdirectories: %s",
                      join_string_g_ptr_array_t(result->conflicting_driver_names, ", "));

   dir_filtered_ordered_foreach(
         bus_path,
         is_n_nnnn, NULL,
         simple_one_n_nnnn,
         result->conflicting_driver_names,
         depth);
   DBGMSF(debug, "After collecting %s subdirectories: %s", bus_path,
                     join_string_g_ptr_array_t(result->conflicting_driver_names, ", "));



   DBGMSF(debug, "Done. Returning %p", (void*) result);
   return result;
}


// typedef Dir_Foreach_Func
void simple_get_i2c_info(
      const char * dir_name,  // e.g. /sys/bus/i2c/devices/i2c-4
      const char * fn,        // e.g. 4-0037
      void *       accumulator,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dir_name=%s, fn=%s, depth=%d", dir_name, fn, depth);
   int busno = i2c_name_to_busno(fn);
   Sysfs_I2C_Info * info = get_i2c_info(busno, depth);
   g_ptr_array_add(accumulator, info);
   DBGMSF(debug, "Done. accumulator now has %d records", ((GPtrArray*)accumulator)->len);
}

static GPtrArray * all_i2c_info;

GPtrArray * get_all_i2c_info(bool rescan, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. depth=%d", depth);
   if (all_i2c_info && rescan)  {
      g_ptr_array_free(all_i2c_info, true);
      all_i2c_info = NULL;
    }

   GPtrArray * all = g_ptr_array_new_with_free_func(destroy_sysfs_i2c_info);

   dir_ordered_foreach(
         "/sys/bus/i2c/devices",
         startswith_i2c,
         i2c_compare,
         simple_get_i2c_info,
         all,
         depth);
   DBGMSF(debug, "Returning array of %d records", all->len);
   all_i2c_info = all;    // its a hack
   return all;
}


void dbgrpt_all_sysfs_i2c_info(GPtrArray * infos, int depth) {
   rpt_vstring(depth, "All Sysfs_I2C_Info records");
   if (infos && infos->len > 0) {
      for (int ndx = 0; ndx < infos->len; ndx++)
         dbgrpt_sysfs_i2c_info(g_ptr_array_index(infos,ndx), depth+1);
   }
   else
      rpt_vstring(depth+1, "None");
}


char * get_conflicting_drivers_for_bus(int busno) {
   Sysfs_I2C_Info * info = get_i2c_info(busno, -1);
   char * result = join_string_g_ptr_array(info->conflicting_driver_names, ", ");
   free_sysfs_i2c_info(info);
   return result;
}
