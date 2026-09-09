/** @file sysfs_base.c */

 // Copyright (C) 2020-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#include "config.h"

/** \cond */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/** \endcond */

#include "util/coredefs.h"
#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/drm_card_connector_util.h"
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

#include "sysfs_simple.h"
#include "sysfs_base.h"


static const DDCA_Trace_Group  TRACE_GROUP = DDCA_TRC_SYSFS;

//
// Globals
//

bool force_sysfs_unreliable = false;
bool force_sysfs_reliable = false;

//
// Predicate Functions
//

/** Tests if a filename has the form of an I2C slave device directory,
 *  i.e. D-00hh -- a bus number, a hyphen, and a 4 digit hex address,
 *  e.g. 4-0037, 4-0050.
 *
 *  Signature is that of a #Dir_Filter_Func, so **dirname** is accepted and
 *  ignored; only **simple_fn** is examined.
 *
 *  @param  dirname    directory containing the file, not used
 *  @param  simple_fn  filename to test
 *  @return true if the name matches, false if not
 */
bool is_n_nnnn(const char * dirname, const char * simple_fn) {
   bool result = predicate_any_D_00hh(simple_fn);
   DBGMSF(false,"dirname=%s. simple_fn=%s, returning %s", dirname, simple_fn, SBOOL(result));
   return result;
}


#ifdef NOT_NEEDED   // just set func arg to NULL
bool fn_any(const char * filename, const char * ignore) {
   DBGMSF(true, "filename=%s, ignore=%s: Returning true", filename, ignore);
   return true;
}
#endif


//
// *** Common Functions
//

#ifdef UNUSED
static
void add_video_device_to_array(
      const char * dirname,     //
      const char * fn,
      void *       data,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "dirname=%s, fn=%s", dirname, fn);
   GPtrArray* accumulator = (GPtrArray*) data;
   char * s = g_strdup_printf("%s/%s", dirname, fn);
   g_ptr_array_add(accumulator, s);
   // RPT_ATTR_TEXT(    1, NULL, dirname, fn, "class");
   // RPT_ATTR_REALPATH(1, NULL, dirname, fn, "driver");
}


/** Gets all sysfs devices with class video device, i.e. x03
 *
 *  @return array of fully qualified device paths
 */
GPtrArray * get_sys_video_devices() {
   bool debug = false;
   GPtrArray * video_devices = g_ptr_array_new_with_free_func(g_free);
   DBGTRC_STARTING(debug, TRACE_GROUP, "video_devices=%p", video_devices);

   dir_filtered_ordered_foreach("/sys/bus/pci/devices",
                       has_class_display,      // filter function
                       NULL,                    // ordering function
                       add_video_device_to_array,
                       video_devices,                    // accumulator
                       -1);
   DBGTRC_DONE(debug, TRACE_GROUP,"Returning array with %d video devices", video_devices->len);
   return video_devices;
}
#endif



//
// Driver inquiry functions
//


//
// Sysfs_Connector_Names functions
//

 /** Adds a single connector name, e.g. card0-HDMI-1, to the accumulated
  *  list of all connections, and if the connector has a valid EDID, to
  *  the accumulated list of connectors having a valid EDID.
  *
  *  @param  dirname    directory to examine, <device>/drm/cardN
  *  @param  simple_fn  filename to examine
  *  @param  data       pointer to Sysfs_Connector_Names instance
  *  @param  depth      if >= 0, emits a report with this logical indentation depth
  */
STATIC
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
   POSSIBLY_WRITE_DETECT_TO_STATUS_BY_CONNECTOR_NAME(simple_fn);
   bool collect = GET_ATTR_EDID(NULL, dirname, simple_fn, "edid");
   if (collect) {
      g_ptr_array_add(accum->connectors_having_edid, g_strdup(simple_fn));
      DBGMSF(debug, "Added connector %s", simple_fn);
   }
   DBGMSF(debug, "Connector %s has edid = %s", simple_fn, SBOOL(collect));
}


typedef struct {
   bool found;
} Found_Accumulator;

/** Reports whether one directory entry is a DRM connector directory,
 *  recording the fact in the accumulator.
 *
 *  Signature is that of a #Dir_Foreach_Terminatable_Func: returning true stops
 *  the traversal, so the first connector found ends the search.
 *
 *  @param  dirname      directory being scanned, e.g. /sys/class/drm
 *  @param  fn           entry to examine
 *  @param  accumulator  pointer to a Found_Accumulator, its **found** field set
 *                       to true if this entry is a connector directory
 *  @param  depth        logical indentation depth, unused
 *  @return true if this entry is a connector directory, i.e. stop scanning
 */
