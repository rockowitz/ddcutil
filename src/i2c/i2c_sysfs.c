/** @file i2c_sysfs.c
 *
 *  Query /sys file system for information on I2C devices
 */

// Copyright (C) 2020-2023 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include "util/utilrpt.h"

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/rtti.h"

#include "i2c/i2c_sysfs.h"

static const DDCA_Trace_Group  TRACE_GROUP = DDCA_TRC_NONE;

//
// *** Common Functions
//

/** Given a sysfs node, walk up the chain of device directory links
 *  until an adapter node is found.
 *
 *  @param  path   e.g. /sys/bus/i2c/drivers/i2c-5
 *  @param  depth  logical indentation depth
 *  @return sysfs path to adapter
 *
 *  Parameter **depth** behaves as usual for sysfs RPT_... functions.
 *  If depth >= 0, sysfs attributes are reported.
 *  If depth <  0, there is no output
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


/** Given the sysfs path to an adapter of some sort, returns
 *  the name of its driver.
 *
 *  @param adapter_path
 *  @param depth        logical indentation depth
 *  @return name of driver module, NULL if not found
 *
 *  Parameter **depth** behaves as usual for sysfs RPT_... functions.
 *  If depth >= 0, sysfs attributes are reported.
 *  If depth <  0, there is no output
 *
 *  Caller is responsible for freeing the returned value
 */
char * get_driver_for_adapter(char * adapter_path, int depth) {
   char * basename = NULL;
   RPT_ATTR_REALPATH_BASENAME(depth, &basename, adapter_path, "driver", "module");
   return basename;
}


/** Given a sysfs node, walk up the chain of device directory links
 *  until an adapter node is found, and return the name of its driver.
 *
 *  @param  path   e.g. /sys/bus/i2c/drivers/i2c-5
 *  @param  depth  logical indentation depth
 *  @return sysfs path to adapter
 *
 *  Parameter **depth** behaves as usual for sysfs RPT_... functions.
 *  If depth >= 0, sysfs attributes are reported.
 *  If depth <  0, there is no output
 *
 *  Caller is responsible for freeing the returned value
 */
