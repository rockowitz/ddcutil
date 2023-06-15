/** @file usb_displays.c
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <linux/hiddev.h>
#include <sys/ioctl.h>
#include <unistd.h>
/** \endcond */

#include "util/device_id_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/udev_usb_util.h"
#include "util/udev_util.h"

#include "usb_util/hiddev_reports.h"
#include "usb_util/hiddev_util.h"
#include "usb_util/usb_hid_common.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/execution_stats.h"
#include "base/linux_errno.h"
#include "base/rtti.h"

#include "usb/usb_base.h"
#include "usb/usb_edid.h"

#include "usb/usb_displays.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_USB;  // may be unused if all diagnostics turned off

// // n. #pragma GCC diagnostic ignored "-Wunused-variable" not working
// void usb_core_unused_function_to_avoid_unused_variable_warning() {
//    printf("0x%02x\n",TRACE_GROUP);
// }

// // Forward declarations
// static GPtrArray * get_usb_monitor_list();  // returns array of Usb_Monitor_Info

// Global variables
static GPtrArray * usb_monitors = NULL;    // array of Usb_Monitor_Info
static GPtrArray * usb_open_errors = NULL;


#define HID_USAGE_PAGE_MASK   0xffff0000

#define HID_UP_MONITOR        0x00800000
#define HID_UP_MONITOR_ENUM   0x00810000
#define HID_UP_MONITOR_VESA   0x00820000


// In keeping with the style of Linux USB code, this module prefers
// "struct xxx {}" to "typedef {} xxx"


//
// Data Structures
//
// Report and manage data structures for this module.
// Some data structures are defined here, others in usb_core.h
//

/** Emits a debugging report of a #Usb_Monitor_Vcp_Rec struct describing
 *  a single USB "report"
 *
 *  @param vcprec  pointer to struct
 *  @param depth   logical indentation depth
 */
static void
dbgrpt_usb_monitor_vcp_rec(Usb_Monitor_Vcp_Rec * vcprec, int depth) {
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


static void
free_usb_monitor_vcp_rec(gpointer p) {
   struct usb_monitor_vcp_rec * vrec = p;
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_USB, "vrec = %p", vrec);

   if (vrec) {
      assert(memcmp(vrec->marker, USB_MONITOR_VCP_REC_MARKER, 4) == 0);
      free(vrec->rinfo);
      free(vrec->finfo);
      free(vrec->uref);
      vrec->marker[3] = 'x';
      free(vrec);
   }
   DBGTRC_DONE(debug, DDCA_TRC_USB, "");
}


/** Emits a debugging report of a #Usb_Monitor_Info struct
 *
 *  @param  moninfo     pointer to instance
 *  @param  depth       logical indentation depth
 */
void
dbgrpt_usb_monitor_info(Usb_Monitor_Info * moninfo, int depth) {
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
         rpt_vstring(d1, "vcp_codes[0x%02x]=%p is a GPtrArray with %d records:",
               feature_code, monrecs, monrecs->len);
         for (int ndx=0; ndx<monrecs->len; ndx++) {
            dbgrpt_usb_monitor_vcp_rec( g_ptr_array_index(monrecs, ndx), d2);
         }
      }
   }
}


