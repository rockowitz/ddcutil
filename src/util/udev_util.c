/** @file udev_util.c
 *
 * UDEV utility functions
 */

// Copyright (C) 2016-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// Adapted from source code at http://www.signal11.us/oss/udev/

/** \cond */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "debug_util.h"
#include "report_util.h"
#include "string_util.h"

#include "udev_util.h"


/* GDestroyNotify() function to be called when a data element in a GPtrRrray of
 * #Udev_Device_Summary is destroyed.
 *
 * Arguments:
 *    data    pointer to Udev_Device_Summary
 */
static
void free_udev_device_summary(gpointer data) {
   if (data) {
      Udev_Device_Summary * summary = (Udev_Device_Summary *) data;
      assert(memcmp(summary->marker, UDEV_DEVICE_SUMMARY_MARKER, 4) == 0);
      summary->marker[3] = 'x';
      // no need to free strings, they are consts
      free(summary->devpath);
      free(summary->sysattr_name);
      free(summary->sysname);
      free(summary->subsystem);
   free(summary);
   }
}


/** Destroys a GPtrArray of Udev_Device_Summary
 *
 * @param summaries  pointer to GPtrArray of #Udev_Device_Summary
 */
void free_udev_device_summaries(GPtrArray* summaries) {
   g_ptr_array_free(summaries, true);
}


/** Returns a struct containing key fields extracted from a **struct udev_device**.
 *
 * \param dev pointer to struct udev_device
 * \return newly allocated #Udev_Device_Summary
 *
 * \remark
 * It is the responsibility of the caller to free the returned struct
 * using #free_udev_device_summary().
 * The strings pointed within #Udev_Device_Summary are consts
 * within the UDEV data structures and should not be freed.
 */
static
Udev_Device_Summary * get_udev_device_summary(struct udev_device * dev) {
  Udev_Device_Summary * summary = calloc(1,sizeof(struct udev_device_summary));
  memcpy(summary->marker, UDEV_DEVICE_SUMMARY_MARKER, 4);
  // n. all strings returned are const char *
  summary->devpath      = g_strdup(udev_device_get_devpath(dev));
  summary->sysname      = g_strdup(udev_device_get_sysname(dev));
  summary->sysattr_name = g_strdup(udev_device_get_sysattr_value(dev, "name"));
  summary->subsystem    = g_strdup(udev_device_get_subsystem(dev));
  return summary;
}


/** Queries UDEV to obtain summaries of each device in a subsystem.
 *
 * @param  subsystem    subsystem name, e.g. "i2c-dev"
 * @return GPtrArray of #Udev_Device_Summary
 *
 * @remark
 * Use #free_udev_device_summaries() to free the returned data structure.
 */
GPtrArray * summarize_udev_subsystem_devices(char * subsystem) {
   struct udev *udev;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *devices, *dev_list_entry;
   struct udev_device *dev;

   GPtrArray * summaries = g_ptr_array_sized_new(10);
   g_ptr_array_set_free_func(summaries, free_udev_device_summary);

   /* Create the udev object */
   udev = udev_new();
   if (!udev) {
      fprintf(stderr, "(%s) Can't create udev\n", __func__);
      goto bye;
   }

   /* Create a list of the devices in the specified subsystem. */
   enumerate = udev_enumerate_new(udev);
   udev_enumerate_add_match_subsystem(enumerate, subsystem);
   udev_enumerate_scan_devices(enumerate);
   devices = udev_enumerate_get_list_entry(enumerate);
   /* For each item enumerated, print out its information.
      udev_list_entry_foreach is a macro which expands to
      a loop. The loop will be executed for each member in
      devices, setting dev_list_entry to a list entry
      which contains the device's path in /sys. */
   udev_list_entry_foreach(dev_list_entry, devices) {
      const char *path;

      /* Get the filename of the /sys entry for the device
         and create a udev_device object (dev) representing it */
      path = udev_list_entry_get_name(dev_list_entry);
      dev = udev_device_new_from_syspath(udev, path);

      g_ptr_array_add(summaries, get_udev_device_summary(dev));
      udev_device_unref(dev);
   }
   udev_enumerate_unref(enumerate);
   udev_unref(udev);

bye:
   return summaries;
}


