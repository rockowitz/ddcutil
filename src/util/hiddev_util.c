/* hiddev_util.c
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

#include <config.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <glib.h>
#ifdef USE_LIBUDEV
#include <libudev.h>
#endif
#include <linux/hiddev.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wchar.h>

#include "util/coredefs.h"
#include "util/string_util.h"
#include "util/glib_util.h"
#include "util/report_util.h"
#include "util/hiddev_reports.h"   // circular dependency, but only used in debug code
#include "util/edid.h"

#include "util/hiddev_util.h"


static const char* report_type_name_table[] = {
      "invalid value",
      "HID_REPORT_TYPE_INPUT",
      "HID_REPORT_TYPE_OUTPUT",
      "HID_REPORT_TYPE_FEATURE"
};


/* Returns a string representation of a report type id
 *
 * Arguments:  report_type
 *
 * Returns:  string representation of id
 */
const char * report_type_name(__u32 report_type) {
   if (report_type < HID_REPORT_TYPE_MIN || report_type > HID_REPORT_TYPE_MAX)
      report_type = 0;
   return report_type_name_table[report_type];
}



//
// *** Functions to identify hiddev devices representing monitors ***
//

/* Filter to find hiddevN files for scandir() */
static int is_hiddev(const struct dirent *ent) {
   return !strncmp(ent->d_name, "hiddev", strlen("hiddev"));
}


/*  Scans /dev to obtain list of hiddev device names
 *
 * Returns:   GPtrArray of device device names.
 *
 * Adapted from usbmonctl
 */
GPtrArray * get_hiddev_device_names_using_filesys() {
   const char *hiddev_paths[] = { "/dev/", "/dev/usb/", NULL };
   bool debug = false;
   GPtrArray * devnames =  g_ptr_array_new();
   char path[PATH_MAX];

   for (int i = 0; hiddev_paths[i] != NULL; i++) {
      struct dirent ** filelist;

      int count = scandir(hiddev_paths[i], &filelist, is_hiddev, alphasort);
      if (count < 0) {
         assert(count == -1);
         fprintf(stderr, "(%s) scandir() error: %s\n", __func__, strerror(errno));
         continue;
      }
      for (int j = 0; j < count; j++) {
         snprintf(path, PATH_MAX, "%s%s", hiddev_paths[i], filelist[j]->d_name);
         g_ptr_array_add(devnames, strdup(path));
         free(filelist[j]);
      }
      free(filelist);
   }

   if (debug) {
      printf("(%s) Found %d device names:", __func__, devnames->len);
      for (int ndx = 0; ndx < devnames->len; ndx++)
         printf("   %s\n", (char *) g_ptr_array_index(devnames, ndx) );
   }
   return devnames;
}


#ifdef USE_LIBUDEV
/* Comparison function used by g_ptr_array_sort() in function
 * find_hiddev_devices()
 */
gint g_ptr_scomp(gconstpointer a, gconstpointer b) {
   char ** ap = (char **) a;
   char ** bp = (char **) b;
   // printf("(%s) ap = %p -> -> %p -> |%s|\n", __func__, ap, *ap, *ap);
   // printf("(%s) bp = %p -> -> %p -> |%s|\n", __func__, bp, *bp, *bp);
   return g_ascii_strcasecmp(*ap,*bp);
}


/* Find hiddev device names using udev.
 *
 * Slightly more robust since doesn't make assumptions as to where
 * hiddev devices are found in the /dev tree.
 *
 * Arguments:     none
 * Returns:       array of hiddev path names in /dev
 */

GPtrArray *
get_hiddev_device_names_using_udev() {
   bool debug = false;
   if (debug) printf("(%s) Starting...\n", __func__);

   GPtrArray * dev_names = g_ptr_array_sized_new(10);
   // Null_Terminated_String_Array result = NULL;

   struct udev *udev;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *
   devices, *dev_list_entry;
   struct udev_device *dev;
   char * subsystem = "usbmisc";   // hiddev devices are in usbmisc subsystem

    /* Create the udev object */
    udev = udev_new();
    if (!udev) {
       printf("Can't create udev\n");
       goto bye;
       // return NULL;   // exit(1);
    }

    /* Create a list of the devices in the subsystem. */
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, subsystem);
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    /* udev_list_entry_foreach is a macro which expands to
       a loop. The loop will be executed for each member in
       devices, setting dev_list_entry to a list entry
       which contains the device's path in /sys. */
    udev_list_entry_foreach(dev_list_entry, devices) {
       const char *path;

       /* Get the filename of the /sys entry for the device
          and create a udev_device object (dev) representing it */
       path = udev_list_entry_get_name(dev_list_entry);
       // printf("path: %s\n", path);
       dev = udev_device_new_from_syspath(udev, path);
       const char * sysname = udev_device_get_sysname(dev);
       if (str_starts_with(sysname, "hiddev")) {
          g_ptr_array_add(dev_names, strdup(udev_device_get_devnode(dev)));
       }
       udev_device_unref(dev);
    }

    g_ptr_array_sort(dev_names, g_ptr_scomp);
    // result = g_ptr_array_to_ntsa(dev_names);


