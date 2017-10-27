/* sysfs_util.h
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

#ifndef SYSFS_UTIL_H_
#define SYSFS_UTIL_H_

/** \file 
 * Functions for reading /sys file system
 */


#endif /* SYSFS_UTIL_H_ */

#include <stdbool.h>
#include <glib-2.0/glib.h>

char *
read_sysfs_attr(
      char * dirname,
      char * attrname,
      bool verbose);

char *
read_sysfs_attr_w_default(
      char * dirname,
      char * attrname,
      char * default_value,
      bool verbose);

GByteArray *
read_binary_sysfs_attr(
      char * dirname,
      char * attrname,
      int    est_size,
      bool   verbose);

bool
is_module_loaded_using_sysfs(
      const char * module_name);
