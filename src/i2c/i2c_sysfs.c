/** @file i2c_sysfs.c
 *
 *  Query /sys file system for information on I2C devices
 */

// Copyright (C) 2020-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#include "config.h"

/** \cond */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/** \endcond */

#ifdef USE_LIBDRM
#include "util/drm_common.h"
#endif
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

#include "base/i2c_bus_base.h"
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
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "path=%s", path);
   char * devpath = NULL;
   if ( RPT_ATTR_NOTE_INDIRECT_SUBDIR(depth, NULL, path, "device") ) {
       if ( RPT_ATTR_TEXT(depth, NULL, path, "device", "class") ) {
          RPT_ATTR_REALPATH(depth, &devpath, path, "device");
       }
       else {
          char p2[PATH_MAX];
           g_snprintf(p2, PATH_MAX, "%s/device", path);
           devpath = find_adapter(p2, depth);
       }
   }
   DBGTRC_RETURNING(debug, TRACE_GROUP, devpath, "");
   return devpath;
}

// Driver related functions

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


bool fn_equal(const char * filename, const char * val) {
   return streq(filename, val);
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
// Detailed exploratory scan of sysfs
// Called from query_sysenv_sysfs.c
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

// *** End of I2C_Sys_Info


//
//  *** Scan /sys by drm connector - uses struct Sys_Drm_Connector ***
//

// from query_sysenv_sysfs
// 9/28/2021 Requires hardening, testing on other than amdgpu, MST etc

GPtrArray * sys_drm_connectors = NULL;  // Sys_Drm_Connector
GPtrArray * sys_drm_connectors_fixedinfo = NULL;  // future


/** Frees a Sys_Drm_Connector instance
 *
 *  @param pointer to instance to free
 */
void free_sys_drm_connector(void * conninfo) {
   bool debug = false;
   DBGMSF(debug, "Starting. conninfo=%p", conninfo);
   if (conninfo) {
      Sys_Drm_Connector * disp = conninfo;
      assert(memcmp(disp->marker, SYS_DRM_CONNECTOR_MARKER, 4) == 0);
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

// future simplified version
void free_sys_drm_connector_fixedinfo(void * display) {
   bool debug = false;
   DBGMSF(debug, "Starting. display=%p", display);
   if (display) {
      Sys_Drm_Connector_FixedInfo * disp = display;
      free(disp->connector_name);
      free(disp->connector_path);
      free(disp->name);
      // free(disp->dev);
      free(disp->ddc_dir_path);
      free(disp->base_name);
      free(disp->base_dev);
      free(disp->edid_bytes);
      // free(disp->enabled);
      // free(disp->status);
      free(disp);
   }
   DBGMSF(debug, "Done.");
}


/** Frees the persistent GPtrArray of #Sys_Drm_Connector instances pointed
 *  to by global sys_drm_connectors.
 */
void free_sys_drm_connectors() {
   if (sys_drm_connectors)
      g_ptr_array_free(sys_drm_connectors, true);
   sys_drm_connectors = NULL;
}

// future simplified version
void free_sys_drm_connectors_fixedinfo() {
   if (sys_drm_connectors_fixedinfo)
      g_ptr_array_free(sys_drm_connectors_fixedinfo, true);
   sys_drm_connectors_fixedinfo = NULL;
}


/** Reports the contents of one #Sys_Drm_Connector instance
 *
 *  @param depth logical indentation depth
 *  @param cur   pointer to instance
 */
void report_one_sys_drm_connector(int depth, Sys_Drm_Connector * cur)
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

// Simplified variant
void report_one_sys_drm_display_fixedinfo(int depth, Sys_Drm_Connector_FixedInfo * cur)
{
   int d0 = depth;
   int d1 = depth+1;

   rpt_vstring(d0, "Connector:   %s", cur->connector_name);
   rpt_vstring(d1, "i2c_busno:   %d", cur->i2c_busno);
   rpt_vstring(d1, "name:        %s", cur->name);
   // rpt_vstring(d1, "dev:         %s", cur->dev);
   // rpt_vstring(d1, "enabled:     %s", cur->enabled);
   // rpt_vstring(d1, "status:      %s", cur->status);

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


/** Scans a single connector directory of /sys/class/drm.
 *
 *  Has typedef Dir_Foreach_Func
 */
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
   memcpy(cur->marker, SYS_DRM_CONNECTOR_MARKER, 4);
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
      // *** BAD TEST, Nvidia driver does not have drm_dp_aux subdir for DP

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
         bool has_ddc_subdir = RPT_ATTR_NOTE_INDIRECT_SUBDIR(-1, NULL, dirname, fn, "ddc");
         if (has_ddc_subdir) {
            RPT_ATTR_REALPATH(-1, &cur->ddc_dir_path,    dirname, fn, "ddc");
            // e.g. /sys/class/drm/card0-DP-1/ddc/name:
            RPT_ATTR_TEXT(d0, &cur->base_name, dirname, fn, "ddc", "name");

            bool has_i2c_dev_subdir = RPT_ATTR_NOTE_INDIRECT_SUBDIR(-1, NULL, dirname, fn, "ddc", "i2c-dev");
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

// simplified variant
void one_drm_connector_fixedinfo(
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
   bool validate = true;
   GPtrArray * drm_displays = accumulator;

   Sys_Drm_Connector_FixedInfo * cur = calloc(1, sizeof(Sys_Drm_Connector_FixedInfo));
   cur->i2c_busno = -1;      // 0 is valid bus number
   cur->base_busno = -1;
   g_ptr_array_add(drm_displays, cur);
   cur->connector_name = g_strdup(fn);   // e.g. card0-DP-1
   RPT_ATTR_REALPATH(d0, &cur->connector_path, dirname, fn);

   GByteArray * edid_byte_array = NULL;
   RPT_ATTR_EDID(d0, &edid_byte_array, dirname, fn, "edid");   // e.g. /sys/class/drm/card0-DP-1/edid
   // DBGMSG("edid_byte_array=%p", (void*)edid_byte_array);
   if (edid_byte_array) {
     cur->edid_size = edid_byte_array->len;
     cur->edid_bytes = g_byte_array_free(edid_byte_array, false);
     // DBGMSG("Setting cur->edid_bytes = %p", (void*)cur->edid_bytes);
   }

   // char * driver = find_adapter_and_get_driver( cur->connector_path, -1);
   // DBGTRC_NOPREFIX(debug, TRACE_GROUP, "driver=%s", driver);

   bool has_drm_dp_aux_subdir =          // does is exist? /sys/class/drm/card0-DP-1/drm_dp_aux0
         RPT_ATTR_SINGLE_SUBDIR(d0, NULL, fn_starts_with, "drm_dp_aux", dirname, fn);
   cur->is_aux_channel = has_drm_dp_aux_subdir;

   char * i2cN_buf = NULL;   // i2c-N
   bool has_i2c_subdir =
            RPT_ATTR_SINGLE_SUBDIR(d0, &i2cN_buf, fn_starts_with,"i2c-", dirname, fn);

   if (has_i2c_subdir) {  // DP
      cur->i2c_busno = i2c_name_to_busno(i2cN_buf);

      // e.g. /sys/class/drm/card0-DP-1/i2c-6/name:
      char * buf = NULL;
      RPT_ATTR_TEXT(d0, &cur->name, dirname, fn, i2cN_buf, "name");

      if (validate) {
         RPT_ATTR_TEXT(d0, &buf,       dirname, fn, i2cN_buf, "i2c-dev", i2cN_buf, "name");
         // DBGMSG("name = |%s|", cur->name);
         // DBGMSG("buf  = |%s|", buf);
         // assert(streq(cur->name, buf));
         if (!streq(cur->name, buf) && d0 >= 0 )
            rpt_vstring(d0, "Unexpected: name and i2c-dev/%s/name do not match", i2cN_buf);
         free(buf);
      }

      // Examine ddc subdirectory - does not exist on Nvidia driver
      bool has_ddc_subdir = RPT_ATTR_NOTE_INDIRECT_SUBDIR(-1, NULL, dirname, fn, "ddc");
      if (has_ddc_subdir) {
         RPT_ATTR_REALPATH(-1, &cur->ddc_dir_path,    dirname, fn, "ddc");
         // e.g. /sys/class/drm/card0-DP-1/ddc/name:
         RPT_ATTR_TEXT(d0, &cur->base_name, dirname, fn, "ddc", "name");

         bool has_i2c_dev_subdir = RPT_ATTR_NOTE_INDIRECT_SUBDIR(-1, NULL, dirname, fn, "ddc", "i2c-dev");
         if (has_i2c_dev_subdir) {
            // looking for e.g. /sys/bus/drm/card0-DP-1/ddc/i2c-dev/i2c-1
            has_i2c_subdir =
                  RPT_ATTR_SINGLE_SUBDIR(d0, &i2cN_buf, fn_starts_with, "i2c-",
                                         dirname, fn, "ddc", "i2c-dev");
            if (has_i2c_subdir) {
               cur->base_busno = i2c_name_to_busno(i2cN_buf);

               if (validate) {
                  char * buf = NULL;
                  RPT_ATTR_TEXT(d0, &buf, dirname, fn, "ddc", "i2c-dev", i2cN_buf, "name");
                  // assert (streq(buf, cur->base_name));
                  if (!streq(buf, cur->base_name) && d0 >= 0 )
                     rpt_vstring(d0, "Unexpected: %s/ddc/i2c-dev/%s/name and ddc/i2c-dev/%s/name do not match",
                                    fn, i2cN_buf, fn);
                  free(buf);
               }

               RPT_ATTR_TEXT(d0, &cur->base_dev, dirname, fn, "ddc", "i2c-dev", i2cN_buf, "dev");
               free(i2cN_buf);
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
               RPT_ATTR_TEXT(d0, &cur->base_dev, dirname, fn, "ddc", "i2c-dev", i2cN_buf, "dev");

               char * buf = NULL;
               RPT_ATTR_TEXT(d0, &buf,       dirname, fn, "ddc", "i2c-dev", i2cN_buf, "name");
               // assert (streq(buf, cur->name));
               if (!streq(buf, cur->name) && d0 >= 0 )
                  rpt_vstring(d0, "Unexpected: %s/ddc/i2c-dev/%s/name and %s/ddc/name do not match",
                                  fn, i2cN_buf, fn);
               free(buf);
            }
            free(i2cN_buf);
         }  // had ddc subdirectory
      }   // not DP

      if (depth >= 0)
         rpt_nl();
      DBGTRC_DONE(debug, TRACE_GROUP, "");
   }
}


/** Collects information from all connector subdirectories of /sys/class/drm,
 *  optionally emitting a report.
 *
 *  @param  depth  logical indentation depth, if < 0 do not emit report
 *  @return array of #Sys_Drm_Connector structs, one for each connector found
 *
 *  Returns GPtrArray with 0 entries if no DRM displays found
 *
 *  Also sets global #sys_drm_connectors
 */
GPtrArray * scan_sys_drm_connectors(int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "depth=%d", depth);

   GPtrArray * sys_drm_connectors = g_ptr_array_new_with_free_func(free_sys_drm_connector);
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

// future simplified variant
GPtrArray * scan_sys_drm_connectors_fixedinfo(int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "depth=%d", depth);

   GPtrArray * sys_drm_connectors = g_ptr_array_new_with_free_func(free_sys_drm_connector_fixedinfo);
   dir_filtered_ordered_foreach(
         "/sys/class/drm",
         is_drm_connector,      // filter function
         NULL,                  // ordering function
         one_drm_connector_fixedinfo,
         sys_drm_connectors,         // accumulator, GPtrArray *
         depth);
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "size of sys_drm_connectors: %d", sys_drm_connectors->len);
   return sys_drm_connectors;
}


/** Gets the value of global #sys_drm_connectors.
 *
 *  @param rescan free the existing data structure
 *
 *  If sys_drm_connectors == NULL or rescan was set,
 *  scan the /sys/class/drm/<connector> directories
 */
GPtrArray* get_sys_drm_connectors(bool rescan) {
   if (sys_drm_connectors && rescan) {
      g_ptr_array_free(sys_drm_connectors, true);
      sys_drm_connectors = NULL;
   }
   if (!sys_drm_connectors)
      sys_drm_connectors = scan_sys_drm_connectors(-1);
   return sys_drm_connectors;
}

// future simplified variant
GPtrArray* get_sys_drm_connectors_fixedinfo(bool rescan) {
   if (sys_drm_connectors_fixedinfo && rescan) {
      g_ptr_array_free(sys_drm_connectors_fixedinfo, true);
      sys_drm_connectors_fixedinfo = NULL;
   }
   if (!sys_drm_connectors_fixedinfo)
      sys_drm_connectors_fixedinfo = scan_sys_drm_connectors_fixedinfo(-1);
   return sys_drm_connectors_fixedinfo;
}


/** Reports the contents of the array of #Sys_Drm_Connector instances
 *  pointed to by global #sys_drm_connectors. If #sys_drm_connectors is
 *  not NULL, scan the /sys/class/drm/<connecter> tree.
 */
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
         report_one_sys_drm_connector(depth, cur);
         rpt_nl();
      }
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}

// future simplified variant
void report_sys_drm_connectors_fixedinfo(int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "depth=%d", depth);
   int d0 = depth;
   int d1 = (debug) ? 2 : -1;
   rpt_nl();
   rpt_label(d0, "Display connectors reported by DRM:");
   if (!sys_drm_connectors_fixedinfo)
     sys_drm_connectors_fixedinfo = scan_sys_drm_connectors_fixedinfo(d1);
   GPtrArray * displays = sys_drm_connectors_fixedinfo;
   if (!displays || displays->len == 0) {
      rpt_label(d1, "None");
   }
   else {
      for (int ndx = 0; ndx < displays->len; ndx++) {
         Sys_Drm_Connector_FixedInfo * cur = g_ptr_array_index(displays, ndx);
         report_one_sys_drm_display_fixedinfo(depth, cur);
         rpt_nl();
      }
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


/** Find a #Sys_Drm_Connector instance using one of the I2C bus number,
 *  EDID value, or DRM connector name.
 *
 *  @param  busno   I2C bus number
 *  @param  edid    pointer to 128 byte EDID
 *  @param  connector_name  e.g. card0-HDMI-A-1
 *
 *  Scans /sys/class/drm if global #sys_class_drm not already set
 */
Sys_Drm_Connector *
find_sys_drm_connector(int busno, Byte * edid, const char * connector_name) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "busno=%d, edid=%p, connector_name=%s",
                                        busno, (void*)edid, connector_name);
   if (!sys_drm_connectors)
     sys_drm_connectors = scan_sys_drm_connectors(-1);
   assert(sys_drm_connectors);
   Sys_Drm_Connector * result = NULL;
   for (int ndx = 0; ndx < sys_drm_connectors->len; ndx++) {
      Sys_Drm_Connector * cur = g_ptr_array_index(sys_drm_connectors, ndx);
      // DBGMSG("cur->busno = %d", cur->i2c_busno);
      if (busno >= 0 && cur->i2c_busno == busno) {
         DBGTRC(debug, DDCA_TRC_NONE, "Matched by bus number");
         result = cur;
         break;
      }
      if (edid && cur->edid_size >= 128 && (memcmp(edid, cur->edid_bytes,128) == 0)) {
         DBGTRC(debug, DDCA_TRC_NONE, "Matched by edid");
         result = cur;
         break;
      }
      if (connector_name && streq(connector_name, cur->connector_name)) {
         DBGTRC(debug, DDCA_TRC_NONE, "Matched by connector_name");
         result = cur;
         break;
      }
   }
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p", (void*) result);
   return result;
}