#ifdef OLD
   if (debug) {
      printf("(%s) Returning:\n", __func__);
      int ndx=0;
      // char * curname = result[0];
      while (result[ndx]) {
         printf("   %p -> %s\n", result[ndx], result[ndx]);
         ndx++;
      }
   }
#endif

   /* Free the enumerator object */
    udev_enumerate_unref(enumerate);

    udev_unref(udev);

bye:
   // g_ptr_array_free(dev_names, false);   // or true?
   // return result;
   return dev_names;
}
#endif

GPtrArray * get_hiddev_device_names() {

#ifdef USE_LIBUDEV
   return get_hiddev_device_names_using_udev();
#else
   return get_hiddev_device_names_using_filesys();
#endif
}


struct vid_pid {
   __s16   vid;
   __s16   pid;
};


/* Check for specific USB devices that should be treated as
 * monitors, even though the normal monitor check fails.
 *
 * This is a hack.
 *
 * Arguments:
 *   fd       file descriptor
 *
 * Returns    true/false
 */
bool force_hiddev_monitor(int fd) {
   bool debug = false;
   bool result = false;

   struct hiddev_devinfo dev_info;

   int rc = ioctl(fd, HIDIOCGDEVINFO, &dev_info);
   if (rc != 0) {
      REPORT_IOCTL_ERROR("HIDIOCGDEVINFO", rc);
      goto bye;
   }

   struct vid_pid exceptions[] = {
         {0x0424, 0x3328},    // Std Micrososystems USB HID I2C - HP LP2480
         {0x056d, 0x0002},    // Eizo,      HID Monitor Controls

         // NEC monitors
         {0x0409, 0x040d},    // P232W
         {0x0409, 0x02b7},    // P241W
         {0x0409, 0x042c},    // P242W
         {0x0409, 0x02bb},    // PA231W
         {0x0409, 0x02b8},    // PA241W   (seen at RIT)
         {0x0409, 0x042d},    // PA242W
         {0x0409, 0x02b9},    // PA271W
         {0x0409, 0x042e},    // PA272W
         {0x0409, 0x02ba},    // PA301W
         {0x0409, 0x042f},    // PA302W
         {0x0409, 0x02bc},    // MD301C4
         {0x0409, 0x040a},    // MD211G3
         {0x0409, 0x040b},    // MD211C3
         {0x0409, 0x040c},    // MD211C2
         {0x0409, 0x042b},    // MD242C2
         {0x0409, 0x044f},    // EA244UHD
         {0x0409, 0x042b},    // EA304WMi
         {0x0409, 0x046b},    // PA322UHD
         {0x0409, 0x047d},    // X841UHD
         {0x0409, 0x04ac},    // X981UHD
         {0x0409, 0x04ad},    // X651UHD
         {0x0409, 0x046c},    // MD322C8
         {0x0409, 0x04Ae},    // P212
         {0x0409, 0x050c},    // PA322UHD2

         // additional values from usb.ids
         {0x0419, 0x8002},    // Samsung,   Syncmaster HID Monitor Control
         {0x0452, 0x0021},    // Misubishi, HID Monitor Controls
         {0x04a6, 0x0181},    // Nokia,     HID Monitor Controls
         {0x04ca, 0x1766},    // Lite-on,   HID Monitor Controls
   };
   const int vid_pid_ct = sizeof(exceptions)/sizeof(struct vid_pid);

   for (int ndx = 0; ndx < vid_pid_ct && !result; ndx++) {
      if (dev_info.vendor == exceptions[ndx].vid) {
         if (exceptions[ndx].pid == 0 || dev_info.product == exceptions[ndx].pid) {
            result = true;
            if (debug)
               printf("(%s) Matched exception vid=0x%04x, pid=0x%04x\n", __func__,
                      exceptions[ndx].vid, exceptions[ndx].pid);
         }
      }
   }

bye:
   if (debug)
      printf("(%s) vid=0x%04x, pid=0x%04x, returning: %s\n", __func__,
             dev_info.vendor, dev_info.product, bool_repr(result));
   return result;
}


