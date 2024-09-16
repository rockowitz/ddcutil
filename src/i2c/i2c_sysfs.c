/** @file i2c_sysfs.c
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
#include "i2c/i2c_sysfs.h"


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
   if (cbn->name)
      cur->name = strdup(cbn->name);
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




#ifdef OLD
/** Gets a list of all displays known to DRM.
 *
 *  \param sysfs_drm_cards
 *  \bool  verbose
 *  \return GPtrArray of connector names for DRM displays
 *
 *  \remark
 *  The caller is responsible for freeing the returned #GPtrArray.
 */
GPtrArray * get_sysfs_drm_displays_old(Byte_Bit_Flags sysfs_drm_cards, bool verbose)
{
   bool debug = false;
   int  depth = 0;
   int  d1    = depth+1;
   int  d2    = depth+2;

   struct dirent *dent;
   DIR           *dir1;
   char          *dname;
   char          dnbuf[90];
   const int     cardname_sz = 20;
   char          cardname[cardname_sz];

   GPtrArray * connected_displays = g_ptr_array_new();
   g_ptr_array_set_free_func(connected_displays, g_free);

#ifdef TARGET_BSD
   dname = "/compat/linux/sys/class/drm";
#else
   dname = "/sys/class/drm";
#endif
   DBGTRC_STARTING(debug, TRACE_GROUP, "Examining %s...", dname);
   Byte_Bit_Flags iter = bbf_iter_new(sysfs_drm_cards);
   int cardno = -1;
   while ( (cardno = bbf_iter_next(iter)) >= 0) {
      snprintf(cardname, cardname_sz, "card%d", cardno);
      snprintf(dnbuf, 80, "%s/%s", dname, cardname);
      dir1 = opendir(dnbuf);
      DBGMSF(debug, "dnbuf=%s", dnbuf);
      if (!dir1) {
         // rpt_vstring(d1, "Unable to open sysfs directory %s: %s\n", dnbuf, strerror(errno));
         break;
      }
      else {
         while ((dent = readdir(dir1)) != NULL) {
            // DBGMSG("%s", dent->d_name);
            // char cur_fn[100];
            if (str_starts_with(dent->d_name, cardname)) {
               if (verbose)
                  rpt_vstring(d1, "Found connector: %s", dent->d_name);
               char cur_dir_name[PATH_MAX];
               g_snprintf(cur_dir_name, PATH_MAX, "%s/%s", dnbuf, dent->d_name);
               char * s_status = read_sysfs_attr(cur_dir_name, "status", false);
               // rpt_vstring(d2, "%s/status: %s", cur_dir_name, s_status);
               if (verbose)
                  rpt_vstring(d2, "Display: %s, status=%s", dent->d_name, s_status);
               // edid present iff status == "connected"
               if (streq(s_status, "connected")) {
                  if (verbose) {
                     GByteArray * gba_edid = read_binary_sysfs_attr(
                           cur_dir_name, "edid", 128, /*verbose=*/ true);
                     if (gba_edid) {
                        rpt_vstring(d2, "%s/edid:", cur_dir_name);
                        rpt_hex_dump(gba_edid->data, gba_edid->len, d2);
                        g_byte_array_free(gba_edid, true);
                     }
                     else {
                        rpt_vstring(d2, "Reading %s/edid failed.", cur_dir_name);
                     }
                  }

                  g_ptr_array_add(connected_displays, g_strdup(dent->d_name));
               }
               free(s_status);
               if (verbose)
                  rpt_nl();
            }
         }
         closedir(dir1);
      }
   }
   bbf_iter_free(iter);
   g_ptr_array_sort(connected_displays, gaux_ptr_scomp);
   DBGTRC_DONE(debug, TRACE_GROUP, "Connected displays: %s",
                              join_string_g_ptr_array_t(connected_displays, ", "));
   return connected_displays;
}
#endif


#ifdef OLD_HOTPLUG_VERSION
/** Examines a single connector, e.g. card0-HDMI-1, in a directory /sys/class/drm/cardN
 *  to determine if it is has a monitor connected.  If so, appends the simple
 *  connector name to the list of active connectors
 *
 *  @param  dirname    directory to examine, <device>/drm/cardN
 *  @param  simple_fn  filename to examine
 *  @param  data       GPtrArray of connected monitors
 *  @param  depth      if >= 0, emits a report with this logical indentation depth
 *
 *  @remark
 *  Move get_sysfs_drm_examine_one_connector(), get_sysfs_drm_displays()
 * to sysfs_i2c_util.c?
 */
