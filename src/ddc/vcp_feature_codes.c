/* vcp_feature_code_data.c
 *
 * Created on: Nov 17, 2015
 *     Author: rock
 *
 * VCP Feature Code Table and the functions it references
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

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/string_util.h"

#include "base/msg_control.h"

#include "ddc/vcp_feature_codes.h"


// Forward references
int vcp_feature_code_count;
VCP_Feature_Table_Entry vcp_code_table[];
static Feature_Value_Entry x14_color_preset_absolute_values[];
       Feature_Value_Entry xc8_display_controller_type_values[];
bool default_table_feature_detail_function(Buffer * data, Version_Spec vcp_version, char** presult);
bool format_feature_detail_debug_continuous(
         Interpreted_Nontable_Vcp_Response * code_info,  Version_Spec vcp_version, char * buffer, int bufsz);


//
// Functions applicable to VCP_Feature_Table as a whole
//

// Creates humanly readable interpretation of VCP feature flags.
//
// The result is returned in a buffer supplied by the caller.
static char * vcp_interpret_feature_flags(VCP_Feature_Flags flags, char* buf, int buflen) {
   // DBGMSG("flags: 0x%04x", flags);
   char * rwmsg = "";
   if (flags & VCP_RO)
      rwmsg = "ReadOnly ";
   else if (flags & VCP_WO)
      rwmsg = "WriteOnly";
   else if (flags & VCP_RW)
      rwmsg = "ReadWrite";

   char * typemsg = "";
   // NEED TO ALSO HANDLE TABLE TYPE
   if (flags & VCP_CONTINUOUS)
      typemsg = "Continuous";
   else if (flags & VCP_NON_CONT)
      typemsg = "Non-continuous";
   else if (flags & VCP_TABLE)
      typemsg = "Table";
   else if (flags & VCP_TYPE_V2NC_V3T)
      typemsg = "V2:NC, V3:Table";
   else
      typemsg = "Type not set";

   char * vermsg = "";
   if (flags & VCP_FUNC_VER)
      vermsg = " (Version specific interpretation)";

   snprintf(buf, buflen, "%s  %s%s", rwmsg, typemsg, vermsg);
   return buf;
}


void vcp_list_feature_codes() {
   printf("Recognized VCP feature codes:\n");
   char buf[200];
   int ndx = 0;
   for (;ndx < vcp_feature_code_count; ndx++) {
      VCP_Feature_Table_Entry entry = vcp_code_table[ndx];
      // DBGMSG("code=0x%02x, flags: 0x%04x", entry.code, entry.flags);
      printf("  %02x - %-40s  %s\n",
             entry.code,
             entry.name,
             vcp_interpret_feature_flags(entry.flags, buf, 200)
            );
   }
}



//
// Miscellaneous VCP_Feature_Table lookup functions
//

char * get_feature_name(Byte feature_id) {
   char * result = NULL;
   VCP_Feature_Table_Entry * vcp_entry = vcp_find_feature_by_hexid(feature_id);
   if (vcp_entry)
      result = vcp_entry->name;
   else if (0xe0 <= feature_id && feature_id <= 0xff)
      result = "manufacturer specific feature";
   else
      result = "unrecognized feature";
   return result;
}

int vcp_get_feature_code_count() {
   return vcp_feature_code_count;
}


//
// Functions that return a function for formatting a feature value
//


// Functions that lookup a value contained in a VCP_Feature_Table_Entry,
// returning a default if the value is not set for that entry.

Format_Feature_Detail_Function get_nontable_feature_detail_function( VCP_Feature_Table_Entry * pvft_entry) {
   assert(pvft_entry != NULL);

   // TODO:
   // if VCP_V2NC_V3T, then get version id
   // based on version id, choose .formatter or .formatter_v3
   // NO - test needs to be set in caller, this must return a Format_Feature_Detail_Function, which is not for Table

   Format_Feature_Detail_Function func = pvft_entry->formatter;
   if (!func)
      func = format_feature_detail_debug_continuous;
   return func;
}



Format_Table_Feature_Detail_Function get_table_feature_detail_function( VCP_Feature_Table_Entry * pvft_entry) {
   assert(pvft_entry != NULL);

   // TODO:
   // if VCP_V2NC_V3T, then get version id
   // based on version id, choose .formatter or .formatter_v3
   // NO - test needs to be set in caller, this must return a Format_Feature_Detail_Function, which is not for Table

   Format_Table_Feature_Detail_Function func = pvft_entry->table_formatter;
   if (!func)
      func = default_table_feature_detail_function;
   return func;
}


// Functions that apply formatting

bool vcp_format_nontable_feature_detail(
        VCP_Feature_Table_Entry * vcp_entry,
        Version_Spec              vcp_version,
        Interpreted_Nontable_Vcp_Response *    code_info,
        char *                    buffer,
        int                       bufsz)
{
   Format_Feature_Detail_Function ffd_func = get_nontable_feature_detail_function(vcp_entry);
   ffd_func(code_info, vcp_version,  buffer, bufsz);
   return true;
}

bool vcp_format_table_feature_detail(
       VCP_Feature_Table_Entry * vcp_entry,
       Version_Spec              vcp_version,
       Buffer *                  accumulated_value,
       char * *                  aformatted_data
     )
{
   Format_Table_Feature_Detail_Function ffd_func = get_table_feature_detail_function(vcp_entry);
   bool ok = ffd_func(accumulated_value, vcp_version, aformatted_data);
   return ok;
}


//
// Functions that return a VCP_Feature_Table_Entry
//

/* Returns an entry in the VCP feature table based on its index in the table.
 *
 * Arguments:
 *    ndx     table index
 *
 * Returns:
 *    VCP_Feature_Table_Entry
 */
VCP_Feature_Table_Entry * vcp_get_feature_table_entry(int ndx) {
   // DBGMSG("ndx=%d, vcp_code_count=%d  ", ndx, vcp_code_count );
   assert( 0 <= ndx && ndx < vcp_feature_code_count);
   return &vcp_code_table[ndx];
}


/* Creates a dummy VCP feature table entry for a feature code.
 * It is the responsibility of the caller to free this memory.
 *
 * Arguments:
 *    id     feature id
 *
 * Returns:
 *   created VCP_Feature_Table_Entry
 */
static VCP_Feature_Table_Entry * vcp_create_dummy_feature_for_hexid(Byte id) {
   // memory leak
   VCP_Feature_Table_Entry* pentry = calloc(1, sizeof(VCP_Feature_Table_Entry) );
   pentry->code = id;
   if (id >= 0xe0)
      pentry->name = "Manufacturer Specific";
   else
      pentry->name = "Unknown feature";
   // VCP_SYNTHETIC => caller should free
   pentry->flags = VCP_READABLE;    // so readability tests pass
   pentry->formatter = format_feature_detail_debug_continuous;
   return pentry;
}


/* Creates a dummy VCP feature table entry for a feature code,
 * based on a a character string representation of the code.
 * It is the responsibility of the caller to free this memory.
 *
 * Arguments:
 *    id     feature id, as character string
 *
 * Returns:
 *   created VCP_Feature_Table_Entry
 *   NULL if id does not consist of 2 hex characters
 */
VCP_Feature_Table_Entry * vcp_create_dummy_feature_for_charid(char * id) {
   VCP_Feature_Table_Entry * result = NULL;
   Byte hexId;
   bool ok = hhs_to_byte_in_buf(id, &hexId);
   if (!ok) {
      DBGMSG("Invalid feature code: %s", id);
   }
   else {
      result = vcp_create_dummy_feature_for_hexid(hexId);
   }
   // DBGMSG("Returning %p", result);
   return result;
}


/* Returns an entry in the VCP feature table based on the hex value
 * of its feature code.
 *
 * Arguments:
 *    id    feature id
 *
 * Returns:
 *    VCP_Feature_Table_Entry, NULL if not found
 */
VCP_Feature_Table_Entry * vcp_find_feature_by_hexid(Byte id) {
   // DBGMSG("Starting. id=0x%02x ", id );
   int ndx = 0;
   VCP_Feature_Table_Entry * result = NULL;

   for (;ndx < vcp_feature_code_count; ndx++) {
      if (id == vcp_code_table[ndx].code) {
         result = &vcp_code_table[ndx];
         break;
      }
   }
   // DBGMSG("Done.  ndx=%d. returning %p", ndx, result);
   return result;
}


/* Returns an entry in the VCP feature table based on the character
 * string representation of its feature code.
 *
 * Arguments:
 *    id    feature id
 *
 * Returns:
 *    VCP_Feature_Table_Entry
 *    NULL if id does not consist of 2 hex characters, or feature code not found
 */
