/* vcp_feature_codes.h
 *
 * Created on: Nov 17, 2015
 *     Author: rock
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
 */

#ifndef VCP_FEATURE_CODES_H_
#define VCP_FEATURE_CODES_H_

#include "util/string_util.h"

#include "base/ddc_base_defs.h"
#include "base/ddc_packets.h"
#include "base/util.h"

//
// VCP Feature Interpretation
//

// Bits in VCP_Feature_Table_Entry.flags:

// Exactly 1 of the following 3 bits must be set
#define  VCP_RO         0x80
#define  VCP_WO         0x40
#define  VCP_RW         0x20
#define  VCP_READABLE   (VCP_RO | VCP_RW)
#define  VCP_WRITABLE   (VCP_WO | VCP_RW)

// Exactly 1 of the following 4 bits must be set
#define  VCP_CONTINUOUS 0x08
#define  VCP_NON_CONT   0x04
#define  VCP_TABLE      0x02
// a few codes have had their type change from NC to T in version 3
// for now, assume that is the only time type changed
// need to find a copy of the v2.2 MCCS spec to confirm
#define  VCP_TYPE_V2NC_V3T 0x01

// 12/2015:
// new way, defines beginning with VCP_V3_..., if set override VCP_CONTINUOUS, VCP_NON_CONT, VCP_TABLE if V3
// so VCP_TYPE_V2NC_V3T becomes VCP_NON_CONT | VCP_V3_TABLE
// for example of VCP_NON_CONT | VCP_V3_NC_COMPLEX, see 0x62 audio volume
// for example of NC using both sl and sh bytes, see 0xdf vcp version
// should also have have e.g. VCP_NC_SIMPLE, VC_NC_COMPLEX
// ..SIMPLE.. means it's just the sl byte
// ..COMPLEX.. can mean: use of multiple bytes,
//                       SL byte with some fixed values, remaining range continuous (see audio settings)
// #define  VCP_V3_CONT
// #define  VCP_V3_NC_SIMPLE_SL
// #define  VCP_V3_NC_COMPLEX
// #define  VCP_V3_TABLE

// 12/2015
// new new way
// Separate bytes for each VCP version
typedef ushort Version_Feature_Flags;
typedef Byte Global_Feature_Flags;



#define VCP2_PROFILE         0x8000
#define VCP2_COLORMGT        0x4000

// Exactly 1 of the following 3 bits must be set
#define  VCP2_RO         0x0400
#define  VCP2_WO         0x0200
#define  VCP2_RW         0x0100
#define  VCP2_READABLE   (VCP2_RO | VCP2_RW)
#define  VCP2_WRITABLE   (VCP2_WO | VCP2_RW)


#define VCP2_STD_CONT        0x80
#define VCP2_COMPLEX_CONT    0x40
#define VCP2_CONT            (VCP2_STD_CONT|VCP2_COMPLEX_CONT)
#define VCP2_SIMPLE_NC       0x20
#define VCP2_COMPLEX_NC      0x10
// For WO NC features.  There's no interpretation function or lookup table
// Used to mark that the feature is defined for a version
#define VCP2_WO_NC           0x08
#define VCP2_NC              (VCP2_SIMPLE_NC|VCP2_COMPLEX_NC|VCP2_WO_NC)
#define VCP2_TABLE           0x04
#define VCP2_WO_TABLE        0x02
#define VCP2_ANY_TABLE       (VCP2_TABLE | VCP2_WO_TABLE)
#define VCP2_DEPRECATED      0x01

// typedef Byte Global_Feature_Flags
// Global_Feature_Flags global_flags;
// RO/RW/WO same
// VCPV_PROFILE
// VCPV_COLORMGT

// n. VCP_FUNC_VER no longer needed


// 0 or more of the following group bits may be set:
#define  VCP_PROFILE    0x8000     // emit when -profile option selected
#define  VCP_COLORMGT   0x4000     // my designation, indicates related to color mgt

//(hack because vcp table fields do not initialize to 0 unless explicitly set
#define  VCP_NCSL       0x0200     // field nc_sl_values is present


// optional bit
#define  VCP_FUNC_VER   0x0100   // interpretation function needs to know version


// more generic, deprecated:
// #define  VCP_TYPE_VER 0x1000    // type (C, NC, T) varies by version

// #define  VCP_SYNTHETIC  0x0100

// MCCS specification group to which code belongs
// note a function can appear in multiple groups, e.g. in different spec versions
#define VCP_SPEC_PRESET   0x80     // Section 8.1 Preset Operations
#define VCP_SPEC_IMAGE    0x40     // Section 8.2 Image Adjustment
#define VCP_SPEC_CONTROL  0x20     // Section 8.3 Display Control
#define VCP_SPEC_GEOMETRY 0x10     // Section 8.4 Geometry
#define VCP_SPEC_MISC     0x08     // Section 8.5 Miscellaneous Functions
#define VCP_SPEC_AUDIO    0x04     // Section 8.6 Audio Functions
#define VCP_SPEC_DPVL     0x02     // Section 8.7 DPVL Functions
#define VCP_SPEC_MFG      0x01     // Section 8.8 Manufacturer Specific

// my groupings, not in spec
#define VCP_CLASS_ANALOG  0x80     // setting that only applies to analog, e.g. CRT, devices
#define VCP_CLASS_TV      0x40     // TV related setting
#define VCP_CLASS_WINDOW  0x20

