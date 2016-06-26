/* hiddev_util.h
 *
 * <copyright>
 * Copyright (C) 2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef SRC_UTIL_HIDDEV_UTIL_H_
#define SRC_UTIL_HIDDEV_UTIL_H_

#include <glib.h>
#include <linux/hiddev.h>

#include "util/data_structures.h"


#define REPORT_IOCTL_ERROR(_ioctl_name, _rc) \
   do { \
         printf("(%s) ioctl(%s) returned %d (0x%08x), errno=%d: %s\n", \
                __func__, \
                _ioctl_name, \
                _rc, \
                _rc, \
                errno, \
                strerror(errno) \
               ); \
   } while(0)


const char * report_type_name(__u32 report_type);

bool force_hiddev_monitor(int fd);

bool is_hiddev_monitor(int fd);

GPtrArray * get_hiddev_device_names();

char * get_hiddev_name(int fd);

Buffer * get_hiddev_edid(int fd);

struct hiddev_field_info *
is_field_edid(int fd, struct hiddev_report_info * rinfo, int field_index);

__u32 get_identical_ucode(int fd, struct hiddev_field_info * finfo, __u32 actual_field_index);

#ifdef OLD
Byte * get_hiddev_edid_base(
      int fd,
      struct hiddev_report_info * rinfo,
      int field_index);
#endif
#endif /* SRC_UTIL_HIDDEV_UTIL_H_ */
