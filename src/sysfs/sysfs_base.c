/** @file sysfs_base.c */

 // Copyright (C) 2020-2025 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "sysfs_i2c_info.h"   // ugh - circular

#include "sysfs_base.h"


static const DDCA_Trace_Group  TRACE_GROUP = DDCA_TRC_SYSFS;


//
// Predicate functions
//

// typedef Dir_Filter_Func
bool is_drm_connector(const char * dirname, const char * simple_fn) {
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = false;
   if (str_starts_with(simple_fn, "card")) {
      char * s0 = g_strdup( simple_fn + 4);   // work around const char *
      char * s = s0;
      while (isdigit(*s)) s++;
      if (*s == '-')
         result = true;
      free(s0);
   }
   DBGMSF(debug, "Done.     Returning %s", SBOOL(result));
   return result;
}


bool fn_equal(const char * filename, const char * val) {
   return streq(filename, val);
}


bool fn_starts_with(const char * filename, const char * val) {
   return str_starts_with(filename, val);
}


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
// Extract bus numbers connetor_id, and name from card-connector directories
//

void dbgrpt_connector_bus_numbers(Connector_Bus_Numbers * cbn, int depth) {
   rpt_structure_loc("Connector_Bus_Numbers", cbn, depth);
   int d1 = depth+1;
   rpt_vstring(d1, "i2c_busno:    %d", cbn->i2c_busno);
   rpt_vstring(d1, "base_busno:   %d", cbn->base_busno);
   rpt_vstring(d1, "connector_id: %d", cbn->connector_id);
   rpt_vstring(d1, "name:         %s", cbn->name);
}


void free_connector_bus_numbers(Connector_Bus_Numbers * cbn) {
   free(cbn->name);
   free(cbn);
}


/** Attempts to extract an I2C bus number and additional information from a
 * card-connector directory. This may not always be successful:
 *  - connector is on MST hub
 *  - Nvidia proprietary driver
 *
 *  @param dirname   <device>drm/cardN
 *  @param fn        connector name, e.g. card0-HDMI-1
 *  @param cbn       struct in which to collect results
 *
 *  @remark
 *  DP connectors:
 *  - normally have a i2c-N subdirectory
 *    - not present for MST
 *  - have drm_dp_aux subdirectory  (amdgpu, i915)
 *    - not present for Nvidia
 *  - name attribute in drm_dp_aux subdir may be "DPMST"
 *  - ddc/i2c-dev directory contains dir with name of "base" i2c-dev device
 *    - not present for MST
 *  HDMI, DVI connectors:
 *  - have ddc directory
 *    - ddc/i2c-dev contains subdirectory with i2c bus name
 *    - ddc/name exists
 */
