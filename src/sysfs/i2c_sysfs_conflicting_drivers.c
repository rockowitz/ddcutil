// i2c_sysfs_conflicting_drivers.c

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "i2c_sys_drm_connector.h"

#include "i2c_sysfs_conflicting_drivers.h"

static const DDCA_Trace_Group  TRACE_GROUP = DDCA_TRC_NONE;

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

void        init_i2c_sysfs_conflicting_drivers() {
   RTTI_ADD_FUNC(one_n_nnnn);
   RTTI_ADD_FUNC(collect_conflicting_drivers0);
   RTTI_ADD_FUNC(collect_conflicting_drivers);
   RTTI_ADD_FUNC(collect_conflicting_drivers_for_any_bus);
   RTTI_ADD_FUNC(conflicting_driver_names);
}

//
// End of conflicting drivers section
//
