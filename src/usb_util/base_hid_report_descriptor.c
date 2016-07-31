/* base_hid_report_descriptor.c
 *
 * Functions to perform basic parsing of the HID Report Descriptor and
 * display the contents of the Report Descriptor in the format used
 * in HID documentation.
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
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/device_id_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "usb_util/usb_hid_common.h"
#include "usb_util/base_hid_report_descriptor.h"


typedef
struct ptr_pair{
   void * p1;
   void * p2;
} Ptr_Pair;

Ptr_Pair item_flag_names_r(uint16_t flags, char * b1, int b1_size, char * b2, int b2_size) {
   assert(b1_size >= 80);
   assert(b2_size >= 80);

   snprintf(b1, b1_size, "%s %s %s %s %s",
         flags & 0x01  ? "Constant"   : "Data",
         flags & 0x02  ? "Variable"   : "Array",
         flags & 0x04  ? "Relative"   : "Absolute",
         flags & 0x08  ? "Wrap"       : "No_Wrap",
         flags & 0x10  ? "Non_Linear" : "Linear"
        );
   snprintf(b2, b2_size, "%s %s %s %s",
         flags & 0x20  ? "No_Preferred_State" : "Preferred_State",
         flags & 0x40  ? "Null_State"         : "No_Null_Position",
         flags & 0x80  ? "Volatile"           : "Non_Volatile",
         flags & 0x100 ? "Buffered Bytes"     : "Bitfield"
        );

   Ptr_Pair result = {b1,b2};
   return result;
}


Ptr_Pair item_flag_names(uint16_t flags) {
   static char b1[80];
   static char b2[80];
   return item_flag_names_r(flags, b1, 80, b2, 80);
}



#ifdef OLD
static void dump_unit(unsigned int data, unsigned int len)
{
   char *systems[5] = { "None", "SI Linear", "SI Rotation",
         "English Linear", "English Rotation" };

   char *units[5][8] = {
      { "None", "None", "None", "None", "None",
            "None", "None", "None" },
      { "None", "Centimeter", "Gram", "Seconds", "Kelvin",
            "Ampere", "Candela", "None" },
      { "None", "Radians",    "Gram", "Seconds", "Kelvin",
            "Ampere", "Candela", "None" },
      { "None", "Inch",       "Slug", "Seconds", "Fahrenheit",
            "Ampere", "Candela", "None" },
      { "None", "Degrees",    "Slug", "Seconds", "Fahrenheit",
            "Ampere", "Candela", "None" },
   };

   unsigned int i;
   unsigned int sys;
   int earlier_unit = 0;

   /* First nibble tells us which system we're in. */
   sys = data & 0xf;
   data >>= 4;

   if (sys > 4) {
      if (sys == 0xf)
         printf("System: Vendor defined, Unit: (unknown)\n");
      else
         printf("System: Reserved, Unit: (unknown)\n");
      return;
   } else {
      printf("System: %s, Unit: ", systems[sys]);
   }
   for (i = 1 ; i < len * 2 ; i++) {
      char nibble = data & 0xf;
      data >>= 4;
      if (nibble != 0) {
         if (earlier_unit++ > 0)
            printf("*");
         printf("%s", units[sys][i]);
         if (nibble != 1) {
            /* This is a _signed_ nibble(!) */

            int val = nibble & 0x7;
            if (nibble & 0x08)
               val = -((0x7 & ~val) + 1);
            printf("^%d", val);
         }
      }
   }
   if (earlier_unit == 0)
      printf("(None)");
   printf("\n");
}

#endif


static char * unit_name_r(unsigned int data, unsigned int len, char * buf, int bufsz)
{
   assert(bufsz >= 80);

   char *systems[5] = { "None", "SI Linear", "SI Rotation",
         "English Linear", "English Rotation" };

   char *units[5][8] = {
      { "None", "None", "None", "None", "None",
            "None", "None", "None" },
      { "None", "Centimeter", "Gram", "Seconds", "Kelvin",
            "Ampere", "Candela", "None" },
      { "None", "Radians",    "Gram", "Seconds", "Kelvin",
            "Ampere", "Candela", "None" },
      { "None", "Inch",       "Slug", "Seconds", "Fahrenheit",
            "Ampere", "Candela", "None" },
      { "None", "Degrees",    "Slug", "Seconds", "Fahrenheit",
            "Ampere", "Candela", "None" },
   };

   unsigned int i;
   unsigned int sys;
   int earlier_unit = 0;

   /* First nibble tells us which system we're in. */
   sys = data & 0xf;
   data >>= 4;

   if (sys > 4) {
      if (sys == 0xf)
         strcpy(buf, "System: Vendor defined, Unit: (unknown)");
      else
         strcpy(buf, "System: Reserved, Unit: (unknown)");
   }
   else {
      sprintf(buf, "System: %s, Unit: ", systems[sys]);

      for (i = 1 ; i < len * 2 ; i++) {
         char nibble = data & 0xf;
         data >>= 4;
         if (nibble != 0) {
            if (earlier_unit++ > 0)
               strcat(buf, "*");
            sprintf(buf+strlen(buf), "%s", units[sys][i]);
            if (nibble != 1) {
               /* This is a _signed_ nibble(!) */

               int val = nibble & 0x7;
               if (nibble & 0x08)
                  val = -((0x7 & ~val) + 1);
               sprintf(buf+strlen(buf), "^%d", val);
            }
         }
      }
   }
   if (earlier_unit == 0)
      strcat(buf, "(None)");

   return buf;
}