/** Queries udev to find all devices with a given name attribute
 *
 *  @param  name  e.g. DPMST
 *  @return GPtrArray of #Udev_Device_Summary
 *
 * @remark
 * Use #free_udev_device_summaries() to free the returned data structure.
 */
GPtrArray * find_devices_by_sysattr_name(char * name) {
   struct udev *udev;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *devices, *dev_list_entry;
   struct udev_device *dev;

   GPtrArray * result = g_ptr_array_sized_new(10);
   g_ptr_array_set_free_func(result, free_udev_device_summary);

   udev = udev_new();    // Create the udev object
   if (!udev) {
      printf("(%s) Can't create udev\n", __func__);
      goto bye;
   }

   /* Create a list of the devices in the specified subsystem. */
   enumerate = udev_enumerate_new(udev);
   udev_enumerate_add_match_sysattr(enumerate, "name", name);
   udev_enumerate_scan_devices(enumerate);
   devices = udev_enumerate_get_list_entry(enumerate);
   // udev_list_entry_foreach is a macro which expands to a loop.
   // The loop will be executed for each member in devices, setting dev_list_entry
   // to a list entry which contains the device's path in /sys.
   udev_list_entry_foreach(dev_list_entry, devices) {
      // Get the filename of the /sys entry for the device,
      // and create a udev_device object (dev) representing it
      const char * path = udev_list_entry_get_name(dev_list_entry);
      dev = udev_device_new_from_syspath(udev, path);

      g_ptr_array_add(result, get_udev_device_summary(dev));
      udev_device_unref(dev);
   }
   udev_enumerate_unref(enumerate);
   udev_unref(udev);

bye:
   return result;
}


/** Filters a **GPtrArray** of #Udev_Device_Summary removing
 *  entries that do not satisfy the predicate function provided.
 *
 *  summaries **GPtrArray** of #Udev_Device_Summary
 *  func      filter function, if returns false then delete the entry
 *  \return   NULL or **summaries**
 *
 *  \remark
 *  - if **summaries** is NULL, returns NULL
 *  - if **summaries** is non_NULL, but keep_func is NULL, returns **summaries**
 */
GPtrArray * filter_device_summaries(
      GPtrArray * summaries,
      Udev_Summary_Filter_Func keep_func)
{
   if (!summaries || !keep_func)
      goto bye;

   for (int ndx=0; ndx < summaries->len; ndx++) {
      Udev_Device_Summary * summary = g_ptr_array_index(summaries, ndx);
      if (!keep_func(summary))
         g_ptr_array_remove_index(summaries, ndx);
   }

bye:
   return summaries;
}


/** Report on a single UDEV device
 *
 * @param  dev           pointer to struct **udev_device** to report
 * @param  depth         logical indentation depth
 */