/* Check if an open hiddev device represents a USB compliant monitor.
 *
 * Arguments:
 *    fd       file descriptor
 *
 * Returns:    true/false
 *
 * Per USB Monitor Control Class Specification section 5.5,
 * "to identify a HID class device as a monitor, the devices's
 * HID Report Descriptor must contain a top-level collection with
 * a usage of Monitor Control from the USB Monitor Usage Page."
*/
bool is_hiddev_monitor(int fd) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting\n", __func__);
   int monitor_collection_index = -1;

   int cndx = 0;   // indexes start at 0
   int ioctl_rc = 0;
   for (cndx=0; ioctl_rc != -1; cndx++) {
      struct hiddev_collection_info  cinfo;
      memset(&cinfo, 0, sizeof(cinfo));
      errno = 0;
      cinfo.index = cndx;
      ioctl_rc = ioctl(fd, HIDIOCGCOLLECTIONINFO, &cinfo);
      if (ioctl_rc == -1)
         continue;
      assert(ioctl_rc == 0);
      if (debug)
         printf("(%s) cndx=%d, cinfo.level=%d, cinfo.usage=0x%08x\n", __func__,
                cndx, cinfo.level, cinfo.usage);
      if (cinfo.level == 0 && cinfo.usage == 0x00800001) { // USB Monitor Usage Page/Monitor Control
         monitor_collection_index = cndx;
         break;
      }
   }

   bool result = (monitor_collection_index >= 0);

   // FOR TESTING
   // if (!result)
   //    result = force_hiddev_monitor(fd);

   if (debug)
      printf("(%s) Returning: %s\n", __func__, bool_repr(result));
   return result;
}


/* Checks that all usages of a field have the same usage code.
 *
 * Arguments:
 *   fd            file descriptor of open hiddev device
 *   finfo         pointer to hiddev_field_info struct describing the field
 *   field_index   actual field index, value in finfo may have been changed by
 *                 HIDIOCGFIELDINFO call, that filled in hiddev_field_info
 *
 * Returns:        usage code if all are identical, 0 if not
 */
__u32 get_identical_ucode(int fd, struct hiddev_field_info * finfo, __u32 field_index) {
   // assert(finfo->flags & HID_FIELD_BUFFERED_BYTE);
   __u32 result = 0;

   for (int undx = 0; undx < finfo->maxusage; undx++) {
      struct hiddev_usage_ref uref = {
          .report_type = finfo->report_type,
          .report_id   = finfo->report_id,
          .field_index = field_index,         // actual field index, not value changed by HIDIOCGFIELDINFO
          .usage_index = undx
      };
      // printf("(%s) report_type=%d, report_id=%d, field_index=%d, usage_index=%d\n",
      //       __func__, rinfo->report_type, rinfo->report_id, field_index=saved_field_index, undx);
      int rc = ioctl(fd, HIDIOCGUCODE, &uref);    // Fills in usage code
      if (rc != 0) {
          REPORT_IOCTL_ERROR("HIDIOCGUCODE", rc);
          result = 0;
          break;
      }
      // printf("(%s) uref.field_index=%d, uref.usage_code = 0x%08x\n",
      //        __func__, uref.field_index, uref.usage_code);
      if (undx == 0)
         result = uref.usage_code;
      else if (uref.usage_code != result) {
         result = 0;
         break;
      }
   }   // loop over usages

   return result;
}


/* Collects all the usage values for a field and returns them in a Buffer.
 *
 * The field must meet the following requirements:
 *   All usages must have the same usage code
 *   All values must be single byte
 *
 * The function should only be called for INPUT and FEATURE reports.  (assertion check)
 *
 * This function assumes that HIDIOCGREPORT has already been called
 *
 * Arguments:
 *   fd           file descriptor
 *   finfo        pointer to filled in hiddev_field_info for the field
 *   field_index  actual field index to use
 *
 * Returns:   Buffer with accumulated value,
 *            NULL if multiple usage codes or some usage value is > 0xff
 */

