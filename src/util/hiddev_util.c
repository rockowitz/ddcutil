/* hiddev_util.c
 *
 * Created on: Apr 26, 2016
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <stddef.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <wchar.h>
#include <dirent.h>
#include <glib.h>
#include <strings.h>

#include <linux/limits.h>

#include <linux/hiddev.h>

#include "base/common.h"
#include "base/msg_control.h"
#include "base/execution_stats.h"
#include "base/linux_errno.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"    // for Parsed_Nontable_Vcp_Response    - to sort out

#include "util/coredefs.h"
#include "util/string_util.h"
#include "util/report_util.h"
#include "util/hiddev_reports.h"
#include "util/hiddev_util.h"




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
GPtrArray * get_hiddev_device_names() {
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



// Per USB Monitor Control Class Specification section 5.5,
// "to identify a HID class device as a monitor, the devices's
// HID Report Descriptor must contain a top-level collection with
// a usage of Monitor Control from the USB Monitor Usage Page."

bool is_hiddev_monitor(int fd) {
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
      if (cinfo.level == 0 && cinfo.usage == 0x00800001) { // USB Monitor Usage Page/Monitor Control
         monitor_collection_index = cndx;
         break;
      }
   }

   bool result = (monitor_collection_index >= 0);
   return result;
}


/* Does a field report an EDID?  The field must have at least 128 usages, and
 * the usage code for each must be USB Monitor/EDID information
 */
struct hiddev_field_info *
is_field_edid(int fd, struct hiddev_report_info * rinfo, int field_index) {
   bool debug = false;
   if (debug)
      printf("(%s) report_id=%d, field index = %d\n", __func__, rinfo->report_id, field_index);

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
         if (uref.usage_code != 0x00800002) {   // USB Monitor/EDID Information
            all_usages_edid = false;
      }
   }   // loop over usages
   if (all_usages_edid)
      result = malloc(sizeof(struct hiddev_field_info));
      memcpy(result, &finfo, sizeof(struct hiddev_field_info));
   }

bye:
   return result;
}






#define EDID_SIZE 128

#ifdef OLD
struct edid_report {
   int report_id;
   int field_id;
   Byte edid[EDID_SIZE];
};

// a resusable fragment
Byte * get_hiddev_edid_base(
      int fd,
      struct hiddev_report_info * rinfo,
      int field_index)
{
   Byte edidbuf[128];
   int rc;

   printf("(%s) report_id=%d, field index = %d\n", __func__, rinfo->report_id, field_index);
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
   if (finfo.field_index != saved_field_index) {
      printf("(%s) !!! ioctl(HIDIOCGFIELDINFO) changed field_index from %d to %d\n",
             __func__, saved_field_index, finfo.field_index);
      printf("(%s)   rinfo.num_fields=%d, finfo.maxusage=%d\n",
             __func__, rinfo->num_fields, finfo.maxusage);
   }
   // result->field_id = fndx;

   bool all_usages_edid = true;
   int undx;
   for (undx = 0; undx < finfo.maxusage && all_usages_edid; undx++) {
      struct hiddev_usage_ref uref = {
          .report_type = rinfo->report_type,   // rinfo.report_type;
          .report_id =   rinfo->report_id,     // rinfo.report_id;
          .field_index = saved_field_index,    // use original value, not value changed form HIDIOCGFIELDINFO
          .usage_index = undx
      };
      // printf("(%s) report_type=%d, report_id=%d, field_index=%d, usage_index=%d\n",
      //       __func__, rinfo->report_type, rinfo->report_id, field_index=saved_field_index, undx);
      rc = ioctl(fd, HIDIOCGUCODE, &uref);    // Fills in usage code
      if (rc != 0) {
          REPORT_IOCTL_ERROR("HIDIOCGUCODE", rc);
          all_usages_edid = false;
          continue;
      }
      // printf("(%s) uref.field_index=%d, uref.usage_code = 0x%08x\n",
      //        __func__, uref.field_index, uref.usage_code);
      if (uref.usage_code != 0x00800002) {   // USB Monitor/EDID Information
         all_usages_edid = false;
         continue;
      }
      // Only interested in first 128 bytes
      if (undx < 128) {
         rc = ioctl(fd, HIDIOCGUSAGE, &uref);  // Fills in usage value
         if (rc != 0) {
            REPORT_IOCTL_ERROR("HIDIOCGUSAGE", rc);
            all_usages_edid = false;
            continue;
         }
         edidbuf[undx] = uref.value & 0xff;
         // printf("(%s) byte = 0x%02x\n", __func__, uref.value&0xff);
      }
   }   // loop over usages
   Byte * result = NULL;
   printf("(%s) all_usages_edid = %d\n", __func__, all_usages_edid);
   if (all_usages_edid) {

      result = malloc(128);
      memcpy(result, edidbuf, 128);
   }
   return result;
}
#endif

