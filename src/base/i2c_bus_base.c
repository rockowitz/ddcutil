/** @file i2c_bus_base.c
 */

// Copyright (C) 2014-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

#include "util/coredefs.h"
#include "util/debug_util.h"
#include "util/edid.h"

#include "public/ddcutil_types.h"

#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"

#include "core.h"
#include "rtti.h"

#include "i2c_bus_base.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;


GPtrArray * all_i2c_buses = NULL;  ///  array of  #I2C_Bus_Info
bool use_x37_detection_table = false;
static GMutex all_i2c_buses_mutex;


//
// Local utility functions
//

// Keep in sync with definitions in i2c_bus_base.h
Value_Name_Table i2c_bus_flags_table = {
      VN(I2C_BUS_EXISTS),
      VN(I2C_BUS_ACCESSIBLE),
    //  VN(I2C_BUS_ADDR_0X50),
      VN(I2C_BUS_ADDR_X37),
      VN(I2C_BUS_ADDR_X30),
//      VN(I2C_BUS_EDP),
//      VN(I2C_BUS_LVDS),
      VN(I2C_BUS_PROBED),
//      VN(I2C_BUS_VALID_NAME_CHECKED),
//      VN(I2C_BUS_HAS_VALID_NAME),
      VN(I2C_BUS_SYSFS_EDID),
      VN(I2C_BUS_X50_EDID),
//      VN(I2C_BUS_DRM_CONNECTOR_CHECKED),
      VN(I2C_BUS_LVDS_OR_EDP),
      VN(I2C_BUS_APPARENT_LAPTOP),
      VN(I2C_BUS_DISPLAYLINK),
#ifdef OLD
      VN(I2C_BUS_SYSFS_UNRELIABLE),
#endif
      VN(I2C_BUS_INITIAL_CHECK_DONE),
      VN(I2C_BUS_DDC_CHECKS_IGNORABLE),
      VN(I2C_BUS_SYSFS_KNOWN_RELIABLE),
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
   // assert(businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED);
   assert(businfo->drm_connector_found_by != DRM_CONNECTOR_NOT_CHECKED);
   char * result = NULL;
   if (businfo->drm_connector_found_by != DRM_CONNECTOR_NOT_FOUND) {
      assert(businfo->drm_connector_name);
      RPT_ATTR_TEXT(-1, &result, "/sys/class/drm", businfo->drm_connector_name, attribute);
   }
   return result;
}


void i2c_remove_bus_by_busno(int busno) {
   bool debug  = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d", busno);
   assert(busno);
   g_mutex_lock(&all_i2c_buses_mutex);
   int  busNdx = i2c_find_bus_info_index_in_gptrarray_by_busno(all_i2c_buses, busno);
   if (busNdx < 0) {
      MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "Record for busno %d not found in all_i2c_buses array", busno);
   }
   else {
      I2C_Bus_Info * businfo = g_ptr_array_remove_index(all_i2c_buses, busNdx);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "businfo=%p", businfo);
      // not needed, i2c_free_bus_info() is free func for all_i2c_buses
      // i2c_free_bus_info(businfo);
   }
   g_mutex_unlock(&all_i2c_buses_mutex);
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


// called if display removed, bus may or may not still exist
void i2c_reset_bus_info(I2C_Bus_Info * businfo) {
   bool debug  = false;
   assert(businfo);
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo=%p, busno = %d, flags=%s",
         businfo, businfo->busno, i2c_interpret_bus_flags_t(businfo->flags));
   if (i2c_device_exists(businfo->busno))
      businfo->flags &= ~(I2C_BUS_ACCESSIBLE | I2C_BUS_ADDR_X30 | I2C_BUS_ADDR_X37 |
             I2C_BUS_SYSFS_EDID | I2C_BUS_X50_EDID );
   if (businfo->edid) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,  "Calling free_parsed_edid for %p, marker=%s",
            businfo->edid, hexstring_t((Byte*) businfo->marker, 4));
      SYSLOG2(DDCA_SYSLOG_DEBUG, "Calling free_parsed_edid for %p, marker=%s",
            businfo->edid, hexstring_t((Byte*) businfo->marker,4));
      free_parsed_edid(businfo->edid);
      businfo->edid = NULL;
   }
   if ( IS_DBGTRC(debug, TRACE_GROUP) ) {
      DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Final businfo:");
      i2c_dbgrpt_bus_info(businfo, true, 2);
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


  // if (!(businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED) ) {    // ??? when can this be false? ???
      result = businfo->drm_connector_name;
 //   }

   DBGTRC_RETURNING(debug, TRACE_GROUP, result, "");
   return result;
}



/** Reports on a single I2C bus.
 *
 *  \param   businfo         pointer to Bus_Info structure describing bus
 *  \param   include_sysinfo report I2C_Sys_Info for bus
 *  \param   depth           logical indentation depth
 *
 *  \remark
 *  Although this is a debug type report, it is called (indirectly) by the
 *  ENVIRONMENT command.
 */