Buffer * collect_single_byte_usage_values(
            int                         fd,
            struct hiddev_field_info *  finfo,
            __u32                       field_index)
{
   bool debug = false;
   Buffer * result = buffer_new(finfo->maxusage, __func__);
   bool ok = true;
   __s32 common_usage_code;

   // n.b. assumes HIDIOCGREPORT has been called

   assert(finfo->report_type != HID_REPORT_TYPE_OUTPUT);

   for (int undx = 0; undx < finfo->maxusage; undx++) {
       struct hiddev_usage_ref uref = {
           .report_type = finfo->report_type,   // rinfo.report_type;
           .report_id =   finfo->report_id,     // rinfo.report_id;
           .field_index = field_index,   // use original value, not value changed by HIDIOCGFIELDINFO
           .usage_index = undx
       };
       // printf("(%s) report_type=%d, report_id=%d, field_index=%d, usage_index=%d\n",
       //       __func__, rinfo->report_type, rinfo->report_id, field_index=saved_field_index, undx);
       int rc = ioctl(fd, HIDIOCGUCODE, &uref);    // Fills in usage code
       if (rc != 0) {
           REPORT_IOCTL_ERROR("HIDIOCGUCODE", rc);
           ok = false;
           break;
       }
       // printf("(%s) uref.field_index=%d, uref.usage_code = 0x%08x\n",
       //        __func__, uref.field_index, uref.usage_code);
       if (undx == 0) {
          common_usage_code = uref.usage_code;
       }
       else if (uref.usage_code != common_usage_code) {
          ok = false;
          if (debug)
             printf("(%s) Multiple usage codes", __func__);
          break;
       }

       rc = ioctl(fd, HIDIOCGUSAGE, &uref);  // Fills in usage value
       if (rc != 0) {
          REPORT_IOCTL_ERROR("HIDIOCGUSAGE", rc);
          ok = false;
          break;
       }
       if (uref.value &0xffffff00) {     // if high order bytes non-zero
          if (true)
             printf("(%s) High order bytes of value for usage %d are non-zero\n", __func__, undx);
          ok = false;
          break;
       }
       Byte b = uref.value;
       buffer_add(result, b);
       if (false)
          printf("(%s) usage = %d, value=0x%08x, byte = 0x%02x\n",
                 __func__, undx, uref.value, uref.value&0xff);
    }   // loop over usages


   if (!ok && result) {
      buffer_free(result, __func__);
      result = NULL;
   }

   if (debug) {
      printf("(%s) Returning: %p\n", __func__, result);
      // if (result)
      //    buffer_dump(result);
   }
   return result;
}


//
// *** Functions for EDID retrieval ***
//

/* Checks if a field in a HID report represents an EDID
 *
 * Arguments:
 *    fd           file descriptor
 *    rinfo        pointer to hiddev_report_info struct
 *    field_index  index number of field to check
 *
 * Returns:        true if the field represents an EDID, false if not
 *
 * The field must have at least 128 usages, and the usage code for each must
 * be USB Monitor/EDID information
 */
bool is_field_edid(int fd, struct hiddev_report_info * rinfo, int field_index) {
   bool debug = false;
   if (debug)
      printf("(%s) report_type=%d, report_id=%d, field index = %d\n",
             __func__, rinfo->report_type, rinfo->report_id, field_index);

   // struct hiddev_field_info *  result = NULL;
   bool all_usages_edid = false;
   int rc;

   struct hiddev_field_info finfo = {
      .report_type = rinfo->report_type,
      .report_id   = rinfo->report_id,
      .field_index = field_index
   };

   int saved_field_index = field_index;
   rc = ioctl(fd, HIDIOCGFIELDINFO, &finfo);
   if (rc != 0)
      REPORT_IOCTL_ERROR("HIDIOCGFIELDINFO", rc);
   assert(rc == 0);
   if (debug) {
      if (finfo.field_index != saved_field_index && debug) {
         printf("(%s) !!! ioctl(HIDIOCGFIELDINFO) changed field_index from %d to %d\n",
                __func__, saved_field_index, finfo.field_index);
         printf("(%s)   rinfo.num_fields=%d, finfo.maxusage=%d\n",
                __func__, rinfo->num_fields, finfo.maxusage);
      }
   }
   // result->field_id = fndx;

   if (finfo.maxusage < 128)
      goto bye;

   all_usages_edid = ( get_identical_ucode(fd, &finfo, field_index) == 0x00800002 );
#ifdef OLD
   bool all_usages_edid = true;
   int undx;
   for (undx = 0; undx < finfo.maxusage && all_usages_edid; undx++) {
      struct hiddev_usage_ref uref = {
          .report_type = rinfo->report_type,   // rinfo.report_type;
          .report_id =   rinfo->report_id,     // rinfo.report_id;
          .field_index = saved_field_index,    // use original value, not value changed by HIDIOCGFIELDINFO
          .usage_index = undx
      };
      // printf("(%s) report_type=%d, report_id=%d, field_index=%d, usage_index=%d\n",
      //       __func__, rinfo->report_type, rinfo->report_id, field_index=saved_field_index, undx);
      rc = ioctl(fd, HIDIOCGUCODE, &uref);    // Fills in usage code
      if (rc != 0) {
          REPORT_IOCTL_ERROR("HIDIOCGUCODE", rc);
          all_usages_edid = false;
      }
      else {
         // printf("(%s) uref.field_index=%d, uref.usage_code = 0x%08x\n",
         //        __func__, uref.field_index, uref.usage_code);
         if (uref.usage_code != 0x00800002)   // USB Monitor/EDID Information
            all_usages_edid = false;
      }
   }   // loop over usages
#endif

   // if (all_usages_edid) {
   //    result = malloc(sizeof(struct hiddev_field_info));
   //    memcpy(result, &finfo, sizeof(struct hiddev_field_info));
   // }

bye:
#ifdef OLD
   if (debug) {
      if (result) {
         printf("(%s) Returning: \n", __func__);
         report_hiddev_field_info(result, 1);
      }
      else
         printf("(%s) Returning: null\n", __func__);

   }
   return result;
#endif
   return all_usages_edid;
}