static void
free_usb_monitor_info(gpointer p) {
   struct usb_monitor_info * moninfo = p;
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "moninfo = %p", moninfo);

   if (moninfo) {
      assert(memcmp(moninfo->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
      if (debug)
         dbgrpt_usb_monitor_info(moninfo, 2);
      free(moninfo->hiddev_device_name);
      DBGMSF(debug, "Freeing moninfo->edid = %p", moninfo->edid);
      free_parsed_edid(moninfo->edid);
      free(moninfo->hiddev_devinfo);
      for (int ndx = 0; ndx < 256; ndx++) {
         if (moninfo->vcp_codes[ndx]) {
            g_ptr_array_set_free_func(moninfo->vcp_codes[ndx], free_usb_monitor_vcp_rec);
            g_ptr_array_free(moninfo->vcp_codes[ndx], true);
         }
      }
      moninfo->marker[3] = 'x';
      free(moninfo);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


/** Reports on an array of #Usb_Monitor_info structs
 *
 *  @param monitors    pointer to GPtrArray of pointer to struct #Usb_Monitor_Info
 *  @param depth       logical indentation depth
 */
static void
report_usb_monitors(GPtrArray * monitors, int depth) {
   const int d1 = depth+1;

   rpt_vstring(depth, "GPtrArray of %d Usb_Monitor_Info at %p", monitors->len, monitors);
   for (int ndx = 0; ndx < monitors->len; ndx++) {
      dbgrpt_usb_monitor_info( g_ptr_array_index(monitors, ndx), d1);
   }
}


//
// HID Report Inquiry
//

Usb_Monitor_Vcp_Rec * create_usb_monitor_vcp_rec(Byte feature_code) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "feature_code=0x%02x", feature_code);
   Usb_Monitor_Vcp_Rec * vcprec = calloc(1, sizeof(Usb_Monitor_Vcp_Rec));
   memcpy(vcprec->marker, USB_MONITOR_VCP_REC_MARKER, 4);
   vcprec->vcp_code = feature_code;
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %p", vcprec);
   return vcprec;
}



/** Locates all USB HID reports for a device that relate to querying and
 *  setting VCP feature values.
 *
 *  @param  fd  file descriptor of open HID device
 *  @return array of #Usb_Monitor_Vcp_Rec for each usage
 */
static GPtrArray *
collect_vcp_reports(int fd) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
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
          reportinfo_rc = hiddev_get_report_info(fd, &rinfo, CALLOPT_ERR_MSG); //HIDIOCGDEVINFO
          // reportinfo_rc = ioctl(fd, HIDIOCGREPORTINFO, &rinfo);
          if (reportinfo_rc != 0) {    // no more reports
             assert( reportinfo_rc == -1);
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
             Byte callopts = CALLOPT_ERR_MSG;
             if (debug)
                callopts |= CALLOPT_WARN_FINDEX;
             int rc = hiddev_get_field_info(fd, &finfo, callopts);
             if (rc < 0)
                continue;
             if (finfo.application != 0x00800001) // USB Monitor Page/Monitor Control
                continue;

             for (undx = 0; undx < finfo.maxusage; undx++) {
                struct hiddev_usage_ref uref = {
                       .report_type = rinfo.report_type,   // rinfo.report_type;
                       .report_id =   rinfo.report_id,     // rinfo.report_id;
                       .field_index = fndx,
                       .usage_index = undx
                };
                int rc = hiddev_get_usage_code(fd, &uref, CALLOPT_ERR_MSG);
                if (rc < 0)
                   continue;
                if ( (uref.usage_code & 0xffff0000) != 0x00820000)  // Monitor VESA Virtual Controls page
                   continue;
                Byte vcp_feature = uref.usage_code & 0xff;

                Usb_Monitor_Vcp_Rec * vcprec = create_usb_monitor_vcp_rec(vcp_feature);
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
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %d VCP reports", vcp_reports->len);
   return vcp_reports;
}


//
// Capabilities
//

/** Creates a capabilities string for the USB device.
 *
 *  @param  pointer to #Usb_Monitor_Info instance
 *  @return synthesized capabilities string, containing only a vcp segment
 *
 *  It is the responsibility of the caller to free the returned string
 *
 *  @remark
 *  Note that the USB HID Monitor spec does not define a capabilities report.
 */
static char *
usb_synthesize_capabilities_string(Usb_Monitor_Info * moninfo) {
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
   char * result = g_strdup(buf);
   return result;
}


/** Checks the interfaces for a device to determine if it may
 *  be a keyboard or mouse, in which case it should not be probed.
 *
 *  @param  interfaces interface ids, separated by ":"
 *  @return true/false
 */
static bool
avoid_device_by_usb_interfaces_property_string(const char * interfaces) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "interfaces = |%s|", interfaces);

   Null_Terminated_String_Array pieces = strsplit(interfaces, ":");
   // if (debug)
   //    ntsa_show(pieces);

   bool avoid = false;
   int ndx = 0;
   while (pieces[ndx]) {
       //    Interface Class    03  Human Interface Device
       //    Interface Subclass 01  Boot Interface Subclass
       //    Interface Protocol 02  Mouse
       //    Interface Protocol 01  Keyboard
       //
       //    Q: is it even possible to have a interface protocol mouse when
       //    sublass is not Boot Interface?  We're extra careful
       if (
             // streq( pieces[ndx], "030102" )   ||      // mouse
             // streq( pieces[ndx], "030101")   ||       // keyboard
             (strncmp(pieces[ndx], "03",   2) != 0 ) ||  // not a HID device (why were re even called?)
             (strncmp(pieces[ndx], "0301", 4) == 0 ) ||  // any HID boot interface subclass device
             (strncmp(pieces[ndx]+4, "01", 2) == 0 ) ||  // any keyboard
             (strncmp(pieces[ndx]+4, "02", 2) == 0 )     // any mouse
          )
       {
          avoid = true;
          DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Avoiding device with interface %s", pieces[ndx]);
          break;
       }
       ndx++;
    }
    ntsa_free(pieces, true);

    DBGTRC_RET_BOOL(debug, TRACE_GROUP, avoid, "");
    return avoid;
}