static char * unit_name(unsigned int data, unsigned int len) {
   static char buf[80];
   return unit_name_r(data, len, buf, 80);
}


/** Debugging function.
 */
void report_raw_hid_report_item(Hid_Report_Descriptor_Item * item, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Hid_Report_Item", item, depth);
   rpt_vstring(d1, "%-20s:  0x%02x", "btype", item->btype);
   rpt_vstring(d1, "%-20s:  0x%02x", "btag", item->btag);
   // rpt_vstring(d1, "%-20s:  %d",     "bsize", item->bsize);
   // rpt_vstring(d1, "%-20s:  %d",     "bsize_orig", item->bsize_orig);
   rpt_vstring(d1, "%-20s:  %d",     "bsize_bytect", item->bsize_bytect);
   rpt_vstring(d1, "%-20s:  0x%08x", "data", item->data);
}



void free_hid_report_item_list(Hid_Report_Descriptor_Item * head) {
   while (head) {
      Hid_Report_Descriptor_Item * next = head->next;
      free(head);
      head = next;
   }
}


/* Converts the bytes of a HID Report Descriptor to a linked list of
 * Hid_Report_Items.
 *
 * Arguments:
 *    b        address of bytes
 *    l        number of bytes
 *
 * Returns:    linked list of Hid_Report_Items
 */
// need better name
Hid_Report_Descriptor_Item * tokenize_hid_report_descriptor(Byte * b, int l) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. b=%p, l=%d\n", __func__, b, l);

   Hid_Report_Descriptor_Item * root   = NULL;
   Hid_Report_Descriptor_Item * prev   = NULL;
   Hid_Report_Descriptor_Item * cur    = NULL;

   int i, j;

   // if (debug)
   //   printf("(%s)          Report Descriptor: (length is %d)\n", __func__, l);

   for (i = 0; i < l; ) {
      cur = calloc(1, sizeof(Hid_Report_Descriptor_Item));

      Byte b0 = b[i] & 0x03;                  // first 2 bits are size indicator, 0, 1, 2, or 3
      cur->bsize_bytect = (b0 == 3) ? 4 : b0; // actual number of bytes
      cur->btype = (b[i] & (0x03 << 2))>>2;   // next 2 bits are type, shift to range 0..3
      cur->btag = b[i] & ~0x03;               // mask out size bits to get tag

      if (cur->bsize_bytect > 0) {
         cur->data = 0;
         for (j = 0; j < cur->bsize_bytect; j++) {
            cur->data += (b[i+1+j] << (8*j));
         }
      }

      if (!root) {
         root = cur;
         prev = cur;
      }
      else {
         prev->next = cur;
         prev = cur;
      }
      i += 1 + cur->bsize_bytect;
   }

   if (debug)
      printf("(%s) Returning: %p\n", __func__, root);
   return root;
}


typedef
struct hid_report_item_globals {
   uint16_t    usage_page;
   struct hid_report_item_globals * prev;
} Hid_Report_Item_Globals;


/* Reports a single Hid_Report_Item
 *
 * Arguments:
 *   item          pointer to Hid_Report_Item to report
 *   globals       current globals state
 *   depth         logical indentation depth
 */