void get_connector_bus_numbers(
      const char *            dirname,    // <device>/drm/cardN
      const char *            fn,         // card0-HDMI-1 etc
      Connector_Bus_Numbers * cbn)
{
   bool debug = false;
   int d = (IS_DBGTRC(debug, DDCA_TRC_NONE)) ? 1 : -1;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dirname=%s, fn=|%s|", dirname, fn);
   int d0 = (debug) ? 1 : -1;
   bool validate_name = debug;

   bool is_dp_connector = (str_contains(fn, "-DP-") > 0) ;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "is_dp_connector=%s", sbool(is_dp_connector));

   cbn->i2c_busno = -1;      // 0 is valid bus number
   cbn->base_busno = -1;
   cbn->connector_id = -1;

   int connector_id;
   bool found = RPT_ATTR_INT(d, &connector_id, dirname, fn, "connector_id");
   if (found)
      cbn->connector_id = connector_id;

   if (is_dp_connector) {  // DP  // was has_i2c_subdir
      // name attribute exists in multiple location
      char * aux_dir_name = NULL;
      char * i2cN_dir_name = NULL;
      char * ddc_dir_name = NULL;

      // Examine drm_dp_auxN subdirectory
      // Present: i915, amdgpu
      // Absent:  Nvidia
      char * drm_dp_aux_dir = NULL;
      bool has_drm_dp_aux_dir =    // does it exist? e.g. /sys/class/drm/card0-DP-1/drm_dp_aux0
            RPT_ATTR_SINGLE_SUBDIR(d0, &drm_dp_aux_dir, fn_starts_with, "drm_dp_aux", dirname, fn);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "has_drm_dp_aux_dir=%s, drm_dp_aux_dir = %s",
            SBOOL(has_drm_dp_aux_dir), drm_dp_aux_dir);
      if (has_drm_dp_aux_dir) {
         RPT_ATTR_TEXT(d0, &aux_dir_name, dirname, fn, drm_dp_aux_dir, "name");
         free(drm_dp_aux_dir);
      }

      // Examine i2c-N subdirectory
      // Present: i915, amdgpu (normal)
      // Absent:  amdgpu(MST), Nvidia
      char * i2cN_buf = NULL;   // i2c-N
      char * i2cN_buf2 = NULL;
      bool has_i2c_subdir =
               RPT_ATTR_SINGLE_SUBDIR(d0, &i2cN_buf, fn_starts_with,"i2c-", dirname, fn);
      if (has_i2c_subdir) {   // i2c-N directory not present for MST hub
         cbn->i2c_busno = i2c_name_to_busno(i2cN_buf);

         // e.g. /sys/class/drm/card0-DP-1/i2c-6/name:
         RPT_ATTR_TEXT(d0, &i2cN_dir_name, dirname, fn, i2cN_buf, "name");
      }

      // Examine ddc subdirectory.
      // Present: i915, amdgpu (normal)
      // Absent:  Nvidia, amdgpu(MST)
      bool has_ddc_subdir = RPT_ATTR_NOTE_SUBDIR(-1, NULL, dirname, fn, "ddc");
      // char * ddc_dir_path;
      if (has_ddc_subdir) {
         // RPT_ATTR_REALPATH(-1, &ddc_dir_path,  dirname, fn, "ddc");
         // RPT_ATTR_TEXT(-1, &ddc_dir_name, ddc_dir_path, "name");
         RPT_ATTR_TEXT(-1, &ddc_dir_name, dirname, fn, "ddc", "name");

         bool has_i2c_dev_subdir = RPT_ATTR_NOTE_SUBDIR(-1, NULL, dirname, fn, "ddc", "i2c-dev");
         if (has_i2c_dev_subdir) {
            // looking for e.g. /sys/bus/drm/card0-DP-1/ddc/i2c-dev/i2c-1
            has_i2c_subdir =
                  RPT_ATTR_SINGLE_SUBDIR(d0, &i2cN_buf2, fn_starts_with, "i2c-",
                                         dirname, fn, "ddc", "i2c-dev");
            if (has_i2c_subdir) {
               cbn->base_busno = i2c_name_to_busno(i2cN_buf2);

               // RPT_ATTR_TEXT(d0, &cbn->base_dev, dirname, fn, "ddc", "i2c-dev", i2cN_buf2, "dev");

            }
         }
      }  //ddc subdirectory
      free(i2cN_buf);
      free(i2cN_buf2);

      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
      //      "connector: %s, aux_dir_name: |%s|, i2cN_dir_name: |%s|, ddc_dir_name: |%s|",
      //      fn, aux_dir_name, i2cN_dir_name, ddc_dir_name);
      if (aux_dir_name)
         cbn->name = strdup(aux_dir_name);
      else if (i2cN_dir_name)
         cbn->name = strdup(i2cN_dir_name);
      else if (ddc_dir_name)
         cbn->name = strdup(ddc_dir_name);
      else
         cbn->name = NULL;

      free(aux_dir_name);
      free(i2cN_dir_name);
      free(ddc_dir_name);
   } // DP

   else {   // not DP
      // Examine ddc subdirectory
      // Not present: Nvidia
      char * ddc_dir_path = NULL;
      bool found_ddc = RPT_ATTR_REALPATH(d0, &ddc_dir_path,    dirname, fn, "ddc");
      ASSERT_IFF(found_ddc, ddc_dir_path);  // guaranteed by RPT_ATTR_REALPATH()
      if (ddc_dir_path) {
         RPT_ATTR_TEXT(d0, &cbn->name, dirname, fn, "ddc", "name");
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "name=%s", cbn->name);
         // No ddc directory on Nvidia?
         // Examine ddc subdirectory

         char * i2cN_buf = NULL;
         // looking for e.g. /sys/bus/drm/card0-DVI-D-1/ddc/i2c-dev/i2c-1
         bool has_i2c_subdir =
             RPT_ATTR_SINGLE_SUBDIR(d0, &i2cN_buf, fn_starts_with, "i2c-",
                                             dirname, fn, "ddc", "i2c-dev");
         if (has_i2c_subdir) {
            cbn->i2c_busno = i2c_name_to_busno(i2cN_buf);
            // RPT_ATTR_TEXT(d0, &cur->base_dev, dirname, fn, "ddc", "i2c-dev", i2cN_buf, "dev");

            if (validate_name) {
               // Check that <connector>/ddc/i2c-dev/i2c-N/name and <connector>/ddc/name match
               char * ddc_i2c_dev_name = NULL;
               RPT_ATTR_TEXT(d0, &ddc_i2c_dev_name, dirname, fn, "ddc", "i2c-dev", i2cN_buf, "name");
               if (!streq(ddc_i2c_dev_name, cbn->name) && debug)
                     rpt_vstring(d0, "Unexpected: %s/ddc/i2c-dev/%s/name and %s/ddc/name do not match",
                                              fn, i2cN_buf, fn);
               free(ddc_i2c_dev_name);
            }
         }
         free(i2cN_buf);
         free(ddc_dir_path);
      }  // has ddc subdirectory

   }   // not DP

   if (IS_DBGTRC(debug, TRACE_GROUP))
      dbgrpt_connector_bus_numbers(cbn, 1);
    DBGTRC_DONE(debug, TRACE_GROUP, "");
}


