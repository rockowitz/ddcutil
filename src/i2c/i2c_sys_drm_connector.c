/** @file i2c_sys_drm_connector.c
 *
 *  Query /sys file system for information on I2C devices
 */

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

#include "i2c/i2c_sysfs_base.h"
#include <i2c/i2c_sys_drm_connector.h>


static const DDCA_Trace_Group  TRACE_GROUP = DDCA_TRC_NONE;

//
//  *** Scan /sys by drm connector - uses struct Sys_Drm_Connector ***
//

// from query_sysenv_sysfs
// 9/28/2021 Requires hardening, testing on other than amdgpu, MST etc

GPtrArray * sys_drm_connectors = NULL;  // Sys_Drm_Connector
bool all_drm_connectors_have_connector_id = false;

/** Frees a Sys_Drm_Connector instance
 *
 *  @param pointer to instance to free
 */
void free_sys_drm_connector(void * display) {
   bool debug = false;
   DBGMSF(debug, "Starting. display=%p", display);
   if (display) {
      Sys_Drm_Connector * disp = display;
      free(disp->connector_name);
      free(disp->connector_path);
      free(disp->name);
      // free(disp->dev);
      free(disp->ddc_dir_path);
      free(disp->base_name);
      free(disp->base_dev);
      free(disp->edid_bytes);
      free(disp->enabled);
      free(disp->status);
      free(disp);
   }
   DBGMSF(debug, "Done.");
}


/** Frees the persistent GPtrArray of #Sys_Drm_Connector instances pointed
 *  to by global sys_drm_connectors.
 */
void free_sys_drm_connectors() {
   if (sys_drm_connectors)
      g_ptr_array_free(sys_drm_connectors, true);
   sys_drm_connectors = NULL;
}


/** Reports the contents of one #Sys_Drm_Connector instance
 *
 *  @param cur            pointer to instance
 *  @param detailed_edid  if false, show only edid summary
 *  @param depth          logical indentation depth
 */
void report_one_sys_drm_connector(Sys_Drm_Connector * cur, bool detailed_edid, int depth)
{
   int d0 = depth;
   int d1 = depth+1;

   rpt_vstring(d0, "Connector:    %s", cur->connector_name);
   rpt_vstring(d1, "i2c_busno:    %d", cur->i2c_busno);
   rpt_vstring(d1, "connector_id: %d", cur->connector_id);
   rpt_vstring(d1, "name:         %s", cur->name);
   // rpt_vstring(d1, "dev:          %s", cur->dev);
   rpt_vstring(d1, "enabled:      %s", cur->enabled);
   rpt_vstring(d1, "status:       %s", cur->status);

   if (cur->is_aux_channel) {
      rpt_vstring(d1, "base_busno:   %d", cur->base_busno);
      rpt_vstring(d1, "base_name:    %s", cur->base_name);
      rpt_vstring(d1, "base dev:     %s", cur->base_dev);
   }

   if (cur->edid_size > 0) {
      if (detailed_edid) {
         rpt_label(d1,   "edid:");
         rpt_hex_dump(cur->edid_bytes, cur->edid_size, d1);
      }
      else {
         Parsed_Edid * edid = create_parsed_edid(cur->edid_bytes);
         if (edid) {
            rpt_vstring(d1, "edid:        %s, %s, %s",
                  edid->mfg_id, edid->model_name, edid->serial_ascii);
            free_parsed_edid(edid);
         }
         else
            rpt_label(d1, "edid:              invalid");
      }
   }
   else
      rpt_label(d1,      "edid:         None");
}


/** Returns a #Sys_Drm_Connector object for a single connector directory of
 *  /sys/class/drm.  It reads the directory itself instead of using the
 *  #sys_drm_connectors array.
 *
 *  @oaram  connector_name   e.g. card2-DP-3
 *  @param  depth            logical indentation depth, output connector report if >= 0
 *  @return Sys_Drm_Connector object for the connector
 */
Sys_Drm_Connector * get_drm_connector(const char * fn, int depth) {
   return one_drm_connector0("/sys/class/drm", fn, depth);
}


/** Scans a single connector directory of /sys/class/drm.
 *
 *  Has typedef Dir_Foreach_Func
 */
