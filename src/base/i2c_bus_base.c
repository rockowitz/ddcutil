/** @file i2c_bus_base.c
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <sys/types.h>
#include <string.h>

#include "config.h"

#include "util/coredefs.h"
#include "util/edid.h"

#include "util/glib_string_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "core.h"
#include "rtti.h"

#include "i2c/i2c_sysfs.h"    // temp

#include "i2c_bus_base.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

/** All I2C buses.  GPtrArray of pointers to #I2C_Bus_Info - shared with i2c_bus_selector.c */
/* static */ GPtrArray * i2c_buses = NULL;


//
// Local utility functions
//

/** Creates a string interpretation of I2C_Bus_Info.flags.
 *
 *  @param  flags flags value
 *  @return string interpretation, caller must free
 *
 *  @remark
 *  Keep the names in sync with flag definitions
 */
char * interpret_i2c_bus_flags(uint16_t flags) {
   GPtrArray * names = g_ptr_array_new();

#define ADD_NAME(_name) \
   if (_name & flags) g_ptr_array_add(names, #_name)

   ADD_NAME(I2C_BUS_EXISTS             );
   ADD_NAME(I2C_BUS_ACCESSIBLE         );
   ADD_NAME(I2C_BUS_ADDR_0X50          );
   ADD_NAME(I2C_BUS_ADDR_0X37          );
   ADD_NAME(I2C_BUS_ADDR_0X30          );
   ADD_NAME(I2C_BUS_EDP                );
   ADD_NAME(I2C_BUS_LVDS               );
   ADD_NAME(I2C_BUS_PROBED             );
   ADD_NAME(I2C_BUS_VALID_NAME_CHECKED );
   ADD_NAME(I2C_BUS_HAS_VALID_NAME     );
   ADD_NAME(I2C_BUS_BUSY               );
   ADD_NAME(I2C_BUS_SYSFS_EDID         );
   ADD_NAME(I2C_BUS_DRM_CONNECTOR_CHECKED);

#undef ADD_NAME

   char * joined =  join_string_g_ptr_array(names, " | ");
   return joined;
}


#ifdef NOT_WORTH_THE_SPACE
char * interpret_i2c_bus_flags_t(uint16_t flags) {
   static GPrivate  buffer_key = G_PRIVATE_INIT(g_free);
   static GPrivate  buffer_len_key = G_PRIVATE_INIT(g_free);

   char * sflags = interpret_i2c_bus_flags(flags);
   int required_size = strlen(sflags) + 1;
   char * buf = get_thread_dynamic_buffer(&buffer_key, &buffer_len_key, required_size);
   strncpy(buf, sflags, required_size);
   free(sflags);
   return buf;
}
#endif


/** Allocates and initializes a new #I2C_Bus_Info struct
 *
 * @param busno I2C bus number
 * @return newly allocated #I2C_Bus_Info
 */
I2C_Bus_Info * i2c_new_bus_info(int busno) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "busno=%d", busno);
   I2C_Bus_Info * businfo = calloc(1, sizeof(I2C_Bus_Info));
   memcpy(businfo->marker, I2C_BUS_INFO_MARKER, 4);
   businfo->busno = busno;
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %p", businfo);
   return businfo;
}


