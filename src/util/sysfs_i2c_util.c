/** @file sysfs_i2c_util.c
 *  i2c specific /sys functions
 */

// Copyright (C) 2018-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <errno.h>
#include <linux/limits.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "coredefs_base.h"
#include "debug_util.h"
#include "file_util.h"
#include "i2c_util.h"
#include "report_util.h"
#include "string_util.h"
#include "subprocess_util.h"
#include "sysfs_util.h"
#include "sysfs_filter_functions.h"
#include "timestamp.h"

#include "sysfs_i2c_util.h"


/** Looks in the /sys file system to check if a module is loaded.
 *  Note that only loadable kernel modules will be found. Those
 *  built into the kernel will not.
 *
 * \param  module_name    module name
 * \return true if the module is loaded, false if not
 */
bool
is_module_loaded_using_sysfs(
      const char * module_name)
{
   bool debug = false;

   struct stat statbuf;
   char   module_fn[100];

   bool   found = true;
   snprintf(module_fn, sizeof(module_fn), "/sys/module/%s", module_name);
   int rc = stat(module_fn, &statbuf);
   if (rc < 0) {
      // will be ENOENT (2) if file not found
      str_replace_char(module_fn, '-', '_');
      rc = stat(module_fn, &statbuf);
      if (rc < 0)
         found = false;
   }

   if (debug)
      printf("(%s) module_name = %s, returning %s\n", __func__, module_name, SBOOL(found));
   return found;
}


// The following functions are not really generic sysfs utilities, and more
// properly belong in a file in subdirectory base, but to avoid yet more file
// proliferation are included here.

/** Gets the sysfs name of an I2C device,
 *  i.e. the value of /sys/bus/i2c/devices/i2c-n/name
 *
 *  \param  busno   I2C bus number
 *  \return newly allocated string containing attribute value,
 *          NULL if not found
 *
 *  \remark
 *  Caller is responsible for freeing returned value
 */
char *
get_i2c_device_sysfs_name(int busno)
{
   char workbuf[50];
   snprintf(workbuf, 50, "/sys/bus/i2c/devices/i2c-%d/name", busno);
   char * name = file_get_first_line(workbuf, /*verbose */ false);
   // DBGMSG("busno=%d, returning: %s", busno, bool_repr(result));
   return name;
}


/** Given a sysfs node, walk up the chain of device directory links
 *  until an adapter node is found.
 *
 *  @param  path   e.g. /sys/bus/i2c/devices/i2c-5
 *  @param  depth  logical indentation depth
 *  @return sysfs path to adapter
 *
 *  Parameter **depth** behaves as usual for sysfs RPT_... functions.
 *  If depth >= 0, sysfs attributes are reported.
 *  If depth <  0, there is no output
 *
 *  Caller is responsible for freeing the returned value
 */
char * sysfs_find_adapter(char * path) {
   bool debug = true;
   DBGF(debug, "Starting. path=%s", path);
   int depth = (debug) ? 2 : -1;

   char * devpath = NULL;
// #ifdef OUT
   if ( RPT_ATTR_NOTE_SUBDIR(depth, NULL, path, "device") ) {
       if ( RPT_ATTR_TEXT(depth, NULL, path, "device", "class") ) {
          RPT_ATTR_REALPATH(depth, &devpath, path, "device");
       }
       else {
          char p2[PATH_MAX];
           g_snprintf(p2, PATH_MAX, "%s/device", path);
           devpath = sysfs_find_adapter(p2);
       }
   }
   else
// #endif
   {
      char * rp1 = NULL;
      char * rp2 = NULL;
      RPT_ATTR_REALPATH(depth, &rp1, path);
      if ( RPT_ATTR_TEXT(depth, NULL, rp1, "class")) {
          devpath = rp1;
      }
      else {
         RPT_ATTR_REALPATH(depth, &rp2, rp1, "..");
         free(rp1);
         DBGF(debug, "   rp2 = %s", rp2);
         if ( RPT_ATTR_TEXT(depth, NULL, rp2, "../class"))
            devpath = rp2;
         else
            free(rp2);
      }
   }

   DBGF(debug,"Done.  Returning: %s", devpath);
   return devpath;
}





/** Gets the driver name of an I2C device,
 *  i.e. the basename of /sys/bus/i2c/devices/i2c-n/device/driver/module
 *
 *  \param  busno   I2C bus number
 *  \return newly allocated string containing driver name
 *          NULL if not found
 *
 *  \remark
 *  Caller is responsible for freeing returned value
 */