static char *
find_adapter_and_get_driver(char * path, int depth) {
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
 * @param  busno   I2 bus number
 * @return driver name, NULL if can't determine
 *
 * Caller is responsible for freeing the returned string.
 */
char * get_driver_for_busno(int busno) {
   char path[PATH_MAX];
   g_snprintf(path, PATH_MAX, "/sys/bus/i2c/devices/i2c-%d", busno);
   char * result = find_adapter_and_get_driver(path, -1);
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
      char * s0 = g_strdup( simple_fn + 4);   // work around const char *
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

// static
void
read_i2cN_device_node(
      const char *   device_path,
      I2C_Sys_Info * info,
      int            depth)
{
   assert(device_path);
   assert(info);
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "device_path=%s", device_path);
   int d0 = depth;
   if (debug && d0 < 0)
      d0 = 2;

   char * i2c_N = g_path_get_basename(device_path);
   RPT_ATTR_TEXT( d0, &info->device_name,    device_path, "name");
   RPT_ATTR_TEXT( d0, &info->i2c_dev_dev,    device_path, "i2c-dev", i2c_N, "dev");
   RPT_ATTR_TEXT( d0, &info->i2c_dev_name,   device_path, "i2c-dev", i2c_N, "name");
   free(i2c_N);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
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

// static
void
read_drm_dp_card_connector_node(
      const char *   connector_path,
      I2C_Sys_Info * info,
      int            depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "connector_path=%s", connector_path);
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

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}



// Process a <controller>/drm/cardN/cardN-<connector> for case when
// cardN-<connector> is not a DisplayPort connector

// static
void
read_drm_nondp_card_connector_node(
      const char * dirname,                // e.g /sys/devices/pci.../card0
      const char * connector,              // e.g card0-DP-1
      void *       accumulator,
      int          depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dirname=%s, connector=%s", dirname, connector);
   int d1 = (depth < 0) ? -1 : depth + 1;
   if (debug && d1 < 0)
      d1 = 2;
   I2C_Sys_Info * info = accumulator;

   if (info->connector) {  // already handled by read_drm_dp_card_connector_node()
      DBGTRC_DONE(debug, TRACE_GROUP, "Connector already found, skipping");
      return;
   }

   bool is_dp = RPT_ATTR_SINGLE_SUBDIR(depth, NULL, str_starts_with, "drm_dp_aux", dirname, connector);
   if (is_dp) {
      DBGTRC_DONE(debug, TRACE_GROUP, "Is display port connector, skipping");
      return;
   }

   char i2cN[20];
   g_snprintf(i2cN, 20, "i2c-%d", info->busno);
   bool found_i2c = RPT_ATTR_SINGLE_SUBDIR(depth, NULL, streq, i2cN, dirname, connector, "ddc/i2c-dev");
   if (found_i2c) {
      info->connector = g_strdup(connector);
      RPT_ATTR_TEXT(d1, NULL, dirname, connector, "ddc", "name");
      RPT_ATTR_TEXT(d1, NULL, dirname, connector, "ddc/i2c-dev", i2cN, "dev");
      RPT_ATTR_TEXT(d1, NULL, dirname, connector, "ddc/i2c-dev", i2cN, "name");
      RPT_ATTR_EDID(d1, NULL, dirname, connector, "edid");
      RPT_ATTR_TEXT(d1, NULL, dirname, connector, "enabled");
      RPT_ATTR_TEXT(d1, NULL, dirname, connector, "status");
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
   return;
}


// Dir_Foreach_Func
// Process a <controller>/drm/cardN node

// static
void
one_drm_card(
      const char * dirname,     // e.g /sys/devices/pci
      const char * fn,          // card0, card1 ...
      void *       info,
      int          depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dirname=%s, fn=%s", dirname, fn);
   char buf[PATH_MAX];
   g_snprintf(buf, PATH_MAX, "%s/%s", dirname, fn);
   dir_ordered_foreach(
         buf,
         predicate_cardN_connector,
         gaux_ptr_scomp,
         read_drm_nondp_card_connector_node,
         info,
         depth);
   DBGTRC_DONE(debug, TRACE_GROUP, "");
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

// static
void
read_pci_display_controller_node(
      const char *   nodepath,
      int            busno,
      I2C_Sys_Info * info,
      int            depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d, nodepath=%s", busno, nodepath);
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
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Calling dir_ordered_foreach, buf=%s, predicate predicate_cardN_connector()", buf);
      dir_ordered_foreach(buf, predicate_cardN_connector, i2c_compare, one_drm_card, info, depth1);
   }
   free(class);
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


I2C_Sys_Info *
get_i2c_sys_info(
      int busno,
      int depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d. depth=%d", busno, depth);
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
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "pci_i2c_device_path=%s", pci_i2c_device_path);
      read_i2cN_device_node(pci_i2c_device_path, result, d1);

      RPT_ATTR_REALPATH(d1, &pci_i2c_device_parent, pci_i2c_device_path, "..");
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "pci_i2c_device_parent=%s", pci_i2c_device_parent);

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
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %p", result);
   return result;
}


/** Emit debug type report of a #I2C_Sys_Info struct
 *
 *  @param  info   pointer to struct with relevant /sys information
 *  @param  depth  logical indentation depth, if < 0 perform no indentation
 */
void dbgrpt_i2c_sys_info(I2C_Sys_Info * info, int depth) {
   int d1 = (depth < 0) ? 0 : depth + 1;
   int d2 = (depth < 0) ? 0 : depth + 2;
   if (depth < 0)
      depth = 0;

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
      rpt_vstring(depth, "Ignoring %s/%s", dirname, fn);
   }
   else {
      rpt_vstring(depth, "Examining /sys/bus/i2c/devices/i2c-%d...", busno);
      int d1 = depth+1;
      // d1 > 0 => get_i2c_sys_info() reports as it collects, no need to call report_i2c_sys_info()
      I2C_Sys_Info * info = get_i2c_sys_info(busno, d1);
      // report_i2c_sys_info(info, depth+1);
      free_i2c_sys_info(info);
   }
}


void dbgrpt_sys_bus_i2c(int depth) {
   rpt_label(depth, "Examining /sys/bus/i2c/devices:");
   dir_ordered_foreach("/sys/bus/i2c/devices", NULL, i2c_compare, report_one_bus_i2c, NULL, depth);
}


//
//  *** Scan /sys by drm connector - uses struct Sys_Drm_Connector ***
//

static GPtrArray * sys_drm_connectors = NULL;  // Sys_Drm_Display;

// from query_sysenv_sysfs
// 9/28/2021 Requires hardening, testing on other than amdgpu, MST etc


