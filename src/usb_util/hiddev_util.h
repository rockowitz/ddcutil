/* hiddev_util.h
 *
 * <copyright>
 * Copyright (C) 2016-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \f
 *
 */

#ifndef HIDDEV_UTIL_H_
#define HIDDEV_UTIL_H_

#include <glib.h>
#include <linux/hiddev.h>
// so callers will have definition of strerror() used in REPORT_IOCTL_ERROR macro:
#include <string.h>

#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/edid.h"


// The define is used within directory usb_util.
// The more general REPORT_USB_ERROR is defined in base/core.h, so is unavailable
// at the util level.

#define REPORT_USB_IOCTL_ERROR(_ioctl_name, _errno) \
   do { \
         printf("(%s) ioctl(%s) failed.  errno=%d: %s\n", \
                __func__, \
                _ioctl_name, \
                _errno, \
                strerror(_errno) \
               ); \
         printf("(%s) Backtrace:\n", __func__); \
         show_backtrace(2);   \
   } while(0)


#define HID_REPORT_TYPE_INPUT_FG   (1 << HID_REPORT_TYPE_INPUT)
#define HID_REPORT_TYPE_OUTPUT_FG  (1 << HID_REPORT_TYPE_OUTPUT)
#define HID_REPORT_TYPE_FEATURE_FG (1 << HID_REPORT_TYPE_FEATURE)


// Identifies a field within a report
// n. names of structs in hiddev.h begin with "hiddev_", so using that prefix
// for the following struct would be confusing
struct hid_field_locator {
   struct hiddev_field_info  * finfo;         // simplify by eliminating?
   __u32                       report_type;
   __u32                       report_id;
   __u32                       field_index;
};

void free_hid_field_locator(struct hid_field_locator * location);
void report_hid_field_locator(struct hid_field_locator * ploc, int depth);

// n. hiddev.h contains declarations of structs, but not functions, so
// prefixing the following functions with "hiddev_" does not lead to confusion


struct hid_field_locator*
hiddev_find_report(int fd, __u32 report_type, __u32 ucode, bool match_all_ucodes);
Buffer * hiddev_get_multibyte_report_value_by_hid_field_locator(int fd, struct hid_field_locator * loc);
Buffer * get_hiddev_edid(int fd);

Buffer * hiddev_get_multibyte_value_by_report_type_and_ucode(int fd, __u32 report_type, __u32 usage_code, __u32 num_values);
Buffer * hiddev_get_multibyte_value_by_ucode(int fd, __u32 usage_code, __u32 num_values);


const char * hiddev_report_type_name(__u32 report_type);

bool force_hiddev_monitor(int fd);

bool is_hiddev_monitor(int fd);

GPtrArray * get_hiddev_device_names();

char * get_hiddev_name(int fd);

bool hiddev_is_field_edid(int fd, struct hiddev_report_info * rinfo, int field_index);

__u32 hiddev_get_identical_ucode(int fd, struct hiddev_field_info * finfo, __u32 actual_field_index);

Buffer * hiddev_collect_single_byte_usage_values(
            int                         fd,
            struct hiddev_field_info *  finfo,
            __u32                       field_index);

#endif /* HIDDEV_UTIL_H_ */
