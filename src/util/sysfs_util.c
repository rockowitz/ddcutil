/* sysfs_util.c
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

/** \file
 * Functions for reading /sys file system
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_util.h"
#include "string_util.h"

#include "sysfs_util.h"


/** Reads a /sys attribute file, which is 1 line of text
 *
 * \param  dirname    directory name
 * \param  attrname   attribute name, i.e. file name
 * \param  verbose    if true, write message to stderr if unable to open file
 * \return pointer to attribute value string, caller is responsible for freeing
 */
char *
read_sysfs_attr(
      const char * dirname,
      const char * attrname,
      bool         verbose)
{
   char fn[PATH_MAX];
   sprintf(fn, "%s/%s", dirname, attrname);
   return file_get_first_line(fn, verbose);
}


/** Reads a /sys attribute file, which is 1 line of text.
 *  If the attribute is not found, returns a default value
 *
 * \param  dirname        directory name
 * \param  attrname       attribute name, i.e. file name
 * \param  default_value  default value, duplicated
 * \param  verbose        if true, write message to stderr if unable to open file
 * \return pointer to attribute value string, caller is responsible for freeing
 */
char *
read_sysfs_attr_w_default(
      const char * dirname,
      const char * attrname,
      const char * default_value,
      bool         verbose)
{
   char fn[PATH_MAX];
   sprintf(fn, "%s/%s", dirname, attrname);
   char * result = file_get_first_line(fn, verbose);
   if (!result)
      result = strdup(default_value);  // strdup() so caller can free any result
   return result;
}

char *
read_sysfs_attr_w_default_r(
      const char * dirname,
      const char * attrname,
      const char * default_value,
      char *       buf,
      unsigned     bufsz,
      bool         verbose)
{
   char fn[PATH_MAX];
   sprintf(fn, "%s/%s", dirname, attrname);
   char * result = file_get_first_line(fn, verbose);
   if (result) {
      g_strlcpy(buf, result, bufsz);
      free(result);
   }
   else {
      g_strlcpy(buf, default_value, bufsz);
   }
   return buf;
}



/** Reads a binary /sys attribute file
 *
 * \param  dirname    directory name
 * \param  attrname   attribute name, i.e. file name
 * \param  est_size  estimated size
 * \param  verbose   if open fails, write message to stderr
 * \return if successful, a **GByteArray** of bytes, caller is responsible for freeing
 *          if failure, then NULL
 */
GByteArray *
read_binary_sysfs_attr(
      const char * dirname,
      const char * attrname,
      int          est_size,
      bool         verbose)
{
   assert(dirname);
   assert(attrname);

   char fn[PATH_MAX];
   sprintf(fn, "%s/%s", dirname, attrname);

   return read_binary_file(fn, est_size, verbose);
}


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
 *  i.e. the value of /sys/bus/in2c/devices/i2c-n/name
 *
 *  \param  busno   I2C bus number
 *  \return newly allocated string containing attribute value,
 *          NULL if not found
 *
 *  \remark
 *  Caller is responsible for freeing returned value
 */
char * get_i2c_device_sysfs_name(int busno) {
   char workbuf[50];
   snprintf(workbuf, 50, "/sys/bus/i2c/devices/i2c-%d/name", busno);
   char * name = file_get_first_line(workbuf, /*verbose */ false);
   // DBGMSG("busno=%d, returning: %s", busno, bool_repr(result));
   return name;
}


/** Gets the driver name of an I2C device,
 *  i.e. the basename of /sys/bus/in2c/devices/i2c-n/device/driver/module
 *
 *  \param  busno   I2C bus number
 *  \return newly allocated string containing driver name
 *          NULL if not found
 *
 *  \remark
 *  Caller is responsible for freeing returned value
 */
static char * get_i2c_device_sysfs_driver(int busno) {
   char * driver_name = NULL;
   char workbuf[100];
   snprintf(workbuf, 100, "/sys/bus/i2c/devices/i2c-%d/device/driver/module", busno);

   char resolved_path[PATH_MAX];
   char * rpath = realpath(workbuf, resolved_path);
   if (!rpath) {
      int errsv = errno;
      if (errsv == ENOENT) {
         // Path does not exist for amdgpu driver, perhaps others
      }
      else {
         printf("(%s) realpath(%s) returned NULL, errno=%d", __func__, workbuf, errsv);
      }
   }
   else {
      // printf("realpath returned %s\n", rpath);
      // printf("%s --> %s\n",workfn, resolved_path);
      driver_name = g_path_get_basename(rpath);
   }
   // printf("(%s) busno=%d, returning %s\n", __func__, busno, driver_name);
   return driver_name;
}


#ifdef UNUSED
static bool is_smbus_device_using_sysfs(int busno) {
#ifdef OLD
   char workbuf[50];
   snprintf(workbuf, 50, "/sys/bus/i2c/devices/i2c-%d/name", busno);
   char * name = file_get_first_line(workbuf, /*verbose */ false);
#endif
   char * name = get_i2c_device_sysfs_name(busno);

   bool result = false;
   if (name && str_starts_with(name, "SMBus"))
      result = true;
   free(name);
   // DBGMSG("busno=%d, returning: %s", busno, bool_repr(result));
   return result;
}
#endif


static bool ignorable_i2c_device_sysfs_name(const char * name, const char * driver) {
   bool result = false;
   const char * ignorable_prefixes[] = {
         "SMBus",
         "soc:i2cdsi",   // Raspberry Pi
         "smu",          // Mac G5, probing causes system hang
         "mac-io",       // Mac G5
         "u4",           // Mac G5
         NULL };
   if (name) {
      if (starts_with_any(name, ignorable_prefixes) >= 0)
         result = true;
      if (streq(driver, "nouveau")) {
         if ( !str_starts_with(name, "nkvm-") ) {
            result = true;
            printf("(%s) name=|%s|, driver=|%s| - Ignore\n", __func__, name, driver);
         }
      }
   }
   // printf("(%s) name=|%s|, driver=|%s|, returning: %s\n", __func__, name, driver, bool_repr(result));
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
bool is_ignorable_i2c_device(int busno) {
   bool result = false;
   char * name = get_i2c_device_sysfs_name(busno);
   char * driver = get_i2c_device_sysfs_driver(busno);
   if (name)
      result = ignorable_i2c_device_sysfs_name(name, driver);

   // printf("(%s) busno=%d, name=|%s|, returning: %s\n", __func__, busno, name, bool_repr(result));
   free(name);    // safe if NULL
   free(driver);  // ditto
   return result;
}