void report_hid_report_item(Hid_Report_Descriptor_Item * item, Hid_Report_Item_Globals * globals, int depth) {
   // int d1 = depth+1;
   int d_indent = depth+5;

   // TODO: handle push/pop of globals

   char *types[4] = { "Main", "Global", "Local", "reserved" };

   char databuf[80];
   if (item->bsize_bytect == 0)
      strcpy(databuf, "none");
   else
      snprintf(databuf, 80, "[ 0x%0*x ]", item->bsize_bytect*2, item->data);

   rpt_vstring(depth, "Item(%-6s): %s, data=%s",
                      types[item->btype],
                      devid_hid_descriptor_item_type(item->btag),  // replacement for names_reporttag()
                      databuf);


   switch (item->btag) {
   case 0x04: /* Usage Page */
      // hack
        switch(item->data) {     // belongs elsewhere
        case 0xffa0:
           printf("Fixup: data = 0xffa0 -> 0x80\n");
           item->data = 0x80;
           break;
        case 0xffa1:
           item->data = 0x81;
           break;
        }
      rpt_vstring(d_indent, "%s", devid_usage_code_page_name(item->data));   // names_huts(data));
      globals->usage_page = item->data;
      break;

   case 0x08: /* Usage */
   case 0x18: /* Usage Minimum */
   case 0x28: /* Usage Maximum */
   {
      // char * name = names_hutus((hut<<16) + item->data);
      char * name = devid_usage_code_id_name(globals->usage_page,item->data);
      // char buf[16];
      // if (!name && item->btag == 0x08) {
      //    sprintf(buf, "EDID %d", item->data);
      //    name = buf;
      // }
      if (!name) {
         name = "Unrecognized usage";
      }
      rpt_vstring(d_indent, "%s", name);
      // printf("%s%s\n", indent, name);

      // printf("%s%s\n", indent,
      //        names_hutus((hut << 16) + item->data));                                 // B
      // printf("%s0x%08x\n", indent,
      //        (hut << 16) + item->data);
   }
      break;

   case 0x54: /* Unit Exponent */
      rpt_vstring(d_indent, "Unit Exponent: %i", (signed char)item->data);
      break;

   case 0x64: /* Unit */
      rpt_vstring(d_indent, "%s", unit_name(item->data, item->bsize_bytect));
      break;


   case 0xa0: /* Collection */
      rpt_vstring(d_indent, "%s", collection_type_name(item->data));
      break;

   case 0x80: /* Input */
   case 0x90: /* Output */
   case 0xb0: /* Feature */
   {
#ifdef OLD
      printf("%s%s %s %s %s %s\n%s%s %s %s %s\n",
             indent,
             item->data & 0x01  ? "Constant"   : "Data",
             item->data & 0x02  ? "Variable"   : "Array",
             item->data & 0x04  ? "Relative"   : "Absolute",
             item->data & 0x08  ? "Wrap"       : "No_Wrap",
             item->data & 0x10  ? "Non_Linear" : "Linear",
             indent,
             item->data & 0x20  ? "No_Preferred_State" : "Preferred_State",
             item->data & 0x40  ? "Null_State"     : "No_Null_Position",
             item->data & 0x80  ? "Volatile"       : "Non_Volatile",
             item->data & 0x100 ? "Buffered Bytes" : "Bitfield");
#endif
      Ptr_Pair flag_names = item_flag_names(item->data);
      rpt_vstring(d_indent, "%s", flag_names.p1);
      rpt_vstring(d_indent, "%s", flag_names.p2);
      break;
   }

   }
}


/* Given a Hid Report Descriptor, represented as a linked list of
 * Hid_Report_Items, display the descriptor in a form similar to that
 * used in HID documentation, with annotation.
 *
 * Arguments:
 *    head      first item in list
 *    depth     logical indentation depth
 *
 * Returns:     nothing
 */
void report_hid_report_item_list(Hid_Report_Descriptor_Item * head, int depth) {
   bool debug = true;
   if (debug)
      printf("(%s) Starting.\n", __func__);
   Hid_Report_Item_Globals globals;   // will contain current globals as list is walked
   memset(&globals, 0, sizeof(Hid_Report_Item_Globals));
   while (head) {
      report_hid_report_item(head, &globals, depth);
      head = head->next;
   }
}


/* Indicates if a tokenized HID Report Descriptor, represented as a linked
 * list of Hid_Report_Descriptor_Items, represents a USB connected monitor.
 *
 * Arguments:
 *    report_item_list   list head
 *
 * Returns:        true/false
 *
 * Per section 5.5 of Usb Monitor Control Class Specification Rev 1.0:
 * "In order to identify a HID class device as a monitor, the device's
 * HID Report Descriptor must contain a top-level collection with a usage
 * of Monitor Control from the USB Monitor Usage Page."
 *
 * i.e. Usage page = 0x80  USB monitor
 *      Usage id   = 0x01  Monitor Control
 */
bool is_monitor_by_tokenized_report_descriptor(Hid_Report_Descriptor_Item * report_item_list) {
   bool is_monitor = false;

   Hid_Report_Descriptor_Item * cur_item = report_item_list;

   // We cheat on the spec. Just look at the first Usage Page item, is it USB Monitor?
   while (cur_item) {
      if (cur_item->btag == 0x04) {
         if (cur_item->data == 0x80) {
            is_monitor = true;
         }
         break;
      }
   }
   return is_monitor;
}
