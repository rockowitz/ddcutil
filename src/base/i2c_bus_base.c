/** @file i2c_bus_base.c
 */

// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <assert.h>
#include <glib-2.0/glib.h>

#include "util/coredefs.h"
#include "util/data_structures.h"
#include "util/edid.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "public/ddcutil_types.h"

#include "core.h"
#include "i2c_bus_aux.h"
#include "rtti.h"

#include "i2c_bus_base.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

GPtrArray * all_i2c_buses = NULL;  ///  array of  #I2C_Bus_Info
GPtrArray * removed_i2c_buses = NULL;
static GMutex all_i2c_buses_mutex;

//
// Generalized Bus_Info retrieval
//

I2C_Bus_Info * i2c_find_bus_info_in_gptrarray_by_busno(GPtrArray * buses, int busno) {
   bool debug = false;
   DBGMSF(debug, "Starting. buses=%p, busno=%d", buses, busno);

   I2C_Bus_Info * result = NULL;
   for (int ndx = 0; ndx < buses->len; ndx++) {
      I2C_Bus_Info * cur_info = g_ptr_array_index(all_i2c_buses, ndx);
      if (cur_info->busno == busno) {
         result = cur_info;
         break;
      }
   }

   DBGMSF(debug, "Done.     Returning: %p", result);
   return result;
}


int i2c_find_bus_info_index_in_gptrarray_by_busno(GPtrArray * buses, int busno) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno);

    int result = -1;
    if (buses) {
       for (int ndx = 0; ndx < buses->len; ndx++) {
          I2C_Bus_Info * cur_info = g_ptr_array_index(all_i2c_buses, ndx);
          if (cur_info->busno == busno) {
             result = ndx;
             break;
          }
       }
    }

   DBGMSF(debug, "Done.     Returning: %d", result);
   return result;
}


int i2c_find_bus_info_index_in_gptrarray_by_businfo(GPtrArray * buses, I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGMSF(debug, "Starting. businfo=%p", businfo);

    int result = -1;
    if (buses) {
       for (int ndx = 0; ndx < buses->len; ndx++) {
          I2C_Bus_Info * cur_info = g_ptr_array_index(all_i2c_buses, ndx);
          if (cur_info == businfo) {
             result = ndx;
             break;
          }
       }
    }

   DBGMSF(debug, "Done.     Returning: %d", result);
   return result;
}


//
// Bus Info retrieval
//

/** Retrieves bus information by its index in the i2c_buses array
 *
 *  @param   busndx
 *
 *  @return  pointer to Bus_Info struct for the bus,\n
 *           NULL if invalid index
 */
I2C_Bus_Info * i2c_get_bus_info_by_index(guint busndx) {
   bool debug = false;
   DBGMSF(debug, "busndx=%d", busndx);
   assert(all_i2c_buses);

   I2C_Bus_Info * businfo = NULL;
   if (busndx < all_i2c_buses->len) {
      businfo = g_ptr_array_index(all_i2c_buses, busndx);
      DBGMSF(debug, "busno=%d, flags = 0x%04x = %s",
            businfo->busno, businfo->flags, i2c_interpret_bus_flags_t(businfo->flags) );
   }

   DBGMSF(debug, "Done.  Returning businfo-%p. busndx=%d, busno=%d",
                 businfo,  busndx, (businfo) ? businfo->busno : -1 );
   return businfo;
}


/** Retrieves bus information by I2C bus number.
 *
 * @param   busno    bus number
 *
 * @return  pointer to Bus_Info struct for the bus,\n
 *          NULL if invalid bus number
 */
I2C_Bus_Info * i2c_find_bus_info_by_busno(int busno) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno);

   I2C_Bus_Info * businfo =
       i2c_find_bus_info_in_gptrarray_by_busno(all_i2c_buses, busno);

   DBGMSF(debug, "Done.     Returning: %p", businfo);
   return businfo;
}


int i2c_find_bus_info_index_by_busno(int busno) {

   int result = i2c_find_bus_info_index_in_gptrarray_by_busno(all_i2c_buses, busno);

   return result;
}


int  i2c_find_bus_info_index_by_businfo(I2C_Bus_Info * businfo) {

   int result = i2c_find_bus_info_index_in_gptrarray_by_businfo(all_i2c_buses, businfo);
   return result;
}


/** Retrieves bus information by DRM connector id
 *
 *  @param   drm_connector_id
 *
 *  @return  pointer to I2C_Bus_Info struct for the bus,\n
 *           NULL if not found
 */