bool is_card_connector_dir1(
   const char *  dirname,
   const char *  fn,
   void *        accumulator,
   int           depth)
{
   bool debug = false;
   DBGF(debug, "dirname=%s, fn=%s, accumulator=%p", dirname, fn, accumulator);

   Found_Accumulator * accum = accumulator;
   bool found = is_drm_connector(dirname, fn);
   if (found)
      accum->found = true;

   DBGF(debug, "Returning %s", sbool(found));
   return found;
}


/** Checks whether /sys/class/drm contains any card connector directories.
 *
 *  A false result means the video driver does not expose connectors through
 *  sysfs at all, so nothing that reads connector attributes can work.
 *
 *  @return true if at least one connector directory exists, false if not
 *
 *  @remark
 *  The answer is determined once and cached in file static variables.  The set
 *  of connector directories does not change over the life of the process --
 *  individual connectors come and go, but not the driver's use of sysfs.
 */
bool sysfs_connector_directories_exist() {
   bool debug = false;
   static bool executed = false;
   static bool found = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "executed = %s", sbool(executed));

   if (!executed) {
      char * dirname = SYS"/class/drm";
      Found_Accumulator accumulator;
      accumulator.found = false;
      dir_foreach_terminatable(
            dirname,
            NULL,
            is_card_connector_dir1,
            &accumulator,
            (debug) ? 1 : -1);
      found = accumulator.found;
      executed = true;
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, found, "");
   return found;
}


/** Checks /sys/class/drm for connectors for the names of all
 *  DRM connectors, and and the names for those having an EDID.
 *
 *  @return struct Sysfs_Connector_Names
 *
 *  @remark
 *  Note the result is returned on the stack, not the heap
 */
Sysfs_Connector_Names get_sysfs_drm_connector_names() {
   bool debug = false;
   const char * dname = SYS"/class/drm";
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


/** Frees the two string arrays a #Sysfs_Connector_Names holds.
 *
 *  @param  names_struct  struct whose contents are to be freed
 *
 *  @remark
 *  Frees the contents, not the struct, which is passed and returned by value
 *  rather than allocated.  The caller's copy still holds the freed pointers
 *  afterward, so it must not be used again.
 */
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


/** Deep copies a #Sysfs_Connector_Names, duplicating both string arrays and
 *  every string in them.
 *
 *  @param  original  struct to copy
 *  @return copy, whose contents the caller frees using
 *          #free_sysfs_connector_names_contents()
 */
Sysfs_Connector_Names copy_sysfs_connector_names_struct(Sysfs_Connector_Names original) {
   Sysfs_Connector_Names result = {NULL, NULL};
   result.all_connectors = gaux_deep_copy_string_array(original.all_connectors);
   result.connectors_having_edid = gaux_deep_copy_string_array(original.connectors_having_edid);
   return result;
}

// End of Sysf_Connector_Names functions


// Note: On amdgpu, for DP device realpath is connector with EDID, for HDMI and DVI device is adapter


/** Searches connectors for one with matching EDID
 *
 *  @param  connector_names  array of connector names
 *  @param  edid             pointer to 128 byte EDID
 *  @return name of connector with matching EDID (caller must free)
 */
char * find_sysfs_drm_connector_name_by_edid(
            GPTRARRAY(char*) * connector_names,
            Byte * edid) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "edid=%p", edid);

   char * result = NULL;
   for (int ndx = 0; ndx < connector_names->len; ndx++) {
      char * connector_name = g_ptr_array_index(connector_names, ndx);
      GByteArray * sysfs_edid;
      int depth = (debug) ? 1 : -1;
      POSSIBLY_WRITE_DETECT_TO_STATUS_BY_CONNECTOR_NAME(connector_name);
      RPT_ATTR_EDID(depth, &sysfs_edid, "/sys/class/drm", connector_name, "edid");
      if (sysfs_edid) {
         if (sysfs_edid->len >= 128 && memcmp(sysfs_edid->data, edid, 128) == 0)
            result = g_strdup(connector_name);
         g_byte_array_free(sysfs_edid, true);
         if (result)
            break;
      }
   }

   DBGTRC_RET_STRING(debug, DDCA_TRC_I2C, result, "");
   return result;
}