/**
 * Verifies that the device class of the Monitor is 3 (HID Device) and
 * that the subclass and interface do not indicate a mouse or keyboard.
 *
 *  @param   hiddev  device name
 *  @return  true/false
 */

bool
is_possible_monitor_by_hiddev_name(const char * hiddev_name) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "hiddev_name = %s", hiddev_name);

   Usb_Detailed_Device_Summary * devsum =  NULL;
   bool avoid = false;
   DBGTRC(debug, TRACE_GROUP, "Before lookup call");
   devsum = lookup_udev_usb_device_by_devname(hiddev_name, /* verbose = */ false);
   if (devsum) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "detailed_device_summary: ");
      if (debug || IS_TRACING()) {
         report_usb_detailed_device_summary(devsum, 2);
      }
      char * interfaces = devsum->prop_usb_interfaces;
      avoid = avoid_device_by_usb_interfaces_property_string(interfaces);
      free_usb_detailed_device_summary(devsum);
   }
   else {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Lookup failed");
      avoid = true;
   }

   // can only pass a variable, not an expression, to DBGTRC_RET_BOOL()
   // because failure simulation may assign a new value to the variable
   bool result = !avoid;
   DBGTRC_RET_BOOL(debug, TRACE_GROUP, result, "");
   return result;
}


Usb_Monitor_Info * create_usb_monitor_info(const char * hiddev_name) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "hiddev_name |%s|", hiddev_name);
   Usb_Monitor_Info * moninfo = calloc(1,sizeof(Usb_Monitor_Info));
   memcpy(moninfo->marker, USB_MONITOR_INFO_MARKER, 4);
   moninfo->hiddev_device_name = g_strdup(hiddev_name);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %p", moninfo);
   return moninfo;
}


static
void destroy_bus_open_error(gpointer p) {
   Bus_Open_Error * boe = p;
   free(boe->detail);
   free(boe);
}


//
// Probe HID devices, create USB_Mon_Info data structures
//

/**  Examines all hiddev devices to see if they are USB HID compliant monitors.
 *   If so, obtains the EDID, determines which reports to use for VCP feature
 *   values, etc.
 *
 *   @return:  array of pointers to USB_Mon_Info records
 *
 *   As a side effect, collects a GPtrArray of errors in global variable
 *   usb_open_errors.
 *
 *  The result is cached in global variables usb_monitors and usb_open_errors.
 */
