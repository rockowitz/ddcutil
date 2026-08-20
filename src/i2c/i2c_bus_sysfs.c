/** @file i2c_bus_sysfs.c
 *  Sysfs based functions for I2C buses
 *
 *  Functions that determine properties of an I2C bus, or of the display
 *  attached to it, by reading sysfs rather than by communicating with the
 *  display over the bus.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "util/coredefs.h"
#include "util/edid.h"
#include "util/file_util.h"
#include "util/glib_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"
#include "base/i2c_bus_base.h"
#include "base/rtti.h"

#include "sysfs/sysfs_base.h"

#include "i2c_bus_sysfs.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;


// quick and dirty for debugging
static char *
edid_summary_from_bytes(Byte * edidbytes) {
   static GPrivate  key = G_PRIVATE_INIT(g_free);

   char * buf = get_thread_fixed_buffer(&key, 200);
   if (!edidbytes)
      strcpy(buf, "null edid ptr");
   else {
      Parsed_Edid * parsed = create_parsed_edid(edidbytes);
      if (!parsed)
         strcpy(buf, "Invalid EDID");
      else {
         strcpy(buf, parsed->model_name);
         free_parsed_edid(parsed);
      }
   }

   return buf;
}


/** Determines whether an I2C bus is that of a DisplayLink device,
 *  by examining the bus's sysfs name attribute.
 *
 *  @param  busno  I2C bus number
 *  @return true/false
 */
bool is_displaylink_device(int busno) {
   bool debug = false;
   bool result = false;
   char bus_path[40];
   g_snprintf(bus_path, 40, "/sys/bus/i2c/devices/i2c-%d", busno);
   char * name;
   RPT_ATTR_TEXT((debug)? 1 : -1, &name, bus_path, "name");
   if (name) {
      result =  streq(name, "DisplayLink I2C Adapter");
      free(name);
   }
   return result;
}


void free_found_sys_drm_connector_result_contents(Found_Sys_Drm_Connector rec) {
   free(rec.connector_name);
}

static void dbgrpt_found_sys_drm_connector(Found_Sys_Drm_Connector val, int depth) {
   rpt_vstring(depth, "Found_Sys_Drm_Connector:");
   rpt_vstring(depth+1, "connector_name:   %s", val.connector_name);
   rpt_vstring(depth+1, "connector_id:     %d", val.connector_id);
   rpt_vstring(depth+1, "found_by:         %s", drm_connector_found_by_name(val.found_by));
}


/** Locates a drm-card-connector directory using either an
 *  I2C bus number or EDID value.
 *
 *  @param  busno      (-1 for not set)
 *  @param  edid_bytes pointer to 128 byte edid
 *  @return #DRM
 *
 *  @remark
 *  Either one of busno edid_bytes should be set.  Having both parameters
 *  avoids having 2 separate functions, one for bus number and one for EDID,
 *  with essentially the same logic.
 *  @remark
 *  Given its small size, the result is returned on the stack, not
 *  the heap, avoiding the need for the caller to free.
 */