#define EDID_SIZE 128


//
// hid_field_locator functions
//

void free_hid_field_locator(struct hid_field_locator * location) {
   if (location) {
      // if (location->rinfo)
      //    free(location->rinfo);
      if (location->finfo)
         free(location->finfo);
      free(location);
   }
}


void report_hid_field_locator(struct hid_field_locator * ploc, int depth) {
   int d1 = depth+1;

   rpt_structure_loc("struct hid_field_locator", ploc, depth);
   if (ploc) {
      rpt_vstring(d1, "%-20s %u", "report_type:",  ploc->report_type );
      rpt_vstring(d1, "%-20s %u", "report_id:",  ploc->report_id );
      rpt_vstring(d1, "%-20s %u", "field_index:",  ploc->field_index);
      // report_hiddev_report_info(ploc->rinfo, d1);
      report_hiddev_field_info(ploc->finfo, d1);
   }
}


/* Test if all, or at least 1, usage codes of a field match a specified usage code.
 *
 * Arguments:
 *   fd                 file descriptor of open hiddev device
 *   report_type        HID_REPORT_TYPE_INPUT, HID_REPORT_TYPE_OUTPUT, or HID_REPORT_TYPE_FEATURE
 *   report_id          report number
 *   field_index        field index
 *   ucode              usage code to test against
 *   require_all_match  if true, all usages must be the specified value
 *
 * Returns:    field information if true, NULL if false
 */
struct hiddev_field_info *
test_field_ucode(
   int    fd,
   __u32  report_type,
   __u32  report_id,
   __u32  field_index,
   __u32  ucode,
   bool   require_all_match)
{
   bool debug = true;
   if (debug)
      printf("(%s) report_type=%d, report_id=%d, field index=%d, ucode=0x%08x, require_all_match=%s\n",
             __func__, report_type, report_id, field_index, ucode, bool_repr(require_all_match));

   struct hiddev_field_info * result = NULL;
   int rc;

   struct hiddev_field_info finfo = {
        .report_type = report_type,
        .report_id   = report_id,
        .field_index = field_index
   };

   int saved_field_index = field_index;
   rc = ioctl(fd, HIDIOCGFIELDINFO, &finfo);
   if (rc != 0) {
      REPORT_IOCTL_ERROR("HIDIOCGFIELDINFO", rc);
      goto bye;
   }
   if (debug) {
      if (finfo.field_index != saved_field_index && debug)
         printf("(%s) !!! ioctl(HIDIOCGFIELDINFO) changed field_index from %d to %d\n",
                __func__, saved_field_index, finfo.field_index);
   }
   // result->field_id = fndx;

   bool is_matched = false;
   if (require_all_match) {
      __u32 ucode_found = get_identical_ucode(fd, &finfo, field_index);

      if (ucode_found == ucode)
         is_matched = true;
   }
   else {
      // loop over all usage codes
      int undx;
      for (undx = 0; undx < finfo.maxusage; undx++) {
         struct hiddev_usage_ref uref = {
            .report_type = report_type,
            .report_id =   report_id,
            .field_index = field_index,
            .usage_index = undx
         };
         rc = ioctl(fd, HIDIOCGUCODE, &uref);    // Fills in usage code
         if (rc != 0) {
            REPORT_IOCTL_ERROR("HIDIOCGUCODE", rc);
            break;
         }
         if (uref.usage_code == ucode) {
            is_matched = true;
            break;
         }
      }   // loop over usages
   }
   if (is_matched) {
      result = malloc(sizeof(struct hiddev_field_info));
      memcpy(result, &finfo, sizeof(struct hiddev_field_info));
   }

bye:
   if (debug) {
      printf("(%s) Returning: %p\n", __func__, result);
      if (result)
         report_hiddev_field_info(result, 1);
   }

   return result;
}