I2C_Bus_Info * i2c_find_businfo_by_drm_connector_id(int drm_connector_id) {
   bool debug = false;
   DBGMSF(debug, "Starting. drm_connector_id=%d", drm_connector_id);

   I2C_Bus_Info * result = NULL;
   if (all_i2c_buses) {
      for (int ndx = 0; ndx < all_i2c_buses->len; ndx++) {
         I2C_Bus_Info * businfo = g_ptr_array_index(all_i2c_buses, ndx);
         if (businfo->drm_connector_id == drm_connector_id) {
            result = businfo;
            break;
         }
      }
   }

   DBGMSF(debug, "Done.     Returning: %p", result);
   return result;
}


//
// Lifecycle
//

#ifdef UNUSED
void i2c_add_bus_info(I2C_Bus_Info * businfo) {
   assert(businfo);
   bool debug  = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Adding businfo record for bus %d to all_i2c_buses", businfo->busno);
   assert(businfo->busno != 255 && businfo->busno != -1);

   g_mutex_lock(&all_i2c_buses_mutex);
   g_ptr_array_add(all_i2c_buses, businfo);
   g_mutex_unlock(&all_i2c_buses_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


I2C_Bus_Info * i2c_add_bus(int busno) {
   I2C_Bus_Info *  businfo = i2c_new_bus_info(busno);
   businfo->flags = I2C_BUS_EXISTS;
   i2c_add_bus_info(businfo);
   return businfo;
}
#endif


/** Gets the I2C_Bus_Info struct for the specified bus.
 *  If the struct does not already exist, and the bus exists,
 *  a new struct is allocated.
 *
 *  @param busno    I2C bus number
 *  @param new_info location at which to return a flag
 *                  indicating whether the struct returned
 *                  is to a newly allocated instance
 *  @return pointer to struct for the specified bus
 *                  NULL if no existing struct found and the
 *                  bus does not exist
 */
I2C_Bus_Info * i2c_get_bus_info(int busno, bool* new_info) {
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d", busno);
   *new_info = false;
   g_mutex_lock(&all_i2c_buses_mutex);
   I2C_Bus_Info * businfo =  i2c_find_bus_info_in_gptrarray_by_busno(all_i2c_buses, busno);
   if (!businfo) {
      if (i2c_device_exists(busno)) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding /dev/"I2C"-%d to set of buses", busno);
         businfo = i2c_new_bus_info(busno);
         businfo->flags = I2C_BUS_EXISTS;
         g_ptr_array_add(all_i2c_buses, businfo);
         *new_info = true;
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "/dev/"I2C"-%d does not exist, not adding to set of buses", busno);
      }
   }

   g_mutex_unlock(&all_i2c_buses_mutex);
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning businfo=%p for busno%d, *new_info=%s",
         businfo, busno, SBOOL(*new_info));
   return businfo;
}


#ifdef UNUSED
/** Updates an existing #I2C_Bus_Info struct with recent
 *  data from a source sruct.  Modifies those fields which
 *  can change.
 *
 *  @param old
 *  @param new
 */
void  i2c_update_bus_info(I2C_Bus_Info * existing, I2C_Bus_Info* new) {
   bool debug = false;
   assert(existing);
   assert(new);
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d, existing=%p, new=%p",
         existing->busno, existing, new);
   if ( IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Initial bus info:");
      i2c_dbgrpt_bus_info(existing, true, 4);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "new bus info:");
      i2c_dbgrpt_bus_info(new, true, 4);
   }

   if (existing->edid) {
      free_parsed_edid(existing->edid);
   }
   if (new->edid)
      existing->edid = copy_parsed_edid(new->edid);
   else
      existing->edid = NULL;

#define COPY_BIT(_old, _new, _bit) \
   if (_new->flags & _bit) \
      _old->flags |= _bit; \
   else                    \
      _old->flags &= ~_bit;

   COPY_BIT(existing, new, I2C_BUS_ADDR_X37);
   COPY_BIT(existing, new, I2C_BUS_ADDR_X30);
   COPY_BIT(existing, new, I2C_BUS_PROBED);
   COPY_BIT(existing, new, I2C_BUS_SYSFS_EDID);
   COPY_BIT(existing, new, I2C_BUS_X50_EDID);
