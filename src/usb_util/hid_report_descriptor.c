/* hid_report_descriptor.c
 *
 * Interpret a HID Report Descriptor
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
 *
 * Report parsing adapted from lsusb.c (command lsusb) by Thomas Sailer and
 * David Brownell.
 */

#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/coredefs.h"
#include "util/device_id_util.h"
#include "util/report_util.h"

#include "usb_util/usb_hid_common.h"
#include "usb_util/base_hid_report_descriptor.h"

#include "usb_util/hid_report_descriptor.h"


//
// Lookup tables
//

static const char* report_type_name_table[] = {
      "invalid",
      "Input",
      "Output",
      "Feature"
};


/* Returns a string representation of a report type id
 *
 * Arguments:  report_type
 *
 * Returns:  string representation of id
 */
const char * hid_report_type_name(Byte report_type) {
   if (report_type < HID_REPORT_TYPE_MIN || report_type > HID_REPORT_TYPE_MAX)
      report_type = 0;
   return report_type_name_table[report_type];
}


/* Create a string representation of Main Item flags bitfield.
 * The representation is returned in a buffer provided, which
 * must be at least 150 bytes in size.
 *
 * Arguments:
 *    data       flags
 *    buffer     where to save formatted response
 *    bufsz      buffer size
 *
 * Returns:      buffer
 */
char * interpret_item_flags_r(uint16_t data, char * buffer, int bufsz) {
   assert(buffer && bufsz > 150);

   snprintf(buffer, bufsz, "%s %s %s %s %s %s %s %s %s",
       data &  0x01 ? "Constant"           : "Data",
       data &  0x02 ? "Variable"           : "Array",
       data &  0x04 ? "Relative"           : "Absolute",
       data &  0x08 ? "Wrap"               : "No_Wrap",
       data &  0x10 ? "Non_Linear"         : "Linear",
       data &  0x20 ? "No_Preferred_State" : "Preferred_State",
       data &  0x40 ? "Null_State"         : "No_Null_Position",
       data &  0x80 ? "Volatile"           : "Non_Volatile",
       data & 0x100 ? "Buffered Bytes"     : "Bitfield");
   return buffer;
}


//
// Functions to report Parsed_Hid_Descriptor and its contained structs
//

void report_hid_field(Parsed_Hid_Field * hf, int depth) {
   int d1 = depth+1;
   // rpt_structure_loc("Hid_Field", hf, depth);
   rpt_title("Field: ", depth);
   char buf[200];
   rpt_vstring(d1, "%-20s:  0x%04x      %s", "Usage page",
                   hf->usage_page,
                   devid_usage_code_page_name(hf->usage_page));

   // deprecated
   // rpt_vstring(d1, "%-20s:  0x%04x  %s", "Usage id",
   //                 hf->usage_id,
   //                 devid_usage_code_id_name(hf->usage_page, hf->usage_id));

   char * ucode_name = "";
   if (hf->extended_usage) {
      ucode_name = devid_usage_code_name_by_extended_id(hf->extended_usage);
      if (!ucode_name)
         ucode_name = "(Unrecognized usage code)";
   }
   rpt_vstring(d1, "%-20s:  0x%08x  %s", "Extended Usage",
                   hf->extended_usage,
                   ucode_name);

   rpt_vstring(d1, "%-20s:  0x%04x      %s", "Item flags",
                   hf->item_flags,
                   interpret_item_flags_r(hf->item_flags, buf, 200) );
   rpt_vstring(d1, "%-20s:  0x%04x      %d", "Logical minimum",
                   hf->logical_minimum, hf->logical_minimum);
   rpt_vstring(d1, "%-20s:  0x%04x      %d", "Logical maximum",
                   hf->logical_maximum, hf->logical_maximum);
   rpt_vstring(d1, "%-20s:  0x%04x      %d", "Physical minimum",
                   hf->physical_minimum, hf->physical_minimum);
   rpt_vstring(d1, "%-20s:  0x%04x      %d", "Physical maximum",
                   hf->physical_maximum, hf->physical_maximum);
   rpt_vstring(d1, "%-20s:  %d", "Report size", hf->report_size);
   rpt_vstring(d1, "%-20s:  %d", "Report count", hf->report_count);
   rpt_vstring(d1, "%-20s:  0x%04x      %d", "Unit_exponent", hf->unit_exponent, hf->unit_exponent);
   rpt_vstring(d1, "%-20s:  0x%04x      %d", "Unit", hf->unit, hf->unit);
}


/* Report a single report in a parsed HID report descriptor
 *
 * Arguments:
 *   pdesc      pointer to Hid_Report instance
 *   depth      logical indentation depth
 *
 * Returns:     nothing
 */
