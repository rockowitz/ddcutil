/** usb_base.h */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef USB_BASE_H_
#define USB_BASE_H_

#include <linux/hiddev.h>
#include <stdint.h>

#include "util/coredefs.h"
#include "util/data_structures.h"

#include "base/status_code_mgt.h"

#ifdef UNUSED
#define REPORT_IOCTL_ERROR_AND_QUIT(_ioctl_name, _rc) \
   do { \
         printf("(%s) ioctl(%s) returned %d (0x%08x), errno=%d: %s\n", \
                __func__, \
                _ioctl_name, \
                _rc, \
                _rc, \
                errno, \
                strerror(errno) \
               ); \
         ddc_abort(errno); \
   } while(0)
#endif


int          usb_open_hiddev_device(char * hiddev_devname, Byte calloptions);
Status_Errno usb_close_device(int fd, char * device_fn, Byte calloptions);

Status_Errno hiddev_get_device_info(int fd, struct hiddev_devinfo *     dinfo, Byte calloptions);
Status_Errno hiddev_get_report_info(int fd, struct hiddev_report_info * rinfo, Byte calloptions);
Status_Errno hiddev_get_field_info( int fd, struct hiddev_field_info *  finfo, Byte calloptions);
Status_Errno hiddev_get_usage_code( int fd, struct hiddev_usage_ref *   uref,  Byte calloptions);
Status_Errno hiddev_get_usage_value(int fd, struct hiddev_usage_ref *   uref,  Byte calloptions);
Status_Errno hiddev_get_report(     int fd, struct hiddev_report_info * rinfo, Byte calloptions);

typedef uint32_t Vid_Pid_Value;
#define VID_PID_VALUE_TO_VID(_vid_pid) (_vid_pid >> 16)
#define VID_PID_VALUE_TO_PID(_vid_pid) (_vid_pid & 0xff)
#define VID_PID_VALUE(_vid,_pid) ( _vid << 16 | _pid)

void usb_ignore_hiddevs(Bit_Set_32 ignored_hiddevs);
bool usb_is_ignored_hiddev(uint8_t hiddev_number);
void usb_ignore_vid_pid_values(uint8_t ignored_ct, Vid_Pid_Value* ignored_vid_pids);
bool usb_is_ignored_vid_pid(uint16_t vid, uint16_t pid);
bool usb_is_ignored_vid_pid_value(Vid_Pid_Value vidpid);

void init_usb_base();
void terminate_usb_base();

#endif /* USB_BASE_H_ */
