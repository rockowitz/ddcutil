// i2c_sysfs_base.c

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

#include "i2c/i2c_sysfs_i2c_info.h"   // ugh

#include "i2c/i2c_sysfs_base.h"


static const DDCA_Trace_Group  TRACE_GROUP = DDCA_TRC_NONE;



bool nvidia_driver_implies_sysfs_unreliable = false;

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
char * find_adapter(char * path, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "path=%s", path);

   char * devpath = NULL;
   if ( RPT_ATTR_NOTE_INDIRECT_SUBDIR(depth, NULL, path, "device") ) {
       if ( RPT_ATTR_TEXT(depth, NULL, path, "device", "class") ) {
          RPT_ATTR_REALPATH(depth, &devpath, path, "device");
       }
       else {
          char p2[PATH_MAX];
           g_snprintf(p2, PATH_MAX, "%s/device", path);
           devpath = find_adapter(p2, depth);
       }
   }
   DBGTRC_RETURNING(debug, TRACE_GROUP, devpath, "");
   return devpath;
}



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
      bool has_ddc_subdir = RPT_ATTR_NOTE_INDIRECT_SUBDIR(-1, NULL, dirname, fn, "ddc");
      // char * ddc_dir_path;
      if (has_ddc_subdir) {
         // RPT_ATTR_REALPATH(-1, &ddc_dir_path,  dirname, fn, "ddc");
         // RPT_ATTR_TEXT(-1, &ddc_dir_name, ddc_dir_path, "name");
         RPT_ATTR_TEXT(-1, &ddc_dir_name, dirname, fn, "ddc", "name");

         bool has_i2c_dev_subdir = RPT_ATTR_NOTE_INDIRECT_SUBDIR(-1, NULL, dirname, fn, "ddc", "i2c-dev");
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


      DBGTRC(debug, DDCA_TRC_NONE, "connector: %s, aux_dir_name: |%s|, i2cN_dir_name: |%s|, ddc_dir_name: |%s|",
               fn, aux_dir_name, i2cN_dir_name, ddc_dir_name);
#ifdef REWRITE
         if (validate_name) {
            if (aux_dir_name && i2cN_dir_name) {
            char * buf = NULL;
            RPT_ATTR_TEXT(d0, &buf, dirname, fn, i2cN_buf, "i2c-dev", i2cN_buf, "name");
            if (!streq(cbn->name, buf) && d0 >= 0 )
               rpt_vstring(d0, "Unexpected: %s/name and i2c-dev/%s/name do not match",
                       drm_dp_aux_dir, i2cN_buf);
            free(buf);
         }
#endif
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
   free(cbn);

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



// Driver related functions

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
   bool debug = true;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "path=%s,  depth=%d", path, depth);

   char * result = NULL;
   char * adapter_path = find_adapter(path, depth);
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
  *  the accumulated list of connectors having a valid EDID.
  *
  *  @param  dirname    directory to examine, <device>/drm/cardN
  *  @param  simple_fn  filename to examine
  *  @param  data       pointer to Sysfs_Connector_Names instance
  *  @param  depth      if >= 0, emits a report with this logical indentation depth
  */
// static
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


bool is_sysfs_unreliable(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d, nvidia_driver_implies_sysfs_unreliable=%s",
         busno, SBOOL(nvidia_driver_implies_sysfs_unreliable));

   bool sysfs_unreliable = false;
   if (nvidia_driver_implies_sysfs_unreliable) {
      // TODO: eliminate use of Sysfs_I2C_Info, access sysfs directly
      Sysfs_I2C_Info * driver_info = get_i2c_driver_info(busno, (debug) ? 1 : -1);
      sysfs_unreliable =  streq(driver_info->driver, "nvidia"); //  || str_starts_with(driver_info->name, "NVIDIA");
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "sysfs_unreliable=%s, driver=%s, name=%s",
            SBOOL(sysfs_unreliable), driver_info->driver, driver_info->name);
      free_sysfs_i2c_info(driver_info);
   }

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, sysfs_unreliable, "");
   return sysfs_unreliable;
}



void init_i2c_sysfs_base() {
   RTTI_ADD_FUNC(dbgrpt_sysfs_basic_connector_attributes);
   RTTI_ADD_FUNC(find_adapter_and_get_driver);
   RTTI_ADD_FUNC(find_adapter);
   RTTI_ADD_FUNC(find_sysfs_drm_connector_name_by_edid);
   RTTI_ADD_FUNC(get_connector_bus_numbers);
   RTTI_ADD_FUNC(get_sys_drm_connector_name_by_connector_id);
   RTTI_ADD_FUNC(get_sys_video_devices);
   RTTI_ADD_FUNC(is_sysfs_unreliable);
}

