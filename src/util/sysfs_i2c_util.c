/** @file sysfs_i2c_util.c
 *  i2c specific /sys functions
 */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <errno.h>
#include <linux/limits.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "coredefs_base.h"
#include "debug_util.h"
#include "file_util.h"
#include "i2c_util.h"
#include "report_util.h"
#include "string_util.h"
#include "subprocess_util.h"
#include "sysfs_util.h"
#include "sysfs_filter_functions.h"

#include "sysfs_i2c_util.h"


/** Looks in the /sys file system to check if a module is loaded.
 *  Note that only loadable kernel modules will be found. Those
 *  built into the kernel will not.
 *
 * \param  module_name    module name
 * \return true if the module is loaded, false if not
 */
bool
is_module_loaded_using_sysfs(
      const char * module_name)
{
   bool debug = false;

   struct stat statbuf;
   char   module_fn[100];

   bool   found = true;
   snprintf(module_fn, sizeof(module_fn), "/sys/module/%s", module_name);
   int rc = stat(module_fn, &statbuf);
   if (rc < 0) {
      // will be ENOENT (2) if file not found
      str_replace_char(module_fn, '-', '_');
      rc = stat(module_fn, &statbuf);
      if (rc < 0)
         found = false;
   }

   if (debug)
      printf("(%s) module_name = %s, returning %s\n", __func__, module_name, SBOOL(found));
   return found;
}


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
   char * driver_name = NULL;
   char workbuf[100];
   snprintf(workbuf, 100, "/sys/bus/i2c/devices/i2c-%d/device/driver/module", busno);
   driver_name = get_rpath_basename(workbuf);
   if (!driver_name) {
      snprintf(workbuf, 100, "/sys/bus/i2c/devices/i2c-%d/device/device/device/driver/module", busno);
      driver_name = get_rpath_basename(workbuf);
   }
   // printf("(%s) busno=%d, returning %s\n", __func__, busno, driver_name);
   return driver_name;
}


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
   bool result = false;

   // It is possible for a display device to have an I2C bus
   // that should be ignored.  Recent AMD Navi board (e.g. RX 6000)
   // have an I2C SMU bus that will hang the card if probed.
   // So first check for specific device names to ignore.
   // If not found, then base the result on the device's class.

   char * name = get_i2c_device_sysfs_name(busno);
   char * driver = get_i2c_sysfs_driver_by_busno(busno);
   if (name)
      result = ignorable_i2c_device_sysfs_name(name, driver);
   if (debug)
      printf("(%s) busno=%d, name=|%s|, result=%s\n", __func__, busno, name, sbool(result));
   free(name);    // safe if NULL
   free(driver);  // ditto

   if (!result) {
      uint32_t class = get_i2c_device_sysfs_class(busno);
      if (class) {
         // printf("(%s) class = 0x%08x\n", __func__, class);
         uint32_t cl2 = class & 0xffff0000;
         if (debug)
            printf("(%s) cl2 = 0x%08x\n", __func__, cl2);
         result = (cl2 != 0x030000 &&
                   cl2 != 0x0a0000);    // docking station
      }
   }

   if (debug)
      printf("(%s) busno=%d, returning: %s\n", __func__, busno, sbool(result));
   return result;
}


#ifdef UNUSED
static void
do_sysfs_drm_card_number_dir(
            const char * dirname,     // <device>/drm
            const char * simple_fn,   // card0, card1, etc.
            void *       data,
            int          depth)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. dirname=%s, simple_fn=%s\n", __func__, dirname, simple_fn);
   Bit_Set_256 * card_numbers = (Bit_Set_256*) data;
   const char * s = simple_fn+4;
   int card_number = atoi(s);
   *card_numbers = bs256_insert(*card_numbers, card_number);
   if (debug)
      printf("(%s) Done.    Added %d\n", __func__, card_number);
}


Bit_Set_32
get_sysfs_drm_card_numbers() {
   bool debug = false;
   char * dname =
 #ifdef TARGET_BSD
              "/compat/linux/sys/class/drm";
 #else
              "/sys/class/drm";
 #endif
   if (debug)
      printf("(%s) Examining %s\n", __func__, dname);
   Bit_Set_32 result = EMPTY_BIT_SET_32;
   dir_foreach(
                 dname,
                 predicate_cardN,            // filter function
                 do_sysfs_drm_card_number_dir,
                 &result,                 // accumulator
                 0);
   if (debug)
      printf("(%s) Done.    Returning DRM card numbers: %s\n", __func__, bs32_to_string_decimal(result, "", ", "));
   return result;
 }