void report_parsed_hid_report(Parsed_Hid_Report * hr, int depth) {
   int d1 = depth+1;
   // int d2 = depth+2;
   // rpt_structure_loc("Hid_Report", hr,depth);
   rpt_vstring(depth, "%-20s:%*s 0x%02x  %d", "Report id",   rpt_indent(1), "", hr->report_id, hr->report_id);
   rpt_vstring(d1, "%-20s: 0x%02x  %s", "Report type",
                   hr->report_type, hid_report_type_name(hr->report_type) );
   if (hr->hid_fields && hr->hid_fields->len > 0) {
      // rpt_title("Fields: (alt) ", d1);
      for (int ndx=0; ndx<hr->hid_fields->len; ndx++) {
         report_hid_field( g_ptr_array_index(hr->hid_fields, ndx), d1);
      }
   }
   else
      rpt_vstring(d1, "%-20s: none", "Fields");
}


void report_hid_collection(Parsed_Hid_Collection * col, int depth) {
   bool show_dummy_root = false;

   int d1 = depth+1;
   if (!col->is_root_collection || show_dummy_root)
      rpt_structure_loc("Hid_Collection", col, depth);
   if (col->is_root_collection) {
      if (show_dummy_root)
         rpt_title("Dummy root collection", d1);
   }
   else {
      rpt_vstring(d1, "%-20s:  x%02x  %s", "Collection type",
                      col->collection_type, collection_type_name(col->collection_type));
      rpt_vstring(d1, "%-20s:  x%02x  %s", "Usage page",
                      col->usage_page, devid_usage_code_page_name(col->usage_page));

      // deprecated:
      // rpt_vstring(d1, "%-20s:  x%02x  %s", "Usage id",
      //                 col->usage_id, devid_usage_code_id_name(col->usage_page, col->usage_id));

      rpt_vstring(d1, "%-20s:  0x%08x  %s", "Extended Usage",
                      col->extended_usage,
                      devid_usage_code_name_by_extended_id(col->extended_usage));
   }

   if (col->child_collections && col->child_collections->len > 0) {
      int child_depth = d1;
      if (!col->is_root_collection || show_dummy_root)
         rpt_title("Contained collections: ", d1);
      else
         child_depth = depth;
      for (int ndx = 0; ndx < col->child_collections->len; ndx++) {
         Parsed_Hid_Collection * a_child = g_ptr_array_index(col->child_collections, ndx);
         report_hid_collection(a_child, child_depth);
      }
   }

   if (col->reports && col->reports->len > 0) {
      if (col->is_root_collection)
         printf("(%s) ERROR: Dummy root collection contains reports\n", __func__);
      rpt_title("Reports:", d1);
      for (int ndx = 0; ndx < col->reports->len; ndx++)
         report_parsed_hid_report(g_ptr_array_index(col->reports, ndx), d1);
   }
   else
      rpt_vstring(d1, "%-20s:  None", "Reports");
}


/* Report a parsed HID descriptor.
 *
 * Arguments:
 *   pdesc      pointer to Parsed_Hid_Descriptor
 *   depth      logical indentation depth
 *
 * Returns:     nothing
 */
void report_parsed_hid_descriptor(Parsed_Hid_Descriptor * pdesc, int depth) {
   int d1 = depth + 1;
   rpt_structure_loc("Parsed_Hid_Descriptor", pdesc, depth);
   report_hid_collection(pdesc->root_collection, d1);
}


static void accumulate_report_descriptors_for_collection(
               Parsed_Hid_Collection * col, Byte report_type_flags, GPtrArray * accumulator)
{
   if (col->child_collections && col->child_collections->len > 0) {
      for (int ndx = 0; ndx < col->child_collections->len; ndx++) {
         Parsed_Hid_Collection * a_child = g_ptr_array_index(col->child_collections, ndx);
         accumulate_report_descriptors_for_collection(a_child, report_type_flags, accumulator);
      }
   }

   if (col->reports && col->reports->len > 0) {
      for (int ndx = 0; ndx < col->reports->len; ndx++) {
         Parsed_Hid_Report * rpt = g_ptr_array_index(col->reports, ndx);
         if ( (rpt->report_type == HID_REPORT_TYPE_INPUT   && (report_type_flags & HIDF_REPORT_TYPE_INPUT  )) ||
              (rpt->report_type == HID_REPORT_TYPE_OUTPUT  && (report_type_flags & HIDF_REPORT_TYPE_OUTPUT )) ||
              (rpt->report_type == HID_REPORT_TYPE_FEATURE && (report_type_flags & HIDF_REPORT_TYPE_FEATURE))
            )
         {
            g_ptr_array_add(accumulator, rpt);
         }
      }
   }
}


/* Extracts the report descriptors of the specified report type or types and
 * returns them as an array of pointers to the reports.
 *
 * Arguments:
 *   phd                 pointer to parsed HID descriptor
 *   report_type_flags   report types to select, as array of flag bits
 *
 * Returns:              array of pointers to Hid_Report
 */
