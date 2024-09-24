/** @file i2c_sysfs_i2c_info.c */

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


#include "util/data_structures.h"
#include "util/debug_util.h"
#ifdef USE_LIBDRM
#include "util/drm_common.h"
#endif
#include "util/edid.h"
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/i2c_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_filter_functions.h"
#include "util/sysfs_i2c_util.h"
#include "util/sysfs_util.h"
#include "util/utilrpt.h"

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/i2c_bus_base.h"
#include "base/rtti.h"

#include "i2c_sysfs_base.h"

#include "i2c_sysfs_i2c_info.h"

static const DDCA_Trace_Group  TRACE_GROUP = DDCA_TRC_NONE;

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
   if (info->adapter_path) {
      rpt_vstring(d1, "adapter supports DRM:      %s",
            sbool(adapter_supports_drm_using_drm_api(info->adapter_path)));
   }
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
Bit_Set_256 get_possible_ddc_ci_bus_numbers_using_sysfs_i2c_info() {
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



void init_i2c_sysfs_i2c_info() {
   // Sysfs_I2C_Info
   RTTI_ADD_FUNC(best_driver_name_for_n_nnnn);
   RTTI_ADD_FUNC(simple_one_n_nnnn);
   RTTI_ADD_FUNC(get_i2c_info);
   RTTI_ADD_FUNC(get_single_i2c_info);
   RTTI_ADD_FUNC(get_all_sysfs_i2c_info);
   RTTI_ADD_FUNC(get_possible_ddc_ci_bus_numbers_using_sysfs_i2c_info);
}


/** Module termination.  Release resources. */
void terminate_i2c_sysfs_i2c_info() {
   if (all_i2c_info)
      g_ptr_array_free(all_i2c_info, true);
}

