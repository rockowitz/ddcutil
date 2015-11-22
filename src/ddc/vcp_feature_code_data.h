/*
 * vcp_feature_code_data.h
 *
 *  Created on: Nov 17, 2015
 *      Author: rock
 */

#ifndef VCP_FEATURE_CODE_DATA_H_
#define VCP_FEATURE_CODE_DATA_H_

#include <util/string_util.h>

#include <base/ddc_base_defs.h>
#include <base/ddc_packets.h>
#include <base/util.h>

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

typedef bool (*Format_Feature_Detail_Function) (
                 Interpreted_Vcp_Code* code_info, Version_Spec vcp_version, char * buffer,  int     bufsz);

typedef bool (*Format_Table_Feature_Detail_Function) (
                 Version_Spec vcp_version,  Buffer * data_bytes, Buffer** presult_buffer);

typedef ushort VCP_Feature_Flags;

typedef  struct {
   Byte   value_code;
   char * value_name;
} Feature_Value_Entry;


typedef
struct {
   Byte                   code;
   char *                 name;
//   Byte                 flags;
   VCP_Feature_Flags      flags;
   Format_Feature_Detail_Function formatter;
   Format_Table_Feature_Detail_Function table_formatter;
   Feature_Value_Entry *  nc_sl_values;  // for NC feature where value is in SL byte
// VCP_Feature_Parser   data_parser;
// VCP_Feature_Reporter data_reporter;
} VCP_Feature_Table_Entry;


VCP_Feature_Table_Entry * vcp_get_feature_table_entry(int ndx);
VCP_Feature_Table_Entry * vcp_create_dummy_feature_for_charid(char * id);
VCP_Feature_Table_Entry * vcp_find_feature_by_hexid(Byte id);
VCP_Feature_Table_Entry * vcp_find_feature_by_hexid_w_default(Byte id);
VCP_Feature_Table_Entry * vcp_find_feature_by_charid(char * id);


bool format_feature_detail_debug_continuous(
        Interpreted_Vcp_Code * code_info,  Version_Spec vcp_version, char * buffer, int bufsz);

extern VCP_Feature_Table_Entry vcp_code_table[];




Feature_Value_Entry * find_feature_values_new(Byte feature_code, Version_Spec vcp_version);
Feature_Value_Entry * find_feature_values_for_capabilities(Byte feature_code, Version_Spec vcp_version);
char * find_value_name_new(Feature_Value_Entry * value_entries, Byte value_id);


Format_Feature_Detail_Function get_nontable_feature_detail_function( VCP_Feature_Table_Entry * pvft_entry);

Format_Table_Feature_Detail_Function get_table_feature_detail_function(VCP_Feature_Table_Entry * pvft_entry);

bool vcp_format_nontable_feature_detail(
        VCP_Feature_Table_Entry * vcp_entry,
        Version_Spec              vcp_version,
        Interpreted_Vcp_Code *    code_info,
        char *                    buffer,
        int                       bufsz) ;

bool vcp_format_table_feature_detail(
       VCP_Feature_Table_Entry * vcp_entry,
       Version_Spec              vcp_version,
       Buffer *                  accumulated_value,
       Buffer * *                aformatted_data   // address at which to return newly allocated buffer
     );

// #define NULL_VCP_CODE (0x00)     /* used for unrecognized codes */


char * get_feature_name(Byte feature_id);
int vcp_get_feature_code_count();



void vcp_list_feature_codes();

void init_vcp_feature_codes();



#endif /* VCP_FEATURE_CODE_DATA_H_ */
