/* usb_core.c
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <linux/hiddev.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wchar.h>

#include "util/hiddev_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/x11_util.h"         // *** TEMP **

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/execution_stats.h"
#include "base/linux_errno.h"

#include "i2c/i2c_bus_core.h"      // *** TEMP ***

#include "usb/usb_core.h"


// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_USB;

// Forward declarations
static GPtrArray * get_usb_monitor_list();

// Global variables
static GPtrArray * usb_monitors;    // array of Usb_Monitor_Info


#define HID_USAGE_PAGE_MASK   0xffff0000

#define HID_UP_MONITOR        0x00800000
#define HID_UP_MONITOR_ENUM   0x00810000
#define HID_UP_MONITOR_VESA   0x00820000


// In keeping with the style of Linux USB code, this module prefers
// "struct xxx {}" to "typedef {} xxx"




/* Reports contents of usb_monitor_vcp_rec struct */
static void report_usb_monitor_vcp_rec(Usb_Monitor_Vcp_Rec * vcprec, int depth) {
   const int d1 = depth+1;
   rpt_structure_loc("Usb_Monitor_Vcp_Rec", vcprec, depth);
   rpt_vstring(d1, "%-20s:    %-4.4s", "marker",       vcprec->marker);
   rpt_vstring(d1, "%-20s:    0x%02x", "vcp_code",     vcprec->vcp_code);
   rpt_vstring(d1, "%-20s:    %d",     "report_type",  vcprec->report_type);
   rpt_vstring(d1, "%-20s:    %d",     "report_id",    vcprec->report_id);
   rpt_vstring(d1, "%-20s:    %d",     "field_index",  vcprec->field_index);
   rpt_vstring(d1, "%-20s:    %d",     "usage_index",  vcprec->usage_index);
   // to be completed
   rpt_structure_loc("struct hiddev_report_info", vcprec->rinfo, d1);
   rpt_structure_loc("struct hiddev_field_info ", vcprec->finfo, d1);
   rpt_structure_loc("struct hiddev_usage_ref  ", vcprec->uref, d1);
}



/* Reports contents of Usb_Monitor_Info struct
 *
 * Arguments:
 *    moninfo     pointer to Monitor_Info
 *    depth       logical indentation depth
 *
 * Returns:       nothing
 */
static void report_usb_monitor_info(Usb_Monitor_Info * moninfo, int depth) {
   const int d1 = depth+1;
   const int d2 = depth+2;
   rpt_structure_loc("Usb_Monitor_Info", moninfo, d1);
   rpt_vstring(d1, "%-20s:    %-4.4s", "marker",              moninfo->marker);
   rpt_vstring(d1, "%-20s:    %s",     "hiddev_device_name",  moninfo->hiddev_device_name);
   rpt_vstring(d1, "%-20s:    %p",     "edid",                moninfo->edid);
   rpt_vstring(d1, "%-20s:    %p",     "hiddev_devinfo",      moninfo->hiddev_devinfo);
   rpt_title("Non-empty vcp_codes entries:", d1);
   int feature_code;
   for (feature_code = 0; feature_code < 256; feature_code++) {
      GPtrArray * monrecs = moninfo->vcp_codes[feature_code];
      if (monrecs) {
         rpt_vstring(d1, "vcp feature code 0x%02x has %d records:", feature_code, monrecs->len);
         for (int ndx=0; ndx<monrecs->len; ndx++) {
            report_usb_monitor_vcp_rec( g_ptr_array_index(monrecs, ndx), d2);
         }
      }
   }
}



static void report_usb_monitors(GPtrArray * monitors, int depth) {
   const int d1 = depth+1;

   rpt_vstring(depth, "GPtrArray of %d Usb_Monitor_Info at %p", monitors->len, monitors);
   for (int ndx = 0; ndx < monitors->len; ndx++) {
      report_usb_monitor_info( g_ptr_array_index(monitors, ndx), d1);
   }
}



/** Creates a capabilities string for the USB device.
 *
 *  Returns:   synthesized capabilities string, containing only a vcp segment
 *
 *  Note that the USB HID Monitor spec does not define a capabilities report.
 *
 *  It is the responsibility of the caller to free the returned string.
 */