void report_udev_device(struct udev_device * dev, int depth) {
   bool debug = false;
   DBGF(debug, "Starting");
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_structure_loc("struct udev_device", dev, depth);
   rpt_vstring(d1, "devpath:   %s", udev_device_get_devpath(dev));
   rpt_vstring(d1, "subsystem: %s", udev_device_get_subsystem(dev));
   rpt_vstring(d1, "devtype:   %s", udev_device_get_devtype(dev));
   rpt_vstring(d1, "syspath:   %s", udev_device_get_syspath(dev));
   rpt_vstring(d1, "sysname:   %s", udev_device_get_sysname(dev));
   rpt_vstring(d1, "sysnum:    %s", udev_device_get_sysnum(dev));
   rpt_vstring(d1, "devnode:   %s", udev_device_get_devnode(dev));

   struct udev_list_entry * cur_entry;
   struct udev_list_entry * properties_list_entry;
   struct udev_list_entry * sysattr_list_entry;

   properties_list_entry = udev_device_get_properties_list_entry(dev);
   sysattr_list_entry    = udev_device_get_sysattr_list_entry(dev);

   rpt_vstring(d1, "Properties:");
   udev_list_entry_foreach(cur_entry, properties_list_entry) {
      const char * prop_name   = udev_list_entry_get_name(cur_entry);
      const char * prop_value  = udev_list_entry_get_value(cur_entry);
#ifndef NDEBUG
      const char * prop_value2 = udev_device_get_property_value(dev, prop_name);
      assert(streq(prop_value, prop_value2));
#endif
      rpt_vstring(d2, "%s -> %s", prop_name, prop_value);
   }

   rpt_vstring(d1, "Sysattrs:");
   udev_list_entry_foreach(cur_entry, sysattr_list_entry) {
      const char * attr_name   = udev_list_entry_get_name(cur_entry);
#ifndef NDEBUG
      const char * attr_value  = udev_list_entry_get_value(cur_entry);
      assert(attr_value == NULL);
#endif
      const char * attr_value2 = udev_device_get_sysattr_value(dev, attr_name);
      // hex_dump( (Byte*) attr_value2, strlen(attr_value2)+1);
      if (attr_value2 && strchr(attr_value2, '\n')) {
      // if (streq(attr_name, "uevent")) {
         // output is annoying to visually scan since it contains newlines
         Null_Terminated_String_Array ntsa = strsplit(attr_value2, "\n");
         if (ntsa_length(ntsa) == 0)
            rpt_vstring(d2, "%s -> %s", attr_name, "");
         else {
            rpt_vstring(d2, "%s -> %s", attr_name, ntsa[0]);
            int ndx = 1;
            while (ntsa[ndx]) {
               rpt_vstring(d2, "%*s %s", (int) strlen(attr_name) + 3, " ", ntsa[ndx]);
               ndx++;
            }
         }
         ntsa_free(ntsa, /* free_strings */ true);

#ifdef ALTERNATIVE
         // simpler, works
         char * av = g_strdup(attr_value2);
         char * p = av;
         while (*p) {
            if (*p == 0x0a)
               *p = ',';
            p++;
         }
         rpt_vstring(d2, "%s -> %s", attr_name, av);
         free(av);
#endif
      }
      // n. attr_name "descriptors" returns a hex value, not a null-terminated string
      //    should display as hex, but how to determine length?
      // for example of reading, see http://fossies.org/linux/systemd/src/udev/udev-builtin-usb_id.c
      // not worth pursuing
      else {
         rpt_vstring(d2, "%s -> %s", attr_name, attr_value2);
      }
   }
   DBGF(debug, "Done");
}


void show_udev_list_entries(
      struct udev_list_entry * entries,
      char * title)
{
   printf( "   %s: \n", title);
   struct udev_list_entry * cur = NULL;
   udev_list_entry_foreach(cur, entries) {
      const char * name  = udev_list_entry_get_name(cur);
      const char * value = udev_list_entry_get_value(cur);
      printf("      %s  -> %s\n", name, value);
   }
}


void show_sysattr_list_entries(
      struct udev_device *       dev,
      struct udev_list_entry * head)
{
   int d1 = 1;
   int d2 = 2;

   rpt_vstring(d1, "Sysattrs:");
   struct udev_list_entry * cur_entry = NULL;
   udev_list_entry_foreach(cur_entry, head) {
      const char * attr_name   = udev_list_entry_get_name(cur_entry);
#ifndef NDEBUG
      const char * attr_value  = udev_list_entry_get_value(cur_entry);
      assert(attr_value == NULL);
#endif
      const char * attr_value2 = udev_device_get_sysattr_value(dev, attr_name);
      // hex_dump( (Byte*) attr_value2, strlen(attr_value2)+1);
      if (attr_value2 && strchr(attr_value2, '\n')) {
      // if (streq(attr_name, "uevent")) {
         // output is annoying to visually scan since it contains newlines
         char * av = g_strdup(attr_value2);
         char * p = av;
         while (*p) {
            if (*p == 0x0a)
               *p = ',';
            p++;
         }
         rpt_vstring(d2, "%s -> %s", attr_name, av);
         free(av);
      }
      // n. attr_name "descriptors" returns a hex value, not a null-terminated string
      //    should display as hex, but how to determine length?
      // for example of reading, see http://fossies.org/linux/systemd/src/udev/udev-builtin-usb_id.c
      // not worth pursuing
      else {
         rpt_vstring(d2, "%s -> %s", attr_name, attr_value2);
      }
   }
}


