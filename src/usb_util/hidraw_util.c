/** @file hidraw_util.c
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <libudev.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "util/coredefs.h"
#include "util/file_util.h"
#include "util/glib_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/udev_util.h"
#include "util/udev_usb_util.h"

#include "usb_util/base_hid_report_descriptor.h"
#include "usb_util/hid_report_descriptor.h"
#include "usb_util/usb_hid_common.h"

#include "usb_util/hidraw_util.h"


//
// *** Functions to identify hidraw devices representing monitors ***
//

/* Filter to find hiddevN files for scandir() */
static int is_hidraw(const struct dirent *ent) {
   return !strncmp(ent->d_name, "hidraw", strlen("hidraw"));
}


GPtrArray * get_hidraw_device_names_using_filesys() {
   const char *hidraw_paths[] = { "/dev/", NULL };
   // Dirent_Filter filterfunc = is_hidraw;
   return get_filenames_by_filter(hidraw_paths, is_hidraw);
}


//
// Utility functions
//

const char *
bus_str(int bus)
{
   switch (bus) {
   case BUS_USB:
      return "USB";
      break;
   case BUS_HIL:
      return "HIL";
      break;
   case BUS_BLUETOOTH:
      return "Bluetooth";
      break;
   case BUS_VIRTUAL:
      return "Virtual";
      break;
   default:
      return "Other";
      break;
   }
}


//
// Probe hidraw devices
//