int  sys_drm_get_busno_by_connector(const char * connector_name) {
   int result = -1;
   Sys_Drm_Connector * sdc = find_sys_drm_connector(-1, NULL, connector_name);
   if (sdc)
      result = sdc->i2c_busno;
   return result;
}



/** Searches for a Sys_Drm_Connector instance by I2C bus number.
 *
 *  @param  I2C bus number
 *  @return pointer to instance, NULL if not found
 */
Sys_Drm_Connector * find_sys_drm_connector_by_busno(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "busno=%d", busno);
   Sys_Drm_Connector * result = find_sys_drm_connector(busno, NULL, NULL);
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p: %s",
         result, (result) ? result->connector_name : "NOT FOUND");
   return result;
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
char * get_drm_connector_name_by_busno(int busno) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. busno = %d", busno);
   char * result = NULL;
   Sys_Drm_Connector * drm_connector = find_sys_drm_connector_by_busno(busno);
   if (drm_connector) {
      result = g_strdup(drm_connector->connector_name);
   }
   DBGTRC_RETURNING(debug, TRACE_GROUP, result, "");
   return result;
}


/** Searches for a Sys_Drm_Connector instance by EDID.
 *
 *  @param  pointer to 128 byte EDID value
 *  @return pointer to instance, NULL if not found
 */