GPtrArray *
get_usb_monitor_list() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   if (usb_monitors)      // already initialized?
   {
      DBGTRC_DONE(debug, TRACE_GROUP, "Returning previously calculated monitor list");
      return usb_monitors;
   }

   usb_monitors = g_ptr_array_new();
   usb_open_errors = g_ptr_array_new_with_free_func(destroy_bus_open_error);

   GPtrArray * hiddev_names = get_hiddev_device_names();
   for (int devname_ndx = 0; devname_ndx < hiddev_names->len; devname_ndx++) {
      char * hiddev_fn = g_ptr_array_index(hiddev_names, devname_ndx);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Examining device: %s", hiddev_fn);

      if (usb_is_ignored_hiddev(hiddev_name_to_number(hiddev_fn))) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Explicitly ignored: %s", hiddev_fn);
         continue;
      }

      // Ensures we don't touch a keyboard, mouse or some non-HID device.
      // Probing a keyboard or mouse can hang the system.
      if (!is_possible_monitor_by_hiddev_name(hiddev_fn)) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Not a possible monitor: %s", hiddev_fn);
         continue;
      }

      bool deny_checked = false;
      char * detail = NULL;
      Bus_Open_Error * boe = NULL;
      Usb_Detailed_Device_Summary * devsum =
            lookup_udev_usb_device_by_devname(hiddev_fn, /* verbose = */ false);
      if (devsum) {
         // report_usb_detailed_device_summary(devsum, 4);
         detail = g_strdup_printf("  USB bus %s, device %s, vid:pid: %s:%s - %s:%s",
                        devsum->busnum_s,
                        devsum->devnum_s,
                        devsum->vendor_id,
                        devsum->product_id,
                        devsum->vendor_name,
                        devsum->product_name);
         bool denied = deny_hid_monitor_by_vid_pid(devsum->vid, devsum->pid);
         denied |= usb_is_ignored_vid_pid(devsum->vid, devsum->pid);
         deny_checked = true;
         if (denied) {
            DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Denied monitor %s:%s", devsum->vendor_id, devsum->product_id);
         }

         free_usb_detailed_device_summary(devsum);
         if (denied)
            continue;
      }
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "detail = |%s|", detail);

      int fd = usb_open_hiddev_device(hiddev_fn, CALLOPT_RDONLY);
      if (fd < 0) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Open failed");
         f0printf(ferr(), "Open failed for %s: errno=%s %s\n", hiddev_fn, linux_errno_desc(-fd),
               (detail) ? detail : "");
         boe = calloc(1, sizeof(Bus_Open_Error));
         boe->io_mode = DDCA_IO_USB;
         boe->devno = hiddev_name_to_number(hiddev_fn);    // is this simple or fully qualified?
         boe->error = fd;
         boe->detail = detail;
         g_ptr_array_add(usb_open_errors, boe);
      }
      else {     // fd == 0 should never occur
         assert(fd != 0);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "open succeeded");
         free(detail);
         // Declare variables here and initialize them to NULL so that code at label close: works
         struct hiddev_devinfo *   devinfo     = NULL;
         char *                    cgname      = NULL;
         Parsed_Edid *             parsed_edid = NULL;
         GPtrArray *               vcp_reports = NULL;
         Usb_Monitor_Info *        moninfo =     NULL;

         cgname = get_hiddev_name(fd);               // HIDIOCGNAME

         devinfo = calloc(1,sizeof(struct hiddev_devinfo));
         Status_Errno rc2 = hiddev_get_device_info(fd, devinfo, CALLOPT_ERR_MSG);     //  HIDIOCGDEVINFO
         if (rc2 != 0) {
            DBGTRC_NOPREFIX(debug, TRACE_GROUP, "hiddev_get_device_info() failed. rc=%d", rc2);
            goto close;
         }

         if (!deny_checked) {
            bool deny = deny_hid_monitor_by_vid_pid(devinfo->vendor, devinfo->product);
            deny |= usb_is_ignored_vid_pid(devinfo->vendor, devinfo->product);
            if (deny) {
               DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Denied monitor 0x%04x:0x%04x", devinfo->vendor, devinfo->product);
               goto close;
            }
         }

         // DBGMSF(debug, "Calling is_hiddev_monitor()...");
         bool is_hid_monitor = is_hiddev_monitor(fd);           // HIDIOCGCOLLECTIONINFO
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "is_hiddev_monitor() returned %s", sbool(is_hid_monitor));
         if (!is_hid_monitor)
            goto close;

         // Solves problem of ddc detect not getting edid unless ddcutil env called first
         DBGMSF(debug, "calling ioctl(,HIDIOCINITREPORT)...");
         int rc = ioctl(fd, HIDIOCINITREPORT);
         DBGMSF(debug, "ioctl() returned %d", rc);
         if (rc < 0) {
            int errsv = errno;
            // call should never fail.  always write an error message
            REPORT_IOCTL_ERROR("HIDIOCINITGREPORT", errsv);
            goto close;
         }

         parsed_edid = get_hiddev_edid_with_fallback(fd, devinfo);
         if (!parsed_edid) {
            f0printf(ferr(),
                    "Monitor on device %s reports no EDID or has invalid EDID. Ignoring.\n",
                    hiddev_fn);
            goto close;
         }

         DBGTRC(debug, TRACE_GROUP, "Collecting USB reports...");
         vcp_reports = collect_vcp_reports(fd);   // HIDIOCGDEVINFO


         moninfo = create_usb_monitor_info(hiddev_fn);
         moninfo->edid = parsed_edid;
         moninfo->hiddev_devinfo = devinfo;
         devinfo = NULL;        // so that struct not freed

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
         if (debug) {
            DBGMSG("Added monitor:");
            dbgrpt_usb_monitor_info(moninfo, 3);
         }

 close:
         if (devinfo)
            free(devinfo);
         if (cgname)
            free(cgname);
         // TODO, free device summary
         usb_close_device(fd, hiddev_fn, CALLOPT_NONE); // return error if failure
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Closed");
      }  // monitor opened
   } // loop over device names

   g_ptr_array_set_free_func(hiddev_names, g_free);
   g_ptr_array_free(hiddev_names, true);

