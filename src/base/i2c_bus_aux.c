/** @file i2c_bus_aux.c
 */

// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <assert.h>
#include <glib-2.0/glib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util/coredefs.h"
#include "util/edid.h"

#include "public/ddcutil_types.h"

#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"

#include "core.h"
#include "rtti.h"

#include "i2c_bus_aux.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

//
// Local utility functions
//

// Keep in sync with definitions in i2c_bus_aux.h
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
      VN(I2C_BUS_DDC_DISABLED),
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


// Drm Connector
const char * drm_connector_found_by_name(Drm_Connector_Found_By found_by) {
   char * result = NULL;
   switch(found_by) {
   case DRM_CONNECTOR_NOT_CHECKED:    result = "DRM_CONNECTOR_NOT_CHECKED";    break;
   case DRM_CONNECTOR_NOT_FOUND:      result = "DRM_CONNECTOR_NOT_FOUND";      break;
   case DRM_CONNECTOR_FOUND_BY_BUSNO: result = "DRM_CONNECTOR_FOUND_BY_BUSNO"; break;
   case DRM_CONNECTOR_FOUND_BY_EDID:  result = "DRM_CONNECTOR_FOUND_BY_EDID";  break;
#ifdef USER_CONNECTOR
   case DRM_CONNECTOR_FOUND_BY_USER:  result = "DRM_CONNECTOR_FOUND_BY_USER";  break;
#endif
   }
   return result;
}


const char * drm_connector_found_by_public_name(Drm_Connector_Found_By found_by) {
   char * result = NULL;
   switch(found_by) {
   case DRM_CONNECTOR_NOT_CHECKED:    result = "Not checked";    break;
   case DRM_CONNECTOR_NOT_FOUND:      result = "Not found";      break;
   case DRM_CONNECTOR_FOUND_BY_BUSNO: result = "I2C bus number"; break;
   case DRM_CONNECTOR_FOUND_BY_EDID:  result = "EDID";           break;
#ifdef USER_CONNECTOR
   case DRM_CONNECTOR_FOUND_BY_USER:  result = "User";           break;
#endif
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


#ifdef UNUSED
char * i2c_get_drm_connector_name(I2C_Bus_Info * businfo) {
   bool debug = false;
   char * result = NULL;
   DBGTRC_STARTING(debug, TRACE_GROUP,
         "busno=%d, drm_connector_found_by=%s drm_connector_name=|%s|",
         businfo->busno, drm_connector_found_by_name(businfo->drm_connector_found_by),
         businfo->drm_connector_name);
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "flags: %s", i2c_interpret_bus_flags_t(businfo->flags) );

   assert(businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED);
   result = businfo->drm_connector_name;

   DBGTRC_RET_STRING(debug, TRACE_GROUP, result, "");
   return result;
}
#endif

//
// Reports
//

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
   rpt_vstring(depth, "removed:                 %s", sbool(businfo->removed));
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
         if (businfo->drm_connector_name) {
            // possibly_write_detect_to_status_by_businfo(businfo); // in i2c/i2c_sysfs_base.h
            rpt_label(depth, "Current /sys attributes:");
            RPT_ATTR_TEXT(depth+1, NULL, "/sys/class/drm", businfo->drm_connector_name, "enabled");
            RPT_ATTR_TEXT(depth+1, NULL, "/sys/class/drm", businfo->drm_connector_name, "status");
            RPT_ATTR_TEXT(depth+1, NULL, "/sys/class/drm", businfo->drm_connector_name, "dpms");
            // RPT_ATTR_EDID(depth+1, NULL, "/sys/class/drm", businfo->drm_connector_name, "edid");
            bool edid_found = GET_ATTR_EDID(NULL, "/sys/class/drm",businfo->drm_connector_name, "edid");
            rpt_vstring(depth, "/sys/class/drm/%s/edid:                                  %s",
                  businfo->drm_connector_name, (edid_found) ? "Found" : "Not found");
         }
      }
      // not useful and clutters the output
      // i2c_report_functionality_flags(businfo->functionality, /* maxline */ 90, depth);
      // if (businfo->edid) {
      //    report_parsed_edid(businfo->edid, true /* verbose */, depth);
      // }
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
// Basic lifecycle
//

/** Allocates and initializes a new #I2C_Bus_Info struct
 *
 * @param busno I2C bus number
 * @return newly allocated #I2C_Bus_Info
 */
