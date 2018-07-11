/* sysfs_util.h
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef SYSFS_UTIL_H_
#define SYSFS_UTIL_H_

#include <stdbool.h>
#include <glib-2.0/glib.h>

char *
read_sysfs_attr(
      const char * dirname,
      const char * attrname,
      bool         verbose);

char *
read_sysfs_attr_w_default(
      const char * dirname,
      const char * attrname,
      const char * default_value,
      bool         verbose);

char *
read_sysfs_attr_w_default_r(
      const char * dirname,
      const char * attrname,
      const char * default_value,
      char *       buf,
      unsigned     bufsz,
      bool         verbose);

GByteArray *
read_binary_sysfs_attr(
      const char * dirname,
      const char * attrname,
      int          est_size,
      bool         verbose);

bool
is_module_loaded_using_sysfs(
      const char * module_name);

char *
get_i2c_device_sysfs_name(
      int busno);

//bool
//ignorable_i2c_device_sysfs_name(
//      const char * name,
//      const char * driver);

bool
is_ignorable_i2c_device(
      int busno);

#endif /* SYSFS_UTIL_H_ */
