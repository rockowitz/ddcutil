/* hiddev_reports.c
 *
 * The functions in this file report the contents of hiddev data structures.
 * They are used for debugging, exploratory programming, and in the
 * ddcutil interrogate command.
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

#include <assert.h>
#include <errno.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wchar.h>

#include "util/device_id_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "usb_util/hiddev_util.h"    // numerous functions and macro definitions

#include "usb_util/hiddev_reports.h"


/* Wrap ioctl(HIDIOCGSTRING) to retrieve a string.
 *
 * It is the responsibility of the caller to free the returned string.
 *
 *  Arguments:   fd
 *               index  string index number
 *
 *  Returns:     string, NULL if invalid index number
 *
 *  Notes:
  */
char * get_hiddev_string(int fd, __s32 index) {
   struct hiddev_string_descriptor desc;
   desc.index = index;
   // strcpy(desc.value,"Unset");   // for debugging Apple display issue
   // Returns string length if found, -1 if not
   // Apple Cinema display never returns -1, always seems to be last valid value
   errno = 0;
   // Very slow call on Apple Cinema display
   int rc = ioctl(fd, HIDIOCGSTRING, &desc);
   // if (rc != 0)
   //     REPORT_IOCTL_ERROR("HIDIOCGSTRING", rc);
   char * result = NULL;
   if (rc > 0)
      result = strdup(desc.value);
   return result;
}


/* Reports all defined strings.
 *
 * Arguments:
 *    fd       file descriptor
 *    max_ct   maximum number of strings to report
 *    depth    logical indentation depth
 *
 * Returns:  nothing
 *
 * Note: Parm max_ct exists because at least the Apple Cinema display
 *       does not report that a string index is out of range, it just
 *       reports the last valid value.
 */
void report_hiddev_strings(int fd, int max_ct, int depth) {
   rpt_title("Device strings returned by ioctl(HIDIOCGSTRING):", depth);
   int d1 = depth+1;
   int string_index = 1;
   char * string_value = NULL;
   for (; (string_value = get_hiddev_string(fd, string_index)); string_index++) {
   // for (; string_index < 10; string_index++) {
      // string_value = get_hiddev_string(fd, string_index);
      if (max_ct >= 0 && string_index > max_ct) {
         free(string_value);
         break;
      }
      rpt_vstring(d1, "String index: %d, value = |%s|", string_index, string_value);
      free(string_value);
   }
}


/* Outputs debug report for struct hiddev_devinfo
 *
 * Arguments:
 *    dinfo         pointer to struct hiddev_devinfo
 *    lookup_names  if true, ids for usb vendor and product ids are looked up
 *    depth         logical indentation depth
 *
 * Returns:  nothing
 */
void report_hiddev_devinfo(struct hiddev_devinfo * dinfo, bool lookup_names, int depth) {
   int d1 = depth+1;

   Pci_Usb_Id_Names names = {"","",""};
   if (lookup_names) {
      devid_get_usb_names(
                dinfo->vendor,
                dinfo->product,
                0,
                2);
   }
   rpt_structure_loc("hiddev_devinfo", dinfo, depth);
   // bus types are defined in <linux/input.h>.  No need to define a lookup table of types
   // to names, since the bus type is always USB
   rpt_vstring(d1,"%-20s: %u  %s",     "bustype", dinfo->bustype, (dinfo->bustype == 3) ? "BUS_USB" : "");
   rpt_vstring(d1,"%-20s: %u",         "busnum",  dinfo->busnum);
   rpt_vstring(d1,"%-20s: %u",         "devnum",  dinfo->devnum);
   rpt_vstring(d1,"%-20s: %u",         "ifnum",   dinfo->ifnum);
   rpt_vstring(d1,"%-20s: 0x%04x  %s", "vendor", dinfo->vendor, names.vendor_name);
   // strip high bytes?   dinfo->product & 0x0000ffff
   // dinfo->product and dinfo->vendor are __s16, before conversion to hex string, they're
   // promoted to int, in which case the sign bit is extended,
   rpt_vstring(d1,"%-20s: 0x%04x  %s", "product", dinfo->product & 0xffff, names.device_name);
   rpt_vstring(d1,"%-20s: %2x.%02x",   "version", dinfo->version>>8, dinfo->version & 0x0f);  // BCD
   rpt_vstring(d1,"%-20s: %u",         "num_applications", dinfo->num_applications);
}