/** Given a DRM connector name, returns the I2C bus number
 *
 *  First tries to get the bus number from /sys. If unsuccessful,
 *  scans all I2C_Bus_Info records.
 *
 *  @param  connector_name   DRM connector name
 *  @return I2C bus number, -1 if not found
 *
 *  @remark
 *  Does checking connector bus numbers first really gain anything?
 */

int search_all_businfo_records_by_connector_name(char *connector_name) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "connector_name = |%s|", connector_name);

   // reads connector dir directly, i.e. does not retrieve persistent data structure
   //  Sys_Drm_Connector * conn = get_drm_connector(connector_name, debug_depth);
   // int busno = conn->i2c_busno;
   // free(conn);
   Connector_Bus_Numbers *cbn = calloc(1, sizeof(Connector_Bus_Numbers));
   get_connector_bus_numbers("/sys/class/drm", connector_name, cbn);
   int busno = cbn->i2c_busno;
   free_connector_bus_numbers(cbn);
   if (busno < 0) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Examining businfo records...");
      // look through all businfo records for one with the connector name
      // Hold all_i2c_buses_mutex: the display watch thread may be adding
      // or removing entries concurrently.
      g_mutex_lock(&all_i2c_buses_mutex);
      for (int ndx = 0; ndx < all_i2c_buses->len; ndx++) {
         I2C_Bus_Info *businfo = g_ptr_array_index(all_i2c_buses, ndx);
         DBGMSG("Examining businfo record for bus %d, I2C_BUS_PROBED=%s, connector_found_by=%s",
               businfo->busno, sbool(businfo->flags & I2C_BUS_PROBED),
               drm_connector_found_by_name(businfo->drm_connector_found_by));
         // need to check if businfo record is valid?
         if (streq(businfo->drm_connector_name, connector_name)) {
            busno = businfo->busno;
            break;
         }
      }
      g_mutex_unlock(&all_i2c_buses_mutex);
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "returning busno %d", busno);
   return busno;
}



/* i915, amdgpu, radeon, nouveau and (likely) other video drivers that share
 * the kernel's DRM code can be relied on to maintain the edid, status, and
 * enabled attributes as displays are connected and disconnected.
 *
 * Unfortunately depending on version, the nvidia driver does not.
 * Attribute enabled is always "disabled".  It may be the case
 * that the edid value is that of the monitor initially connected.
 * What has been observed is that if the driver does change the
 * edid attribute, it also properly sets status to "connected" or
 * disconnected.  If it does not, status is always "disconnected",
 * whether or not a monitor is connected.
 */

static
bool known_reliable_driver(const char * driver) {
   return streq(driver, "i915")   ||
          streq(driver, "xe") ||
          streq(driver, "amdgpu") ||
          streq(driver, "radeon") ||
          streq(driver, "nouveau");
}

/** Reports whether a video driver can be relied on to keep the DRM connector
 *  attributes edid, status, and enabled current as displays are connected and
 *  disconnected.
 *
 *  @param  driver_name  driver name, e.g. i915
 *  @return true if the driver is reliable, false if not
 *
 *  @remark
 *  True for the drivers sharing the kernel's DRM implementation, and for any
 *  driver when the user has asserted --force-sysfs-reliable.  Notably false
 *  for nvidia.  See the comment block above #known_reliable_driver().
 */
bool is_driver_reliable(const char * driver_name) {
   bool debug = false;

   bool result = false;
   if (known_reliable_driver(driver_name))
   {
      result = true;
   }
   else {
      //if (streq(driver_name, "nvidia")) {
      if (force_sysfs_reliable)
         result = true;
   }

   DBGTRC_EXECUTED(debug, DDCA_TRC_NONE, "driverr_name=%s, returning %s", driver_name, sbool(result));
   return result;
}


/** Reports whether the DRM attributes of one connector can be relied on,
 *  by finding the driver of the adapter behind it and testing that.
 *
 *  @param  connector_name  DRM connector name, e.g. card1-DP-1
 *  @return true if the connector's driver is reliable, false if not
 */
bool is_connector_reliable(const char * connector_name) {
   bool debug = true;
   bool result = false;

   char buf[PATH_MAX];
   g_snprintf(buf, PATH_MAX, "/sys/ckass.drm/%s", connector_name);
   char * driver = find_adapter_and_get_driver(buf, -1);
   result = is_driver_reliable(driver);

   DBGTRC_EXECUTED(debug, DDCA_TRC_NONE, "connector_name=%s, returning %s", connector_name, sbool(result));
   return result;
}