void free_sys_drm_display(void * display) {
   bool debug = false;
   DBGMSF(debug, "Starting. display=%p", display);
   if (display) {
      Sys_Drm_Connector * disp = display;
      free(disp->connector_name);
      free(disp->connector_path);
      free(disp->name);
      free(disp->dev);
      free(disp->ddc_dir_path);
      free(disp->base_name);
      free(disp->base_dev);
      free(disp->edid_bytes);
      free(disp->enabled);
      free(disp->status);
      free(disp);
   }
   DBGMSF(debug, "Done.");
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
   DBGTRC_STARTING(debug, TRACE_GROUP, "dirname=%s, fn=%s, depth=%d", dirname, fn, depth);
   int d0 = depth;
   if (depth < 0 && (IS_DBGTRC(debug, TRACE_GROUP)))
      d0 = 2;
   GPtrArray * drm_displays = accumulator;

   Sys_Drm_Connector * cur = calloc(1, sizeof(Sys_Drm_Connector));
   cur->i2c_busno = -1;      // 0 is valid bus number
   cur->base_busno = -1;
   g_ptr_array_add(drm_displays, cur);
   cur->connector_name = g_strdup(fn);   // e.g. card0-DP-1
   RPT_ATTR_REALPATH(d0, &cur->connector_path,
                                       dirname, fn);
   RPT_ATTR_TEXT(d0, &cur->enabled, dirname, fn, "enabled");   // e.g. /sys/class/drm/card0-DP-1/enabled
   RPT_ATTR_TEXT(d0, &cur->status,  dirname, fn, "status"); // e.g. /sys/class/drm/card0-DP-1/status

   GByteArray * edid_byte_array = NULL;
   RPT_ATTR_EDID(d0, &edid_byte_array, dirname, fn, "edid");   // e.g. /sys/class/drm/card0-DP-1/edid
   // DBGMSG("edid_byte_array=%p", (void*)edid_byte_array);
   if (edid_byte_array) {
     cur->edid_size = edid_byte_array->len;
     cur->edid_bytes = g_byte_array_free(edid_byte_array, false);
     // DBGMSG("Setting cur->edid_bytes = %p", (void*)cur->edid_bytes);
   }

   char * driver = find_adapter_and_get_driver( cur->connector_path, -1);
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "driver=%s", driver);
   if (!(streq(driver, "nvidia") ))  {
      bool has_drm_dp_aux_subdir =          // does is exist? /sys/class/drm/card0-DP-1/drm_dp_aux0
            RPT_ATTR_SINGLE_SUBDIR(
                  d0,
                  NULL,     // char **      value_loc,
                  fn_starts_with,
                  "drm_dp_aux",
                  dirname,
                  fn);

      // does e.g. /sys/class/drm/card0-DP-1/i2c-6 exist?
      // *** BAD TEST, Nvida driver does not have drm_dp_aux subdir for DP

      char * i2cN_buf = NULL;   // i2c-N
      bool has_i2c_subdir =
            RPT_ATTR_SINGLE_SUBDIR(d0, &i2cN_buf, fn_starts_with,"i2c-",
                                   dirname, fn);

      if ( (has_drm_dp_aux_subdir != has_i2c_subdir) && d0 >= 0 )
         rpt_vstring(d0, "Unexpected: drm_dp_aux subdirectory %s, bug i2c- subdirectory %s",
               has_drm_dp_aux_subdir ? "exists" : "does not exist",
               has_i2c_subdir ? "exists": "does not exist");
      // ASSERT_IFF(has_drm_dp_aux_subdir, has_i2c_subdir);

      cur->is_aux_channel = has_drm_dp_aux_subdir;
      DBGMSF(debug, "cur->is_aux_channel = %s", SBOOL(has_i2c_subdir));
      if (has_i2c_subdir) {  // DP
         cur->i2c_busno = i2c_name_to_busno(i2cN_buf);

         // e.g. /sys/class/drm/card0-DP-1/i2c-6/name:
         char * buf = NULL;
         RPT_ATTR_TEXT(d0, &cur->name, dirname, fn, i2cN_buf, "name");
         RPT_ATTR_TEXT(d0, &buf,       dirname, fn, i2cN_buf, "i2c-dev", i2cN_buf, "name");
         // DBGMSG("name = |%s|", cur->name);
         // DBGMSG("buf  = |%s|", buf);

         // assert(streq(cur->name, buf));
         if (!streq(cur->name, buf) && d0 >= 0 )
            rpt_vstring(d0, "Unexpected: name and i2c-dev/%s/name do not match", i2cN_buf);

         free(buf);

         RPT_ATTR_TEXT(d0, &cur->dev,  dirname, fn, i2cN_buf, "i2c-dev", i2cN_buf, "dev");
         free(i2cN_buf);
         if (depth >= 0)
            rpt_nl();

         // Examine ddc subdirectory - does not exist on Nvidia driver
         bool has_ddc_subdir = RPT_ATTR_NOTE_SUBDIR(-1, NULL, dirname, fn, "ddc");
         if (has_ddc_subdir) {
            RPT_ATTR_REALPATH(-1, &cur->ddc_dir_path,    dirname, fn, "ddc");
            // e.g. /sys/class/drm/card0-DP-1/ddc/name:
            RPT_ATTR_TEXT(d0, &cur->base_name, dirname, fn, "ddc", "name");

            bool has_i2c_dev_subdir = RPT_ATTR_NOTE_SUBDIR(-1, NULL, dirname, fn, "ddc", "i2c-dev");
            if (has_i2c_dev_subdir) {
               // looking for e.g. /sys/bus/drm/card0-DP-1/ddc/i2c-dev/i2c-1
               has_i2c_subdir =
                  RPT_ATTR_SINGLE_SUBDIR(d0, &i2cN_buf, fn_starts_with, "i2c-",
                                         dirname, fn, "ddc", "i2c-dev");
               if (has_i2c_subdir) {
                  cur->base_busno = i2c_name_to_busno(i2cN_buf);
                  char * buf = NULL;
                  RPT_ATTR_TEXT(d0, &buf, dirname, fn, "ddc", "i2c-dev", i2cN_buf, "name");

                  // assert (streq(buf, cur->base_name));
                  if (!streq(buf, cur->base_name) && d0 >= 0 )
                     rpt_vstring(d0, "Unexpected: %s/ddc/i2c-dev/%s/name and ddc/i2c-dev/%s/name do not match",
                                     fn, i2cN_buf, fn);

                  free(buf);
                  RPT_ATTR_TEXT(d0, &cur->base_dev, dirname, fn, "ddc", "i2c-dev", i2cN_buf, "dev");
                  free(i2cN_buf);
               }
            }
         }
      } // has_i2c_subdir
      else {   // not DP
         if (depth >= 0)
            rpt_nl();

         bool found_ddc = RPT_ATTR_REALPATH(d0, &cur->ddc_dir_path,    dirname, fn, "ddc");
         ASSERT_IFF(found_ddc, cur->ddc_dir_path);  // guaranteed by RPT_ATTR_REALPATH()
         if (cur->ddc_dir_path) {
            // No ddc directory on Nvidia?
            // Examine ddc subdirectory
            // e.g. /sys/class/drm/card0-DP-1/ddc/name:
            RPT_ATTR_TEXT(d0, &cur->name,    dirname, fn, "ddc", "name");

            char * i2cN_buf = NULL;
            // looking for e.g. /sys/bus/drm/card0-DVI-D-1/ddc/i2c-dev/i2c-1
            has_i2c_subdir =
                RPT_ATTR_SINGLE_SUBDIR(d0, &i2cN_buf, fn_starts_with, "i2c-",
                                                dirname, fn, "ddc", "i2c-dev");
            if (has_i2c_subdir) {
                cur->i2c_busno = i2c_name_to_busno(i2cN_buf);
               char * buf = NULL;
               RPT_ATTR_TEXT(d0, &buf,       dirname, fn, "ddc", "i2c-dev", i2cN_buf, "name");
               RPT_ATTR_TEXT(d0, &cur->base_dev,
                                                dirname, fn, "ddc", "i2c-dev", i2cN_buf, "dev");

               // assert (streq(buf, cur->name));
               if (!streq(buf, cur->name) && d0 >= 0 )
                  rpt_vstring(d0, "Unexpected: %s/ddc/i2c-dev/%s/name and %s/ddc/name do not match",
                                  fn, i2cN_buf, fn);

               free(buf);
               free(i2cN_buf);
            }
         }  // had ddc subdirectory
      }   // not DP

   }  // not Nvidia
   free(driver);
   if (depth >= 0)
      rpt_nl();
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