/* Look through all reports of a given type (HID_REPORT_TYPE_FEATURE, etc) to
 * find one having a field with a given usage code.
 *
 * Arguments:
 *   fd                file descriptor of open hiddev device
 *   report_type       HID_REPORT_TYPE_INPUT, HID_REPORT_TYPE_OUTPUT, or HID_REPORT_TYPE_FEATURE
 *   ucode             usage code
 *   match_all_ucodes  if true, all usages must match ucode
 *                     if false, at least one usage must match ucode
 *
 * Returns:            record identifying the report and field
 */
struct hid_field_locator*
find_report(int fd, __u32 report_type, __u32 ucode, bool match_all_ucodes) {
   bool debug = true;

   struct hid_field_locator * result = NULL;

   struct hiddev_report_info rinfo = {
      .report_type = report_type,
      .report_id   = HID_REPORT_ID_FIRST
   };

   int report_id_found   = -1;
   int field_index_found = -1;
   struct hiddev_field_info * finfo_found = NULL;
   int reportinfo_rc = 0;
   while (reportinfo_rc >= 0 && report_id_found == -1) {
       // printf("(%s) Report counter %d, report_id = 0x%08x %s\n",
       //       __func__, rptct, rinfo.report_id, interpret_report_id(rinfo.report_id));

      errno = 0;
      reportinfo_rc = ioctl(fd, HIDIOCGREPORTINFO, &rinfo);
      if (reportinfo_rc != 0) {    // no more reports
         if (reportinfo_rc != -1)
            REPORT_IOCTL_ERROR("HIDIOCGREPORTINFO", reportinfo_rc);
         break;
      }

      for (int fndx = 0; fndx < rinfo.num_fields && field_index_found == -1; fndx++) {
         // finfo_found = is_field_edid(fd, &rinfo, fndx);
         // *** TEMP *** FORCE FAILURE
         finfo_found = test_field_ucode(fd, report_type, rinfo.report_id, fndx, ucode, match_all_ucodes);
         if (finfo_found) {
            report_id_found    = rinfo.report_id;
            field_index_found  = fndx;
         }
      }

       rinfo.report_id |= HID_REPORT_ID_NEXT;
     }  // loop over reports

     if (report_id_found >= 0) {
        result = calloc(1, sizeof(struct hid_field_locator));
        // result->rinfo = calloc(1, sizeof(struct hiddev_report_info));
        // memcpy(result->rinfo, &rinfo, sizeof(struct hiddev_report_info));
        result->finfo = finfo_found;   // returned by is_field_edid()
        result->report_type = rinfo.report_type;
        result->report_id   = report_id_found;
        result->field_index = field_index_found;    // finfo.field_index may have been changed by HIDIOGREPORTINFO
     }

     if (debug) {
        if (result) {
           printf("(%s) Returning report_id=%d, field_index=%d\n",
                  __func__, result->report_id, result->field_index);
        }
        else
           printf("(%s) Returning NULL", __func__);
     }

     if (debug) {
        printf("(%s) Returning: %p\n", __func__, result);
        if (result)
           report_hid_field_locator(result, 1);
     }
    return result;
}


/* Finds the report describing the EDID.
 *
 * Arguments:
 *   fd          file descriptor of open hiddev device
 *
 * Returns:      pointer to newly allocated struct hid_field_locator representing
 *               the feature report and field within that report that returns
 *               the EDID,
 *               NULL if not found
 *
 * It is the responsibility of the caller to free the returned struct.
 */
