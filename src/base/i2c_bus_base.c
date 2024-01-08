/** @file i2c_bus_base.c
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <sys/stat.h>
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
char * i2c_interpret_bus_flags(uint16_t flags) {
   return VN_INTERPRET_FLAGS(flags, i2c_bus_flags_table, " | ");
}


/** Creates a string interpretation of I2C_Bus_Info.flags.
 *
 *  @param  flags flags value
 *  @return string interpretation
 *
 *  The string returned is valid until the next call to this function in
 *  the current thread.  It must not be free'd by the caller.
 */
char * i2c_interpret_bus_flags_t(uint16_t flags) {
   return VN_INTERPRET_FLAGS_T(flags, i2c_bus_flags_table, " | ");
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


// called if display removed, bus may or may not still exist
void i2c_reset_bus_info(I2C_Bus_Info * bus_info) {
   bool debug = false;
   assert(bus_info);
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo=%p, busno = %d", bus_info, bus_info->busno);
   bus_info->flags = I2C_BUS_VALID_NAME_CHECKED | I2C_BUS_HAS_VALID_NAME;
   if (i2c_device_exists(bus_info->busno))
      bus_info->flags |= I2C_BUS_EXISTS;
   if (bus_info->edid) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,  "Calling free_parsed_edid for %p, marker=%s",
            bus_info->edid, hexstring_t((Byte*) bus_info->marker, 4));
      SYSLOG2(DDCA_SYSLOG_DEBUG, "Calling free_parsed_edid for %p, marker=%s",
            bus_info->edid, hexstring_t((Byte*) bus_info->marker,4));
      free_parsed_edid(bus_info->edid);
      bus_info->edid = NULL;
   }
   if ( IS_DBGTRC(debug, TRACE_GROUP) ) {
      DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Final bus_info:");
      i2c_dbgrpt_bus_info(bus_info, 2);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


char * i2c_get_drm_connector_name(I2C_Bus_Info * businfo) {
   bool debug = false;
   char * result = NULL;
   DBGTRC_STARTING(debug, TRACE_GROUP,
         "busno=%d, drm_connector_found_by=%s drm_connector_name=|%s|",
         businfo->busno, drm_connector_found_by_name(businfo->drm_connector_found_by),
         businfo->drm_connector_name);
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "flags: %s", i2c_interpret_bus_flags_t(businfo->flags) );

   if (!(businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED) ) {    // ??? when can this be false? ???
      result = businfo->drm_connector_name;
   }

   DBGTRC_RETURNING(debug, TRACE_GROUP, result, "");
   return result;
}



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
   rpt_vstring(depth, "Flags:                   %s", i2c_interpret_bus_flags_t(businfo->flags));
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
      rpt_vstring(depth, "errno for open:          %s", psc_desc(businfo->open_errno));

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
      rpt_vstring(depth, "last_checked_asleep:       %s", sbool(businfo->last_checked_dpms_asleep));
   }
#ifndef TARGET_BSD
   I2C_Sys_Info * info = get_i2c_sys_info(businfo->busno, -1);
   dbgrpt_i2c_sys_info(info, depth);
   free_i2c_sys_info(info);
#endif
   DBGMSF(debug, "Done");
}

//
// Lifecycle
//

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


/** Frees a #I2C_Bus_Info struct
 *
 *  @param businfo pointer to struct
 */
void i2c_free_bus_info(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo = %p", businfo);
   if (businfo)
      DBGTRC(debug, TRACE_GROUP, "marker = |%.4s|, busno = %d",  businfo->marker, businfo->busno);
   if (businfo && memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0) {   // just ignore if already freed
      if (businfo->edid) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,  "Calling free_parsed_edid for %p, marker=%s",
               businfo->edid, hexstring_t((Byte*) businfo->marker,4));
         SYSLOG2(DDCA_SYSLOG_DEBUG, "Calling free_parsed_edid for %p, marker=0x%s",
                                    businfo->edid, hexstring_t((Byte*)businfo->marker,4));
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


#ifdef UNUSED
// For g_ptr_array_set_free_func()
void i2c_gdestroy_bus_info(void * data) {
   i2c_free_bus_info(data);
}
#endif


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
      i2c_dbgrpt_bus_info(existing, 4);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "new bus info:");
      i2c_dbgrpt_bus_info(new, 4);
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

   COPY_BIT(existing, new, I2C_BUS_ADDR_0X50);
   COPY_BIT(existing, new, I2C_BUS_ADDR_0X37);
   COPY_BIT(existing, new, I2C_BUS_ADDR_0X30);
   COPY_BIT(existing, new, I2C_BUS_PROBED);
   COPY_BIT(existing, new, I2C_BUS_SYSFS_EDID);
   COPY_BIT(existing, new, I2C_BUS_DRM_CONNECTOR_CHECKED);