static char * usb_synthesize_capabilities_string(Usb_Monitor_Info * moninfo) {
   assert(moninfo);
   char buf[1000];
   strcpy(buf,"(vcp(");
   bool firstcode = true;
   int curlen = 5;
   for (int feature_code=0; feature_code < 256; feature_code++) {
      if (moninfo->vcp_codes[feature_code]) {
         if (firstcode)
            firstcode = false;
         else {
            strcpy(buf+curlen, " ");
            curlen++;
         }
         sprintf(buf+curlen, "%02x", feature_code);
         curlen += 2;
      }
   }
   strcpy(buf+curlen, "))");
   char * result = strdup(buf);
   return result;
}



/* Locates all USB HID reports relating to querying and setting VCP feature values.
 *
 * Returns:  array of Usb_Monitor_Vcp_Rec for each usage
 */
GPtrArray * collect_vcp_reports(int fd) {
   bool debug = false;
   GPtrArray * vcp_reports = g_ptr_array_new();
   for (__u32 report_type = HID_REPORT_TYPE_MIN; report_type <= HID_REPORT_TYPE_MAX; report_type++) {
      int reportinfo_rc = 0;
      struct hiddev_report_info rinfo = {
          .report_type = report_type,
          .report_id   = HID_REPORT_ID_FIRST
       };

       while (reportinfo_rc >= 0) {
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

          int fndx, undx;
          for (fndx = 0; fndx < rinfo.num_fields; fndx++) {
             // printf("(%s) field index = %d\n", __func__, fndx);
             struct hiddev_field_info finfo = {
                   .report_type = rinfo.report_type,
                   .report_id   = rinfo.report_id,
                   .field_index = fndx
             };
             int saved_field_index = fndx;
             int rc = ioctl(fd, HIDIOCGFIELDINFO, &finfo);
             if (rc != 0)
                REPORT_IOCTL_ERROR("HIDIOCGFIELDINFO", rc);
             assert(rc == 0);
             if (finfo.field_index != saved_field_index && debug) {
                printf("(%s) !!! ioctl(HIDIOCGFIELDINFO) changed field_index from %d to %d\n",
                       __func__, saved_field_index, finfo.field_index);
                printf("(%s)   rinfo.num_fields=%d, finfo.maxusage=%d\n",
                       __func__, rinfo.num_fields, finfo.maxusage);
             }

             if (finfo.application != 0x00800001) // USB Monitor Page/Monitor Control
                continue;

             for (undx = 0; undx < finfo.maxusage; undx++) {
                struct hiddev_usage_ref uref = {
                       .report_type = rinfo.report_type,   // rinfo.report_type;
                       .report_id =   rinfo.report_id,     // rinfo.report_id;
                       .field_index = fndx,
                       .usage_index = undx
                };
                rc = ioctl(fd, HIDIOCGUCODE, &uref);    // Fills in usage code
                if (rc != 0) {
                    REPORT_IOCTL_ERROR("HIDIOCGUCODE", rc);
                    continue;
                }
                if ( (uref.usage_code & 0xffff0000) != 0x00820000)  // Monitor VESA Virtual Controls page
                   continue;
                Byte vcp_feature = uref.usage_code & 0xff;

                Usb_Monitor_Vcp_Rec * vcprec = calloc(1, sizeof(Usb_Monitor_Vcp_Rec));
                memcpy(vcprec->marker, USB_MONITOR_VCP_REC_MARKER, 4);
                vcprec->vcp_code = vcp_feature;
                vcprec->report_type = report_type;
                vcprec->report_id   = rinfo.report_id;
                vcprec->field_index = fndx;
                vcprec->usage_index = undx;
                struct hiddev_report_info * infoptr = malloc(sizeof(struct hiddev_report_info));
                memcpy(infoptr, &rinfo, sizeof(struct hiddev_report_info));
                vcprec->rinfo = infoptr;
                struct hiddev_field_info * fptr = malloc(sizeof(struct hiddev_field_info));
                memcpy(fptr, &finfo, sizeof(struct hiddev_field_info));
                vcprec->finfo = fptr;
                struct hiddev_usage_ref * uptr = malloc(sizeof(struct hiddev_usage_ref));
                memcpy(uptr, &uref, sizeof(struct hiddev_usage_ref));
                vcprec->uref = uptr;;

                g_ptr_array_add(vcp_reports, vcprec);

             }   // loop over usages

          } // loop over fields

          rinfo.report_id |= HID_REPORT_ID_NEXT;
       }
   }
   return vcp_reports;
}