void i2c_dbgrpt_bus_info(I2C_Bus_Info * businfo, bool include_sysinfo, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "businfo=%p, include_sysinfo=%s", businfo, SBOOL(include_sysinfo));
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
      rpt_vstring(depth, "Address 0x30 present:    %s", sbool(businfo->flags & I2C_BUS_ADDR_X30));
#endif
      rpt_vstring(depth, "Address 0x37 present:    %s", sbool(businfo->flags & I2C_BUS_ADDR_X37));
      rpt_vstring(depth, "Address 0x50 present:    %s", sbool(businfo->flags & I2C_BUS_ADDR_0X50));
      rpt_vstring(depth, "Device busy:             %s", sbool(businfo->flags & I2C_BUS_BUSY));
#endif
      rpt_vstring(depth, "errno for open:          %s", psc_desc(businfo->open_errno));

//      rpt_vstring(depth, "Connector name checked:  %s", sbool(businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED));
      rpt_vstring(depth, "drm_connector_found_by:  %s (%d)",
         drm_connector_found_by_name(businfo->drm_connector_found_by), businfo->drm_connector_found_by);
      if (businfo->drm_connector_found_by != DRM_CONNECTOR_NOT_CHECKED) {
         rpt_vstring(depth, "drm_connector_name:      %s", businfo->drm_connector_name);
         rpt_vstring(depth, "drm_connector_id:        %d", businfo->drm_connector_id);
      }
      // not useful and clutters the output
      // i2c_report_functionality_flags(businfo->functionality, /* maxline */ 90, depth);
      if (businfo->edid) {
         report_parsed_edid(businfo->edid, true /* verbose */, depth);
      }
      rpt_vstring(depth, "last_checked_asleep:       %s", sbool(businfo->last_checked_dpms_asleep));
   }
#ifdef OUT    // sole non-sysfs use of i2c_sysfs_i2c_sys_info.c:
#ifndef TARGET_BSD
   if (include_sysinfo) {
      I2C_Sys_Info * info = get_i2c_sys_info(businfo->busno, -1);
      dbgrpt_i2c_sys_info(info, depth);
      free_i2c_sys_info(info);
   }
#endif
#endif

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
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
   assert(busno != 255 && busno != -1);
   I2C_Bus_Info * businfo = calloc(1, sizeof(I2C_Bus_Info));
   memcpy(businfo->marker, I2C_BUS_INFO_MARKER, 4);
   businfo->busno = busno;
   businfo->drm_connector_found_by = DRM_CONNECTOR_NOT_CHECKED;
#ifdef ALT_LOCK_REC
   businfo->lock_record = create_display_lock_record(i2c_io_path(busno));
#endif
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %p", businfo);
   return businfo;
}


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


I2C_Bus_Info * i2c_get_bus_info(int busno, bool* new_info) {
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d", busno);
   *new_info = false;
   g_mutex_lock(&all_i2c_buses_mutex);
   I2C_Bus_Info * businfo =  i2c_find_bus_info_in_gptrarray_by_busno(all_i2c_buses, busno);
   if (!businfo) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding /dev/"I2C"-%d to set of buses", busno);
      businfo = i2c_new_bus_info(busno);
      businfo->flags = I2C_BUS_EXISTS;
      g_ptr_array_add(all_i2c_buses, businfo);
      *new_info = true;
   }
   g_mutex_unlock(&all_i2c_buses_mutex);
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning businfo=%p for busno%d, *new_info=%s",
         businfo, busno, SBOOL(*new_info));
   return businfo;
}



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