//
// Debug Reports
//

static
void simple_report_one_connector0(
      const char * dirname,     // <device>/drm/cardN
      const char * simple_fn,   // card0-HDMI-1 etc
      bool         verbose,
      int          depth)
{
   bool debug = false;
   // verbose = true;
   int d1 = depth+1;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);
   assert(dirname);
   assert(simple_fn);

   GByteArray * edid_byte_array = NULL;
   char * status       = NULL;
   char * connector_id = NULL;
   char * enabled      = NULL;
   possibly_write_detect_to_status_by_connector_name(simple_fn);
   GET_ATTR_TEXT(&connector_id,    dirname, simple_fn, "connector_id");
   GET_ATTR_TEXT(&status,          dirname, simple_fn, "status");
   GET_ATTR_TEXT(&enabled,         dirname, simple_fn, "enabled");
   GET_ATTR_EDID(&edid_byte_array, dirname, simple_fn, "edid");
   Connector_Bus_Numbers * cbn = calloc(1, sizeof(Connector_Bus_Numbers));
   get_connector_bus_numbers(dirname, simple_fn, cbn);

   if (verbose || edid_byte_array || streq(status, "connected")) {
      rpt_nl();
      rpt_vstring(depth, "Connector: %s", simple_fn);
      rpt_vstring(d1,       "connector id: %s", connector_id);
      rpt_vstring(d1,       "status:       %s", status);
      rpt_vstring(d1,       "enabled:      %s", enabled);
      if (edid_byte_array) {
         Parsed_Edid * parsed = create_parsed_edid(edid_byte_array->data);
         if (parsed) {
            rpt_vstring(d1, "edid:         %s/%s/%s",   parsed->mfg_id, parsed->model_name, parsed->serial_ascii);
            free_parsed_edid(parsed);
         }
         else
            rpt_label(  d1, "edid:         parse failed");
      }
      rpt_vstring(d1,       "i2c busno:    %d", cbn->i2c_busno);
      rpt_vstring(d1,       "name:         %s", cbn->name);
   }
   free_connector_bus_numbers(cbn);
   free(status);
   free(connector_id);
   free(enabled);
   if (edid_byte_array)
      g_byte_array_free(edid_byte_array, true);

   DBGMSF(debug, "Done");
}


static
void simple_report_one_connector(
      const char * dirname,     // <device>/drm/cardN
      const char * simple_fn,   // card0-HDMI-1 etc
      void *       data,
      int          depth)
{
   simple_report_one_connector0(dirname, simple_fn, false, depth);
}


/** Reports sysfs attributes connector_id, enabled, status, dpms, and edid
 *  for each DRM connector.
 *
 *  @param depth  logical indentation depth
 */