//   if ( debug || IS_TRACING() ) {
//      DBGTRC_DONE(debug, TRACE_GROUP, "Returning  %d monitors ", usb_monitors->len);
//      report_usb_monitors(usb_monitors,1);
//   }

   DBGTRC_RET_STRUCT(debug, TRACE_GROUP, "usb_monitors",report_usb_monitors, usb_monitors);
   return usb_monitors;
}


GPtrArray * get_usb_open_errors() {
   return usb_open_errors;
}


void
discard_usb_monitor_list() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "usb_monitors=%p, usb_open_errors=%p", usb_monitors, usb_open_errors);

   if (usb_monitors) {
      g_ptr_array_set_free_func(usb_monitors, free_usb_monitor_info);
      DBGMSF(debug, "Freeing usb_monitors = %p", usb_monitors);
      g_ptr_array_free(usb_monitors, true);
      usb_monitors = NULL;
      DBGMSF(debug, "Freeing usb_open_errors = %p", usb_open_errors);
      g_ptr_array_free(usb_open_errors, true);
      usb_open_errors = NULL;
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


//
// Functions to find Usb_Monitor_Info for a display
//

static Usb_Monitor_Info *
usb_find_monitor_by_busnum_devnum(int busnum, int devnum) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busnum=%d, devnum=%d", busnum, devnum);
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
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %p", result);
   return result;
}


static Usb_Monitor_Info *
usb_find_monitor_by_dref(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref = %s", dref_repr_t(dref));
   assert(dref->io_path.io_mode == DDCA_IO_USB);
   Usb_Monitor_Info * result = usb_find_monitor_by_busnum_devnum(dref->usb_bus, dref->usb_device);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %p", result);
   return result;
}


/** Gets the #Usb_Monitor_Info struct for a display
 *
 *  @param  dh  display handle
 *  @return pointer to #Usb_Monitor_Info struct, NULL if not found
 */