void i2c_discard_buses0(GPtrArray* buses) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "buses=%p", buses);

   if (buses) {
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


/** Frees a #I2C_Bus_Info struct
 *
 *  @param businfo pointer to struct
 */
void i2c_free_bus_info(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo = %p", businfo);
   // if (IS_DBGTRC(debug, TRACE_GROUP))
   //    show_backtrace(1);
   if (businfo)
      DBGTRC(debug, TRACE_GROUP, "marker = |%.4s|, busno = %d",  businfo->marker, businfo->busno);
   if (businfo && memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0) {   // just ignore if already freed
      if (businfo->edid) {
         char msg[100];
         g_snprintf(msg, 100,  "Calling free_parsed_edid busno=%d, edid=%p, marker=%s",
               businfo->busno, businfo->edid, hexstring_t((Byte*) businfo->marker,4));
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,  "%s", msg);
         if (IS_DBGTRC(debug, TRACE_GROUP))
            SYSLOG2(DDCA_SYSLOG_DEBUG, "%s", msg);
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


//
// Generic Bus_Info retrieval
//

I2C_Bus_Info *
i2c_find_bus_info_in_gptrarray_by_busno(GPtrArray * buses, int busno) {
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
// Simple /dev/i2c inquiry
//

/** Checks if an I2C bus with a given number exists.
 *
 * @param   busno     bus number
 *
 * @return  true/false
 */
bool i2c_device_exists(int busno) {
   bool debug = false;

   bool result = false;
   char namebuf[20];
   struct stat statbuf;
   sprintf(namebuf, "/dev/"I2C"-%d", busno);
   int rc = stat(namebuf, &statbuf);
   if (rc == 0) {
      result = true;
   }
   else {
      DBGMSF(debug,  "stat(%s) returned %d, errno=%s",
                     namebuf, rc, linux_errno_desc(errno) );
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


Error_Info * i2c_check_device_access(char * dev_name) {
   Error_Info * err = NULL;
   if ( access(dev_name, R_OK|W_OK) < 0 ) {
      int errsv = errno;   // EACCESS if lack permissions, ENOENT if file doesn't exist
      char * s = NULL;
      if (errsv == ENOENT) {
        s = g_strdup_printf("access(%s) returned ENOENT", dev_name);
        DBGMSG("%s", s);
        err = ERRINFO_NEW(-ENOENT, "%s", s);
        // should never occur because of i2c_device_exists() check
      }
      else if (errsv == EACCES) {
        s = g_strdup_printf("Device %s lacks R/W permissions", dev_name);
        // DBGMSG("%s", s);
        err = ERRINFO_NEW(-EACCES, "%s", s);
        SYSLOG2(DDCA_SYSLOG_WARNING, "%s", s);
      }
      else {
        s = g_strdup_printf( "access() returned errno = %s", psc_desc(errsv));
        SYSLOG2(DDCA_SYSLOG_ERROR, "%s", s);
        err = ERRINFO_NEW(-ENOENT, "%s", s);
      }
      free(s);
   }
   return err;
}


//
// x37 detection table - Records x37 responsiveness to avoid recheck
//

const char * x37_detection_state_name(X37_Detection_State state) {
   char * s = NULL;
   switch(state) {
   case X37_Not_Recorded:  s = "X37_Not_Recorded"; break;
   case X37_Not_Detected:  s = "X37_Not_Detected"; break;
   case X37_Detected:      s = "X37_Detected";     break;
   }
   return s;
}


static GHashTable * x37_detection_table = NULL;

char * x37_detection_table_key(int busno, Byte* edidbytes) {
   char * buf = g_strdup_printf("%s%d", hexstring_t(edidbytes,128), busno);
   //guint key = g_str_hash(buf);
   // free(buf);
   return buf;
}


void  i2c_record_x37_detected(int busno, Byte * edidbytes, X37_Detection_State detected) {
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "detected = %s, busno=%d, edidbytes = %s",
         x37_detection_state_name(detected), busno, hexstring_t(edidbytes+120, 8));

   if (!x37_detection_table)
      x37_detection_table =  g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
   assert(detected != X37_Not_Recorded);
   char * key = x37_detection_table_key(busno, edidbytes);
   g_hash_table_replace(x37_detection_table, key, GINT_TO_POINTER(detected));
   // free(key);

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


X37_Detection_State  i2c_query_x37_detected(int busno, Byte * edidbytes) {
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d, edidbytes = ...%s",
         busno, hexstring_t(edidbytes+120, 8));

   X37_Detection_State result = X37_Not_Recorded;
   if (x37_detection_table) {
      char * key = x37_detection_table_key(busno, edidbytes);
      gpointer pval = g_hash_table_lookup(x37_detection_table, key);
      result = GPOINTER_TO_INT(pval);
      free(key);
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning: %s", x37_detection_state_name(result));
   return result;
}


// moved from sysfs_i2c_util.c:


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
   bool debug = false;
   DBGF(debug, "Starting. path=%s", path);
   assert(path);
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
   bool debug = false;
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
      free(adapter_path);
   }

   DBGF(debug, "Done. busno=%d, returning %s", busno, driver_name);
   return driver_name;
}


#ifdef UNUSED
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
#endif


#ifdef UNUSED
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
#endif


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
   bool debug = false;
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




/** Module initialization. */
void init_i2c_bus_base() {
   // RTTI_ADD_FUNC(i2c_add_bus);
   RTTI_ADD_FUNC(i2c_get_bus_info);
   RTTI_ADD_FUNC(i2c_discard_buses0);
   RTTI_ADD_FUNC(i2c_discard_buses);
   RTTI_ADD_FUNC(i2c_dbgrpt_buses);
   RTTI_ADD_FUNC(i2c_free_bus_info);
//   RTTI_ADD_FUNC(i2c_get_drm_connector_name);
   RTTI_ADD_FUNC(i2c_new_bus_info);
   RTTI_ADD_FUNC(i2c_reset_bus_info);
   RTTI_ADD_FUNC(i2c_update_bus_info);
   RTTI_ADD_FUNC(i2c_remove_bus_by_busno);
   RTTI_ADD_FUNC(i2c_dbgrpt_bus_info);
   RTTI_ADD_FUNC(i2c_query_x37_detected);
   RTTI_ADD_FUNC(i2c_record_x37_detected);

   // connected_buses = EMPTY_BIT_SET_256;
}

void terminate_i2c_bus_base() {
   // DBGMSG("Executing.  x37_detection_table = %p", x37_detection_table);
   if (x37_detection_table) {
      g_hash_table_destroy(x37_detection_table);
   }

   if (all_i2c_buses) {
      // i2c_free_bus_info() already set as free function
      g_ptr_array_free(all_i2c_buses, true);
      free (all_i2c_buses);
   }
}

