/* hid_report_descriptor.h
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

#ifndef HID_REPORT_DESCRIPTOR_H_
#define HID_REPORT_DESCRIPTOR_H_

#include <stdint.h>

#include "util/coredefs.h"
#include "usb_util/base_hid_report_descriptor.h"


// values identical to those for HID_REPORT_TYPE_... in hiddev.h:
#define HID_REPORT_TYPE_INPUT 1
#define HID_REPORT_TYPE_OUTPUT   2
#define HID_REPORT_TYPE_FEATURE  3
#define HID_REPORT_TYPE_MIN     1
#define HID_REPORT_TYPE_MAX     3


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

typedef struct parsed_hid_field {
   uint16_t           item_flags;
   uint16_t           usage_page;
   uint32_t           extended_usage;      // hi 16 bits usage_page, lo 16 bits usage_id
   uint16_t           logical_minimum;
   uint16_t           logical_maximum;
   uint16_t           physical_minimum;
   uint16_t           physical_maximum;
   uint16_t           report_size;
   uint16_t           report_count;
   uint16_t           unit_exponent;
   uint16_t           unit;
} Parsed_Hid_Field;

typedef struct parsed_hid_report {
   uint16_t   report_id;
   Byte       report_type;
   GPtrArray * hid_fields;
} Parsed_Hid_Report;

typedef struct parsed_hid_collection {
   uint16_t     usage_page;
   uint32_t     extended_usage;
   Byte         collection_type;
   bool         is_root_collection;
   GPtrArray *  reports;
   GPtrArray *  child_collections;
} Parsed_Hid_Collection;

typedef struct parsed_hid_descriptor {
   Parsed_Hid_Collection * root_collection;
} Parsed_Hid_Descriptor;

Parsed_Hid_Descriptor * parse_report_desc_from_item_list(Hid_Report_Descriptor_Item * items_head);
Parsed_Hid_Descriptor * parse_report_desc(Byte * b, int desclen);

void report_parsed_hid_report(Parsed_Hid_Report * hr, int depth);
void report_parsed_hid_descriptor(Parsed_Hid_Descriptor * pdesc, int depth);

bool is_monitor_by_parsed_report_descriptor(Parsed_Hid_Descriptor * phd);

// TODO: use same bit values as item type?   will that work?
// TODO: poor names
typedef enum hid_report_type_enum {
   HIDF_REPORT_TYPE_NONE    = 0x00,
   HIDF_REPORT_TYPE_INPUT   = 0x0,
   HIDF_REPORT_TYPE_OUTPUT  = 0x02,
   HIDF_REPORT_TYPE_FEATURE = 0x04,
   HIDF_REPORT_TYPE_ANY     = 0xff
} Hid_Report_Type_Enum;

GPtrArray * select_parsed_report_descriptors(Parsed_Hid_Descriptor * phd, Byte report_type_flags);

#endif /* HID_REPORT_DESCRIPTOR_H_ */