char *
get_i2c_sysfs_driver_by_busno(int busno) {
   bool debug = true;
   DBGF(debug, "Starting. busno=%d", busno);
   int depth = (debug) ? 2 : -1;

   char * driver_name = NULL;
   char workbuf[100];
#ifdef FAILS_FOR_NVIDIA
   snprintf(workbuf, 100, "/sys/bus/i2c/devices/i2c-%d/device/driver/module", busno);
   DBGF(debug, "workbuf(1) = %s", workbuf);
   driver_name = get_rpath_basename(workbuf);
   if (!driver_name) {
      snprintf(workbuf, 100, "/sys/bus/i2c/devices/i2c-%d/device/device/device/driver/module", busno);
      DBGF(debug, "workbuf(2) = %s", workbuf);
      driver_name = get_rpath_basename(workbuf);
   }
#endif
   snprintf(workbuf, 100, "/sys/bus/i2c/devices/i2c-%d", busno);
   DBGF(debug, "workbuf(3) = %s", workbuf);
   char * adapter_path  = sysfs_find_adapter(workbuf);
   if (adapter_path) {
      // RPT_ATTR_TEXT(             depth, &result->adapter_class,  adapter_path, "class");
      RPT_ATTR_REALPATH_BASENAME(depth, &driver_name,         adapter_path, "driver");
      // RPT_ATTR_TEXT(             depth, &result->driver_version, adapter_path, "driver/module/version");
   }



   DBGF(debug, "Done. busno=%d, returning %s", busno, driver_name);
   return driver_name;
}


/** Gets the name of the driver for a /dev/i2c-N device,
 *  i.e. the basename of /sys/bus/i2c/devices/i2c-n/device/driver/module
 *
 *  \param  device_name   e.g. /dev/i2c-n
 *  \return newly allocated string containing driver name
 *          NULL if not found
 *
 *  \remark
 *  Caller is responsible for freeing returned value
 */
char *
get_i2c_sysfs_driver_by_device_name(char * device_name) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. device_name = %s", __func__, device_name);
   char * driver_name = NULL;
   int busno = extract_number_after_hyphen(device_name);
   if (busno >= 0) {
      driver_name = get_i2c_sysfs_driver_by_busno(busno);
   }
   if (debug)
      printf("(%s) Done. Returning: %s", __func__, driver_name);
   return driver_name;
}


/** Gets the name of the driver for a /dev/i2c-N device, specified by its file descriptor.
 *  i.e. the basename of /sys/bus/i2c/devices/i2c-n/device/driver/module
 *
 *  \param  fd   file descriptor
 *  \return newly allocated string containing driver name
 *          NULL if not found
 *
 *  \remark
 *  Caller is responsible for freeing returned value
 */
char *
get_i2c_sysfs_driver_by_fd(int fd) {
   bool debug = false;
   char * driver_name = NULL;
   int busno = extract_number_after_hyphen(filename_for_fd_t(fd));
   if (busno >= 0) {
      driver_name = get_i2c_sysfs_driver_by_busno(busno);
   }
   if (debug)
      printf("(%s) fd=%d, returning %s\n", __func__, fd, driver_name);
   return driver_name;
}


/** Gets the class of an I2C device,
 *  i.e. /sys/bus/i2c/devices/i2c-n/device/class
 *  or   /sys/bus/i2c/devices/i2c-n/device/device/device/class
 *
 *  \param  busno   I2C bus number
 *  \return device class
 *          0 if not found (should never occur)
 */
uint32_t
get_i2c_device_sysfs_class(int busno) {
   uint32_t result = 0;
   char workbuf[100];
   snprintf(workbuf, 100, "/sys/bus/i2c/devices/i2c-%d/device", busno);

   char * s_class = read_sysfs_attr(workbuf, "class", /*verbose*/ false);
   if (!s_class) {
     snprintf(workbuf, 100, "/sys/bus/i2c/devices/i2c-%d/device/device/device", busno);
     s_class = read_sysfs_attr(workbuf, "class", /*verbose*/ false);
   }
   if (s_class) {
      // printf("(%s) Found %s/class\n", __func__, workbuf);
      /* bool ok =*/  str_to_int(s_class, (int*) &result, 16);   // if fails, &result unchanged
      free(s_class);
   }
   else{
      // printf("(%s) class for bus %d not found\n", __func__, busno);
   }
   // printf("(%s) busno=%d, returning 0x%08x\n", __func__, busno, result);
   return result;
}


