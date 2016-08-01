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

#ifndef HID_FIELD_CONSTANT
// Bits of item flags, as defined in hiddev.h
// Define them here if hiddev.h was not already included
#define HID_FIELD_CONSTANT       0x001
#define HID_FIELD_VARIABLE       0x002
#define HID_FIELD_RELATIVE       0x004
#define HID_FIELD_WRAP           0x008
#define HID_FIELD_NONLINEAR      0x010
#define HID_FIELD_NO_PREFERRED   0x020
#define HID_FIELD_NULL_STATE     0x040
#define HID_FIELD_VOLATILE       0x080
#define HID_FIELD_BUFFERED_BYTE  0x100
#endif

// values identical to those for HID_REPORT_TYPE_* in hiddev.h:
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
   uint16_t       item_flags;
   uint16_t       usage_page;
   uint32_t       extended_usage;      // hi 16 bits usage_page, lo 16 bits usage_id

   /* The meaning of logical min/max vs physical/min max is the opposite of
      what one might expect.  Logical refers to the values returned by the
      device.  Physical refers to the "real world" bounds.

      Per section 6.2.2.7 of the HID Device Class Definition v 1.11:

      While Logical Minimum and Logical Maximum (extents) bound the values
      returned by a device, Physical Minimum and Physical Maximum give
      meaning to those bounds by allowing the report value to be offset and scaled.
      For example, a thermometer might have logical extents of 0 and 999 but
      physical extents of 32 and 212 degrees.
    */

   uint16_t       logical_minimum;
   uint16_t       logical_maximum;
   uint16_t       physical_minimum;
   uint16_t       physical_maximum;
   uint16_t       report_size;
   uint16_t       report_count;
   uint16_t       unit_exponent;
   uint16_t       unit;
} Parsed_Hid_Field;

typedef struct parsed_hid_report {
   uint16_t       report_id;
   Byte           report_type;
   GPtrArray *    hid_fields;             // array of pointers to Parsed_Hid_Field
} Parsed_Hid_Report;

typedef struct parsed_hid_collection {
   uint16_t       usage_page;
   uint32_t       extended_usage;
   Byte           collection_type;
   bool           is_root_collection;
   GPtrArray *    reports;                // array of pointers to Parsed_Hid_Report
   GPtrArray *    child_collections;      // array of pointers to Parsed_Hid_Collection
} Parsed_Hid_Collection;

// #define PARSED_HID_DESCRIPTOR_ANALYZED 0x80
typedef struct parsed_hid_descriptor {
   // Byte * raw_bytes;
   // int    raw_byte_ct;
   // point to tokenized version
   Parsed_Hid_Collection * root_collection;
   // Byte   flags;
   // Parsed_Hid_Report * edid_report;
   // GPtrArray *         vcp_feature_reports;
} Parsed_Hid_Descriptor;

Parsed_Hid_Descriptor * parse_report_desc_from_item_list(Hid_Report_Descriptor_Item * items_head);
Parsed_Hid_Descriptor * parse_report_desc(Byte * b, int desclen);

void report_parsed_hid_report(Parsed_Hid_Report * hr, int depth);
void summarize_parsed_hid_report(Parsed_Hid_Report * hr, int depth);
void report_parsed_hid_descriptor(Parsed_Hid_Descriptor * pdesc, int depth);

bool is_monitor_by_parsed_report_descriptor(Parsed_Hid_Descriptor * phd);

// TODO: use same bit values as item type?   will that work?
// TODO: poor names
typedef enum hid_report_type_enum {
   HIDF_REPORT_TYPE_NONE    = 0x00,
   HIDF_REPORT_TYPE_INPUT   = 0x02,   // == 1 << HID_REPORT_TYPE_INPUT
   HIDF_REPORT_TYPE_OUTPUT  = 0x04,   // == 1 << HID_REPORT_TYPE_OUTPUT
   HIDF_REPORT_TYPE_FEATURE = 0x08,   // == 1 << HID_REPORT_TYPE_FEATURE
   HIDF_REPORT_TYPE_ANY     = 0xff
} Hid_Report_Type_Enum;

GPtrArray * select_parsed_report_descriptors(Parsed_Hid_Descriptor * phd, Byte report_type_flags);

typedef struct vcp_code_report {
   uint8_t vcp_code;
   Parsed_Hid_Report * rpt;
} Vcp_Code_Report;

void report_vcp_code_report(Vcp_Code_Report * vcr, int depth);
void report_vcp_code_report_array(GPtrArray * vcr_array,  int depth);
void summarize_vcp_code_report(Vcp_Code_Report * vcr, int depth);
void summarize_vcp_code_report_array(GPtrArray * vcr_array,  int depth);
GPtrArray * get_vcp_code_reports(Parsed_Hid_Descriptor * phd);
Parsed_Hid_Report * find_edid_report_descriptor(Parsed_Hid_Descriptor * phd);

#endif /* HID_REPORT_DESCRIPTOR_H_ */