// redundant but might make table more self-documenting
#define MCCS_V10          0x80
#define MCCS_V20          0x40
#define MCCS_V21          0x20
#define MCCS_V30          0x01
#define MCCS_V22          0x80



#ifdef ALTERNATIVE
// VCP_Feature_Parser is function that parses data from a VCP get feature response
typedef
int                    // return code
(*VCP_Feature_Parser) (
      Byte *  vcp_data_bytes,
      int     bytect,
      Byte    requested_vcp_code,
      void ** aux_data,           // where to store parsed data
      bool    debug);             //  debug)


// didn't work out
// VCP_Feature_Reporter parses the data returned by a VCP_Feature_Parser
typedef
void                                   // returns nothing
(*VCP_Feature_Reporter) (
      void *  interpreted_vcp_data);   // data returned by a VCP_Parser
#endif

typedef bool (*Format_Normal_Feature_Detail_Function) (
                 Interpreted_Nontable_Vcp_Response* code_info, Version_Spec vcp_version, char * buffer,  int     bufsz);

#ifdef OLD
typedef bool (*Format_Table_Feature_Detail_Function) (
                 Version_Spec vcp_version,  Buffer * data_bytes, Buffer** presult_buffer);
#endif

typedef bool (*Format_Table_Feature_Detail_Function) (
                 Buffer * data_bytes, Version_Spec vcp_version, char ** presult_buffer);


typedef ushort VCP_Feature_Flags;

typedef  struct {
   Byte   value_code;
   char * value_name;
} Feature_Value_Entry;


extern Feature_Value_Entry * pxc8_display_controller_type_values;

typedef
struct {
   Byte                                 code;
   // char *                               name;
   // VCP_Feature_Flags                    flags;
   Format_Normal_Feature_Detail_Function nontable_formatter;
   Format_Table_Feature_Detail_Function table_formatter;
   Feature_Value_Entry *                nc_sl_values;  // for NC feature where value is in SL byte

   // VCP_Feature_Parser   data_parser;
   // VCP_Feature_Reporter data_reporter;

   // new way
   Byte                                 vcp_spec_groups;
   Byte                                 vcp_classes;
   char *                               desc;
   char *                               v20_name;
   char *                               v21_name;
   char *                               v30_name;
   char *                               v22_name;
   // ushort                               global_flags;
   Version_Feature_Flags                v20_flags;
   Version_Feature_Flags                v21_flags;
   Version_Feature_Flags                v30_flags;
   Version_Feature_Flags                v22_flags;
   Feature_Value_Entry *                v21_sl_values;
   Feature_Value_Entry *                v30_sl_values;
   Feature_Value_Entry *                v22_sl_values;
} VCP_Feature_Table_Entry;


VCP_Feature_Table_Entry * vcp_get_feature_table_entry(int ndx);
VCP_Feature_Table_Entry * vcp_create_dummy_feature_for_charid(char * id);
VCP_Feature_Table_Entry * vcp_find_feature_by_hexid(Byte id);
VCP_Feature_Table_Entry * vcp_find_feature_by_hexid_w_default(Byte id);
VCP_Feature_Table_Entry * vcp_find_feature_by_charid(char * id);


Feature_Value_Entry * find_feature_values_new(Byte feature_code, Version_Spec vcp_version);
Feature_Value_Entry * find_feature_values_for_capabilities(Byte feature_code, Version_Spec vcp_version);
char * find_value_name_new(Feature_Value_Entry * value_entries, Byte value_id);

bool has_version_specific_features(
      VCP_Feature_Table_Entry * pvft_entry);
Version_Spec get_highest_non_deprecated_version(
      VCP_Feature_Table_Entry * pvft_entry);

Version_Feature_Flags get_version_specific_feature_flags(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec              vcp_version);
bool is_feature_readable_by_vcp_version(
      VCP_Feature_Table_Entry * pvft_entry,
      Version_Spec              vcp_version);
bool is_feature_writable_by_vcp_version(
      VCP_Feature_Table_Entry * pvft_entry,
      Version_Spec              vcp_version);
bool is_version_conditional_vcp_type(VCP_Feature_Table_Entry * pvft_entry);
Feature_Value_Entry * get_version_specific_sl_values(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec              vcp_version);

char * get_version_specific_feature_name(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec              vcp_version);
char * get_non_version_specific_feature_name(
       VCP_Feature_Table_Entry * pvft_entry);



Format_Normal_Feature_Detail_Function get_nontable_feature_detail_function( VCP_Feature_Table_Entry * pvft_entry);
Format_Table_Feature_Detail_Function get_table_feature_detail_function(VCP_Feature_Table_Entry * pvft_entry);

bool vcp_format_nontable_feature_detail(
        VCP_Feature_Table_Entry * vcp_entry,
        Version_Spec              vcp_version,
        Interpreted_Nontable_Vcp_Response *    code_info,
        char *                    buffer,
        int                       bufsz) ;

bool vcp_format_table_feature_detail(
       VCP_Feature_Table_Entry * vcp_entry,
       Version_Spec              vcp_version,
       Buffer *                  accumulated_value,
       char * *                  aformatted_data   // address at which to return newly allocated buffer
     );

// #define NULL_VCP_CODE (0x00)     /* used for unrecognized codes */


char * get_feature_name(Byte feature_id);
char * get_feature_name_by_id_and_vcp_version(Byte feature_id, Version_Spec vspec);
int vcp_get_feature_code_count();

void vcp_list_feature_codes();

void init_vcp_feature_codes();

#endif /* VCP_FEATURE_CODES_H_ */