void dbgrpt_udev_device(struct udev_device * dev, bool verbose, int depth) {
   rpt_structure_loc("udev_device", dev, depth);
   int d1 = depth+1;
   // printf("   Node: %s\n", udev_device_get_devnode(dev));         // /dev/dri/card0
   // printf("   Subsystem: %s\n", udev_device_get_subsystem(dev));  // drm
   // printf("   Devtype: %s\n", udev_device_get_devtype(dev));      // drm_minor

   rpt_vstring(d1, "action:      %s", udev_device_get_action(   dev));     // "change"
   rpt_vstring(d1, "devnode:     %s", udev_device_get_devnode(  dev));     // /dev/dri/card0
   rpt_vstring(d1, "devpath:     %s", udev_device_get_devpath(  dev));
   rpt_vstring(d1, "devtype:     %s", udev_device_get_devtype(  dev));     // drm_minor
   rpt_vstring(d1, "driver:      %s", udev_device_get_driver(  dev));
   rpt_vstring(d1, "initialized: %d", udev_device_get_is_initialized(  dev));
   rpt_vstring(d1, "subsystem:   %s", udev_device_get_subsystem(dev));     // drm
   rpt_vstring(d1, "sysname:     %s", udev_device_get_sysname(  dev));
   rpt_vstring(d1, "sysnum:      %s", udev_device_get_sysnum(   dev));
   rpt_vstring(d1, "syspath:     %s", udev_device_get_syspath(  dev));

   if (verbose) {
      struct udev_list_entry * entries = NULL;

#ifdef NOT_USEFUL     // see udevadm -p
      entries = udev_device_get_devlinks_list_entry(dev);
      show_udev_list_entries(entries, "devlinks");

      entries = udev_device_get_tags_list_entry(dev);
      show_udev_list_entries(entries, "tags");
#endif

      entries = udev_device_get_properties_list_entry(dev);
      show_udev_list_entries(entries, "properties");

      entries = udev_device_get_sysattr_list_entry(dev);
      //show_udev_list_entries(entries, "sysattrs");
      show_sysattr_list_entries(dev,entries);
   }
}

//
// Udev_Event_Detail
//

Udev_Event_Detail* collect_udev_event_detail(struct udev_device * dev) {
   Udev_Event_Detail * cd = calloc(1, sizeof(Udev_Event_Detail));
   cd->attr_name      = udev_device_get_sysattr_value(dev, "name");
   cd->prop_action    = udev_device_get_property_value(dev, "ACTION");     // always "changed"
   cd->prop_connector = udev_device_get_property_value(dev, "CONNECTOR");  // drm connector number
   cd->prop_devname   = udev_device_get_property_value(dev, "DEVNAME");    // e.g. /dev/dri/card0
   cd->prop_hotplug   = udev_device_get_property_value(dev, "HOTPLUG");    // always 1
   cd->prop_subsystem = udev_device_get_property_value(dev, "SUBSYSTEM");
   cd->sysname        = udev_device_get_sysname(dev);                      // e.g. card0, i2c-27
   cd->syspath        = udev_device_get_syspath(  dev);
   return cd;
}


void free_udev_event_detail(Udev_Event_Detail * detail) {
   free(detail);
}