Usb_Monitor_Info *
usb_find_monitor_by_dh(Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh = %s", dh_repr(dh));
   assert(dh && dh->dref);
   assert(dh->dref->io_path.io_mode == DDCA_IO_USB);

   Usb_Monitor_Info * result =
         usb_find_monitor_by_busnum_devnum(dh->dref->usb_bus, dh->dref->usb_device);

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %p", result);
   return result;
}


#ifdef APPARENTLY_UNUSED
char * get_hiddev_devname_by_dref(Display_Ref * dref) {
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_dref(dref);
   char * result = moninfo->hiddev_device_name;
   DBGMSG("dref=%s, returning: %s", dref_short_name(dref), result);
   return result;
}
#endif



//
// Display_Info_list Functions
//

#ifdef UNUSED
/* Returns a list of all valid USB HID compliant monitors,
 * in a form expected by higher levels of ddcutil, namely
 * a collection of Display_Refs
 *
 * Arguments:    none
 *
 * Returns:      Display_Info_List of Display_Refs
 */
Display_Info_List usb_get_valid_displays() {
   // static GPtrArray * usb_monitors;    // array of Usb_Monitor_Info
   bool debug = false;
   GPtrArray * all_usb_monitors = get_usb_monitor_list();

   Display_Info_List info_list = {0,NULL};
   Display_Info info_recs[256];  // coverity flags uninitialized scalar

   DBGMSF(debug, "Found %d USB displays", __func__, usb_monitors->len);
   info_list.info_recs = calloc(usb_monitors->len,sizeof(Display_Info));
   for (int ndx=0; ndx<all_usb_monitors->len; ndx++) {
      Usb_Monitor_Info  * curmon = g_ptr_array_index(usb_monitors,ndx);
      Display_Ref * dref = create_usb_display_ref(
                              curmon->hiddev_devinfo->busnum,
                              curmon->hiddev_devinfo->devnum,
                              curmon->hiddev_device_name);
      info_recs[ndx].dispno = -1;    // not yet set
      info_recs[ndx].dref = dref;
      info_recs[ndx].edid = curmon->edid;
      memcpy(info_recs[ndx].marker, DISPLAY_INFO_MARKER, 4);
   }
   memcpy(info_list.info_recs, info_recs, (usb_monitors->len)*sizeof(Display_Info));
   info_list.ct = usb_monitors->len;

   if (debug) {
      DBGMSG("Done. Returning:");
      report_display_info_list(&info_list, 1);
   }

   return info_list;
}
#endif


//  *** Functions to return a Display_Ref for a USB monitor ***

#ifdef UNUSED
bool
usb_is_valid_display_ref(Display_Ref * dref, bool emit_error_msg) {
   bool result = true;
   if (!usb_find_monitor_by_dref(dref)) {
      result = false;
      if (emit_error_msg)
         fprintf(stderr, "Invalid Display_Ref\n");
   }
   return result;
}
#endif


/** Output of DETECT command for a USB connected monitor.
 *
 *  @param dref  display reference
 *  @param depth logical indentation depth
 */