VCP_Feature_Table_Entry * vcp_find_feature_by_charid(char * id) {
   bool debug = false;
   if (debug)
      DBGMSG("Starting id=|%s|  ", id );
   VCP_Feature_Table_Entry * result = NULL;

   Byte hexId;
   bool ok = hhs_to_byte_in_buf(id, &hexId);
   if (!ok) {
      if (debug)
         DBGMSG("Invalid feature code: %s", id);
   }
   else {
      result = vcp_find_feature_by_hexid(hexId);
   }
   if (debug)
      DBGMSG("Returning %p", result);
   return result;
}


/* Returns an entry in the VCP feature table based on the hex value
 * of its feature code. If the entry is not found, a synthetic entry
 * is generated.  It is the responsibility of the caller to free this
 * entry.
 *
 * Arguments:
 *    id    feature id
 *
 * Returns:
 *    VCP_Feature_Table_Entry
 */
VCP_Feature_Table_Entry * vcp_find_feature_by_hexid_w_default(Byte id) {
   // DBGMSG("Starting. id=0x%02x ", id );
   VCP_Feature_Table_Entry * result = vcp_find_feature_by_hexid(id);
   if (!result)
      result = vcp_create_dummy_feature_for_hexid(id);
   // DBGMSG("Done.  ndx=%d. returning %p", ndx, result);
   return result;
}


//
// Value formatting functions for use with table features when we don't
// understand how to interpret the values for a feature.
//

bool default_table_feature_detail_function(Buffer * data, Version_Spec vcp_version, char ** presult) {
   DBGMSG("vcp_version=%d.%d", vcp_version.major, vcp_version.minor);
   int hexbufsize = buffer_length(data) * 3;
   char * result_buf = calloc(hexbufsize,1);

   char space = ' ';
   hexstring2(data->bytes, data->len, &space, false /* upper case */, result_buf, hexbufsize);
   *presult = result_buf;
   return true;
}

//
// Functions applicable to multiple table feature codes
//

// none so far


//
// Functions for specific table VCP Feature Codes
//

// x73
bool format_feature_detail_x73_lut_size(
        Buffer *      data_bytes,
        Version_Spec  vcp_version,
        char **       pformatted_result)
{
   bool ok = true;
   if (data_bytes->len != 9) {
      DBGMSG("Expected 9 byte response.  Actual response:");
      hex_dump(data_bytes->bytes, data_bytes->len);
      ok = default_table_feature_detail_function(data_bytes, vcp_version, pformatted_result);
   }
   else {
      Byte * bytes = data_bytes->bytes;
      ushort red_entry_ct   = bytes[0] << 8 | bytes[1];
      ushort green_entry_ct = bytes[2] << 8 | bytes[3];
      ushort blue_entry_ct  = bytes[4] << 8 | bytes[5];
      int    red_bits_per_entry   = bytes[6];
      int    green_bits_per_entry = bytes[7];
      int    blue_bits_per_entry  = bytes[8];
      char buf[200];
      snprintf(buf, sizeof(buf),
               "Number of entries: %d red, %d green, %d blue,  Bits per entry: %d red, %d green, %d blue",
               red_entry_ct,       green_entry_ct,       blue_entry_ct,
               red_bits_per_entry, green_bits_per_entry, blue_bits_per_entry);
      *pformatted_result = strdup(buf);
   }
   return ok;
}



//
// Functions for interpreting non-continuous features whose values are
// stored in the SL byte
//


Feature_Value_Entry * find_feature_values_new(Byte feature_code, Version_Spec vcp_version) {
   bool debug = false;
   if (debug)
      DBGMSG("Starting. feature_code=0x%02x", feature_code);
   Feature_Value_Entry * result = NULL;
   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
   // may not be found if called for capabilities and it's a mfg specific code
   if (pentry) {
      // TODO:
      // if VCP_V2NC_V3T, check version id, if V3 return NULL
      //
      // could add a VCP feature table entry for v3 version values a distinguished from V2

      if (pentry->flags & VCP_NCSL) {
         assert(pentry->nc_sl_values);
         result = pentry->nc_sl_values;
      }
   }
   if (debug)
      DBGMSG("Starting. feature_code=0x%02x. Returning: %p", feature_code, result);
   return result;
}


// hack to handle x14, where the sl values are not stored in the vcp feature table
Feature_Value_Entry * find_feature_values_for_capabilities(Byte feature_code, Version_Spec vcp_version) {
   bool debug = false;
   if (debug)
      DBGMSG("Starting. feature_code=0x%02x", feature_code);
   // ugh .. need to know the version number here
   // for now just assume vcp version < 3, return the table for v2

   Feature_Value_Entry * result = NULL;
   if (feature_code == 0x14) {
      if (vcp_version.major < 3)
         result = x14_color_preset_absolute_values;
      else {
         SEVEREMSG("Unimplemented: x14 lookup when vcp version >= 3");
      }
   }
   else {
      // returns NULL if feature_code not found, which would be the case, e.g., for a
      // manufacturer specific code
      result = find_feature_values_new(feature_code, vcp_version);
   }

   if (debug)
      DBGMSG("Starting. feature_code=0x%02x. Returning: %p", feature_code, result);
   return result;
}


/* Given a hex value to be interpreted and an array of value table entries,
 * return the explanation string for value.
 *
 * Arguments:
 *    value_entries   array of Feature_Value_Entry
 *    value_id        value to look up
 *
 * Returns:
 *    explanation string from the Feature_Value_Entry found,
 *    NULL if not found
 */
char * find_value_name_new(Feature_Value_Entry * value_entries, Byte value_id) {
   // DBGMSG("Starting. pvalues_for_feature=%p, value_id=0x%02x", pvalues_for_feature, value_id);
   char * result = NULL;
   Feature_Value_Entry *  cur_value = value_entries;
   while (cur_value->value_name != NULL) {
      // DBGMSG("value_code=0x%02x, value_name = %s", cur_value->value_code, cur_value->value_name);
      if (cur_value->value_code == value_id) {
         result = cur_value->value_name;
         // DBGMSG("Found");
         break;
      }
      cur_value++;
   }
   return result;
}


/* Given the ids for a feature code and a SL byte value,
 * return the explanation string for value.
 *
 * The VCP version is also passed, given that for a few
 * features the version 3 values are not a strict superset
 * of the version 2 values.
 *
 * Arguments:
 *    feature_code    VCP feature code
 *    vcp_version     VCP version
 *    value_id        value to look up
 *
 * Returns:
 *    explanation string, or "Invalid value" if value_id not found
 */
char * lookup_value_name_new(
          Byte          feature_code,
          Version_Spec  vcp_version,
          Byte          sl_value) {
   Feature_Value_Entry * values_for_feature = find_feature_values_new(feature_code, vcp_version);
   assert(values_for_feature);
   char * name = find_value_name_new(values_for_feature, sl_value);
   if (!name)
      name = "Invalid value";
   return name;
}


//
// Value formatting functions for use with non-table features when we don't
// understand how to interpret the values for a feature.
//

// used when the value is calculated using the SL and SH bytes, but we haven't
// written a full interpretation function
bool format_feature_detail_debug_sl_sh(
        Interpreted_Nontable_Vcp_Response * code_info,  Version_Spec vcp_version, char * buffer, int bufsz)
{
    snprintf(buffer, bufsz,
             "SL: 0x%02x ,  SH: 0x%02x",
             code_info->sl,
             code_info->sh);
   return true;
}


// For debugging features marked as Continuous
// Outputs both the byte fields and calculated cur and max values
bool format_feature_detail_debug_continuous(
        Interpreted_Nontable_Vcp_Response * code_info,  Version_Spec vcp_version, char * buffer, int bufsz)
{
   snprintf(buffer, bufsz,
            "mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x, max value = %5d, cur value = %5d",
            code_info->mh,        code_info->ml,
            code_info->sh,        code_info->sl,
            code_info->max_value, code_info->cur_value);
   return true;
}


bool format_feature_detail_debug_bytes(
        Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   snprintf(buffer, bufsz,
            "mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x",
            code_info->mh, code_info->ml, code_info->sh, code_info->sl);
   return true;
}


//
// Functions applicable to multiple non-table feature codes
//

// used when the value is just the SL byte, but we haven't
// written a full interpretation function
bool format_feature_detail_sl_byte(
        Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
    snprintf(buffer, bufsz,
             "Value: 0x%02x" ,
             code_info->sl);
   return true;
}


/* Formats the value of a non-continuous feature whose value is returned in byte SL.
 * The names of possible values is stored in a value list in the feature table entry
 * for the feature.
 *
 * Arguments:
 *    code_info   parsed feature data
 *    vcp_version VCP version
 *    buffer      buffer in which to store output
 *    bufsz       buffer size
 *
 * Returns:
 *    true if formatting successful, false if not
 */
bool format_feature_detail_sl_lookup_new(
        Interpreted_Nontable_Vcp_Response *  code_info,
        Version_Spec            vcp_version,
        char *                  buffer,
        int                     bufsz)
{
   char * s = lookup_value_name_new(code_info->vcp_code, vcp_version, code_info->sl);
   snprintf(buffer, bufsz,"%s (sl=0x%02x)", s, code_info->sl);
   return true;
}