Sys_Drm_Connector * find_sys_drm_connector_by_edid(Byte * raw_edid) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "edid=%p", (void*) raw_edid);
   Sys_Drm_Connector * result = find_sys_drm_connector(-1, raw_edid, NULL);
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p", (void*) result);
   return result;
}


/** Gets the DRM connector name, e.g. card0-DP-3, using the EDID.
 *
 *  @param  edid_bytes  pointer to 128 byte EDID
 *  @return connector name, NULL if not found
 */
char * get_drm_connector_name_by_edid(Byte * edid_bytes) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Finding connector by EDID...");
   char * result = NULL;
   Sys_Drm_Connector * connector_rec = find_sys_drm_connector_by_edid(edid_bytes);
   if (connector_rec) {
      result = g_strdup(connector_rec->connector_name);
   }
   DBGTRC_RETURNING(debug, TRACE_GROUP, result, "");
   return result;
}


/** Searches for a Sys_Drm_Connector instance by the connector name
 *
 *  @param  connector name
 *  @return pointer to instance, NULL if not found
 */
Sys_Drm_Connector * find_sys_drm_connector_by_connector_name(const char * name) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "name=|%s|", name);
   Sys_Drm_Connector * result = find_sys_drm_connector(-1, NULL, name);
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p", (void*) result);
   return result;
}

