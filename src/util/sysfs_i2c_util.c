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

