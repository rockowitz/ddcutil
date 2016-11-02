/* udev_util.c
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

// Adapted from source code at http://www.signal11.us/oss/udev/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/report_util.h"
#include "util/string_util.h"

#include "util/udev_util.h"


/* Report on a single udev device
 *
 * Arguments:
 *   dev           pointer to struct_udev_device to report
 *   depth         logical indentation depth
 *
 * Returns:        nothing
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
         if (null_terminated_string_array_length(ntsa) == 0)
            rpt_vstring(d2, "%s -> %s", attr_name, "");
         else {
            rpt_vstring(d2, "%s -> %s", attr_name, ntsa[0]);
            int ndx = 1;
            while (ntsa[ndx]) {
               rpt_vstring(d2, "%*s %s", strlen(attr_name) + 3, " ", ntsa[ndx]);
               ndx++;
            }
         }
         null_terminated_string_array_free(ntsa);

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
      else
         rpt_vstring(d2, "%s -> %s", attr_name, attr_value2);

   }
}


/* Reports on all devices in a udev subsystem
 *
 * Arguments:
 *   subsystem      subsystem name, e.g. "usbmisc"
 *   depth          logical indentation depth
 *
 * Returns:         nothing
 *
 * Adapted from USB sample code
 */
void probe_udev_subsystem(char * subsystem, bool show_usb_parent, int depth) {
   int d1 = depth+1;

   struct udev *udev;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *devices, *dev_list_entry;
   struct udev_device *dev;

   /* Create the udev object */
   udev = udev_new();
   if (!udev) {
      printf("(%s) Can't create udev\n", __func__);
      return;   // exit(1);
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
      puts("");
      rpt_vstring(depth, "***One Device ***");
      const char *path;

      /* Get the filename of the /sys entry for the device
         and create a udev_device object (dev) representing it */
      path = udev_list_entry_get_name(dev_list_entry);
      rpt_vstring(depth, "path: %s", path);
      dev = udev_device_new_from_syspath(udev, path);

      /* udev_device_get_devnode() returns the path to the device node
         itself in /dev. */
      rpt_vstring(depth, "Device Node Path: %s", udev_device_get_devnode(dev));

      report_udev_device(dev, d1);

      if (!show_usb_parent)
         continue;

      /* The device pointed to by dev contains information about
         the hidraw device. In order to get information about the
         USB device, get the parent device with the
         subsystem/devtype pair of "usb"/"usb_device". This will
         be several levels up the tree, but the function will find
         it.*/
      dev = udev_device_get_parent_with_subsystem_devtype(
             dev,
             "usb",
             "usb_device");
      if (!dev) {
         rpt_vstring(depth, "Unable to find parent USB device.");
         continue;   // exit(1);
      }

      puts("");
      rpt_vstring(depth, "Parent device:");

      /* From here, we can call get_sysattr_value() for each file
         in the device's /sys entry. The strings passed into these
         functions (idProduct, idVendor, serial, etc.) correspond
         directly to the files in the directory which represents
         the USB device. Note that USB strings are Unicode, UCS2
         encoded, but the strings returned from
         udev_device_get_sysattr_value() are UTF-8 encoded. */
      rpt_vstring(d1, "VID/PID: %s %s",
                      udev_device_get_sysattr_value(dev,"idVendor"),
                      udev_device_get_sysattr_value(dev, "idProduct"));
      rpt_vstring(d1, "%s",
              udev_device_get_sysattr_value(dev,"manufacturer") );
      rpt_vstring(d1, "%s",
              udev_device_get_sysattr_value(dev,"product"));
      rpt_vstring(d1, "serial: %s",
               udev_device_get_sysattr_value(dev, "serial"));

      report_udev_device(dev, d1);

      udev_device_unref(dev);
   }
   /* Free the enumerator object */
   udev_enumerate_unref(enumerate);

   udev_unref(udev);

   return;
}


void report_udev_usb_devinfo(struct udev_usb_devinfo * dinfo, int depth) {
   rpt_structure_loc("udev_usb_devinfo", dinfo, depth);
   int d1 = depth+1;
   rpt_vstring(d1, "%-20s %d 0x%04x", "busno", dinfo->busno, dinfo->busno);
   rpt_vstring(d1, "%-20s %d 0x%04x", "devno", dinfo->devno, dinfo->devno);
}