void one_drm_connector(
      const char *  dirname,      // /sys/class/drm
      const char *  fn,           // e.g. card0-DP-1
      void *        accumulator,
      int           depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dirname=%s, fn=%s, depth=%d", dirname, fn, depth);

   Sys_Drm_Connector * cur = one_drm_connector0(dirname, fn, depth);
   GPtrArray * drm_displays = accumulator;
   if (cur)
      g_ptr_array_add(drm_displays, cur);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


// simplified variant
Sys_Drm_Connector * one_drm_connector0(
      const char *  dirname,      // /sys/class/drm
      const char *  fn,           // e.g. card0-DP-1
      int           depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dirname=%s, fn=%s, depth=%d", dirname, fn, depth);
   int d0 = depth;
   if (depth < 0 && (IS_DBGTRC(debug, TRACE_GROUP)))
      d0 = 2;

   Sys_Drm_Connector * cur = calloc(1, sizeof(Sys_Drm_Connector));
   cur->i2c_busno = -1;      // 0 is valid bus number
   cur->base_busno = -1;
   cur->connector_id = -1;
   cur->connector_name = g_strdup(fn);   // e.g. card0-DP-1
   RPT_ATTR_INT(     d0, &cur->connector_id, dirname, fn, "connector_id");
   RPT_ATTR_REALPATH(d0, &cur->connector_path, dirname, fn);

   GByteArray * edid_byte_array = NULL;
   RPT_ATTR_EDID(d0, &edid_byte_array, dirname, fn, "edid");   // e.g. /sys/class/drm/card0-DP-1/edid
   // DBGMSG("edid_byte_array=%p", (void*)edid_byte_array);
   if (edid_byte_array) {
     cur->edid_size = edid_byte_array->len;
     cur->edid_bytes = g_byte_array_free(edid_byte_array, false);
     // DBGMSG("Setting cur->edid_bytes = %p", (void*)cur->edid_bytes);
   }

   Connector_Bus_Numbers * cbn = calloc(1, sizeof(Connector_Bus_Numbers));
   get_connector_bus_numbers(dirname, fn, cbn);
   cur->base_busno = cbn->base_busno;
   cur->i2c_busno = cbn->i2c_busno;
   cur->connector_id = cbn->connector_id;
   free_connector_bus_numbers(cbn);
   RPT_ATTR_TEXT(d0, &cur->enabled, dirname, fn, "enabled");   // e.g. /sys/class/drm/card0-DP-1/enabled
   RPT_ATTR_TEXT(d0, &cur->status,  dirname, fn, "status"); // e.g. /sys/class/drm/card0-DP-1/status

   if (depth >= 0)
      rpt_nl();
   DBGTRC_DONE(debug, TRACE_GROUP, "");
   return cur;
}


/** Collects information from all connector subdirectories of /sys/class/drm,
 *  optionally emitting a report.
 *
 *  @param  depth  logical indentation depth, if < 0 do not emit report
 *  @return array of #Sys_Drm_Connector structs, one for each connector found
 *
 *  Returns GPtrArray with 0 entries if no DRM displays found
 *
 *  Also sets global #sys_drm_connectors
 */
GPtrArray * scan_sys_drm_connectors(int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "depth=%d", depth);
   if (depth < 0 && (IS_DBGTRC(debug, TRACE_GROUP)))
      depth = 1;

   GPtrArray * sys_drm_connectors = g_ptr_array_new_with_free_func(free_sys_drm_connector);
   dir_filtered_ordered_foreach(
         "/sys/class/drm",
         is_drm_connector,      // filter function
         NULL,                  // ordering function
         one_drm_connector,
         sys_drm_connectors,    // accumulator, GPtrArray *
         depth);
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "size of sys_drm_connectors: %d", sys_drm_connectors->len);
   return sys_drm_connectors;
}


/** Gets the value of global #sys_drm_connectors.
 *
 *  @param rescan free the existing data structure
 *
 *  If sys_drm_connectors == NULL or rescan was set,
 *  scan the /sys/class/drm/<connector> directories
 */
GPtrArray* get_sys_drm_connectors(bool rescan) {
   if (sys_drm_connectors && rescan) {
      g_ptr_array_free(sys_drm_connectors, true);
      sys_drm_connectors = NULL;
   }
   if (!sys_drm_connectors)
      sys_drm_connectors = scan_sys_drm_connectors(-1);
   return sys_drm_connectors;
}