GPtrArray *
select_parsed_report_descriptors(Parsed_Hid_Descriptor * phd, Byte report_type_flags) {
   GPtrArray * selected_reports = g_ptr_array_new();
   accumulate_report_descriptors_for_collection(
      phd->root_collection, report_type_flags, selected_reports);
   return selected_reports;
}


//
// Data structures and functions For building Parsed_Hid_Descriptor
//

// struct cur_report_globals;

typedef
struct cur_report_globals {
   uint16_t  usage_page;
   int16_t   logical_minimum;
   int16_t   logical_maximum;
   int16_t   physical_minimum;
   int16_t   physical_maximum;
   uint16_t  unit_exponent;
   uint16_t  unit;
   uint16_t  report_size;
   uint16_t  report_id;
   uint16_t  report_count;   // number of data fields for the item
   struct cur_report_globals *  prev;
} Cur_Report_Globals;


typedef
struct cur_report_locals {
   // if bSize = 3, usages are 4 byte extended usages
   // int         usage_bsize;    // actually just 0..4
   int         usage_bsize_bytect;   // 0, 1, 2, or 4
   GArray *    usages;
   uint32_t    usage_minimum;
   uint32_t    usage_maximum;
   GArray *    designator_indexes;
   uint16_t    designator_minimum;
   uint16_t    designator_maximum;
   GArray *    string_indexes;
   uint16_t    string_maximum;
   uint16_t    string_minimum;
} Cur_Report_Locals;


void free_cur_report_locals(Cur_Report_Locals * locals) {
   if (locals) {
      if (locals->usages)
         g_array_free(locals->usages, true);
      if (locals->string_indexes)
         g_array_free(locals->string_indexes, true);
      if (locals->designator_indexes)
         g_array_free(locals->designator_indexes, true);
      free(locals);
   }
}


Parsed_Hid_Report * find_hid_report(Parsed_Hid_Collection * col, Byte report_type, uint16_t report_id) {
   Parsed_Hid_Report * result = NULL;

   if (col->reports->len) {
      for (int ndx=0; ndx < col->reports->len && !result; ndx++) {
         Parsed_Hid_Report * cur = g_ptr_array_index(col->reports, ndx);
         if (cur->report_type == report_type && cur->report_id == report_id)
            result = cur;
      }
   }

   return result;
}

Parsed_Hid_Report * find_hid_report_or_new(Parsed_Hid_Collection * hc, Byte report_type, uint16_t report_id) {
   bool debug = false;
   if (debug)
      printf("(%s) report_type=%d, report_id=%d\n", __func__, report_type, report_id);
   Parsed_Hid_Report * result = find_hid_report(hc, report_type, report_id);
   if (!result) {
      if (!hc->reports) {
         hc->reports = g_ptr_array_new();
      }
      result = calloc(1, sizeof(Parsed_Hid_Report));
      result->report_id = report_id;
      result->report_type = report_type;
      g_ptr_array_add(hc->reports, result);
   }
   return result;
}


void add_report_field(Parsed_Hid_Report * hr, Parsed_Hid_Field * hf) {
   assert(hr && hf);
   if (!hr->hid_fields)
      hr->hid_fields = g_ptr_array_new();
   g_ptr_array_add(hr->hid_fields, hf);
}


void add_hid_collection_child(Parsed_Hid_Collection * parent, Parsed_Hid_Collection * new_child) {
   if (!parent->child_collections)
      parent->child_collections = g_ptr_array_new();

   g_ptr_array_add(parent->child_collections, new_child);
}


/* From the Device Class Definition for Human Interface Devices:

Interpretation of Usage, Usage Minimum orUsage Maximum items vary as a
function of the item's bSize field. If the bSize field = 3 then the item is
interpreted as a 32 bit unsigned value where the high order 16 bits defines the
Usage Page  and the low order 16 bits defines the Usage ID. 32 bit usage items
that define both the Usage Page and Usage ID are often referred to as
"Extended" Usages.

If the bSize field = 1 or 2 then the Usage is interpreted as an unsigned value
that selects a Usage ID on the currently defined Usage Page. When the parser
encounters a main item it concatenates the last declared Usage Page with a
Usage to form a complete usage value. Extended usages can be used to
override the currently defined Usage Page for individual usages.
 */


/* Creates an extended usage value from a usage page and usage value.
 * If the usage value size (usage_bsize) is 4 bytes, then it is already
 * and extended value and is returned.   If it is 1 or 2 bytes, then it
 * represents a simple usage id and is combined with the usage page to
 * create an extended value.
 *
 * If usage_bsize is not in the range 1..4, then the extended value is
 * created heuristically.  If the high order bytes of usage are non-zero,
 * the usage is assumed to be an extended usage and returned.  If the high
 * order bytes of usage are 0, it is treated as a simple usage id and is
 * combined with with usage_page to create the extended usage value.
 *
 * Arguments:
 *   usage_page
 *   usage
 *   usage_bsize         3 or 4 indicates a 4 byte value
 *
 * Returns:              extended usage value
 */