//
// End of Sys_Drm_Connector section
//


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


#ifdef UNUSED
static
void free_sys_conflicting_driver0(void * rec) {
   free_sys_conflicting_driver((Sys_Conflicting_Driver*) rec);
}
#endif


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

   GPtrArray * conflicting_drivers = g_ptr_array_new_with_free_func((GDestroyNotify) free_sys_conflicting_driver);
   collect_conflicting_drivers0(conflicting_drivers, busno, depth);

   DBGTRC_DONE(debug, TRACE_GROUP,  "Returning: %p", (void*)conflicting_drivers);
   return conflicting_drivers;
}


GPtrArray * collect_conflicting_drivers_for_any_bus(int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   GPtrArray* all_connectors = get_sys_drm_connectors(false);
   GPtrArray * conflicting_drivers = g_ptr_array_new_with_free_func((GDestroyNotify) free_sys_conflicting_driver);
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
// End of conflicting drivers section
//


//
// Sysfs_I2C_Info
//

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
#ifdef USE_LIBDRM
   rpt_vstring(d1, "adapter supports DRM:      %s",
         sbool(adapter_supports_drm_using_drm_api(info->adapter_path)));
#endif
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


/** Returns a newly allocated #Sysfs_I2c_Info struct describing
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
   if (debug)
      rpt_nl();
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
GPtrArray * get_all_sysfs_i2c_info(bool rescan, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "depth=%d", depth);

   if (all_i2c_info && rescan)  {
      g_ptr_array_free(all_i2c_info, true);
      all_i2c_info = NULL;
   }
   if (!all_i2c_info) {
      all_i2c_info = g_ptr_array_new_with_free_func((GDestroyNotify) free_sysfs_i2c_info);
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

//
// *** DRM Checks ***
//

/** Uses the Sys_I2C_Info array to get a list of all video adapters
 *  and checks if each supports DRM.
 *
 *  @param rescan  always recreate the Sys_I2C_Info array
 *  @return true if all display adapters support DRM, false if not
 */
bool all_sysfs_i2c_info_drm(bool rescan) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "rescan=%s", SBOOL(rescan));
   bool result = false;