// n. result returned on stack
Found_Sys_Drm_Connector find_sys_drm_connector_by_busno_or_edid(
                                 int busno, Byte * edid_bytes)
{
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, " busno = %d, edid = %p" , busno, edid_bytes);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "edid=%p -> %s",
         edid_bytes, edid_summary_from_bytes(edid_bytes));
   int d = (IS_DBGTRC(debug, DDCA_TRC_NONE)) ? 1 : -1;
   if (busno == 255)  // happens somehow
      busno = -1;
   bool check_busno = (busno != -1);
   bool check_edid = edid_bytes;
   assert(check_busno || check_edid);

   Found_Sys_Drm_Connector result;
   result.connector_name = NULL;
   result.found_by = DRM_CONNECTOR_NOT_FOUND;
   result.connector_id = 0;

   Sysfs_Connector_Names cnames = get_sysfs_drm_connector_names();
   GPtrArray * drm_connector_names = cnames.all_connectors;
   bool found = false;
   for (int ndx = 0; ndx < drm_connector_names->len && !found; ndx++) {
      char * cname = g_ptr_array_index(drm_connector_names, ndx);
      if (check_busno) {
         Connector_Bus_Numbers * cbn = calloc(1, sizeof(Connector_Bus_Numbers));
         get_connector_bus_numbers("/sys/class/drm", cname, cbn);
         if (cbn->i2c_busno == busno){
            found = true;
            result.connector_name = strdup(cname);
            result.found_by = DRM_CONNECTOR_FOUND_BY_BUSNO;
            result.connector_id = cbn->connector_id;
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                  "Found connector %s by i2c bus number match for bus i2c-%d", cname, busno);
         }
         free_connector_bus_numbers(cbn);
      }
      if (check_edid) {
         // don't bother if we already have the answer
         if (result.found_by != DRM_CONNECTOR_FOUND_BY_BUSNO) {
            GByteArray*  edid_bytes_array = NULL;
            possibly_write_detect_to_status_by_connector_name(cname);
            RPT_ATTR_EDID(d, &edid_bytes_array, "/sys/class/drm", cname, "edid");
            if (edid_bytes_array && edid_bytes_array->len >= 128) {
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Got edid from sysfs: %s",
                      edid_summary_from_bytes(edid_bytes_array->data));
                if (memcmp(edid_bytes_array->data, edid_bytes, 128) == 0) {
                   found = true;
                   result.connector_name = strdup(cname);
                   result.found_by = DRM_CONNECTOR_FOUND_BY_EDID;
                   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                         "Found connector %s by EDID match for bus i2c-%d", cname, busno);
                }
                g_byte_array_free(edid_bytes_array, true);
            }
         }
      }
   }
   free_sysfs_connector_names_contents(cnames);

   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      dbgrpt_found_sys_drm_connector(result, 1);
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
   return result;
}


/** Returns the value of the edid attribute for a DRM connector.
 *
 *  @param  connector_name
 *  @return pointer to EDID bytes, caller responsible for freeing
 *          NULL if not found
 */
Byte * get_connector_edid(const char * connector_name) {
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "connector_name = %s", connector_name);
   int d = (debug) ? 1 : -1;

   // char * driver =  get_i2c_sysfs_driver_by_busno(busno);    // where to get busno;
   // maybe_write_detect_to_status("nvidia", connector_name);     // lie

   Byte * result = NULL;
   GByteArray*  edid_bytes = NULL;
   possibly_write_detect_to_status_by_connector_name(connector_name);
   RPT_ATTR_EDID(d, &edid_bytes, "/sys/class/drm", connector_name, "edid");
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "edid_bytes=%p", edid_bytes);
   if (edid_bytes && edid_bytes->len >= 128) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "edid_bytes->len=%d", edid_bytes->len);
      result = edid_bytes->data;
      g_byte_array_free(edid_bytes, false);
   }
   else {
      if (edid_bytes)   {
         // handle pathological case of < 128 bytes read
         g_byte_array_free(edid_bytes, true);
      }
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "result = %p", result);
   if (IS_DBGTRC(debug, DDCA_TRC_NONE) && result)
      rpt_hex_dump(result, 128, 2);
   return result;
}


 //
 // Functions used only by i2c_check_bus(), but factored out to clarify
 // the function logic
 //

/** Reads the EDID for a bus from the sysfs directory of the DRM connector
 *  recorded in its #I2C_Bus_Info.
 *
 *  @param  businfo  pointer to I2C_Bus_Info instance
 *  @return pointer to parsed EDID, NULL if it could not be read or parsed
 */
