/** @file udev_util.h
 * UDEV utility functions
 */

// Copyright (C) 2016-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef UDEV_UTIL_H_
#define UDEV_UTIL_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <libudev.h>
#include <stdint.h>
/** \endcond */

#define UDEV_DEVICE_SUMMARY_MARKER "UDSM"
/** Summary information for one UDEV device
 */
typedef struct udev_device_summary {
   char   marker[4];        ///< always "UDSM"
   char * sysname;          ///< e.g. i2c-3
   char * devpath;          ///< device path
   char * sysattr_name;     ///< sysattr name
   char * subsystem;        ///< subsystem, e.g. usbmisc
} Udev_Device_Summary;

void free_udev_device_summaries(GPtrArray* summaries);
GPtrArray * summarize_udev_subsystem_devices(char * subsystem);
GPtrArray * find_devices_by_sysattr_name(char * name);

// Function returns true if keeep, false if discard
typedef bool (*Udev_Summary_Filter_Func)(Udev_Device_Summary * summary);
GPtrArray * filter_device_summaries(GPtrArray * summaries, Udev_Summary_Filter_Func func);

void report_udev_device(struct udev_device * dev, int depth);

#endif /* UDEV_UTIL_H_ */