#ifdef USE_LIBDRM
   GPtrArray* all_info = get_all_sysfs_i2c_info(rescan, -1);
   GPtrArray* adapter_paths = g_ptr_array_sized_new(4);
   g_ptr_array_set_free_func(adapter_paths, g_free);
   if (all_info->len > 0) {
      for (int ndx = 0; ndx < all_info->len; ndx++) {
         Sysfs_I2C_Info * info = g_ptr_array_index(all_info, ndx);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, adapter_class=%s, adapter_path=%s",
               info->busno, info->adapter_class, info->adapter_path);
         if (str_starts_with(info->adapter_class, "0x03")) {
               g_ptr_array_add(adapter_paths, info->adapter_path);
         }
      }
      result = all_video_adapters_support_drm_using_drm_api(adapter_paths);
   }
   g_ptr_array_free(adapter_paths, false);
#endif
   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "");
   return result;
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
 *  those, such as ones with SMBUS in their name, that are definitely not
 *  used for DDC/CI communication with a monitor.
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
   GPtrArray * allinfo = get_all_sysfs_i2c_info(true, -1);
   for (int ndx = 0; ndx < allinfo->len; ndx++) {
      Sysfs_I2C_Info* cur = g_ptr_array_index(allinfo, ndx);
      if (!sysfs_is_ignorable_i2c_device(cur->busno))
      // if (is_potential_i2c_display(cur))
         result = bs256_insert(result, cur->busno);
   }
   // result = bs256_insert(result, 33); // for testing
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %s", bs256_to_string_t(result, "0x", ", "));
   return result;
}


void consolidated_i2c_sysfs_report(int depth) {
   int d0 = depth;
   int d1 = depth+1;

   rpt_label(d0, "*** Sys_Drm_Connector report: Detailed /sys/class/drm report: ***");
   report_sys_drm_connectors(d1);
   rpt_nl();

   // not currently used, and leaks memory
   // rpt_label(d0, "*** Sys_Drm_Connector_FixedInfo report: Simplified /sys/class/drm report: ***");
   // report_sys_drm_connectors_fixedinfo(d1);
   // rpt_nl();

   rpt_label(d0, "*** Sysfs_I2C_Info report ***");
   GPtrArray * reports = get_all_sysfs_i2c_info(true, -1);
   dbgrpt_all_sysfs_i2c_info(reports, d1);
   rpt_nl();

   rpt_label(d0, "*** Sysfs I2C devices possibly associated with displays ***");
   Bit_Set_256 buses = get_possible_ddc_ci_bus_numbers();
   rpt_vstring(d0, "I2C buses to check: %s", bs256_to_string_t(buses, "x", " "));
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


#ifdef WRONG
// directory drm always exists, need to check if it has connector nodes

/** Checks that all video devices have DRM drivers.
 *
 *  @return true/false
 */
bool i2c_all_video_devices_drm() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   GPtrArray * video_devices = get_sys_video_devices();
   bool all_devices_drm = true;
   for (int ndx = 0; ndx < video_devices->len; ndx++) {
      char * device_path = g_ptr_array_index(video_devices, ndx);
      int d = IS_DBGTRC(debug,DDCA_TRC_NONE) ? 1 : -1;
      bool found_drm = RPT_ATTR_SINGLE_SUBDIR(d, NULL, fn_equal, "drm", device_path);
      if (!found_drm) {
         all_devices_drm = false;
         break;
      }
   }
   g_ptr_array_free(video_devices, true);

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, all_devices_drm, "");
   return all_devices_drm;
}
#endif


/** If possible, determines the drm connector for an I2C bus number.
 *  If insufficient fields exist in sysfs to do this with absolute
 *  assurance, EDID comparison is used.
 *
 *  Fields drm_connector_name and drm_connector_found_by are set.
 *  If the drm connector cannot be determined, drm_connector_found_by
 *  is set to DRM_CONNECTOR_NOT_FOUND.
 *
 *  @param businfo   I2C_Bus_Info record
 *  @return pointer to Sys_Drm_Connector instance
 */
Sys_Drm_Connector * i2c_check_businfo_connector(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Checking I2C_Bus_Info for /dev/i2c-%d", businfo->busno);
   businfo->drm_connector_found_by = DRM_CONNECTOR_NOT_FOUND;
   Sys_Drm_Connector * drm_connector = find_sys_drm_connector_by_busno(businfo->busno);
   if (drm_connector) {
     businfo->drm_connector_found_by = DRM_CONNECTOR_FOUND_BY_BUSNO;
     businfo->drm_connector_name = g_strdup(drm_connector->connector_name);
   }
   else if (businfo->edid) {
     drm_connector = find_sys_drm_connector_by_edid(businfo->edid->bytes);
     if (drm_connector) {
        businfo->drm_connector_name = g_strdup(drm_connector->connector_name);
        businfo->drm_connector_found_by = DRM_CONNECTOR_FOUND_BY_EDID;
     }
   }
   businfo->flags |= I2C_BUS_DRM_CONNECTOR_CHECKED;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Final businfo flags: %s", i2c_interpret_bus_flags_t(businfo->flags));
   if (businfo->drm_connector_name)
      DBGTRC_DONE(debug, TRACE_GROUP, "Returning: SYS_Drm_Connector for %s", businfo->drm_connector_name);
   else
      DBGTRC_RETURNING(debug, TRACE_GROUP, NULL, "");
   return drm_connector;
}




