/* udev_util.h
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

#ifndef UDEV_UTIL_H_
#define UDEV_UTIL_H_

#include <libudev.h>
#include <stdint.h>

void report_udev_device(struct udev_device * dev, int depth);
void query_udev_subsystem(char * udev_class, int depth);

typedef struct udev_usb_devinfo {
   uint16_t busno;
   uint16_t devno;
} Udev_Usb_Devinfo;

void report_hidraw_devinfo(struct udev_usb_devinfo * dinfo, int depth);
Udev_Usb_Devinfo * get_udev_device_info(char * subsystem, char * simple_devname);

#endif /* UDEV_UTIL_H_ */