I2C_Bus_Info * i2c_new_bus_info(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d", busno);
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


/** Frees a #I2C_Bus_Info struct
 *
 *  @param businfo pointer to struct
 */
void i2c_free_bus_info(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo = %p", businfo);
   if (businfo)
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "marker = |%.4s|, busno = %d",  businfo->marker, businfo->busno);
   if (businfo && memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0) {   // just ignore if already freed
      if (businfo->edid) {
         char msg[100];
         g_snprintf(msg, 100,  "Calling free_parsed_edid busno=%d, edid=%p, marker=%s",
               businfo->busno, businfo->edid, hexstring_t((Byte*) businfo->marker,4));
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,  "%s", msg);
         if (IS_DBGTRC(debug, TRACE_GROUP))
            DECORATED_SYSLOG(DDCA_SYSLOG_DEBUG, "%s", msg);
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


I2C_Bus_Info *   i2c_copy_bus_info(I2C_Bus_Info * businfo) {
   I2C_Bus_Info * businfo2 = calloc(1, sizeof(I2C_Bus_Info));
   memcpy(businfo2, businfo, sizeof(I2C_Bus_Info));
   // now copy pointer fields
   businfo2->driver = g_strdup(businfo->driver);
   businfo2->drm_connector_name = g_strdup(businfo->drm_connector_name);
   if (businfo->edid)
      businfo2->edid = copy_parsed_edid(businfo->edid);
   else
      businfo2->edid = NULL;
   return businfo2;
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


#ifdef UNUSED
/** Checks if the current user has R/W access to a file,
 *  using function access()
 *
 *  @param dev_name  file to check
 *  @return NULL if file can be read and written, Error_Info struct if not
 *
 *  Status values if Error_Info:
 *  - -ENOENT file does not exist, or unexpected error (explanation in detail)
 *  - -EACCES R/W permissions lacking
 */
Error_Info * i2c_check_device_access(char * dev_name) {
   Error_Info * err = NULL;
   if (running_as_root) {
      struct stat stat_buf;
      int rc = stat(dev_name, &stat_buf);
      if (rc != 0)
         err = ERRINFO_NEW(-ENOENT, "");
   }
   else {
      if ( access(dev_name, R_OK|W_OK) < 0 ) {
         int errsv = errno;   // EACCESS if lack permissions, ENOENT if file doesn't exist
         char * s = NULL;
         if (errsv == ENOENT) {
            // should never occur because of prior i2c_device_exists() check
           s = g_strdup_printf("access(%s) returned ENOENT", dev_name);
           DBGMSG("%s", s);
           err = ERRINFO_NEW(-ENOENT, "%s", s);
           DECORATED_SYSLOG(DDCA_SYSLOG_WARNING, "%s", s);
         }
         else if (errsv == EACCES) {
           s = g_strdup_printf("Device %s lacks R/W permissions", dev_name);
           // DBGMSG("%s", s);
           err = ERRINFO_NEW(-EACCES, "%s", s);
           DECORATED_SYSLOG(DDCA_SYSLOG_WARNING, "%s", s);
         }
         else {
           s = g_strdup_printf( "access() returned errno = %s", psc_desc(errsv));
           DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "%s", s);
           err = ERRINFO_NEW(-ENOENT, "%s", s);
         }
         free(s);
      }
   }
   return err;
}
#endif


//
// x37 detection table - Records x37 responsiveness to avoid recheck
//
//  Key:   EDIDa and bus number, in text form
//  Value: X37_Detection_State, integer, stored as a pointer
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

/** Creates hash table key
 *
 *  @param  busno      /dev/i2c bus number
 *  @param  edidbytes  pointer to 128 byte EDID
 *  @return hash table key as newly allocated string
 */
char * x37_detection_table_key(int busno, Byte* edidbytes) {
   char * buf = g_strdup_printf("%s%d", hexstring_t(edidbytes,128), busno);
   return buf;
}

/** Records the X37 detection state for an EDID/busno pair
 *
 *  @param  busno      /dev/i2c bus number
 *  @param  edidbytes  pointer to 128 byte EDID
 *  @param  detected   detection state to record
 *
 *  The hash table is created if it does not already exist
 */
void  i2c_record_x37_detected(int busno, Byte * edidbytes, X37_Detection_State detected) {
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "detected = %s, busno=%d, edidbytes = %s",
         x37_detection_state_name(detected), busno, hexstring_t(edidbytes+120, 8));
   assert(detected != X37_Not_Recorded);

   if (!x37_detection_table)
      x37_detection_table =  g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

   char * key = x37_detection_table_key(busno, edidbytes);
   g_hash_table_replace(x37_detection_table, key, GINT_TO_POINTER(detected));

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


/** Query the detection state for an EDID/busno pair
 *
 *  @param  busno      /dev/i2c bus number
 *  @param  edidbytes  pointer to 128 byte EDID
 *  @return detection state
 */
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


//
// Ignored buses
//

static Bit_Set_256 ignored_i2c_buses = {0};

/** Specify /dev/i2c-N devices to be ignored, i2c bus numbers.
 *
 *  @param ignored_busno_flags bits indicate i2c bus numbers to ignore
 */
void
i2c_exclude_buses(Bit_Set_256 ignored_busno_flags) {
   bool debug = false;
   ignored_i2c_buses = ignored_busno_flags;

   DBGTRC_EXECUTED(debug, TRACE_GROUP, "ignored_i2c_buses: %s",
         bs256_to_string_decimal_t(ignored_i2c_buses, "", " "));
}


bool  i2c_bus_is_excluded(int busno) {
   bool debug = false;
   bool result = bs256_contains(ignored_i2c_buses, busno);

   DBGTRC_EXECUTED(debug, TRACE_GROUP, "busno=%d, returning %s",
         busno, sbool(result) );
   return result;
}


// for use as Byte_Value_Array filter function
bool  i2c_bus_is_not_excluded(int busno) {
   bool debug = false;
   bool result = !bs256_contains(ignored_i2c_buses, busno);

   DBGTRC_EXECUTED(debug, TRACE_GROUP, "busno=%d, returning %s",
         busno, sbool(result) );
   return result;
}


void init_i2c_bus_aux(void) {
   RTTI_ADD_FUNC(i2c_dbgrpt_bus_info);
   RTTI_ADD_FUNC(i2c_new_bus_info);
   RTTI_ADD_FUNC(i2c_free_bus_info);
   RTTI_ADD_FUNC(i2c_device_count);
   RTTI_ADD_FUNC(i2c_record_x37_detected);
   RTTI_ADD_FUNC(i2c_query_x37_detected);
   RTTI_ADD_FUNC(i2c_exclude_buses);
   RTTI_ADD_FUNC(i2c_bus_is_excluded);
   RTTI_ADD_FUNC(i2c_bus_is_not_excluded);
}

/** Module termination **/
void terminate_i2c_bus_aux() {
   // DBGMSG("Executing.  x37_detection_table = %p", x37_detection_table);
   if (x37_detection_table) {
      g_hash_table_destroy(x37_detection_table);
   }
}