/* Tests if a hiddev device (specified by its name) appears to
 * be a USB HID compliant monitor.
 *
 * This stripped down test implements the ddctool chkusbmon command,
 * which is intended for use in a udev rules test.
 *
 * Arguments:
 *    device_name        e.g. /dev/usb/hiddev3
 *
 * Returns:              true if device is a monitor,
 *                       false if not, or unable to open device
 *
 * Note that messages will not appear when this function runs as part
 * of normal udev execution.  They are intended to aid in debugging.
 */
bool check_usb_monitor( char * device_name ) {
   assert(device_name);
   bool debug = false;
   Output_Level ol = get_output_level();
   if (debug)
      ol = OL_VERBOSE;

   DBGMSF(debug, "Examining device: %s", device_name);
   bool result = false;

   int fd = open(device_name, O_RDONLY);
   if (fd < 1) {
      if (ol >= OL_VERBOSE)
         printf("Unable to open device %s: %s", device_name, strerror(errno));
      goto exit;
   }

   result = is_hiddev_monitor(fd);

   close(fd);

   if (ol >= OL_VERBOSE) {
      if (result)
         printf("Device %s appears to be a USB HID compliant monitor.\n", device_name);
      else
         printf("Device %s is not a USB HID compliant monitor.\n", device_name);
   }

 exit:
    return result;
 }



#ifdef NO_LONGER_USED
Display_Info * is_usb_display_device(char * device_name) {
   bool debug = true;

   const int d0 = 0;
   const int d1 = d0 + 1;
   // const int d2 = d0 + 2;
   // const int d3 = d0 + 3;
   int rc;

   DBGMSF(debug, "Examining device: %s", device_name);
   Display_Info * result = NULL;

   struct hiddev_devinfo dev_info;

   int fd = open(device_name, O_RDONLY);
   if (fd < 1) {
      perror("Unable to open device");
      goto bye;
    }

    char * cgname = get_hiddev_name(fd);               // HIDIOCGNAME
    // printf("(%s) get_hiddev_name() returned: |%s|\n", __func__, cgname);
    rpt_vstring(d1, "device name (reported by HIDIOCGNAME): |%s|", cgname);
    free(cgname);

    rc = ioctl(fd, HIDIOCGDEVINFO, &dev_info);
    if (rc != 0) {
       REPORT_IOCTL_ERROR("HIDIOCGDEVINFO", rc);
       goto close;
    }
    // report_hiddev_devinfo(&dev_info, d1);

    Buffer * pedid = NULL;
    if (is_hiddev_monitor(fd)) {
       pedid = get_hiddev_edid_with_backup(fd);
    }
    if (pedid) {
       Display_Ref * dref = create_usb_display_ref(dev_info.busnum, dev_info.devnum, device_name);
       // include vid/pid?
       // Parsed_Edid * parsed_edid = create_parsed_edid(pedid->edid);
       Parsed_Edid * parsed_edid = create_parsed_edid(pedid->bytes);   // assumes 128 byte edid
       report_parsed_edid(parsed_edid, true /*verbose*/, 0);

       result = calloc(1, sizeof(Display_Info));
       result->dispno = -1;    // will be set in caller
       result->dref   = dref;
       result->edid   = parsed_edid;
    }

close:
    close(fd);

bye:
   return result;
}
#endif


// *** Functions to find Usb_Monitor_Info for a display ***

static Usb_Monitor_Info * usb_find_monitor_by_busnum_devnum(int busnum, int devnum) {
   bool debug = false;
   DBGMSF(debug, "Starting. busnum=%d, devnum=%d", busnum, devnum);
   assert(usb_monitors);
   Usb_Monitor_Info * result = NULL;
   for (int ndx = 0; ndx < usb_monitors->len; ndx++) {
      struct usb_monitor_info * curmon = g_ptr_array_index(usb_monitors, ndx);
      struct hiddev_devinfo * devinfo = curmon->hiddev_devinfo;
      if (busnum == devinfo->busnum &&
          devnum == devinfo->devnum)
      {
         result = curmon;
         break;
      }
   }
   DBGMSF(debug, "Returning %p", result);
   return result;
}