/**
 *
 *  @param  depth  logical indentation depth, if < 0 do not emit report
 *  @return array of #Sys_Drm_Connector structs, one for each connector found
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
         sys_drm_connectors,         // accumulator, GPtrArray *
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
   DBGTRC_STARTING(debug, TRACE_GROUP, "depth=%d", depth);
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
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


Sys_Drm_Connector *
find_sys_drm_connector(int busno, Byte * edid, const char * connector_name) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "busno=%d, edid=%p, connector_name=%s",
                                        busno, (void*)edid, connector_name);
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
         if (connector_name && streq(connector_name, cur->connector_name)) {
            DBGMSF(debug, "Matched by connector_name");
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
   Sys_Drm_Connector * result = find_sys_drm_connector(busno, NULL, NULL);
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p", (void*) result);
   return result;
}


Sys_Drm_Connector * find_sys_drm_connector_by_edid(Byte * raw_edid) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "edid=%p", (void*) raw_edid);
   Sys_Drm_Connector * result = find_sys_drm_connector(-1, raw_edid, NULL);
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p", (void*) result);
   return result;
}


//
//  Scan for conflicting modules/drivers: Struct Sys_Conflicting_Driver
//

void free_sys_conflicting_driver(Sys_Conflicting_Driver * rec) {
   bool debug = false;
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
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dirname=%s, fn=%s, depth=%d", dir_name, fn, depth);

   GPtrArray* conflicting_drivers= accumulator;
   Sys_Conflicting_Driver * conflicting_driver = calloc(1, sizeof(Sys_Conflicting_Driver));
   DBGMSF(debug, "Allocated Sys_Conflicting_Driver %p", (void*) conflicting_driver);
   conflicting_driver->n_nnnn = g_strdup(fn);

   RPT_ATTR_TEXT(depth, &conflicting_driver->name, dir_name, fn, "name");

   if (str_ends_with(fn, "0050")) {
      GByteArray * edid_byte_array = NULL;
      RPT_ATTR_EDID(depth, &edid_byte_array, dir_name, fn, "eeprom");
      if (edid_byte_array) {
         conflicting_driver->eeprom_edid_size = edid_byte_array->len;
         conflicting_driver->eeprom_edid_bytes = g_byte_array_free(edid_byte_array, false);
      }
   }

   // N.  subdirectory driver does not always exist, e.g. for ddcci - N-0037
   RPT_ATTR_REALPATH_BASENAME(depth, &conflicting_driver->driver_module, dir_name, fn, "driver/module");
   RPT_ATTR_TEXT(depth, &conflicting_driver->modalias, dir_name, fn, "modalias");

   g_ptr_array_add(conflicting_drivers, conflicting_driver);
   if (depth >= 0)
      rpt_nl();
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


static
void collect_conflicting_drivers0(GPtrArray * conflicting_drivers, int busno, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d, conflicting_drivers=%p", busno, (void*)conflicting_drivers);

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

   DBGTRC_DONE(debug, TRACE_GROUP, "" );
}


GPtrArray * collect_conflicting_drivers(int busno, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d, depth=%d", busno, depth);

   GPtrArray * conflicting_drivers = g_ptr_array_new_with_free_func(free_sys_conflicting_driver0);
   collect_conflicting_drivers0(conflicting_drivers, busno, depth);

   DBGTRC_DONE(debug, TRACE_GROUP,  "Returning: %p", (void*)conflicting_drivers);
   return conflicting_drivers;
}


GPtrArray * collect_conflicting_drivers_for_any_bus(int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   GPtrArray* all_connectors = get_sys_drm_connectors(false);
   GPtrArray * conflicting_drivers = g_ptr_array_new_with_free_func(free_sys_conflicting_driver0);
   for (int ndx = 0; ndx < all_connectors->len; ndx++) {
      Sys_Drm_Connector * cur = g_ptr_array_index(all_connectors, ndx);
      DBGMSF(debug, "cur->i2c_busno=%d", cur->i2c_busno);
      if (cur->i2c_busno >= 0)   // may not have been set
         collect_conflicting_drivers0(conflicting_drivers, cur->i2c_busno, depth);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %p", (void*) conflicting_drivers);
   return conflicting_drivers;
}


void report_conflicting_drivers(GPtrArray * conflicts, int depth) {
   if (conflicts && conflicts->len > 0) {
      for (int ndx=0; ndx < conflicts->len; ndx++) {
         Sys_Conflicting_Driver * cur = g_ptr_array_index(conflicts, ndx);
         dbgrpt_conflicting_driver(cur, depth);
      }
   }
   else
      rpt_label(depth, "No conflicting drivers found");
}


GPtrArray * conflicting_driver_names(GPtrArray * conflicts) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "conflicts=%p", (void*)conflicts);
   GPtrArray * result = g_ptr_array_new_with_free_func(g_free);
   for (int ndx = 0; ndx < conflicts->len; ndx++) {
      Sys_Conflicting_Driver * cur = g_ptr_array_index(conflicts, ndx);
      gaux_unique_string_ptr_array_include(result, best_conflicting_driver_name(cur));
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %s", join_string_g_ptr_array_t(result, " + ") );
   return result;
}


char * conflicting_driver_names_string_t(GPtrArray * conflicts) {
   GPtrArray * driver_names = conflicting_driver_names(conflicts);
   char * result = join_string_g_ptr_array_t(driver_names, ". ");
   g_ptr_array_free(driver_names, true);
   return result;
}


void free_conflicting_drivers(GPtrArray* conflicts) {
   if (conflicts)
      g_ptr_array_free(conflicts, true);
}


//
// *** Collect basic /dev/i2c-N information into Sysfs_I2C_Info records ***
//

static GPtrArray * all_i2c_info = NULL;


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
   DBGTRC_STARTING(debug, TRACE_GROUP, "dirname=%s, fn=%s", dirname, fn);

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

   DBGTRC_DONE(debug, TRACE_GROUP, "using attr=%s, returning: %s",
                 attr, best_name);
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
   DBGTRC_STARTING(debug, TRACE_GROUP, "dirname=%s, fn=%s, depth=%d", dir_name, fn, depth);

   char * best_name = best_driver_name_for_n_nnnn(dir_name, fn, depth);
   if (best_name) {
      gaux_unique_string_ptr_array_include(accumulator,best_name );
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "appending: |%s|", best_name);
      free(best_name);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


/** Returns a newly allocated #Sys_I2c_Info struct describing
 *  a /sys/bus/i2c/devices/i2c-N instance, and optionally reports the
 *  result of examining the instance
 *
 *  @param  busno  i2c bus number
 *  @param  depth  logical indentation depth, if < 0 do not emit report
 *  @result newly allocated #Sys_I2c_Info struct
 */