uint32_t extended_usage(uint16_t usage_page, uint32_t usage, int usage_bsize) {
   bool debug = false;
   uint32_t result = 0;
   if (usage_bsize == 3 || usage_bsize == 4)  // allow it to be indicator (3) or actual number of bytes
      result = usage;
   else if (usage_bsize == 1 || usage_bsize == 2) {
      assert( (usage & 0xff00) == 0);
      result = usage_page <<16 | usage;
   }
   else if (usage & 0xff00)
      result = usage;
   else
      result = usage_page << 16 | usage;

   if (debug) {
      printf("(%s) usage_page=0x%04x, usage=0x%08x, usage_bsize=%d, returning 0x%08x\n",
             __func__, usage_page, usage, usage_bsize, result);
   }
   return result;
}


#ifdef OLD
/* Primary function to parse the bytes of a HID report descriptor.
 *
 * Arguments:
 *    b             address of first byte
 *    desclen       number of bytes
 *
 * Returns:         parsed report descriptor
 */
Parsed_Hid_Descriptor * parse_report_desc_old(Byte * b, int desclen) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. b=%p, desclen=%d\n", __func__, b, desclen);

   char *types[4] = { "Main", "Global", "Local", "reserved" };

   unsigned int j, bsize, btag, btype, data = 0xffff;
   unsigned int bsize_bytect;
   // unsigned int hut = 0xffff;
   int i;

   Cur_Report_Globals * cur_globals = calloc(1, sizeof(struct cur_report_globals));
   Cur_Report_Locals  * cur_locals  = calloc(1, sizeof(struct cur_report_locals));
   Parsed_Hid_Collection * cur_collection = NULL;

   Parsed_Hid_Descriptor * parsed_descriptor = calloc(1, sizeof(Parsed_Hid_Descriptor));
   parsed_descriptor->root_collection = calloc(1,sizeof(Parsed_Hid_Collection));
   parsed_descriptor->root_collection->is_root_collection = true;

