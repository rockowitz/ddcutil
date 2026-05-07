/** @file udev_util.h
 * UDEV utility functions
 */

// Copyright (C) 2016-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef UDEV_UTIL_H_
#define UDEV_UTIL_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <libudev.h>
#include <stdbool.h>
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
   char * devnode;          ///< device node path, e.g. /dev/i2c-3
   bool   is_readable;      ///< real UID has read access
   bool   is_writable;      ///< real UID has write access
   bool   is_ereadable;     ///< effective UID has read access
   bool   is_ewritable;     ///< effective UID has write access
   char * mode_string;      ///< device file mode, e.g. "0660"
} Udev_Device_Summary;

void free_udev_device_summaries(GPtrArray* summaries);
GPtrArray * summarize_udev_subsystem_devices(char * subsystem);
void dbgrpt_udev_device_summary(Udev_Device_Summary * summary, int depth);
void dbgrpt_udev_device_summaries(GPtrArray * device_summaries, int depth);
GPtrArray * find_devices_by_sysattr_name(char * name);

// Function returns true if keep, false if discard
typedef bool (*Udev_Summary_Filter_Func)(Udev_Device_Summary * summary);
GPtrArray * filter_device_summaries(GPtrArray * summaries, Udev_Summary_Filter_Func func);

void report_udev_device(struct udev_device * dev, int depth);
void dbgrpt_udev_device(struct udev_device * dev, bool verbose, int depth);
void dbgrpt_udev_device_by_function_call(struct udev_device * dev, int depth);
void dbgrpt_udev_list_entries(struct udev_list_entry * entries, char * title, int depth);
void dbgrpt_udev_device_properties(struct udev_device * dev, int depth);
void dbgrpt_udev_device_sysattrs(struct udev_device * dev, int depth);
void dbgrpt_udev_device_lists(struct udev_device * dev, int depth);

void show_udev_list_entries(
      struct udev_list_entry * entries,
      char * title);
void show_sysattr_list_entries(
      struct udev_device *     dev,
      struct udev_list_entry * head);

typedef struct {
   const char * prop_subsystem;
   const char * prop_action;
   const char * prop_connector;
   const char * prop_devname;
   const char * prop_devmode;
   const char * prop_hotplug;
   const char * prop_major;
   const char * prop_minor;
   const char * sysname;
   const char * syspath;
   const char * attr_name;  // value of the attribute whose name is "name"
} Udev_Event_Detail;

Udev_Event_Detail* collect_udev_event_detail(struct udev_device * dev);
void free_udev_event_detail(Udev_Event_Detail * detail);
void dbgrpt_udev_event_detail(Udev_Event_Detail * detail, int depth);
void dbgrpt_udev_event_basic_detail(Udev_Event_Detail * detail, int depth);
GPtrArray* udev_event_detail_to_collector(Udev_Event_Detail * detail, GPtrArray* collector);

#endif /* UDEV_UTIL_H_ */