char * interpret_collection_type(__u32 type) {
   char * result = NULL;

   // Per USB HID spec section 6.2.2.4 Main Items
   // and section 6.2.2.6 Collection, End Collection Items
   switch(type) {
   case(0x00):  result = "Physical";       break;
   case(0x01):  result = "Application";    break;
   case(0x02):  result = "Logical";        break;
   case(0x03):  result = "Report";         break;
   case(0x04):  result = "Named Array";    break;
   case(0x05):  result = "Usage Switch";   break;
   case(0x06):  result = "Usage Modifier"; break;
   default:
      if (type >= 0x80 && type <= 0xff)
                result = "Vendor-defined";
      else
                result = "Reserved";        // should never occur
   } //switch
   return result;
}


/* Outputs debug report for struct hiddev_collection info
 *
 * Arguments:
 *    cinfo         pointer to struct hiddev_collecion_info
 *    depth         logical indentation depth
 *
 * Returns:  nothing
 */
void report_hiddev_collection_info(struct hiddev_collection_info * cinfo, int depth) {
   int d1 = depth+1;

   rpt_structure_loc("hiddev_collection_info", cinfo, depth);
   rpt_vstring(d1, "%-20s: %u",        "index", cinfo->index);
   rpt_vstring(d1, "%-20s: %u  %s",    "type",  cinfo->type, interpret_collection_type(cinfo->type));
   rpt_vstring(d1, "%-20s: 0x%08x %s", "usage", cinfo->usage, hiddev_interpret_usage_code(cinfo->usage));
   rpt_vstring(d1, "%-20s: %u",        "level", cinfo->level);
}


/* Outputs debug report for struct hiddev_string_descriptor
 *
 * Arguments:
 *    desc          pointer to struct hiddev_string_descriptor
 *    depth         logical indentation depth
 *
 * Returns:  nothing
 */
void report_hiddev_string_descriptor(struct hiddev_string_descriptor * desc, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("hiddev_string_descriptor", desc, depth);
   rpt_vstring(d1, "%-20s: %d",   "index", desc->index);
   rpt_vstring(d1, "%-20s: |%s|", "value", desc->value);
}


#ifdef REF
#define HID_FIELD_CONSTANT    0x001
#define HID_FIELD_VARIABLE    0x002
#define HID_FIELD_RELATIVE    0x004
#define HID_FIELD_WRAP        0x008
#define HID_FIELD_NONLINEAR      0x010
#define HID_FIELD_NO_PREFERRED      0x020
#define HID_FIELD_NULL_STATE     0x040
#define HID_FIELD_VOLATILE    0x080
#define HID_FIELD_BUFFERED_BYTE     0x100
#endif


/* Produces a string representation of the HID field flag bits
 *
 * Arguments: flags    word of flags
 *
 * Returns:  String representation of flags.
 *
 * The value is built in an internal buffer and is valid
 * until the next call of this function.
 */