static Usb_Monitor_Info * usb_find_monitor_by_display_ref(Display_Ref * dref) {
   bool debug = false;
   DBGMSF(debug, "Starting. dref = %s", dref_short_name(dref));
   assert(dref->io_mode == USB_IO);
   Usb_Monitor_Info * result = usb_find_monitor_by_busnum_devnum(dref->usb_bus, dref->usb_device);
   DBGMSF(debug, "Returning %p", result);
   return result;
}


Usb_Monitor_Info * usb_find_monitor_by_display_handle(Display_Handle * dh) {
   // printf("(%s) Starting. dh=%p\n", __func__, dh);
   bool debug = false;
   DBGMSF(debug, "Starting. dh = %s", display_handle_repr(dh));
   assert(dh->io_mode == USB_IO);
   Usb_Monitor_Info * result = NULL;
   result = usb_find_monitor_by_busnum_devnum(dh->usb_bus, dh->usb_device);
   DBGMSF(debug, "Returning %p", result);
   return result;
}


#ifdef APPARENTLY_UNUSED
char * get_hiddev_devname_by_display_ref(Display_Ref * dref) {
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_ref(dref);
   char * result = moninfo->hiddev_device_name;
   DBGMSG("dref=%s, returning: %s", dref_short_name(dref), result);
   return result;
}
#endif


//  *** Functions to return a Display_Ref for a USB monitor ***


static Display_Ref *
create_display_ref_from_usb_monitor_info(Usb_Monitor_Info * moninfo) {
   // hacky - to be cleaned up
   Display_Ref * dref = create_usb_display_ref(moninfo->hiddev_devinfo->busnum,
                                               moninfo->hiddev_devinfo->devnum,
                                               moninfo->hiddev_device_name);
   return dref;
}


Display_Ref *
usb_find_display_by_model_sn(const char * model, const char * sn) {
   Display_Ref * result = NULL;

   Usb_Monitor_Info * found_monitor = NULL;
   GPtrArray * all_usb_monitors = get_usb_monitor_list();
   for (int ndx=0; ndx<all_usb_monitors->len; ndx++) {
      Usb_Monitor_Info * curmon = g_ptr_array_index(all_usb_monitors, ndx);
      if ( strcmp(model, curmon->edid->model_name)   == 0 &&
           strcmp(sn,    curmon->edid->serial_ascii) == 0
         )
      {
          found_monitor = curmon;
          break;
       }
    }
    if (found_monitor)
       result = create_display_ref_from_usb_monitor_info(found_monitor);

   return result;
}


Display_Ref *
usb_find_display_by_busnum_devnum(int busnum, int devnum) {
   Display_Ref * result = NULL;

   Usb_Monitor_Info * found_monitor = NULL;
   GPtrArray * all_usb_monitors = get_usb_monitor_list();
   for (int ndx=0; ndx<all_usb_monitors->len; ndx++) {
      Usb_Monitor_Info * curmon = g_ptr_array_index(all_usb_monitors, ndx);
      if ( curmon->hiddev_devinfo->busnum == busnum &&
           curmon->hiddev_devinfo->devnum == devnum
         )
      {
          found_monitor = curmon;
          break;
       }
    }
    if (found_monitor)
       result = create_display_ref_from_usb_monitor_info(found_monitor);

   return result;
}


Display_Ref *
usb_find_display_by_edid(const Byte * edidbytes) {
   Display_Ref * result = NULL;

   Usb_Monitor_Info * found_monitor = NULL;
    GPtrArray * all_usb_monitors = get_usb_monitor_list();
    for (int ndx=0; ndx<all_usb_monitors->len; ndx++) {
       Usb_Monitor_Info * curmon = g_ptr_array_index(all_usb_monitors, ndx);
       if ( memcmp(edidbytes, curmon->edid->bytes, 128) == 0) {
          found_monitor = curmon;
          break;
       }
    }
    if (found_monitor)
       result = create_display_ref_from_usb_monitor_info(found_monitor);

   return result;
}


bool usb_is_valid_display_ref(Display_Ref * dref, bool emit_error_msg) {
   bool result = true;
   if (!usb_find_monitor_by_display_ref(dref)) {
      result = false;
      if (emit_error_msg)
         fprintf(stderr, "Invalid Display_Ref\n");
   }
   return result;
}