typedef struct {
   bool     known_good_driver_seen;
   bool     nvidia_driver_seet;
   bool     other_driver_seen;
#ifdef OUT
   uint8_t  nvidia_connector_ct;
   uint8_t  nvidia_connector_w_edid_ct;
   uint8_t  nvidia_connector_w_edid_and_connected_ct;
#endif
} Sysfs_Reliability_Accumulator;



static bool drm_reliability_checked = false;
static bool other_drivers_seen = false;
// static bool nvidia_connectors_reliable = false;
static bool nvidia_connectors_exist = false;
static bool known_good_drivers_seen = false;



static
void check_connector_reliability(
            const char *  dirname,
            const char *  fn,
            void *        accumulator,
            int           depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dirname=|%s|, fn=|%s|", dirname, fn);
   int debug_depth = (debug) ? 1 : -1;

   // Sysfs_Reliability_Accumulator * accum = accumulator;

   char buf[PATH_MAX];
   g_snprintf(buf, PATH_MAX, "%s/%s", dirname, fn);
   char * driver = find_adapter_and_get_driver(buf, debug_depth);
#ifdef OLD
   if (is_driver_reliable(driver))
   {
      accum->known_good_driver_seen = true;
   }
   else if (streq(driver, "nvidia")) {
      // Per Michael Hamilton, testing that status == "connected" for any connector with EDID
      // does not guarantee that DRM connector is updated when a display is connected/disconnected
      accum->nvidia_connector_ct++;
      GByteArray * edid_byte_array = NULL;
      POSSIBLY_WRITE_DETECT_TO_STATUS_BY_CONNECTOR_NAME(fn);
      RPT_ATTR_EDID(debug_depth, &edid_byte_array, dirname, fn, "edid");   // e.g. /sys/class/drm/card0-DP-1/edid
      // DBGMSG("edid_byte_array=%p", (void*)edid_byte_array);
      if (edid_byte_array) {
         accum->nvidia_connector_w_edid_ct++;
         g_byte_array_free(edid_byte_array,true);

         char * status = NULL;
         RPT_ATTR_TEXT(debug_depth, &status,  dirname, fn, "status"); // e.g. /sys/class/drm/card0-DP-1/status
         if (status) {
            if (streq(status, "connected"))
               accum->nvidia_connector_w_edid_and_connected_ct++;
            free(status);
         }
      }
   }
#endif

   if (streq(driver, "nvidia")) {
      // accum->nvidia_driver_seet = true;
       nvidia_connectors_exist = true;
   }
   else if (is_driver_reliable(driver)) {
         // accum->known_good_driver_seen = true;
         known_good_drivers_seen = true;
   }
   else {
      // accum->other_driver_seen = true;
       other_drivers_seen = true;
   }


   free(driver);

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


// moved from sysfs_i2c_util.c:



static
void check_sysfs_reliability() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");

//    Sysfs_Reliability_Accumulator * accum = calloc(1, sizeof(Sysfs_Reliability_Accumulator));
   int depth=0;
   dir_foreach(
         "/sys/class/drm",
         predicate_cardN_connector,       // filter function
         check_connector_reliability,
         NULL, //      accum,
         depth);

   drm_reliability_checked = true;
#ifdef oud
   nvidia_connectors_exist = (accum->nvidia_connector_ct > 0);
   // known_good_driver_seen = > 0;
   // This appears to be a necessary, but not sufficient, condition
   nvidia_connectors_reliable =
         accum->nvidia_connector_w_edid_ct > 0 &&
         accum->nvidia_connector_w_edid_ct == accum->nvidia_connector_w_edid_and_connected_ct;
   other_drivers_seen = accum->other_driver_seen;
   free(accum);
#endif

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "nvidia_connectors_exist=%s",
         sbool(nvidia_connectors_exist));
}


/** Reports whether sysfs attributes for DRM connectors using the given video
 *  driver reliably reflect display connection and disconnection.
 *
 *  @param  driver  name of driver
 *  @return true if reliable, false if not
 */
bool is_sysfs_reliable_for_driver(const char * driver) {
   bool debug = false;

//    if (!drm_reliability_checked)
//       check_sysfs_reliability();

   bool result = false;
   // force_sysfs_unreliable, force_sysfs_reliable exist to facilitate testing
   if (force_sysfs_unreliable)
      result = false;
   else if (force_sysfs_reliable)
      result = true;

   else {
      if (streq(driver, "nvidia"))
         result = false;   // set in check_sysfs_reliable()
      else
         result = known_reliable_driver(driver);
   }

   DBGTRC_EXECUTED(debug, DDCA_TRC_NONE, "Returning %s, driver=%s", SBOOL(result), driver);
   return result;
}


