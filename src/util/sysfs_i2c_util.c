// sysfs_i2c_util.c

// Copyright (C) 2018-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "file_util.h"
#include "string_util.h"
#include "subprocess_util.h"
#include "sysfs_util.h"

#include "sysfs_i2c_util.h"


/** Looks in the /sys file system to check if a module is loaded.
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
   bool   found = false;

   snprintf(module_fn, sizeof(module_fn), "/sys/module/%s", module_name);
   int rc = stat(module_fn, &statbuf);
   if (rc < 0) {
      // will be ENOENT (2) if file not found
      found = false;
   }
   else {
      // if (S_ISDIR(statbuf.st_mode))   // pointless
         found = true;
   }

   if (debug)
      printf("(%s) module_name = %s, returning %d", __func__, module_name, found);
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
get_i2c_device_sysfs_name(
      int busno)
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
get_i2c_device_sysfs_driver(int busno) {
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


/** Gets the class of an I2C device,
 *  i.e. /sys/bus/i2c/devices/i2c-n/device/class
 *  or   /sys/bus/i2c/devices/i2c-n/device/device/device/class
 *
 *  \param  busno   I2C bus number
 *  \return device class
 *          0 if not found (should never occur)
 */
uint32_t get_i2c_device_sysfs_class(int busno) {
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


#ifdef UNUSED
static bool is_smbus_device_using_sysfs(int busno) {
   char * name = get_i2c_device_sysfs_name(busno);

   bool result = false;
   if (name && str_starts_with(name, "SMBus"))
      result = true;
   free(name);
   // DBGMSG("busno=%d, returning: %s", busno, bool_repr(result));
   return result;
}
#endif


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
 *
 *  \remark
 *  This function avoids unnecessary calls to i2cdetect, which can be
 *  slow for SMBus devices and fills the system logs with errors
 */
bool
sysfs_is_ignorable_i2c_device(int busno) {
   bool debug = false;
   bool result = false;
   uint32_t class = get_i2c_device_sysfs_class(busno);
   if (class) {
      // printf("(%s) class = 0x%08x\n", __func__, class);
      uint32_t cl2 = class & 0xffff0000;
      if (debug)
         printf("(%s) cl2 = 0x%08x\n", __func__, cl2);
      result = (cl2 != 0x030000 &&
                cl2 != 0x0a0000);    // docking station
   }
   else {
      char * name = get_i2c_device_sysfs_name(busno);
      char * driver = get_i2c_device_sysfs_driver(busno);
      if (name)
         result = ignorable_i2c_device_sysfs_name(name, driver);
      if (debug)
         printf("(%s) busno=%d, name=|%s|, result=%s\n", __func__, busno, name, sbool(result));
      free(name);    // safe if NULL
      free(driver);  // ditto
   }

   if (debug)
      printf("(%s) busno=%d, returning: %s\n", __func__, busno, sbool(result));
   return result;
}


int get_sysfs_drm_edid_count() {
   int ival = 0;
   GPtrArray * output = execute_shell_cmd_collect("ls /sys/class/drm/card*-*/edid | wc -w");
   if (output) {
      char * s = g_ptr_array_index(output, 0);
      bool ok = str_to_int(s, &ival, 10);
      assert(ok);
      g_ptr_array_free(output, true);
   }
   return ival;
}