#define COLLECTION_STACK_SIZE 10
   Parsed_Hid_Collection * collection_stack[COLLECTION_STACK_SIZE];
   collection_stack[0] = parsed_descriptor->root_collection;
   int collection_stack_cur = 0;

   for (i = 0; i < desclen; ) {
      bsize = b[i] & 0x03;           // first 2 bits are size indicator
      bsize_bytect = (bsize == 3) ? 4 : bsize;
      if (bsize == 3)                // values are indicators, not the actual size:
         bsize = 4;                  //  0,1,2,4
      btype = b[i] & (0x03 << 2);    // next 2 bits are type
      btag = b[i] & ~0x03;           // mask out size bits to get tag

      //   printf("             Item(%-6s): %s, data=", types[btype>>2],
      //       names_reporttag(btag));                                       // ok

      data = 0;
      if (bsize_bytect > 0) {
         // printf(" [ ");
         for (j = 0; j < bsize_bytect; j++) {
            // if (debug)
            //    printf("0x%02x ", b[i+1+j]);
            data += (b[i+1+j] << (8*j));
         }
      }   // bsize > 0

      if (debug) {
         char datastr[20];
         switch(bsize_bytect) {
         case 0:
            strcpy(datastr, "none");
            break;
         case 1:
            snprintf(datastr, 20, "[0x%02x] %d", data, data);
            break;
         case 2:
            snprintf(datastr, 20, "[0x%04x] %d", data, data);
            break;
         case 4:
            snprintf(datastr, 20, "[0x%08x] %d", data, data);
            break;
         default:
           // program logic error
            break;
         }
         printf("(%s) Item(%-6s): %s, data=%s\n",
                 __func__,
                 types[btype>>2],
                 devid_hid_descriptor_item_type(btag),
                 datastr);
      }


      switch (btype) {

      // Main item tags

      case 0x00:     // Main item
         switch(btag) {

         case 0xa0:     // Collection
         {
            cur_collection = calloc(1, sizeof(Parsed_Hid_Collection));
            cur_collection->collection_type = data;
            cur_collection->usage_page = cur_globals->usage_page;
            uint32_t cur_usage = 0;
            if (cur_locals->usages && cur_locals->usages->len > 0) {
               cur_usage = g_array_index(cur_locals->usages, uint32_t, 0);
               // cur_collection->usage_id = cur_usage;    // deprecated
            }
            else {
               printf("(%s) No usage id has been set for collection\n", __func__);
            }
            if (cur_usage) {
               cur_collection->extended_usage = extended_usage(
                                                cur_globals->usage_page,
                                                cur_usage,
                                                cur_locals->usage_bsize_bytect);  // or 0 to use heuristic
            }
            else {
               // what to do if there was no usage value?
               // makes no sense to combine it with usage page
               printf("(%s) Collection has no usage value\n", __func__);
            }

            cur_collection->reports = g_ptr_array_new();

            // add this collection as a child of the parent collection
            add_hid_collection_child(collection_stack[collection_stack_cur], cur_collection);
            assert(collection_stack_cur < COLLECTION_STACK_SIZE-1);
            collection_stack[++collection_stack_cur] = cur_collection;
            break;
         }

         case 0x80: /* Input */
         case 0x90: /* Output */
         case 0xb0: /* Feature */
         {
            Parsed_Hid_Field * hf = calloc(1, sizeof(Parsed_Hid_Field));
            Byte report_type;
            if      (btag == 0x80) report_type = HID_REPORT_TYPE_INPUT;
            else if (btag == 0x90) report_type = HID_REPORT_TYPE_OUTPUT;
            else                   report_type = HID_REPORT_TYPE_FEATURE;
            hf->item_flags = data;
            uint16_t report_id = cur_globals->report_id;
            Parsed_Hid_Report * hr = find_hid_report_or_new(
                     cur_collection,
                     report_type,
                     report_id);

            // add this item/field to current report
            add_report_field(hr, hf);

            int field_index = hr->hid_fields->len - 1;
            if (cur_locals->usages && cur_locals->usages->len > 0) {
               int usagect = cur_locals->usages->len;
               int usagendx = (field_index < usagect) ? field_index : usagect-1;
               uint32_t this_usage = g_array_index(cur_locals->usages, uint32_t, usagendx);
               hf->extended_usage = extended_usage(cur_globals->usage_page,
                                                   this_usage,
                                                   cur_locals->usage_bsize_bytect); // or 0  to use heuristic
               if (debug) {
                  printf("(%s) item 0x%02x, usagect=%d, usagendx=%d, this_usage=0x%04x\n", __func__,
                         btag, usagect, usagendx, this_usage);
               }
            }
            else {
               printf("(%s) Tag 0x%02x, Report id: %d: No usage values in cur_locals\n",
                     __func__, btag, report_id);
            }

            hf->usage_page       = cur_globals->usage_page;
            hf->logical_minimum  = cur_globals->logical_minimum;
            hf->logical_maximum  = cur_globals->logical_maximum;
            hf->physical_minimum = cur_globals->physical_minimum;
            hf->physical_maximum = cur_globals->physical_maximum;
            hf->report_size      = cur_globals->report_size;
            hf->report_count     = cur_globals->report_count;
            hf->unit_exponent    = cur_globals->unit_exponent;
            hf->unit             = cur_globals->unit;

#define UNHANDLED(F) \
   if (cur_locals->F) \
      printf("%s) Tag 0x%02x, Unimplemented: %s\n", __func__, btag, #F);

            UNHANDLED(designator_indexes)
            UNHANDLED(designator_minimum)
            UNHANDLED(designator_maximum)
            UNHANDLED(string_indexes)
            UNHANDLED(string_minimum)
            UNHANDLED(string_maximum)
            UNHANDLED(usage_minimum)
#undef UNHANDLED

            break;
         }
         case 0xc0: // End Collection
            if (collection_stack_cur == 0) {
               printf("(%s) End Collection item without corresponding Collection\n", __func__);
            }
            collection_stack_cur--;
            break;
         default:
            break;
         }   // switch(btag)

         free_cur_report_locals(cur_locals);
         cur_locals  = calloc(1, sizeof(struct cur_report_locals));
         break;

      // Global item tags

      case 0x04:     // Global item
         switch (btag) {
         case 0x04: /* Usage Page */
              cur_globals->usage_page = data;
              break;
         case 0x14:       // Logical Minimum
              cur_globals->logical_minimum = data;
              break;
         case 0x24:
              cur_globals->logical_maximum = data;
              break;
         case 0x34:
              cur_globals->physical_minimum = data;
              break;
         case 0x44:
              cur_globals->physical_maximum = data;
              break;
         case 0x54:     // Unit Exponent
              cur_globals->unit_exponent = data;                     // Global
              break;
         case 0x64:     // Unit
              cur_globals->unit = data;      // ??                   // Global
              break;
         case 0x74:
              cur_globals->report_size = data;
              break;
         case 0x84:
              cur_globals->report_id = data;
              break;
         case 0x94:
              cur_globals->report_count = data;
              break;
         case 0xa4:      // Push
         {
              Cur_Report_Globals* old_globals = cur_globals;
              cur_globals = calloc(1, sizeof(Cur_Report_Globals));
              cur_globals->prev = old_globals;
              break;
         }
         case 0xb4:     // Pop
              if (!cur_globals->prev) {
                 printf("(%s) Invalid item Pop without previous Push\n", __func__);
              }
              else {
                 Cur_Report_Globals * popped_globals = cur_globals;
                 cur_globals = cur_globals->prev;
                 free(popped_globals);
              }
              break;
         default:
              printf("(%s) Invalid global item tag: 0x%02x\n", __func__, btag);

         }   // switch(btag)
         break;


      // Local item tags

      case 0x08:     // Local item
         switch(btag) {
         case 0x08:     // Usage
           {
              if (debug)
                 printf("(%s) tag 0x08 (Usage), bSize=%d, bsize_bytect=%d, value=0x%08x %d\n",
                        __func__, bsize, bsize_bytect, data, data);

              if (cur_locals->usages == NULL)
                 cur_locals->usages = g_array_new(
                       /* null terminated */ false,
                       /* init to 0       */ true,
                       /* field size      */ sizeof(uint32_t) );
              g_array_append_val(cur_locals->usages, data);
              if (cur_locals->usages->len > 1) {
                 printf("(%s) After append, cur_locals->usages->len = %d\n", __func__,
                        cur_locals->usages->len);
              }
              if (cur_locals->usages->len == 1)
                 cur_locals->usage_bsize_bytect = bsize_bytect;
              else {
                 if (bsize_bytect != cur_locals->usage_bsize_bytect &&
                       cur_locals->usage_bsize_bytect != 0)       // avoid redundant messages
                 {
                    printf("(%s) Warning: Multiple usages for fields have different size values\n", __func__);
                    printf("     Switching to heurisitic interpretation of usage\n");
                    cur_locals->usage_bsize_bytect = 0;
                 }
              }
              break;
           }
           case 0x18:     // Usage minimum
             cur_locals->usage_minimum = data;
             break;
           case 0x28:
              cur_locals->usage_maximum = data;
              break;
           case 0x38:    // designator index
              // TODO: same as 0x08 Usage
              printf("(%s) Local item value 0x38 (Designator Index) unimplemented\n", __func__);
              break;
           case 0x48:
              cur_locals->designator_minimum = data;
              break;
           case 0x58:
              cur_locals->designator_maximum = data;
              break;
           case 0x78:           // string index
              // TODO: same as 0x08 Usage
              printf("(%s) Local item value 0x78 (String Index) unimplemented\n", __func__);
              break;
           case 0x88:
              cur_locals->string_minimum = data;
              break;
           case 0x98:
              cur_locals->string_maximum = data;
              break;
           case 0xa8:     // delimiter - defines beginning or end of set of local items
              // what to do?
              printf("(%s) Local item Delimiter unimplemented\n", __func__);
              break;
           default:
              printf("(%s) Invalid local item tax: 0x%02x\n", __func__, btag);
         }
         break;

      default:
           printf("(%s) Invalid item type: 0x%04x\n", __func__, btype);

      }

      i += 1 + bsize;
   }

   return parsed_descriptor;
}
#endif