void dbgrpt_sysfs_basic_connector_attributes(int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   int d0 = depth;
   rpt_nl();
   char * dname = "/sys/class/drm";

   rpt_vstring(d0, "*** Examining %s for card-connector dirs that appear to be connected ***", dname);
   dir_filtered_ordered_foreach(
                dname,
                is_card_connector_dir,        // filter function
                sys_drm_connector_name_cmp,   // ordering function
                simple_report_one_connector,
                NULL,                         // accumulator
                depth);
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


//
// Get DRM connector name given an I2C bus number or connector id.

typedef struct {
   int    connector_id;
   // char * connector_id_s;
   char * connector_name;
} Check_Connector_Id_Accumulator;

typedef struct {
   int    busno;
   char * connector_name;
} Check_Busno_Accumulator;


static
bool check_connector_id(
      const char *  dirname,
      const char *  fn,
      void *        accumulator,
      int           depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dirname=|%s|, fn=|%s|", dirname, fn);
   int debug_depth = (debug) ? 1 : -1;

   bool terminate = false;
   int this_connector_id = -1;
   Check_Connector_Id_Accumulator * accum = accumulator;
   // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "accum->connector_id=%d, accum->connector_id_s=|%s|",
   //       accum->connector_id, accum->connector_id_s);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "accum->connector_id=%d", accum->connector_id);
   bool connector_id_found = RPT_ATTR_INT(debug_depth, &this_connector_id, dirname, fn, "connector_id");
   if (connector_id_found && this_connector_id == accum->connector_id) {
      accum->connector_name = strdup(fn);
      terminate = true;
   }

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, terminate, "accum->connector_name = |%s|", accum->connector_name);
   return terminate;
}


static
bool check_busno(
      const char *  dirname,
      const char *  fn,
      void *        accumulator,
      int           depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dirname=|%s|, fn=|%s|", dirname, fn);

   bool terminate = false;
   Check_Busno_Accumulator * accum = accumulator;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "accum->busno=%d", accum->busno);

   Connector_Bus_Numbers * cbn = calloc(1, sizeof(Connector_Bus_Numbers));
   get_connector_bus_numbers(dirname, fn, cbn);

   if (cbn->i2c_busno == accum->busno) {
      terminate = true;
      accum->connector_name = g_strdup(fn);
   }
   free_connector_bus_numbers(cbn);

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, terminate, "accum->connector_name = |%s|", accum->connector_name);
   return terminate;
}


/** Given a DRM connector id, return the sysfs connector name
 *
 *  @param  connector_id
 *  @return connector name, e.g. card1-DP-1, caller must free
 */
char * get_sys_drm_connector_name_by_connector_id(int connector_id) {
   bool debug = false;
   int depth = 0;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "connector_id=%d", connector_id);

   char connector_id_s[20];
   snprintf(connector_id_s, 20, "%d", connector_id);

   Check_Connector_Id_Accumulator accum;
   accum.connector_id = connector_id;
   // accum.connector_id_s = connector_id_s;
   accum.connector_name = NULL;

   dir_foreach_terminatable(
         "/sys/class/drm",
         predicate_cardN_connector,       // filter function
         check_connector_id,
         &accum,
         depth);

   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %s", accum.connector_name);
   return accum.connector_name;
}


/** Given a I2C bus number, return the name of the
 *  connector for that bus number.
 *
 *  @param  busno  i2c bus number
 *  @return connector name
 */
char * get_sys_drm_connector_name_by_busno(int busno) {
   bool debug = false;
   int depth = 0;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "busno=%d", busno);

   Check_Busno_Accumulator accum;
   accum.busno = busno;
   accum.connector_name = NULL;

   dir_foreach_terminatable(
         "/sys/class/drm",
         predicate_cardN_connector,       // filter function
         check_busno,
         &accum,
         depth);

   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %s", accum.connector_name);
   return accum.connector_name;
}


//
// Checks whether connector_id exists
//

typedef struct {
   bool   all_connectors_have_connector_id;
} Check_Connector_Id_Present_Accumulator;

static
bool check_connector_id_present(
      const char *  dirname,
      const char *  fn,
      void *        accumulator,
      int           depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dirname=|%s|, fn=|%s|", dirname, fn);
   int debug_depth = (debug) ? 1 : -1;

   bool terminate = false;
   int this_connector_id = -1;
   Check_Connector_Id_Present_Accumulator * accum = accumulator;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "accum->all_connectors_have_connector_id=%s",
         SBOOL(accum->all_connectors_have_connector_id));
   bool found = RPT_ATTR_INT(debug_depth, &this_connector_id, dirname, fn, "connector_id");
   if (!found) {
      accum->all_connectors_have_connector_id = false;
      terminate = true;
   }

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, terminate, "accum->all_connectors_have_connector_id = %s",
         SBOOL(accum->all_connectors_have_connector_id));
   return terminate;
}