Parsed_Edid * get_parsed_edid_for_businfo_using_sysfs(I2C_Bus_Info * businfo) {
    assert(businfo);
    bool debug  = false;
    DBGTRC_STARTING(debug, DDCA_TRC_NONE,
          "businfo = %p, businfo->busno=%d", businfo, businfo->busno);

    Parsed_Edid * pedid = NULL;

    // maybe_write_detect_to_status(businfo->driver, businfo->drm_connector_name);

    Byte * edidbytes = get_connector_edid(businfo->drm_connector_name);
    if (edidbytes) {
       pedid = create_parsed_edid2(edidbytes, "SYSFS");
       if (!pedid) {
          DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Invalid EDID read from /sys/class/drm/%s/edid",
                businfo->drm_connector_name);
          DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "Invalid EDID read from /sys/class/drm/%s/edid",
                businfo->drm_connector_name);
       }
       else {
          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                "Found edid for /dev/i2c-%d using connector name %s",
                 businfo->busno, businfo->drm_connector_name);
       }
       free(edidbytes);
    }
    else {
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
             "Failed to get edid using DRM connector %s", businfo->drm_connector_name);
    }

    DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning %p", pedid);
    return pedid;
 }


/** Determines if a card-connector class attribute represents a
 *  display controller.
 *
 *  @param  adapter_class  class value as string
 *  @return true/false
 */
bool is_adapter_class_display_controller(const char * adapter_class) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "class = %s", adapter_class);

   bool result = true;
   uint32_t cl2 = 0;
   uint32_t i_class = 0;
   /* bool ok =*/  str_to_int(adapter_class, (int*) &i_class, 16);  // if fails, &result unchanged
   cl2 = i_class & 0xffff0000;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cl2 = 0x%08x", cl2);
   if (cl2 != 0x030000 && cl2 != 0x0a0000 /* docking station*/ ) {
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Device class not a display driver: 0x%08x", cl2);
       result = false;
   }

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "");
   return result;
}


 // probably belongs elsewhere

/** Determines whether a DRM connector name is that of an existing
 *  card-connector directory in /sys/class/drm.
 *
 *  @param  connector_name
 *  @return true/false
 */
bool is_valid_drm_connector_name(const char * connector_name) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "connector_name=|%s|", connector_name);

   char * fq_name = g_strdup_printf("/sys/class/drm/%s", connector_name);
   bool result = directory_exists(fq_name);

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Connector_name=|%s|, fq_name=|%s|, returning %s",
         connector_name, fq_name, sbool(result));
   free(fq_name);
   return result;
}


#ifdef UNUSED
 // TODO: MOVE ELSEWHERE
 /** Scans a single connector directory of /sys/class/drm.
  *
  *  Has typedef Dir_Foreach_Func
  *
  *  Adds the card-connector (as a string) to a #GPtrArray.
  *
  *  @param   dirname
  *  @param   fn
  *  @param   accumulator  #GPtrArray to collect the names
  *  @param   depth        ignored
  */
 void add_one_drm_connector_name(
       const char *  dirname,      // /sys/class/drm
       const char *  fn,           // e.g. card0-DP-1
       void *        accumulator,
       int           depth)
 {
    bool debug = false;
    DBGTRC_STARTING(debug, TRACE_GROUP, "dirname=%s, fn=%s, depth=%d", dirname, fn, depth);

    GPtrArray * connectors = accumulator;
    g_ptr_array_add(connectors, g_strdup(fn));

    DBGTRC_DONE(debug, TRACE_GROUP, "");
 }


 /** Returns a GPtrArray of all the card-connector directory names,
  *  e.g. card1-HDMI-A-1
  *
  *  @return #GPtrArray of names, caller must free
  */
 GPtrArray *  get_drm_connector_names() {
    bool debug = false;
    DBGTRC_STARTING(debug, DDCA_TRC_I2C,"");

    GPtrArray * connector_names = g_ptr_array_new_with_free_func(free_sys_drm_connector);
    dir_filtered_ordered_foreach(
          "/sys/class/drm",
          is_drm_connector,      // filter function
          NULL,                  // ordering function
          add_one_drm_connector_name,
          connector_names,    // accumulator, GPtrArray *
          -1);
    DBGTRC_DONE(debug, DDCA_TRC_I2C, "size of sys_drm_connectors: %d", sys_drm_connectors->len);
    return connector_names;
 }


 /** Do card-connector directories exist?
  *
  *  @return true/false
  */
 bool drm_connectors_exist() {
    bool debug = false;
    DBGTRC_STARTING(debug, DDCA_TRC_I2C, "");
    GPtrArray * connector_names = get_drm_connector_names();
    bool result = (connector_names->len > 0);
    g_ptr_array_free(connector_names, true);
    DBGTRC_RET_BOOL(debug, DDCA_TRC_I2C, result, "");
    return result;
 }