/* Fully interpret a sequence of Hid_Report_Items
 *
 * Arguments:
 *    head          pointer to linked list of Hid_Report_Items
 *
 * Returns:         parsed report descriptor
 */
Parsed_Hid_Descriptor * parse_report_desc_from_item_list(Hid_Report_Descriptor_Item * items_head) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting.\n", __func__);

   char *types[4] = { "Main", "Global", "Local", "reserved" };

   Cur_Report_Globals * cur_globals = calloc(1, sizeof(struct cur_report_globals));
   Cur_Report_Locals  * cur_locals  = calloc(1, sizeof(struct cur_report_locals));
   Parsed_Hid_Collection * cur_collection = NULL;

   Parsed_Hid_Descriptor * parsed_descriptor = calloc(1, sizeof(Parsed_Hid_Descriptor));
   parsed_descriptor->root_collection = calloc(1,sizeof(Parsed_Hid_Collection));
   parsed_descriptor->root_collection->is_root_collection = true;

#define COLLECTION_STACK_SIZE 10
   Parsed_Hid_Collection * collection_stack[COLLECTION_STACK_SIZE];
   collection_stack[0] = parsed_descriptor->root_collection;
   int collection_stack_cur = 0;

   Hid_Report_Descriptor_Item * item = items_head;
   while(item) {

      if (debug) {
         char datastr[20];
         snprintf(datastr, 20, "[0x%0*x] %d", item->bsize_bytect*2, item->data, item->data);
         printf("(%s) Item(%-6s): %s, data=%s\n",
                 __func__,
                 types[item->btype],
                 devid_hid_descriptor_item_type(item->btag),
                 datastr);
      }


      switch (item->btype) {

      // Main item tags

      case 0:     // Main item
         switch(item->btag) {

         case 0xa0:     // Collection
         {
            cur_collection = calloc(1, sizeof(Parsed_Hid_Collection));
            cur_collection->collection_type = item->data;
            cur_collection->usage_page = cur_globals->usage_page;
            uint32_t cur_usage = 0;
            if (cur_locals->usages && cur_locals->usages->len > 0) {
               cur_usage = g_array_index(cur_locals->usages, uint32_t, 0);
               // cur_collection->usage_id = cur_usage;    // deprecated
            }
            else {
               printf("(%s) No usage id has been set for collection\n", __func__);
            }
            if (cur_usage) {
               cur_collection->extended_usage = extended_usage(
                                                cur_globals->usage_page,
                                                cur_usage,
                                                cur_locals->usage_bsize_bytect);  // or 0 to use heuristic
            }
            else {
               // what to do if there was no usage value?
               // makes no sense to combine it with usage page
               printf("(%s) Collection has no usage value\n", __func__);
            }

            cur_collection->reports = g_ptr_array_new();

            // add this collection as a child of the parent collection
            add_hid_collection_child(collection_stack[collection_stack_cur], cur_collection);
            assert(collection_stack_cur < COLLECTION_STACK_SIZE-1);
            collection_stack[++collection_stack_cur] = cur_collection;
            break;
         }

         case 0x80: /* Input */
         case 0x90: /* Output */
         case 0xb0: /* Feature */
         {
            Parsed_Hid_Field * hf = calloc(1, sizeof(Parsed_Hid_Field));
            Byte report_type;
            if      (item->btag == 0x80) report_type = HID_REPORT_TYPE_INPUT;
            else if (item->btag == 0x90) report_type = HID_REPORT_TYPE_OUTPUT;
            else                         report_type = HID_REPORT_TYPE_FEATURE;
            hf->item_flags = item->data;
            uint16_t report_id = cur_globals->report_id;
            Parsed_Hid_Report * hr = find_hid_report_or_new(
                     cur_collection,
                     report_type,
                     report_id);

            // add this item/field to current report
            add_report_field(hr, hf);

            int field_index = hr->hid_fields->len - 1;
            if (cur_locals->usages && cur_locals->usages->len > 0) {
               int usagect = cur_locals->usages->len;
               int usagendx = (field_index < usagect) ? field_index : usagect-1;
               uint32_t this_usage = g_array_index(cur_locals->usages, uint32_t, usagendx);
               hf->extended_usage = extended_usage(cur_globals->usage_page,
                                                   this_usage,
                                                   cur_locals->usage_bsize_bytect); // or 0  to use heuristic
               if (debug) {
                  printf("(%s) item 0x%02x, usagect=%d, usagendx=%d, this_usage=0x%04x\n", __func__,
                         item->btag, usagect, usagendx, this_usage);
               }
            }
            else {
               printf("(%s) Tag 0x%02x, Report id: %d: No usage values in cur_locals\n",
                     __func__, item->btag, report_id);
            }

            hf->usage_page       = cur_globals->usage_page;
            hf->logical_minimum  = cur_globals->logical_minimum;
            hf->logical_maximum  = cur_globals->logical_maximum;
            hf->physical_minimum = cur_globals->physical_minimum;
            hf->physical_maximum = cur_globals->physical_maximum;
            hf->report_size      = cur_globals->report_size;
            hf->report_count     = cur_globals->report_count;
            hf->unit_exponent    = cur_globals->unit_exponent;
            hf->unit             = cur_globals->unit;

#define UNHANDLED(F) \
   if (cur_locals->F) \
      printf("%s) Tag 0x%02x, Unimplemented: %s\n", __func__, item->btag, #F);

            UNHANDLED(designator_indexes)
            UNHANDLED(designator_minimum)
            UNHANDLED(designator_maximum)
            UNHANDLED(string_indexes)
            UNHANDLED(string_minimum)
            UNHANDLED(string_maximum)
            UNHANDLED(usage_minimum)
#undef UNHANDLED

            break;
         }
         case 0xc0: // End Collection
            if (collection_stack_cur == 0) {
               printf("(%s) End Collection item without corresponding Collection\n", __func__);
            }
            collection_stack_cur--;
            break;
         default:
            break;
         }   // switch(btag)

         free_cur_report_locals(cur_locals);
         cur_locals  = calloc(1, sizeof(struct cur_report_locals));
         break;

      // Global item tags

      case 1:     // Global item
         switch (item->btag) {
         case 0x04: /* Usage Page */
              cur_globals->usage_page = item->data;
              break;
         case 0x14:       // Logical Minimum
              cur_globals->logical_minimum = item->data;
              break;
         case 0x24:
              cur_globals->logical_maximum = item->data;
              break;
         case 0x34:
              cur_globals->physical_minimum = item->data;
              break;
         case 0x44:
              cur_globals->physical_maximum = item->data;
              break;
         case 0x54:     // Unit Exponent
              cur_globals->unit_exponent = item->data;                     // Global
              break;
         case 0x64:     // Unit
              cur_globals->unit = item->data;      // ??                   // Global
              break;
         case 0x74:
              cur_globals->report_size = item->data;
              break;
         case 0x84:
              cur_globals->report_id = item->data;
              break;
         case 0x94:
              cur_globals->report_count = item->data;
              break;
         case 0xa4:      // Push
         {
              Cur_Report_Globals* old_globals = cur_globals;
              cur_globals = calloc(1, sizeof(Cur_Report_Globals));
              cur_globals->prev = old_globals;
              break;
         }
         case 0xb4:     // Pop
              if (!cur_globals->prev) {
                 printf("(%s) Invalid item Pop without previous Push\n", __func__);
              }
              else {
                 Cur_Report_Globals * popped_globals = cur_globals;
                 cur_globals = cur_globals->prev;
                 free(popped_globals);
              }
              break;
         default:
              printf("(%s) Invalid global item tag: 0x%02x\n", __func__, item->btag);

         }   // switch(btag)
         break;


      // Local item tags

      case 2:     // Local item
         switch(item->btag) {
         case 0x08:     // Usage
           {
              if (debug)
                 printf("(%s) tag 0x08 (Usage), bsize_bytect=%d, value=0x%08x %d\n",
                        __func__, item->bsize_bytect, item->data, item->data);

              if (cur_locals->usages == NULL)
                 cur_locals->usages = g_array_new(
                       /* null terminated */ false,
                       /* init to 0       */ true,
                       /* field size      */ sizeof(uint32_t) );
              g_array_append_val(cur_locals->usages, item->data);
              if (cur_locals->usages->len > 1) {
                 printf("(%s) After append, cur_locals->usages->len = %d\n", __func__,
                        cur_locals->usages->len);
              }
              if (cur_locals->usages->len == 1)
                 cur_locals->usage_bsize_bytect = item->bsize_bytect;
              else {
                 if (item->bsize_bytect != cur_locals->usage_bsize_bytect &&
                       cur_locals->usage_bsize_bytect != 0)       // avoid redundant messages
                 {
                    printf("(%s) Warning: Multiple usages for fields have different size values\n", __func__);
                    printf("     Switching to heurisitic interpretation of usage\n");
                    cur_locals->usage_bsize_bytect = 0;
                 }
              }
              break;
           }
           case 0x18:     // Usage minimum
             cur_locals->usage_minimum = item->data;
             break;
           case 0x28:
              cur_locals->usage_maximum = item->data;
              break;
           case 0x38:    // designator index
              // TODO: same as 0x08 Usage
              printf("(%s) Local item value 0x38 (Designator Index) unimplemented\n", __func__);
              break;
           case 0x48:
              cur_locals->designator_minimum = item->data;
              break;
           case 0x58:
              cur_locals->designator_maximum = item->data;
              break;
           case 0x78:           // string index
              // TODO: same as 0x08 Usage
              printf("(%s) Local item value 0x78 (String Index) unimplemented\n", __func__);
              break;
           case 0x88:
              cur_locals->string_minimum = item->data;
              break;
           case 0x98:
              cur_locals->string_maximum = item->data;
              break;
           case 0xa8:     // delimiter - defines beginning or end of set of local items
              // what to do?
              printf("(%s) Local item Delimiter unimplemented\n", __func__);
              break;
           default:
              printf("(%s) Invalid local item tax: 0x%02x\n", __func__, item->btag);
         }
         break;

      default:
           printf("(%s) Invalid item type: 0x%04x\n", __func__, item->btype);

      }

      item = item->next;
   }

   return parsed_descriptor;
}