void i2c_free_bus_info(I2C_Bus_Info * bus_info) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "bus_info = %p", bus_info);
   if (bus_info) {
      if (memcmp(bus_info->marker, "BINx", 4) != 0) {   // just ignore if already freed
         assert( memcmp(bus_info->marker, I2C_BUS_INFO_MARKER, 4) == 0);
         if (bus_info->edid)
            free_parsed_edid(bus_info->edid);
         if (bus_info->driver)
            free(bus_info->driver);
         if (bus_info->drm_connector_name)
            free(bus_info->drm_connector_name);
         bus_info->marker[3] = 'x';
         free(bus_info);
      }
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


const char * drm_connector_found_by_name(Drm_Connector_Found_By found_by) {
   char * result = NULL;
   switch(found_by) {
   case DRM_CONNECTOR_NOT_FOUND:      result = "DRM_CONNECTOR_NOT_FOUND";     break;
   case DRM_CONNECTOR_FOUND_BY_BUSNO: result = "DRM_CONNECTOR_FOUND_BY_BUSNO"; break;
   case DRM_CONNECTOR_FOUND_BY_EDID:  result = "DRM_CONNECTOR_FOUND_BY_EDID";  break;
   }
   return result;
}


//
// Bus Reports
//

/** Reports on a single I2C bus.
 *
 *  \param   bus_info    pointer to Bus_Info structure describing bus
 *  \param   depth       logical indentation depth
 *
 *  \remark
 *  Although this is a debug type report, it is called (indirectly) by the
 *  ENVIRONMENT command.
 */
void i2c_dbgrpt_bus_info(I2C_Bus_Info * bus_info, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   assert(bus_info);
   rpt_structure_loc("I2C_Bus_Info", bus_info, depth);
   char * s = interpret_i2c_bus_flags(bus_info->flags);
   rpt_vstring(depth, "Flags:                   %s", s);
   free(s);
   rpt_vstring(depth, "Bus /dev/i2c-%d found:   %s", bus_info->busno, sbool(bus_info->flags&I2C_BUS_EXISTS));
   rpt_vstring(depth, "Bus /dev/i2c-%d probed:  %s", bus_info->busno, sbool(bus_info->flags&I2C_BUS_PROBED ));
   if ( bus_info->flags & I2C_BUS_PROBED ) {
      rpt_vstring(depth, "Driver:                  %s", bus_info->driver);
      rpt_vstring(depth, "Bus accessible:          %s", sbool(bus_info->flags&I2C_BUS_ACCESSIBLE ));
      rpt_vstring(depth, "Bus is eDP:              %s", sbool(bus_info->flags&I2C_BUS_EDP ));
      rpt_vstring(depth, "Bus is LVDS:             %s", sbool(bus_info->flags&I2C_BUS_LVDS));
      rpt_vstring(depth, "Valid bus name checked:  %s", sbool(bus_info->flags & I2C_BUS_VALID_NAME_CHECKED));
      rpt_vstring(depth, "I2C bus has valid name:  %s", sbool(bus_info->flags & I2C_BUS_HAS_VALID_NAME));
#ifdef DETECT_SLAVE_ADDRS
      rpt_vstring(depth, "Address 0x30 present:    %s", sbool(bus_info->flags & I2C_BUS_ADDR_0X30));
#endif
      rpt_vstring(depth, "Address 0x37 present:    %s", sbool(bus_info->flags & I2C_BUS_ADDR_0X37));
      rpt_vstring(depth, "Address 0x50 present:    %s", sbool(bus_info->flags & I2C_BUS_ADDR_0X50));
      rpt_vstring(depth, "Device busy:             %s", sbool(bus_info->flags & I2C_BUS_BUSY));
      rpt_vstring(depth, "errno for open:          %d", bus_info->open_errno);
      rpt_vstring(depth, "Connector name checked:  %s", sbool(bus_info->flags & I2C_BUS_DRM_CONNECTOR_CHECKED));

      if (bus_info->flags & I2C_BUS_DRM_CONNECTOR_CHECKED) {
         rpt_vstring(depth, "drm_connector_found_by:  %s (%d)",
            drm_connector_found_by_name(bus_info->drm_connector_found_by), bus_info->drm_connector_found_by);
         rpt_vstring(depth, "drm_connector_name:      %s", bus_info->drm_connector_name);
      }
      // not useful and clutters the output
      // i2c_report_functionality_flags(bus_info->functionality, /* maxline */ 90, depth);
      if ( bus_info->flags & I2C_BUS_ADDR_0X50) {
         if (bus_info->edid) {
            report_parsed_edid(bus_info->edid, true /* verbose */, depth);
         }
      }
   }

#ifndef TARGET_BSD
   I2C_Sys_Info * info = get_i2c_sys_info(bus_info->busno, -1);
   dbgrpt_i2c_sys_info(info, depth);
   free_i2c_sys_info(info);
#endif

   DBGMSF(debug, "Done");
}


//
// Bus_Info retrieval
//


GPtrArray * i2c_get_all_buses() {
   return i2c_buses;
}

// Simple Bus_Info retrieval

/** Retrieves bus information by its index in the i2c_buses array
 *
 * @param   busndx
 *
 * @return  pointer to Bus_Info struct for the bus,\n
 *          NULL if invalid index
 */
I2C_Bus_Info * i2c_get_bus_info_by_index(guint busndx) {
   bool debug = false;
   DBGMSF(debug, "Starting.  busndx=%d", busndx );
   assert(i2c_buses);
   I2C_Bus_Info * bus_info = NULL;
   if (busndx < i2c_buses->len) {
      bus_info = g_ptr_array_index(i2c_buses, busndx);
      // report_businfo(busInfo);
      if (debug) {
         char * s = interpret_i2c_bus_flags(bus_info->flags);
         DBGMSG("busno=%d, flags = 0x%04x = %s", bus_info->busno, bus_info->flags, s);
         free(s);
      }
      assert( bus_info->flags & I2C_BUS_PROBED );
   }

   DBGMSF(debug, "Done.  busndx=%d, Returning %p, busno=%d",
                         busndx, bus_info,  (bus_info) ? bus_info->busno : -1) ;
   return bus_info;
}


/** Retrieves bus information by I2C bus number.
 *
 * If the bus information does not already exist in the #I2C_Bus_Info struct for the
 * bus, it is calculated by calling check_i2c_bus()
 *
 * @param   busno    bus number
 *
 * @return  pointer to Bus_Info struct for the bus,\n
 *          NULL if invalid bus number
 */
I2C_Bus_Info * i2c_find_bus_info_by_busno(int busno) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno);

   I2C_Bus_Info * result = NULL;
   for (int ndx = 0; ndx < i2c_buses->len; ndx++) {
      I2C_Bus_Info * cur_info = g_ptr_array_index(i2c_buses, ndx);
      if (cur_info->busno == busno) {
         result = cur_info;
         break;
      }
   }

   DBGMSF(debug, "Done.     Returning: %p", result);
   return result;
}