#endif

//
// I2C bus / DRM connector associations supplied by the user,
// i.e. option --bus-drm-connector
//

 typedef struct {
    int    busno;
    char * drm_connector_name;
 } Busno_Connector_Table_Entry;

 static void free_busno_connector_table_entry (void * ptr) {
    Busno_Connector_Table_Entry * entry = (Busno_Connector_Table_Entry *) ptr;
    if (entry) {
       free(entry->drm_connector_name);  // ok if null
    }
    free(entry);
 }

 static GPtrArray * user_busno_connector_table;


/** Returns the DRM connector name that the user has associated with an I2C bus
 *  number, using option --bus-drm-connector.
 *
 *  @param  busno  I2C bus number
 *  @return connector name, NULL if the bus number is not in the table
 *
 *  @remark
 *  The returned value belongs to the table.  Do not free.
 */
const char * user_drm_connector_for_busno(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d, user_busno_connector_table=%p",
                                         busno, user_busno_connector_table);

   const char * result = NULL;
   if (user_busno_connector_table) {
      for (int ndx = 0; ndx < user_busno_connector_table->len; ndx++) {
         Busno_Connector_Table_Entry * entry = g_ptr_array_index(user_busno_connector_table, ndx);
         if (entry->busno == busno) {
            result = entry->drm_connector_name;
            break;
         }
      }
   }

   DBGTRC_RET_STRING(debug, DDCA_TRC_NONE, result, "");
   return result;
}


void add_busno_connector(int busno, const char * connector_name) {
   Busno_Connector_Table_Entry* entry = calloc(1, sizeof(Busno_Connector_Table_Entry));
   entry->busno = busno;
   entry->drm_connector_name = strdup(connector_name);
   if (!user_busno_connector_table)
      user_busno_connector_table = g_ptr_array_new_full(4, free_busno_connector_table_entry);
   g_ptr_array_add(user_busno_connector_table, entry);
}


void dbgrpt_busno_connector_table(int depth) {
   rpt_label(depth, "busno_connector_table contents:");
   if (user_busno_connector_table) {
      for (int ndx = 0; ndx < user_busno_connector_table->len; ndx++) {
         Busno_Connector_Table_Entry * entry = g_ptr_array_index(user_busno_connector_table, ndx);
         rpt_vstring(depth+1, "/dev/i2c-%d  -  %s",  entry->busno, entry->drm_connector_name);
      }
   }
   else
      rpt_label(depth+1, "Empty");
}


/** Module initialization */
void init_i2c_bus_sysfs() {
#ifdef UNUSED
   RTTI_ADD_FUNC(add_one_drm_connector_name);
   RTTI_ADD_FUNC(get_drm_connector_names);
   RTTI_ADD_FUNC(drm_connectors_exist);
#endif
   RTTI_ADD_FUNC(find_sys_drm_connector_by_busno_or_edid);
   RTTI_ADD_FUNC(get_connector_edid);
   RTTI_ADD_FUNC(get_parsed_edid_for_businfo_using_sysfs);
   RTTI_ADD_FUNC(is_adapter_class_display_controller);
   RTTI_ADD_FUNC(is_valid_drm_connector_name);
   RTTI_ADD_FUNC(user_drm_connector_for_busno);
}