/* Standard feature detail formatting function for a feature marked
 * as Continuous.
 *
 * Arguments:
 *    code_info
 *    vcp_version
 *    buffer        location where to return formatted value
 *    bufsz         size of buffer
 *
 * Returns:
 *    true
 */
bool format_feature_detail_standard_continuous(
        Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   // TODO: calculate cv, mv here from bytes
   int cv = code_info->cur_value;
   int mv = code_info->max_value;
   // if (msgLevel == TERSE)
   // printf("VCP %02X %5d\n", vcp_code, cv);
   // else
   snprintf(buffer, bufsz,
            "current value = %5d, max value = %5d",
            cv, mv); // code_info->cur_value, code_info->max_value);)
   return true;
}


//
// Functions for specific non-table VCP Feature Codes
//

// 0x02
bool format_feature_detail_new_control_value(    // 0x02
        Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   char * name = NULL;
   switch(code_info->sl) {
      case 0x01: name = "No new control values";                            break;
      case 0x02: name = "One or more new control values have been saved";   break;
      case 0xff: name = "No user controls are present";                       break;
      default:   name = "<reserved code, must be ignored>";
   }
   snprintf(buffer, bufsz,
            "%s (0x%02x)" ,
            name, code_info->sl);
   // perhaps set to false if a reserved code
   return true;
}