#ifdef OLD
/** Gets a list of all displays known to DRM.
 *
 *  \param sysfs_drm_cards
 *  \bool  verbose
 *  \return GPtrArray of connector names for DRM displays
 *
 *  \remark
 *  The caller is responsible for freeing the returned #GPtrArray.
 */
GPtrArray * get_sysfs_drm_displays_old(Byte_Bit_Flags sysfs_drm_cards, bool verbose)
{
   bool debug = false;
   int  depth = 0;
   int  d1    = depth+1;
   int  d2    = depth+2;

   struct dirent *dent;
   DIR           *dir1;
   char          *dname;
   char          dnbuf[90];
   const int     cardname_sz = 20;
   char          cardname[cardname_sz];

   GPtrArray * connected_displays = g_ptr_array_new();
   g_ptr_array_set_free_func(connected_displays, g_free);

#ifdef TARGET_BSD
   dname = "/compat/linux/sys/class/drm";
#else
   dname = "/sys/class/drm";
#endif
   DBGTRC_STARTING(debug, TRACE_GROUP, "Examining %s...", dname);
   Byte_Bit_Flags iter = bbf_iter_new(sysfs_drm_cards);
   int cardno = -1;
   while ( (cardno = bbf_iter_next(iter)) >= 0) {
      snprintf(cardname, cardname_sz, "card%d", cardno);
      snprintf(dnbuf, 80, "%s/%s", dname, cardname);
      dir1 = opendir(dnbuf);
      DBGMSF(debug, "dnbuf=%s", dnbuf);
      if (!dir1) {
         // rpt_vstring(d1, "Unable to open sysfs directory %s: %s\n", dnbuf, strerror(errno));
         break;
      }
      else {
         while ((dent = readdir(dir1)) != NULL) {
            // DBGMSG("%s", dent->d_name);
            // char cur_fn[100];
            if (str_starts_with(dent->d_name, cardname)) {
               if (verbose)
                  rpt_vstring(d1, "Found connector: %s", dent->d_name);
               char cur_dir_name[PATH_MAX];
               g_snprintf(cur_dir_name, PATH_MAX, "%s/%s", dnbuf, dent->d_name);
               char * s_status = read_sysfs_attr(cur_dir_name, "status", false);
               // rpt_vstring(d2, "%s/status: %s", cur_dir_name, s_status);
               if (verbose)
                  rpt_vstring(d2, "Display: %s, status=%s", dent->d_name, s_status);
               // edid present iff status == "connected"
               if (streq(s_status, "connected")) {
                  if (verbose) {
                     GByteArray * gba_edid = read_binary_sysfs_attr(
                           cur_dir_name, "edid", 128, /*verbose=*/ true);
                     if (gba_edid) {
                        rpt_vstring(d2, "%s/edid:", cur_dir_name);
                        rpt_hex_dump(gba_edid->data, gba_edid->len, d2);
                        g_byte_array_free(gba_edid, true);
                     }
                     else {
                        rpt_vstring(d2, "Reading %s/edid failed.", cur_dir_name);
                     }
                  }

                  g_ptr_array_add(connected_displays, g_strdup(dent->d_name));
               }
               free(s_status);
               if (verbose)
                  rpt_nl();
            }
         }
         closedir(dir1);
      }
   }
   bbf_iter_free(iter);
   g_ptr_array_sort(connected_displays, gaux_ptr_scomp);
   DBGTRC_DONE(debug, TRACE_GROUP, "Connected displays: %s",
                              join_string_g_ptr_array_t(connected_displays, ", "));
   return connected_displays;
}
#endif


#ifdef OLD_HOTPLUG_VERSION
/** Examines a single connector, e.g. card0-HDMI-1, in a directory /sys/class/drm/cardN
 *  to determine if it is has a monitor connected.  If so, appends the simple
 *  connector name to the list of active connectors
 *
 *  @param  dirname    directory to examine, <device>/drm/cardN
 *  @param  simple_fn  filename to examine
 *  @param  data       GPtrArray of connected monitors
 *  @param  depth      if >= 0, emits a report with this logical indentation depth
 *
 *  @remark
 *  Move get_sysfs_drm_examine_one_connector(), get_sysfs_drm_displays()
 * to sysfs_i2c_util.c?
 */