struct hid_field_locator *
locate_edid_report(int fd) {
   bool debug = true;

   struct hid_field_locator* result = NULL;
   result = find_report(fd, HID_REPORT_TYPE_FEATURE, 0x00800002, /*match_all_ucodes=*/true);

#ifdef OLD
   struct hiddev_report_info rinfo = {
      .report_type = HID_REPORT_TYPE_FEATURE,
      .report_id   = HID_REPORT_ID_FIRST
   };

   int report_id_found   = -1;
   int field_index_found = -1;
   struct hiddev_field_info * finfo_found = NULL;
   int reportinfo_rc = 0;
   while (reportinfo_rc >= 0 && report_id_found == -1) {
      // printf("(%s) Report counter %d, report_id = 0x%08x %s\n",
      //       __func__, rptct, rinfo.report_id, interpret_report_id(rinfo.report_id));

      errno = 0;
      reportinfo_rc = ioctl(fd, HIDIOCGREPORTINFO, &rinfo);
      if (reportinfo_rc != 0) {    // no more reports
         if (reportinfo_rc != -1)
            REPORT_IOCTL_ERROR("HIDIOCGREPORTINFO", reportinfo_rc);
         break;
      }

      for (int fndx = 0; fndx < rinfo.num_fields && field_index_found == -1; fndx++) {
         finfo_found = is_field_edid(fd, &rinfo, fndx);
         if (finfo_found) {
            report_id_found    = rinfo.report_id;
            field_index_found  = fndx;
         }
      }

      rinfo.report_id |= HID_REPORT_ID_NEXT;
    }  // loop over reports

    struct hid_field_locator * result = NULL;
    if (report_id_found >= 0) {
       result = calloc(1, sizeof(struct hid_field_locator));
       // result->rinfo = calloc(1, sizeof(struct hiddev_report_info));
       // memcpy(result->rinfo, &rinfo, sizeof(struct hiddev_report_info));
       result->finfo = finfo_found;   // returned by is_field_edid()
       result->report_type = rinfo.report_type;
       result->report_id   = report_id_found;
       result->field_index = field_index_found;    // finfo.field_index may have been changed by HIDIOGREPORTINFO
    }
#endif

    if (debug) {
       if (result) {
          printf("(%s) Returning report_id=%d, field_index=%d\n",
                 __func__, result->report_id, result->field_index);
       }
       else
          printf("(%s) Returning NULL", __func__);
    }

    if (debug) {
       printf("(%s) Returning: %p\n", __func__, result);
       if (result)
          report_hid_field_locator(result, 1);
    }
   return result;
}


/* Retrieve first 128 bytes of EDID, given that the report and field
 * locating the EDID are known.
 *
 * Arguments:
 *    fd     file descriptor of open hiddev device
 *    loc    pointer to hid_field_locator struct
 *
 * Returns:
 *    pointer to Buffer struct containing the EDID
 *
 * It is the responsibility of the caller to free the returned buffer.
 */
Buffer * get_hiddev_edid_by_location(int fd, struct hid_field_locator * loc) {
   assert(loc);
   bool debug = true;
   if (debug) {
      printf("(%s) Starting.  loc=%p, loc->report_id=%d, loc->field_index=%d\n",
             __func__, loc, loc->report_id, loc->field_index);
      // report_hiddev_report_info(loc->rinfo, 1);
      // report_hiddev_field_info(loc->finfo, 1);
      report_hid_field_locator(loc, 1);
   }

   int rc;
   Buffer * result = NULL;
   // Byte edidbuf[128];

   struct hiddev_report_info rinfo;
   // rinfo.report_type = loc->rinfo->report_type;
   rinfo.report_type = loc->report_type;
   rinfo.report_id   = loc->report_id;
   // rinfo.num_fields  = loc->rinfo->num_fields;
   rinfo.num_fields  = 1;

   // rc = ioctl(fd, HIDIOCGREPORT, loc->rinfo);
   rc = ioctl(fd, HIDIOCGREPORT, &rinfo);
   if (rc != 0) {
      REPORT_IOCTL_ERROR("HIDIOCGREPORT", rc);
      // printf("(%s) Unable to get report %d\n", __func__, loc->rinfo->report_id);
      printf("(%s) Unable to get report %d\n", __func__, loc->report_id);
      goto bye;
   }

   // To do: replace with HIDIOCGUSAGES

   assert(loc->finfo->maxusage >= 128);

#ifdef OLD
   int undx;
   for (undx = 0; undx < 128; undx++) {
      struct hiddev_usage_ref uref = {
        //   .report_type = loc->rinfo->report_type,  // rinfo.report_type;
          .report_type = loc->report_type,
          .report_id   = loc->report_id,           // rinfo.report_id may have flag bits set
        //  .report_id   = loc->finfo->report_id,    // *** loc->report)id also has flag bits set
          .field_index = loc->field_index,         // use original value, not value changed from HIDIOCGFIELDINFO
          .usage_index = undx
      };

      rc = ioctl(fd, HIDIOCGUSAGE, &uref);  // Fills in usage value
      if (rc != 0) {
         REPORT_IOCTL_ERROR("HIDIOCGUSAGE", rc);
         break;
      }
      edidbuf[undx] = uref.value & 0xff;
      if (debug)
         printf("(%s) usage = %d, value=0x%08x, byte = 0x%02x\n", __func__, undx, uref.value, uref.value&0xff);
   }   // loop over usages

   if (undx == 128) {   // if got them all
      result = buffer_new_with_value(edidbuf, 128, __func__);
   }
#endif


   struct hiddev_usage_ref_multi uref_multi;
   memset(&uref_multi, 0, sizeof(uref_multi));  // initialize all fields to make valgrind happy
   uref_multi.uref.report_type = loc->report_type;
   uref_multi.uref.report_id   = loc->report_id;
   uref_multi.uref.field_index = loc->field_index;
   uref_multi.uref.usage_index = 0;
   uref_multi.num_values = 128; // needed? yes!

   rc = ioctl(fd, HIDIOCGUSAGES, &uref_multi);  // Fills in usage value
   if (rc != 0) {
      REPORT_IOCTL_ERROR("HIDIOCGUSAGES", rc);
      goto bye;
   }
   Byte edidbuf2[128];
   for (int ndx=0; ndx<128; ndx++)
      edidbuf2[ndx] = uref_multi.values[ndx] & 0xff;
   result = buffer_new_with_value(edidbuf2, 128, __func__);

bye:
   if (debug) {
      printf("(%s) Returning: %p\n", __func__, result);
      if (result) {
         buffer_dump(result);
      }
   }
   return result;
}