void usb_report_active_display_by_display_ref(Display_Ref * dref, int depth) {
   Output_Level output_level = get_output_level();
   rpt_vstring(depth, "USB bus:device:      %d:%d", dref->usb_bus, dref->usb_device);

   Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_ref(dref);

   if (output_level == OL_TERSE || output_level == OL_PROGRAM)
      rpt_vstring(depth, "Monitor:             %s:%s:%s",
                         moninfo->edid->mfg_id,
                         moninfo->edid->model_name,
                         moninfo->edid->serial_ascii);
   if (output_level >= OL_NORMAL) {
      rpt_vstring(depth, "Device name:         %s",    dref->usb_hiddev_name);
      rpt_vstring(depth, "Vendor id:           %04x",  moninfo->hiddev_devinfo->vendor  & 0xffff);
      rpt_vstring(depth, "Product id:          %04x",  moninfo->hiddev_devinfo->product & 0xffff);
      bool dump_edid = (output_level >= OL_VERBOSE);

      report_parsed_edid(moninfo->edid, dump_edid /* verbose */, depth);
   }
}

Parsed_Edid * usb_get_parsed_edid_by_display_ref(Display_Ref * dref) {
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_ref(dref);
   return moninfo->edid;
}

Parsed_Edid * usb_get_parsed_edid_by_display_handle(Display_Handle * dh) {
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_handle(dh);
   return moninfo->edid;
}



/* Open a USB device
 *
 * Arguments:
 *   hiddev_devname
 *   emit_error_msg   if true, output message if error
 *
 * Returns:
 *   file descriptor ( > 0) if success
 *   -errno if failure
 *
 */
int usb_open_hiddev_device(char * hiddev_devname, bool emit_error_msg) {
   bool debug = false;
   DBGMSF(debug, "hiddev_devname=%s", hiddev_devname);

   int  file;

   RECORD_IO_EVENT(
         IE_OPEN,
         ( file = open(hiddev_devname, O_RDWR) )
         );
   // per man open:
   // returns file descriptor if successful
   // -1 if error, and errno is set
   int errsv = errno;
   if (file < 0) {
      if (emit_error_msg)
         f0printf(FERR, "Open failed for %s: errno=%s\n", hiddev_devname, linux_errno_desc(errsv));
      file = -errno;
   }
   DBGMSF(debug, "open() finished, file=%d", file);

   if (file > 0)
   {
      // Solves problem of ddc detect not getting edid unless ddctool env called first
      errsv = errno;
      int rc = ioctl(file, HIDIOCINITREPORT);
      if (rc != 0) {
         REPORT_IOCTL_ERROR("HIDIOCGREPORT", rc);
         // printf("(%s) HIDIOCINITREPORT failed\n", __func__  );
      }
   }
   DBGMSF(debug, "Returning %d", file);
   return file;
}


/* Closes an open USB device.
 *
 * Arguments:
 *   fd     file descriptor for open hiddev device
 *   device_fn
 *          if NULL, ignore
 *   failure_action  if true, exit if close fails
 *
 * Returns:
 *    0 if success
 *    -errno if close fails and exit on failure was not specified
 */
int usb_close_device(int fd, char * device_fn, Failure_Action failure_action) {
   bool debug = false;
   DBGMSF(debug, "Starting. fd=%d", fd);

   errno = 0;
   int rc = 0;
   RECORD_IO_EVENT(IE_CLOSE, ( rc = close(fd) ) );
   int errsv = errno;
   if (rc < 0) {
      // EBADF  fd isn't a valid open file descriptor
      // EINTR  close() interrupted by a signal
      // EIO    I/O error
      char workbuf[300];
      if (device_fn)
         snprintf(workbuf, 300,
                  "Close failed for USB device %s. errno=%s",
                  device_fn, linux_errno_desc(errsv));
      else
         snprintf(workbuf, 300,
                  "USB device close failed. errno=%s",
                  linux_errno_desc(errsv));

      if (failure_action == EXIT_IF_FAILURE)
         TERMINATE_EXECUTION_ON_ERROR(workbuf);

      fprintf(stderr, "%s\n", workbuf);

      rc = errsv;
   }
   return rc;
}


//
// Functions to get EDID
//