void
usb_show_active_display_by_dref(Display_Ref * dref, int depth) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "dref = %s", dref_repr_t(dref) );
   DDCA_Output_Level output_level = get_output_level();
   rpt_vstring(depth, "USB bus:device:      %d:%d", dref->usb_bus, dref->usb_device);

   Usb_Monitor_Info * moninfo = usb_find_monitor_by_dref(dref);

   if (output_level == DDCA_OL_TERSE) {
      rpt_vstring(depth, "Monitor:             %s:%s:%s",
                         moninfo->edid->mfg_id,
                         moninfo->edid->model_name,
                         moninfo->edid->serial_ascii);
   }
   else {
      assert(output_level >= DDCA_OL_NORMAL);
      Pci_Usb_Id_Names usb_names = devid_get_usb_names(moninfo->hiddev_devinfo->vendor,
                                                       moninfo->hiddev_devinfo->product,
                                                       0, 2);
      char vname[80] = {'\0'};
      char dname[80] = {'\0'};
      if (usb_names.vendor_name)
         snprintf(vname, 80, "(%s)", usb_names.vendor_name);
      if (usb_names.device_name)
         snprintf(dname, 80, "(%s)", usb_names.device_name);

      rpt_vstring(depth, "Device name:         %s",    dref->usb_hiddev_name);
      rpt_vstring(depth, "Vendor id:           %04x  %s",
                         moninfo->hiddev_devinfo->vendor  & 0xffff, vname);
      rpt_vstring(depth, "Product id:          %04x  %s",
                         moninfo->hiddev_devinfo->product & 0xffff, dname);

      bool dump_edid = (output_level >= DDCA_OL_VERBOSE);
      report_parsed_edid(moninfo->edid, dump_edid /* verbose */, depth);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


//
// Get monitor information by Display_Ref or Display_Handle
// (for hiding Usb_Monitor_Info from higher software levels)
//

Parsed_Edid *
usb_get_parsed_edid_by_dref(Display_Ref * dref) {
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_dref(dref);
   return moninfo->edid;
}


Parsed_Edid *
usb_get_parsed_edid_by_dh(Display_Handle * dh) {
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_dh(dh);
   return moninfo->edid;
}


char *
usb_get_capabilities_string_by_dh(Display_Handle * dh) {
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_dh(dh);
   assert(dh);
   return usb_synthesize_capabilities_string(moninfo);
}


//
// *** Miscellaneous services ***
//

/** Tests if a hiddev device (specified by its name) appears to
 *  be a USB HID compliant monitor.
 *
 *  This stripped down test implements the ddcutil chkusbmon command,
 *  which is intended for use in a udev rules test.
 *
 *  @param  device_name   e.g. /dev/usb/hiddev3
 *  @retval true  device is a monitor,
 *  @retval false device is not a monitor, or unable to open device
 *
 * @remark
 * Note that messages will not appear when this function runs as part
 * of normal udev execution.  They are intended to aid in debugging.
 */
bool
check_usb_monitor( char * device_name ) {
   assert(device_name);
   bool debug = false;
   DDCA_Output_Level ol = get_output_level();
   if (debug)
      ol = DDCA_OL_VERBOSE;

   DBGMSF(debug, "Examining device: %s", device_name);
   bool result = false;

#ifdef OLD
   if (is_possible_monitor_by_hiddev_name(device_name))  {
      // only check if a HID device that isn't a mouse or keyboard
      int fd = open(device_name, O_RDONLY);
      if (fd < 1) {
         if (ol >= DDCA_OL_VERBOSE)
            printf("Unable to open device %s: %s\n", device_name, strerror(errno));
      }
      else {
         result = is_hiddev_monitor(fd);
      }
      close(fd);
   }
#endif
   result = is_possible_monitor_by_hiddev_name(device_name);

   if (ol >= DDCA_OL_VERBOSE) {
      if (result)
         printf("Device %s may be a USB HID compliant monitor.\n", device_name);
      else
         printf("Device %s is not a USB HID compliant monitor.\n", device_name);
   }
   return result;
 }


void
init_usb_displays() {
   RTTI_ADD_FUNC(avoid_device_by_usb_interfaces_property_string);
   RTTI_ADD_FUNC(collect_vcp_reports);
   RTTI_ADD_FUNC(create_usb_monitor_info);
   RTTI_ADD_FUNC(create_usb_monitor_vcp_rec);
   RTTI_ADD_FUNC(discard_usb_monitor_list);
   RTTI_ADD_FUNC(free_usb_monitor_info);
   RTTI_ADD_FUNC(free_usb_monitor_vcp_rec);
   RTTI_ADD_FUNC(get_usb_monitor_list);
   RTTI_ADD_FUNC(is_possible_monitor_by_hiddev_name);
   RTTI_ADD_FUNC(usb_find_monitor_by_busnum_devnum);
   RTTI_ADD_FUNC(usb_find_monitor_by_dh);
   RTTI_ADD_FUNC(usb_find_monitor_by_dref);
   RTTI_ADD_FUNC(usb_show_active_display_by_dref);
}


void
terminate_usb_displays() {
    // discard_usb_monitor_list();   // unnecessary, already called
}