#undef COPY_BIT

   if (existing->drm_connector_name) {
      free(existing->drm_connector_name);
      existing->drm_connector_name = NULL;
   }
   if (!existing->drm_connector_name) {
      if (new->flags & I2C_BUS_DRM_CONNECTOR_CHECKED) {
         if (new->drm_connector_name)
            existing->drm_connector_name = g_strdup_printf("%s", new->drm_connector_name);
         existing->drm_connector_found_by = new->drm_connector_found_by;
         existing->flags |= I2C_BUS_DRM_CONNECTOR_CHECKED;
      }
   }
   existing->last_checked_dpms_asleep = new->last_checked_dpms_asleep;

   if ( IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Updated bus info:");
      i2c_dbgrpt_bus_info(existing, 4);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


//
// Generic Bus_Info retrieval
//

I2C_Bus_Info *
i2c_find_bus_info_in_gptrarray_by_busno(GPtrArray * buses, int busno) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno);

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


int   i2c_find_bus_info_index_in_gptrarray_by_busno(GPtrArray * buses, int busno) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno);

   int result = -1;
   for (int ndx = 0; ndx < buses->len; ndx++) {
      I2C_Bus_Info * cur_info = g_ptr_array_index(all_i2c_buses, ndx);
      if (cur_info->busno == busno) {
         result = ndx;
         break;
      }
   }

   DBGMSF(debug, "Done.     Returning: %d", result);
   return result;
}


//
// Operations on the set of all buses
//

GPtrArray * all_i2c_buses = NULL;  ///  array of  #I2C_Bus_Info


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

   I2C_Bus_Info * result = i2c_find_bus_info_in_gptrarray_by_busno(all_i2c_buses, busno);

   DBGMSF(debug, "Done.     Returning: %p", result);
   return result;
}


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

   assert(all_i2c_buses);
   int busct = all_i2c_buses->len;
   int reported_ct = 0;

   puts("");
   if (report_all)
      rpt_vstring(depth,"Detected %d non-ignorable I2C buses:", busct);
   else
      rpt_vstring(depth, "I2C buses with monitors detected at address 0x50:");

   for (int ndx = 0; ndx < busct; ndx++) {
      I2C_Bus_Info * busInfo = g_ptr_array_index(all_i2c_buses, ndx);
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


//
// Simple /dev/i2c inquiry
//

/** Checks if an I2C bus with a given number exists.
 *
 * @param   busno     bus number
 *
 * @return  true/false
 */
bool i2c_device_exists(int busno) {
   bool result = false;
   bool debug = false;
   int  errsv;
   char namebuf[20];
   struct stat statbuf;
   int  rc = 0;
   sprintf(namebuf, "/dev/"I2C"-%d", busno);
   errno = 0;
   rc = stat(namebuf, &statbuf);
   errsv = errno;
   if (rc == 0) {
      DBGMSF(debug, "Found %s", namebuf);
      result = true;
   }
   else {
      DBGMSF(debug,  "stat(%s) returned %d, errno=%s",
                     namebuf, rc, linux_errno_desc(errsv) );
   }

   DBGMSF(debug, "busno=%d, returning %s", busno, sbool(result) );
   return result;
}


/** Returns the number of I2C buses on the system, by looking for
 *  devices named /dev/i2c-n.
 *
 *  Note that no attempt is made to open the devices.
 */
int i2c_device_count() {
   bool debug = false;
   int  busct = 0;

   for (int busno=0; busno < I2C_BUS_MAX; busno++) {
      if (i2c_device_exists(busno))
         busct++;
   }
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Returning %d", busct );
   return busct;
}




/** Module initialization. */
void init_i2c_bus_base() {
   RTTI_ADD_FUNC(i2c_dbgrpt_buses);
   RTTI_ADD_FUNC(i2c_free_bus_info);
   RTTI_ADD_FUNC(i2c_get_drm_connector_name);
   RTTI_ADD_FUNC(i2c_new_bus_info);
   RTTI_ADD_FUNC(i2c_reset_bus_info);
   RTTI_ADD_FUNC(i2c_update_bus_info);

   // connected_buses = EMPTY_BIT_SET_256;
}