static
void get_sysfs_drm_examine_one_connector(
      const char * dirname,     // <device>/drm/cardN
      const char * simple_fn,   // card0-HDMI-1 etc
      void *       data,        // GPtrArray collecting connector names
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);
   GPtrArray * connected_displays = (GPtrArray *) data;

   char * status = NULL;
   bool found_status = RPT_ATTR_TEXT(-1, &status, dirname, simple_fn, "status");
   if (found_status && streq(status,"connected")) {
         g_ptr_array_add(connected_displays, g_strdup(simple_fn));
      }
   g_free(status);

   DBGMSF(debug, "Added connector %s", simple_fn);
}


/**Checks /sys/class/drm for connectors with active displays.
 *
 * @return newly allocated GPtrArray of DRM connector names, sorted
 */
static
GPtrArray * get_sysfs_drm_displays() {
   bool debug = false;
   char * dname =
 #ifdef TARGET_BSD
              "/compat/linux/sys/class/drm";
 #else
              "/sys/class/drm";
 #endif
   DBGTRC_STARTING(debug, TRACE_GROUP, "Examining %s", dname);
   GPtrArray * connected_displays = g_ptr_array_new_with_free_func(g_free);
   dir_filtered_ordered_foreach(
                 dname,
                 is_card_connector_dir,   // filter function
                 NULL,                    // ordering function
                 get_sysfs_drm_examine_one_connector,
                 connected_displays,      // accumulator
                 0);
   g_ptr_array_sort(connected_displays, gaux_ptr_scomp);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning Connected displays: %s",
                              join_string_g_ptr_array_t(connected_displays, ", "));
   return connected_displays;
 }
#endif



 /** Adds a single connector name, e.g. card0-HDMI-1, to the accumulated
  *  list of all connections, and if the connector has a valid EDID, to
  *  the accumlated list of connectors having a valid EDID.
  *
  *  @param  dirname    directory to examine, <device>/drm/cardN
  *  @param  simple_fn  filename to examine
  *  @param  data       pointer to Sysfs_Connector_Names instance
  *  @param  depth      if >= 0, emits a report with this logical indentation depth
  */
static
void get_sysfs_drm_add_one_connector_name(
      const char * dirname,     // <device>/drm/cardN
      const char * simple_fn,   // card0-HDMI-1 etc
      void *       data,        // pointer to Sysfs_Connector_Names collecting connector names
      int          depth)
{
   bool debug = false;
   Sysfs_Connector_Names * accum = (Sysfs_Connector_Names*) data;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);

   g_ptr_array_add(accum->all_connectors, strdup(simple_fn));
   bool collect = GET_ATTR_EDID(NULL, dirname, simple_fn, "edid");
   if (collect) {
      g_ptr_array_add(accum->connectors_having_edid, g_strdup(simple_fn));
      DBGMSF(debug, "Added connector %s", simple_fn);
   }
   DBGMSF(debug, "Connector %s has edid = %s", simple_fn, SBOOL(collect));
}


/**Checks /sys/class/drm for connectors.
 *
 * @return struct Sysfs_Connector_Names
 *
 * @remark
 * Note the result is returned on the stack, not the heap
 */
Sysfs_Connector_Names get_sysfs_drm_connector_names() {
   bool debug = false;
   char * dname =
 #ifdef TARGET_BSD
              "/compat/linux/sys/class/drm";
 #else
              "/sys/class/drm";
 #endif
   DBGTRC_STARTING(debug, TRACE_GROUP, "Examining %s", dname);

   Sysfs_Connector_Names connector_names = {NULL, NULL};
   connector_names.all_connectors = g_ptr_array_new_with_free_func(g_free);
   connector_names.connectors_having_edid = g_ptr_array_new_with_free_func(g_free);

   dir_filtered_ordered_foreach(
                 dname,
                 is_card_connector_dir,   // filter function
                 NULL,                    // ordering function
                 get_sysfs_drm_add_one_connector_name,
                 &connector_names,      // accumulator
                 0);
   g_ptr_array_sort(connector_names.all_connectors, gaux_ptr_scomp);
   g_ptr_array_sort(connector_names.connectors_having_edid, gaux_ptr_scomp);

   DBGTRC_RET_STRUCT_VALUE(debug, DDCA_TRC_NONE, Sysfs_Connector_Names,
                                  dbgrpt_sysfs_connector_names, connector_names);
   return connector_names;
 }


/** Tests if two Sysfs_Connector_Names instances have the same lists
 *  for all connectors and for connectors having a valid EDID
 *
 *  @param cn1  first  instance
 *  @param cn2  second instance
 *  @return     true if the arrays in each instance contain the same connector names
 */
bool sysfs_connector_names_equal(Sysfs_Connector_Names cn1, Sysfs_Connector_Names cn2) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");
   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cn1 = %p:", cn1);
      dbgrpt_sysfs_connector_names(cn1, 1);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cn2 = %p:", cn2);
      dbgrpt_sysfs_connector_names(cn2, 1);
   }

   bool result = gaux_unique_string_ptr_arrays_equal(cn1.all_connectors,
                                                     cn2.all_connectors);
   result &= gaux_unique_string_ptr_arrays_equal(cn1.connectors_having_edid,
                                                 cn2.connectors_having_edid);

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "");
   return result;
}


