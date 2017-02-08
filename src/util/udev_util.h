/* udev_util.h
 *
 * <copyright>
 * Copyright (C) 2016-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <glib.h>
#include <libudev.h>
#include <stdint.h>

#define UDEV_DEVICE_SUMMARY_MARKER "UDSM"
typedef struct udev_device_summary {
   char   marker[4];
   const char * sysname;
   const char * devpath;
   const char * sysattr_name;
} Udev_Device_Summary;

void free_udev_device_summaries(GPtrArray* summaries);
GPtrArray * summarize_udev_subsystem_devices(char * subsystem);

void report_udev_device(struct udev_device * dev, int depth);
void probe_udev_subsystem(char * udev_class, bool show_usb_parent, int depth);


// Used for informational messages.
// Unless additional uses found, no need to convert from strings to integers
#define UDEV_DETAILED_DEVICE_SUMMARY_MARKER "UDDS"
typedef struct {
   char      marker[4];
   char *    devname;          // e.g. /dev/usb/hiddev2
   // int       usb_busnum;
   // int       usb_devnum;
   // uint16_t  vid;
   // uint16_t  pid;
   char *    vendor_id;
   char *    product_id;
   char *    vendor_name;
   char *    product_name;
   char *    busnum_s;
   char *    devnum_s;
} Usb_Detailed_Device_Summary;

void free_usb_detailed_device_summary(Usb_Detailed_Device_Summary * devsum);
void report_usb_detailed_device_summary(Usb_Detailed_Device_Summary * devsum, int depth);
Usb_Detailed_Device_Summary * lookup_udev_usb_device_by_devname(char * devname);


typedef struct udev_usb_devinfo {
   uint16_t busno;
   uint16_t devno;
} Udev_Usb_Devinfo;

void report_udev_usb_devinfo(struct udev_usb_devinfo * dinfo, int depth);
Udev_Usb_Devinfo * get_udev_usb_devinfo(char * subsystem, char * simple_devname);

#endif /* UDEV_UTIL_H_ */