Sysfs_I2C_Info *  get_i2c_info(int busno, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d, depth=%d", busno, depth);

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
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "After collecting /sys/bus/i2c/devices subdirectories: %s",
                      join_string_g_ptr_array_t(result->conflicting_driver_names, ", "));

   dir_filtered_ordered_foreach(
         bus_path,              // e.g. /sys/bus/i2c/devices/i2c-0
         is_n_nnnn, NULL,
         simple_one_n_nnnn,
         result->conflicting_driver_names,
         depth);
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "After collecting %s subdirectories: %s", bus_path,
                     join_string_g_ptr_array_t(result->conflicting_driver_names, ", "));
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %p", (void*) result);
   return result;
}


/** Function of typedef Dir_Foreach_Func, called from #get_all_i2c_info()
 *  for each i2c-N device in /sys/bus/i2c/devices
 *
 *  @param  dir_name     always /sys/bus/i2c/devices
 *  @param  fn           i2c-N
 *  @param  accumulator  GPtrArray to which to add newly allocated Sysfs_I2c_Info
 *                       instance
 */
// typedef Dir_Foreach_Func
void get_single_i2c_info(
      const char * dir_name,  // e.g. /sys/bus/i2c/devices
      const char * fn,        // e.g. i2c-3
      void *       accumulator,
      int          depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dir_name=%s, fn=%s, depth=%d", dir_name, fn, depth);

   int busno = i2c_name_to_busno(fn);
   if (busno >= 0) {
      Sysfs_I2C_Info * info = get_i2c_info(busno, depth);
      g_ptr_array_add(accumulator, info);
   }
   
   DBGTRC_DONE(debug, TRACE_GROUP, "accumulator now has %d records", ((GPtrArray*)accumulator)->len);
}