// 0x14
bool format_feature_detail_select_color_preset(
      Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   bool debug = false;
   if (debug)
      DBGMSG("vcp_version=%d.%d", vcp_version.major, vcp_version.minor);

   char buf0[100];
   bool ok = true;
   char * mh_msg = NULL;
   Byte mh = code_info->mh;
   if (mh == 0x00)
      mh_msg = "No tolerance specified";
   else if (mh >= 0x0b) {
      mh_msg = "Invalid tolerance";
      ok = false;
   }
   else {
      snprintf(buf0, 100, "Tolerance: %d%%", code_info->mh);
      mh_msg = buf0;
   }

   // char buf2[100];
   char * sl_msg = NULL;
   Byte sl = code_info->sl;
   if (sl == 0x00 || sl >= 0xe0) {
      sl_msg = "Invalid SL value.";
      ok = false;
   }
   else if (vcp_version.major < 3 || code_info->mh == 0x00) {
      sl_msg = find_value_name_new(x14_color_preset_absolute_values, code_info->sl);
      if (!sl_msg) {
         sl_msg = "Invalid SL value";
         ok = false;
      }
#ifdef OLD
      switch (code_info->sl) {
      case 0x01:  sl_msg = "sRGB";             break;
      case 0x02:  sl_msg = "Display Native";   break;
      case 0x03:  sl_msg = "4000 K";           break;
      case 0x04:  sl_msg = "5000 K";           break;
      case 0x05:  sl_msg = "6500 K";           break;
      case 0x06:  sl_msg = "7500 K";           break;
      case 0x07:  sl_msg = "8200 K";           break;
      case 0x08:  sl_msg = "9300 K";           break;
      case 0x09:  sl_msg = "10000 K";           break;
      case 0x0a:  sl_msg = "11500 K";           break;
      case 0x0b:  sl_msg = "User 1";           break;
      case 0x0c:  sl_msg = "User 2";           break;
      case 0x0d:  sl_msg = "User 3";           break;
      default:    sl_msg = "Invalid SL value"; ok = false;
#endif

   }
   else {
      switch (code_info->sl) {
      case 0x01:  sl_msg = "sRGB";             break;
      case 0x02:  sl_msg = "Display Native";   break;
      case 0x03:  sl_msg = "-4 relative warmer";           break;
      case 0x04:  sl_msg = "-3 relative warmer";           break;
      case 0x05:  sl_msg = "-2 relative warmer";           break;
      case 0x06:  sl_msg = "-1 relative warmer";           break;
      case 0x07:  sl_msg = "+1 relative cooler";           break;
      case 0x08:  sl_msg = "+2 relative cooler";           break;
      case 0x09:  sl_msg = "+3 relative cooler";           break;
      case 0x0a:  sl_msg = "+4 relative cooler";           break;
      case 0x0b:  sl_msg = "User 1";           break;
      case 0x0c:  sl_msg = "User 2";           break;
      case 0x0d:  sl_msg = "User 3";           break;
      default:    sl_msg = "Invalid SL value"; ok = false;
      }
   }

   if (vcp_version.major < 3) {
   snprintf(buffer, bufsz,
            "Setting: %s (0x%02x)",
            sl_msg, sl);
   }
   else {
      snprintf(buffer, bufsz,
               "Setting: %s (0x%02x), %s (0x%02x)",
               sl_msg, sl, mh_msg, mh);

   }
   return ok;
}


// 0xc0
bool format_feature_detail_display_usage_time(
        Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert (code_info->vcp_code == 0xc0);
   uint usage_time;
   // DBGMSG("vcp_version=%d.%d", vcp_version.major, vcp_version.minor);

   // TODO: Control with Output_Level
   // v2 spec says this is a 2 byte value, says nothing about mh, ml
   if (vcp_version.major >= 3) {
      if (code_info->mh != 0x00) {
         printf("(%s) Data error.  Mh byte = 0x%02x, should 0x00 for display usage time  \n",
                    __func__, code_info->mh );
      }
      usage_time = (code_info->ml << 16) | (code_info->sh << 8) | (code_info->sl);
   }
   else
      usage_time = (code_info->sh << 8) | (code_info->sl);
   snprintf(buffer, bufsz,
            "Usage time (hours) = %d (0x%06x) mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x",
            usage_time, usage_time, code_info->mh, code_info->ml, code_info->sh, code_info->sl);
   return true;
}


// 0xc8
bool format_feature_detail_display_controller_type(
        Interpreted_Nontable_Vcp_Response * info,  Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert(info->vcp_code == 0xc8);
   bool ok = true;
   Byte mfg_id = info->sl;
   char *sl_msg = NULL;
   sl_msg = find_value_name_new(xc8_display_controller_type_values, info->sl);
   if (!sl_msg) {
      sl_msg = "Invalid SL value";
      ok = false;
   }

   // ushort controller_number = info->ml << 8 | info->sh;
   // spec is inconsistent, controller number can either be ML/SH or MH/ML
   // observation suggests it's ml and sh
   snprintf(buffer, bufsz,
            "Mfg: %s (sl=0x%02x), controller number: mh=0x%02x, ml=0x%02x, sh=0x%02x",
            sl_msg, mfg_id, info->mh, info->ml, info->sh);
   return ok;
}

// xc9, xdf
bool format_feature_detail_version(
        Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   int version_number  = code_info->sh;
   int revision_number = code_info->sl;
   snprintf(buffer, bufsz, "%d.%d", version_number, revision_number);
   return true;
}


//
// SL byte value lookup tables
//

// 0x1e, 0x1f
static Feature_Value_Entry x1e_x1f_auto_setup_values[] = {
      {0x00, "Auto setup not active"},
      {0x01, "Performing auto setup"},
      {0x02, "Enable continuous/periodic auto setup"},
      {0x00,  NULL}       // end of list marker, 0x00 might be a valid value, but NULL never is
};

// 0x14
static Feature_Value_Entry x14_color_preset_absolute_values[] = {
     {0x01, "sRGB"},
     {0x02, "Display Native"},
     {0x03, "4000 K"},
     {0x04, "5000 K"},
     {0x05, "6500 K"},
     {0x06, "7500 K"},
     {0x07, "8200 K"},
     {0x08, "9300 K"},
     {0x09, "10000 K"},
     {0x0a, "11500 K"},
     {0x0b, "User 1"},
     {0x0c, "User 2"},
     {0x0d, "User 3"},
     {0x00,  NULL}       // end of list marker, 0x00 might be a valid value, but NULL never is
};

// 0x60: These are MCCS V2 values.   In V3, x60 is type table.
Feature_Value_Entry x60_v2_input_source_values[] = {
      {0x01,  "VGA-1"},
      {0x02,  "VGA-2"},
      {0x03,  "DVI-1"},
      {0x04,  "DVI-2"},
      {0x05,  "Composite video 1"},
      {0x06,  "Composite video 2"},
      {0x07,  "S-Video-1"},
      {0x08,  "S-Video-2"},
      {0x09,  "Tuner-1"},
      {0x0a,  "Tuner-2"},
      {0x0b,  "Tuner-3"},
      {0x0c,  "Component video (YPrPb/YCrCb) 1"},
      {0x0d,  "Component video (YPrPb/YCrCb) 2"},
      {0x0e,  "Component video (YPrPb/YCrCb) 3"},
      {0x0f,  "DisplayPort-1"},
      {0x10,  "DisplayPort-2"},
      {0x11,  "HDMI-1"},
      {0x12,  "HDMI-2"},
      {0x00,  NULL}       // end of list marker, 0x00 might be a valid value, but NULL never is
};

// 0xaa
static Feature_Value_Entry xaa_screen_orientation_values[] = {
      {0x01, "0 degrees"},
      {0x02, "90 degrees"},
      {0x03, "180 degrees"},
      {0x04, "270 degrees"},
      {0xff, "Display cannot supply orientation"},
      {0xff, NULL}     // terminator
};

// 0xb2
static Feature_Value_Entry xb2_flat_panel_subpixel_layout_values[] = {
      {0x00, "Sub-pixel layout not defined"},
      {0x01, "Red/Green/Blue vertical stripe"},
      {0x02, "Red/Green/Blue horizontal stripe"},
      {0x03, "Blue/Green/Red vertical stripe"},
      {0x04, "Blue/Green/Red horizontal stripe"},
      {0x05, "Quad pixel, red at top left"},
      {0x06, "Quad pixel, red at bottom left"},
      {0x07, "Delta (triad)"},
      {0x08, "Mosaic"},
      {0xff, NULL}     // terminator
};

// 0xb6
static Feature_Value_Entry xb6_display_technology_type_values[] = {
          { 0x01, "CRT (shadow mask)"},
          { 0x02, "CRT (aperture grill)"},
          { 0x03, "LCD (active matrix)"},
          { 0x04, "LCos"},
          { 0x05, "Plasma"},
          { 0x06, "OLED"},
          { 0x07, "EL"},
          { 0x08, "Dynamic MEM"},
          { 0x09, "Static MEM"},
          {0xff, NULL}     // terminator
};

// 0xc8
Feature_Value_Entry xc8_display_controller_type_values[] = {
   {0x01,  "Conexant"},
   {0x02,  "Genesis"},
   {0x03,  "Macronix"},
   {0x04,  "MRT"},
   {0x05,  "Mstar"},
   {0x06,  "Myson"},
   {0x07,  "Phillips"},
   {0x08,  "PixelWorks"},
   {0x09,  "RealTek"},
   {0x0a,  "Sage"},
   {0x0b,  "Silicon Image"},
   {0x0c,  "SmartASIC"},
   {0x0d,  "STMicroelectronics"},
   {0x0e,  "Topro"},
   {0x0f,  "Trumpion"},
   {0x10,  "Welltrend"},
   {0x11,  "Samsung"},
   {0x12,  "Novatek"},
   {0x13,  "STK"},
   {0xff,  "Not defined - a manufacturer designed controller"},
   {0xff, NULL}     // terminator
};
Feature_Value_Entry * pxc8_display_controller_type_values = xc8_display_controller_type_values;

// 0xca
static Feature_Value_Entry xca_osd_values[] = {
      {0x01, "OSD Disabled"},
      {0x02, "OSD Enabled"},
      {0xff, "Display cannot supply this information"},
      {0xff, NULL}     // terminator
};

// 0xcc
static Feature_Value_Entry xcc_osd_language_values[] = {
          {0x00, "Reserved value, must be ignored"},
          {0x01, "Chinese (traditional, Hantai)"},
          {0x02, "English"},
          {0x03, "French"},
          {0x04, "German"},
          {0x05, "Italian"},
          {0x06, "Japanese"},
          {0x07, "Korean"},
          {0x08, "Portuguese (Portugal)"},
          {0x09, "Russian"},
          {0x0a, "Spanish"},
          {0x0b, "Swedish"},
          {0x0c, "Turkish"},
          {0x0d, "Chinese (simplified / Kantai)"},
          {0x0e, "Portuguese (Brazil)"},
          {0x0f, "Arabic"},
          {0x10, "Bulgarian "},
          {0x11, "Croatian"},
          {0x12, "Czech"},
          {0x13, "Danish"},
          {0x14, "Dutch"},
          {0x15, "Estonian"},
          {0x16, "Finish"},
          {0x17, "Greek"},
          {0x18, "Hebrew"},
          {0x19, "Hindi"},
          {0x1a, "Hungarian"},
          {0x1b, "Latvian"},
          {0x1c, "Lithuanian"},
          {0x1d, "Norwegian "},
          {0x1e, "Polish"},
          {0x1f, "Romanian "},
          {0x20, "Serbian"},
          {0x21, "Slovak"},
          {0x22, "Slovenian"},
          {0x23, "Thai"},
          {0x24, "Ukranian"},
          {0x25, "Vietnamese"},
          {0x00,  NULL}       // end of list marker, 0x00 might be a valid value, but NULL never is
};

// 0xd6
static  Feature_Value_Entry xd6_power_mode_values[] =
   { {0x01, "DPM: On,  DPMS: Off"},
     {0x02, "CPM: Off, DPMS: Standby"},
     {0x03, "DPM: Off, DPMS: Suspend"},
     {0x04, "DPM: Off, DPMS: Off" },
     {0x05, "Write only value to turn off display"},
     {0x00, NULL}    // termination entry
};

static Feature_Value_Entry xdc_display_application_values[] = {
   {0x00, "Standard/Default mode"},
   {0x01, "Productivity"},
   {0x02, "Mixed"},
   {0x03, "Movie"},
   {0x04, "User defined"},
   {0x05, "Games"},
   {0x06, "Sports"},
   {0x07, "Professional (all signal processing disabled)"},
   {0x08, "Standard/Default mode with intermediate power consumption"},
   {0x09, "Standard/Default mode with low power consumption"},
   {0x0a, "Demonstration"},
   {0xf0, "Dynamic contrast"},
   {0xff, NULL}     // terminator
};


//
// DDC Virtual Control Panel (VCP) Feature Code Table
//

VCP_Feature_Table_Entry vcp_code_table[] = {
   { .code=0x02,
     .name="New Control Value",
     .flags=VCP_RW | VCP_NON_CONT,
      .formatter = format_feature_detail_new_control_value
   },
   { .code=0x04,
     .name="Restore factory defaults",
     .flags=VCP_WO | VCP_NON_CONT,

   },
   { .code=0x05,
     .name="Restore factory lum/contrast",
     .flags=VCP_WO | VCP_NON_CONT,

   },
   { .code=0x06,
     .name="Restore factory geometry defaults",
     .flags=VCP_WO | VCP_NON_CONT,

   },
   { .code=0x08,
     .name="Restore factory color defaults",
     .flags=VCP_WO | VCP_NON_CONT,

   },
   { .code=0x0b,
     .name="Color temperature increment",
     .flags=VCP_RO | VCP_NON_CONT   | VCP_COLORMGT,

     .formatter=format_feature_detail_debug_continuous
   },
   { .code=0x0c,
     .name="Color temperature request",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .formatter=format_feature_detail_debug_continuous
   },
   { .code=0x0e,
     .name="Clock",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_debug_continuous,
   },
   { .code=0x10,
     .name="Luminosity",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
      .formatter=format_feature_detail_standard_continuous,
   },
   { .code=0x11,
     .name="Flesh tone enhancement",
     .flags=VCP_RW | VCP_NON_CONT   | VCP_COLORMGT,
      .formatter=format_feature_detail_debug_bytes,
   },
   { .code=0x12,
     .name="Contrast",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
      .formatter=format_feature_detail_standard_continuous,
   },
   { .code=0x13,
     .name="Backlight",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_debug_bytes,
   },
   { .code=0x14,
     .name="Select color preset",
     .flags=VCP_RW | VCP_NON_CONT | VCP_FUNC_VER,
     // .flags2=VCP_FUNC_VER,       // interpretation varies depending on VCP version
     .formatter=format_feature_detail_select_color_preset,
   },
   { .code=0x16,
     .name="Red",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
      .formatter=format_feature_detail_standard_continuous,
   },
   { .code=0x18,
     .name="Green",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
      .formatter=format_feature_detail_standard_continuous,
   },
   { .code=0x1a,
     .name="Blue",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
      .formatter=format_feature_detail_standard_continuous,
   },
   { .code=0x1e,
     .name="Auto Setup",
     .flags=VCP_RW | VCP_NON_CONT | VCP_NCSL,
     // .formatter=format_feature_detail_auto_setup,
     .formatter=format_feature_detail_sl_lookup_new,
      .nc_sl_values = x1e_x1f_auto_setup_values,
   },
   { .code=0x1f,
     .name="Auto Color Setup",
     .flags=VCP_RW | VCP_NON_CONT | VCP_COLORMGT | VCP_NCSL,
      // .formatter=format_feature_detail_auto_setup,
      .formatter=format_feature_detail_sl_lookup_new,
       .nc_sl_values = x1e_x1f_auto_setup_values,
   },

   { .code=0x20,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2 except for name
     // When did name change to include "(phase)"?  Assuming 2.1
     .name="Horizontal Position",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags = VCP2_STD_CONT,
     .v20_name="Horizontal Position",
     .v21_name="Horizontal Position (Phase)",
   },
   { .code=0x22,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2
     .name="Horizontal Size",
     .flags=VCP_RW | VCP_CONTINUOUS,
     .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_name="Horizontal Size",
     .v20_flags = VCP2_STD_CONT,
   },
   { // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2
     .code=0x24,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .name="Horizontal Pincushion",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags = VCP2_STD_CONT,
     .v20_name="Horizontal Pincushion",
   },
   { // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2
     .code=0x26,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .name="Horizontal Pincushion Balance",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags = VCP2_STD_CONT,
     .v20_name="Horizontal Pincushion Balance",
   },
   { // Group 8.4 Geometry, name changed in 3.0 & 2.2 vs 2.0  what should it be for 2.1?
     // assume it changed in 2.1
     .code=0x28,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .name="Horizontal Convergence",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,

     .v20_flags = VCP2_STD_CONT,
     .v20_name="Horizontal Convergence",
     .v21_name="Horizontal Convergence R/B",
   },
   { .code=0x29,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // when was this added?  2.1 or 3.0?   assuming 2.1
     .name="Horizontal Convergence M/G",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v21_name="Horizontal Convergence M/G",
     .v21_flags=VCP2_STD_CONT,
   },
   { .code = 0x2a,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry, identical in 3.0, 2.2
     .name = "Horizontal Linearity",
     .flags = VCP_RW | VCP_CONTINUOUS,

     .global_flags=VCP_RW,
     .v20_flags = VCP2_STD_CONT,
     .v20_name="Horizontal Linearity",
   },
   { // Group 8.4 Geometry
     .code = 0x2c,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .name = "Horizontal Linearity Balance",
     .flags = VCP_RW | VCP_CONTINUOUS,

     .global_flags=VCP_RW,
     .v20_flags = VCP2_STD_CONT,
     .v20_name = "Horizontal Linearity Balance",
   },
   { .code=0x2e,
     .name="Gray scale expansion",
     .flags=VCP_RW | VCP_NON_CONT   | VCP_COLORMGT,
      .formatter=format_feature_detail_debug_bytes,
   },
   { .code=0x30,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry
     // When did name change from 2.0? assuming 2.1
     .name="Vertical Position",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags = VCP2_STD_CONT,
     .v20_name="Vertical Position",
     .v21_name="Vertical Position (Phase)",
   },
   { .code=0x32,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry.  Did name change with 2.1 or 3.0/2.2? - assuming 2.1
     .name="Vertical Size",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags=VCP2_STD_CONT,
     .v20_name="Vertical Size",
     .v21_name="Height",
   },
   { .code=0x34,
     // Group 8.4 Geometry.  Identical in 2.0, 3.0, 2.2
     .name = "Vertical Pincushion",
     .flags=VCP_RW | VCP_CONTINUOUS,

     .global_flags=VCP_RW,
     .v20_flags = VCP2_STD_CONT,
     .v20_name = "Vertical Pincushion",
   },
   { .code=0x36,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry.
     .name = "Vertical Pincushion Balance",
     .flags=VCP_RW | VCP_CONTINUOUS,

     .global_flags=VCP_RW,
     .v20_flags = VCP2_STD_CONT,
     .v20_name = "Vertical Pincushion Balance",
   },
   { .code=0x38,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry.  Assume name changed with 2.1
     .name="Vertical Convergence",
     .flags=VCP_RW | VCP_CONTINUOUS,
     .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags=VCP2_STD_CONT,
     .v20_name="Vertical Convergence",
     .v21_name="Vertical Convergence R/B",
   },
   { .code=0x39,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry.  Not in 2.0.  Assume added in 2.1
     .flags=VCP_RW | VCP_CONTINUOUS,
     .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v21_name="Vertical Convergence M/G",
     .v21_flags=VCP2_STD_CONT,
   },
   { .code=0x3a,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry
     .name = "Vertical Linearity",
     .flags = VCP_RW | VCP_CONTINUOUS,
     .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags=VCP2_STD_CONT,
     .v20_name = "Vertical Linearity",
   },
   { .code=0x3c,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry
     .name = "Vertical Linearity Balance",
     .flags = VCP_RW | VCP_CONTINUOUS,
     .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags=VCP2_STD_CONT,
     .v20_name = "Vertical Linearity Balance",
   },
   { .code=0x3e,
     .name="Clock phase",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,
   },
   { .code=0x40,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry
     // When did name change from 2.0?   assume 2.1
     .name="Horizontal Parallelogram",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,

      .global_flags=VCP_RW,
      .v20_flags=VCP2_STD_CONT,
     .v20_name="Key Balance",  // 2.0
     .v21_name="Horizontal Parallelogram",
   },
   { .code=0x42,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry
     // When did name change from 2.0?   assume 2.1
     .name="Horizontal Keystone",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags=VCP2_STD_CONT,
     .v20_name="Horizontal Trapezoid",
     .v21_name="Horizontal Keystone",   // ??
   },
   { .code=0x43,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry
     // When did name change from 2.0?   assume 2.1
     .name="Vertical Keystone",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags=VCP2_STD_CONT,
     .v20_name="Vertical Trapezoid",
     .v21_name="Vertical Keystone",   // ??
   },
   { .code=0x44,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry
     // When did name change from 2.0?   assume 2.1
     .name="Rotation",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags=VCP2_STD_CONT,
     .v20_name="Tilt (rotation)",
     .v21_name="Rotation",   // ??
   },
   { .code=0x46,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry
     // When did name change from 2.0?   assume 2.1
     .name="Top Corner Flare",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags=VCP2_STD_CONT,
     .v20_name="Top Corner",
     .v21_name="Top Corner Flare",   // ??
   },
   { .code=0x48,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry
     // Data from 2.0 spec only, need to check others
     .name="Placeholder",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags=VCP2_STD_CONT,
     .v20_name="Top Corner Balance",
   },
   { .code=0x4a,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry
     // When did name change from 2.0?   assume 2.1
     .name="Bottom Corner Flare",
     .flags=VCP_RW | VCP_CONTINUOUS,
     .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags=VCP2_STD_CONT,
     .v20_name="Bottom Corner",
     .v21_name="Bottom Corner Flare",   // ??
   },
   { .code=0x4c,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Group 8.4 Geometry
     // Data from 2.0 spec only, need to check others
     .name="Placeholder",
     .flags=VCP_RW | VCP_CONTINUOUS,
     .formatter=format_feature_detail_standard_continuous,

     .global_flags=VCP_RW,
     .v20_flags=VCP2_STD_CONT,
     .v20_name="Bottom Corner Balance",
   },

   { .code=0x52,
     .name="Active control",
     .flags=VCP_RO | VCP_NON_CONT,
      .formatter=format_feature_detail_debug_bytes,
   },
   { .code=0x56,
     .name="Horizontal Moire",
     .flags=VCP_RW | VCP_CONTINUOUS,
      .formatter=format_feature_detail_standard_continuous,
   },
   { .code=0x59,
     .name="6 axis saturation: Red",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .formatter=format_feature_detail_sl_byte,
   },
   { .code=0x5a,
     .name="6 axis saturation: Yellow",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .formatter=format_feature_detail_sl_byte,
   },
   { .code=0x5b,
     .name="6 axis saturation: Green",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,

     .formatter=format_feature_detail_sl_byte,
   },
   { .code=0x5c,
     .name="6 axis saturation: Cyan",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,

     .formatter=format_feature_detail_sl_byte,
   },
   { .code=0x5d,
     .name="6 axis saturation: Blue",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,

     .formatter=format_feature_detail_sl_byte,
   },
   { .code=0x5e,
     .name="6 axis saturation: Magenta",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .formatter=format_feature_detail_sl_byte,
   },
   { .code=0x60,
     .name="Input source",
     // should VCP_NCSL be set here?  yes: applies to NC values
     .flags= VCP_RW | VCP_TYPE_V2NC_V3T | VCP_NCSL,   // MCCS 2.0: NC, MCCS 3.0: T
     .formatter=format_feature_detail_sl_lookup_new,    // used only for V2
     //  .formatter=format_feature_detail_debug_bytes,
     .nc_sl_values = x60_v2_input_source_values     // used only for V2
   },
   {
     .code=0x62,
     .name="Audio speaker volume",
     .flags=VCP_RW | VCP_CONTINUOUS,         // actually v2: C, v3: NC
     .formatter=format_feature_detail_standard_continuous,
     // requires special handling for V3, mix of C and NC, SL byte only
   },
   { .code=0x66,
     .name="Ambient light sensor",
     .flags=VCP_RW | VCP_NON_CONT,
      .formatter=format_feature_detail_debug_bytes,
   },
   { .code=0x6c,
     .name="Video black level: Red",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
      .formatter=format_feature_detail_standard_continuous,
   },
   { .code=0x6e,
     .name="Video black level: Green",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
      .formatter=format_feature_detail_standard_continuous,
   },
   { .code=0x70,
     .name="Video black level: Blue",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
      .formatter=format_feature_detail_standard_continuous,
   },
   { .code=0x72,
     .name="Gamma",
     .flags=VCP_RW | VCP_NON_CONT   | VCP_COLORMGT,
      .formatter=format_feature_detail_debug_sl_sh
   },
   { .code=0x73,
     .name="LUT size",
     .flags=VCP_RO | VCP_TABLE      | VCP_COLORMGT,
      .table_formatter=format_feature_detail_x73_lut_size,
   },
   { .code=0x7e,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Section 8.4 Geometry
     // data from v2.0 spec
     // not in v3.0 spec
     // when was it deleted?  v3.0 or v2.1?   For safety assume 3.0
     .name="Placeholder",
     .flags=VCP_RW | VCP_CONTINUOUS,

     .global_flags=VCP_RW,
     .v20_flags=VCP2_STD_CONT,
     .v30_flags=VCP2_DEPRECATED,
     .v22_flags=VCP2_DEPRECATED,
     .v20_name="Trapezoid",
   },
   { .code=0x80,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     // Section 8.4 Geometry
     // data from v2.0 spec, need to check others
     .name="Placeholder",
     .flags=VCP_RW | VCP_CONTINUOUS,

     .global_flags=VCP_RW,
     .v20_flags=VCP2_STD_CONT,
     .v20_name="Keystone",
   },
   { .code=0x87,
     .name="Sharpness",
     .flags=VCP_RW | VCP_CONTINUOUS               ,
      .formatter=format_feature_detail_standard_continuous,
   },
   { .code=0x8a,
     .name="Color saturation",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .formatter=format_feature_detail_standard_continuous,
   },
   { .code=0x90,
     .name="Hue",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
     .formatter=format_feature_detail_standard_continuous,
   },
   { .code=0x9b,
     .name="6 axis hue: Red",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .formatter=format_feature_detail_sl_byte
   },
   { .code=0x9c,
     .name="6 axis hue: Yellow",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .formatter=format_feature_detail_sl_byte
   },
   { .code=0x9d,
     .name="6 axis hue: Green",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .formatter=format_feature_detail_sl_byte
   },
   { .code=0x9e,
     .name="6 axis hue: Cyan",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .formatter=format_feature_detail_sl_byte
   },
   { .code=0x9f,
     .name="6 axis hue: Blue",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .formatter=format_feature_detail_sl_byte
   },
   { .code=0xa0,
     .name="6 axis hue: Magenta",
     .flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .formatter=format_feature_detail_sl_byte
   },
   { .code=0xaa,
     .name="Screen orientation",
     .flags=VCP_RO  | VCP_NON_CONT | VCP_NCSL,
      // .formatter=format_feature_detail_screen_orientation,
     .formatter=format_feature_detail_sl_lookup_new,
      .nc_sl_values=xaa_screen_orientation_values,
   },
   { .code=0xac,
     .name="Horizontal frequency",
     .flags=VCP_RO | VCP_CONTINUOUS,
      .formatter=format_feature_detail_debug_continuous,
   },
   { .code=0xae,
     .name="Vertical frequency",
     .flags=VCP_RO | VCP_CONTINUOUS,
      .formatter=format_feature_detail_debug_continuous,
   },
   { .code=0xb0,
     .name="(Re)Store user saved values for cur mode",
     .flags=VCP_RO | VCP_NON_CONT,
      .formatter=format_feature_detail_debug_bytes,
   },
   { .code=0xb2,
     .name="Flat panel sub-pixel layout",
     .flags=VCP_RO | VCP_NON_CONT | VCP_NCSL,
      // .formatter=format_feature_flat_panel_subpixel_layout,
     .formatter=format_feature_detail_sl_lookup_new,
      .nc_sl_values=xb2_flat_panel_subpixel_layout_values,
   },
   { .code=0xb6,
     .name="Display technology type",
     .flags=VCP_RO | VCP_NON_CONT | VCP_NCSL,
      // .formatter=format_feature_detail_display_technology_type,
     .formatter=format_feature_detail_sl_lookup_new,
      .nc_sl_values=xb6_display_technology_type_values,
   },
   { .code=0xc0,
     .name="Display usage time",
     .flags=VCP_RO | VCP_CONTINUOUS | VCP_FUNC_VER,
      .formatter=format_feature_detail_display_usage_time,
   },
   { .code=0xc6,
     .name="Application enable key",
     .flags=VCP_RO | VCP_NON_CONT                 ,
      .formatter=format_feature_detail_debug_bytes,
   },
   { .code=0xc8,
     .name="Display controller type",
     .flags=VCP_RW | VCP_NON_CONT  /* |  VCP_NCSL */ ,
     // .formatter=format_feature_detail_sl_lookup_new,    // works, but only interprets mfg id in sl
     .formatter=format_feature_detail_display_controller_type,
     .nc_sl_values=xc8_display_controller_type_values,
   },
   { .code=0xc9,
     .name="Display firmware level",
     .flags=VCP_RO | VCP_CONTINUOUS,
      .formatter=format_feature_detail_version,
   },
   { .code=0xca,
       .name="On Screen Display",
       .flags=VCP_RW | VCP_NON_CONT  | VCP_NCSL                ,
        // .formatter=format_feature_detail_osd,
       .formatter=format_feature_detail_sl_lookup_new,
        .nc_sl_values=xca_osd_values,
     },
   { .code=0xcc,
     .name="OSD Language",
     .flags=VCP_RW | VCP_NON_CONT | VCP_NCSL,
     // .formatter=format_feature_detail_osd_language,
     .formatter=format_feature_detail_sl_lookup_new,
      .nc_sl_values=xcc_osd_language_values,
   },
   { .code=0xd6,
     .name="Power mode",
     .flags=VCP_RW | VCP_NON_CONT | VCP_NCSL,
     // .formatter=format_feature_detail_power_mode,
     .formatter=format_feature_detail_sl_lookup_new,
     .nc_sl_values = xd6_power_mode_values
   },
   { .code=0xdc,
     .name="Display application",
     .flags=VCP_RW | VCP_NON_CONT | VCP_NCSL,
      // .formatter=format_feature_detail_display_application,
     .formatter=format_feature_detail_sl_lookup_new,
      .nc_sl_values=xdc_display_application_values,
   },
   { .code=0xdf,
     .name="VCP Version",
     .flags=VCP_RO | VCP_NON_CONT,
      .formatter=format_feature_detail_version,
   }
};
// #pragma GCC diagnostic pop
int vcp_feature_code_count = sizeof(vcp_code_table)/sizeof(VCP_Feature_Table_Entry);

VCP_Feature_Table_Entry null_vcp_code_table_entry = { 0x00, "Unknown Feature", 0x00, NULL};


#ifdef NO
void init_vcp_feature_table() {
   int ndx = 0;
   for (;ndx < vcp_feature_code_count;ndx++) {
      vcp_code_table[ndx].nc_sl_values = NULL;
   }
   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(0xd6);
   assert(pentry);
   pentry->nc_sl_values = xd6_power_mode_values;

}
#endif



void validate_vcp_feature_table() {
   // DBGMSG("Starting");
   bool ok = true;
   int ndx = 0;
   for (;ndx < vcp_feature_code_count;ndx++) {
      VCP_Feature_Table_Entry entry = vcp_code_table[ndx];
      int ct = 0;
      if (entry.flags & VCP_RO) ct++;
      if (entry.flags & VCP_WO) ct++;
      if (entry.flags & VCP_RW) ct++;
      if (ct != 1) {
         fprintf(stderr, "code: 0x%02x, exactly 1 of VCP_RO, VCP_WO, VCP_RW must be set\n", entry.code);
         ok = false;
      }

      ct = 0;
      if (entry.flags & VCP_CONTINUOUS) ct++;
      if (entry.flags & VCP_NON_CONT)   ct++;
      if (entry.flags & VCP_TABLE)      ct++;
      if (entry.flags & VCP_TYPE_V2NC_V3T) ct++;
      if (ct != 1) {
          fprintf(
             stderr,
             "code: 0x%02x, exactly 1 of VCP_CONTINUOUS, VCP_NON_CONT, VCP_TABLE, VCP_TYPE_V2NC_V3T must be set\n",
             entry.code);
          ok = false;
       }

      if ( (entry.flags & VCP_NCSL) && (entry.formatter != format_feature_detail_sl_lookup_new)) {
         fprintf(stderr, "code: 0x%02x, VCP_NCSL set but formatter != feature_detail_sl_lookup_new\n", entry.code);
         ok = false;
      }
   }
   if (!ok)
      PROGRAM_LOGIC_ERROR(NULL);
}

void init_vcp_feature_codes() {
   validate_vcp_feature_table();
#ifdef NO
   init_vcp_feature_table();
#endif
}



#ifdef OLD

typedef struct {
   Byte              feature_id;
   Feature_Value_Entry * value_entries;
} VCP_Values_For_Feature;


VCP_Values_For_Feature sl_values[] = {
      {0x60, x60_v2_input_source_values},
//      {0x60, {{0x01, "VGA-1"}, {0x02, "VGA-2"}} }    // fails
};
int sl_values_ct = sizeof(sl_values)/sizeof(VCP_Values_For_Feature);


VCP_Values_For_Feature * find_feature_values(Byte feature_code) {
   VCP_Values_For_Feature * result = NULL;
   int ndx;
   for (ndx=0; ndx< sl_values_ct; ndx++) {
      if (sl_values[ndx].feature_id == feature_code) {
         result = &sl_values[ndx];
         break;
      }
   }
   return result;
}



char * find_value_name(VCP_Values_For_Feature * pvalues_for_feature, Byte value_id) {
   // DBGMSG("Starting. pvalues_for_feature=%p, value_id=0x%02x", pvalues_for_feature, value_id);
   Feature_Value_Entry * value_entries = pvalues_for_feature->value_entries;
   char * result = NULL;
   Feature_Value_Entry *  cur_value = value_entries;
   while (cur_value->value_name != NULL) {
      // DBGMSG("value_code=0x%02x, value_name = %s", cur_value->value_code, cur_value->value_name);
      if (cur_value->value_code == value_id) {
         result = cur_value->value_name;
         // DBGMSG("Found");
         break;
      }
      cur_value++;
   }
   return result;
}



char * lookup_value_name(Byte feature_code, Byte sl_value) {
   VCP_Values_For_Feature * values_for_feature = find_feature_values(feature_code);
   char * name = find_value_name(values_for_feature, sl_value);
   if (!name)
      name = "Invalid value";
   return name;
}




// 0x60
// data seen does not correspond to the MCCS spec
// values taken from EloView Remote Mgt Local Cmd Set document
bool format_feature_detail_sl_lookup(
      Interpreted_Nontable_Vcp_Response * code_info,  Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert(code_info->vcp_code == 0x60);
   char * s = lookup_value_name(code_info->vcp_code, code_info->sl);

   snprintf(buffer, bufsz,"%s (sl=0x%02x)", s, code_info->sl);
   return true;
}


// should not be needed:
// .formatter = lookup_sl_new
// .formatter_v3 = table

bool format_feature_detail_input_source(
         Interpreted_Nontable_Vcp_Response * code_info,  Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert(code_info->vcp_code == 0x60);

   // assert vcp_version < 3

   // call

   return true;
}
#endif





#ifdef OLD
// 0x1e, 0x1f
bool format_feature_detail_auto_setup(
      Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert(code_info->vcp_code == 0x1e || code_info->vcp_code == 0x1f);
   char * s = NULL;
   switch(code_info->sl) {
      case 0x00: s = "Auto setup not active";                 break;
      case 0x01: s = "Performing auto setup";                 break;
      case 0x02: s = "Enable continuous/periodic auto setup"; break;
      default:   s = "Reserved code, must be ignored";
   }
   snprintf(buffer, bufsz,"%s (sl=0x%02x)", s, code_info->sl);
   return true;
}
#endif



#ifdef OLD
// 0xaa
bool format_feature_detail_screen_orientation(
      Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert(code_info->vcp_code == 0xaa);
   char * s = NULL;
   switch(code_info->sl) {
      case 0x01: s = "0 degrees";    break;
      case 0x02: s = "90 degrees";   break;
      case 0x03: s = "180 degrees";  break;
      case 0x04: s = "270 degrees";  break;
      case 0xff: s = "Display cannot supply orientation"; break;
      default:   s = "Reserved code, must be ignored";
   }
   snprintf(buffer, bufsz,"%s (sl=0x%02x)", s, code_info->sl);
   return true;
}

// 0xb2
bool format_feature_flat_panel_subpixel_layout(
      Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert(code_info->vcp_code == 0xb2);
   char * s = NULL;
   switch(code_info->sl) {
      case 0x00: s = "Sub-pixel layout not defined";     break;
      case 0x01: s = "Red/Green/Blue vertical stripe";   break;
      case 0x02: s = "Red/Green/Blue horizontal stripe"; break;
      case 0x03: s = "Blue/Green/Red vertical stripe";   break;
      case 0x04: s = "Blue/Green/Red horizontal stripe"; break;
      case 0x05: s = "Quad pixel, red at top left";      break;
      case 0x06: s = "Quad pixel, red at bottom left";   break;
      case 0x07: s = "Delta (triad)";                   break;
      case 0x08: s = "Mosaic";                           break;
      default:   s = "Reserved code, must be ignored";
   }
   snprintf(buffer, bufsz,"%s (sl=0x%02x)", s, code_info->sl);
   return true;
}
#endif

#ifdef OLD
// 0xb6
bool format_feature_detail_display_technology_type(
        Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert(code_info->vcp_code == 0xb6);
   Byte techtype = code_info->sl;
          char * typename = NULL;
          switch(techtype) {
          case 0x01: typename = "CRT (shadow mask)";      break;
          case 0x02: typename = "CRT (aperture grill)";   break;
          case 0x03: typename = "LCD (active matrix)";    break;
          case 0x04: typename = "LCos";                   break;
          case 0x05: typename = "Plasma";                 break;
          case 0x06: typename = "OLED";                   break;
          case 0x07: typename = "EL";                     break;
          case 0x08: typename = "Dynamic MEM";            break;
          case 0x09: typename = "Static MEM";             break;
          default:   typename = "<reserved code>";
          }
    snprintf(buffer, bufsz,
             "Display technology type:  %s (0x%02x)" ,
             typename, techtype);
   return true;
}
#endif

#ifdef OLD
// 0xc8
bool format_feature_detail_display_controller_type(
        Interpreted_Nontable_Vcp_Response * info,  Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert(info->vcp_code == 0xc8);
   Byte mfg_id = info->sl;
   char *mfg_name = NULL;
   switch (mfg_id) {
   case 0x01: mfg_name = "Conexant";    break;
   case 0x02: mfg_name = "Genesis";     break;
   case 0x03: mfg_name = "Macronix";    break;
   case 0x04: mfg_name = "MRT";         break;
   case 0x05: mfg_name = "Mstar";       break;
   case 0x06: mfg_name = "Myson";       break;
   case 0x07: mfg_name = "Phillips";    break;
   case 0x08: mfg_name = "PixelWorks";  break;
   case 0x09: mfg_name = "RealTek";     break;
   case 0x0a: mfg_name = "Sage";        break;
   case 0x0b: mfg_name = "Silicon Image";   break;
   case 0x0c: mfg_name = "SmartASIC";   break;
   case 0x0d: mfg_name = "STMicroelectronics";   break;
   case 0x0e: mfg_name = "Topro";   break;
   case 0x0f: mfg_name = "Trumpion";   break;
   case 0x10: mfg_name = "Welltrend";   break;
   case 0x11: mfg_name = "Samsung";   break;
   case 0x12: mfg_name = "Novatek";   break;
   case 0x13: mfg_name = "STK";   break;
   case 0xff: mfg_name = "Not defined - a manufacturer designed controller";   break;
   default:   mfg_name = "Reserved value, must be ignored";
   }
   // ushort controller_number = info->ml << 8 | info->sh;
   // spec is inconsistent, controller number can either be ML/SH or MH/ML
   // observation suggests it's ml and sh
   snprintf(buffer, bufsz,
            "Mfg: %s (sl=0x%02x), controller number: mh=0x%02x, ml=0x%02x, sh=0x%02x",
            mfg_name, mfg_id, info->mh, info->ml, info->sh);
   return true;
}
#endif


#ifdef OLD
// 0xca
bool format_feature_detail_osd(
        Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert(code_info->vcp_code == 0xca);
   char * s = NULL;
   switch(code_info->sl) {
          case 0x01: s = "OSD Disabled";      break;
          case 0x02: s = "OSD Enabled";       break;
          case 0xff: s = "Display cannot supply this information";   break;
          default:   s = "Reserved value, must be ignored";
    }
    snprintf(buffer, bufsz,
             "%s (sl=0x%02x)", s, code_info->sl);
   return true;
}
#endif

#ifdef OLD
// 0xcc
bool format_feature_detail_osd_language(
        Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert(code_info->vcp_code == 0xcc);
   char * s = NULL;
   switch(code_info->sl) {
          case 0x00: s = "Reserved value, must be ignored";  break;
          case 0x01: s = "Chinese (traditional, Hantai)";      break;
          case 0x02: s = "English";   break;
          case 0x03: s = "French";   break;
          case 0x04: s = "German";                   break;
          case 0x05: s = "Italian";                 break;
          case 0x06: s = "Japanese";                 break;
          case 0x07: s = "Korean";                 break;
          case 0x08: s = "Portuguese (Portugal)";                 break;
          case 0x09: s = "Russian";                 break;
          case 0x0a: s = "Spanish";                 break;
          case 0x0b: s = "Swedish";                 break;
          case 0x0c: s = "Turkish";                 break;
          case 0x0d: s = "Chinese (simplified / Kantai)";                 break;
          case 0x0e: s = "Portuguese (Brazil)"; break;
          case 0x0f: s = "Arabic"; break;
          case 0x10: s = "Bulgarian"; break;
          case 0x11: s = "Croatian"; break;
          case 0x12: s = "Czech  "; break;
          case 0x13: s = "Danish "; break;
          case 0x14: s = "Dutch  "; break;
          case 0x15: s = "Estonian"; break;
          case 0x16: s = "Finish "; break;
          case 0x17: s = "Greek  "; break;
          case 0x18: s = "Hebrew "; break;
          case 0x19: s = "Hindi  "; break;
          case 0x1a: s = "Hungarian  "; break;
          case 0x1b: s = "Latvian"; break;
          case 0x1c: s = "Lithuanian "; break;
          case 0x1d: s = "Norwegian  "; break;
          case 0x1e: s = "Polish "; break;
          case 0x1f: s = "Romanian  "; break;
          case 0x20: s = "Serbian"; break;
          case 0x21: s = "Slovak "; break;
          case 0x22: s = "Slovenian  "; break;
          case 0x23: s = "Thai"; break;
          case 0x24: s = "Ukranian   "; break;
          case 0x25: s = "Vietnamese                           "; break;


          default:   s = "Some other language, interpretation table incomplete";
          }
    snprintf(buffer, bufsz,
        "%s (sl=0x%02x)", s, code_info->sl);
   return true;
}
#endif

#ifdef OLD
// 0xd6
bool format_feature_detail_power_mode(
        Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert(code_info->vcp_code == 0xd6);
   Byte techtype = code_info->sl;
          char * s = NULL;
          switch(techtype) {
          case 0x00: s = "Reserved value, must be ignored";  break;
          case 0x01: s = "DPM: On, DPMS: Off";      break;
          case 0x02: s = "DPM: Off, DPMS: Standby";   break;
          case 0x03: s = "DPM: Off, DPMS: Suspend";   break;
          case 0x04: s = "DPM: Off, DPMS: Off";                   break;
          case 0x05: s = "Write only value to turn off display";                 break;
          default:   s = "Reserved value, must be ignored";
          }
    snprintf(buffer, bufsz,
             "%s (sl=0x%02x)", s, techtype);
   return true;
}
#endif

#ifdef OLD
// 0xdc
bool format_feature_detail_display_application(
        Interpreted_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert(code_info->vcp_code == 0xdc);
   char * s = NULL;
   switch(code_info->sl) {
          case 0x00: s = "Standard/Default mode";  break;
          case 0x01: s = "Productivity";      break;
          case 0x02: s = "Mixed";   break;
          case 0x03: s = "Movie";   break;
          case 0x04: s = "User defined";                   break;
          case 0x05: s = "Games";                 break;
          case 0x06: s = "Sports"; break;
          case 0x07: s = "Professional (all signal processing disabled)";  break;
          case 0x08: s = "Standard/Default mode with intermediate power consumption"; break;
          case 0x09: s = "Standard/Default mode with low power consumption"; break;
          case 0x0a: s = "Demonstration"; break;
          case 0xf0: s = "Dynamic contrast"; break;
          default:   s = "Reserved value, must be ignored";
          }
    snprintf(buffer, bufsz,
             "%s (sl=0x%02x)", s, code_info->sl);
   return true;
}
#endif




#ifdef OLD
VCP_Feature_Table_Entry0 vcp_code_table0[] = {
    {0x02,  "New Control Value",              VCP_RW | VCP_NON_CONT                 , NULL, NULL},
    {0x04,  "Restore factory defaults",       VCP_WO | VCP_NON_CONT                 , NULL, NULL},
    {0x05,  "Restore factory lum/contrast",   VCP_WO | VCP_NON_CONT                 , NULL, NULL},
    {0x06,  "Restore factory geometry dflts", VCP_WO | VCP_NON_CONT                 , NULL, NULL},
    {0x08,  "Restore factory color defaults", VCP_WO | VCP_NON_CONT                 , NULL, NULL},
    {0x0b,  "Color temperature increment",    VCP_RO | VCP_NON_CONT   | VCP_COLORMGT, NULL, NULL},
    {0x0c,  "Color temperature request",      VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0x10,  "Luminosity",                     VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE, NULL, NULL},
    {0x11,  "Flesh tone enhancement",         VCP_RW | VCP_NON_CONT   | VCP_COLORMGT, NULL, NULL},
    {0x12,  "Contrast",                       VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE, NULL, NULL},
    {0x13,  "Backlight",                      VCP_RW | VCP_CONTINUOUS,                NULL, NULL},
    {0x14,  "Select color preset",            VCP_RW | VCP_TABLE     ,                NULL, NULL},
    {0x16,  "Red",                            VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE, NULL, NULL},
    {0x18,  "Green",                          VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE, NULL, NULL},
    {0x1a,  "Blue",                           VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE, NULL, NULL},
    {0x22,  "Width",                          VCP_RW | VCP_CONTINUOUS,                NULL, NULL},
    {0x2e,  "Gray scale expansion",           VCP_RW | VCP_NON_CONT   | VCP_COLORMGT, NULL, NULL},
    {0x32,  "Height",                         VCP_RW | VCP_CONTINUOUS,                NULL, NULL},
    {0x52,  "Active control",                 VCP_RO | VCP_NON_CONT ,                 NULL, NULL},
    {0x59,  "6 axis saturation: Red",         VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0x5a,  "6 axis saturation: Yellow",      VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0x5b,  "6 axis saturation: Green",       VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0x5c,  "6 axis saturation: Cyan",        VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0x5e,  "6 axis saturation: Blue",        VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0x5e,  "6 axis saturation: Magenta",     VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0x60,  "Input source",                   VCP_RW | VCP_TABLE ,                    NULL, NULL},
    {0x66,  "Ambient light sensor",           VCP_RW | VCP_NON_CONT                 , NULL, NULL},
    {0x6c,  "Video black level: Red",         VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE, NULL, NULL},
    {0x6e,  "Video black level: Green",       VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE, NULL, NULL},
    {0x70,  "Video black level: Blue",        VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE, NULL, NULL},
    {0x72,  "Gamma",                          VCP_RW | VCP_NON_CONT   | VCP_COLORMGT, NULL, NULL},
    {0x73,  "LUT size",                       VCP_RO | VCP_TABLE      | VCP_COLORMGT, NULL, NULL},
    {0x87,  "Sharpness",                      VCP_RW | VCP_CONTINUOUS               , NULL, NULL},
    {0x8a,  "Color saturation",               VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0x90,  "Hue",                            VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0x9b,  "6 axis hue: Red",                VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0x5a,  "6 axis hue: Yellow",             VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0x9c,  "6 axis hue: Green",              VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0x9e,  "6 axis hue: Cyan",               VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0x9f,  "6 axis hue: Blue",               VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0xa0,  "6 axis hue: Magenta",            VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT, NULL, NULL},
    {0xac,  "Horizontal frequency",           VCP_RO | VCP_CONTINUOUS               , NULL, NULL},
    {0xae,  "Vertical frequency",             VCP_RO | VCP_CONTINUOUS               , NULL, NULL},
    {0xb0,  "(Re)Store user saved vals for cur mode", VCP_RO | VCP_NON_CONT         , NULL, NULL},
    {0xb2,  "Flat panel sub-pixel layout",    VCP_RO | VCP_NON_CONT                 , NULL, NULL},
    {0xb6,  "Display technology type",        VCP_RO | VCP_NON_CONT                 , NULL, NULL},
    {0xc0,  "Display usage time",             VCP_RO | VCP_CONTINUOUS,                NULL, NULL},
    {0xc6,  "Application enable key",         VCP_RO | VCP_NON_CONT                 , NULL, NULL},
    {0xc8,  "Display controller type",        VCP_RW | VCP_NON_CONT                 , NULL, NULL},
    {0xc9,  "Display firmware level",         VCP_RO | VCP_CONTINUOUS               , NULL, NULL},
    {0xcc,  "OSD Language",                   VCP_RW | VCP_NON_CONT                 , NULL, NULL},
    {0xdc,  "Display application",            VCP_RW | VCP_NON_CONT                 , NULL, NULL},
    {0xd6,  "Power mode",                     VCP_RW | VCP_NON_CONT                 , NULL, NULL},
    {0xdf,  "VCP Version",                    VCP_RO | VCP_NON_CONT                 , NULL, NULL}
};
int vcp_feature_code_count0 = sizeof(vcp_code_table0)/sizeof(VCP_Feature_Table_Entry0);

VCP_Feature_Table_Entry0 null_vcp_code_table_entry0 = { 0x00, "Unknown Feature", 0x00, NULL, NULL};



VCP_Feature_Table_Entry0 * get_vcp_feature_code_table_entry0(int ndx) {
   // DBGMSG("ndx=%d, vcp_code_count=%d  ", ndx, vcp_code_count );
   assert( 0 <= ndx && ndx < vcp_feature_code_count);
   return &vcp_code_table0[ndx];
}
#endif




