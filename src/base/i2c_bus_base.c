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
#include "util/sysfs_util.h"

#include "core.h"
#include "rtti.h"

#include "i2c/i2c_sysfs.h"

#include "i2c_bus_base.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

/** All I2C buses.  GPtrArray of pointers to #I2C_Bus_Info - shared with i2c_bus_selector.c */
/* static */ GPtrArray * i2c_buses = NULL;


//
// Local utility functions
//

// Keep in sync with definitions in i2c_bus_base.h
Value_Name_Table i2c_bus_flags_table = {
      VN(I2C_BUS_EXISTS),
      VN(I2C_BUS_ACCESSIBLE),
      VN(I2C_BUS_ADDR_0X50),
      VN(I2C_BUS_ADDR_0X37),
      VN(I2C_BUS_ADDR_0X30),
//      VN(I2C_BUS_EDP),
//      VN(I2C_BUS_LVDS),
      VN(I2C_BUS_PROBED),
      VN(I2C_BUS_VALID_NAME_CHECKED),
      VN(I2C_BUS_HAS_VALID_NAME),
      VN(I2C_BUS_BUSY),
      VN(I2C_BUS_SYSFS_EDID),
      VN(I2C_BUS_DRM_CONNECTOR_CHECKED),
      VN(I2C_BUS_LVDS_OR_EDP),
      VN(I2C_BUS_APPARENT_LAPTOP),
      VN_END
};



/** Creates a string interpretation of I2C_Bus_Info.flags.
 *
 *  @param  flags flags value
 *  @return string interpretation, caller must free
 */
char * interpret_i2c_bus_flags(uint16_t flags) {
   return VN_INTERPRET_FLAGS(flags, i2c_bus_flags_table, " | ");
}


/** Creates a string interpretation of I2C_Bus_Info.flags.
 *
 *  @param  flags flags value
 *  @return string interpretation
 *
 *  The string is returned is valid until the next call
 *  to this function in the current thread.
 *  It must not be free'd by the caller.
 */
char * interpret_i2c_bus_flags_t(uint16_t flags) {
   return VN_INTERPRET_FLAGS_T(flags, i2c_bus_flags_table, " | ");
}


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