/** Returns an array of #Sysfs_I2C_Info describing each i2c-N device in
 *  directory /sys/bus/i2c/devices, and optionally reports the contents
 *
 *  @param rescan  if true, discard cached array and rescan
 *  @param depth   logical indentation depth, if < 0 do not emit report
 *  @return pointer to array containing one #Sysfs_I2C_Info for each i2c-N device
 *
 *  The returned array is cached.  Caller should not free.
 */
GPtrArray * get_all_i2c_info(bool rescan, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "depth=%d", depth);

   if (all_i2c_info && rescan)  {
      g_ptr_array_free(all_i2c_info, true);
      all_i2c_info = NULL;
   }
   if (!all_i2c_info) {
      all_i2c_info = g_ptr_array_new_with_free_func(destroy_sysfs_i2c_info);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "newly allocated all_i2c_info=%p", all_i2c_info);
      dir_ordered_foreach(
            "/sys/bus/i2c/devices",
            predicate_i2c_N,
            i2c_compare,
            get_single_i2c_info,
            all_i2c_info,
            depth);
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning pointer to GPtrArray=%p, containing %d records",
                                   (void*)all_i2c_info, all_i2c_info->len);
   return all_i2c_info;
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


#ifdef UNUSED
static bool is_potential_i2c_display(Sysfs_I2C_Info * info) {
   assert(info);
   bool debug = false;
   char * uname = strdup_uc(info->name);
   bool result = str_starts_with(info->adapter_class, "0x03") && str_contains(uname, "SMBUS")<0;
   free(uname);
   DBGMSF(debug, "busno=%d, adapter_class=%s, name=%s, returning %s",
                 info->busno, info->adapter_class, info->name, SBOOL(result));
   return result;
}
#endif


/** Return the bus numbers for all video adapter i2c buses, filtering out
 *  those, such as ones with SMBUS in their name, that are cannot be used
 *  for DDC/CI communication with a monitor.
 *
 *  The numbers are determined by examining /sys/bus/i2c.
 *
 *  This function looks only in /sys. It does not verify that the
 *  corresponding /dev/i2c-N devices exist.
 */
Bit_Set_256 get_possible_ddc_ci_bus_numbers() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   Bit_Set_256 result = EMPTY_BIT_SET_256;
   GPtrArray * allinfo = get_all_i2c_info(true, -1);
   for (int ndx = 0; ndx < allinfo->len; ndx++) {
      Sysfs_I2C_Info* cur = g_ptr_array_index(allinfo, ndx);
      if (!sysfs_is_ignorable_i2c_device(cur->busno))
      // if (is_potential_i2c_display(cur))
         result = bs256_insert(result, cur->busno);
   }
   // result = bs256_insert(result, 33); // for testing
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %s", bs256_to_string(result, "0x", ", "));
   return result;
}


