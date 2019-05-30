 /** \file
  * USB specific udev utility functions
  */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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

#include "udev_usb_util.h"


Usb_Detailed_Device_Summary * new_usb_detailed_device_summary() {
   Usb_Detailed_Device_Summary * devsum = calloc(1, sizeof(Usb_Detailed_Device_Summary));
   memcpy(devsum->marker, UDEV_DETAILED_DEVICE_SUMMARY_MARKER, 4);
   return devsum;
}


/** Frees a Usb_Detailed_Device_Summary.
 *  All underlying memory is released.
 *
 * @param devsum pointer to **Usb_Detailed_Device_Summary**
 */
void free_usb_detailed_device_summary(Usb_Detailed_Device_Summary * devsum) {
   if (devsum) {
      assert( memcmp(devsum->marker, UDEV_DETAILED_DEVICE_SUMMARY_MARKER, 4) == 0);
      free(devsum->devname);
      free(devsum->vendor_id);
      free(devsum->product_id);
      free(devsum->vendor_name);
      free(devsum->product_name);
      free(devsum->busnum_s);
      free(devsum->devnum_s);
      free(devsum);
   }
}


/** Reports a Usb_Detailed_Device_Summary instance.
 *
 * @param devsum pointer to **Usb_Detailed_Device_Summary**
 * @param depth  logical indentation depth
 */
void report_usb_detailed_device_summary(Usb_Detailed_Device_Summary * devsum, int depth) {
   assert( devsum && (memcmp(devsum->marker, UDEV_DETAILED_DEVICE_SUMMARY_MARKER, 4) == 0));
   rpt_structure_loc("Usb_Detailed_Device_Summary", devsum, depth);
   int d1 = depth+1;

   rpt_str("devname",    NULL, devsum->devname,    d1);
   // rpt_int("usb_busnum", NULL, devsum->usb_busnum, d1);
   // rpt_int("usb_devnum", NULL, devsum->usb_devnum, d1);
   // rpt_int("vid",        NULL, devsum->vid, d1);
   // rpt_int("pid",        NULL, devsum->pid, d1);
   rpt_str("vendor_id",  NULL, devsum->vendor_id, d1);
   rpt_str("product_id",  "", devsum->product_id, d1);
   rpt_str("vendor_name", NULL,  devsum->vendor_name, d1);
   rpt_str("product_name",  NULL, devsum->product_name, d1);
   rpt_str("busnum_s",  NULL, devsum->busnum_s, d1);
   rpt_str("devnum_s",  NULL, devsum->devnum_s, d1);
}


/** Look up information for a device name.
 * The expected use in in error messages.
 *
 * @param  devname       device name, e.g. /dev/usb/hiddev3
 * @return pointer to newly allocated **Usb_Detailed_Device_Summary** struct,
 *         NULL if not found
 */
Usb_Detailed_Device_Summary * lookup_udev_usb_device_by_devname(char * devname) {
   // printf("(%s) Starting. devname=%s\n", __func__, devname);
   int depth = 0;
   // int d1 = depth+1;
   struct udev * udev;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *devices, *dev_list_entry;
   struct udev_device * dev = NULL;
   struct udev_device * dev0 = NULL;

   /* Create the udev object */
   udev = udev_new();
   if (!udev) {
      printf("(%s) Can't create udev\n", __func__);
      return NULL;   // exit(1);
   }

   Usb_Detailed_Device_Summary * devsum = new_usb_detailed_device_summary();
   devsum->devname = strdup(devname);

   /* Create a list of matching devices. */
   enumerate = udev_enumerate_new(udev);
   udev_enumerate_add_match_property(enumerate, "DEVNAME", devname);
   udev_enumerate_scan_devices(enumerate);
   devices = udev_enumerate_get_list_entry(enumerate);
   int devct = 0;
   /*  udev_list_entry_foreach is a macro which expands to
      a loop. The loop will be executed for each member in
      devices, setting dev_list_entry to a list entry
      which contains the device's path in /sys. */
   udev_list_entry_foreach(dev_list_entry, devices) {
      const char *path;

      /* Get the filename of the /sys entry for the device
         and create a udev_device object (dev) representing it */
      path = udev_list_entry_get_name(dev_list_entry);
      // rpt_vstring(depth, "path: %s", path);
      dev0 = udev_device_new_from_syspath(udev, path);

      /* udev_device_get_devnode() returns the path to the device node
         itself in /dev. */
      // rpt_vstring(depth, "Device Node Path: %s", udev_device_get_devnode(dev));

      // report_udev_device(dev, d1);

      /* The device pointed to by dev contains information about
         the named device. In order to get information about the
         USB device, get the parent device with the
         subsystem/devtype pair of "usb"/"usb_device". This will
         be several levels up the tree, but the function will find
         it.*/
      dev = udev_device_get_parent_with_subsystem_devtype(
             dev0,
             "usb",
             "usb_device");
      if (!dev) {
         rpt_vstring(depth, "Unable to find parent USB device.");
         udev_device_unref(dev0);
         continue;   // exit(1);   // TODO: fix
      }

      // puts("");
      // rpt_vstring(depth, "Parent device:");

      /* From here, we can call get_sysattr_value() for each file
         in the device's /sys entry. The strings passed into these
         functions (idProduct, idVendor, serial, etc.) correspond
         directly to the files in the directory which represents
         the USB device. Note that USB strings are Unicode, UCS2
         encoded, but the strings returned from
         udev_device_get_sysattr_value() are UTF-8 encoded. */

      devsum->vendor_id    = strdup( udev_device_get_sysattr_value(dev,"idVendor") );
      devsum->product_id   = strdup( udev_device_get_sysattr_value(dev,"idProduct") );
      devsum->vendor_name  = strdup( udev_device_get_sysattr_value(dev,"manufacturer") );
      devsum->product_name = strdup( udev_device_get_sysattr_value(dev,"product") );
      devsum->busnum_s     = strdup( udev_device_get_sysattr_value(dev,"busnum") );
      devsum->devnum_s     = strdup( udev_device_get_sysattr_value(dev,"devnum") );
      // report_udev_device(dev, d1);

      // udev_device_unref(dev);
      udev_device_unref(dev0);   // freeing dev0 also frees dev
      devct++;
   }
   /* Free the enumerator object */
   udev_enumerate_unref(enumerate);

   udev_unref(udev);

   if (devct != 1)
      printf("(%s) Unexpectedly found %d matching devices for %s\n", __func__, devct, devname);
   if (devct == 0) {
      free_usb_detailed_device_summary(devsum);
      devsum = NULL;
   }

   // if (devsum)
   //    report_usb_device_summary(devsum, 0);
   return devsum;


}