/* Parse and interpret the bytes of a HID report descriptor.
 *
 * Arguments:
 *    b             address of first byte
 *    desclen       number of bytes
 *
 * Returns:         parsed report descriptor
 */
Parsed_Hid_Descriptor * parse_report_desc(Byte * b, int desclen) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. b=%p, desclen=%d\n", __func__, b, desclen);

   Hid_Report_Descriptor_Item * item_list = tokenize_hid_report_descriptor(b, desclen) ;

   return parse_report_desc_from_item_list(item_list);
}


/* Indicates if a parsed HID Report Descriptor represents a USB connected monitor.
 *
 * Arguments:
 *    phd       pointer parsed HID Report Descriptor
 *
 * Returns:     true/false
 *
 * Per section 5.5 of USB Monitor Control Class Specification Rev 1.0:
 * "In order to identify a HID class device as a monitor, the device's
 * HID Report Descriptor must contain a top-level collection with a usage
 * of Monitor Control from the USB Monitor Usage Page."
 *
 * i.e. Usage page = 0x80  USB monitor
 *      Usage id   = 0x01  Monitor Control
 */
bool is_monitor_by_parsed_report_descriptor(Parsed_Hid_Descriptor * phd) {
   bool is_monitor = false;

   Parsed_Hid_Collection * root_collection = phd->root_collection;
   for (int ndx = 0; ndx < root_collection->child_collections->len; ndx++) {
      Parsed_Hid_Collection * col = g_ptr_array_index(root_collection->child_collections, ndx);
      if (col->extended_usage == (0x0080 << 16 | 0x0001) ) {
         is_monitor = true;
         break;
      }
   }

   return is_monitor;
}