/** Checks if attribute connector_id exists for all sysfs drm connectors
 *
 *  @return true if all drm connectors have connector_id, false if not
 *
 *  /remark
 *  returns true if there are no drm_connectors
 */
bool all_sys_drm_connectors_have_connector_id_direct() {
   bool debug = false;
   int depth = 0;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "depth=%d", depth);

   Check_Connector_Id_Present_Accumulator accum;
   accum.all_connectors_have_connector_id = true;
   dir_foreach_terminatable(
         "/sys/class/drm",
         predicate_cardN_connector,       // filter function
         check_connector_id_present,
         &accum,
         depth);

   DBGTRC_RET_BOOL(debug, DDCA_TRC_I2C, accum.all_connectors_have_connector_id, "");
   return accum.all_connectors_have_connector_id;
}


//
// Driver inquiry functions
//

/** Given the sysfs path to an adapter of some sort, returns
 *  the name of its driver.
 *
 *  @param adapter_path
 *  @param depth        logical indentation depth
 *  @return name of driver module, NULL if not found
 *
 *  Parameter **depth** behaves as usual for sysfs RPT_... functions.
 *  If depth >= 0, sysfs attributes are reported.
 *  If depth <  0, there is no output
 *
 *  Caller is responsible for freeing the returned value
 */
char * get_driver_for_adapter(char * adapter_path, int depth) {
   char * basename = NULL;
   RPT_ATTR_REALPATH_BASENAME(depth, &basename, adapter_path, "driver", "module");
   return basename;
}


/** Given a sysfs node, walk up the chain of device directory links
 *  until an adapter node is found, and return the name of its driver.
 *
 *  @param  path   e.g. /sys/bus/i2c/drivers/i2c-5
 *  @param  depth  logical indentation depth
 *  @return sysfs path to adapter
 *
 *  Parameter **depth** behaves as usual for sysfs RPT_... functions.
 *  If depth >= 0, sysfs attributes are reported.
 *  If depth <  0, there is no output
 *
 *  Caller is responsible for freeing the returned value
 */
char *
find_adapter_and_get_driver(char * path, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "path=%s,  depth=%d", path, depth);
   assert(path);
   assert(strlen(path)>0);

   char * result = NULL;
   char * adapter_path = sysfs_find_adapter(path);
   if (adapter_path) {
      result = get_driver_for_adapter(adapter_path, depth);
      free(adapter_path);
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE,"Returning: %s", result);
   return result;
}


/** Returns the name of the video driver for an I2C bus.
 *
 * @param  busno   I2 bus number
 * @return driver name, NULL if can't determine
 *
 * Caller is responsible for freeing the returned string.
 */
char * get_driver_for_busno(int busno) {
   char path[PATH_MAX];
   g_snprintf(path, PATH_MAX, "/sys/bus/i2c/devices/i2c-%d", busno);
   char * result = find_adapter_and_get_driver(path, -1);
   return result;
}


//
// Possibly write "detect" to attribute status before reading connector attributes
// with nvidia driver
//

void possibly_write_detect_to_status(const char * driver, const char * connector) {
   bool debug = false;
   assert(driver);
   assert(connector);
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "driver=%s, connector=%s", driver, connector);
   bool wrote_detect_to_status = false;

   bool do_driver = streq(driver, "nvidia");
   if (enable_write_detect_to_status && do_driver && connector) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Writing detect to status");
      char path[50];
      g_snprintf(path, 50, "/sys/class/drm/%s/status", connector);
      FILE * f = fopen(path, "w");
      if (f) {
         fputs("detect", f);
         fclose(f);
         wrote_detect_to_status = true;
      }
      else {
         DBGTRC(debug, DDCA_TRC_NONE, "fopen() failed. connector=%s,  errno=%d", connector, errno);
      }
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "wrote detect to status: %s",
               SBOOL(wrote_detect_to_status));
}


void possibly_write_detect_to_status_by_connector_path(const char * connector_path) {
   bool debug = false;
    int d = (debug) ? 1 : -1;
    if (enable_write_detect_to_status) {
       char * driver = find_adapter_and_get_driver((char*) connector_path, d);
       if (driver) {
          possibly_write_detect_to_status(driver, connector_path);
         free(driver);
       }
    }
}


