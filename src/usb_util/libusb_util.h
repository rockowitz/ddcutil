/** @file libusb_util.h
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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
