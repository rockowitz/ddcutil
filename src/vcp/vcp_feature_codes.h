/* vcp_feature_codes.h
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

#ifndef VCP_FEATURE_CODES_H_
#define VCP_FEATURE_CODES_H_

#include <stdio.h>

#include "util/string_util.h"

#include "base/core.h"
#include "base/ddc_packets.h"
#include "base/feature_sets.h"

#include "vcp/vcp_feature_values.h"


//
// VCP Feature Interpretation
//

// Examples of features that create need for complex data structures:
//
// x62: Audio Volume:
//    v 2.0:   C
//    v 3.0:   NC  Values x00 and xff are reserved, x01..xfe are a continuous range
// xdf: VCP Version:
//    NC, uses both high and low bytes


typedef ushort Version_Feature_Flags;
// Bits in Version_Feature_Flags:

// Exactly 1 of the following 3 bits must be set
#define  VCP2_RO             0x0400
#define  VCP2_WO             0x0200
#define  VCP2_RW             0x0100
#define  VCP2_READABLE       (VCP2_RO | VCP2_RW)
#define  VCP2_WRITABLE       (VCP2_WO | VCP2_RW)

// Further refine the MCCS C/NC/TABLE categorization
#define VCP2_STD_CONT        0x80
#define VCP2_COMPLEX_CONT    0x40
#define VCP2_CONT            (VCP2_STD_CONT|VCP2_COMPLEX_CONT)
#define VCP2_SIMPLE_NC       0x20
#define VCP2_COMPLEX_NC      0x10
// For WO NC features.  There's no interpretation function or lookup table
// Used to mark that the feature is defined for a version
#define VCP2_WO_NC           0x08
#define VCP2_NC              (VCP2_SIMPLE_NC|VCP2_COMPLEX_NC|VCP2_WO_NC)
#define VCP2_NON_TABLE       (VCP2_CONT | VCP2_NC)
#define VCP2_TABLE           0x04
#define VCP2_WO_TABLE        0x02
#define VCP2_ANY_TABLE       (VCP2_TABLE | VCP2_WO_TABLE)

// Additional bits:
#define VCP2_DEPRECATED      0x01


// Bits in vcp_global_flags:
#define VCP2_SYNTHETIC       0x80


// MCCS specification group to which feature belongs
// Note a function can appear in multiple groups, e.g. in different spec versions
// Should probably have been made version specific, but it's not worth redoing
typedef enum {
   VCP_SPEC_PRESET   = 0x80     ,  // Section 8.1 Preset Operations
   VCP_SPEC_IMAGE    = 0x40     ,  // Section 8.2 Image Adjustment
   VCP_SPEC_CONTROL  = 0x20     ,  // Section 8.3 Display Control
   VCP_SPEC_GEOMETRY = 0x10     ,  // Section 8.4 Geometry
   VCP_SPEC_MISC     = 0x08     ,  // Section 8.5 Miscellaneous Functions
   VCP_SPEC_AUDIO    = 0x04     ,  // Section 8.6 Audio Functions
   VCP_SPEC_DPVL     = 0x02     ,  // Section 8.7 DPVL Functions
   VCP_SPEC_MFG      = 0x01     ,  // Section 8.8 Manufacturer Specific
   VCP_SPEC_WINDOW   = 0x8000   ,  // Table 5 in MCCS 2.0 spec
} Vcp_Spec_Ids;


// Set these bits in a flag byte to indicate the MCCS versions for which a feature is valid
#define MCCS_V10          0x80
#define MCCS_V20          0x40
#define MCCS_V21          0x20
#define MCCS_V30          0x10
#define MCCS_V22          0x08


typedef
bool (*Format_Normal_Feature_Detail_Function) (
          Nontable_Vcp_Value*  code_info,
          Version_Spec         vcp_version,
          char *               buffer,
          int                  bufsz);

typedef
bool (*Format_Table_Feature_Detail_Function) (
          Buffer *            data_bytes,
          Version_Spec        vcp_version,
          char **             presult_buffer);

// Describes one simple NC feature value
typedef
struct {
   Byte   value_code;
   char * value_name;
} Feature_Value_Entry;

extern Feature_Value_Entry * pxc8_display_controller_type_values;

// To consider:
// In retrospect this is probably better, but not worth redoing
// typedef struct {
//    char *                 name;
//    VCP_Feature_Subset     subsets;
//    Version_Feature_Flags  flags;
//    Feature_Value_Entry *  sl_values;
// } Version_Specific_Info;

#define VCP_FEATURE_TABLE_ENTRY_MARKER "VFTE"
typedef
struct {
   char                                  marker[4];
   Byte                                  code;
   char *                                desc;
   Format_Normal_Feature_Detail_Function nontable_formatter;
   Format_Table_Feature_Detail_Function  table_formatter;
   Feature_Value_Entry *                 default_sl_values;
   Byte                                  vcp_global_flags;
   ushort                                vcp_spec_groups;
   VCP_Feature_Subset                    vcp_subsets;
   char *                                v20_name;
   char *                                v21_name;
   char *                                v30_name;
   char *                                v22_name;
   Version_Feature_Flags                 v20_flags;
   Version_Feature_Flags                 v21_flags;
   Version_Feature_Flags                 v30_flags;
   Version_Feature_Flags                 v22_flags;
   Feature_Value_Entry *                 v21_sl_values;
   Feature_Value_Entry *                 v30_sl_values;
   Feature_Value_Entry *                 v22_sl_values;
} VCP_Feature_Table_Entry;

int
vcp_get_feature_code_count();

void
free_synthetic_vcp_entry(VCP_Feature_Table_Entry * pfte);

void
report_vcp_feature_table_entry(VCP_Feature_Table_Entry * entry, int depth);

VCP_Feature_Table_Entry *
vcp_get_feature_table_entry(int ndx);

VCP_Feature_Table_Entry *
vcp_create_dummy_feature_for_hexid(Byte id);

VCP_Feature_Table_Entry *
vcp_create_table_dummy_feature_for_hexid(Byte id);

VCP_Feature_Table_Entry *
vcp_find_feature_by_hexid(Byte id);

VCP_Feature_Table_Entry *
vcp_find_feature_by_hexid_w_default(Byte id);

Feature_Value_Entry *
find_feature_values(Byte feature_code, Version_Spec vcp_version);

Feature_Value_Entry *
find_feature_values_for_capabilities(Byte feature_code, Version_Spec vcp_version);

char *
get_feature_value_name(Feature_Value_Entry * value_entries, Byte value_id);

bool
has_version_specific_features(
      VCP_Feature_Table_Entry * pvft_entry);

Version_Spec
get_highest_non_deprecated_version(
      VCP_Feature_Table_Entry * pvft_entry);

Version_Feature_Flags
get_version_specific_feature_flags(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec              vcp_version);

Version_Feature_Flags
get_version_sensitive_feature_flags(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec              vcp_version);

bool
is_feature_supported_in_version(
      VCP_Feature_Table_Entry * pvft_entry,
      Version_Spec              vcp_version);

bool
is_feature_readable_by_vcp_version(
      VCP_Feature_Table_Entry * pvft_entry,
      Version_Spec              vcp_version);

bool
is_feature_writable_by_vcp_version(
      VCP_Feature_Table_Entry * pvft_entry,
      Version_Spec              vcp_version);

bool
is_feature_table_by_vcp_version(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec vcp_version);

bool
is_version_conditional_vcp_type(VCP_Feature_Table_Entry * pvft_entry);

Feature_Value_Entry *
get_version_specific_sl_values(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec              vcp_version);

char *
get_version_sensitive_feature_name(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec              vcp_version);

char *
get_non_version_specific_feature_name(
       VCP_Feature_Table_Entry * pvft_entry);

bool
vcp_format_feature_detail(
       VCP_Feature_Table_Entry * vcp_entry,
       Version_Spec              vcp_version,
       Single_Vcp_Value *        valrec,
       char * *                  aformatted_data
     );

char*
get_feature_name_by_id_only(Byte feature_code);

char*
get_feature_name_by_id_and_vcp_version(Byte feature_code, Version_Spec vspec);

void
vcp_list_feature_codes(FILE * fh);

void
init_vcp_feature_codes();

#endif /* VCP_FEATURE_CODES_H_ */