/** Reports on all devices in a UDEV subsystem
 *
 * @param  subsystem        subsystem name, e.g. "usbmisc"
 * @param  show_usb_parent  include reports on parent devices
 * @param  depth            logical indentation depth
 *
 * @remark
 * Adapted from USB sample code
 */
void probe_udev_subsystem(char * subsystem, bool show_usb_parent, int depth) {
   int d1 = depth+1;

   struct udev *udev;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *devices, *dev_list_entry;
   struct udev_device* dev  = NULL;
   struct udev_device* dev0 = NULL;

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
      puts("");
      rpt_vstring(depth, "***One Device ***");
      const char *path;

      /* Get the filename of the /sys entry for the device
         and create a udev_device object (dev) representing it */
      path = udev_list_entry_get_name(dev_list_entry);
      rpt_vstring(depth, "path: %s", path);
      dev0 = udev_device_new_from_syspath(udev, path);

      /* udev_device_get_devnode() returns the path to the device node
         itself in /dev. */
      rpt_vstring(depth, "Device Node Path: %s", udev_device_get_devnode(dev0));

      report_udev_device(dev0, d1);

      if (show_usb_parent) {

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
         }
         else {
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
         }

      }
      udev_device_unref(dev0);
   }
   /* Free the enumerator object */
   udev_enumerate_unref(enumerate);

   udev_unref(udev);

bye:
   return;
}


/** Reports on a struct **udev_usb_devinfo**.
 *
 * @param dinfo pointer to struct
 * @param depth logical indentation depth
 */
void report_udev_usb_devinfo(struct udev_usb_devinfo * dinfo, int depth) {
   rpt_structure_loc("udev_usb_devinfo", dinfo, depth);
   int d1 = depth+1;
   rpt_vstring(d1, "%-20s %d 0x%04x", "busno", dinfo->busno, dinfo->busno);
   rpt_vstring(d1, "%-20s %d 0x%04x", "devno", dinfo->devno, dinfo->devno);
}


/** Use UDEV to get the bus and device numbers for a USB device
 *
 *  @param   subsystem        device subsystem,   e.g. "usbmisc"
 *  @param   simple_devname   simple device name, e.g. "hiddev"
 *
 *  @return  pointer to Udev_Usb_Devinfo containing result\n
 *           NULL if not found
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


/** Encapsulates location of hiddev device files, in case it needs to be generalized

 \remark
According to file https://www.kernel.org/doc/Documentation/hid/hiddev.txt,
hiddev devices are always named /dev/usb/hiddevN (where n = 0..15).

However, searching the web finds statements that the hiddev device files
might be located in any of several distribution dependent directories, and
code examples that looks in multiple directories.

I suspect that for recent kernels/distributions the device file location is
fixed, but that variation existed in the past.

For now, assume the location is fixed.  If in fact it proves to be variable,
this function can be extended to use udev or other some other mechanisms to locate
the hiddev directory.
 */
char *
usb_hiddev_directory() {
   return "/dev/usb";
}