static gint
sort_sys_drm_connectors (gconstpointer a, gconstpointer b)
{
  const Sys_Drm_Connector *entry1 = *((Sys_Drm_Connector **) a);
  const Sys_Drm_Connector *entry2 = *((Sys_Drm_Connector **) b);

  return  sys_drm_connector_name_cmp0(entry1->connector_name, entry2->connector_name);
}


/** Reports the contents of the array of #Sys_Drm_Connector instances
 *  pointed to by global #sys_drm_connectors. If global #sys_drm_connectors
 *  is NULL, scan the /sys/class/drm/<connecter> tree.
 *
 *  @param verbose  if true, show full edid information
 *  @param depth    logical indentation depth
 */
void report_sys_drm_connectors(bool verbose, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "depth=%d", depth);
   int d0 = depth;
   int d1 = (debug) ? 2 : -1;
   rpt_nl();
   rpt_label(d0, "Display connectors reported by /sys:");
   if (!sys_drm_connectors)
     sys_drm_connectors = scan_sys_drm_connectors(d1);
   GPtrArray * displays = sys_drm_connectors;
   if (!displays || displays->len == 0) {
      rpt_label(d1, "None");
   }
   else {
      g_ptr_array_sort(displays, sort_sys_drm_connectors);
      for (int ndx = 0; ndx < displays->len; ndx++) {
         Sys_Drm_Connector * cur = g_ptr_array_index(displays, ndx);
         report_one_sys_drm_connector(cur, verbose, depth);
         rpt_nl();
      }
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


// TODO: Create version that directly examines each connector directory

bool all_sys_drm_connectors_have_connector_id(bool rescan) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "rescan=%s", SBOOL(rescan));
   GPtrArray * connectors = get_sys_drm_connectors(rescan);
   bool result = true;
   for (int ndx = 0; ndx < connectors->len; ndx++) {
      Sys_Drm_Connector * conn = g_ptr_array_index(connectors, ndx);
      if (conn->connector_id < 0)
         result = false;
   }
   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "");
   return result;
}


Bit_Set_256 buses_having_edid_from_sys_drm_connectors(bool rescan) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "rescan=%s", SBOOL(rescan));
   GPtrArray * connectors = get_sys_drm_connectors(rescan);
   Bit_Set_256 result = EMPTY_BIT_SET_256;
   for (int ndx = 0; ndx < connectors->len; ndx++) {
      Sys_Drm_Connector * conn = g_ptr_array_index(connectors, ndx);
      if (conn->edid_bytes)
         result = bs256_insert(result, conn->i2c_busno);
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning; %s", bs256_to_string_decimal_t(result, "", ","));
   return result;
}


/** Find a #Sys_Drm_Connector instance using one of the I2C bus number,
 *  EDID value, or DRM connector name.
 *
 *  @param  busno   I2C bus number
 *  @param  edid    pointer to 128 byte EDID
 *  @param  connector_name  e.g. card0-HDMI-A-1
 *
 *  Scans /sys/class/drm if global #sys_class_drm not already set
 */
Sys_Drm_Connector *
find_sys_drm_connector(int busno, Byte * edid, const char * connector_name) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "busno=%d, edid=%p, connector_name=%s",
                                        busno, (void*)edid, connector_name);
   if (!sys_drm_connectors)
     sys_drm_connectors = scan_sys_drm_connectors(-1);
   assert(sys_drm_connectors);
   Sys_Drm_Connector * result = NULL;
   for (int ndx = 0; ndx < sys_drm_connectors->len; ndx++) {
      Sys_Drm_Connector * cur = g_ptr_array_index(sys_drm_connectors, ndx);
      // DBGMSG("cur->busno = %d", cur->i2c_busno);
      if (busno >= 0 && cur->i2c_busno == busno) {
         DBGTRC(debug, DDCA_TRC_NONE, "Matched by bus number");
         result = cur;
         break;
      }
      if (edid && cur->edid_size >= 128 && (memcmp(edid, cur->edid_bytes,128) == 0)) {
         DBGTRC(debug, DDCA_TRC_NONE, "Matched by edid");
         result = cur;
         break;
      }
      if (connector_name && streq(connector_name, cur->connector_name)) {
         DBGTRC(debug, DDCA_TRC_NONE, "Matched by connector_name");
         result = cur;
         break;
      }
   }
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p", (void*) result);
   return result;
}


