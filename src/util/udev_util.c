/* udev_util.c
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

// Adapted from source code at http://www.signal11.us/oss/udev/

/** @file udev_util.c
 * UDEV utility functions
 */

/** \cond */
#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "report_util.h"
#include "string_util.h"

#include "udev_util.h"


/* GDestroyNotify() function to be called when a data element in a GPtrRrray of
 * #Udev_Device_Summary is destroyed.
 *
 * Arguments:
 *    data    pointer to Udev_Device_Summary
 */
void free_udev_device_summary(gpointer data) {
   if (data) {
      Udev_Device_Summary * summary = (Udev_Device_Summary *) data;
      assert(memcmp(summary->marker, UDEV_DEVICE_SUMMARY_MARKER, 4) == 0);
      // no need to free strings, they are consts
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
 * using #free_udev_device_summary()
 */
Udev_Device_Summary * get_udev_device_summary(struct udev_device * dev) {
  Udev_Device_Summary * summary = calloc(1,sizeof(struct udev_device_summary));
  memcpy(summary->marker, UDEV_DEVICE_SUMMARY_MARKER, 4);
  // n. all strings returned are const char *
  summary->devpath      = udev_device_get_devpath(dev);
  summary->sysname      = udev_device_get_sysname(dev);
  summary->sysattr_name = udev_device_get_sysattr_value(dev, "name");
  summary->subsystem    = udev_device_get_subsystem(dev);
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
      printf("(%s) Can't create udev\n", __func__);
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
   }

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
   }

bye:
   return result;
}


/** Report on a single UDEV device
 *
 * @param  dev           pointer to struct **udev_device** to report
 * @param  depth         logical indentation depth
 */
void report_udev_device(struct udev_device * dev, int depth) {
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
      const char * prop_value2 = udev_device_get_property_value(dev, prop_name);
      assert(streq(prop_value, prop_value2));
      rpt_vstring(d2, "%s -> %s", prop_name, prop_value);
   }

   rpt_vstring(d1, "Sysattrs:");
   udev_list_entry_foreach(cur_entry, sysattr_list_entry) {
      const char * attr_name   = udev_list_entry_get_name(cur_entry);
      const char * attr_value  = udev_list_entry_get_value(cur_entry);
      const char * attr_value2 = udev_device_get_sysattr_value(dev, attr_name);
      assert(attr_value == NULL);
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
               rpt_vstring(d2, "%*s %s", strlen(attr_name) + 3, " ", ntsa[ndx]);
               ndx++;
            }
         }
         ntsa_free(ntsa);

#ifdef ALTERNATIVE
         // simpler, works
         char * av = strdup(attr_value2);
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
}
