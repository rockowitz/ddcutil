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
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wchar.h>

#include "util/string_util.h"
#include "util/report_util.h"
#include "util/device_id_util.h"
#include "util/hid_report_descriptor.h"
#include "util/usb_hid_common.h"

#include "util/base_hid_report_descriptor.h"



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




void report_raw_hid_report_item(Hid_Report_Item * item, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Hid_Report_Item", item, depth);
   rpt_vstring(d1, "%-20s:  0x%02x", "btype", item->btype);
   rpt_vstring(d1, "%-20s:  0x%02x", "btag", item->btag);
   rpt_vstring(d1, "%-20s:  %d",     "bsize", item->bsize);
   rpt_vstring(d1, "%-20s:  0x%08x", "data", item->data);
}



void free_hid_report_item_list(Hid_Report_Item * head) {
   while (head) {
      Hid_Report_Item * next = head->next;
      free(head);
      head = next;
   }
}




// need better name
Hid_Report_Item * preparse_hid_report(Byte * b, int l) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. b=%p, l=%d\n", __func__, b, l);

   Hid_Report_Item * root   = NULL;
   Hid_Report_Item * prev   = NULL;
   Hid_Report_Item * cur    = NULL;

   // unsigned int j, bsize, btag, btype, data = 0xffff, hut = 0xffff;
   int i, j;
   // char *types[4] = { "Main", "Global", "Local", "reserved" };

   if (debug)
      printf("(%s)          Report Descriptor: (length is %d)\n", __func__, l);

   for (i = 0; i < l; ) {
      cur = calloc(1, sizeof(Hid_Report_Item));

      Byte b0 = b[i] & 0x03;           // first 2 bits are size indicator
      cur->bsize = b0;
      if (cur->bsize == 3)                // values are indicators, not the actual size:
         cur->bsize = 4;                  //  0,1,2,4
      cur->btype = b[i] & (0x03 << 2);    // next 2 bits are type
      cur->btag = b[i] & ~0x03;           // mask out size bits to get tag

      if (cur->bsize > 0) {
         cur->data = 0;
         for (j = 0; j < cur->bsize; j++) {
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
      i += 1 + cur->bsize;
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





void report_hid_report_item(Hid_Report_Item * item, Hid_Report_Item_Globals * globals, int depth) {
   // int d1 = depth+1;
   int d_indent = depth+5;

   // TODO: handle push/pop of globals

   // unsigned int j, bsize, btag, btype, data = 0xffff, hut = 0xffff;
   // unsigned int hut = 0xffff;
   // int i;
   char *types[4] = { "Main", "Global", "Local", "reserved" };
   char indent[] = "                            ";

   char databuf[80];
   if (item->bsize == 0)
      strcpy(databuf, "none");
   else
      snprintf(databuf, 80, "[ 0x%0*x ]", item->bsize*2, item->data);

   rpt_vstring(depth, "Item(%-6s): %s, data=%s",
                      types[item->btype>>2],
                      devid_hid_descriptor_item_type(item->btag),  // replacement for names_reporttag()
                      databuf);


   switch (item->btag) {
   case 0x04: /* Usage Page */
      // printf("%s0x%02x ", indent, data);                                //  A
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
      // hut = item->data;
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
      printf("%sUnit Exponent: %i\n", indent,
             (signed char)item->data);

      break;

   case 0x64: /* Unit */
      printf("%s", indent);
      dump_unit(item->data, item->bsize);
      break;


   case 0xa0: /* Collection */
      rpt_vstring(d_indent, "%s", collection_type_name(item->data));
      break;

   case 0x80: /* Input */
   case 0x90: /* Output */
   case 0xb0: /* Feature */
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
      break;

   }
}


void report_hid_report_item_list(Hid_Report_Item * head, int depth) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting.\n", __func__);
   Hid_Report_Item_Globals globals;
   memset(&globals, 0, sizeof(Hid_Report_Item_Globals));
   while (head) {
      report_hid_report_item(head, &globals, depth);
      head = head->next;
   }
}



/* Processes the bytes of a HID Report Descriptor,
 * writes a report in the form similar to that used
 * in HID Report Descriptor documentation.
 *
 * Arguments:
 *    b        address of bytes
 *    l        number of bytes
 *
 * Returns:    nothing
 */
 void dump_report_desc(unsigned char *b, int l)
{
    bool debug = true;
    if (debug)
       printf("(%s) Starting. b=%p, l=%d\n", __func__, b, l);
   unsigned int j, bsize, btag, btype, data = 0xffff, hut = 0xffff;
   int i;
   char *types[4] = { "Main", "Global", "Local", "reserved" };
   char indent[] = "                            ";

   printf("          Report Descriptor: (length is %d)\n", l);
   for (i = 0; i < l; ) {
      bsize = b[i] & 0x03;           // first 2 bits are size indicator
      if (bsize == 3)                // values are indicators, not the actual size:
         bsize = 4;                  //  0,1,2,4
      btype = b[i] & (0x03 << 2);    // next 2 bits are type
      btag = b[i] & ~0x03;           // mask out size bits to get tag

      printf("            Item(%-6s): %s, data=", types[btype>>2],
            devid_hid_descriptor_item_type(btag));      // replaces names_reporttag()
      // printf("            Item(%-6s): 0x%08x, data=",
      //        types[btype>>2],
      //        btag);
      if (bsize > 0) {
         printf(" [ ");
         data = 0;
         for (j = 0; j < bsize; j++) {
            printf("0x%02x ", b[i+1+j]);
            data += (b[i+1+j] << (8*j));
         }
         printf("] %d", data);
      } else
         printf("none");
      printf("\n");

      switch (btag) {
      case 0x04: /* Usage Page */
         // printf("%s0x%02x ", indent, data);                                //  A
         // hack
           switch(data) {
           case 0xffa0:
              printf("Fixup: data = 0xffa0 -> 0x80\n");
              data = 0x80;
              break;
           case 0xffa1:
              data = 0x81;
              break;
           }
         printf("%s%s\n", indent,
               devid_usage_code_page_name(data));
               // names_huts(data));
         hut = data;

         break;

      case 0x08: /* Usage */
      case 0x18: /* Usage Minimum */
      case 0x28: /* Usage Maximum */
      {
         // char * name = names_hutus((hut<<16) + data);
         char * name = devid_usage_code_id_name(hut,data);
         char buf[16];
         if (!name && btag == 0x08) {
            sprintf(buf, "EDID %d", data);
            name = buf;
         }
         printf("%s%s\n", indent, name);

         // printf("%s%s\n", indent,
         //        names_hutus((hut << 16) + data));                                 // B
         // printf("%s0x%08x\n", indent,
         //        (hut << 16) + data);
      }
         break;

      case 0x54: /* Unit Exponent */
         printf("%sUnit Exponent: %i\n", indent,
                (signed char)data);

         break;

      case 0x64: /* Unit */
         printf("%s", indent);
         dump_unit(data, bsize);
         break;

      case 0xa0: /* Collection */
         printf("%s", indent);
         switch (data) {
         case 0x00:
            printf("Physical\n");
            break;

         case 0x01:
            printf("Application\n");
            break;

         case 0x02:
            printf("Logical\n");
            break;

         case 0x03:
            printf("Report\n");
            break;

         case 0x04:
            printf("Named Array\n");
            break;

         case 0x05:
            printf("Usage Switch\n");
            break;

         case 0x06:
            printf("Usage Modifier\n");
            break;

         default:
            if (data & 0x80)
               printf("Vendor defined\n");
            else
               printf("Reserved for future use.\n");
         }
         break;
      case 0x80: /* Input */
      case 0x90: /* Output */
      case 0xb0: /* Feature */
         printf("%s%s %s %s %s %s\n%s%s %s %s %s\n",
                indent,
                data & 0x01  ? "Constant"   : "Data",
                data & 0x02  ? "Variable"   : "Array",
                data & 0x04  ? "Relative"   : "Absolute",
                data & 0x08  ? "Wrap"       : "No_Wrap",
                data & 0x10  ? "Non_Linear" : "Linear",
                indent,
                data & 0x20  ? "No_Preferred_State" : "Preferred_State",
                data & 0x40  ? "Null_State"     : "No_Null_Position",
                data & 0x80  ? "Volatile"       : "Non_Volatile",
                data & 0x100 ? "Buffered Bytes" : "Bitfield");
         break;
      }
      i += 1 + bsize;
   }
}