static bool
ignorable_i2c_device_sysfs_name(const char * name, const char * driver) {
   bool result = false;
   const char * ignorable_prefixes[] = {
         "SMBus",
         "Synopsys DesignWare",
         "soc:i2cdsi",   // Raspberry Pi
         "smu",          // Mac G5, probing causes system hang
         "mac-io",       // Mac G5
         "u4",           // Mac G5
         "AMDGPU SMU",   // AMD Navi2 variants, e.g. RX 6000 series
         NULL };
   if (name) {
      if (starts_with_any(name, ignorable_prefixes) >= 0)
         result = true;
      else if (streq(driver, "nouveau")) {
         if ( !str_starts_with(name, "nvkm-") ) {
            result = true;
            // printf("(%s) name=|%s|, driver=|%s| - Ignore\n", __func__, name, driver);
         }
      }
   }
   // printf("(%s) name=|%s|, driver=|%s|, returning: %s\n", __func__, name, driver, sbool(result));
   return result;
}


/** Checks if an I2C bus cannot be a DDC/CI connected monitor
 *  and therefore can be ignored, e.g. if it is an SMBus device.
 *
 *  \param  busno  I2C bus number
 *  \return true if ignorable, false if not
 */
bool
sysfs_is_ignorable_i2c_device(int busno) {
   bool debug = true;
   bool ignorable = false;
   DBGF(debug, "Starting.  busno=%d", busno);

   // It is possible for a display device to have an I2C bus
   // that should be ignored.  Recent AMD Navi board (e.g. RX 6000)
   // have an I2C SMU bus that will hang the card if probed.
   // So first check for specific device names to ignore.
   // If not found, then base the result on the device's class.

   char * name = get_i2c_device_sysfs_name(busno);
   char * driver = get_i2c_sysfs_driver_by_busno(busno);
   if (name) {
      ignorable = ignorable_i2c_device_sysfs_name(name, driver);
      DBGF(debug, "   busno=%d, name=|%s|, ignorable_i2c_sysfs_name() returned %s", busno, name, sbool(ignorable));
   }
   free(name);    // safe if NULL
   free(driver);  // ditto

   if (!ignorable) {
      uint32_t class = get_i2c_device_sysfs_class(busno);
      if (class) {
         DBGF(debug, "   class = 0x%08x", class);
         uint32_t cl2 = class & 0xffff0000;
         DBGF(debug, "   cl2 = 0x%08x", cl2);
         ignorable = (cl2 != 0x030000 &&
                      cl2 != 0x0a0000);    // docking station
      }
   }

   DBGF(debug, "busno=%d, returning: %s", busno, sbool(ignorable));
   return ignorable;
}


// Beginning of get_video_devices2() segment
// Use C code instead of bash command to find all subdirectories
// of /sys/devices having class x03

#ifdef UNUSED
bool not_ata(const char * simple_fn) {
   return !str_starts_with(simple_fn, "ata");
}
#endif

bool is_pci_dir(const char * simple_fn) {
   bool debug = false;
   bool result = str_starts_with(simple_fn, "pci0");
   DBGF(debug, "simple_fn = %s, returning %s", simple_fn, sbool(result));
   return result;
}


bool predicate_starts_with_0(const char * simple_fn) {
   bool debug = false;
   bool result = str_starts_with(simple_fn, "0");
   DBGF(debug, "simple_fn = %s, returning %s", simple_fn, sbool(result));
   return result;
}


void find_class_dirs(const char * dirname,
                     const char * simple_fn,
                     void *       accumulator,
                     int          depth)
{
    bool debug = false;
    DBGF(debug, "Starting. dirname=%s, simple_fn=%s, accumulator=%p, depth=%d",
          dirname, simple_fn, accumulator, depth);
    char * subdir = g_strdup_printf("%s/%s", dirname, simple_fn);
    GPtrArray* accum = accumulator;
    char * result = NULL;
    bool found = RPT_ATTR_TEXT(-1, &result, dirname, simple_fn, "class");
    if (found) {
       DBGF(debug, "subdir=%s has attribute class = %s. Adding.", subdir, result);
       free(result);
       g_ptr_array_add(accum, g_strdup(subdir));
    }
    else {
       DBGF(debug, "subdir=%s does not have attribute class", subdir);
       DBGF(debug, "Examining subdirs of %s", subdir);
       dir_foreach(subdir, predicate_starts_with_0, find_class_dirs, accumulator, depth+1);
    }
    free(subdir);
}

