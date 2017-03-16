/* usb_base.h
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

#ifndef USB_BASE_H_
#define USB_BASE_H_

#include <linux/hiddev.h>

#include "util/coredefs.h"


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


int usb_open_hiddev_device(char * hiddev_devname, Byte calloptions);
Status_Errno usb_close_device(int fd, char * device_fn, Byte calloptions);

int hiddev_get_device_info(int fd, struct hiddev_devinfo *     dinfo, Byte calloptions);
int hiddev_get_report_info(int fd, struct hiddev_report_info * rinfo, Byte calloptions);
int hiddev_get_field_info( int fd, struct hiddev_field_info *  finfo, Byte calloptions);
int hiddev_get_usage_code( int fd, struct hiddev_usage_ref *   uref,  Byte calloptions);
int hiddev_get_usage_value(int fd, struct hiddev_usage_ref *   uref,  Byte calloptions);
int hiddev_get_report(     int fd, struct hiddev_report_info * rinfo, Byte calloptions);


#endif /* USB_BASE_H_ */