static
void get_sysfs_drm_examine_one_connector(
      const char * dirname,     // <device>/drm/cardN
      const char * simple_fn,   // card0-HDMI-1 etc
      void *       data,        // GPtrArray collecting connector names
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);
   GPtrArray * connected_displays = (GPtrArray *) data;

   char * status = NULL;
   bool found_status = RPT_ATTR_TEXT(-1, &status, dirname, simple_fn, "status");
   if (found_status && streq(status,"connected")) {
         g_ptr_array_add(connected_displays, g_strdup(simple_fn));
      }
   g_free(status);

   DBGMSF(debug, "Added connector %s", simple_fn);
}


/**Checks /sys/class/drm for connectors with active displays.
 *
 * @return newly allocated GPtrArray of DRM connector names, sorted
 */
static
GPtrArray * get_sysfs_drm_displays() {
   bool debug = false;
   char * dname =
 #ifdef TARGET_BSD
              "/compat/linux/sys/class/drm";
 #else
              "/sys/class/drm";
 #endif
   DBGTRC_STARTING(debug, TRACE_GROUP, "Examining %s", dname);
   GPtrArray * connected_displays = g_ptr_array_new_with_free_func(g_free);
   dir_filtered_ordered_foreach(
                 dname,
                 is_card_connector_dir,   // filter function
                 NULL,                    // ordering function
                 get_sysfs_drm_examine_one_connector,
                 connected_displays,      // accumulator
                 0);
   g_ptr_array_sort(connected_displays, gaux_ptr_scomp);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning Connected displays: %s",
                              join_string_g_ptr_array_t(connected_displays, ", "));
   return connected_displays;
 }
#endif



 /** Adds a single connector name, e.g. card0-HDMI-1, to the accumulated
  *  list of all connections, and if the connector has a valid EDID, to
  *  the accumlated list of connectors having a valid EDID.
  *
  *  @param  dirname    directory to examine, <device>/drm/cardN
  *  @param  simple_fn  filename to examine
  *  @param  data       pointer to Sysfs_Connector_Names instance
  *  @param  depth      if >= 0, emits a report with this logical indentation depth
  */
static
void get_sysfs_drm_add_one_connector_name(
      const char * dirname,     // <device>/drm/cardN
      const char * simple_fn,   // card0-HDMI-1 etc
      void *       data,        // pointer to Sysfs_Connector_Names collecting connector names
      int          depth)
{
   bool debug = false;
   Sysfs_Connector_Names * accum = (Sysfs_Connector_Names*) data;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);

   g_ptr_array_add(accum->all_connectors, strdup(simple_fn));
   bool collect = GET_ATTR_EDID(NULL, dirname, simple_fn, "edid");
   if (collect) {
      g_ptr_array_add(accum->connectors_having_edid, g_strdup(simple_fn));
      DBGMSF(debug, "Added connector %s", simple_fn);
   }
   DBGMSF(debug, "Connector %s has edid = %s", simple_fn, SBOOL(collect));
}


/**Checks /sys/class/drm for connectors.
 *
 * @return struct Sysfs_Connector_Names
 *
 * @remark
 * Note the result is returned on the stack, not the heap
 */
Sysfs_Connector_Names get_sysfs_drm_connector_names() {
   bool debug = false;
   char * dname =
 #ifdef TARGET_BSD
              "/compat/linux/sys/class/drm";
 #else
              "/sys/class/drm";
 #endif
   DBGTRC_STARTING(debug, TRACE_GROUP, "Examining %s", dname);

   Sysfs_Connector_Names connector_names = {NULL, NULL};
   connector_names.all_connectors = g_ptr_array_new_with_free_func(g_free);
   connector_names.connectors_having_edid = g_ptr_array_new_with_free_func(g_free);

   dir_filtered_ordered_foreach(
                 dname,
                 is_card_connector_dir,   // filter function
                 NULL,                    // ordering function
                 get_sysfs_drm_add_one_connector_name,
                 &connector_names,      // accumulator
                 0);
   g_ptr_array_sort(connector_names.all_connectors, gaux_ptr_scomp);
   g_ptr_array_sort(connector_names.connectors_having_edid, gaux_ptr_scomp);

   DBGTRC_RET_STRUCT_VALUE(debug, DDCA_TRC_NONE, Sysfs_Connector_Names,
                                  dbgrpt_sysfs_connector_names, connector_names);
   return connector_names;
 }


/** Tests if two Sysfs_Connector_Names instances have the same lists
 *  for all connectors and for connectors having a valid EDID
 *
 *  @param cn1  first  instance
 *  @param cn2  second instance
 *  @return     true if the arrays in each instance contain the same connector names
 */
bool sysfs_connector_names_equal(Sysfs_Connector_Names cn1, Sysfs_Connector_Names cn2) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");
   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cn1 = %p:", cn1);
      dbgrpt_sysfs_connector_names(cn1, 1);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cn2 = %p:", cn2);
      dbgrpt_sysfs_connector_names(cn2, 1);
   }

   bool result = gaux_unique_string_ptr_arrays_equal(cn1.all_connectors,
                                                     cn2.all_connectors);
   result &= gaux_unique_string_ptr_arrays_equal(cn1.connectors_having_edid,
                                                 cn2.connectors_having_edid);

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "");
   return result;
}