char * interpret_field_bits(__u32 flags) {

#define FLAG_BIT(_bitname) \
   if (flags & _bitname) \
      curpos += sprintf(curpos, #_bitname "|")

   static char field_bits_buffer[200];

   field_bits_buffer[0] = '\0';
   char * curpos = field_bits_buffer;
   FLAG_BIT(HID_FIELD_CONSTANT);
   FLAG_BIT(HID_FIELD_VARIABLE);
   FLAG_BIT(HID_FIELD_RELATIVE);
   FLAG_BIT(HID_FIELD_WRAP);
   FLAG_BIT(HID_FIELD_NONLINEAR);
   FLAG_BIT(HID_FIELD_NO_PREFERRED);
   FLAG_BIT(HID_FIELD_NULL_STATE);
   FLAG_BIT(HID_FIELD_VOLATILE);
   FLAG_BIT(HID_FIELD_BUFFERED_BYTE);
   assert( (curpos-field_bits_buffer) < sizeof(field_bits_buffer) );
   if (curpos != field_bits_buffer)
      *(curpos-1) = '\0';
   return field_bits_buffer;

#undef FLAG_BIT
}


/* Outputs debug report for struct hiddev_report_info
 *
 * Arguments:
 *    desc          pointer to struct hiddev_report_info
 *    depth         logical indentation depth
 *
 * Returns:  nothing
 */
void report_hiddev_report_info(struct hiddev_report_info * rinfo, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("hiddev_report_info", rinfo, depth);
   rpt_vstring(d1, "%-20s: %u %s", "report_type", rinfo->report_type,
                                                  report_type_name(rinfo->report_type));
   rpt_vstring(d1, "%-20s: %s  0x%08x", "report_id", hiddev_interpret_report_id(rinfo->report_id), rinfo->report_id);   // may have next flag set in high order bit
   rpt_vstring(d1, "%-20s: %u", "num_fields", rinfo->num_fields);
}


#ifdef REF
#define HID_REPORT_ID_UNKNOWN 0xffffffff
#define HID_REPORT_ID_FIRST   0x00000100
#define HID_REPORT_ID_NEXT    0x00000200
#define HID_REPORT_ID_MASK    0x000000ff
#define HID_REPORT_ID_MAX     0x000000ff
#endif


/* Returns a string representation of a report id value
 *
 * Arguments:  report_id
 *
 * Returns:  string representation of id
 *
 * The value is built in an internal buffer and is valid
 * until the next call of this function.
 */
char * hiddev_interpret_report_id(__u32 report_id) {
   static char report_id_buffer[100];
   report_id_buffer[0] = '\0';
   if (report_id == HID_REPORT_ID_UNKNOWN)
      strcpy(report_id_buffer, "HID_REPORT_ID_UNKNOWN");
   else {
      if (report_id & HID_REPORT_ID_FIRST)
         strcpy(report_id_buffer, "HID_REPORT_ID_FIRST|");
      if (report_id & HID_REPORT_ID_NEXT)
         strcat(report_id_buffer, "HID_REPORT_ID_NEXT|");
      sprintf(report_id_buffer + strlen(report_id_buffer),
              "%u",
              report_id & HID_REPORT_ID_MASK);
   }
   return report_id_buffer;
}


/* Returns a string representation of a HID usage code
 *
 * Arguments:  usage_code
 *
 * Returns:    string representation of usage code
 *
 * The value is built in an internal buffer and is valid
 * until the next call of this function.
 */
char * hiddev_interpret_usage_code(int usage_code ) {
   static char usage_buffer[100];
   usage_buffer[0] = '\0';
   if (usage_code == 0) {
       // sprintf(usage_buffer, "0x%08x", usage_code);
      usage_buffer[0] = '\0';
   }
   else {
      unsigned short usage_page = usage_code >> 16;
      unsigned short usage_id   = usage_code & 0xffff;
      char * page_name;
      char * page_value_name;
      if (usage_page >= 0xff00) {
         page_name = "Manufacturer";
         page_value_name = "";
      }
      else {
         page_name = devid_usage_code_page_name(usage_page);
         if (!page_name) {
            page_name = "";
            page_value_name = "";
         }
         else {
            page_value_name = devid_usage_code_id_name(usage_page, usage_id);
            if (!page_value_name)
               page_value_name = "";
         }
      }

      snprintf(usage_buffer, sizeof(usage_buffer),
               // "0x%08x page=0x%04x (%s), id=0x%04x (%s)",
               // usage_code,
               "page=0x%04x (%s), id=0x%04x (%s)",
               usage_page,
               page_name,
               usage_id,
               page_value_name);
   }

   return usage_buffer;
}


/* Outputs debug report for struct hiddev_field_info
 *
 * Arguments:
 *    desc          pointer to struct hiddev_field_info
 *    depth         logical indentation depth
 *
 * Returns:  nothing
 */
void report_hiddev_field_info(struct hiddev_field_info * finfo, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("hiddev_field_info", finfo, depth);
   rpt_vstring(d1, "%-20s: %u %s", "report_type", finfo->report_type,
                                                  report_type_name(finfo->report_type));
   // const char * s0 = names_reporttag(finfo->report_id);
   // const char * s1 = names_huts(finfo->physical>>16);
   // const char * s2 = names_hutus(finfo->physical);
   rpt_vstring(d1, "%-20s: %s (0x%08x)", "report_id",
                   hiddev_interpret_report_id(finfo->report_id), finfo->report_id);
   rpt_vstring(d1, "%-20s: %u",     "field_index", finfo->field_index);
   rpt_vstring(d1, "%-20s: %u",     "maxusage",    finfo->maxusage);
   rpt_vstring(d1, "%-20s: 0x%08x  %s", "flags",
                   finfo->flags, interpret_field_bits(finfo->flags) );
   // rpt_vstring(d1, "%-20s: %u 0x%08x huts=|%s|, hutus=|%s| (physical usage for this field)", "physical",
   //                 finfo->physical, finfo->physical, s1, s2);
   rpt_vstring(d1, "%-20s: 0x%08x  %s",     "physical (usage)",
                   finfo->physical, hiddev_interpret_usage_code(finfo->physical) );
   rpt_vstring(d1, "%-20s: 0x%08x  %s",     "logical (usage)",
                   finfo->logical,  hiddev_interpret_usage_code(finfo->logical) );
   rpt_vstring(d1, "%-20s: 0x%08x  %s",     "application (usage)",
                   finfo->application, hiddev_interpret_usage_code(finfo->application) );
   rpt_vstring(d1, "%-20s: %d",     "logical_minimum",  finfo->logical_minimum);
   rpt_vstring(d1, "%-20s: %d",     "logical_maximum",  finfo->logical_maximum);
   rpt_vstring(d1, "%-20s: %d",     "physical_minimum", finfo->physical_minimum);
   rpt_vstring(d1, "%-20s: %d",     "physical_maximum", finfo->physical_maximum);
   rpt_vstring(d1, "%-20s: %u",     "unit_exponent",    finfo->unit_exponent);
   rpt_vstring(d1, "%-20s: 0x%08x", "unit",             finfo->unit);
}


/* Outputs debug report for struct hiddev_usage_ref
 *
 * Arguments:
 *    desc          pointer to struct hiddev_usage_ref
 *    depth         logical indentation depth
 *
 * Returns:  nothing
 */
void report_hiddev_usage_ref(struct hiddev_usage_ref * uref, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("hiddev_usage_ref", uref, depth);
   rpt_vstring(d1, "%-20s: %u %s", "report_type", uref->report_type,
                                                  report_type_name(uref->report_type));
   // const char * s0 = names_reporttag(uref->report_id);
   // char * s1 = names_huts(uref->usage_code>>16);
   // char * s2 = names_hutus(uref->usage_code);
   rpt_vstring(d1, "%-20s: %u  %s",     "report_id",
               uref->report_id, hiddev_interpret_report_id(uref->report_id));
   rpt_vstring(d1, "%-20s: %u",     "field_index",  uref->field_index);
   rpt_vstring(d1, "%-20s: %u",     "usage_index",  uref->usage_index);
   // rpt_vstring(d1, "%-20s: 0x%08x huts=|%s|, hutus=|%s|", "usage_code",   uref->usage_code, s1, s2);
   // rpt_usage_code("usage_code", NULL, uref->usage_code, d1);
   rpt_vstring(d1, "%-20s: 0x%08x  %s", "usage_code", uref->usage_code,
                   hiddev_interpret_usage_code(uref->usage_code) );
   rpt_vstring(d1, "%-20s: %d", "value", uref->value);
}


void report_hiddev_usage_ref_multi(struct hiddev_usage_ref_multi * uref_multi, int depth) {
   int d1 = depth+1;
   // int d2 = depth+2;
   rpt_structure_loc("hiddev_usage_ref_multi", uref_multi, depth);
   report_hiddev_usage_ref(&uref_multi->uref, d1);
   rpt_vstring(d1, "%-20s: %d", "num_values", uref_multi->num_values);
   rpt_vstring(d1, "%-20s at %p", "values", &uref_multi->values);
   // rpt_hex_dump(&uref_multi->values, uref_multi->num_values, d2);
}


/** Reports a usage code for a field, based on the index, and also optionally the
 *  usage value of the field.
 *
 */
void report_field_usage(
        int  fd,
        int  report_type,
        int  report_id,
        int  field_index,
        int  usage_index,
        bool show_value,
        int  depth)
{
   int d0 = depth;
   int d1 = depth+1;
   int rc;
   // printf("(%s) field index = %d, usage index=%d\n", __func__, i, j);
   struct hiddev_usage_ref uref = {0};  // initialize to make valgrind happy
   uref.report_type = report_type;   // rinfo.report_type;
   uref.report_id   = report_id;     // rinfo.report_id;
   uref.field_index = field_index;   // i;
   uref.usage_index = usage_index;   //  j;
   rpt_vstring(d0, "report_id: %d, field_index: %d, usage_index: %d",
                     uref.report_id, uref.field_index, uref.usage_index);
   errno = 0;
   rc = ioctl(fd, HIDIOCGUCODE, &uref);    // Fills in usage code
   if (rc != 0)
      REPORT_IOCTL_ERROR("HIDIOCGUCODE", rc);
   // assert(rc == 0);
   if (rc == 0) {
     rpt_vstring(d1, "Usage code = 0x%08x  %s",
                     uref.usage_code, hiddev_interpret_usage_code(uref.usage_code));
     int collection_index = ioctl(fd, HIDIOCGCOLLECTIONINDEX, &uref);
     rpt_vstring(d1, "Collection index for usage code: %d", collection_index);

     if (show_value) {
        // Gets the current value of the field
        rc = ioctl(fd, HIDIOCGUSAGE, &uref);  // Fills in usage value
        if (rc != 0)
           REPORT_IOCTL_ERROR("HIDIOCGUSAGE", rc);
        // occasionally see -1, errno = 22 invalid argument - for Battery System Page: Run Time to Empty
        if (rc == 0)
           rpt_vstring(d1, "Current value (value) = %d (0x%08x)", uref.value, uref.value);
        else
           rpt_vstring(d1, "Error getting current value");
     }
   }
}


/* Reports all report descriptors of a particular type for an open HID device.
 *
 * Arguments:
 *   fd           file descriptor for open hiddev device
 *   report_type  HID_REPORT_TYPE_INPUT, HID_REPORT_TYPE_OUTPUT, or HID_REPORT_TYPE_FEATURE
 *   depth        logical indentation depth
 *
 * Returns:    nothing
 */
void report_report_descriptors_for_report_type(int fd, __u32 report_type, int depth) {
   int ret;
   const int d0 = depth;
   const int d1 = d0 + 1;
   const int d2 = d0 + 2;
   const int d3 = d0 + 3;
   const int d4 = d0 + 4;

   struct hiddev_report_info rinfo = {0};  // initialize to make valgrind happy

   rinfo.report_type = report_type;
   rinfo.report_id   = HID_REPORT_ID_FIRST;

   puts("");
   rpt_vstring(d0, "Getting descriptors for report_type=%s", report_type_name(report_type));

   ret = ioctl(fd, HIDIOCGREPORTINFO, &rinfo);
   if (ret != 0) {    // no more reports
      if (ret != -1)
         REPORT_IOCTL_ERROR("HIDIOCGREPORTINFO", ret);
      rpt_vstring(d1, "No reports defined");
      return;
   }

   int rptct = 0;    // count of reports seen, for our local interest
   while (ret >= 0) {
      // printf("(%s) Report counter %d, report_id = 0x%08x %s\n",
      //       __func__, rptct, rinfo.report_id, interpret_report_id(rinfo.report_id));
      puts("");
      rpt_vstring(d0, "Report %s:", hiddev_interpret_report_id(rinfo.report_id));
      report_hiddev_report_info(&rinfo, d1);
      rptct++;

      if (rinfo.report_type != HID_REPORT_TYPE_OUTPUT) {
         // So that usage value filled in
         int rc = ioctl(fd, HIDIOCGREPORT, &rinfo);
         if (rc != 0) {
            REPORT_IOCTL_ERROR("HIDIOCGREPORT", rc);
            printf("(%s) Unable to get report %d\n", __func__, rinfo.report_id);
            break;
         }
      }

      int fndx, undx;
      if (rinfo.num_fields > 0)
         rpt_vstring(d1, "Scanning fields of report %s", hiddev_interpret_report_id(rinfo.report_id));
      for (fndx = 0; fndx < rinfo.num_fields; fndx++) {
         // printf("(%s) field index = %d\n", __func__, i);
         bool edidfg = is_field_edid(fd, &rinfo, fndx);
         if (edidfg) {
            rpt_vstring(d2, "Report id: %d, Field index: %d contains EDID:",
                            rinfo.report_id, fndx);
         }

         struct hiddev_field_info finfo = {0};
         memset(&finfo, 0, sizeof(finfo));
         finfo.report_type = rinfo.report_type;
         finfo.report_id   = rinfo.report_id;
         finfo.field_index = fndx;
         rpt_vstring(d2, "Report id: %d, Field index %d:", finfo.report_id, fndx);
         int rc = ioctl(fd, HIDIOCGFIELDINFO, &finfo);
         if (rc != 0) {   // should never occur
            REPORT_IOCTL_ERROR("HIDIOCGFIELDINFO", rc);
            break;        // just stop checking fields
         }

         rpt_vstring(d2, "Description of field %d:", fndx);
         if (finfo.field_index != fndx) {
            rpt_vstring(d3, "!! Note that HIDIOCGFIELDINFO changed field_index to %d",
                            finfo.field_index);
         }
         report_hiddev_field_info(&finfo, d3);

         bool usage_values_reported = false;
         __u32 common_ucode = 0;
         if (finfo.maxusage > 1)
            common_ucode = get_identical_ucode(fd, &finfo, fndx);
         if (common_ucode) {
            rpt_vstring(d2, "Identical ucode for all usages: 0x%08x  %s",
                            common_ucode, hiddev_interpret_usage_code(common_ucode));
         }

         // Get values for Feature or Input report
         if (finfo.report_type != HID_REPORT_TYPE_OUTPUT) {
            if (common_ucode) {
               if (finfo.flags & HID_FIELD_BUFFERED_BYTE) {
                  rpt_vstring(d2, "Retrieving values using HIDIOCGUSAGES");

                  struct hiddev_usage_ref_multi uref_multi;
                  uref_multi.uref.report_type = finfo.report_type;
                  uref_multi.uref.report_id   = finfo.report_id;
                  uref_multi.uref.field_index = fndx;
                  uref_multi.uref.usage_index = 0;
                  uref_multi.num_values = finfo.maxusage; // needed? yes!

                  rc = ioctl(fd, HIDIOCGUSAGES, &uref_multi);  // Fills in usage values
                  if (rc != 0) {
                     REPORT_IOCTL_ERROR("HIDIOCGUSAGES", rc);
                  }
                  else {
                     // printf("(%s) Value retrieved by HIDIOCGUSAGES:\n", __func__);
                     Byte * buf = calloc(1, finfo.maxusage);

                     for (int ndx=0; ndx<finfo.maxusage; ndx++)
                        buf[ndx] = uref_multi.values[ndx] & 0xff;
                     rpt_hex_dump(buf, finfo.maxusage, d2);
                  }
                  usage_values_reported = true;
               }
               else {
                  Buffer * buf = collect_single_byte_usage_values(fd, &finfo, fndx);
                  if (buf) {
                     // printf("(%s) Values retrieved by collect_single_byte_usage_values()\n", __func__);
                     rpt_vstring(d2, "Retrieving values using multiple HIDIOCGUSAGE calls");
                     rpt_hex_dump(buf->bytes, buf->len, d2);
                     usage_values_reported = true;
                     buffer_free(buf, __func__);
                  }
               }
            } // common_ucode == true

            if (!usage_values_reported) {
               rpt_vstring(d2, "Usages for report_id: %d, field_index %d:",
                               finfo.report_id, fndx /*finfo.field_index */);
               for (undx = 0; undx < finfo.maxusage; undx++) {
                  report_field_usage(fd,
                                     finfo.report_type,
                                     finfo.report_id,
                                     fndx,
                                     undx,
                                     /*show_value=*/ true,
                                     d4);
               }  //loop over undx
            }  // common_ucode == false or !usage_values_collected
         }  // HID_REPORT_TYPE_OUTPUT

      }  // loop over fndx
      rinfo.report_id |= HID_REPORT_ID_NEXT;
      ret = ioctl(fd, HIDIOCGREPORTINFO, &rinfo);
   }
   if (rptct == 0)
      rpt_title("None", d1);
}


/* Reports all report descriptors for an open HID device.
 *
 * Arguments:
 *   fd        file descriptor
 *   depth     logical indentation depth
 *
 * Returns:    nothing
 */
void report_all_report_descriptors(int fd, int depth) {
   report_report_descriptors_for_report_type(fd, HID_REPORT_TYPE_INPUT, depth);
   report_report_descriptors_for_report_type(fd, HID_REPORT_TYPE_OUTPUT, depth);
   report_report_descriptors_for_report_type(fd, HID_REPORT_TYPE_FEATURE, depth);
}


/* Reports all collection information for an open HID device.
 *
 * Arguments:
 *   fd        file descriptor
 *   depth     logical indentation depth
 *
 * Returns:    nothing
 */
void report_all_collections(int fd, int depth) {
   int d1 = depth+1;
   // int d2 = depth+2;
   rpt_title("All collections for device:", depth);
   int cndx = 0;   // collection indexes start at 0
   int ioctl_rc = 0;
   for (cndx=0; ioctl_rc != -1; cndx++) {
      struct hiddev_collection_info  cinfo;
      memset(&cinfo, 0, sizeof(cinfo));
      errno = 0;
      cinfo.index = cndx;
      ioctl_rc = ioctl(fd, HIDIOCGCOLLECTIONINFO, &cinfo);
      if (ioctl_rc != -1) {
         rpt_vstring(d1,"Collection %d:", cinfo.index);
         report_hiddev_collection_info(&cinfo, d1);
      }
   }
}


/* Reports all information about an open HID device.
 *
 * Arguments:
 *   fd        file descriptor
 *   depth     logical indentation depth
 *
 * Returns:    nothing
 */
void report_hiddev_device_by_fd(int fd, int depth) {
   const int d1 = depth+1;
   const int d2 = depth+2;

   struct hiddev_devinfo dev_info;
   int rc;
   int version;
   ioctl(fd, HIDIOCGVERSION, &version);   // no need to test return code, always succeeds
   rpt_vstring(depth, "hiddev driver version (reported by HIDIOCGVERSION): %d.%d.%d",
          version>>16, (version >> 8) & 0xff, version & 0xff);

#ifdef REDUNDANT_INFORMATION
   char * cgname = get_hiddev_name(fd);               // HIDIOCGNAME
   // printf("(%s) get_hiddev_name() returned: |%s|\n", __func__, cgname);
   rpt_vstring(depth, "device name (reported by HIDIOCGNAME): |%s|", cgname);
   free(cgname);
#endif

   rc = ioctl(fd, HIDIOCGDEVINFO, &dev_info);
   if (rc != 0) {
      REPORT_IOCTL_ERROR("HIDIOCGDEVINFO", rc);
      return;
   }
   report_hiddev_devinfo(&dev_info, /*lookup_names=*/true, depth);

   // if (!is_interesting_device(&dev_info)) {
   //       printf("(%s) Uninteresting device\n", __func__);
   //       return;
   // }

   int string_id_limit = -1;
   if (dev_info.vendor == 0x05ac) {
      // string_id_limit = 3;   // Apple never returns invalid index.
      rpt_vstring(depth, "Skipping string retrieval for Apple Cinema display due to limitations.");
      string_id_limit = 0;     // disable entirely - string retrieval painfully slow for Apple Cinema
   }
   puts("");
   if (string_id_limit != 0) {
      report_hiddev_strings(fd,string_id_limit,depth);    // HIDIOCGSTRING
      puts("");
   }

   rpt_title("Usages for each application associated with the device:", depth);
   if (dev_info.num_applications == 0) {   // should never occur, but just in case
      rpt_title("No applications", d2);
   }
   else {
      for (int ndx = 0; ndx < dev_info.num_applications; ndx++) {
         int usage = ioctl(fd, HIDIOCAPPLICATION, ndx);
         // printf("(%s) HIDIOCAPPLICATION returned 0x%08x for application %d\n", __func__, usage, i);
         if (usage == -1) {
            continue;
         }
         rpt_vstring(d1, "Application %d:  Usage code: 0x%08x  %s",
                         ndx, usage, hiddev_interpret_usage_code(usage));
      }
   }
   puts("");

   rpt_title("Collection information is a superset of application information.", depth);
   rpt_title("Querying collections returns information on all collections the device has,", depth);
   rpt_title("not just application collections.", depth);
   puts("");
   report_all_collections(fd,depth);
   puts("");

   rpt_vstring(depth, "Identified as HID monitor: %s", bool_repr(is_hiddev_monitor(fd)) );
   // puts("");


   report_all_report_descriptors(fd, depth);

#ifdef FUTURE
   puts("");
   if (dev_info.vendor == 0x05ac)    // Apple
      get_edid(fd);

   if (is_hiddev_monitor(fd)) {
      find_edid_report(fd);
   }
#endif
}


#ifdef NOT_NEEDED
// pci_usb_ids functions now call pciusb_ids_ensure_initialized() as necessary
void init_hiddev_reports() {
   pciusb_id_ensure_initialized();
   // names_init();
}
#endif

