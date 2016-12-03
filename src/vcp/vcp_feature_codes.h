/* vcp_feature_codes.h
 *
 * Contains tables describing VCP feature codes, and functions to interpret
 * those tables.
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

#include "public/ddcutil_types.h"

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
// #define MCCS_V10          0x01
// #define MCCS_V20          0x02
// #define MCCS_V21          0x04
// #define MCCS_V30          0x08
// #define MCCS_V22          0x10



typedef
bool (*Format_Normal_Feature_Detail_Function) (
          Nontable_Vcp_Value*     code_info,
          DDCA_MCCS_Version_Spec  vcp_version,
          char *                  buffer,
          int                     bufsz);

typedef
bool (*Format_Table_Feature_Detail_Function) (
          Buffer *                data_bytes,
          DDCA_MCCS_Version_Spec  vcp_version,
          char **                 p_result_buffer);



extern DDCA_Feature_Value_Entry * pxc8_display_controller_type_values;

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
   Byte                                  vcp_global_flags;
   ushort                                vcp_spec_groups;
   VCP_Feature_Subset                    vcp_subsets;
   char *                                v20_name;
   char *                                v21_name;
   char *                                v30_name;
   char *                                v22_name;
   DDCA_Version_Feature_Flags            v20_flags;
   DDCA_Version_Feature_Flags            v21_flags;
   DDCA_Version_Feature_Flags            v30_flags;
   DDCA_Version_Feature_Flags            v22_flags;
   DDCA_Feature_Value_Entry *            default_sl_values;
   DDCA_Feature_Value_Entry *            v21_sl_values;
   DDCA_Feature_Value_Entry *            v30_sl_values;
   DDCA_Feature_Value_Entry *            v22_sl_values;
} VCP_Feature_Table_Entry;


//
// Functions that return or destroy a VCP_Feature_Table_Entry
//

void
free_synthetic_vcp_entry(
      VCP_Feature_Table_Entry * vfte);

VCP_Feature_Table_Entry *
vcp_get_feature_table_entry(int ndx);

VCP_Feature_Table_Entry *
vcp_create_dummy_feature_for_hexid(
      VCP_Feature_Code id);

VCP_Feature_Table_Entry *
vcp_create_table_dummy_feature_for_hexid(
      VCP_Feature_Code id);

VCP_Feature_Table_Entry *
vcp_find_feature_by_hexid(
      VCP_Feature_Code id);

VCP_Feature_Table_Entry *
vcp_find_feature_by_hexid_w_default(
      VCP_Feature_Code id);

//
// Functions to extract information from a VCP_Feature_Table_Entry
//

bool
has_version_specific_features(
       VCP_Feature_Table_Entry *  vfte);

DDCA_MCCS_Version_Spec
get_highest_non_deprecated_version(
      VCP_Feature_Table_Entry *  vfte);

bool
is_version_conditional_vcp_type(
      VCP_Feature_Table_Entry * vfte);

bool
is_feature_supported_in_version(
      VCP_Feature_Table_Entry *  vfte,
      DDCA_MCCS_Version_Spec     vcp_version);

bool
is_feature_readable_by_vcp_version(
      VCP_Feature_Table_Entry *  vfte,
      DDCA_MCCS_Version_Spec     vcp_version);

bool
is_feature_writable_by_vcp_version(
      VCP_Feature_Table_Entry *  vfte,
      DDCA_MCCS_Version_Spec     vcp_version);

bool
is_feature_table_by_vcp_version(
      VCP_Feature_Table_Entry *  vfte,
      DDCA_MCCS_Version_Spec     vcp_version);


DDCA_Version_Feature_Flags
get_version_specific_feature_flags(
      VCP_Feature_Table_Entry *  vfte,
      DDCA_MCCS_Version_Spec     vcp_version);

DDCA_Version_Feature_Flags
get_version_sensitive_feature_flags(
      VCP_Feature_Table_Entry *  vfte,
      DDCA_MCCS_Version_Spec     vcp_version);


DDCA_Feature_Value_Entry *
get_version_specific_sl_values(
      VCP_Feature_Table_Entry * vfte,
      DDCA_MCCS_Version_Spec    vcp_version);

DDCA_Feature_Value_Entry *
get_version_sensitive_sl_values(
      VCP_Feature_Table_Entry * vfte,
      DDCA_MCCS_Version_Spec    vcp_version);

char *
get_version_sensitive_feature_name(
      VCP_Feature_Table_Entry * vfte,
      DDCA_MCCS_Version_Spec    vcp_version);


char *
get_version_specific_feature_name(
      VCP_Feature_Table_Entry * vfte,
      DDCA_MCCS_Version_Spec    vcp_version);

char *
get_non_version_specific_feature_name(
       VCP_Feature_Table_Entry * pvft_entry);

//
// Functions that query the feature table by VCP feature code
//

char*
get_feature_name_by_id_only(
      VCP_Feature_Code           feature_code);

char*
get_feature_name_by_id_and_vcp_version(
      VCP_Feature_Code           feature_code,
      DDCA_MCCS_Version_Spec     vspec);

Version_Feature_Info *
get_version_specific_feature_info(
      VCP_Feature_Code           feature_code,
      bool                       with_default,
      // DDCT_MCCS_Version_Spec  vspec,
      DDCA_MCCS_Version_Id       mccs_version_id);

Version_Feature_Info *
get_version_sensitive_feature_info(
      VCP_Feature_Code       feature_code,
      bool                   with_default,
   // DDCT_MCCS_Version_Spec vspec,
      DDCA_MCCS_Version_Id   mccs_version_id);

DDCA_Feature_Value_Entry *
find_feature_values(
      VCP_Feature_Code       feature_code,
      DDCA_MCCS_Version_Spec vcp_version);

DDCA_Feature_Value_Entry *
find_feature_values_for_capabilities(
      VCP_Feature_Code       feature_code,
      DDCA_MCCS_Version_Spec vcp_version);

//
// Report Functions
//

void
report_vcp_feature_table_entry(
      VCP_Feature_Table_Entry * vfte,
      int                       depth);

void report_version_feature_info(
      Version_Feature_Info * info, int depth);

void
vcp_list_feature_codes(FILE * fh);


//
// Miscellaneous Functions
//


char *
get_feature_value_name(
      DDCA_Feature_Value_Entry * value_entries,
      Byte                       value_id);


bool
vcp_format_feature_detail(
       VCP_Feature_Table_Entry * vcp_entry,
       DDCA_MCCS_Version_Spec    vcp_version,
       Single_Vcp_Value *        valrec,
       char * *                  aformatted_data
     );

int
vcp_get_feature_code_count();


void
init_vcp_feature_codes();

#endif /* VCP_FEATURE_CODES_H_ */