Sys_Drm_Connector *
find_sys_drm_connector_by_connector_id(int connector_id) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "connector_id=%d", connector_id);
   if (!sys_drm_connectors)
     sys_drm_connectors = scan_sys_drm_connectors(-1);
   assert(sys_drm_connectors);
   Sys_Drm_Connector * result = NULL;
   for (int ndx = 0; ndx < sys_drm_connectors->len; ndx++) {
      Sys_Drm_Connector * cur = g_ptr_array_index(sys_drm_connectors, ndx);
      if (cur->connector_id < 0)  // driver does not set connector number, need only check once
         break;
      if (cur->connector_id == connector_id) {
         DBGTRC(debug, DDCA_TRC_NONE, "Matched");
         result = cur;
         break;
      }
   }
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p", (void*) result);
   return result;
}


Sys_Drm_Connector *
find_sys_drm_connector_by_connector_identifier(Drm_Connector_Identifier dci) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "dci = %s",  dci_repr_t(dci));
   if (!sys_drm_connectors)
     sys_drm_connectors = scan_sys_drm_connectors(-1);
   assert(sys_drm_connectors);
   Sys_Drm_Connector * result = NULL;
   for (int ndx = 0; ndx < sys_drm_connectors->len; ndx++) {
      Sys_Drm_Connector * cur = g_ptr_array_index(sys_drm_connectors, ndx);
      Drm_Connector_Identifier cur_dci = parse_sys_drm_connector_name(cur->connector_name);
      if (dci_eq(dci, cur_dci)) {
         result = cur;
         break;
      }
   }
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p", (void*) result);
   return result;
}


int sys_drm_get_busno_by_connector_name(const char * connector_name) {
   int result = -1;
   Sys_Drm_Connector * sdc = find_sys_drm_connector(-1, NULL, connector_name);
   if (sdc)
      result = sdc->i2c_busno;
   return result;
}


/** Searches for a Sys_Drm_Connector instance by I2C bus number.
 *
 *  @param  I2C bus number
 *  @return pointer to instance, NULL if not found
 */
Sys_Drm_Connector *
find_sys_drm_connector_by_busno(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "busno=%d", busno);
   Sys_Drm_Connector * result = find_sys_drm_connector(busno, NULL, NULL);
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p: %s",
         result, (result) ? result->connector_name : "NOT FOUND");
   return result;
}


/** If the display has an open-source conformant driver,
 *  returns the connector name.
 *
 *  If the display has a DRM driver that doesn't conform
 *  to the standard (I'm looking at you, Nvidia), or it
 *  is not a DRM driver, returns NULL.
 *
 *  @param busno
 *  @return connector name, caller must free
 */
char * find_drm_connector_name_by_busno(int busno) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. busno = %d", busno);
   char * result = NULL;
   Sys_Drm_Connector * drm_connector = find_sys_drm_connector_by_busno(busno);
   if (drm_connector) {
      result = g_strdup(drm_connector->connector_name);
   }
   DBGTRC_RETURNING(debug, TRACE_GROUP, result, "");
   return result;
}


/** Searches for a Sys_Drm_Connector instance by EDID.
 *
 *  @param  pointer to 128 byte EDID value
 *  @return pointer to instance, NULL if not found
 */
Sys_Drm_Connector * find_sys_drm_connector_by_edid(Byte * raw_edid) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "edid=%p", (void*) raw_edid);
   Sys_Drm_Connector * result = find_sys_drm_connector(-1, raw_edid, NULL);
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p", (void*) result);
   return result;
}


/** Gets the DRM connector name, e.g. card0-DP-3, using the EDID.
 *
 *  @param  edid_bytes  pointer to 128 byte EDID
 *  @return connector name, NULL if not found
 */
char * get_drm_connector_name_by_edid(Byte * edid_bytes) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Finding connector by EDID...");
   char * result = NULL;
   Sys_Drm_Connector * connector_rec = find_sys_drm_connector_by_edid(edid_bytes);
   if (connector_rec) {
      result = g_strdup(connector_rec->connector_name);
   }
   DBGTRC_RETURNING(debug, TRACE_GROUP, result, "");
   return result;
}


