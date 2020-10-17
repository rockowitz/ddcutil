/** \file sysfs_util.h
 * Functions for reading /sys file system
 */

// Copyright (C) 2016-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later



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


char *
get_rpath_basename(char * path);

char *
get_i2c_device_sysfs_driver(int busno);

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
sysfs_is_ignorable_i2c_device(
      int busno);

#endif /* SYSFS_UTIL_H_ */