void consolidated_i2c_sysfs_report(int depth) {
   int d0 = depth;
   int d1 = depth+1;

   rpt_label(d0, "*** Sys_Drm_Connector report: Detailed /sys/class/drm report: ***");
   report_sys_drm_connectors(d1);
   rpt_nl();

   rpt_label(d0, "*** Sysfs_I2C_Info report ***");
   GPtrArray * reports = get_all_i2c_info(true, -1);
   dbgrpt_all_sysfs_i2c_info(reports, d1);
   rpt_nl();

   rpt_label(d0, "*** Sysfs I2C devices possibly associated with displays ***");
   Bit_Set_256 buses = get_possible_ddc_ci_bus_numbers();
   rpt_vstring(d0, "I2C buses to check: %s", bs256_to_string(buses, "x", " "));
   rpt_nl();

   rpt_label(d0, "*** Sys_Conflicting_Driver report: Check for Conflicting Device Drivers ***");
   GPtrArray * conflicts = collect_conflicting_drivers_for_any_bus(-1);
   if (conflicts && conflicts->len > 0) {
      report_conflicting_drivers(conflicts, d1);
      rpt_vstring(d1, "Likely conflicting drivers found: %s\n", conflicting_driver_names_string_t(conflicts));
   }
   else
      rpt_label(d1, "No conflicting drivers found");
   free_conflicting_drivers(conflicts);
   rpt_nl();

   rpt_label(0, "*** Sysfs Reports Done ***");
   rpt_nl();
}



#ifdef FOR_FUTURE_USE

Connector_Busno_Dref_Table * cbd_table = NULL;

void gdestroy_cbd(void * data) {
   Connector_Busno_Dref * cbd = data;
   free(cbd->connector);
}


Connector_Busno_Dref_Table * create_connector_busnfo_dref_table() {
   Connector_Busno_Dref_Table * cbdt = g_ptr_array_new_with_free_func(gdestroy_cbd);
   return cbdt;
}

Connector_Busno_Dref * new_cbd(const char * connector, int busno) {
   Connector_Busno_Dref * cbd = calloc(1, sizeof(Connector_Busno_Dref));
   cbd->connector = g_strdup(connector);
   cbd->busno = busno;
   return cbd;
}


Connector_Busno_Dref * new_cbd0(int busno) {
   Connector_Busno_Dref * cbd = calloc(1, sizeof(Connector_Busno_Dref));
   cbd->busno = busno;
   return cbd;
}

Connector_Busno_Dref * get_cbd_by_connector(const char * connector) {
   Connector_Busno_Dref * result = NULL;
   assert(cbd_table);
   for (int ndx = 0; ndx < cbd_table->len; ndx++) {
      Connector_Busno_Dref * cur = g_ptr_array_index(cbd_table, ndx);
      if (streq(connector, cur->connector)) {
         result = cur;
         break;
      }
   }
   return result;
}


// how to handle secondary busno's
Connector_Busno_Dref * get_cbd_by_busno(int busno) {
   Connector_Busno_Dref * result = NULL;
   assert(cbd_table);
   for (int ndx = 0; ndx < cbd_table->len; ndx++) {
      Connector_Busno_Dref * cur = g_ptr_array_index(cbd_table, ndx);
      if ( cur->busno == busno) {
         result = cur;
         break;
      }
   }
   return result;
}
// if dref != NULL, replaces, if NULL, just erases
void set_cbd_connector(Connector_Busno_Dref * cbd, Display_Ref * dref) {
   cbd->dref = dref;
}

void dbgrpt_cbd_table(Connector_Busno_Dref_Table * cbd_table, int depth) {
   rpt_structure_loc("cbd_table", cbd_table, depth);
   int d0 = depth;
   int d1 = depth+1;
   for (int ndx =  0; ndx < cbd_table->len; ndx++) {
      Connector_Busno_Dref* cur = g_ptr_array_index(cbd_table, ndx);
      rpt_structure_loc("Connector_Busno_Dref", cur, d0);
      rpt_vstring(d1, "connector:     %s", cur->connector);
      rpt_vstring(d1, "busno:         %s", cur->busno);
      rpt_vstring(d1, "dref:          %s", (cur->dref) ? dref_repr_t(cur->dref) : "NULL");

   }
}
#endif


void add_video_device_to_array(
      const char * dirname,     //
      const char * fn,
      void *       data,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "dirname=%s, fn=%s", dirname, fn);
   GPtrArray* accumulator = (GPtrArray*) data;
   char * s = g_strdup_printf("%s/%s", dirname, fn);
   g_ptr_array_add(accumulator, s);
   // RPT_ATTR_TEXT(    1, NULL, dirname, fn, "class");
   // RPT_ATTR_REALPATH(1, NULL, dirname, fn, "driver");
}


/** Gets all sysfs devices with class video device, i.e. x03
 *
 *  @return array of fully qualified device paths
 */
GPtrArray * get_sys_video_devices() {
   bool debug = false;
   GPtrArray * video_devices = g_ptr_array_new_with_free_func(g_free);
   DBGTRC_STARTING(debug, TRACE_GROUP, "video_devices=%p", video_devices);

   dir_filtered_ordered_foreach("/sys/bus/pci/devices",
                       has_class_display,      // filter function
                       NULL,                    // ordering function
                       add_video_device_to_array,
                       video_devices,                    // accumulator
                       -1);
   DBGTRC_DONE(debug, TRACE_GROUP,"Returning array with %d video devices", video_devices->len);
   return video_devices;
}