Buffer * get_multibyte_report_value(int fd, struct hid_field_locator * loc) {
   bool debug = true;
   Buffer * result = NULL;

   struct hiddev_report_info rinfo;
   rinfo.report_type = loc->report_type;
   rinfo.report_id   = loc->report_id;

   // int rc = ioctl(fd, HIDIOCGREPORT, loc->rinfo);
   int rc = ioctl(fd, HIDIOCGREPORT, &rinfo);
   if (rc != 0) {
      REPORT_IOCTL_ERROR("HIDIOCGREPORT", rc);
      goto bye;
   }

   int maxusage = loc->finfo->maxusage;

   struct hiddev_usage_ref_multi uref_multi;
   uref_multi.uref.report_type = loc->report_type;
   uref_multi.uref.report_id   = loc->report_id;
   uref_multi.uref.field_index = loc->field_index;
   uref_multi.uref.usage_index = 0;
   uref_multi.num_values = maxusage;

   rc = ioctl(fd, HIDIOCGUSAGES, &uref_multi);  // Fills in usage value
   if (rc != 0) {
      REPORT_IOCTL_ERROR("HIDIOCGUSAGES", rc);
      goto bye;
   }

   Byte workbuf[HID_MAX_MULTI_USAGES];
   for (int ndx=0; ndx<maxusage; ndx++)
       workbuf[ndx] = uref_multi.values[ndx] & 0xff;
   if (debug) {
      printf("(%s) Value retrieved by HIDIOCGUSAGES:\n", __func__);
      hex_dump(workbuf, maxusage);
   }
   result = buffer_new_with_value(workbuf, maxusage, __func__);

bye:
   if (debug) {
      printf("(%s) Returning: %p\n", __func__, result);
      if (result) {
         buffer_dump(result);
      }
   }

   return result;
}


/* Retrieves the EDID (128 bytes) from a hiddev device representing a HID
 * compliant monitor.
 *
 * Arguments:
 *    fd     file descriptor
 *
 * Returns:
 *    pointer to Buffer struct containing the EDID
 *
 * It is the responsibility of the caller to free the returned buffer.
 */
Buffer * get_hiddev_edid(int fd)  {
   bool debug = true;
   if (debug)
      printf("(%s) Starting\n", __func__);
   Buffer * result = NULL;
   struct hid_field_locator * loc = locate_edid_report(fd);
   if (loc) {
      result = get_hiddev_edid_by_location(fd, loc);
   }

#ifdef MOVE_TO_CALLER
   if (result) {
       Parsed_Edid * parsed_edid0 = create_parsed_edid(result->bytes);
       if (!parsed_edid0) {
          result = NULL;
          DBGMSF(debug, "create_parsed_edid() returned invalid EDID");
       }
    }
#endif

   if (debug)
      printf("(%s) Returning: %p\n", __func__, result);
   return result;
}



//
// *** Miscellaneous functions ***
//

/* Returns the name of a hiddev device, as reported by ioctl HIDIOCGNAME.
 *
 * Arguments:
 *    fd         file descriptor of open hiddev device
 *
 * Returns:      pointer to newly allocated string,
 *               NULL if ioctl call fails (should never happen)
 */
char * get_hiddev_name(int fd) {
   // printf("(%s) Starting. fd=%d\n", __func__, fd);
   const int blen = 256;
   char buf1[blen];
   for (int ndx=0; ndx < blen; ndx++)
   buf1[ndx] = '\0';   // initialize to avoid valgrind warning
   // returns length of result, including terminating null
   int rc = ioctl(fd, HIDIOCGNAME(blen), buf1);
   // printf("(%s) HIDIOCGNAME returned %d\n", __func__, rc);
   // hex_dump(buf1,64);
   char * result = NULL;
   if (rc >= 0)
      result = strdup(buf1);
   // printf("(%s) Returning |%s|\n", __func__, result);
   return result;
}
