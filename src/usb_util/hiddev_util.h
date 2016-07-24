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

#ifndef HIDDEV_UTIL_H_
#define HIDDEV_UTIL_H_

#include <glib.h>
#include <linux/hiddev.h>
// so callers will have definition of strerror() used in REPORT_IOCTL_ERROR macro:
#include <string.h>

#include "util/data_structures.h"
#include "util/edid.h"


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


#define HID_REPORT_TYPE_INPUT_FG   (1 << HID_REPORT_TYPE_INPUT)
#define HID_REPORT_TYPE_OUTPUT_FG  (1 << HID_REPORT_TYPE_OUTPUT)
#define HID_REPORT_TYPE_FEATURE_FG (1 << HID_REPORT_TYPE_FEATURE)


// Identifies a field within a report
struct hid_field_locator {
   struct hiddev_field_info  * finfo;         // simplify by eliminating?
   __u32                       report_type;
   __u32                       report_id;
   __u32                       field_index;
};

void free_hid_field_locator(struct hid_field_locator * location);
void report_hid_field_locator(struct hid_field_locator * ploc, int depth);

struct hid_field_locator*
find_report(int fd, __u32 report_type, __u32 ucode, bool match_all_ucodes);
Buffer * get_multibyte_report_value_by_hid_field_locator(int fd, struct hid_field_locator * loc);
Buffer * get_hiddev_edid(int fd);

Buffer * get_multibyte_value_by_report_type_and_ucode(int fd, __u32 report_type, __u32 usage_code, __u32 num_values);
Buffer * get_multibyte_value_by_ucode(int fd, __u32 usage_code, __u32 num_values);


const char * report_type_name(__u32 report_type);

bool force_hiddev_monitor(int fd);

bool is_hiddev_monitor(int fd);

GPtrArray * get_hiddev_device_names();

char * get_hiddev_name(int fd);

bool is_field_edid(int fd, struct hiddev_report_info * rinfo, int field_index);

__u32 get_identical_ucode(int fd, struct hiddev_field_info * finfo, __u32 actual_field_index);

Buffer * collect_single_byte_usage_values(
            int                         fd,
            struct hiddev_field_info *  finfo,
            __u32                       field_index);

#endif /* HIDDEV_UTIL_H_ */