/** Checks that all video devices have DRM drivers.
 *
 *  @return true/false
 */
bool all_video_devices_drm() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   GPtrArray * video_devices = get_sys_video_devices();
   bool all_devices_drm = true;
   for (int ndx = 0; ndx < video_devices->len; ndx++) {
      char * device_path = g_ptr_array_index(video_devices, ndx);
      int d = IS_DBGTRC(debug,DDCA_TRC_NONE) ? -1 : 1;
      bool found_drm = RPT_ATTR_NOTE_SUBDIR(d, NULL, device_path, "drm");
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
            "device_path=|%s|, found drm=%s", device_path, sbool(found_drm));
      if (!found_drm) {
         all_devices_drm = false;
         break;
      }
   }
   g_ptr_array_free(video_devices, true);

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, all_devices_drm, "");
   return all_devices_drm;
}


/** If the display has an open-source conformant driver,
 *  returns the connector name.
 *
 *  If the display has a DRM driver that doesn't conform
 *  to the standard (I'm looking at you, Nvidia), or it
 *  is not a DRM driver, returns NULL.
 *
 *  @param busno
 *  @return connector name, caller must free
 */
char * get_drm_connector_by_busno(int busno) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno = %d", busno);
   char * result = NULL;
   Sys_Drm_Connector * drm_connector = find_sys_drm_connector_by_busno(busno);
   if (drm_connector) {
      result = g_strdup(drm_connector->connector_name);
   }
   DBGMSF(debug, "Done. Returning %s", result);
   return result;
}


/** Checks if a display has a DRM driver by looking for
 *  subdirectory drm in the adapter directory.
 *
 *  Note that this test does not detect which connector
 *  is associated with the display.
 *
 *  @param busno   I2C bus number
 *  @return true/false
 */
 bool is_drm_display_by_busno(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno = %d", busno);
   bool result = false;
   char i2cdir[40];
   g_snprintf(i2cdir, 40, "/sys/bus/i2c/devices/i2c-%d",busno);
   char * real_i2cdir = NULL;
   GET_ATTR_REALPATH(&real_i2cdir, i2cdir);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "real_i2cdir = %s", real_i2cdir);
   assert(real_i2cdir);
   int depth = IS_DBGTRC(debug, TRACE_GROUP) ? 1 : -1;
   char * adapter_dir = find_adapter(real_i2cdir, depth);
   assert(adapter_dir);
   result = RPT_ATTR_NOTE_SUBDIR(depth, NULL, adapter_dir, "drm");
   free(real_i2cdir);
   free(adapter_dir);
   DBGTRC_RET_BOOL(debug, TRACE_GROUP, result, "");
   return result;
}


/** Module initialization */
void init_i2c_sysfs() {

   // I2C_Sys_Info
   RTTI_ADD_FUNC(read_i2cN_device_node);
   RTTI_ADD_FUNC(read_drm_dp_card_connector_node);
   RTTI_ADD_FUNC(read_drm_nondp_card_connector_node);
   RTTI_ADD_FUNC(one_drm_card);
   RTTI_ADD_FUNC(read_pci_display_controller_node);
   RTTI_ADD_FUNC(get_i2c_sys_info);

   // Sys_Drm_Connector
   RTTI_ADD_FUNC(one_drm_connector);
   RTTI_ADD_FUNC(scan_sys_drm_connectors);
   RTTI_ADD_FUNC(report_sys_drm_connectors);
   RTTI_ADD_FUNC(find_sys_drm_connector);
   RTTI_ADD_FUNC(find_sys_drm_connector_by_busno);
   RTTI_ADD_FUNC(find_sys_drm_connector_by_edid);

   // conflicting drivers
   RTTI_ADD_FUNC(one_n_nnnn);
   RTTI_ADD_FUNC(collect_conflicting_drivers0);
   RTTI_ADD_FUNC(collect_conflicting_drivers);
   RTTI_ADD_FUNC(collect_conflicting_drivers_for_any_bus);
   RTTI_ADD_FUNC(conflicting_driver_names);

   // Sysfs_I2C_Info
   RTTI_ADD_FUNC(best_driver_name_for_n_nnnn);
   RTTI_ADD_FUNC(simple_one_n_nnnn);
   RTTI_ADD_FUNC(get_i2c_info);
   RTTI_ADD_FUNC(get_single_i2c_info);
   RTTI_ADD_FUNC(get_all_i2c_info);
   RTTI_ADD_FUNC(get_possible_ddc_ci_bus_numbers);

   // other
   RTTI_ADD_FUNC(get_sys_video_devices);
   RTTI_ADD_FUNC(all_video_devices_drm);
   RTTI_ADD_FUNC(get_drm_connector_by_busno);
   RTTI_ADD_FUNC(is_drm_display_by_busno);
}


/** Module termination.  Release resources. */
void terminate_i2c_sysfs() {
   if (all_i2c_info)
      g_ptr_array_free(all_i2c_info, true);
}