struct hid_field_locator * find_eizo_model_sn_report(int fd) {
   // struct hiddev_report_info * rinfo = calloc(1,sizeof(struct hiddev_report_info));

   // find report
   // Find: HID_REPORT_TYPE_FEATURE
   // field application(usage)  00800001      USB Monitor/Monitor Control
   // flags   HID_FIELD_VARIABLE | HID_FIELD_BUFFERED_BYTE
   // ucode: 0xff000035


   bool debug = true;
   struct hid_field_locator * loc = NULL;
   struct hiddev_devinfo dev_info;

   int rc = ioctl(fd, HIDIOCGDEVINFO, &dev_info);
   if (rc != 0) {
      REPORT_IOCTL_ERROR("HIDIOCGDEVINFO", rc);
      goto bye;
   }
   if (dev_info.vendor == 0x056d && dev_info.product == 0x0002)  {
      loc = find_report(fd, HID_REPORT_TYPE_FEATURE, 0xff000035, /*match_all_ucodes=*/false);
   }

bye:
   if (debug) {
      printf("(%s) Returning: %p\n", __func__, loc);
      if (loc)
         report_hid_field_locator(loc,2);
   }
   return loc;
}


struct model_sn_pair {
   char * model;
   char * sn;
};

void free_model_sn_pair(struct model_sn_pair * p) {
   if (p) {
      free(p->model);
      free(p->sn);
      free(p);
   }
}

void report_model_sn_pair(struct model_sn_pair * p, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("struct model_sn_pair",p, depth);
   rpt_vstring(d1, "model:  %s", p->model);
   rpt_vstring(d1, "sn:     %s", p->sn);
}


bool is_eizo_monitor(int fd) {
   bool debug = true;
   bool result = false;
   struct hiddev_devinfo dev_info;
   int rc = ioctl(fd, HIDIOCGDEVINFO, &dev_info);
   if (rc != 0) {
      REPORT_IOCTL_ERROR("HIDIOCGDEVINFO", rc);
      goto bye;
   }
   if (dev_info.vendor == 0x056d && dev_info.product == 0x0002)
      result = true;

bye:
   DBGMSF(debug, "Returning %s", bool_repr(result));
   return result;
}


struct model_sn_pair *  get_eizo_model_sn_by_report(int fd) {
   bool debug = true;
   struct model_sn_pair* result = NULL;

   if (is_eizo_monitor(fd)) {
      struct hid_field_locator * loc = find_eizo_model_sn_report(fd);
      DBGMSF(debug, "find_eizo_model_sn_report() returned: %p", loc);
      if (loc) {
         // get report
         Buffer * modelsn = get_multibyte_report_value(fd, loc);
         if (modelsn) {
            assert(modelsn->len >= 16);
            result = calloc(1, sizeof(struct model_sn_pair));
            result->model = calloc(1,9);
            result->sn    = calloc(1,9);
            memcpy(result->sn, modelsn->bytes,8);
            result->sn[8] = '\0';
            memcpy(result->model, modelsn->bytes+8, 8);
            result->model[8] = '\0';
            rtrim_in_place(result->sn);
            rtrim_in_place(result->model);
            free(modelsn);
         }
      }
   }

   if (debug) {
      printf("(%s) Returning: %p\n", __func__, result);
      if (result)
         report_model_sn_pair(result, 1);
   }
   return result;

}