void probe_hidraw_device(char * devname, bool show_monitors_only,  int depth) {
   bool debug = false;
   puts("");
   rpt_vstring(depth, "Probing device %s", devname);
   // printf("(%s) %s\n", __func__, devname);
   int d1 = depth+1;
   int d2 = depth+2;

   int fd;
   // int i;
   int res, desc_size = 0;
   Byte buf[1024] = {0};     // initialize to avoid valgrind warning

   struct hidraw_report_descriptor rpt_desc;
   struct hidraw_devinfo info;

   memset(&rpt_desc, 0x0, sizeof(rpt_desc));
   memset(&info,     0x0, sizeof(info));
   // memset(buf,       0x0, sizeof(buf));

   /* Open the Device with non-blocking reads. In real life,
      don't use a hard coded path; use libudev instead. */
   fd = open(devname, O_RDWR|O_NONBLOCK);

   if (fd < 0) {
      rpt_vstring(depth, "Unable to open device %s: %s", devname, strerror(errno));
      Usb_Detailed_Device_Summary * devsum = lookup_udev_usb_device_by_devname(devname, true);
      if (devsum) {
         // report_usb_detailed_device_summary(devsum, 2);
         rpt_vstring(d1, "USB bus %s, device %s, vid:pid: %s:%s - %s:%s",
                         devsum->busnum_s,
                         devsum->devnum_s,
                         devsum->vendor_id,
                         devsum->product_id,
                         devsum->vendor_name,
                         devsum->product_name);
         free_usb_detailed_device_summary(devsum);
      }
      goto bye;
   }

   /* Get Raw Name */
   res = ioctl(fd, HIDIOCGRAWNAME(256), buf);
   if (res < 0) {
      perror("HIDIOCGRAWNAME");
      goto bye;
   }
   rpt_vstring(d1, "Raw Name: %s", buf);

   /* Get Physical Location */
   res = ioctl(fd, HIDIOCGRAWPHYS(256), buf);
   if (res < 0) {
      perror("HIDIOCGRAWPHYS");
      goto bye;
   }
   rpt_vstring(d1, "Raw Phys: %s", buf);

   /* Get Raw Info */
   res = ioctl(fd, HIDIOCGRAWINFO, &info);
   if (res < 0) {
      perror("HIDIOCGRAWINFO");
      goto bye;
   }
   rpt_vstring(d1, "Raw Info:");
   rpt_vstring(d2, "bustype: %d (%s)", info.bustype, bus_str(info.bustype));
   rpt_vstring(d2, "vendor:  0x%04hx", info.vendor);
   rpt_vstring(d2, "product: 0x%04hx", info.product);


   // Get bus and device numbers using udev since hidraw doesn't report it
   char * simple_devname = strstr(devname, "hidraw");
   Udev_Usb_Devinfo * dinfo = get_udev_usb_devinfo("hidraw", simple_devname);
   if (dinfo) {
      rpt_vstring(d1, "Busno:Devno as reported by get_udev_usb_devinfo() for %s: %03d:%03d",
                      simple_devname, dinfo->busno, dinfo->devno);
      free(dinfo);
   }
   else
      rpt_vstring(d1, "Error getting busno:devno using get_udev_usb_devinfo()");

   /* Get Report Descriptor Size */
   // why is this necessary? buffer in rpt_desc is already HID_MAX_DESCRIPTOR_SIZE?
   res = ioctl(fd, HIDIOCGRDESCSIZE, &desc_size);
   if (res < 0) {
      perror("HIDIOCGRDESCSIZE");
      goto bye;
   }
   if (debug)
      rpt_vstring(d1, "Report Descriptor Size: %d", desc_size);
   // bool is_monitor = false;

   /* Get Report Descriptor */
   rpt_desc.size = desc_size;
   res = ioctl(fd, HIDIOCGRDESC, &rpt_desc);
   if (res < 0) {
      perror("HIDIOCGRDESC");
      goto bye;
   }
   if (debug) {
      rpt_vstring(d1, "Report Descriptor:");
      // for (i = 0; i < rpt_desc.size; i++)
      //    printf("%hhx ", rpt_desc.value[i]);
      // puts("\n");
      rpt_hex_dump(rpt_desc.value, rpt_desc.size, d2);
   }
   Hid_Report_Descriptor_Item * report_item_list = tokenize_hid_report_descriptor(rpt_desc.value, rpt_desc.size) ;
   // report_hid_report_item_list(report_item_list, d2);
   bool is_monitor = is_monitor_by_tokenized_hid_report_descriptor(report_item_list);

#ifdef OLD
   Hid_Report_Descriptor_Item * cur_item = report_item_list;
   // Look at the first Usage Page item, is it USB Monitor?
   while (cur_item) {
      if (cur_item->btag == 0x04) {
         if (cur_item->data == 0x80)
            is_monitor = true;
         break;
      }
      cur_item = cur_item->next;
   }
#endif

   rpt_vstring(d1, "%s a USB connected monitor",
                    (is_monitor) ? "Is" : "Not");

   if (!is_monitor && show_monitors_only) {
      is_monitor = force_hid_monitor_by_vid_pid( info.vendor, info.product);
      if (is_monitor)
         rpt_vstring(d1, "Device vid/pid matches exception list.  Forcing report for device.");
   }

   Parsed_Hid_Descriptor * phd = NULL;
   if (is_monitor || !show_monitors_only) {
      rpt_vstring(d1,"Tokenized report descriptor:");
      report_hid_report_item_list(report_item_list, d2);
   }

   if (is_monitor) {
      puts("");
      phd =  parse_hid_report_desc(rpt_desc.value, rpt_desc.size);
      Parsed_Hid_Report * edid_report = find_edid_report_descriptor(phd);
      if (edid_report) {
         rpt_title("Report descriptor for EDID:", d1);
         summarize_parsed_hid_report(edid_report, d2);
      }
      else
         rpt_title("No EDID report descriptor found!!!", d1);

      puts("");
      GPtrArray * feature_reports = get_vcp_code_reports(phd);
      if (feature_reports && feature_reports->len > 0) {
         rpt_title("Report descriptors for VCP features:", d1);
         summarize_vcp_code_report_array(feature_reports, d2);
      }
      else
         rpt_title("No VCP Feature report descriptors found!!!", d1);

      GPtrArray * reports = select_parsed_hid_report_descriptors(phd, HIDF_REPORT_TYPE_FEATURE);
      if (reports->len == 0) {
         puts("");
         rpt_title("No HID reports exist of type HIDF_REPORT_TYPE_FEATURE.", d1);
      }
      for (int ndx = 0; ndx < reports->len; ndx++) {
         Parsed_Hid_Report * a_report = g_ptr_array_index(reports, ndx);
         puts("");
         rpt_vstring(d1, "HID Feature report id: %3d  0x%02x", a_report->report_id, a_report->report_id);

         rpt_vstring(d1, "Parsed report description:");
         dbgrpt_parsed_hid_report(a_report, d2);

         /* Get Feature */
         buf[0] = a_report->report_id; /* Report Number */
         res = ioctl(fd, HIDIOCGFEATURE(1024), buf);
         if (res < 0) {
            perror("HIDIOCGFEATURE");
         } else {
            // printf("ioctl HIDIOCGFEATURE returned: %d\n", res);
            rpt_vstring(d1,"Report data:");
            rpt_vstring(d1, "Per hidraw.h: The first byte of SFEATURE and GFEATURE is the report number");
            rpt_hex_dump(buf, res, d2);
         }
      }
      free_parsed_hid_descriptor(phd);  // not defined, but need to free phd
   }

   free_hid_report_item_list(report_item_list);


bye:
   if (fd > 0)
      close(fd);
}