void possibly_write_detect_to_status_by_connector_name(const char * connector) {
   bool debug = false;
   int d = (debug) ? 1 : -1;
   if (enable_write_detect_to_status) {
      char path[50];
      g_snprintf(path, 50, "/sys/class/drm/%s", connector);
      char * driver = find_adapter_and_get_driver(path, d);
      if (driver) {
         possibly_write_detect_to_status(driver, connector);
        free(driver);
      }
   }
}


void possibly_write_detect_to_status_by_businfo(I2C_Bus_Info * businfo) {
   if (enable_write_detect_to_status) {
      if (businfo->driver)
         possibly_write_detect_to_status(businfo->driver, businfo->drm_connector_name);
      else {
         char * driver = get_driver_for_busno(businfo->busno);
         possibly_write_detect_to_status(businfo->driver, businfo->drm_connector_name);
         free(driver);
      }
   }
}


void possibly_write_detect_to_status_by_dref(Display_Ref * dref) {
   if (enable_write_detect_to_status) {
      if (dref->io_path.io_mode == DDCA_IO_I2C) {
         I2C_Bus_Info * businfo = dref->detail;
         possibly_write_detect_to_status_by_businfo(businfo);
      }
      else {
         if (dref->drm_connector) {
            possibly_write_detect_to_status_by_connector_name(dref->drm_connector);
         }
      }
   }
}


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
   possibly_write_detect_to_status_by_connector_name(simple_fn);
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


// Note: On amdgpu, for DP device realpath is connector with EDID, for HDMI and DVI device is adapter


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
      possibly_write_detect_to_status_by_connector_name(connector_name);
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


/* i915, amdgpu, radeon, nouveau and (likely) other video drivers that share
 * the kernel's DRM code can be relied on to maintain the edid,
 * status, and enabled attributes as displays are connected and
 * disconnected.
 *
 * Unfortunately depending version, the nvidia driver does not.
 * Attribute enabled is always "disabled".  It may be the case
 * that the edid value is that of the monitor initially connected.
 * What has been observed is that if the driver does change the
 * edid attribute, it also properly sets status to "connected" or
 * disconnected.  If it does not, status is always "disconnected",
 * whether or not a monitor is connected.
 */

typedef struct {
   bool     known_good_driver_seen;
   bool     other_driver_seen;
   uint8_t  nvidia_connector_ct;
   uint8_t  nvidia_connector_w_edid_ct;
   uint8_t  nvidia_connector_w_edid_and_connected_ct;
} Sysfs_Reliability_Accumulator;


static
bool known_reliable_driver(const char * driver) {
   return streq(driver, "i915")   ||
          streq(driver, "xe") ||
          streq(driver, "amdgpu") ||
          streq(driver, "radeon") ||
          streq(driver, "nouveau");
}


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

   Sysfs_Reliability_Accumulator * accum = accumulator;

   char buf[PATH_MAX];
   g_snprintf(buf, PATH_MAX, "%s/%s", dirname, fn);
   char * driver = find_adapter_and_get_driver(buf, debug_depth);
   if (known_reliable_driver(driver))
   {
      accum->known_good_driver_seen = true;
   }
   else if (streq(driver, "nvidia")) {
      // Per Michael Hamilton, testing that status == "connected" for any connector with EDID
      // does not guarantee that DRM connector is updated when a display is connected/disconnected
      accum->nvidia_connector_ct++;
      GByteArray * edid_byte_array = NULL;
      possibly_write_detect_to_status_by_connector_name(fn);
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
   else {
      accum->other_driver_seen = true;
   }
   free(driver);

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}

static bool drm_reliability_checked = false;
static bool other_drivers_seen = false;
static bool nvidia_connectors_reliable = false;
static bool nvidia_connectors_exist = false;

static
void check_sysfs_reliability() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");

   Sysfs_Reliability_Accumulator * accum = calloc(1, sizeof(Sysfs_Reliability_Accumulator));
   int depth=0;
   dir_foreach(
         "/sys/class/drm",
         predicate_cardN_connector,       // filter function
         check_connector_reliability,
         accum,
         depth);

   drm_reliability_checked = true;
   nvidia_connectors_exist = (accum->nvidia_connector_ct > 0);
   // known_good_driver_seen = > 0;
   // This appears to be a necessary, but not sufficient, condition
   nvidia_connectors_reliable =
         accum->nvidia_connector_w_edid_ct > 0 &&
         accum->nvidia_connector_w_edid_ct == accum->nvidia_connector_w_edid_and_connected_ct;
   other_drivers_seen = accum->other_driver_seen;
   free(accum);

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "nvidia_connectors_exist=%s, nvidia_connectors_reliable=%s",
         sbool(nvidia_connectors_exist), sbool(nvidia_connectors_reliable));
}

