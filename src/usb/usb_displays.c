/* usb_displays.c
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <linux/hiddev.h>
#include <sys/ioctl.h>
#include <unistd.h>
/** \endcond */

#include "util/device_id_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/udev_util.h"

#include "usb_util/hiddev_reports.h"
#include "usb_util/hiddev_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/execution_stats.h"
#include "base/linux_errno.h"

#include "usb/usb_base.h"
#include "usb/usb_edid.h"

#include "usb/usb_displays.h"


// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_USB;  // may be unused if all diagnostics turned off

// n. #pragma GCC diagnostic ignored "-Wunused-variable" not working
void usb_core_unused_function_to_avoid_unused_variable_warning() {
   printf("0x%02x\n",TRACE_GROUP);
}

// // Forward declarations
// static GPtrArray * get_usb_monitor_list();  // returns array of Usb_Monitor_Info

// Global variables
static GPtrArray * usb_monitors = NULL;    // array of Usb_Monitor_Info


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

/* Reports contents of usb_monitor_vcp_rec struct
 *
 * Arguments:
 *   vcprec
 *   depth
 *
 * Returns:   nothing
 */
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
void report_usb_monitor_info(Usb_Monitor_Info * moninfo, int depth) {
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


/* Reports on an array of Usb_Monitor_info structs
 *
 * Arguments:
 *   monitors    pointer to GPtrArray of pointer to struct Usb_Monitor_Info
 *   depth       logical indentation depth
 *
 * Returns:      nothing
 */
// static
void report_usb_monitors(GPtrArray * monitors, int depth) {
   const int d1 = depth+1;

   rpt_vstring(depth, "GPtrArray of %d Usb_Monitor_Info at %p", monitors->len, monitors);
   for (int ndx = 0; ndx < monitors->len; ndx++) {
      report_usb_monitor_info( g_ptr_array_index(monitors, ndx), d1);
   }
}


//
// HID Report Inquiry
//

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
          reportinfo_rc = hiddev_get_report_info(fd, &rinfo, CALLOPT_ERR_MSG | CALLOPT_ERR_ABORT);
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
             Byte callopts = CALLOPT_ERR_MSG | CALLOPT_ERR_ABORT;
             if (debug)
                callopts |= CALLOPT_WARN_FINDEX;
             hiddev_get_field_info(fd, &finfo, callopts);
             if (finfo.application != 0x00800001) // USB Monitor Page/Monitor Control
                continue;

             for (undx = 0; undx < finfo.maxusage; undx++) {
                struct hiddev_usage_ref uref = {
                       .report_type = rinfo.report_type,   // rinfo.report_type;
                       .report_id =   rinfo.report_id,     // rinfo.report_id;
                       .field_index = fndx,
                       .usage_index = undx
                };
                hiddev_get_usage_code(fd, &uref, CALLOPT_ERR_MSG|CALLOPT_ERR_ABORT);
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


//
// Capabilities
//

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



//
// Probe HID devices, create USB_Mon_Info data stuctures
//

/*  Examines all hiddev devices to see if they are USB HID compliant monitors.
 *  If so, obtains the EDID, determines which reports to use for VCP feature
 *  values, etc.
 *
 *  Returns:   array of pointers to USB_Mon_Info records
 *
 *  The result is cached in global variable usb_monitors
 */
GPtrArray * get_usb_monitor_list() {
   bool debug = false;
   DBGMSF(debug, "Starting...");
   DDCA_Output_Level ol = get_output_level();

   if (usb_monitors)      // already initialized?
      return usb_monitors;

   usb_monitors = g_ptr_array_new();

   GPtrArray * hiddev_names = get_hiddev_device_names();
   for (int devname_ndx = 0; devname_ndx < hiddev_names->len; devname_ndx++) {
      char * hiddev_fn = g_ptr_array_index(hiddev_names, devname_ndx);
      DBGMSF(debug, "Examining device: %s", hiddev_fn);
      // will need better message handling for API
      Byte calloptions = CALLOPT_RDONLY;
      if (ol >= DDCA_OL_VERBOSE)
         calloptions |= CALLOPT_ERR_MSG;
      int fd = usb_open_hiddev_device(hiddev_fn, calloptions);
      if (fd < 0 && ol >= DDCA_OL_VERBOSE) {
         Usb_Detailed_Device_Summary * devsum =
         lookup_udev_usb_device_by_devname(hiddev_fn);
         // report_usb_detailed_device_summary(devsum, 2);
         f0printf(FERR, "  USB bus %s, device %s, vid:pid: %s:%s - %s:%s\n",
                        devsum->busnum_s,
                        devsum->devnum_s,
                        devsum->vendor_id,
                        devsum->product_id,
                        devsum->vendor_name,
                        devsum->product_name);
         free_usb_detailed_device_summary(devsum);
      }
      else if (fd > 1) {     // fd == 0 should never occur
         // Declare variables here and initialize them to NULL so that code at label close: works
         struct hiddev_devinfo *   devinfo     = NULL;
         char *                    cgname      = NULL;
         Parsed_Edid *             parsed_edid = NULL;
         GPtrArray *               vcp_reports = NULL;
         Usb_Monitor_Info *        moninfo =     NULL;

         cgname = get_hiddev_name(fd);               // HIDIOCGNAME
         devinfo = calloc(1,sizeof(struct hiddev_devinfo));
         if ( hiddev_get_device_info(fd, devinfo, CALLOPT_ERR_MSG) != 0 )
            goto close;
         if (!is_hiddev_monitor(fd))
            goto close;

         parsed_edid = get_hiddev_edid_with_fallback(fd, devinfo);
         if (!parsed_edid) {
            f0printf(FERR,
                    "Monitor on device %s reports no EDID or has invalid EDID. Ignoring.\n",
                    hiddev_fn);
            goto close;
         }

         vcp_reports = collect_vcp_reports(fd);

         moninfo = calloc(1,sizeof(Usb_Monitor_Info));
         memcpy(moninfo->marker, USB_MONITOR_INFO_MARKER, 4);
         moninfo-> hiddev_device_name = strdup(hiddev_fn);
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

 close:
         if (devinfo)
            free(devinfo);
         if (cgname)
            free(cgname);
         usb_close_device(fd, hiddev_fn, CALLOPT_NONE); // return error if failure
      }  // monitor opened
   } // loop over device names

   g_ptr_array_set_free_func(hiddev_names, free);
   g_ptr_array_free(hiddev_names, true);

   if (debug) {
      DBGMSG("Returning  %d monitors ", usb_monitors->len);
      // report_usb_monitors(usb_monitors,1);
   }

   return usb_monitors;
}



//
// Functions to find Usb_Monitor_Info for a display
//

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
   DBGMSF(debug, "Starting. dref = %s", dref_repr(dref));
   assert(dref->io_mode == DDCA_IO_USB);
   Usb_Monitor_Info * result = usb_find_monitor_by_busnum_devnum(dref->usb_bus, dref->usb_device);
   DBGMSF(debug, "Returning %p", result);
   return result;
}


Usb_Monitor_Info * usb_find_monitor_by_display_handle(Display_Handle * dh) {
   // printf("(%s) Starting. dh=%p\n", __func__, dh);
   bool debug = false;
   DBGMSF(debug, "Starting. dh = %s", display_handle_repr(dh));
   assert(dh->io_mode == DDCA_IO_USB);
   Usb_Monitor_Info * result = NULL;
   result = usb_find_monitor_by_busnum_devnum(dh->dref->usb_bus, dh->dref->usb_device);
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



//
// Display_Info_list Functions
//


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



//  *** Functions to return a Display_Ref for a USB monitor ***

#ifdef PRE_DISPLAY_REF

static Display_Ref *
create_display_ref_from_usb_monitor_info(Usb_Monitor_Info * moninfo) {
   // hacky - to be cleaned up
   Display_Ref * dref = create_usb_display_ref(moninfo->hiddev_devinfo->busnum,
                                               moninfo->hiddev_devinfo->devnum,
                                               moninfo->hiddev_device_name);
   return dref;
}


Display_Ref *
usb_find_display_by_mfg_model_sn(const char * mfg_id, const char * model, const char * sn) {
   Display_Ref * result = NULL;

   Usb_Monitor_Info * found_monitor = NULL;
   GPtrArray * all_usb_monitors = get_usb_monitor_list();
   for (int ndx=0; ndx<all_usb_monitors->len; ndx++) {
      Usb_Monitor_Info * curmon = g_ptr_array_index(all_usb_monitors, ndx);
      bool some_test_passed = false;
      bool some_test_failed = false;

      if (mfg_id && strlen(mfg_id) > 0) {
         if ( streq(mfg_id, curmon->edid->mfg_id) )
            some_test_passed = true;
         else
            some_test_failed = false;
      }

      if (model && strlen(model) > 0) {
         if ( streq(model, curmon->edid->model_name) )
            some_test_passed = true;
         else
            some_test_failed = false;
      }

      if (sn && strlen(sn) > 0) {
         if ( streq(sn, curmon->edid->serial_ascii) )
            some_test_passed = true;
         else
            some_test_failed = false;
      }

      // if ( strcmp(model, curmon->edid->model_name)   == 0 &&
      //      strcmp(sn,    curmon->edid->serial_ascii) == 0
      //    )
      if (some_test_passed && !some_test_failed)
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
#endif


bool usb_is_valid_display_ref(Display_Ref * dref, bool emit_error_msg) {
   bool result = true;
   if (!usb_find_monitor_by_display_ref(dref)) {
      result = false;
      if (emit_error_msg)
         fprintf(stderr, "Invalid Display_Ref\n");
   }
   return result;
}


void usb_show_active_display_by_display_ref(Display_Ref * dref, int depth) {
   DDCA_Output_Level output_level = get_output_level();
   rpt_vstring(depth, "USB bus:device:      %d:%d", dref->usb_bus, dref->usb_device);

   Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_ref(dref);

#ifdef OLD
   if (output_level == DDCA_OL_TERSE || output_level == OL_PROGRAM)
#else
   if (output_level == DDCA_OL_TERSE)
#endif
      rpt_vstring(depth, "Monitor:             %s:%s:%s",
                         moninfo->edid->mfg_id,
                         moninfo->edid->model_name,
                         moninfo->edid->serial_ascii);
   Pci_Usb_Id_Names usb_names =  devid_get_usb_names(moninfo->hiddev_devinfo->vendor,
                                                     moninfo->hiddev_devinfo->product,
                                                     0,
                                                     2);

   char vname[80] = {'\0'};
   char dname[80] = {'\0'};
   if (usb_names.vendor_name)
      snprintf(vname, 80, "(%s)", usb_names.vendor_name);
   if (usb_names.device_name)
      snprintf(dname, 80, "(%s)", usb_names.device_name);
   if (output_level >= DDCA_OL_NORMAL) {
      rpt_vstring(depth, "Device name:         %s",    dref->usb_hiddev_name);
      rpt_vstring(depth, "Vendor id:           %04x  %s",
                         moninfo->hiddev_devinfo->vendor  & 0xffff, vname);
      rpt_vstring(depth, "Product id:          %04x  %s",
                         moninfo->hiddev_devinfo->product & 0xffff, dname);
      bool dump_edid = (output_level >= DDCA_OL_VERBOSE);

      report_parsed_edid(moninfo->edid, dump_edid /* verbose */, depth);
   }
}


//
// Get monitor information by Display_Ref or Display_Handle
// (for hiding Usb_Monitor_Info from higher software levels)
//

Parsed_Edid * usb_get_parsed_edid_by_display_ref(Display_Ref * dref) {
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_ref(dref);
   return moninfo->edid;
}

Parsed_Edid * usb_get_parsed_edid_by_display_handle(Display_Handle * dh) {
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_handle(dh);
   return moninfo->edid;
}


char * usb_get_capabilities_string_by_display_handle(Display_Handle * dh) {
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_handle(dh);
   assert(dh);
   return usb_synthesize_capabilities_string(moninfo);
}


//
// *** Miscellaneous services ***
//

/* Tests if a hiddev device (specified by its name) appears to
 * be a USB HID compliant monitor.
 *
 * This stripped down test implements the ddcutil chkusbmon command,
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
   DDCA_Output_Level ol = get_output_level();
   if (debug)
      ol = DDCA_OL_VERBOSE;

   DBGMSF(debug, "Examining device: %s", device_name);
   bool result = false;

   int fd = open(device_name, O_RDONLY);
   if (fd < 1) {
      if (ol >= DDCA_OL_VERBOSE)
         printf("Unable to open device %s: %s\n", device_name, strerror(errno));
      goto exit;
   }

   result = is_hiddev_monitor(fd);

   close(fd);

   if (ol >= DDCA_OL_VERBOSE) {
      if (result)
         printf("Device %s appears to be a USB HID compliant monitor.\n", device_name);
      else
         printf("Device %s is not a USB HID compliant monitor.\n", device_name);
   }

 exit:
    return result;
 }