/** Searches for a Sys_Drm_Connector instance by the connector name
 *
 *  @param  connector name
 *  @return pointer to instance, NULL if not found
 */
Sys_Drm_Connector * find_sys_drm_connector_by_connector_name(const char * name) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "name=|%s|", name);
   Sys_Drm_Connector * result = find_sys_drm_connector(-1, NULL, name);
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %p", (void*) result);
   return result;
}

//
// End of Sys_Drm_Connector section
//



//
// *** DRM Checks ***
//


#ifdef INVALID
// adapter_path and adapter_class not set for nvidia driver

/** Uses the Sys_I2C_Info array to get a list of all video adapters
 *  and checks if each supports DRM.
 *
 *  @param rescan  always recreate the Sys_I2C_Info array
 *  @return true if all display adapters support DRM, false if not
 */
bool all_sysfs_i2c_info_drm(bool rescan) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "rescan=%s", SBOOL(rescan));
   bool result = false;
#ifdef USE_LIBDRM
   GPtrArray* all_info = get_all_sysfs_i2c_info(rescan, -1);
   GPtrArray* adapter_paths = g_ptr_array_sized_new(4);
   g_ptr_array_set_free_func(adapter_paths, g_free);
   if (all_info->len > 0) {
      for (int ndx = 0; ndx < all_info->len; ndx++) {
         Sysfs_I2C_Info * info = g_ptr_array_index(all_info, ndx);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, adapter_class=%s, adapter_path=%s",
               info->busno, info->adapter_class, info->adapter_path);
         if (str_starts_with(info->adapter_class, "0x03")) {
               g_ptr_array_add(adapter_paths, info->adapter_path);
         }
      }
      result = all_video_adapters_support_drm_using_drm_api(adapter_paths);
   }
   g_ptr_array_free(adapter_paths, false);
#endif
   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "");
   return result;
}
#endif






#ifdef WRONG
// directory drm always exists, need to check if it has connector nodes

/** Checks that all video devices have DRM drivers.
 *
 *  @return true/false
 */
bool i2c_all_video_devices_drm() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   GPtrArray * video_devices = get_sys_video_devices();
   bool all_devices_drm = true;
   for (int ndx = 0; ndx < video_devices->len; ndx++) {
      char * device_path = g_ptr_array_index(video_devices, ndx);
      int d = IS_DBGTRC(debug,DDCA_TRC_NONE) ? 1 : -1;
      bool found_drm = RPT_ATTR_SINGLE_SUBDIR(d, NULL, fn_equal, "drm", device_path);
      if (!found_drm) {
         all_devices_drm = false;
         break;
      }
   }
   g_ptr_array_free(video_devices, true);

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, all_devices_drm, "");
   return all_devices_drm;
}
#endif


#ifdef UNUSED
/** If possible, determines the drm connector for an I2C bus number.
 *  If insufficient fields exist in sysfs to do this with absolute
 *  assurance, EDID comparison is used.
 *
 *  Fields drm_connector_name and drm_connector_found_by are set.
 *  If the drm connector cannot be determined, drm_connector_found_by
 *  is set to DRM_CONNECTOR_NOT_FOUND.
 *
 *  @param businfo   I2C_Bus_Info record
 *  @return pointer to Sys_Drm_Connector instance
 */
Sys_Drm_Connector * i2c_check_businfo_connector(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Checking I2C_Bus_Info for /dev/i2c-%d", businfo->busno);
   businfo->drm_connector_found_by = DRM_CONNECTOR_NOT_FOUND;
   Sys_Drm_Connector * drm_connector = find_sys_drm_connector_by_busno(businfo->busno);
   if (drm_connector) {
     businfo->drm_connector_found_by = DRM_CONNECTOR_FOUND_BY_BUSNO;
     businfo->drm_connector_name = g_strdup(drm_connector->connector_name);
     businfo->drm_connector_id = drm_connector->connector_id;
   }
   else if (businfo->edid) {
     drm_connector = find_sys_drm_connector_by_edid(businfo->edid->bytes);
     if (drm_connector) {
        businfo->drm_connector_name = g_strdup(drm_connector->connector_name);
        businfo->drm_connector_found_by = DRM_CONNECTOR_FOUND_BY_EDID;
        businfo->drm_connector_id = drm_connector->connector_id;
     }
   }
   businfo->flags |= I2C_BUS_DRM_CONNECTOR_CHECKED;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Final businfo flags: %s", i2c_interpret_bus_flags_t(businfo->flags));
   if (businfo->drm_connector_name)
      DBGTRC_DONE(debug, TRACE_GROUP, "Returning: SYS_Drm_Connector for %s", businfo->drm_connector_name);
   else
      DBGTRC_RETURNING(debug, TRACE_GROUP, NULL, "");
   return drm_connector;
}
#endif