Parsed_Edid * get_x11_edid_by_model_sn(char * model_name, char * sn_ascii) {
   bool debug = true;
   Parsed_Edid * parsed_edid = NULL;

   GPtrArray* edid_recs = get_x11_edids();
   puts("");
   printf("EDIDs reported by X11 for connected xrandr outputs:\n");
   // DBGMSG("Got %d X11_Edid_Recs\n", edid_recs->len);


   int ndx = 0;
   for (ndx=0; ndx < edid_recs->len; ndx++) {
      X11_Edid_Rec * prec = g_ptr_array_index(edid_recs, ndx);
      // printf(" Output name: %s -> %p\n", prec->output_name, prec->edid);
      // hex_dump(prec->edid, 128);
      rpt_vstring(1, "xrandr output: %s", prec->output_name);
      parsed_edid = create_parsed_edid(prec->edidbytes);
      if (parsed_edid) {
         bool verbose_edid = false;
         report_parsed_edid(parsed_edid, verbose_edid, 2 /* depth */);
         if (streq(parsed_edid->model_name, model_name) &&
               streq(parsed_edid->serial_ascii, sn_ascii) )
         {
            printf("(%s) Found matching EDID from X11\n", __func__);
            break;
         }
         free_parsed_edid(parsed_edid);
      }
      else {
         printf(" Unparsable EDID for output name: %s -> %p\n", prec->output_name, prec->edidbytes);
         hex_dump(prec->edidbytes, 128);
      }
   }

   // HACK FOR TESTING
   if (!parsed_edid && edid_recs->len > 0) {
      printf("(%s) HACK FOR TESTING: Using last X11 EDID\n", __func__);
      X11_Edid_Rec * prec = g_ptr_array_index(edid_recs, edid_recs->len-1);
      parsed_edid = create_parsed_edid(prec->edidbytes);
   }

   g_ptr_array_free(edid_recs, true);


   DBGMSF(debug, "returning %p", parsed_edid);
   return parsed_edid;
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
Parsed_Edid * get_hiddev_edid_with_backup(int fd)  {
   bool debug = true;
   Parsed_Edid * parsed_edid = NULL;
   DBGMSF(debug, "Starting");
   Buffer * edid_buffer = get_hiddev_edid(fd);

   // if edid_buffer == NULL, issue message

   if (edid_buffer) {
       parsed_edid = create_parsed_edid(edid_buffer->bytes);
       if (!parsed_edid) {
          DBGMSF(debug, "get_hiddev_edid() returned invalid EDID");
          // if debug or verbose, dump the bad edid
       }
    }

   struct model_sn_pair * model_sn = NULL;

   if (!edid_buffer) {
      if (is_eizo_monitor(fd)) {
         printf("(%s) *** Special fixup for Eizo monitor ***\n", __func__);

         model_sn = get_eizo_model_sn_by_report(fd);
         if (model_sn) {
            Bus_Info * bus_info = i2c_find_bus_info_by_model_sn(model_sn->model, model_sn->sn);
            if (bus_info) {
               printf("(%s) Using EDID for /dev/i2c-%d\n", __func__, bus_info->busno);
               parsed_edid = bus_info->edid;
               // result = NULL;   // for testing - both i2c and X11 methods work
            }
            else {
               // TODO: try ADL
            }
         }
      }
   }

   // if (model_sn) {
   if (!parsed_edid && model_sn) {
      parsed_edid = get_x11_edid_by_model_sn(model_sn->model, model_sn->sn);
   }

   DBGMSF(debug, "Returning: %p", parsed_edid);
   return parsed_edid;
}


//
//
//


/*  Examines all hiddev devices to see if they are USB HID compliant monitors.
 *  If so, obtains the EDID, determines which reports to use for VCP feature
 *  values, etc.
 *
 *  Returns:   array of pointers to USB_Mon_Info records
 *
 *  The result is cached in global variable usb_monitors
 */
static GPtrArray * get_usb_monitor_list() {
   bool debug = true;
   DBGMSF(debug, "Starting...");
   Output_Level ol = get_output_level();

   if (usb_monitors)      // already initialized?
      return usb_monitors;

   usb_monitors = g_ptr_array_new();

   GPtrArray * hiddev_names = get_hiddev_device_names();
   for (int devname_ndx = 0; devname_ndx < hiddev_names->len; devname_ndx++) {
      char * hiddev_fn = g_ptr_array_index(hiddev_names, devname_ndx);
      DBGMSF(debug, "Examining device: %s", hiddev_fn);
      // will need better message handling for API
      int fd = usb_open_hiddev_device(hiddev_fn, (ol >= OL_VERBOSE));
      if (fd > 0) {
         // Declare variables here and initialize them to NULL so that code at label close: works
         struct hiddev_devinfo *   devinfo     = NULL;
         char *                    cgname      = NULL;
         // struct edid_report *      pedid       = NULL;
         Buffer *                  pedid       = NULL;
         Parsed_Edid *             parsed_edid = NULL;
         GPtrArray *               vcp_reports = NULL;
         Usb_Monitor_Info *        moninfo =     NULL;

         devinfo = calloc(1,sizeof(struct hiddev_devinfo));

         cgname = get_hiddev_name(fd);               // HIDIOCGNAME
         int rc = ioctl(fd, HIDIOCGDEVINFO, devinfo);
         if (rc != 0) {
            REPORT_IOCTL_ERROR("HIDIOCGDEVINFO", rc);
            goto close;
         }

         if (!is_hiddev_monitor(fd))
            goto close;

         // pedid = find_edid_report(fd);
         parsed_edid = get_hiddev_edid_with_backup(fd);

         if (!parsed_edid) {
            fprintf(stderr, "Monitor on device %s reports no EDID or has invalid EDID. Ignoring.\n", hiddev_fn);
            goto close;
         }

         vcp_reports = collect_vcp_reports(fd);

         moninfo = calloc(1,sizeof(Usb_Monitor_Info));
         moninfo-> hiddev_device_name = strdup(hiddev_fn);
         moninfo->edid = parsed_edid;
         parsed_edid = NULL;
         moninfo->hiddev_devinfo = devinfo;
         devinfo = NULL;

         //    Usb_Monitor_Vcp_Rec *    vcp_codes[256];

         // Distribute the accumulated vcp reports by feature code
         for (int ndx = 0; ndx < vcp_reports->len; ndx++) {
             Usb_Monitor_Vcp_Rec * cur_vcp_rec = g_ptr_array_index(vcp_reports, ndx);
             Byte curvcp = cur_vcp_rec->vcp_code;
             GPtrArray * cur_code_table_entry = moninfo->vcp_codes[curvcp];
             if (!cur_code_table_entry) {
                cur_code_table_entry = g_ptr_array_new();
                moninfo->vcp_codes[curvcp] = cur_code_table_entry;
             }
             g_ptr_array_add(cur_code_table_entry, cur_vcp_rec);
         }
         // free vcp_reports without freeing the entries, which are now pointed to
         // by moninfo->vcp_codes
         // n. no free function set
         g_ptr_array_free(vcp_reports, true);

         g_ptr_array_add(usb_monitors, moninfo);

 close:
         if (devinfo)
            free(devinfo);
         if (cgname)
            free(cgname);
         if (pedid)
            free(pedid);
         if (parsed_edid)
            free(parsed_edid);
         usb_close_device(fd, hiddev_fn, RETURN_ERROR_IF_FAILURE);
      }  // monitor opened


   } // loop over device names

   g_ptr_array_set_free_func(hiddev_names, free);
   g_ptr_array_free(hiddev_names, true);

   if (debug) {
      DBGMSG("Returning monitor list:");
      report_usb_monitors(usb_monitors,1);
   }

   return usb_monitors;
}


/* Returns a list of all valid USB HID compliant monitors,
 * in a form expected by higher levels of ddctool, namely
 * a collection of Display_Refs
 *
 * Arguments:    none
 *
 * Returns:      Display_Info_List of Display_Refs
 */
Display_Info_List usb_get_valid_displays() {
   bool debug = false;
   get_usb_monitor_list();

   Display_Info_List info_list = {0,NULL};
   Display_Info info_recs[256];

   DBGMSF(debug, "Found %d USB displays", __func__, usb_monitors->len);
   info_list.info_recs = calloc(usb_monitors->len,sizeof(Display_Info));
   for (int ndx=0; ndx<usb_monitors->len; ndx++) {
      Usb_Monitor_Info  * curmon = g_ptr_array_index(usb_monitors,ndx);
      Display_Ref * dref = create_usb_display_ref(
                              curmon->hiddev_devinfo->busnum,
                              curmon->hiddev_devinfo->devnum,
                              curmon->hiddev_device_name);
      info_recs[ndx].dispno = -1;    // not yet set
      info_recs[ndx].dref = dref;
      info_recs[ndx].edid = curmon->edid;
   }
   memcpy(info_list.info_recs, info_recs, (usb_monitors->len)*sizeof(Display_Info));
   info_list.ct = usb_monitors->len;

   if (debug) {
      DBGMSG("Done. Returning:");
      report_display_info_list(&info_list, 1);
   }

   return info_list;
}


//
// *** Miscellaneous services ***
//

char * usb_get_capabilities_string_by_display_handle(Display_Handle * dh) {
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_handle(dh);
   assert(dh);
   return usb_synthesize_capabilities_string(moninfo);
}