/** Reports whether sysfs attributes for the DRM connector associated with an
 *  I2C bus number reliably reflect display connection and disconnection.
 *
 *  @param  busno  I2C bus number
 *  @return true if reliable, false if not
 */
bool is_sysfs_reliable_for_busno(int busno) {
   char * driver = get_driver_for_busno(busno);
   bool result = is_sysfs_reliable_for_driver(driver);
   free(driver);
   return result;
}


/** Reports whether sysfs attributes for all DRM connectors reliably reflect
 *  display connection and disconnection.
 *
 *  @return true if reliable, false if not
 */
bool is_sysfs_reliable() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "force_sysfs_unreliable=%s, force_sysfs_reliable=%s",
         sbool(force_sysfs_unreliable), sbool(force_sysfs_reliable));

   if (!drm_reliability_checked)
      check_sysfs_reliability();

   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "other_drivers_seen=%s, nvidia_connectors_exist=%s",
         sbool(other_drivers_seen), sbool(nvidia_connectors_exist));

   bool result = true;
   // force_sysfs_unreliable, force_sysfs_reliable exist to facilitate testing
   if (force_sysfs_unreliable)
      result = false;
   else if (force_sysfs_reliable)
      result = true;

   else if (other_drivers_seen)
      result = false;
   else if (nvidia_connectors_exist)
      result = false;

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "");
   return result;
}


/** Module initialization */



/** Frees the strings a #Sysfs_Basic_I2C_Info holds.
 *
 *  @param  info  struct whose contents are to be freed
 *
 *  @remark
 *  Frees the contents, not the struct, which is returned by value rather than
 *  allocated.  The caller's copy holds the freed pointers afterward.
 */
void free_sysfs_basic_i2c_info_contents(Sysfs_Basic_I2C_Info info) {
   free(info.driver);
   free(info.adapter_class);
}


/** Returns just the driver and adapter class for an I2C bus.
 *
 *  Reads only the two attributes #i2c_check_bus() uses, where
 *  #get_i2c_driver_info() also reads the bus name and the driver version and
 *  retains the adapter path.  Emits no report.
 *
 *  @param  busno  I2C bus number
 *  @return #Sysfs_Basic_I2C_Info, returned by value.  Either field is NULL if
 *          the attribute could not be read, which is the normal outcome for a
 *          bus with no adapter behind it.  Caller frees the contents using
 *          #free_sysfs_basic_i2c_info_contents().
 */
Sysfs_Basic_I2C_Info get_basic_i2c_info(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d", busno);

   Sysfs_Basic_I2C_Info result = {NULL, NULL};
   char bus_path[40];
   g_snprintf(bus_path, 40, "/sys/bus/i2c/devices/i2c-%d", busno);
   char * adapter_path = sysfs_find_adapter(bus_path);
   if (adapter_path) {
      RPT_ATTR_TEXT(             -1, &result.adapter_class, adapter_path, "class");
      RPT_ATTR_REALPATH_BASENAME(-1, &result.driver,        adapter_path, "driver");
      free(adapter_path);   // not retained, unlike in get_i2c_driver_info()
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "driver=%s, adapter_class=%s",
         result.driver, result.adapter_class);
   return result;
}


void init_i2c_sysfs_base() {
   RTTI_ADD_FUNC(check_connector_reliability);
   RTTI_ADD_FUNC(check_sysfs_reliability);
   RTTI_ADD_FUNC(find_sysfs_drm_connector_name_by_edid);
   RTTI_ADD_FUNC(get_sysfs_drm_connector_names);
   RTTI_ADD_FUNC(get_basic_i2c_info);
   RTTI_ADD_FUNC(is_connector_reliable);
   RTTI_ADD_FUNC(is_driver_reliable);
   RTTI_ADD_FUNC(is_sysfs_reliable_for_driver);
   RTTI_ADD_FUNC(is_sysfs_reliable);
   RTTI_ADD_FUNC(search_all_businfo_records_by_connector_name);
   RTTI_ADD_FUNC(sysfs_connector_directories_exist);
#ifdef UNUSED
   RTTI_ADD_FUNC(get_sys_video_devices);
#endif
}