/* Use udev to get the bus and device numbers for a USB device
 *
 * Arguments:
 *    subsystem        device subsystem,   e.g. "usbmisc"
 *    simple_devname   simple device name, e.g. "hiddev"
 *
 * Returns:            pointer to Udev_Usb_Devinfo containing result
 *                     NULL if not found
 *
 * Adapted from UDEV sample code.
 */
Udev_Usb_Devinfo * get_udev_usb_devinfo(char * subsystem, char * simple_devname) {
   assert(subsystem);
   assert(simple_devname);

   bool debug = false;
   if (debug)
      printf("(%s) Starting. subsystem=|%s|, simple_devname=|%s|\n",
             __func__, subsystem, simple_devname);

   struct udev *udev;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *dev_list_entry;
   struct udev_device *dev;

   Udev_Usb_Devinfo * result = NULL;

   udev = udev_new();    // create udev context object
   if (!udev) {                   // should never happen
      printf("(%s) Can't create udev\n", __func__);
      goto bye;
   }

   /* Create a list of the devices in the specified subsystem. */
   enumerate = udev_enumerate_new(udev);
   udev_enumerate_add_match_subsystem(enumerate, subsystem);
   udev_enumerate_add_match_sysname(enumerate, simple_devname);
   udev_enumerate_scan_devices(enumerate);
   // Given the specificity of our search, list should contain exactly 0 or 1 entries
   dev_list_entry = udev_enumerate_get_list_entry(enumerate);  // get first entry of list
   if (dev_list_entry) {
      assert( udev_list_entry_get_next(dev_list_entry) == NULL);   // should be 0 or 1 devices

      /* Get the filename of the /sys entry for the device
         and create a udev_device object (dev) representing it */
      const char * path = udev_list_entry_get_name(dev_list_entry);
      // printf("path: %s\n", path);
      dev = udev_device_new_from_syspath(udev, path);

      // udev_device_get_devnode() returns the path to the device node
      //   itself in /dev, e.g. /dev/hidraw3, /dev/usb/hiddev2
      if (debug)
         printf("(%s) Device Node Path: %s\n", __func__, udev_device_get_devnode(dev));

      // report_udev_device(dev, 1);

      /* The device pointed to by dev contains information about
         the hidraw device. In order to get information about the
         USB device, get the parent device with the
         subsystem/devtype pair of "usb"/"usb_device". This will
         be several levels up the tree, but the function will find
         it.*/
      dev = udev_device_get_parent_with_subsystem_devtype(
             dev,
             "usb",
             "usb_device");
      if (!dev) {
         printf("(%s) Unable to find parent USB device for subsystem %s, device %s.",
                __func__, subsystem, simple_devname);
      }
      else {
         // printf("Parent device: \n");

         /* From here, we can call get_sysattr_value() for each file
            in the device's /sys entry. The strings passed into these
            functions (idProduct, idVendor, serial, etc.) correspond
            directly to the files in the directory which represents
            the USB device. Note that USB strings are Unicode, UCS2
            encoded, but the strings returned from
            udev_device_get_sysattr_value() are UTF-8 encoded. */
         if (debug) {
            printf("  VID/PID: %s %s\n",
                    udev_device_get_sysattr_value(dev,"idVendor"),
                    udev_device_get_sysattr_value(dev, "idProduct"));
            printf("  %s\n  %s\n",
                    udev_device_get_sysattr_value(dev,"manufacturer"),
                    udev_device_get_sysattr_value(dev,"product"));
            printf("  serial: %s\n",
                     udev_device_get_sysattr_value(dev, "serial"));
            printf("  busnum: %s\n",
                     udev_device_get_sysattr_value(dev, "busnum"));
            printf("  devnum: %s\n",
                     udev_device_get_sysattr_value(dev, "devnum"));
         }

         const char *  sbusnum = udev_device_get_sysattr_value(dev, "busnum");
         const char *  sdevnum = udev_device_get_sysattr_value(dev, "devnum");

         result = calloc(1, sizeof(Udev_Usb_Devinfo));
         //are these decimal or hex numbers?
         result->busno = atoi(sbusnum);
         result->devno = atoi(sdevnum);

         // report_udev_device(dev, 1);

         udev_device_unref(dev);
      }
   }
   /* Free the enumerator object */
   udev_enumerate_unref(enumerate);

   udev_unref(udev);

bye:
   if (debug) {
      printf("(%s) Returning: %p\n", __func__, result);
      if (result)
         report_udev_usb_devinfo(result, 1);
   }

   return result;
}