void i2c_free_bus_info(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo = %p", businfo);
   if (businfo)
      DBGTRC(debug, TRACE_GROUP, "marker = |%.4s|, busno = %d",  businfo->marker, businfo->busno);
   if (businfo && memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0) {   // just ignore if already freed
      if (businfo->edid) {
         free_parsed_edid(businfo->edid);
         businfo->edid = NULL;
      }
      FREE(businfo->driver);
      FREE(businfo->drm_connector_name);
      businfo->marker[3] = 'x';
      free(businfo);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void i2c_gdestroy_bus_info(void * data) {
   i2c_free_bus_info(data);
}

const char * drm_connector_found_by_name(Drm_Connector_Found_By found_by) {
   char * result = NULL;
   switch(found_by) {
   case DRM_CONNECTOR_NOT_CHECKED:    result = "DRM_CONNECTOR_NOT_CHECKED";    break;
   case DRM_CONNECTOR_NOT_FOUND:      result = "DRM_CONNECTOR_NOT_FOUND";      break;
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
 *  \param   businfo    pointer to Bus_Info structure describing bus
 *  \param   depth      logical indentation depth
 *
 *  \remark
 *  Although this is a debug type report, it is called (indirectly) by the
 *  ENVIRONMENT command.
 */
void i2c_dbgrpt_bus_info(I2C_Bus_Info * businfo, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   assert(businfo);
   rpt_structure_loc("I2C_Bus_Info", businfo, depth);
   rpt_vstring(depth, "Flags:                   %s", interpret_i2c_bus_flags_t(businfo->flags));
   rpt_vstring(depth, "Bus /dev/i2c-%d found:   %s", businfo->busno, sbool(businfo->flags&I2C_BUS_EXISTS));

   rpt_vstring(depth, "Bus /dev/i2c-%d probed:  %s", businfo->busno, sbool(businfo->flags&I2C_BUS_PROBED ));
   if ( businfo->flags & I2C_BUS_PROBED ) {
#ifdef OUT
      rpt_vstring(depth, "Driver:                  %s", businfo->driver);
      rpt_vstring(depth, "Bus accessible:          %s", sbool(businfo->flags&I2C_BUS_ACCESSIBLE ));
      rpt_vstring(depth, "Bus is eDP:              %s", sbool(businfo->flags&I2C_BUS_EDP ));
      rpt_vstring(depth, "Bus is LVDS:             %s", sbool(businfo->flags&I2C_BUS_LVDS));
      rpt_vstring(depth, "Valid bus name checked:  %s", sbool(businfo->flags & I2C_BUS_VALID_NAME_CHECKED));
      rpt_vstring(depth, "I2C bus has valid name:  %s", sbool(businfo->flags & I2C_BUS_HAS_VALID_NAME));
#ifdef DETECT_SLAVE_ADDRS
      rpt_vstring(depth, "Address 0x30 present:    %s", sbool(businfo->flags & I2C_BUS_ADDR_0X30));
#endif
      rpt_vstring(depth, "Address 0x37 present:    %s", sbool(businfo->flags & I2C_BUS_ADDR_0X37));
      rpt_vstring(depth, "Address 0x50 present:    %s", sbool(businfo->flags & I2C_BUS_ADDR_0X50));
      rpt_vstring(depth, "Device busy:             %s", sbool(businfo->flags & I2C_BUS_BUSY));
#endif
      rpt_vstring(depth, "errno for open:          %d", businfo->open_errno);

      rpt_vstring(depth, "Connector name checked:  %s", sbool(businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED));
      if (businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED) {
         rpt_vstring(depth, "drm_connector_found_by:  %s (%d)",
            drm_connector_found_by_name(businfo->drm_connector_found_by), businfo->drm_connector_found_by);
         rpt_vstring(depth, "drm_connector_name:      %s", businfo->drm_connector_name);
      }
      // not useful and clutters the output
      // i2c_report_functionality_flags(businfo->functionality, /* maxline */ 90, depth);
      if ( businfo->flags & I2C_BUS_ADDR_0X50) {
         if (businfo->edid) {
            report_parsed_edid(businfo->edid, true /* verbose */, depth);
         }
      }
   }

#ifndef TARGET_BSD
   I2C_Sys_Info * info = get_i2c_sys_info(businfo->busno, -1);
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
   I2C_Bus_Info * businfo = NULL;
   if (busndx < i2c_buses->len) {
      businfo = g_ptr_array_index(i2c_buses, busndx);
      // report_businfo(busInfo);
      if (debug) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, flags = 0x%04x = %s",
            businfo->busno, businfo->flags, interpret_i2c_bus_flags_t(businfo->flags));
      }
      assert( businfo->flags & I2C_BUS_PROBED );
   }

   DBGMSF(debug, "Done.  busndx=%d, Returning %p, busno=%d",
                         busndx, businfo,  (businfo) ? businfo->busno : -1) ;
   return businfo;
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


/** Retrieves the value of a text attribute (e.g. enabled) in the SYSFS
 *  DRM connector directory for an I2C bus.
 *
 *  @param businfo
 *  @param attribute  attribute name
 *  @return attribute value, or NULL if not a DRM display
 *
 *  Caller is responsible for freeing the returned value
 */
char * i2c_get_drm_connector_attribute(const I2C_Bus_Info * businfo, const char * attribute) {
   assert(businfo);
   assert(businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED);
   assert(businfo->drm_connector_found_by != DRM_CONNECTOR_NOT_CHECKED);
   char * result = NULL;
   if (businfo->drm_connector_found_by != DRM_CONNECTOR_NOT_FOUND) {
      assert(businfo->drm_connector_name);
      RPT_ATTR_TEXT(-1, &result, "/sys/class/drm", businfo->drm_connector_name, attribute);
   }
   return result;
}


#ifdef UNUSED
const char * i2c_get_drm_connected(const I2C_Bus_Info * businfo) {
   return i2c_get_drm_connector_attribute(businfo, "connected");
}
#endif


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


/** Module initialization. */
void init_i2c_bus_base() {
   RTTI_ADD_FUNC(i2c_dbgrpt_buses);
   RTTI_ADD_FUNC(i2c_new_bus_info);
   RTTI_ADD_FUNC(i2c_free_bus_info);
}