/** Reports I2C buses.
 *
 * @param report_all    if false, only reports buses with monitors,\n
 *                      if true, reports all detected buses
 * @param depth         logical indentation depth
 *
 * @return count of reported buses
 *
 * @remark
 * Used by query-sysenv.c, always OL_VERBOSE
 */
int i2c_dbgrpt_buses(bool report_all, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "report_all=%s", sbool(report_all));

   assert(i2c_buses);
   int busct = i2c_buses->len;
   int reported_ct = 0;

   puts("");
   if (report_all)
      rpt_vstring(depth,"Detected %d non-ignorable I2C buses:", busct);
   else
      rpt_vstring(depth, "I2C buses with monitors detected at address 0x50:");

   for (int ndx = 0; ndx < busct; ndx++) {
      I2C_Bus_Info * busInfo = g_ptr_array_index(i2c_buses, ndx);
      if ( (busInfo->flags & I2C_BUS_ADDR_0X50) || report_all) {
         rpt_nl();
         i2c_dbgrpt_bus_info(busInfo, depth);
         reported_ct++;
      }
   }
   if (reported_ct == 0)
      rpt_vstring(depth, "   No buses\n");

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %d", reported_ct);
   return reported_ct;
}


static void init_i2c_bus_base_func_name_table() {
   RTTI_ADD_FUNC(i2c_dbgrpt_buses);
   RTTI_ADD_FUNC(i2c_new_bus_info);
   RTTI_ADD_FUNC(i2c_free_bus_info);
}


void init_i2c_bus_base() {
   init_i2c_bus_base_func_name_table();
}