void find_class03_dirs(const char * dirname,
                       const char * simple_fn,
                       void *       accumulator,
                       int          depth)
{
    bool debug = false;
    DBGF(debug, "Starting. dirname=%s, simple_fn=%s, accumulator=%p, depth=%d",
          dirname, simple_fn, accumulator, depth);
    char * subdir = g_strdup_printf("%s/%s", dirname, simple_fn);
    GPtrArray* accum = accumulator;
    char * result = NULL;
    bool found = RPT_ATTR_TEXT(-1, &result, dirname, simple_fn, "class");
    if (found) {
       DBGF(debug, "subdir=%s has attribute class = %s.", subdir, result);
       if (str_starts_with(result, "0x03"))
          g_ptr_array_add(accum, g_strdup(subdir));
       free(result);
    }
    else {
       DBGF(debug, "subdir=%s does not have attribute class", subdir);
    }
    DBGF(debug, "Examining subdirs of %s", subdir);
    dir_foreach(subdir, predicate_starts_with_0, find_class03_dirs, accumulator, depth+1);

    free(subdir);
}


/** Returns the paths to all video devices in /sys/devices, i.e. those
 *  subdirectories (direct or indirect) having class = 0x03
 *
 *  @return array of directory names, caller must free
 */
GPtrArray * get_video_adapter_devices() {
   bool debug = false;
   DBGF(debug, "Starting.");

   GPtrArray * class03_dirs = g_ptr_array_new_with_free_func(g_free);
   dir_foreach("/sys/devices", is_pci_dir, find_class03_dirs, class03_dirs, 0);

   if (debug) {
      DBG("Returning %d directories:", class03_dirs->len);
      for (int ndx = 0; ndx < class03_dirs->len; ndx++)
         rpt_vstring(2, "%s", (char*) g_ptr_array_index(class03_dirs, ndx));
   }

   return class03_dirs;
}


#ifdef OLD
/** Returns the paths to all video devices in /sys/devices, i.e. those
 *  subdirectories (direct or indirect) having class = 0x03
 *
 *  @return array of directory names, caller must free
 *
 *  @remark
 *  This function exists as a shell for testing alternative algorithms
 */
GPtrArray * get_video_adapter_devices() {
   bool debug = false;
   // int64_t t0;
#ifdef SLOW_DO_NOT_USE
   t0 = cur_realtime_nanosec();
   // 41 millisec
   char * cmd = "find /sys/devices -name class | xargs grep x03 -l | sed 's|class||'";
   GPtrArray * result = execute_shell_cmd_collect(cmd);
   DBGF(debug, "find command tool %jd microsec", NANOS2MICROS( cur_realtime_nanosec() - t0));
   // g_ptr_array_set_free_func(result, g_free);  // redundant
#endif

   uint64_t t0 = cur_realtime_nanosec();
   GPtrArray* devices = NULL;
   devices = get_video_adapter_devices3();
   if (debug) {
      DBG("get_video_adapter_devices3() took %jd microsec", NANOS2MICROS( cur_realtime_nanosec() - t0));
      DBG("get_video_adapter_devices3() returned %d directories:", devices->len);
      for (int ndx = 0; ndx < devices->len; ndx++)
         rpt_vstring(2, "%s", (char*) g_ptr_array_index(devices, ndx));
   }
   g_ptr_array_free(devices, true);

   // 1 millisec
   devices = get_video_adapter_devices2();
   if (debug) {
      DBG("get_video_adapter_devices2() took %jd microsec", NANOS2MICROS( cur_realtime_nanosec() - t0));
      DBG("get_video_adapter_devices2() returned %d directories:", devices->len);
      for (int ndx = 0; ndx < devices->len; ndx++)
         rpt_vstring(2, "%s", (char*) g_ptr_array_index(devices, ndx));
   }

   DBGF(debug, "Returning %d directories:", devices->len);
#ifdef TEMP
   if (debug) {
      for (int ndx = 0; ndx < devices->len; ndx++)
         rpt_vstring(2, "%s", (char*) g_ptr_array_index(devices, ndx));
   }
#endif
   return devices;
}
#endif