/** Module initialization */
void init_i2c_sysfs() {

   // Sys_Drm_Connector
   RTTI_ADD_FUNC(one_drm_connector);
   RTTI_ADD_FUNC(scan_sys_drm_connectors);
   RTTI_ADD_FUNC(report_sys_drm_connectors);
   RTTI_ADD_FUNC(find_sys_drm_connector_by_busno);
   RTTI_ADD_FUNC(find_sys_drm_connector_by_connector_identifier);
   RTTI_ADD_FUNC(find_sys_drm_connector_by_connector_id);
   RTTI_ADD_FUNC(find_sys_drm_connector_by_edid);
   RTTI_ADD_FUNC(find_sys_drm_connector);
   RTTI_ADD_FUNC(find_drm_connector_name_by_busno);
}


#ifdef FOR_FUTURE_USE

Connector_Busno_Dref_Table * cbd_table = NULL;

void gdestroy_cbd(void * data) {
   Connector_Busno_Dref * cbd = data;
   free(cbd->connector);
}


Connector_Busno_Dref_Table * create_connector_busnfo_dref_table() {
   Connector_Busno_Dref_Table * cbdt = g_ptr_array_new_with_free_func(gdestroy_cbd);
   return cbdt;
}

Connector_Busno_Dref * new_cbd(const char * connector, int busno) {
   Connector_Busno_Dref * cbd = calloc(1, sizeof(Connector_Busno_Dref));
   cbd->connector = g_strdup(connector);
   cbd->busno = busno;
   return cbd;
}


Connector_Busno_Dref * new_cbd0(int busno) {
   Connector_Busno_Dref * cbd = calloc(1, sizeof(Connector_Busno_Dref));
   cbd->busno = busno;
   return cbd;
}

Connector_Busno_Dref * get_cbd_by_connector(const char * connector) {
   Connector_Busno_Dref * result = NULL;
   assert(cbd_table);
   for (int ndx = 0; ndx < cbd_table->len; ndx++) {
      Connector_Busno_Dref * cur = g_ptr_array_index(cbd_table, ndx);
      if (streq(connector, cur->connector)) {
         result = cur;
         break;
      }
   }
   return result;
}


// how to handle secondary busno's
Connector_Busno_Dref * get_cbd_by_busno(int busno) {
   Connector_Busno_Dref * result = NULL;
   assert(cbd_table);
   for (int ndx = 0; ndx < cbd_table->len; ndx++) {
      Connector_Busno_Dref * cur = g_ptr_array_index(cbd_table, ndx);
      if ( cur->busno == busno) {
         result = cur;
         break;
      }
   }
   return result;
}
// if dref != NULL, replaces, if NULL, just erases
void set_cbd_connector(Connector_Busno_Dref * cbd, Display_Ref * dref) {
   cbd->dref = dref;
}

void dbgrpt_cbd_table(Connector_Busno_Dref_Table * cbd_table, int depth) {
   rpt_structure_loc("cbd_table", cbd_table, depth);
   int d0 = depth;
   int d1 = depth+1;
   for (int ndx =  0; ndx < cbd_table->len; ndx++) {
      Connector_Busno_Dref* cur = g_ptr_array_index(cbd_table, ndx);
      rpt_structure_loc("Connector_Busno_Dref", cur, d0);
      rpt_vstring(d1, "connector:     %s", cur->connector);
      rpt_vstring(d1, "busno:         %s", cur->busno);
      rpt_vstring(d1, "dref:          %s", (cur->dref) ? dref_repr_t(cur->dref) : "NULL");

   }
}
#endif