/* Indicates if a hidraw device represents a monitor.
 *
 * Arguments:
 *    devname      device name, e.g. /dev/hidraw3
 *
 * Returns:        true/false
 *
 */
bool hidraw_is_monitor_device(char * devname) {
   bool debug = false;
   if (debug)
      printf("(%s) 'Checking device %s\n", __func__, devname);

   bool is_monitor = false;

   int fd;
   int res, desc_size = 0;
   Byte buf[1024];     // TODO: maximum report size
   struct hidraw_report_descriptor rpt_desc;
   // struct hidraw_devinfo info;

   /* Open the Device with non-blocking reads. In real life,
      don't use a hard coded path; use libudev instead. */
   fd = open(devname, O_RDWR|O_NONBLOCK);
   if (fd < 0) {
      perror("Unable to open device");
      goto bye;
   }

   memset(&rpt_desc, 0x0, sizeof(rpt_desc));
   // memset(&info,     0x0, sizeof(info));
   memset(buf,       0x0, sizeof(buf));

   /* Get Report Descriptor Size */
   res = ioctl(fd, HIDIOCGRDESCSIZE, &desc_size);
   if (res < 0) {
      perror("HIDIOCGRDESCSIZE");
      goto bye;
   }

   /* Get Report Descriptor */
   rpt_desc.size = desc_size;
   res = ioctl(fd, HIDIOCGRDESC, &rpt_desc);
   if (res < 0) {
      perror("HIDIOCGRDESC");
      goto bye;
   }

   Hid_Report_Descriptor_Item * report_item_list =
         tokenize_hid_report_descriptor(rpt_desc.value, rpt_desc.size) ;
   // report_hid_report_item_list(report_item_list, 2);
   is_monitor = is_monitor_by_tokenized_hid_report_descriptor(report_item_list);
   free_hid_report_item_list(report_item_list);

bye:
   if (fd >= 0)    // really shouldn't be closing 0..2 stdin..stderr, but coverity complains
      close(fd);
   if (debug)
      printf("(%s) devname=%s, returning %s\n", __func__, devname, sbool(is_monitor));
   return is_monitor;
}


/* Probes hidraw devices.
 *
 * Arguments:
 *   show_monitors_only   show detailed information only for monitors
 *   depth                logical indentation depth
 *
 * Returns: nothing
 */
void probe_hidraw(bool show_monitors_only, int depth) {
   GPtrArray * hidraw_names = get_hidraw_device_names_using_filesys();
   rpt_vstring(depth, "Found %d USB HID devices.", hidraw_names->len);

   for (int ndx = 0; ndx < hidraw_names->len; ndx++) {
      char * devname = g_ptr_array_index(hidraw_names, ndx);
      // printf("(%s) Probing %s...\n", __func__, devname);
      probe_hidraw_device(devname, true,  depth);
   }
   g_ptr_array_free(hidraw_names, true);
}