/** Emit a debugging report of a #Sysfs_Connector_Names instance.
 *
 *  @param connector_names   Sysfs_Connector_Names instance, not a pointer
 *  @param depth             logical indentation depth
 */
void dbgrpt_sysfs_connector_names(Sysfs_Connector_Names connector_names, int depth) {
   rpt_vstring(depth, "all_connectors         @%p: %s", connector_names.all_connectors,
                      join_string_g_ptr_array_t(connector_names.all_connectors,         ", ") );
   rpt_vstring(depth, "connectors_having_edid @%p: %s", connector_names.connectors_having_edid,
                      join_string_g_ptr_array_t(connector_names.connectors_having_edid, ", ") );

   #ifdef FOR_DEBUGGING
   rpt_vstring(depth, "all_connectors         @%p:", connector_names.all_connectors);
   rpt_vstring(depth+3, "%s", join_string_g_ptr_array_t(connector_names.all_connectors,         ", ") );
   rpt_vstring(depth, "connectors_having_edid         @%p:", connector_names.connectors_having_edid);
   rpt_vstring(depth+3, "%s", join_string_g_ptr_array_t(connector_names.connectors_having_edid,         ", ") );
#endif
}


void free_sysfs_connector_names_contents(Sysfs_Connector_Names names_struct) {
   if (names_struct.all_connectors) {
      g_ptr_array_free(names_struct.all_connectors, true);
      names_struct.all_connectors = NULL;
   }
   if (names_struct.connectors_having_edid) {
      g_ptr_array_free(names_struct.connectors_having_edid, true);
      names_struct.connectors_having_edid = NULL;
   }
}


Sysfs_Connector_Names copy_sysfs_connector_names_struct(Sysfs_Connector_Names original) {
   Sysfs_Connector_Names result = {NULL, NULL};
   result.all_connectors = gaux_deep_copy_string_array(original.all_connectors);
   result.connectors_having_edid = gaux_deep_copy_string_array(original.connectors_having_edid);
   return result;
}


#ifdef OUT
// Wrong!  On amdgpu, for DP device realpath is connector with EDID, for HDMI and DVI device is adapter
char * find_sysfs_drm_connector_name_by_busno(GPtrArray* connector_names, int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "connector_names=%p, busno=%d", connector_names, busno);

   char * result = NULL;
   char buf[80];
   g_snprintf(buf, 80, "/sys/bus/i2c/devices/i2c-%d/device", busno);
   DBGMSF(debug, "buf = |%s|", buf);
   char * fq_connector = realpath(buf,NULL);
   DBGMSF(debug, "fq_connector = |%s|", fq_connector);
   if (fq_connector) {
      result = g_path_get_basename(fq_connector);
      free(fq_connector);
   }

   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning %s", result);
   return result;
}
#endif


/** Searches connectors for one with matching EDID
 *
 *  @param  connector_names  array of connector names
 *  @param  edid             pointer to 128 byte EDID
 *  @return name of connector with matching EDID (caller must free)
 */
char * find_sysfs_drm_connector_name_by_edid(GPtrArray* connector_names, Byte * edid) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "edid=%p", edid);

   char * result = NULL;
   for (int ndx = 0; ndx < connector_names->len; ndx++) {
      char * connector_name = g_ptr_array_index(connector_names, ndx);
      GByteArray * sysfs_edid;
      int depth = (debug) ? 1 : -1;
      RPT_ATTR_EDID(depth, &sysfs_edid, "/sys/class/drm", connector_name, "edid");
      if (sysfs_edid) {
         if (sysfs_edid->len >= 128 && memcmp(sysfs_edid->data, edid, 128) == 0)
            result = g_strdup(connector_name);
         g_byte_array_free(sysfs_edid, true);
         if (result)
            break;
      }
   }

   DBGTRC_RETURNING(debug, DDCA_TRC_I2C, result, "");
   return result;
}


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

   // other
   RTTI_ADD_FUNC(find_drm_connector_name_by_busno);
// RTTI_ADD_FUNC(find_sysfs_drm_connector_name_by_busno);
   RTTI_ADD_FUNC(find_sysfs_drm_connector_name_by_edid);
// RTTI_ADD_FUNC(init_sysfs_drm_connector_names);

   RTTI_ADD_FUNC(get_drm_connector_name_by_edid);
   RTTI_ADD_FUNC(find_drm_connector_name_by_busno);

   RTTI_ADD_FUNC(get_sys_drm_connector_name_by_connector_id);
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


