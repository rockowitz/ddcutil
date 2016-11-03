/* libusb_util.h
 *
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef LIBUSB_UTIL_H_
#define LIBUSB_UTIL_H_

#include <libusb-1.0/libusb.h>
#include <stdint.h>

#include "usb_util/libusb_reports.h"


char *make_path(int bus_number, int device_address, int interface_number);
char *make_path_from_libusb_device(libusb_device *dev, int interface_number);

// bool possible_monitor_dev(libusb_device * dev, bool check_forced_monitor);

// singly linked list of possible monitors
typedef
struct possible_monitor_device {
   libusb_device * libusb_device;
   int             bus;
   int             device_address;
   int             alt_setting;
   int             interface;
   ushort          vid;
   ushort          pid;
   char *          manufacturer_name;
   char *          product_name;
   // conversion is annoying, just retrieve both ascii and wchar version of the serial number
   // wchar_t *       serial_number_wide;
   char *          serial_number;  // retrieved as ASCII, note some usages expect wchar
   struct possible_monitor_device * next;
} Possible_Monitor_Device;

struct possible_monitor_device * get_possible_monitors();

void report_possible_monitors(struct possible_monitor_device * mondev_head, int depth);

#ifdef UNTESTED
void free_possible_monitor_device_list(struct possible_monitor_device * head);
#endif

void probe_libusb(bool possible_monitors_only,int depth);

bool libusb_is_monitor_by_path(ushort busno, ushort devno, ushort intfno);

#endif /* LIBUSB_UTIL_H_ */
