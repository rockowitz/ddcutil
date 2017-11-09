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

/** @file udev_util.h
 * UDEV utility functions
 */

#ifndef UDEV_UTIL_H_
#define UDEV_UTIL_H_

/** \cond */
#include <glib.h>
#include <libudev.h>
#include <stdint.h>
/** \endcond */

#define UDEV_DEVICE_SUMMARY_MARKER "UDSM"
/** Summary information for one UDEV device
 */
typedef struct udev_device_summary {
   char         marker[4];        ///< always "UDSM"
   const char * sysname;          ///< e.g. i2c-3
   const char * devpath;          ///< device path
   const char * sysattr_name;     ///< sysattr name
   const char * subsystem;        ///< subsystem, e.g. usbmisc
} Udev_Device_Summary;

void free_udev_device_summaries(GPtrArray* summaries);
GPtrArray * summarize_udev_subsystem_devices(char * subsystem);
GPtrArray * find_devices_by_sysattr_name(char * name);

// Function returns true if keeep, false if discard
typedef bool (*Udev_Summary_Filter_Func)(Udev_Device_Summary * summary);
GPtrArray * filter_device_summaries(GPtrArray * summaries, Udev_Summary_Filter_Func func);

void report_udev_device(struct udev_device * dev, int depth);

#endif /* UDEV_UTIL_H_ */