#endif


// #ifdef UNUSED
/** Returns the paths to all video devices in /sys/devices, i.e. those
 *  subdirectories (direct or indirect) having class = 0x03
 *
 *  @return array of directory names, caller must free
 */
GPtrArray * get_video_adapter_devices() {
   bool debug = true;
   char * cmd = "find /sys/devices -name class | xargs grep x03 -l | sed 's|class||'";
   GPtrArray * result = execute_shell_cmd_collect(cmd);
   g_ptr_array_set_free_func(result, g_free);

   DBGF(debug, "Returning %d directories:", result->len);
   for (int ndx = 0; ndx < result->len; ndx++)
      rpt_vstring(2, "%s", g_ptr_array_index(result, ndx));
   return result;
}
// #endif


bool not_ata(const char * simple_fn) {
   return !str_starts_with(simple_fn, "ata");
}

bool is_pci_dir(const char * simple_fn) {
   bool debug = true;
   bool result = str_starts_with(simple_fn, "pci0");
   DBGF(debug, "simple_fn = %s, returning %s", simple_fn, sbool(result));
   return result;
}

bool predicate_starts_with_0(const char * simple_fn) {
   bool debug = true;
   bool result = str_starts_with(simple_fn, "0");
   DBGF(debug, "simple_fn = %s, returning %s", simple_fn, sbool(result));
   return result;
}


void find_class_dirs(const char * dirname, const char * simple_fn,  void * accumulator, int depth) {
    bool debug = true;
    DBGF(debug, "Starting. dirname=%s, simple_fn=%s, accumulator=%p, depth=%d",
          dirname, simple_fn, accumulator, depth);
    char * fullname = g_strdup_printf("%s/%s", dirname, simple_fn);
    assert(depth < 18);
    int d = (depth < 0) ? -1 : depth+1;
    if (debug &&  depth < 0)
       d = 1;
    GPtrArray* accum = accumulator;
    char * result = NULL;
    bool found = RPT_ATTR_TEXT(d, &result, dirname, simple_fn, "class");
    DBGF(debug, "found=%s", sbool(found));
    if (found) {
       // DBGF(debug, "result=%s", result);
       char * subdir = g_strdup_printf("%s/%s", dirname, simple_fn);
       DBGF(debug, "Adding: %s", subdir);
       g_ptr_array_add(accum, (char*) subdir);
       return;
    }
    DBGF(debug, "fullname=%s does not have attribute class", fullname);
    // if (!str_starts_with(simple_fn, "ata") && !streq(simple_fn, "firmware_node")) {
   //  if (str_starts_with(simple_fn, "0")) {
       char path[PATH_MAX];
       g_snprintf(path, sizeof(path), "%s/%s", dirname, simple_fn);
       DBGF(debug, "Examining subdir %s", path);
       dir_foreach(path, predicate_starts_with_0, find_class_dirs, accumulator, d);
  //   }
}


/** Returns the paths to all video devices in /sys/devices, i.e. those
 *  subdirectories (direct or indirect) having class = 0x03
 *
 *  @return array of directory names, caller must free
 */
GPtrArray *  get_video_adapter_devices2() {
   bool debug = true;
   GPtrArray * class03_dirs = g_ptr_array_new_with_free_func(g_free);
   int d = (debug) ? 1 : -1;

   dir_foreach("/sys/devices", is_pci_dir, find_class_dirs, class03_dirs, d);
   DBGF(debug, "Before filtering: class03_dirs->len =%d", class03_dirs->len);
   for (int ndx = 0; ndx < class03_dirs->len; ndx++)
      rpt_vstring(2, "%s", g_ptr_array_index(class03_dirs, ndx));

   for (int ndx = class03_dirs->len -1; ndx>= 0; ndx--) {
      char * dirname =  g_ptr_array_index(class03_dirs, ndx);
      DBGF(debug, "dirname=%s", dirname);
      char * class = NULL;
      RPT_ATTR_TEXT(d, &class, dirname, "class");
      assert(class);
      if ( !str_starts_with(class, "0x03") ) {
         g_ptr_array_remove_index(class03_dirs, ndx);
      }
   }

   DBGF(debug, "Returning %d directories:", class03_dirs->len);
   for (int ndx = 0; ndx < class03_dirs->len; ndx++)
      rpt_vstring(2, "%s", g_ptr_array_index(class03_dirs, ndx));

   return class03_dirs;
}