void dbgrpt_udev_event_detail(Udev_Event_Detail * detail, int depth) {
   assert(detail);
   rpt_structure_loc("Udev_Event_Detail", detail, depth);
   int d1 = depth + 1;
   rpt_vstring(d1, "prop_subsystem:  %s", detail->prop_subsystem);
   rpt_vstring(d1, "prop_action:     %s", detail->prop_action);
   rpt_vstring(d1, "prop_connector:  %s", detail->prop_connector);
   rpt_vstring(d1, "prop_devname:    %s", detail->prop_devname);
   rpt_vstring(d1, "prop_hotplug:    %s", detail->prop_hotplug);
   rpt_vstring(d1, "sysname:         %s", detail->sysname);
   rpt_vstring(d1, "syspath:         %s", detail->syspath);
   rpt_vstring(d1, "attr_name:       %s", detail->attr_name);
}



#ifdef OLD

// use RPT_ATTR_TEXT(depth, return value loc, syspath, "status") instead

// Read sysfs "status" file for a DRM connector
const char* get_connector_status(const char *syspath) {
    static char status[32];
    char path[512];
    snprintf(path, sizeof(path), "%s/status", syspath);

    FILE *f = fopen(path, "r");
    if (!f) return "unknown";

    if (fgets(status, sizeof(status), f)) {
        // Strip newline
        status[strcspn(status, "\n")] = 0;
        fclose(f);
        return status;
    }

    fclose(f);
    return "unknown";
}

// RPT_ATTR_TEXT(depth, <value_loc>, syspath, "status");

#endif


void dbgrpt_udev_device_by_function_call(struct udev_device * dev, int depth) {
   int d0 = depth;

   rpt_vstring(d0, "action:      %s", udev_device_get_action(dev));    // changed
   rpt_vstring(d0, "devnode:     %s", udev_device_get_devnode(  dev));     // /dev/dri/card0
   rpt_vstring(d0, "devpath:     %s", udev_device_get_devpath(  dev));
   rpt_vstring(d0, "devtype:     %s", udev_device_get_devtype(  dev));     // drm_minor
   rpt_vstring(d0, "driver:      %s", udev_device_get_driver(  dev));
   rpt_vstring(d0, "initialized: %d", udev_device_get_is_initialized(  dev));
   rpt_vstring(d0, "subsystem:   %s", udev_device_get_subsystem(dev));     // drm
   rpt_vstring(d0, "sysname:     %s", udev_device_get_sysname(dev));
   rpt_vstring(d0, "sysnum:      %s", udev_device_get_sysnum(   dev));
   rpt_vstring(d0, "syspath:     %s", udev_device_get_syspath(dev));
   rpt_vstring(d0, "syspath:     %s", udev_device_get_syspath(  dev));
}




void dbgrpt_udev_list_entries(struct udev_list_entry * entries, char * title, int depth)
{
   int d1 = depth+1;
   rpt_vstring(depth, "%s:", title);
   struct udev_list_entry * cur = NULL;
   udev_list_entry_foreach(cur, entries) {
       const char * name  = udev_list_entry_get_name(cur);
       const char * value = udev_list_entry_get_value(cur);
       rpt_vstring(d1, "%s  -> %s", name, value);
   }
 }

void dbgrpt_udev_device_properties(struct udev_device* dev, int depth) {
   struct udev_list_entry * entries = NULL;
   entries = udev_device_get_properties_list_entry(dev);
   dbgrpt_udev_list_entries(entries, "properties",depth);
}


void dbgrpt_udev_device_sysattrs(struct udev_device* dev, int depth) {
   struct udev_list_entry * entries = NULL;
   entries = udev_device_get_sysattr_list_entry(dev);
   //show_udev_list_entries(entries, "sysattrs");
   show_sysattr_list_entries(dev,entries);
}


void dbgrpt_udev_device_lists(struct udev_device* dev, int depth) {
   rpt_vstring(depth, "Properties:");
   dbgrpt_udev_device_properties(dev, depth+1);
   dbgrpt_udev_device_sysattrs(dev, depth+1);

   struct udev_list_entry * entries = NULL;

#ifdef NOT_USEFUL     // see udevadm -p
   entries = udev_device_get_devlinks_list_entry(dev);
   show_udev_list_entries(entries, "devlinks");

   entries = udev_device_get_tags_list_entry(dev);
   show_udev_list_entries(entries, "tags");
#endif

   show_sysattr_list_entries(dev,entries);

}