/** Emit a debugging report of a #Sysfs_Connector_Names instance.
 *
 *  @param connector_names   Sysfs_Connector_Names instance, not a pointer
 *  @param depth             logical indentation depth
 */
void dbgrpt_sysfs_connector_names(Sysfs_Connector_Names connector_names, int depth) {
   rpt_vstring(depth, "all_connectors         @%p: %s", connector_names.all_connectors,
                      join_string_g_ptr_array_t(connector_names.all_connectors,         ", ") );
   rpt_vstring(depth, "connectors_having_edid @%p: %s", connector_names.connectors_having_edid,
                      join_string_g_ptr_array_t(connector_names.connectors_having_edid, ", ") );

   #ifdef FOR_DEBUGGING
   rpt_vstring(depth, "all_connectors         @%p:", connector_names.all_connectors);
   rpt_vstring(depth+3, "%s", join_string_g_ptr_array_t(connector_names.all_connectors,         ", ") );
   rpt_vstring(depth, "connectors_having_edid         @%p:", connector_names.connectors_having_edid);
   rpt_vstring(depth+3, "%s", join_string_g_ptr_array_t(connector_names.connectors_having_edid,         ", ") );
#endif
}


void free_sysfs_connector_names_contents(Sysfs_Connector_Names names_struct) {
   if (names_struct.all_connectors) {
      g_ptr_array_free(names_struct.all_connectors, true);
      names_struct.all_connectors = NULL;
   }
   if (names_struct.connectors_having_edid) {
      g_ptr_array_free(names_struct.connectors_having_edid, true);
      names_struct.connectors_having_edid = NULL;
   }
}


Sysfs_Connector_Names copy_sysfs_connector_names_struct(Sysfs_Connector_Names original) {
   Sysfs_Connector_Names result = {NULL, NULL};
   result.all_connectors = gaux_deep_copy_string_array(original.all_connectors);
   result.connectors_having_edid = gaux_deep_copy_string_array(original.connectors_having_edid);
   return result;
}


#ifdef OUT
// Wrong!  On amdgpu, for DP device realpath is connector with EDID, for HDMI and DVI device is adapter
char * find_sysfs_drm_connector_name_by_busno(GPtrArray* connector_names, int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "connector_names=%p, busno=%d", connector_names, busno);

   char * result = NULL;
   char buf[80];
   g_snprintf(buf, 80, "/sys/bus/i2c/devices/i2c-%d/device", busno);
   DBGMSF(debug, "buf = |%s|", buf);
   char * fq_connector = realpath(buf,NULL);
   DBGMSF(debug, "fq_connector = |%s|", fq_connector);
   if (fq_connector) {
      result = g_path_get_basename(fq_connector);
      free(fq_connector);
   }

   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning %s", result);
   return result;
}
#endif


/** Searches connectors for one with matching EDID
 *
 *  @param  connector_names  array of connector names
 *  @param  edid             pointer to 128 byte EDID
 *  @return name of connector with matching EDID (caller must free)
 */
char * find_sysfs_drm_connector_name_by_edid(GPtrArray* connector_names, Byte * edid) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "edid=%p", edid);

   char * result = NULL;
   for (int ndx = 0; ndx < connector_names->len; ndx++) {
      char * connector_name = g_ptr_array_index(connector_names, ndx);
      GByteArray * sysfs_edid;
      int depth = (debug) ? 1 : -1;
      RPT_ATTR_EDID(depth, &sysfs_edid, "/sys/class/drm", connector_name, "edid");
      if (sysfs_edid) {
         if (sysfs_edid->len >= 128 && memcmp(sysfs_edid->data, edid, 128) == 0)
            result = g_strdup(connector_name);
         g_byte_array_free(sysfs_edid, true);
         if (result)
            break;
      }
   }

   DBGTRC_RETURNING(debug, DDCA_TRC_I2C, result, "");
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
#ifdef OUT
   RTTI_ADD_FUNC(find_sys_drm_connector_by_busno);
#endif
   RTTI_ADD_FUNC(find_sys_drm_connector_by_edid);
   RTTI_ADD_FUNC(get_drm_connector_name_by_busno);

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
   RTTI_ADD_FUNC(get_all_sysfs_i2c_info);
   RTTI_ADD_FUNC(get_possible_ddc_ci_bus_numbers);

   // other
   RTTI_ADD_FUNC(find_adapter);
   RTTI_ADD_FUNC(get_sys_video_devices);
   RTTI_ADD_FUNC(get_drm_connector_name_by_busno);
   RTTI_ADD_FUNC(all_sysfs_i2c_info_drm);

// RTTI_ADD_FUNC(find_sysfs_drm_connector_name_by_busno);
   RTTI_ADD_FUNC(find_sysfs_drm_connector_name_by_edid);
// RTTI_ADD_FUNC(init_sysfs_drm_connector_names);

   RTTI_ADD_FUNC(get_drm_connector_name_by_edid);
   RTTI_ADD_FUNC(get_drm_connector_name_by_busno);
}


/** Module termination.  Release resources. */
void terminate_i2c_sysfs() {
   if (all_i2c_info)
      g_ptr_array_free(all_i2c_info, true);
}