struct edid_location {
   struct hiddev_report_info * rinfo;
   struct hiddev_field_info  * finfo;
   int                         report_id;
   int                         field_index;
};

void free_edid_location(struct edid_location * location) {
   if (location) {
      if (location->rinfo)
         free(location->rinfo);
      if (location->finfo)
         free(location->finfo);
      free(location);
   }
}


struct edid_location *
locate_edid_report(int fd) {
   bool debug = false;

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

   return result;
}






#ifdef OLD
/* Locates the report for querying the monitor's EDID, and also
 * returns the EDID value (first 128 bytes only).
 *
 * Arguments:   fd    file descriptor for open USB monitor hiddev device
 *
 * Returns:   struct edid_report, containing:
 *                report number
 *                raw edid bytes
 *
 * The caller is responsible for freeing the returned struct
 */
struct edid_report * find_edid_report(int fd) {
   bool debug = false;
   int reportinfo_rc = 0;
   struct hiddev_report_info rinfo = {
      .report_type = HID_REPORT_TYPE_FEATURE,
      .report_id   = HID_REPORT_ID_FIRST
   };

   struct edid_report * result = calloc(1, sizeof(struct edid_report));

   bool report_found = false;
   while (reportinfo_rc >= 0 && !report_found) {
       // printf("(%s) Report counter %d, report_id = 0x%08x %s\n",
       //       __func__, rptct, rinfo.report_id, interpret_report_id(rinfo.report_id));

      errno = 0;
      reportinfo_rc = ioctl(fd, HIDIOCGREPORTINFO, &rinfo);
      if (reportinfo_rc != 0) {    // no more reports
         if (reportinfo_rc != -1)
            REPORT_IOCTL_ERROR("HIDIOCGREPORTINFO", reportinfo_rc);
         break;
      }
      // result->report_id = rinfo.report_id;

      if (rinfo.num_fields == 0)
         break;


      // So that usage value filled in
      int rc = ioctl(fd, HIDIOCGREPORT, rinfo);
      if (rc != 0) {
         REPORT_IOCTL_ERROR("HIDIOCGREPORT", rc);
         printf("(%s) Unable to get Feature report %d\n", __func__, rinfo.report_id);
         break;
      }






      bool field_found = false;
      int fndx;
      for (fndx = 0; fndx < rinfo.num_fields && !field_found; fndx++) {

         Byte * edidbytes = get_hiddev_edid_base(fd, &rinfo, fndx);

         if (edidbytes) {
            field_found = true;
            result->field_id = fndx;
            result->report_id = rinfo.report_id;
            memcpy(result->edid, edidbytes, 128);
            free(edidbytes);
         }
      } // loop over fields

      if (field_found)
         report_found = true;
      else
         rinfo.report_id |= HID_REPORT_ID_NEXT;
    }  // loop over reports

   if (!report_found) {
      free(result);
      result = NULL;
   }

   if (debug) {
      if (result) {
         DBGMSG("Returning report_id=%d, field_id=%d, edid bytes:",
               __func__, result->report_id, result->field_id);
         hex_dump(result->edid, EDID_SIZE);
      }
      else
         DBGMSG("Returning NULL");
   }

   return result;
}
#endif


Buffer * get_hiddev_edid_by_location(int fd, struct edid_location * loc) {
   assert(loc);
   bool debug = false;
   if (debug) {
      printf("(%s) Starting.  loc=%p, loc->report_id=%d, loc->field_index=%d\n",
             __func__, loc, loc->report_id, loc->field_index);
      report_hiddev_report_info(loc->rinfo, 1);
      report_hiddev_field_info(loc->finfo, 1);
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
          .report_type = loc->rinfo->report_type,  // rinfo.report_type;
          .report_id   = loc->report_id,           // rinfo.report_id may have flag bits set
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
   return result;
}





#ifdef OLD
Buffer * get_hiddev_edid_old(int fd) {
   Buffer * result = NULL;
   struct edid_report * er = find_edid_report(fd);
   if (er) {
      result = buffer_new_with_value(er->edid, EDID_SIZE, __func__);
      free(er);
   }
   return result;
}
#endif

Buffer * get_hiddev_edid(int fd)  {
   Buffer * result = NULL;
   struct edid_location * loc = locate_edid_report(fd);
   if (loc) {
      result = get_hiddev_edid_by_location(fd, loc);
   }
   return result;
}



// Gets device name - ioctl(HIDIOCGNAME)
char * get_hiddev_name(int fd) {
   const int blen = 256;
   char buf[blen];
   // returns length of result, including terminating null
   int rc = ioctl(fd, HIDIOCGNAME(blen), buf);
   // printf("(%s) HIDIOCGNAME returned %d\n", __func__, rc);
   // hex_dump(buf,64);
   char * result = NULL;
   if (rc >= 0)
      result = strdup(buf);
   return result;
}