// COPY_BIT(existing, new, I2C_BUS_DRM_CONNECTOR_CHECKED);
#undef COPY_BIT

   if (existing->drm_connector_name) {
      free(existing->drm_connector_name);
      existing->drm_connector_name = NULL;
      existing->drm_connector_id = -1;
   }
   existing->drm_connector_found_by = new->drm_connector_found_by;
   if (new->drm_connector_name)
      existing->drm_connector_name = g_strdup_printf("%s", new->drm_connector_name);
   existing->drm_connector_id = new->drm_connector_id;


   existing->last_checked_dpms_asleep = new->last_checked_dpms_asleep;

   if ( IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Updated bus info:");
      i2c_dbgrpt_bus_info(existing, true, 4);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}
#endif


#ifdef UNUSED
void i2c_remove_bus_by_businfo(I2C_Bus_Info * businfo) {
   assert(businfo);
   bool debug  = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Removing businfo record for bus %d from all_i2c_buses", businfo->busno);
   assert(businfo->busno != 255 && businfo->busno != -1);

   g_mutex_lock(&all_i2c_buses_mutex);
   g_ptr_array_remove(all_i2c_buses, businfo);
   g_mutex_unlock(&all_i2c_buses_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}
#endif


void i2c_remove_bus_by_businfo(I2C_Bus_Info * businfo) {
   assert(businfo);
   bool debug  = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo=%p, busno = %d, removed=%s, flags=%s",
         businfo, businfo->busno, sbool(businfo->removed), i2c_interpret_bus_flags_t(businfo->flags));

   int all_index     = i2c_find_bus_info_index_in_gptrarray_by_businfo(all_i2c_buses, businfo);
   int removed_index = i2c_find_bus_info_index_in_gptrarray_by_businfo(removed_i2c_buses, businfo);

   bool perform_remove = false;
   if (all_index >= 0 && removed_index < 0) {
      // normal case
      perform_remove = true;
   }
   else if (all_index < 0 && removed_index >= 0) {
      // already removed
      if (businfo->removed) {
         DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "already removed");
      }
      else {
         DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "already removed, setting removed");
         businfo->removed = true;
      }
   }
   else if (all_index < 0 && removed_index < 0) {
      // lost businfo
      // how to handle?
      assert(false); // temp
   }
   else { // all_index >= 0 && removed_index >= 0) {
      // how to handle?
      assert(false); // temp
   }

   if (perform_remove) {
      int busndx = i2c_find_bus_info_index_by_businfo(businfo);
      businfo->removed = true;
      if (!removed_i2c_buses)
         removed_i2c_buses = g_ptr_array_new_with_free_func((GDestroyNotify) i2c_free_bus_info);
      g_ptr_array_add(removed_i2c_buses, businfo);
      g_ptr_array_remove_index(all_i2c_buses, busndx);
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void i2c_remove_bus_by_busno(int busno) {
   bool debug  = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d", busno);
   assert(busno);
   g_mutex_lock(&all_i2c_buses_mutex);
   int  busNdx = i2c_find_bus_info_index_by_busno(busno);
   if (busNdx < 0) {
      MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "Record for busno %d not found in all_i2c_buses array", busno);
   }
   else {
      I2C_Bus_Info * businfo = g_ptr_array_index(all_i2c_buses, busNdx);
      i2c_remove_bus_by_businfo(businfo);
   }
   g_mutex_unlock(&all_i2c_buses_mutex);
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


// called if display removed, bus may or may not still exist
void i2c_reset_bus_info(I2C_Bus_Info * businfo) {
   bool debug  = false;
   assert(businfo);
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo=%p, busno = %d, removed=%s, flags=%s",
         businfo, businfo->busno, sbool(businfo->removed), i2c_interpret_bus_flags_t(businfo->flags));

   if (!businfo->removed) {   // if bus not (yet) marked removed
      if (i2c_device_exists(businfo->busno)) {   // does it still exist?
         //businfo->flags &= ~(I2C_BUS_ACCESSIBLE | I2C_BUS_ADDR_X30 | I2C_BUS_ADDR_X37 |
         //       I2C_BUS_SYSFS_EDID | I2C_BUS_X50_EDID );
         businfo->flags = 0;
      }
      if (businfo->edid) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,  "Calling free_parsed_edid for %p, marker=%s",
               businfo->edid, hexstring_t((Byte*) businfo->marker, 4));
         DECORATED_SYSLOG(DDCA_SYSLOG_DEBUG, "Calling free_parsed_edid for %p, marker=%s",
               businfo->edid, hexstring_t((Byte*) businfo->marker,4));
         free_parsed_edid(businfo->edid);
         businfo->edid = NULL;
      }
      // free(businfo->drm_connector_name); // double free
      if ( IS_DBGTRC(debug, TRACE_GROUP) ) {
         DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Final businfo:");
         i2c_dbgrpt_bus_info(businfo, true, 2);
      }

   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}