typedef struct {
   bool has_card_connector_dir;
} Check_Card_Struct;

#ifdef REF
typedef void (*Dir_Foreach_Func)(
const char *  dirname,
const char *  fn,
void *        accumulator,
int           depth);
#endif

void dir_foreach_set_true(const char * dirname, const char * fn, void * accumulator, int depth) {
   bool debug = true;
   DBGF(debug, "dirname=%s, fn=%s, accumlator=%p, depth=%d", dirname, fn, accumulator, depth);
   Check_Card_Struct * accum = accumulator;
   DBGF(debug, "Setting accumulator->has_card_connector_dir = true");
   accum->has_card_connector_dir = true;
}


void do_one_card(const char * dirname, const char * fn, void* accumulator, int depth) {
   char buf[PATH_MAX];
   g_snprintf(buf, sizeof(buf), "%s/%s", dirname, fn);
   DBGF("Examining dir buf=%s", buf);
   dir_foreach(buf, predicate_cardN_connector, dir_foreach_set_true, accumulator, depth);
   Check_Card_Struct * accum = accumulator;
   DBGF("Finishing with accumlator->has_card_connector_dir = %s", sbool(accum->has_card_connector_dir));
}



/** Check that all video adapter devices in /sys have drivers that
 *  implement drm.
 *
 *  @param  adapter_devices array of /sys directory names for video adapter devices *  @return true if all adapter video drivers implement drm
 */
bool check_video_adapters_list_implements_drm(GPtrArray * adapter_devices) {
   bool debug = true;
   assert(adapter_devices);
   DBGF(debug, "adapter_devices->len=%d at %p", adapter_devices->len, adapter_devices);
   int d = (debug) ? 1 : -1;
   bool result = true;
   for (int ndx = 0; ndx < adapter_devices->len; ndx++) {
      // char * subdir_name = NULL;
      char * adapter_dir = g_ptr_array_index(adapter_devices, ndx);
      DBGF(debug, "Examining: %s", adapter_dir);
      int lastpos= strlen(adapter_dir) - 1;
      if (adapter_dir[lastpos] == '/')
         adapter_dir[lastpos] = '\0';
      char drm_dir[PATH_MAX];
      g_snprintf(drm_dir, PATH_MAX, "%s/drm", adapter_dir);
      DBGF(debug, "drm_dir=%s", adapter_dir);
      Check_Card_Struct *  accumulator = calloc(1, sizeof(Check_Card_Struct));
      dir_foreach(drm_dir, predicate_cardN, do_one_card, accumulator, d);
      bool has_card_subdir = accumulator->has_card_connector_dir;
      free(accumulator);

#ifdef WRONG
      bool has_card_subdir = RPT_ATTR_SINGLE_SUBDIR(
            d, &subdir_name, is_card_connector_dir, NULL, adapter_dir, "drm");
#endif


      DBGF(debug, "Examined.  has_card_subdir = %s", sbool(has_card_subdir));
      if (!has_card_subdir) {
         result = false;
         break;
      }
   }
   return result;
}


/** Checks that all video adapters on the system have drivers that implement drm.
 *
 *  @return true if all video adapters have drivers implementing drm, false if not
 */
bool check_all_video_adapters_implement_drm() {
   bool debug = true;
   DBGF(debug, "Starting");

   GPtrArray * devices = NULL;
    devices = get_video_adapter_devices();
   // g_ptr_array_free(devices, true);
   //   devices = get_video_adapter_devices2();   // FAILS

    DBGF(debug, "%d devices at %p:", devices->len, devices);
    for (int ndx = 0; ndx < devices->len; ndx++)
       rpt_vstring(2, "%s", g_ptr_array_index(devices, ndx));

   // bool result = false;
   bool all_drm = check_video_adapters_list_implements_drm(devices);
   g_ptr_array_free(devices, true);

   DBGF(debug, "Done.  Returning %s", sbool(all_drm));
   return all_drm;
}