bool force_sysfs_unreliable = false;
bool force_sysfs_reliable = false;
bool enable_write_detect_to_status = false;


/** Reports whether sysfs attributes for DRM connectors using the given video
 *  driver reliably reflect display connection and disconnection.
 *
 *  @param  driver  name of driver
 *  @return true if reliable, false if not
 */
bool is_sysfs_reliable_for_driver(const char * driver) {
   bool debug = false;

   bool result = false;
   if (!drm_reliability_checked)
      check_sysfs_reliability();

   if (force_sysfs_unreliable)
      result = false;
   else if (force_sysfs_reliable)
      result = true;
   else {
      if (streq(driver, "nvidia"))
         result = nvidia_connectors_reliable;
      else
         result = known_reliable_driver(driver);
   }

   DBGF(debug, "Executed. Returning %s, driver=%s", SBOOL(result), driver);
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
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "nvdia_connectors_reliable=%s",
         SBOOL(nvidia_connectors_reliable));

   bool result = true;
   if (force_sysfs_unreliable)
      result = false;
   else if (force_sysfs_reliable)
      result = true;
   else if (other_drivers_seen)
      result = false;
   else if (nvidia_connectors_exist)
      result = nvidia_connectors_reliable;

   DBGTRC_EXECUTED(debug, DDCA_TRC_NONE, "Returning %s", SBOOL(result));
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
#ifdef OLD
char * sysfs_find_adapter_old(char * path) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "path=%s", path);
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

   DBGTRC_DONE(debug,TRACE_GROUP, "Returning: %s", devpath);
   return devpath;
}
#endif


char * sysfs_find_adapter(char * path) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "path=%s", path);
   assert(path);
   int depth = (IS_DBGTRC(debug, DDCA_TRC_NONE)) ? 2 : -1;

   char * devpath = NULL;
   char * rp1 = strdup(path);
   char * rp2 = NULL;

   // strlen(rp1) > 1  shuld be unnecessary, but just in case:
   while(!devpath && strlen(rp1) > 0 && !streq(rp1, "/")) {
      if ( RPT_ATTR_TEXT(depth, NULL, rp1, "class")) {
          devpath = rp1;
      }
      else {
         RPT_ATTR_REALPATH(depth, &rp2, rp1, "..");
         free(rp1);
         rp1 = rp2;
         rp2 = NULL;
      }
   }
   if (!devpath)
      free(rp1);

   DBGTRC_DONE(debug,TRACE_GROUP, "Returning: %s", devpath);
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
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d", busno);

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

   DBGTRC_DONE(debug, TRACE_GROUP, "busno=%d, Returning %s", busno, driver_name);
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
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d");

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

   DBGTRC_DONE(debug, TRACE_GROUP, "busno=%d, Returning 0x%08x", busno, result);
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



void init_i2c_sysfs_base() {
   RTTI_ADD_FUNC(possibly_write_detect_to_status);
   RTTI_ADD_FUNC(sysfs_find_adapter);
   RTTI_ADD_FUNC(get_i2c_sysfs_driver_by_busno);
   RTTI_ADD_FUNC(get_i2c_device_sysfs_class);
   RTTI_ADD_FUNC(check_connector_reliability);
   RTTI_ADD_FUNC(check_sysfs_reliability);
   RTTI_ADD_FUNC(dbgrpt_sysfs_basic_connector_attributes);
   RTTI_ADD_FUNC(find_adapter_and_get_driver);
   RTTI_ADD_FUNC(find_sysfs_drm_connector_name_by_edid);
   RTTI_ADD_FUNC(get_connector_bus_numbers);
   RTTI_ADD_FUNC(get_sys_drm_connector_name_by_connector_id);
   RTTI_ADD_FUNC(is_sysfs_reliable);
#ifdef UNUSED
   RTTI_ADD_FUNC(get_sys_video_devices);
#endif

}