//
// Reset arrays
//

void i2c_discard_buses0(GPtrArray* buses) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "buses=%p", buses);

   if (buses) {
      g_ptr_array_set_free_func(buses, (GDestroyNotify) i2c_free_bus_info);
      g_ptr_array_free(buses, true);
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


/** Discard all known buses */
void i2c_discard_buses() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   g_mutex_lock(&all_i2c_buses_mutex);
   if (all_i2c_buses) {
      i2c_discard_buses0(all_i2c_buses);
      all_i2c_buses= NULL;
   }
   g_mutex_unlock(&all_i2c_buses_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


//
// Debug arrays
//

/** Reports I2C buses.
 *
 * @param report_all    if false, only reports buses with monitors,\n
 *                      if true, reports all detected buses
 * @param include_sysfs_info include sysfs information in report
 * @param depth         logical indentation depth
 *
 * @return count of reported buses
 *
 * @remark
 * Used by query-sysenv.c, always OL_VERBOSE
 */
int i2c_dbgrpt_buses(bool report_all, bool include_sysfs_info, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "report_all=%s", sbool(report_all));

   assert(all_i2c_buses);
   int busct = all_i2c_buses->len;
   int reported_ct = 0;

   puts("");
   if (report_all)
      rpt_vstring(depth,"Detected %d non-ignorable I2C buses:", busct);
   else
      rpt_vstring(depth, "I2C buses with monitors detected:");

   for (int ndx = 0; ndx < busct; ndx++) {
      I2C_Bus_Info * busInfo = g_ptr_array_index(all_i2c_buses, ndx);
      if ( (busInfo->edid) || report_all) {
         rpt_nl();
         i2c_dbgrpt_bus_info(busInfo, include_sysfs_info, depth);
         reported_ct++;
      }
   }
   if (reported_ct == 0)
      rpt_vstring(depth, "   No buses\n");

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %d", reported_ct);
   return reported_ct;
}


void i2c_dbgrpt_buses_summary(int depth) {
   Bit_Set_256 buses_all    = EMPTY_BIT_SET_256;
   Bit_Set_256 buses_w_edid = EMPTY_BIT_SET_256;
   Bit_Set_256 buses_x37    = EMPTY_BIT_SET_256;

   assert(all_i2c_buses);
   int busct = all_i2c_buses->len;
   for (int ndx = 0; ndx < busct; ndx++) {
      I2C_Bus_Info * businfo = g_ptr_array_index(all_i2c_buses, ndx);
      int busno = businfo->busno;
      buses_all = bs256_insert(buses_all, busno);
      if ( (businfo->edid) ) {
         buses_w_edid = bs256_insert(buses_w_edid, busno);
         if ( (businfo->flags & I2C_BUS_ADDR_X37) ) {
            buses_x37 = bs256_insert(buses_x37, busno);
         }
      }
   }

   rpt_vstring(depth, "Number of buses:       %d", bs256_count(buses_all));
   rpt_vstring(depth, "All I2C buses:         %s", bs256_to_string_decimal_t(buses_all, "", " "));
   rpt_vstring(depth, "Buses with edid:       %s", bs256_to_string_decimal_t(buses_w_edid, "", " "));
   rpt_vstring(depth, "Buses with x37 active: %s", bs256_to_string_decimal_t(buses_x37, "", " "));
}


//
// Initialization and termination
//

/** Module initialization. */
void init_i2c_bus_base() {
   RTTI_ADD_FUNC(i2c_new_bus_info);
   RTTI_ADD_FUNC(i2c_free_bus_info);
   RTTI_ADD_FUNC(i2c_get_bus_info);
   RTTI_ADD_FUNC(i2c_remove_bus_by_busno);
   RTTI_ADD_FUNC(i2c_reset_bus_info);
   RTTI_ADD_FUNC(i2c_discard_buses0);
   RTTI_ADD_FUNC(i2c_discard_buses);
   RTTI_ADD_FUNC(i2c_dbgrpt_buses);
}


/** Module termination **/
void terminate_i2c_bus_base() {
   if (all_i2c_buses) {
      // i2c_free_bus_info() already set as free function
      g_ptr_array_free(all_i2c_buses, true);
      free (all_i2c_buses);
   }
}
