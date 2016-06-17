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

#include "util/hiddev_util.h"


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
   bool debug = true;
   int monitor_collection_index = -1;

   int cndx = 0;   // indexes start at 0
   int ioctl_rc = 0;
   for (cndx=0; ioctl_rc != -1; cndx++) {
      struct hiddev_collection_info  cinfo;
      memset(&cinfo, 0, sizeof(cinfo));
      errno = 0;
      cinfo.index = cndx;
      // printf("(%s) Calling HIDIOCGCOLLECTIONINFO, cndx=%d\n", __func__, cndx);
      ioctl_rc = ioctl(fd, HIDIOCGCOLLECTIONINFO, &cinfo);
      // if (rc != 0) {
      //    REPORT_IOCTL_ERROR("HIDIOCGCOLLECTIONINFO", rc);
      //    continue;
      // }
      if (ioctl_rc == -1)
         continue;
      if (debug)
         printf("(%s) cndx=%d, cinfo.level=%d, cinfo.usage=0x%08x\n", __func__,
                cndx, cinfo.level, cinfo.usage);
      if (cinfo.level == 0 && cinfo.usage == 0x00800001) { // USB Monitor Usage Page/Monitor Control
         monitor_collection_index = cndx;
         break;
      }
   }

   bool result = (monitor_collection_index >= 0);
   if (debug)
      printf("(%s) Returning: %s\n", __func__, bool_repr(result));
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
 * Returns:        pointer to hiddev_field_info struct for field if true,
 *                 NULL if field does not represent an EDID
 *
 * The field must have at least 128 usages, and the usage code for each must
 * be USB Monitor/EDID information
 */
struct hiddev_field_info *
is_field_edid(int fd, struct hiddev_report_info * rinfo, int field_index) {
   bool debug = true;
   if (debug)
      printf("(%s) report_type=%d, report_id=%d, field index = %d\n",
             __func__, rinfo->report_type, rinfo->report_id, field_index);

   struct hiddev_field_info *  result = NULL;
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
   if (all_usages_edid) {
      result = malloc(sizeof(struct hiddev_field_info));
      memcpy(result, &finfo, sizeof(struct hiddev_field_info));
   }

bye:
   if (debug) {
      if (result) {
         printf("(%s) Returning: \n", __func__);
         report_hiddev_field_info(result, 1);
      }
      else
         printf("(%s) Returning: null\n", __func__);

   }
   return result;
}



#define EDID_SIZE 128


// Describes report and field within the report that represent the EDID

struct edid_location {
   struct hiddev_report_info * rinfo;         // simplify by eliminating?
   struct hiddev_field_info  * finfo;         // simplify by eliminating?
   __u32                       report_type;
   __u32                       report_id;
   __u32                       field_index;
};


void report_edid_location(struct edid_location * ploc, int depth) {
   int d1 = depth+1;

   rpt_structure_loc("struct edid_location", ploc, depth);
   if (ploc) {
      rpt_vstring(d1, "%-20s %u", "report_type:",  ploc->report_type );
      rpt_vstring(d1, "%-20s %u", "report_id:",  ploc->report_id );
      rpt_vstring(d1, "%-20s %u", "field_index:",  ploc->field_index);
      report_hiddev_report_info(ploc->rinfo, d1);
      report_hiddev_field_info(ploc->finfo, d1);

   }
}


void free_edid_location(struct edid_location * location) {
   if (location) {
      if (location->rinfo)
         free(location->rinfo);
      if (location->finfo)
         free(location->finfo);
      free(location);
   }
}


/* Finds the report describing the EDID.
 *
 * Arguments:
 *   fd          file handle of open hiddev device
 *
 * Returns:      pointer to newly allocated struct edid_location representing
 *               the feature report and field within that report which returns
 *               the EDID,
 *               NULL if not found
 *
 * It is the responsibility of the caller to free the returned struct.
 */
struct edid_location *
locate_edid_report(int fd) {
   bool debug = true;

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

    struct edid_location * result = NULL;
    if (report_id_found >= 0) {
       result = calloc(1, sizeof(struct edid_location));
       result->rinfo = calloc(1, sizeof(struct hiddev_report_info));
       memcpy(result->rinfo, &rinfo, sizeof(struct hiddev_report_info));
       result->finfo = finfo_found;   // returned by is_field_edid()
       result->report_type = rinfo.report_type;
       result->report_id   = report_id_found;
       result->field_index = field_index_found;    // finfo.field_index may habe been changed by HIDIOGREPORTINFO
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
          report_edid_location(result, 1);
    }
   return result;
}


/* Retrieve first 128 bytes of EDID, given that the report and field
 * locating the EDID are known.
 *
 * Arguments:
 *    fd     file descriptor
 *    loc    pointer to edid_location struct
 *
 * Returns:
 *    pointer to Buffer struct containing the EDID
 *
 * It is the responsibility of the caller to free the returned buffer.
 */
Buffer * get_hiddev_edid_by_location(int fd, struct edid_location * loc) {
   assert(loc);
   bool debug = true;
   if (debug) {
      printf("(%s) Starting.  loc=%p, loc->report_id=%d, loc->field_index=%d\n",
             __func__, loc, loc->report_id, loc->field_index);
      // report_hiddev_report_info(loc->rinfo, 1);
      // report_hiddev_field_info(loc->finfo, 1);
      report_edid_location(loc, 1);
   }

   int rc;
   Buffer * result = NULL;
   Byte edidbuf[128];

   rc = ioctl(fd, HIDIOCGREPORT, loc->rinfo);
   if (rc != 0) {
      REPORT_IOCTL_ERROR("HIDIOCGREPORT", rc);
      printf("(%s) Unable to get report %d\n", __func__, loc->rinfo->report_id);
      goto bye;
   }

   // To do: replace with HIDIOCGUSAGES

   assert(loc->finfo->maxusage >= 128);

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
      // if (debug)
      //    printf("(%s) usage = %d, byte = 0x%02x\n", __func__, undx, uref.value&0xff);
   }   // loop over usages

   if (undx == 128) {   // if got them all
      result = buffer_new_with_value(edidbuf, 128, __func__);
   }

bye:
   if (debug)
      printf("(%s) Returning: %p\n", __func__, result);
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
   Buffer * result = NULL;
   struct edid_location * loc = locate_edid_report(fd);
   if (loc) {
      result = get_hiddev_edid_by_location(fd, loc);
   }
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
