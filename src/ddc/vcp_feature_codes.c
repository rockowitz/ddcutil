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
         Preparsed_Nontable_Vcp_Response * code_info,  Version_Spec vcp_version, char * buffer, int bufsz);


//
// Functions applicable to VCP_Feature_Table as a whole
//

// Creates humanly readable interpretation of VCP feature flags.
//
// The result is returned in a buffer supplied by the caller.
#ifdef OLD
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
#endif

// Creates humanly readable interpretation of VCP feature flags.
//
// The result is returned in a buffer supplied by the caller.
static char * vcp_interpret_version_feature_flags(Version_Feature_Flags flags, char* buf, int buflen) {
   // DBGMSG("flags: 0x%04x", flags);
   char * rwmsg = "";
   if (flags & VCP2_RO)
      rwmsg = "ReadOnly ";
   else if (flags & VCP2_WO)
      rwmsg = "WriteOnly";
   else if (flags & VCP2_RW)
      rwmsg = "ReadWrite";

   char * typemsg = "";
   // NEED TO ALSO HANDLE TABLE TYPE
   if (flags & VCP2_CONT)
      typemsg = "Continuous";
   else if (flags & VCP2_NC)
      typemsg = "Non-continuous";
   else if (flags & VCP2_TABLE)
      typemsg = "Table";
   // else if (flags & VCP_TYPE_V2NC_V3T)
   //    typemsg = "V2:NC, V3:Table";
   else if (flags & VCP2_DEPRECATED)
      typemsg = "Deprecated";
   else
      typemsg = "Type not set";

   // TODO: determine if varying interpretation by analyzing entry
   // need function has_version_specific_features(entry)
   char * vermsg = "";
   // if (flags & VCP_FUNC_VER)
   // if (has_version_specific_features(entry))
   //       vermsg = " (Version specific interpretation)";

   snprintf(buf, buflen, "%s  %s%s", rwmsg, typemsg, vermsg);
   return buf;
}



void vcp_list_feature_codes() {
   printf("Recognized VCP feature codes:\n");
   char buf[200];
   char buf2[200];
   //  TODO make listvcp respect display to get version?
   int ndx = 0;
   for (;ndx < vcp_feature_code_count; ndx++) {
      VCP_Feature_Table_Entry entry = vcp_code_table[ndx];
      // DBGMSG("code=0x%02x, flags: 0x%04x", entry.code, entry.flags);
      Version_Spec vspec = get_highest_non_deprecated_version(&entry);
      Version_Feature_Flags vflags = get_version_specific_feature_flags(&entry, vspec);
      vcp_interpret_version_feature_flags(vflags, buf, sizeof(buf));
      char * vermsg = "";
      if (has_version_specific_features(&entry))
         vermsg = " (Version specific interpretation)";
      snprintf(buf2, sizeof(buf2), "%s%s", buf, vermsg);

      printf("  %02x - %-40s  %s\n",
             entry.code,
             get_non_version_specific_feature_name(&entry),
             // vcp_interpret_feature_flags(entry.flags, buf, 200)   // *** TODO: HOW TO HANDLE THIS w/o version?
             buf2
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
      result = get_non_version_specific_feature_name(vcp_entry);
   else if (0xe0 <= feature_id && feature_id <= 0xff)
      result = "manufacturer specific feature";
   else
      result = "unrecognized feature";
   return result;
}


char * get_feature_name_by_id_and_vcp_version(Byte feature_id, Version_Spec vspec) {
   char * result = NULL;
   VCP_Feature_Table_Entry * vcp_entry = vcp_find_feature_by_hexid(feature_id);
   if (vcp_entry) {
      result = get_version_specific_feature_name(vcp_entry, vspec);
      if (!result)
         result = get_non_version_specific_feature_name(vcp_entry);    // fallback
   }
   else if (0xe0 <= feature_id && feature_id <= 0xff)
      result = "manufacturer specific feature";
   else
      result = "unrecognized feature";
   return result;
}


int vcp_get_feature_code_count() {
   return vcp_feature_code_count;
}


/* Gets the appropriate VCP flags value for a feature, given
 * the VCP version for the monitor.
 *
 * Arguments:
 *   pvft_entry  vcp_feature_table entry
 *   vcp_version VCP version for monitor
 *
 * Returns:
 *   flags
 */
Version_Feature_Flags
get_version_specific_feature_flags(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec              vcp_version)
{
   bool debug = false;
   Version_Feature_Flags result = 0;
   if (vcp_version.major >= 3)
      result = pvft_entry->v30_flags;
   else if (vcp_version.major == 2 && vcp_version.minor >= 2)
      result = pvft_entry->v22_flags;

   if (!result &&
       (vcp_version.major >= 3 || (vcp_version.major == 2 && vcp_version.minor == 1))
      )
         result = pvft_entry->v21_flags;

   if (!result)
      result = pvft_entry->v20_flags;

   DBGMSF(debug, "Feature = 0x%02x, vcp version=%d.%d, returning 0x%02x",
          pvft_entry->code, vcp_version.major, vcp_version.minor, result);
   return result;
}

bool has_version_specific_features(VCP_Feature_Table_Entry * pentry) {
   int ct = 0;
   if (pentry->v20_flags)  ct++;
   if (pentry->v21_flags)  ct++;
   if (pentry->v30_flags)  ct++;
   if (pentry->v22_flags)  ct++;

   return (ct > 1);
}

Version_Spec get_highest_non_deprecated_version(VCP_Feature_Table_Entry * pentry) {
   Version_Spec vspec = {0,0};
   if ( pentry->v22_flags && !(pentry->v22_flags & VCP2_DEPRECATED) ) {
      vspec.major = 2;
      vspec.minor = 2;
   }
   else if ( pentry->v30_flags && !(pentry->v30_flags & VCP2_DEPRECATED) ) {
       vspec.major = 3;
       vspec.minor = 0;
    }
   else if ( pentry->v21_flags && !(pentry->v21_flags & VCP2_DEPRECATED) ) {
       vspec.major = 2;
       vspec.minor = 1;
    }
   else if ( pentry->v20_flags && !(pentry->v20_flags & VCP2_DEPRECATED) ) {
       vspec.major = 2;
       vspec.minor = 0;
    }


   return vspec;
}

// convenience function
bool is_feature_readable_by_vcp_version(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec vcp_version)
{
   bool result = (get_version_specific_feature_flags(pvft_entry, vcp_version) & VCP2_READABLE );
   // DBGMSG("code=0x%02x, vcp_version=%d.%d, returning %d",
   //        pvft_entry->code, vcp_version.major, vcp_version.minor, result);
   return result;
}

// convenience function
bool is_feature_writable_by_vcp_version(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec vcp_version)
{
   return (get_version_specific_feature_flags(pvft_entry, vcp_version) & VCP2_WRITABLE );
}

// Checks if the table/non-table choice for a feature is version sensitive

bool is_version_conditional_vcp_type(VCP_Feature_Table_Entry * pvft_entry) {
   bool result = false;

   Byte allflags = pvft_entry->v30_flags |
                   pvft_entry->v22_flags |
                   pvft_entry->v21_flags |
                   pvft_entry->v20_flags;

   bool some_nontable = allflags & (VCP2_CONT | VCP2_NC);
   bool some_table    = allflags & VCP2_TABLE;
   result = some_nontable && some_table;

   return result;
}


Feature_Value_Entry * get_version_specific_sl_values(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec              vcp_version)
{
   bool debug = false;
   Feature_Value_Entry * result = NULL;
   if (vcp_version.major >= 3)
      result = pvft_entry->v30_sl_values;
   else if (vcp_version.major == 2 && vcp_version.minor >= 2)
      result = pvft_entry->v22_sl_values;

   if (!result &&
       (vcp_version.major >= 3 || (vcp_version.major == 2 && vcp_version.minor == 1))
      )
         result = pvft_entry->v21_sl_values;

   if (!result)
      result = pvft_entry->nc_sl_values;

   DBGMSF(debug, "Feature = 0x%02x, vcp version=%d.%d, returning %p",
          pvft_entry->code, vcp_version.major, vcp_version.minor, result);
   return result;
}



char * get_version_specific_feature_name(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec              vcp_version)
{
   bool debug = false;
   char * result = NULL;
   if (vcp_version.major >= 3)
      result = pvft_entry->v30_name;
   else if (vcp_version.major == 2 && vcp_version.minor >= 2)
      result = pvft_entry->v22_name;

   if (!result &&
       (vcp_version.major >= 3 || (vcp_version.major == 2 && vcp_version.minor >= 1))
      )
         result = pvft_entry->v21_name;

   if (!result)
      result = pvft_entry->v20_name;

   if (!result) {
   //    DBGMSG("Using original name field");
   //    result = pvft_entry->name;

      DBGMSG("Feature = 0x%02x, Version=%d.%d: No version specific feature name found",
             pvft_entry->code, vcp_version.major, vcp_version.minor);
   }


   DBGMSF(debug, "Feature = 0x%02x, vcp version=%d.%d, returning %s",
          pvft_entry->code, vcp_version.major, vcp_version.minor, result);
   return result;
}

// for use when we don't know the version
char * get_non_version_specific_feature_name(VCP_Feature_Table_Entry * pvft_entry) {
   Version_Spec vspec = {2,2};
   return get_version_specific_feature_name(pvft_entry, vspec);
}


//
// Functions that return a function for formatting a feature value
//

// Functions that lookup a value contained in a VCP_Feature_Table_Entry,
// returning a default if the value is not set for that entry.

Format_Normal_Feature_Detail_Function get_nontable_feature_detail_function( VCP_Feature_Table_Entry * pvft_entry) {
   assert(pvft_entry != NULL);

   // TODO:
   // if VCP_V2NC_V3T, then get version id
   // based on version id, choose .formatter or .formatter_v3
   // NO - test needs to be set in caller, this must return a Format_Feature_Detail_Function, which is not for Table

   Format_Normal_Feature_Detail_Function func = pvft_entry->nontable_formatter;
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
        Preparsed_Nontable_Vcp_Response *    code_info,
        char *                    buffer,
        int                       bufsz)
{
   // new way, requires that vcp_version be set

   Version_Feature_Flags version_specific_flags = get_version_specific_feature_flags(vcp_entry, vcp_version);
   version_specific_flags = 0x00;   // TEMP ***
   if (version_specific_flags) {

   }
   else {
   Format_Normal_Feature_Detail_Function ffd_func = get_nontable_feature_detail_function(vcp_entry);
   ffd_func(code_info, vcp_version,  buffer, bufsz);
   }
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
   if (id >= 0xe0) {
      // pentry->name = "Manufacturer Specific";
      pentry->v20_name = "Manufacturer Specific";
   }
   else {
      // pentry->name = "Unknown feature";
      pentry->v20_name = "Unknown feature";
   }
   // VCP_SYNTHETIC => caller should free
   // pentry->flags = VCP_READABLE;    // so readability tests pass
   pentry->nontable_formatter = format_feature_detail_debug_continuous;
   pentry->v20_flags = VCP2_RO | VCP2_STD_CONT;
   pentry->vcp_global_flags = VCP2_SYNTHETIC;
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

      Version_Feature_Flags feature_flags = get_version_specific_feature_flags(pentry, vcp_version);
      assert(feature_flags);
      // if (feature_flags) {
         // DBGMSG("new way");
         if (feature_flags & VCP2_SIMPLE_NC) {
            result = get_version_specific_sl_values(pentry, vcp_version);
         }
//      }
//      else {
//         // DBGMSG("old way");
//         if (pentry->flags & VCP_NCSL) {
//            assert(pentry->nc_sl_values);
//            result = pentry->nc_sl_values;
//         }
//      }
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
        Preparsed_Nontable_Vcp_Response * code_info,  Version_Spec vcp_version, char * buffer, int bufsz)
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
        Preparsed_Nontable_Vcp_Response * code_info,  Version_Spec vcp_version, char * buffer, int bufsz)
{
   snprintf(buffer, bufsz,
            "mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x, max value = %5d, cur value = %5d",
            code_info->mh,        code_info->ml,
            code_info->sh,        code_info->sl,
            code_info->max_value, code_info->cur_value);
   return true;
}


bool format_feature_detail_debug_bytes(
        Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
        Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
        Preparsed_Nontable_Vcp_Response *  code_info,
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
        Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
        Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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

// 0x0b
bool x0b_format_feature_detail_color_temperature_increment(
      Preparsed_Nontable_Vcp_Response * code_info,
      Version_Spec                        vcp_version,
      char *                              buffer,
      int                                 bufsz)
{
   if (code_info->cur_value == 0 ||
       code_info->cur_value > 5000
      )
      snprintf(buffer, bufsz, "Invalid value: %d", code_info->cur_value);
   else
      snprintf(buffer, bufsz, "%d degree(s) Kelvin", code_info->cur_value);

   return true;   // or should it be false if invalid value?
}


// 0x0c
bool x0c_format_feature_detail_color_temperature_request(
      Preparsed_Nontable_Vcp_Response * code_info,
      Version_Spec                        vcp_version,
      char *                              buffer,
      int                                 bufsz)
{
   // int increments = code_info->cur_value;
   snprintf(buffer, bufsz,
            "3000 + %d * (feature 0B color temp increment) degree(s) Kelvin",
            code_info->cur_value);

   return true;
}


// 0x14
bool format_feature_detail_select_color_preset(
      Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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


// 0xac
bool format_feature_detail_horizontal_frequency(
      Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
  assert (code_info->vcp_code == 0xac);
  // this is R/O field, so max value is irrelevant
  if (code_info->mh == 0xff &&
      code_info->ml == 0xff &&
      code_info->sh == 0xff &&
      code_info->sl == 0xff)
  {
     snprintf(buffer, bufsz, "Cannot determine frequency or out of range");
  }
  else {
     // this is R/O field, so max value is irrelevant
     // int khz = code_info->cur_value/1000;
     // int dec = code_info->cur_value % 1000;

     // snprintf(buffer, bufsz, "%d.%03d Khz", khz, dec);
     snprintf(buffer, bufsz, "%d hz", code_info->cur_value);
  }
  return true;
}


// 0xae
bool format_feature_detail_vertical_frequency(
      Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
  assert (code_info->vcp_code == 0xae);
  if (code_info->mh == 0xff &&
      code_info->ml == 0xff &&
      code_info->sh == 0xff &&
      code_info->sl == 0xff)
  {
     snprintf(buffer, bufsz, "Cannot determine frequency or out of range");
  }
  else {
     // this is R/O field, so max value is irrelevant
     // code_info->cur_value;   // vert frequency, in .01 hz
     int hz = code_info->cur_value/100;
     int dec = code_info->cur_value % 100;

     snprintf(buffer, bufsz, "%d.%02d hz", hz, dec);
  }
  return true;
}


// 0xc0
bool format_feature_detail_display_usage_time(
        Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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

// 0xc6
bool format_feature_detail_application_enable_key(
        Preparsed_Nontable_Vcp_Response * code_info,
        Version_Spec vcp_version,
        char * buffer,
        int bufsz)
{
   assert (code_info->vcp_code == 0xc6);

   snprintf(buffer, bufsz, "0x%02x%02x", code_info->sh, code_info->sl);
   return true;
 }

// 0xc8
bool format_feature_detail_display_controller_type(
        Preparsed_Nontable_Vcp_Response * info,  Version_Spec vcp_version, char * buffer, int bufsz)
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
        Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   int version_number  = code_info->sh;
   int revision_number = code_info->sl;
   snprintf(buffer, bufsz, "%d.%d", version_number, revision_number);
   return true;
}


//
// SL byte value lookup tables
//

// {0x00,NULL} is used as end of list marker. 0x00 might be a valid value, but NULL never is

// 0x02
static Feature_Value_Entry x02_new_control_values[] = {
      // values identical in 2.0, 3.0, 2.2 specs
      {0x01, "No new control values"},
      {0x02, "One or more new control values have been saved"},
      {0xff, "No user controls are present"},
      {0x00, NULL}
};

static Feature_Value_Entry x03_soft_controls_values[] = {
      // values identical in 2.0, 3.0, 2.2 specs
      {0x00, "No button active"},
      {0x01, "Button 1 active"},
      {0x02, "Button 2 active"},
      {0x03, "Button 3 active"},
      {0x04, "Button 4 active"},
      {0x05, "Button 5 active"},
      {0x06, "Button 6 active"},
      {0x07, "Button 7 active"},
      {0xff, "No user controls are present"},
      {0x00, NULL}
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
     {0x00, NULL}       // end of list marker
};

// 0x1e, 0x1f
static Feature_Value_Entry x1e_x1f_auto_setup_values[] = {
      {0x00, "Auto setup not active"},
      {0x01, "Performing auto setup"},
      // end of values for 0x1e, v2.0
      {0x02, "Enable continuous/periodic auto setup"},
      {0x00, NULL}       // end of list marker
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
      // end of v2.0 values
      // remaining values in v2.2 spec, assume also valid for v2.1
      {0x0f,  "DisplayPort-1"},
      {0x10,  "DisplayPort-2"},
      {0x11,  "HDMI-1"},
      {0x12,  "HDMI-2"},
      {0x00,  NULL}
};

// 0x63
Feature_Value_Entry x63_speaker_select_values[] = {
      {0x00,  "Front L/R"},
      {0x01,  "Side L/R"},
      {0x02,  "Rear L/R"},
      {0x03,  "Center/Subwoofer"},
      {0x00,  NULL}
};

// 0x66
Feature_Value_Entry x66_ambient_light_sensor_values[] = {
      {0x01,  "Disabled"},
      {0x02,  "Enabled"},
      {0x00,  NULL}
};



// 0x82: Horizontal Mirror
Feature_Value_Entry x82_horizontal_flip_values[] = {
      {0x00, "Normal mode"},
      {0x01, "Mirrored horizontally mode"},
      {0x00,  NULL}
};

// 0x84: Horizontal Mirror
Feature_Value_Entry x84_vertical_flip_values[] = {
      {0x00, "Normal mode"},
      {0x01, "Mirrored vertically mode"},
      {0x00,  NULL}
};


// 0x8d: Audio Mute
Feature_Value_Entry x8d_tv_audio_mute_source_values[] = {
      {0x01, "Mute the audio"},
      {0x02, "Unmute the audio"},
      {0x00,  NULL}
};

// 0x86: Display Scaling
Feature_Value_Entry x86_display_scaling_values[] = {
      {0x01, "No scaling"},
      {0x02, "Max image, no aspect ration distortion"},
      {0x03, "Max vertical image, no aspect ratio distortion"},
      {0x04, "Max horizontal image, no aspect ratio distortion"},
      // v 2.0 spec values end here
      {0x05, "Max vertical image with aspect ratio distortion"},
      {0x06, "Max horizontal image with aspect ratio distortion"},
      {0x07, "Linear expansion (compression) on horizontal axis"},   // Full mode
      {0x08, "Linear expansion (compression) on h and v axes"},      // Zoom mode
      {0x09, "Squeeze mode"},
      {0x0a, "Non-linear expansion"},                                // Variable
      {0x00,  NULL}
};

// 0x87: Sharpness algorithm  - used only for v2.0
Feature_Value_Entry x87_sharpness_values[] = {
      {0x01,  "Filter function 1"},
      {0x02,  "Filter function 2"},
      {0x03,  "Filter function 3"},
      {0x04,  "Filter function 4"},
      {0x00,  NULL}
};


// 0x99
Feature_Value_Entry x99_window_control_values[] = {
      {0x00,  "No effect"},
      {0x01,  "Off"},
      {0x02,  "On"},
      {0x00,  NULL}
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

static Feature_Value_Entry xa5_window_select_values[] = {
      {0x00, "Full display image area selected except active windows"},
      {0x01, "Window 1 selected"},
      {0x02, "Window 2 selected"},
      {0x03, "Window 3 selected"},
      {0x04, "Window 4 selected"},
      {0x05, "Window 5 selected"},
      {0x06, "Window 6 selected"},
      {0x07, "Window 7 selected"},
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
      // end of v2.0 values
      {0xff, NULL}     // terminator
};

// 0xb6
static Feature_Value_Entry xb6_v20_display_technology_type_values[] = {
          { 0x01, "CRT (shadow mask)"},
          { 0x02, "CRT (aperture grill)"},
          { 0x03, "LCD (active matrix)"},   // TFT in 2.0
          { 0x04, "LCos"},
          { 0x05, "Plasma"},
          { 0x06, "OLED"},
          { 0x07, "EL"},
          { 0x08, "MEM"},     // MEM in 2.0
          {0xff, NULL}     // terminator
};

// 0xb6
static Feature_Value_Entry xb6_display_technology_type_values[] = {
          { 0x01, "CRT (shadow mask)"},
          { 0x02, "CRT (aperture grill)"},
          { 0x03, "LCD (active matrix)"},   // TFT in 2.0
          { 0x04, "LCos"},
          { 0x05, "Plasma"},
          { 0x06, "OLED"},
          { 0x07, "EL"},
          { 0x08, "Dynamic MEM"},     // MEM in 2.0
          { 0x09, "Static MEM"},      // not in 2.0
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
          // end of MCCS v2.0 values
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


// 0xd0: These are MCCS V2 values.   need to check V3
Feature_Value_Entry xd0_v2_output_select_values[] = {
      {0x01,  "Analog video (R/G/B) 1"},
      {0x02,  "Analog video (R/G/B) 2"},
      {0x03,  "Digital video (TDMS) 1"},
      {0x04,  "Digital video (TDMS) 22"},
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
      // end of v2.0 values
      // remaining values in v2.2 spec, assume also valid for v2.1   // TODO: CHECK
      // {0x0f,  "DisplayPort-1"},
      // {0x10,  "DisplayPort-2"},
      // {0x11,  "HDMI-1"},
      // {0x12,  "HDMI-2"},
      {0x00,  NULL}
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


// 0xd7
static  Feature_Value_Entry xd7_aux_power_output_values[] =
   { {0x01, "Disable auxilliary power"},
     {0x02, "Enable Auxilliar power"},
     {0x00, NULL}    // termination entry
};



// 0xda
static  Feature_Value_Entry xda_scan_mode_values[] =
   { {0x00, "Normal operation"},
     {0x01, "Underscan"},
     {0x02, "Overscan"},
     {0x03, "Widescreen" },                        // in 2.0 spec, not in 3.0
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


#pragma GCC diagnostic push
// not suppressing warning, why?
#pragma GCC diagnostic ignored "-Wunused-variable"
// 0xde         // write-only feature
static  Feature_Value_Entry xde_wo_operation_mode_values[] =
   { {0x01, "Stand alone"},
     {0x02, "Slave (full PC control)"},
     {0x00, NULL}    // termination entry
};
#pragma GCC diagnostic pop


//
// DDC Virtual Control Panel (VCP) Feature Code Table
//

//TODO:
// In 2.0 spec, the only the first letter of the first word of a name is capitalized
// In 3.0/2.2, the first letter of each word of a name is capitalized
// Need to make this consistent thoughout the table

VCP_Feature_Table_Entry vcp_code_table[] = {
   { .code=0x01,
     .vcp_spec_groups = VCP_SPEC_MISC,
     .vcp_classes = VCP_CLASS_ANALOG,
     // defined in 2.0, identical in 3.0
     //,name="Degauss",
     //.flags=VCP_WO | VCP_NON_CONT,

     .desc = "Causes a CRT to perform a degauss cycle",
     //.global_flags = VCP_WO,
     .v20_flags = VCP2_WO |VCP2_WO_NC,
     .v20_name = "Degauss",
   },
   { .code=0x02,
     .vcp_spec_groups = VCP_SPEC_MISC,
     // defined in 2.0, identical in 3.0, 2.2
     //,name="New Control Value",
     //.flags=VCP_RW | VCP_NON_CONT,
     .nontable_formatter = format_feature_detail_new_control_value,   // ??
     .nc_sl_values = x02_new_control_values,

     .desc = "Indicates that a display user control (other than power) has been"
             "used to change and save (or autosave) a new value.",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_COMPLEX_NC,
     .v20_name = "New control value",
   },
   { .code=0x03,
     .vcp_spec_groups = VCP_SPEC_MISC,
     // defined in 2.0, identical in 3.0
     //,name="Soft controls",
     //.flags=VCP_RW | VCP_NON_CONT,
     //.formatter = ?
     .nc_sl_values = x03_soft_controls_values,

     .desc = "Allows display controls to be used as soft keys",
     //.global_flags = VCP_RW,
     .v20_flags =  VCP2_RW | VCP2_SIMPLE_NC,
     .v20_name = "Soft controls",
   },
   { .code=0x04,
     // Section 8.1 Preset operation
     // Defined in 2.0, identical in 3.0
     .vcp_spec_groups = VCP_SPEC_PRESET,
     //,name="Restore factory defaults",
     //.flags=VCP_WO | VCP_NON_CONT,

     .desc = "Restore all factor presets including brightness/contrast, "
             "geometry, color, and TV defaults.",
     //.global_flags = VCP_WO,
     .v20_flags =  VCP2_WO | VCP2_WO_NC,
     .v20_name = "Restore factory defaults",
   },
   { .code=0x05,
     // Section 8.1 Preset operation
     // Defined in 2.0, identical in 3.0
     .vcp_spec_groups = VCP_SPEC_PRESET,
     //.name="Restore factory lum/contrast",
     //.flags=VCP_WO | VCP_NON_CONT,

     .desc = "Restore factory defaults for brightness and contrast",
     //.global_flags = VCP_WO,
     .v20_flags =  VCP2_WO | VCP2_WO_NC,
     .v20_name = "Restore factory brightness/contrast defaults",
   },
   { .code=0x06,
     // Section 8.1 Preset operation
     // Defined in 2.0, identical in 3.0
     .vcp_spec_groups = VCP_SPEC_PRESET,
     //.name="Restore factory geometry defaults",
     //.flags=VCP_WO | VCP_NON_CONT,

     //.global_flags = VCP_WO,
     .desc = "Restore factory defaults for geometry adjustments",
     .v20_flags =  VCP2_WO | VCP2_WO_NC,
     .v20_name = "Restore factory geometry defaults",
   },
   { .code=0x08,
     // Section 8.1 Preset operation
     // Defined in 2.0, identical in 3.0
     .vcp_spec_groups = VCP_SPEC_PRESET,
     //.name="Restore factory color defaults",
     //.flags=VCP_WO | VCP_NON_CONT,

     .desc = "Restore factory defaults for color settings.",
     //.global_flags = VCP_WO,
     .v20_flags =  VCP2_WO | VCP2_WO_NC,
     .v20_name = "Restore color defaults",
   },
   { .code=0x0A,
     // Section 8.1 Preset operation
     // Defined in 2.0, identical in 3.0
     .vcp_spec_groups = VCP_SPEC_PRESET,
     //.name="Restore factory TV defaults",
     //.flags=VCP_WO | VCP_NON_CONT,

     .desc = "Restore factory defaults for TV functions.",
     //.global_flags = VCP_WO,
     .v20_flags =  VCP2_WO | VCP2_WO_NC,
     .v20_name = "Restore factory TV defaults",
   },
   { .code=0x0b,
     //.name="Color temperature increment",
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // Section 8.2 Image Adjustment
     // Defined in 2.0
     //.flags=VCP_RO | VCP_NON_CONT   | VCP_COLORMGT,
     .nontable_formatter=x0b_format_feature_detail_color_temperature_increment,

     // from 2.0 spec:
     .desc="Allows the display to specify the minimum increment in which it can "
           "adjust the color temperature.",
     // simpler:
     .desc="Color temperature increment used by feature 0Ch Color Temperature Request",
     //.global_flags=VCP_RO | VCP2_COLORMGT,
     .v20_flags =  VCP2_RO | VCP2_COLORMGT | VCP2_COMPLEX_NC,
     .v20_name="Color temperature increment",
   },
   { .code=0x0c,
     //.name="Color temperature request",
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // Section 8.2 Image Adjustment
     // Defined in 2.0
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .nontable_formatter=x0c_format_feature_detail_color_temperature_request,

      .desc="Specifies a color temperature (degrees Kelvin)",   // my desc
      //.global_flags=VCP_RW | VCP2_COLORMGT,
      .v20_flags = VCP2_RW | VCP2_COLORMGT | VCP2_COMPLEX_CONT,
      .v20_name="Color temperature request",

   },
   { .code=0x0e,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // Section 8.2 Image Adjustment
     // Defined in 2.0
     //.name="Clock",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

      // from the spec:
      // .desc="Increasing (descreasing) this value will increase (descrease) the "
      //       "sampling clock frequency.",
      // simpler:
      .desc="Increase/decrease the sampling clock frequency.",
      //.global_flags=VCP_RW,
      .v20_flags =  VCP2_RW | VCP2_STD_CONT,
      .v20_name="Clock",

   },
   { .code=0x10,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // Section 8.2 Image Adjustment
     // Defined in 2.0, name changed in 3.0, what is it in 2.1?
     //.name="Luminosity",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
      .nontable_formatter=format_feature_detail_standard_continuous,

     .desc="Increasing (decreasing) this value will increase (decrease) the "
           "brightness of the image.",
     //.global_flags=VCP_RW | VCP2_COLORMGT | VCP2_PROFILE,
     .v20_flags =  VCP2_RW | VCP2_COLORMGT | VCP2_PROFILE | VCP2_STD_CONT,
     .v20_name = "Brightness",
     .v30_name = "Luminosity",
   },
   { .code=0x11,
      // not in 2.0, is in 3.0, assume introduced in 2.1
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     //.name="Flesh tone enhancement",
     //.flags=VCP_RW | VCP_NON_CONT   | VCP_COLORMGT,
      .nontable_formatter=format_feature_detail_debug_bytes,

     .desc = "Select contrast enhancement algorithm respecting flesh tone region",
     //.global_flags = VCP_RW  | VCP2_COLORMGT,
     .v21_flags = VCP2_RW | VCP2_COLORMGT | VCP2_COMPLEX_NC,
     .v21_name = "Flesh tone enhancement",
   },
   { .code=0x12,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // Section 8.2 Image Adjustment
     // Defined in 2.0, identical in 3.0
     //.name="Contrast",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
     .nontable_formatter=format_feature_detail_standard_continuous,

     .desc="Increasing (decreasing) this value will increase (decrease) the "
           "contrast of the image.",
     //.global_flags=VCP_RW | VCP2_COLORMGT | VCP2_PROFILE,
     .v20_flags = VCP2_RW | VCP2_COLORMGT | VCP2_PROFILE | VCP2_STD_CONT,
     .v20_name = "Contrast",
   },
   { .code=0x13,
     // not in 2.0, is in 3.0
     // assume first defined in 2.1
     //.name="Backlight",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_debug_bytes,

      .desc = "Increase/decrease the specified backlight control value",
      //.global_flags = VCP_RW | VCP2_COLORMGT | VCP2_PROFILE,
      .v21_flags = VCP2_RW | VCP2_COLORMGT | VCP2_PROFILE | VCP2_COMPLEX_CONT,
      .v21_name  = "Backlight control",
      .v22_flags = VCP2_DEPRECATED,
   },
   { .code=0x14,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // Defined in 2.0, different in 3.0, 2.2
     // what is appropriate choice for 2.1 ?
     //.name="Select color preset",
     //.flags=VCP_RW | VCP_NON_CONT | VCP_FUNC_VER,
     // //.flags2=VCP_FUNC_VER,       // interpretation varies depending on VCP version
     .nontable_formatter=format_feature_detail_select_color_preset,
     .nc_sl_values= x14_color_preset_absolute_values,

     .desc="Select a specified color temperature",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
     .v20_name  = "Select color preset",
     .v30_flags = VCP2_RW | VCP2_COMPLEX_NC,
     .v22_flags = VCP2_RW | VCP2_COMPLEX_NC,
   },
   { .code=0x16,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // Defined in 2.0
     //.name="Red",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
      .nontable_formatter=format_feature_detail_standard_continuous,

      .desc="Increase/decrease the luminesence of red pixels",   // my simplification
      //.global_flags = VCP_RW | VCP2_COLORMGT | VCP2_PROFILE,
      .v20_flags = VCP2_RW | VCP2_COLORMGT | VCP2_PROFILE | VCP2_STD_CONT,
      .v20_name = "Red video gain",
   },
   { .code=0x18,
         .vcp_spec_groups = VCP_SPEC_IMAGE,
         // Defined in 2.0
     //.name="Green",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
      .nontable_formatter=format_feature_detail_standard_continuous,

      .desc="Increase/decrease the luminesence of green pixels",   // my simplification
      //.global_flags = VCP_RW | VCP2_COLORMGT | VCP2_PROFILE,
      .v20_flags = VCP2_RW | VCP2_COLORMGT | VCP2_PROFILE | VCP2_STD_CONT,
      .v20_name = "Green video gain",
   },
   { .code=0x1a,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // Defined in 2.0
     //.name="Blue",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
      .nontable_formatter=format_feature_detail_standard_continuous,

      .desc="Increase/decrease the luminesence of blue pixels",   // my simplification
      //.global_flags = VCP_RW | VCP2_COLORMGT | VCP2_PROFILE,
      .v20_flags = VCP2_RW | VCP2_COLORMGT | VCP2_PROFILE | VCP2_STD_CONT,
      .v20_name = "Blue video gain",
   },
   { .code=0x1c,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     //.flags = VCP_RW | VCP_CONTINUOUS,
     // defined in 2.0, identical in 3.0

     .desc="Increase/decrease the focus of the image",  // my simplification
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name = "Focus",
   },
   { .code=0x1e,
     .vcp_spec_groups = VCP_SPEC_IMAGE,    // 2.0, 3.0
     // Section 8.2 Image Adjustment
     // Defined in 2.0, values differ in 3.0
     //.name="Auto Setup",
     //.flags=VCP_RW | VCP_NON_CONT | VCP_NCSL,
     // .formatter=format_feature_detail_auto_setup,
     .nontable_formatter=format_feature_detail_sl_lookup_new,
      .nc_sl_values = x1e_x1f_auto_setup_values,

      // from 2.0:
      .desc="Perform autosetup function (H/V position, clock, clock phase, "
            "A/D converter, etc.",
      //.global_flags = VCP_RW,
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "Autosetup",
   },
   { .code=0x1f,
     .vcp_spec_groups = VCP_SPEC_IMAGE,   // 3.0
     // not in 2.0, defined in 3.0
     // assume firt introduced in 2.1
     //.name="Auto Color Setup",
     //.flags=VCP_RW | VCP_NON_CONT | VCP_COLORMGT | VCP_NCSL,
      // .formatter=format_feature_detail_auto_setup,
      .nontable_formatter=format_feature_detail_sl_lookup_new,
       .nc_sl_values = x1e_x1f_auto_setup_values,

       .desc="Perform color autosetup function (R/G/B gain and offset, A/D setup, etc. ",
       //.global_flags = VCP_RW,
       .v21_flags = VCP2_RW | VCP2_SIMPLE_NC,
       .v21_name = "Auto color setup",
   },

   { .code=0x20,        // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     .desc = "Increasing (decreasing) this value moves the image toward the right (left) of the display.",
     // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2 except for name
     // When did name change to include "(phase)"?  Assuming 2.1
     //.name="Horizontal Position",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

     //.global_flags=VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name="Horizontal Position",
     .v21_name="Horizontal Position (Phase)",
     .v30_name="Horizontal Position (Phase)",
   },
   { .code=0x22,         // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     .desc = "Increase/decrease the width of the image.",
     // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2
     //.name="Horizontal Size",
     //.flags=VCP_RW | VCP_CONTINUOUS,
     .nontable_formatter=format_feature_detail_standard_continuous,

     //.global_flags=VCP_RW,
     .v20_name="Horizontal Size",
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
   },
   { // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2
     .code=0x24,       // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     .desc = "Increaseing (decreasing) this value will cause the right and left "
             "sides of the image to become more (less) convex.",
     //.name="Horizontal Pincushion",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

     //.global_flags=VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name="Horizontal Pincushion",
   },
   { // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2
     .code=0x26,              // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     .desc = "Increasing (descrasing) this value moves the center section "
             "of the image toward the right (left) side of the display.",
     //.name="Horizontal Pincushion Balance",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

     //.global_flags=VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name="Horizontal Pincushion Balance",
   },
   { // Group 8.4 Geometry, name changed in 3.0 & 2.2 vs 2.0  what should it be for 2.1?
     // assume it changed in 2.1
     .code=0x28,            // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // description identical in 3.0, 2.0 even though name changed
     .desc = "Increasing (descrasing) this value will shift the red pixels to "
             "the right (left) and the blue pixels left (right) across the "
             "image with respect to the green pixels.",
     //.name="Horizontal Convergence",
     //.flags=VCP_RW | VCP_CONTINUOUS,
     .nontable_formatter=format_feature_detail_standard_continuous,

     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name="Horizontal Convergence",
     .v21_name="Horizontal Convergence R/B",
   },
   { .code=0x29,              // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // when was this added?  2.1 or 3.0?   assuming 2.1
     //.name="Horizontal Convergence M/G",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

      .desc = "Increasing (descrasing) this value will shift the magenta pixels to "
              "the right (left) and the green pixels left (right) across the "
              "image with respect to the magenta (sic) pixels.",
     //.global_flags=VCP_RW,
     .v21_name="Horizontal Convergence M/G",
     .v21_flags=VCP2_RW | VCP2_STD_CONT,
   },
   { .code = 0x2a,           // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry, identical in 3.0, 2.2
     //.name = "Horizontal Linearity",
     //.flags = VCP_RW | VCP_CONTINUOUS,

     .desc = "Increase/decrease the density of pixels in the image center.",
     //.global_flags=VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name="Horizontal Linearity",
   },
   { // Group 8.4 Geometry
     .code = 0x2c,               // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     //.name = "Horizontal Linearity Balance",
     //.flags = VCP_RW | VCP_CONTINUOUS,

     .desc = "Increasing (decreasing) this value shifts the density of pixels "
             "from the left (right) side to the right (left) side of the image.",
     //.global_flags=VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name = "Horizontal Linearity Balance",
   },
   { .code=0x2e,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // not defined in 2.0, is defined in 3.0
     // assume new in 2.1
     //.name="Gray scale expansion",
     //.flags=VCP_RW | VCP_NON_CONT   | VCP_COLORMGT,
      .nontable_formatter=format_feature_detail_debug_bytes,

     .desc = "Gray Scale Expansion",
     //.global_flags = VCP_RW | VCP2_COLORMGT,
     .v21_flags = VCP2_RW | VCP2_COLORMGT | VCP2_COMPLEX_NC,
     .v21_name = "Gray scale expansion",
   },
   { .code=0x30,                // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry
     // When did name change from 2.0? assuming 2.1
     //.name="Vertical Position",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

      .desc = "Increasing (decreasing) this value moves the image toward "
              "the top (bottom) edge of the display.",
     //.global_flags=VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name="Vertical Position",
     .v21_name="Vertical Position (Phase)",
   },
   { .code=0x32,                // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry.  Did name change with 2.1 or 3.0/2.2? - assuming 2.1
     //.name="Vertical Size",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

      .desc = "Increase/decreasing the height of the image.",
     //.global_flags=VCP_RW,
     .v20_flags=VCP2_RW |  VCP2_STD_CONT,
     .v20_name="Vertical Size",
   },
   { .code=0x34,                                  // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry.  Identical in 2.0, 3.0, 2.2
     //.name = "Vertical Pincushion",
     //.flags=VCP_RW | VCP_CONTINUOUS,

     .desc = "Increasing (decreasing) this value will cause the top and "
             "bottom edges of the image to become more (less) convex.",
     //.global_flags=VCP_RW,
     .v20_flags =  VCP2_RW | VCP2_STD_CONT,
     .v20_name = "Vertical Pincushion",
   },
   { .code=0x36,                                 // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry.
     //.name = "Vertical Pincushion Balance",
     //.flags=VCP_RW | VCP_CONTINUOUS,

     .desc = "Increasing (decreasing) this value will move the center "
             "section of the image toward the top (bottom) edge of the display.",
     //.global_flags=VCP_RW,
     .v20_flags =  VCP2_RW | VCP2_STD_CONT,
     .v20_name = "Vertical Pincushion Balance",
   },
   { .code=0x38,                                 // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry.  Assume name changed with 2.1
     //.name="Vertical Convergence",
     //.flags=VCP_RW | VCP_CONTINUOUS,
     .nontable_formatter=format_feature_detail_standard_continuous,

     .desc = "Increasing (decreasing) this value shifts the red pixels up (down) "
             "across the image and the blue pixels down (up) across the image "
             "with respect to the green pixels.",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Vertical Convergence",
     .v21_name="Vertical Convergence R/B",
   },
   { .code=0x39,                                 // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry.  Not in 2.0.  Assume added in 2.1
     //.flags=VCP_RW | VCP_CONTINUOUS,
     .nontable_formatter=format_feature_detail_standard_continuous,

     .desc = "Increasing (decreasing) this value shifts the magenta pixels up (down) "
             "across the image and the green pixels down (up) across the image "
             "with respect to the magenta (sic) pixels.",
     //.global_flags=VCP_RW,
     .v21_name="Vertical Convergence M/G",
     .v21_flags= VCP2_RW | VCP2_STD_CONT,
   },
   { .code=0x3a,                                  // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry
     //.name = "Vertical Linearity",
     //.flags = VCP_RW | VCP_CONTINUOUS,
     .nontable_formatter=format_feature_detail_standard_continuous,

     .desc = "Increase/descrase the density of scan lines in the image center.",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name = "Vertical Linearity",
   },
   { .code=0x3c,                                       // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry
     //.name = "Vertical Linearity Balance",
     //.flags = VCP_RW | VCP_CONTINUOUS,
     .nontable_formatter=format_feature_detail_standard_continuous,

     .desc = "Increasing/decrease the density of scan lines in the image center.",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name = "Vertical Linearity Balance",
   },
   { .code=0x3e,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // Define in 2.0, identical in 3.0
     //.name="Clock phase",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

      // from 2.0 spec:
      // .desc="Increasing (decreasing) this value will increase (decrease) the "
      //       "phase shift of the sampling clock.",
     // simpler:
     .desc="Increase/decrease the sampling clock phase shift",
     //.global_flags = VCP_RW,
     .v20_flags =  VCP2_RW | VCP2_STD_CONT,
     .v20_name = "Clock phase",
   },
   { .code=0x40,                                   // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry
     // When did name change from 2.0?   assume 2.1
     //.name="Horizontal Parallelogram",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

      .desc = "Increasing (decreasing) this value shifts the top section of "
              "the image to the right (left) with respect to the bottom section "
              "of the image.",
      //.global_flags=VCP_RW,
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Key Balance",  // 2.0
     .v21_name="Horizontal Parallelogram",
   },
   { .code=0x41,
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry
     // not defined in 2.0, assume defined in 2.1
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

      // 3.0 doc has same description for x41 as x40, is this right?
      // TODO: check the 2.2 spec
      .desc = "Increasing (decreasing) this value shifts the top section of "
              "the image to the right (left) with respect to the bottom section "
              "of the image.",
     //.global_flags=VCP_RW,
     .v21_flags= VCP2_RW | VCP2_STD_CONT,
     .v21_name="Vertical Parallelogram",
   },
   { .code=0x42,                                  // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry
     // When did name change from 2.0?   assume 2.1
     //.name="Horizontal Keystone",
     //.flags= VCP2_RW | VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

     .desc = "Increasing (decreasing) this value will increase (decrease) the "
           "ratio between the horizontal size at the top of the image and the "
           "horizontal size at the bottom of the image.",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Horizontal Trapezoid",
     .v21_name="Horizontal Keystone",   // ??
   },
   { .code=0x43,                             // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry
     // When did name change from 2.0?   assume 2.1
     //.name="Vertical Keystone",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

      .desc = "Increasing (decreasing) this value will increase (decrease) the "
            "ratio between the vertical size at the left of the image and the "
            "vertical size at the right of the image.",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Vertical Trapezoid",
     .v21_name="Vertical Keystone",   // ??
   },
   { .code=0x44,                                // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry
     // When did name change from 2.0?   assume 2.1
     //.name="Rotation",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

      .desc = "Increasing (decreasing) this value rotates the image (counter) "
            "clockwise around the center point of the screen.",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Tilt (rotation)",
     .v21_name="Rotation",   // ??
   },
   { .code=0x46,                          // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry
     // When did name change from 2.0?   assume 2.1
     //.name="Top Corner Flare",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

      .desc = "Increase/decrease the distance between the left and right sides "
              "at the top of the image.",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Top Corner",
     .v21_name="Top Corner Flare",   // ??
   },
   { .code=0x48,                              // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry
     // name is different in 3.0, assume changed in 2.1
     //.name="Placeholder",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

      .desc = "Increasing (decreasing) this value moves the top of the "
            "image to the right (left).",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Top Corner Balance",
     .v21_name="Top Corner Hook",
   },
   { .code=0x4a,                                             //Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry
     // When did name change from 2.0?   assume 2.1
     //.name="Bottom Corner Flare",
     //.flags=VCP_RW | VCP_CONTINUOUS,
     .nontable_formatter=format_feature_detail_standard_continuous,

     .desc = "Increasing (decreasing) this value will increase (decrease) the"
             "distance between the left and right sides at the bottom of the image.",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Bottom Corner",
     .v21_name="Bottom Corner Flare",   // ??
   },
   { .code=0x4c,                                          // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Group 8.4 Geometry
     // name is different in 3.0, assume changed in 2.1
     //.name="Placeholder",
     //.flags=VCP_RW | VCP_CONTINUOUS,
     .nontable_formatter=format_feature_detail_standard_continuous,

     .desc = "Increasing (decreasing) this value moves the bottom end of the "
           "image to the right (left).",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Bottom Corner Balance",
     .v21_name="Bottom Corner Hook",
   },

   { .code=0x52,
     .vcp_spec_groups = VCP_SPEC_MISC,
     // defined in 2.0, 3.0 has extended consistent explanation
     //.name="Active control",
     //.flags=VCP_RO | VCP_NON_CONT,
      .nontable_formatter=format_feature_detail_debug_bytes,

     .desc= "Read id of one feature that has changed, 0x00 indicates no more",  // my desc
     //.global_flags = VCP_RO,
     .v20_flags = VCP2_RO |  VCP2_COMPLEX_NC,
     .v20_name  = "Active control",
   },
   { .code= 0x54,
     .vcp_spec_groups = VCP_SPEC_MISC,
     //.flags = VCP_RW | VCP_NON_CONT,
     // not defined in 2.0, defined in 3.0, 2.2 identical to 3.0, assume new in 2.1
     .nontable_formatter=format_feature_detail_debug_bytes,    // TODO: write formatter

     .desc = "Controls features aimed at preserving display performance",
     //.global_flags = VCP_RW,
     .v21_flags =  VCP2_RW | VCP2_COMPLEX_NC,
     .v21_name = "Performance Preservation",

   },
   { .code=0x56,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // defined in 2.0
     //.name="Horizontal Moire",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

      .desc="Increase/decrease horizontal moire cancellation.",  // my simplification
      //.global_flags = VCP_RW,
      .v20_flags = VCP2_RW |  VCP2_STD_CONT,
      .v20_name="Horizontal Moire",
   },
   { .code=0x58,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // defined in 2.0
     //.name="Vertical Moire",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_standard_continuous,

      .desc="Increase/decrease vertical moire cancellation.",  // my simplification
      //.global_flags = VCP_RW,
      .v20_flags = VCP2_RW |  VCP2_STD_CONT,
      .v20_name="Vertical Moire",
   },
   { .code=0x59,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // not in 2.0, defined in 3.0, assume new as of 2.1
     //.name="6 axis saturation: Red",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .nontable_formatter=format_feature_detail_sl_byte,

      .desc = "Value < 127 decreases red saturation, 127 nominal (default) value, "
                "> 127 increases red saturation",
        //.global_flags = VCP_RW | VCP2_COLORMGT,
        .v21_flags = VCP2_RW | VCP2_STD_CONT,              // use special function?
        .v21_name = "6 axis saturation: Red",
   },
   { .code=0x5a,
         .vcp_spec_groups = VCP_SPEC_IMAGE,
         // not in 2.0, defined in 3.0, assume new as of 2.1
     //.name="6 axis saturation: Yellow",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .nontable_formatter=format_feature_detail_sl_byte,

      .desc = "Value < 127 decreases yellow saturation, 127 nominal (default) value, "
                 "> 127 increases yellow saturation",
         //.global_flags = VCP_RW | VCP2_COLORMGT,
         .v21_flags = VCP2_RW | VCP2_STD_CONT,              // use special function?
         .v21_name = "6 axis saturation: Yellow",
   },
   { .code=0x5b,
         .vcp_spec_groups = VCP_SPEC_IMAGE,
         // not in 2.0, defined in 3.0, assume new as of 2.1
     //.name="6 axis saturation: Green",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,

     .nontable_formatter=format_feature_detail_sl_byte,
     .desc = "Value < 127 decreases green saturation, 127 nominal (default) value, "
                "> 127 increases green saturation",
        //.global_flags = VCP_RW | VCP2_COLORMGT,
        .v21_flags = VCP2_RW | VCP2_STD_CONT,              // use special function?
        .v21_name = "6 axis saturation: Green",
   },
   { .code=0x5c,
         .vcp_spec_groups = VCP_SPEC_IMAGE,
         // not in 2.0, defined in 3.0, assume new as of 2.1
     //.name="6 axis saturation: Cyan",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,

     .nontable_formatter=format_feature_detail_sl_byte,

     .desc = "Value < 127 decreases cyan saturation, 127 nominal (default) value, "
                "> 127 increases cyan saturation",
        //.global_flags = VCP_RW | VCP2_COLORMGT,
        .v21_flags = VCP2_RW | VCP2_STD_CONT,              // use special function?
        .v21_name = "6 axis saturation: Cyan",
   },
   { .code=0x5d,
         .vcp_spec_groups = VCP_SPEC_IMAGE,
         // not in 2.0, defined in 3.0, assume new as of 2.1
     //.name="6 axis saturation: Blue",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,

     .nontable_formatter=format_feature_detail_sl_byte,

     .desc = "Value < 127 decreases blue saturation, 127 nominal (default) value, "
                "> 127 increases blue saturation",
        //.global_flags = VCP_RW | VCP2_COLORMGT,
        .v21_flags = VCP2_RW | VCP2_STD_CONT,              // use special function?
        .v21_name = "6 axis saturation: Blue",
   },
   { .code=0x5e,
         .vcp_spec_groups = VCP_SPEC_IMAGE,
         // not in 2.0, defined in 3.0, assume new as of 2.1
     //.name="6 axis saturation: Magenta",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .nontable_formatter=format_feature_detail_sl_byte,
      .desc = "Value < 127 decreases magenta saturation, 127 nominal (default) value, "
                 "> 127 increases magenta saturation",
         //.global_flags = VCP_RW | VCP2_COLORMGT,
         .v21_flags = VCP2_RW | VCP2_STD_CONT,              // use special function?
         .v21_name = "6 axis saturation: Magenta",
   },
   { .code=0x60,

     .vcp_spec_groups = VCP_SPEC_MISC,
     //.name="Input source",
     // should VCP_NCSL be set here?  yes: applies to NC values
     //.flags= VCP_RW | VCP_TYPE_V2NC_V3T | VCP_NCSL,   // MCCS 2.0: NC, MCCS 3.0: T
     .nontable_formatter=format_feature_detail_sl_lookup_new,    // used only for V2
     //  .formatter=format_feature_detail_debug_bytes,
     .nc_sl_values = x60_v2_input_source_values,     // used only for V2

     .desc = "Selects active video source",
     //.global_flags = VCP_RW,
     .v20_flags =  VCP2_RW | VCP2_SIMPLE_NC,
     .v20_name = "Input Source",
     .v30_flags = VCP2_RW | VCP2_TABLE,

   },
   {
     .code=0x62,                                       // how to handle?
     .vcp_spec_groups = VCP_SPEC_AUDIO,
     // is in 2.0, special coding as of 3.0 assume changed as of 3.0
     //.name="Audio speaker volume",
     //.flags=VCP_RW | VCP_CONTINUOUS,         // actually v2: C, v3: NC
     .nontable_formatter=format_feature_detail_standard_continuous,
     // requires special handling for V3, mix of C and NC, SL byte only

     .desc = "Adjusts speaker volume",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v30_flags = VCP2_RW | VCP2_COMPLEX_CONT,    // TODO
     .v20_name = "Audio speaker volume",
   },
   { .code=0x63,
     .vcp_spec_groups = VCP_SPEC_AUDIO,
     // not in 2.0, is in 3.0, assume new as of 2.1
     //.name = "Speaker Select",
     //.flags = VCP_RW | VCP_NON_CONT,

     .desc="Selects a group of speakers",
     //.global_flags = VCP_RW,
     .v21_flags = VCP2_RW | VCP2_SIMPLE_NC,
     .v21_sl_values = x63_speaker_select_values,
     .v21_name = "Speaker Select",
   },
   {
     .code=0x64,
     .vcp_spec_groups = VCP_SPEC_AUDIO,
     // is in 2.0
     //.name="Audio microphone volume",
     //.flags=VCP_RW | VCP_CONTINUOUS,         // actually v2: C, v3: NC
     .nontable_formatter=format_feature_detail_standard_continuous,

     .desc = "Increase/decrease microphone gain",
        //.global_flags = VCP_RW,
        .v20_flags = VCP2_RW | VCP2_STD_CONT,
        .v20_name = "Audio: Microphone Volume",
   },
   { .code=0x66,
     .vcp_spec_groups = VCP_SPEC_MISC,
     // not in 2.0, assume new in 2.1
     //.name="Ambient light sensor",
     //.flags=VCP_RW | VCP_NON_CONT,
      .nontable_formatter=format_feature_detail_debug_bytes,

      .desc = "Enable/Disable ambient light sensor",
      //.global_flags = VCP_RW,
      .v21_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v21_name = "Ambient light sensor",
      .v21_sl_values = x66_ambient_light_sensor_values,

   },
   { .code=0x6c,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // Defined in 2.0, name change in ?
     //.name="Video black level: Red",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
     .nontable_formatter=format_feature_detail_standard_continuous,

     .desc="Increase/decrease the black level of red pixels",  // my simplification
     //.global_flags = VCP_RW | VCP2_COLORMGT | VCP2_PROFILE,
     .v20_flags =  VCP2_RW |VCP2_STD_CONT,
     .v20_name = "Red video black level",
   },
   { .code=0x6e,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // Defined in 2.0, name change in ?
     //.name="Video black level: Green",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
     .nontable_formatter=format_feature_detail_standard_continuous,

     .desc="Increase/decrease the black level of green pixels",  // my simplification
     //.global_flags = VCP_RW | VCP2_COLORMGT | VCP2_PROFILE,
     .v20_flags =  VCP2_RW |VCP2_STD_CONT,
     .v20_name = "Green video black level",
   },
   { .code=0x70,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // Defined in 2.0, name change in ?
     //.name="Video black level: Blue",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT | VCP_PROFILE,
     .nontable_formatter=format_feature_detail_standard_continuous,

     .desc="Increase/decrease the black level of blue pixels",  // my simplification
     //.global_flags = VCP_RW | VCP2_COLORMGT | VCP2_PROFILE,
     .v20_flags =  VCP2_RW |VCP2_STD_CONT,
     .v20_name = "Blue video black level",
   },
   { .code=0x72,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // 2.0: undefined, 3.0 & 2.2: defined
     // assume defined in 2.1
     //.name="Gamma",
     //.flags=VCP_RW | VCP_NON_CONT   | VCP_COLORMGT,
      .nontable_formatter=format_feature_detail_debug_sl_sh,

     .desc="Select relative or absolute gamma",
     //.global_flags = VCP_RW | VCP2_COLORMGT,
     .v21_flags = VCP2_RW | VCP2_COMPLEX_NC | VCP2_COLORMGT,    // TODO implement function
     .v21_name = "Gamma",
   },
   { .code=0x73,
     .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_IMAGE,
     // VCP_SPEC_MISC in 2.0, VCP_SPEC_IMAGE in 3.0, 2.1? 2.2?
     //.name="LUT size",
     //.flags=VCP_RO | VCP_TABLE      | VCP_COLORMGT,
      .table_formatter=format_feature_detail_x73_lut_size,

     .desc = "Provides the size (number of entries and number of bits/entry) "
           "for the Red, Green, and Blue LUT in the display.",
     //.global_flags = VCP_RO,
     .v20_flags = VCP2_RO| VCP2_TABLE,
     .v20_name  = "LUT Size",
   },
   { .code=0x74,
     // VCP_SPEC_MISC in 2.0, VCP_SPEC_?? in 3.0, 2.2
     .vcp_spec_groups = VCP_SPEC_MISC,
     //.name = "Single point LUT operation",
     //.flags = VCP_RW | VCP_TABLE | VCP_COLORMGT,
     .table_formatter = default_table_feature_detail_function,

     .desc = "Writes a single point within the display's LUT, reads a single point from the LUT",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_TABLE,
     .v20_name = "Single point LUT operation",
   },
   { .code=0x75,
     // VCP_SPEC_MISC in 2.0, VCP_SPEC_?? in 3.0, 2.2
     .vcp_spec_groups = VCP_SPEC_MISC,
     //.name = "Block LUT operation",
     //.flags = VCP_RW | VCP_TABLE | VCP_COLORMGT,
     .table_formatter = default_table_feature_detail_function,

     .desc = "Load (read) multiple values into (from) the display's LUT",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_TABLE,
     .v20_name = "Block LUT operation",
   },
   { .code=0x76,
     // defined in 2.0, 3.0
     .vcp_spec_groups = VCP_SPEC_MISC,
     //.flags = VCP_WO | VCP_TABLE,
     //.name = "Remote Procedure Call",

     .desc = "Initiates a routine resident in the display",
     //.global_flags = VCP_WO,
     .v20_flags = VCP2_WO | VCP2_WO_TABLE,
     .v20_name = "Remote Procedure Call",
   },

   { .code=0x78,
     .vcp_spec_groups = VCP_SPEC_MISC,
     // apparently not in 2.0
     // defined in 3.0, 2.2 - name and description vary, but content identical
     // what to do about 2.1?  assume it's defined in 2.1 and identical to 3.0
     //.name = "EDID operation",
     //.flags = VCP_RO | VCP_TABLE,

     .desc = "Reads a selected 128 byte block of Display Identification Data "
           "(EDID or Display ID) to be read",
     //.global_flags = VCP_RO,
     .v21_flags =  VCP2_RO | VCP2_TABLE,
     .v21_name  = "EDID operation",
     .v30_flags = VCP2_RO | VCP2_TABLE,
     .v30_name  = "EDID operation",
     .v22_flags = VCP2_RO | VCP2_TABLE,
     .v22_name = "Display Identification Operation",
   },
   { .code=0x7a,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // defined in 2.0, not in 3.0
     //.name="Adjust Focal Plane",
     //.flags=VCP_RW | VCP_CONTINUOUS,

     .desc="Increase/decrease the distance to the focal plane of the image",  // my simplification
     //.global_flags=VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name = "Adjust Focal Plane",
   },
   { .code=0x7c,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // defined in 2.0, not in 3.0
      //.name="Adjust Zoom",
      //.flags=VCP_RW | VCP_CONTINUOUS,

      .desc="Increase/decrease the distance to the zoom function of the optics",  // my simplification
      //.global_flags=VCP_RW,
      .v20_flags =  VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Adjust Zoom",
    },
   { .code=0x7e,                                    // TODO: CHECK 2.2 SPEC
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Section 8.4 Geometry
     // data from v2.0 spec
     // not in v3.0 spec
     // when was it deleted?  v3.0 or v2.1?   For safety assume 3.0
     //.name="Placeholder",
     //.flags=VCP_RW | VCP_CONTINUOUS,

     .desc = "Increase/decrease the trapezoid distortion in the image",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v30_flags=VCP2_DEPRECATED,
     .v22_flags=VCP2_DEPRECATED,
     .v20_name="Trapezoid",
   },
   { .code=0x80,                                    // TODO: CHECK 2.2 SPEC
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     .vcp_classes = VCP_CLASS_ANALOG,
     // Section 8.4 Geometry
     // in 2.0 spec, not in 3.0
     // assume not in 2.1
     // TODO: CHECK 2.2
     //.name="Placeholder",
     //.flags=VCP_RW | VCP_CONTINUOUS,

     // from spec:
     // .desc = "Increasing (decreasing) this value will increase (decrease) "
     //         "the degree of keystone distortion in the image.",
     // my simplification:
     .desc="Increase/decrease the keystone distortion in the image.",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Keystone",
   },
   { .code=0x82,
     .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,         // 2.0: Image, 3.0: Geometry

     //.name="Placeholder",
     //.flags=VCP_WO | VCP_NON_CONT,
     .nc_sl_values = x82_horizontal_flip_values,

     .desc="Flip picture horizontally",
     // DESIGN ISSUE!!!
     // This feature is WO in 2.0 spec, RW in 3.0, what is it in 2.2
     // implies cannot use global_flags to store RO/RW/WO
     //.global_flags=VCP_RW,
     .v20_flags =  VCP2_WO | VCP2_WO_NC,
     .v20_name = "HorFlip",
     .v21_name = "Horizontal Mirror (Flip)",
     .v21_flags =  VCP2_RW | VCP2_SIMPLE_NC,
     .v21_sl_values = x82_horizontal_flip_values,

   },
   { .code=0x84,
       .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,         // 2.0: Image, 3.0: Geometry
       // defined in 2.0, not in 3.0?
       //.name="Placeholder",
       //.flags=VCP_RW | VCP_NON_CONT,
       .nc_sl_values = x84_vertical_flip_values,

       // DESIGN ISSUE!!!
       // This feature is WO in 2.0 spec, RW in 3.0, what is it in 2.2
       // implies cannot use global_flags to store RO/RW/WO
       .desc="Flip picture vertically",
       //.global_flags=VCP_RW,
       .v20_flags =  VCP2_WO | VCP2_WO_NC,
       .v20_name = "VertFlip",
       .v21_name = "Vertical Mirror (Flip)",
       .v21_flags =  VCP2_RW | VCP2_SIMPLE_NC,
       .v21_sl_values = x84_vertical_flip_values,
     },
   { .code=0x86,                                              // Done
     .vcp_spec_groups = VCP_SPEC_GEOMETRY,
     //.name="DisplayScaling",
     //.flags = VCP_RW | VCP_NON_CONT,
     .nc_sl_values = x86_display_scaling_values,

     .desc = "Control the scaling (input vs output) of the display",
     //.global_flags=VCP_RW,
     .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
     .v20_name = "Display Scaling",
   },
   { .code=0x87,                                                 // Done
     .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,    // 2.0 geometry, 3.0 Image
     .vcp_classes = VCP_CLASS_ANALOG,     // ???
     //.name="Sharpness",
     // defined in 2.0, is C in 3.0, assume 2.1 is C as well
     //.flags=VCP_RW | VCP_CONTINUOUS               ,
      .nontable_formatter=format_feature_detail_standard_continuous,
      .nc_sl_values = x87_sharpness_values,

      .desc = "Specifies one of a range of algorithms",
      //.global_flags=VCP_RW,
      .v20_flags=VCP2_RW | VCP2_SIMPLE_NC,        // need a lookup table
      .v20_name="Sharpness",
      .v21_flags=VCP2_RW | VCP2_STD_CONT,
   },
   { .code=0x88,
     // defined in 2.0,
     .vcp_spec_groups = VCP_SPEC_MISC,
     .vcp_classes = VCP_CLASS_ANALOG,    // ???
     //.flags = VCP_RW | VCP_CONTINUOUS,

     .desc = "Increase (decrease) the velocity modulation of the horizontal "
           "scan as a function of the change in luminescence level",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name = "Velocity Scan Modulation",
   },
   { .code = 0x89,
     .vcp_spec_groups = VCP_SPEC_MISC,    // 2.0
     //.flags = VCP_WO | VCP_NON_CONT,
     .vcp_classes = VCP_CLASS_TV,
     //.global_flags = VCP_WO,
     .v20_flags = VCP2_WO| VCP2_WO_NC,
     .desc = "Increment (1) or decrement (2) television channel",
     .v20_name = "TV-channel up/down",

   },
   { .code=0x8a,
     //.name="Color saturation",
     .vcp_spec_groups = VCP_SPEC_MISC,     // 2.0
     .vcp_classes = VCP_CLASS_TV,
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .nontable_formatter=format_feature_detail_standard_continuous,

     .desc = "Increase (decrease) the amplitude of the color difference "
           "components of the video signal",
     //.global_flags = VCP_RW,
     .v20_flags =VCP2_RW |  VCP2_STD_CONT,     // ??
     .v20_name  = "TV Color Saturation",
   },

   { .code = 0x8c,
     .vcp_spec_groups = VCP_SPEC_MISC, // 2.0
     .vcp_classes = VCP_CLASS_TV,
     //.flags = VCP_RW | VCP_CONTINUOUS,
     .desc = "Increase/decrease the amplitude of the high frequency components  "
           "of the video signal",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name = "TV-sharpness",
   },
   { .code=0x8d,
     .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
     .vcp_classes = VCP_CLASS_TV,
     //.flags = VCP_RW | VCP_NON_CONT,
     .desc = "Mute (1) or unmute (2) the TV audio",
     .nc_sl_values = x8d_tv_audio_mute_source_values,
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
     .v20_name = "TV-Audio Mute",
   },
   { .code=0x8e,
     .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
     //.flags = VCP_RW | VCP_CONTINUOUS,
     .vcp_classes = VCP_CLASS_TV,
     .desc = "Increase/decrease the ratio between blacks and whites in the image",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name = "TV-Contrast",
   },
   { .code=0x90,
     .vcp_spec_groups = VCP_SPEC_MISC,     // 2.0
     .vcp_classes = VCP_CLASS_TV,
     //.name="Hue",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
     .nontable_formatter=format_feature_detail_standard_continuous,

     .desc = "Increase/decrease the wavelength of the color component of the video signal. "
           "AKA tint",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name = "TV-Hue",
   },
   { .code=0x92,
     .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
     .vcp_classes = VCP_CLASS_TV,
     .desc = "Increase/decrease the black level of the video",
     //.flags = VCP_RW | VCP_CONTINUOUS,
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_STD_CONT,
     .v20_name = "TV Black level/Brightness",
   },

   { .code=0x95,                               // Done
     .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,  // 2.0: IMAGE, 3.0: GEOMETRY
     //.name="Placeholder",
     //.flags = VCP_RW |VCP_CONTINUOUS,    // something

     .desc="Top left X pixel of an area of the image",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Window Position(TL_X)",
   },
   { .code=0x96,                                             // Done
         .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,  // 2.0: IMAGE, 3.0: GEOMETRY
     //.name="Placeholder",
     //.flags = VCP_RW |VCP_CONTINUOUS,    // something

     .desc="Top left Y pixel of an area of the image",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Window Position(TL_Y)",
   },
   { .code=0x97,                                            // Done
     .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,  // 2.0: IMAGE, 3.0: GEOMETRY
     //.name="Placeholder",
     //.flags = VCP_RW |VCP_CONTINUOUS,    // something

     .desc="Bottom right X pixel of an area of the image",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Window Position(BR_X)",
   },
   { .code=0x98,                                                    // Done
         .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,  // 2.0: IMAGE, 3.0: GEOMETRY
     //.name="Placeholder",
     //.flags = VCP_RW |VCP_CONTINUOUS,    // something

     .desc="Bottom right Y pixel of an area of the image",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Window Position(BR_Y)",
   },
   { .code=0x99,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     .vcp_classes = VCP_CLASS_WINDOW,
     // in 2.0,  not in 3.0 or 2.2, what is correct choice for 2.1?
     //.name="Placeholder",
     //.flags = VCP_RW |VCP_NON_CONT,    // something
     .nc_sl_values = x99_window_control_values,

     .desc="Enables the brightness and color within a window to be different "
           "from the desktop.",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_SIMPLE_NC,
     .v20_name="Window control on/off",
     .v30_flags = VCP2_DEPRECATED,
     .v30_flags = VCP2_DEPRECATED,
   },
   { .code=0x9a,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // in 2.0, same in 3.0
     //.name="Window Background",
     //.flags = VCP_RW |VCP_CONTINUOUS,    // temp

     .desc="Changes the contrast ratio between the area of the window and the "
           "rest of the desktop",
     //.global_flags=VCP_RW,
     .v20_flags= VCP2_RW | VCP2_STD_CONT,
     .v20_name="Window control on/off",
   },
   { .code=0x9b,
         .vcp_spec_groups = VCP_SPEC_IMAGE,
         // in 2.0, same in 3.0
     //.name="6 axis hue: Red",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .nontable_formatter=format_feature_detail_sl_byte,

      .desc = "Value < 127 shifts toward magenta, 127 no effect, "
              "> 127 shifts toward yellow",
      //.global_flags = VCP_RW | VCP2_COLORMGT,           // VCP_COLORMGT?
      .v20_flags =VCP2_RW |  VCP2_COMPLEX_CONT,
      .v20_name = "6 axis color control: Red",
   },
   { .code=0x9c,
         .vcp_spec_groups = VCP_SPEC_IMAGE,
         // in 2.0, same in 3.0
     //.name="6 axis hue: Yellow",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .nontable_formatter=format_feature_detail_sl_byte,


      .desc = "Value < 127 shifts toward green, 127 no effect, "
              "> 127 shifts toward red",
      //.global_flags = VCP_RW | VCP2_COLORMGT,           // VCP_COLORMGT?
      .v20_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .v20_name = "6 axis color control: Yellow",
   },
   { .code=0x9d,
         .vcp_spec_groups = VCP_SPEC_IMAGE,
         // in 2.0, same in 3.0
     //.name="6 axis hue: Green",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP_COLORMGT,
      .nontable_formatter=format_feature_detail_sl_byte,

      .desc = "Value < 127 shifts toward yellow, 127 no effect, "
              "> 127 shifts toward cyan",
      //.global_flags = VCP_RW | VCP2_COLORMGT,           // VCP_COLORMGT?
      .v20_flags =VCP2_RW |  VCP2_COMPLEX_CONT,
      .v20_name = "6 axis color control: Green",
   },
   { .code=0x9e,
         .vcp_spec_groups = VCP_SPEC_IMAGE,
         // in 2.0, same in 3.0
     //.name="6 axis hue: Cyan",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP2_COLORMGT,
      .nontable_formatter=format_feature_detail_sl_byte,

      .desc = "Value < 127 shifts toward green, 127 no effect, "
              "> 127 shifts toward blue",
      //.global_flags = VCP_RW | VCP2_COLORMGT,           // VCP_COLORMGT?
      .v20_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .v20_name = "6 axis color control: Cyan",
   },
   { .code=0x9f,
         .vcp_spec_groups = VCP_SPEC_IMAGE,
         // in 2.0, same in 3.0
     //.name="6 axis hue: Blue",
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP2_COLORMGT,
      .nontable_formatter=format_feature_detail_sl_byte,

      .desc = "Value < 127 shifts toward cyan, 127 no effect, "
              "> 127 shifts toward magenta",
      //.global_flags = VCP_RW | VCP2_COLORMGT,    // VCP_COLORMGT
      .v20_flags =VCP2_RW |  VCP2_COMPLEX_CONT,
      .v20_name = "6 axis color control: Blue",
   },
   { .code=0xa0,
     //.name="6 axis hue: Magenta",
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // in 2.0, same in 3.0
     //.flags=VCP_RW | VCP_CONTINUOUS | VCP2_COLORMGT,
     .nontable_formatter=format_feature_detail_sl_byte,

      .desc = "Value < 127 shifts toward blue, 127 no effect, "
              "> 127 shifts toward red",
      //.global_flags = VCP_RW | VCP2_COLORMGT,    // VCP_COLORMGT?
      .v20_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .v20_name = "6 axis color control: Magenta",
   },
   { .code=0xa2,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // Spec section 8.2 Image Adjustment
     // Defined in 2.0, values differ in 3.0
     //.flags = VCP_WO | VCP_NON_CONT,

     // .desc from 2.0
     .desc="Turn on/off an auto setup function",
     //.global_flags = VCP_WO,
     .v20_flags = VCP2_WO | VCP2_WO_NC,
     .v20_name = "Auto setup on/off",
   },
   { .code=0xa4,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // 2.0 spec says: "This command structure is recommended, in conjunction with VCP A5h
     // for all new designs
     // type NC in 2.0, type T in 3.0, 2.2
     // what is correct choice for 2.1?
     //.name="Window control on/off",
     //.flags = VCP_RW | VCP_NON_CONT,
     .nontable_formatter = format_feature_detail_sl_byte, // TODO: write proper function
     .table_formatter = default_table_feature_detail_function,  // TODO: write proper function

     .desc = "Turn selected window operation on/off",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_COMPLEX_NC,
     .v20_name = "Turn the selected window operation on/off",
     .v30_flags = VCP2_RW | VCP2_TABLE,
     .v22_flags = VCP2_RW | VCP2_TABLE,
   },
   { .code=0xa5,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // 2.0 spec says: "This command structure is recommended, in conjunction with VCP A4h
     // for all new designs
     // designated as C, but only takes specific values
     // v3.0 appears to be identical
     //.name="Window Select",
     //.flags = VCP_RW | VCP_CONTINUOUS,
     .nc_sl_values = xa5_window_select_values,

     .desc = "Change selected window (as defined by 95h..98h)",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,         // need lookup table
     .v20_name = "Change the selected window",

   },
   { .code=0xaa,                                          // Done
     //.name="Screen orientation",
     .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,    // 3.0: IMAGE, 2.0: GEOMETRY
     //.flags=VCP_RO  | VCP_NON_CONT | VCP_NCSL,
      // .formatter=format_feature_detail_screen_orientation,
     .nontable_formatter=format_feature_detail_sl_lookup_new,
      .nc_sl_values=xaa_screen_orientation_values,

      .desc="Indicates screen orientation",
      //.global_flags=VCP_RO,
      .v20_flags=VCP2_RO | VCP2_SIMPLE_NC,
      .v20_name="Screen Orientation",
   },
   { .code=0xac,
     //.name="Horizontal frequency",
     .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
     //.flags=VCP_RO | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_horizontal_frequency,

      .desc = "Horizontal sync signal frequency as determined by the display",
      // 2.0: 0xff 0xff 0xff indicates the display cannot supply this info
      //.global_flags = VCP_RO,
      .v20_flags = VCP2_RO | VCP2_COMPLEX_CONT,
      .v20_name  = "Horizontal frequency",
   },
   { .code=0xae,
         .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
     //.name="Vertical frequency",
     //.flags=VCP_RO | VCP_CONTINUOUS,
      .nontable_formatter=format_feature_detail_vertical_frequency,


      .desc = "Vertical sync signal frequency as determined by the display, "
            "in .01 hz",
      // 2.0: 0xff 0xff indicates the display cannot supply this info
      //.global_flags = VCP_RO,
      .v20_flags =VCP2_RO |  VCP2_COMPLEX_CONT,
      .v20_name  = "Vertical frequency",
   },
   { .code=0xb0,
     .vcp_spec_groups = VCP_SPEC_PRESET,
     // Section 8.1 Preset operation
     // Defined in 2.0, v3.0 spec clarifies that value to be set is in SL byte
     //.name="(Re)Store user saved values for cur mode",   // this was my name from the explanation
     //.flags=VCP_WO | VCP_NON_CONT,
     // .formatter=format_feature_detail_debug_bytes,

     .desc = "Store/restore the user saved values for the current mode.",
     //.global_flags = VCP_WO,
      .v20_flags = VCP2_WO | VCP2_WO_NC,
      .v20_name = "Settings",
   },
   { .code=0xb2,
         .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
     //.name="Flat panel sub-pixel layout",
     //.flags=VCP_RO | VCP_NON_CONT | VCP_NCSL,
      // .formatter=format_feature_flat_panel_subpixel_layout,
     .nontable_formatter=format_feature_detail_sl_lookup_new,
      .nc_sl_values=xb2_flat_panel_subpixel_layout_values,

      .desc = "LCD sub-pixel structure",
      //.global_flags = VCP_RO,
       .v20_flags = VCP2_RO | VCP2_SIMPLE_NC,
       .v20_name = "Flat panel sub-pixel layout",
   },
   { .code=0xb6,                                               // DONE
     .vcp_spec_groups = VCP_SPEC_MISC,     // 2.0, 3.0
     //.name="Display technology type",
     //.flags=VCP_RO | VCP_NON_CONT | VCP_NCSL,
      // .formatter=format_feature_detail_display_technology_type,
     .nontable_formatter=format_feature_detail_sl_lookup_new,
      .nc_sl_values=xb6_v20_display_technology_type_values,

      //.global_flags = VCP_RO,
      .v20_flags = VCP2_RO | VCP2_SIMPLE_NC,       // but v3.0 table not upward compatible w 2.0
      .v20_name = "Display technology type",
      .v21_sl_values = xb6_display_technology_type_values,
   },
   { .code=0xc0,
     .vcp_spec_groups = VCP_SPEC_MISC,
     //.name="Display usage time",
     //.flags=VCP_RO | VCP_CONTINUOUS | VCP_FUNC_VER,
      .nontable_formatter=format_feature_detail_display_usage_time,

     .desc = "Active power on time in hours",
     //.global_flags = VCP_RO,
     .v20_flags =VCP2_RO |  VCP2_COMPLEX_CONT,
     .v20_name = "Display usage time",
   },
   { .code=0xc2,
     .vcp_spec_groups = VCP_SPEC_MISC,    // 2.0
     //.name = "Display descriptor length",
     //.flags = VCP_RO | VCP_CONTINUOUS,

     .desc = "Length in bytes of non-volatile storage in the display available "
           "for writing a display descriptor, max 256",
     //.global_flags = VCP_RO,
     .v20_flags = VCP2_RO | VCP2_STD_CONT,    // should there be a different flag when we know value uses only SL?
     .v20_name = "Display descriptor length",
   },
   {.code=0xc3,
    .vcp_spec_groups = VCP_SPEC_MISC, // 2.0
    //.flags = VCP_RW | VCP_TABLE,
    .table_formatter = default_table_feature_detail_function,

    .desc="Reads (writes) a display descriptor from (to) non-volatile storage "
          "in the display.",
    //.global_flags = VCP_RW,
    .v20_flags = VCP2_RW | VCP2_TABLE,
    .v20_name = "Transmit display descriptor",
   },
   { .code = 0xc4,
     .vcp_spec_groups = VCP_SPEC_MISC,  // 2.0
     //.flags = VCP_RW | VCP_NON_CONT,
     .nontable_formatter=format_feature_detail_debug_bytes,

     .desc = "If enabled, the display descriptor shall be displayed when no video "
           "is being received.",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_COMPLEX_NC,   // need to handle "All other values.  The display descriptor shall not be displayed"
     .v20_name = "Enable display of \'display descriptor\'",
   },
   { .code=0xc6,
     .vcp_spec_groups = VCP_SPEC_MISC, // 2.0
     //.name="Application enable key",
     //.flags=VCP_RO | VCP_NON_CONT                 ,
      // .nontable_formatter=format_feature_detail_debug_bytes,
     .nontable_formatter=format_feature_detail_application_enable_key,

      .desc = "A 2 byte value used to allow an application to only operate with known products.",
      //.global_flags =  VCP_RO,
      .v20_flags = VCP2_RO | VCP2_COMPLEX_NC,
      .v20_name = "Application enable key",
   },
   { .code=0xc8,
     .vcp_spec_groups = VCP_SPEC_MISC,    // 2.0
     //.name="Display controller type",
     //.flags=VCP_RW | VCP_NON_CONT  /* |  VCP_NCSL */ ,
     // .formatter=format_feature_detail_sl_lookup_new,    // works, but only interprets mfg id in sl
     .nontable_formatter=format_feature_detail_display_controller_type,
     .nc_sl_values=xc8_display_controller_type_values,

     .desc = "Mfg id of controller and 2 byte manufacturer-specific controller type",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_COMPLEX_NC,
     .v20_name = "Display controller type",
   },
   { .code=0xc9,
     .vcp_spec_groups = VCP_SPEC_MISC,    // 2.0
     //.name="Display firmware level",
     //.flags=VCP_RO | VCP_NON_CONT,
      .nontable_formatter=format_feature_detail_version,

      .desc = "2 byte firmware level",
      //.global_flags = VCP_RO,
      .v20_flags = VCP2_RO | VCP2_COMPLEX_NC,
      .v20_name = "Display firmware level",
   },
   { .code=0xca,
       //.name="On Screen Display",
       .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_CONTROL,   // 2.0: MISC, 3.0: CONTROL
       //.flags=VCP_RW | VCP_NON_CONT  | VCP_NCSL                ,
        // .formatter=format_feature_detail_osd,
       .nontable_formatter=format_feature_detail_sl_lookup_new,
        .nc_sl_values=xca_osd_values,

        .desc = "Is On Screen Display enabled?",
        //.global_flags = VCP_RW,
        .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
        .v20_name = "OSD",
     },
   { .code=0xcc,
     //.name="OSD Language",
     .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
     //.flags=VCP_RW | VCP_NON_CONT | VCP_NCSL,
     // .formatter=format_feature_detail_osd_language,
     .nontable_formatter=format_feature_detail_sl_lookup_new,
      .nc_sl_values=xcc_osd_language_values,

      .desc = "On Screen Display languge",
      //.global_flags = VCP_RW,
      .v20_flags  = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "OSD Language",
   },
   { .code=0xda,                                                   // DONE
     .vcp_spec_groups = VCP_SPEC_GEOMETRY | VCP_SPEC_IMAGE,         // 2.0: IMAGE, 3.0: GEOMETRY
     .vcp_classes = VCP_CLASS_TV,
     //.flags = VCP_RW | VCP_NON_CONT,
     .desc = "Controls scan characteristics (aka format)",
     .nc_sl_values = xda_scan_mode_values,
     //.global_flags = VCP_RW,

     .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
     .v20_name  = "Scan format",
     // name differs in 3.0, assume changed as of 2.1
     .v21_name  = "Scan characteristics",
   },
   { .code=0xd6,                           // DONE
     .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_CONTROL,   // 2.0: MISC, 3.0: CONTROL
     //.name="Power mode",
     //.flags=VCP_RW | VCP_NON_CONT | VCP_NCSL,
     // .formatter=format_feature_detail_power_mode,
     .nontable_formatter=format_feature_detail_sl_lookup_new,
     .nc_sl_values = xd6_power_mode_values,

     .desc = "DPM and DPMS status",
     //.global_flags = VCP_RW,
     .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
     .v20_name = "Power mode",

   },
   { .code=0xd7,                          // DONE - identical in 2.0, 3.0, 2.2
     .vcp_spec_groups = VCP_SPEC_MISC,    // 2.0, 3.0, 2.2
     .nc_sl_values = xd7_aux_power_output_values,
     .desc="Controls an auxilliary power output from a display to a host device",
     .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
     .v20_name = "Auxilliary power output",
   },
   { .code=0xdc,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     // defined in 2.0, 3.0 has different name, more values
     //.name="Display application",
     //.flags=VCP_RW | VCP_NON_CONT | VCP_NCSL,
      // .formatter=format_feature_detail_display_application,
     .nontable_formatter=format_feature_detail_sl_lookup_new,
      .nc_sl_values=xdc_display_application_values,

      .desc="Type of application used on display",  // my desc
      //.global_flags = VCP_RW,
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "Display Mode",
   },
   { .code=0xde,
      // code 0xde has a completely different name and definition in v2.0
      // vs v3.0/2.2
      // 2.0: Operation Mode, W/O single byte value per xde_wo_operation_mode_values
      // 3.0, 2.1: Scratch Pad: 2 bytes of volatile storage for use of software applications
      // Did the definition really change so radically, or is the 2.0 spec a typo.
      // What to do for 2.1?  Assume same as 3.0,2.2
     .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0, 3.0, 2.2
     .desc = "Operation mode (2.0) or scratch pad (3.0)",
     .nontable_formatter = format_feature_detail_debug_sl_sh,
     .v20_flags = VCP2_WO | VCP2_WO_NC,
     .v20_name  = "Operation Mode",
     .v21_flags = VCP2_RW | VCP2_COMPLEX_NC,
     .v21_name  = "Scratch Pad",
   },
   { .code=0xdf,
     .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
     //.name="VCP Version",
     //.flags=VCP_RO | VCP_NON_CONT,
     .nontable_formatter=format_feature_detail_version,

     .desc = "MCCS version",
     //.global_flags = VCP_RO,
     .v20_flags = VCP2_RO | VCP2_COMPLEX_NC,
     .v20_name  = "VCP Version",

   }
};
// #pragma GCC diagnostic pop
int vcp_feature_code_count = sizeof(vcp_code_table)/sizeof(VCP_Feature_Table_Entry);

// not used, no longer well defined
// VCP_Feature_Table_Entry null_vcp_code_table_entry = { 0x00, "Unknown Feature", 0x00, NULL};


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

#ifdef OLD
bool compare_rw_flags(VCP_Feature_Flags f1, Version_Feature_Flags f2) {
    bool ok = true;
    if ( ( (f1 & VCP_RO) && !(f2 & VCP2_RO) ) || ( !(f1 & VCP_RO) && (f2 & VCP2_RO) )  ||
         ( (f1 & VCP_RW) && !(f2 & VCP2_RW) ) || ( !(f1 & VCP_RW) && (f2 & VCP2_RW) )  ||
         ( (f1 & VCP_WO) && !(f2 & VCP2_WO) ) || ( !(f1 & VCP_WO) && (f2 & VCP2_WO) )
       ) ok = false;

    return ok;
}
#endif

int check_one_version_flags(Version_Feature_Flags vflags, char * which_flags, VCP_Feature_Table_Entry entry) {
   int ct = 0;
   if (vflags && !(vflags & VCP2_DEPRECATED))  {
      if (vflags & VCP2_STD_CONT)     ct++;
      if (vflags & VCP2_COMPLEX_CONT) ct++;
      if (vflags & VCP2_SIMPLE_NC)    ct++;
      if (vflags & VCP2_COMPLEX_NC)   ct++;
      if (vflags & VCP2_WO_NC)        ct++;
      if (vflags & VCP2_TABLE)        ct++;
      if (vflags & VCP2_WO_TABLE)     ct++;
      if (ct != 1) {
          fprintf(
             stderr,
             "code: 0x%02x, exactly 1 of VCP2_STD_CONT, VCP2_COMPLEX_CONT, VCP2_SIMPLE_NC, "
             "VCP2_COMPLEX_NC, VCP2_TABLE, VCP2_WO_TABLE must be set in %s\n",
             entry.code, which_flags);
          ct = -1;
       }

#ifdef OLD
      if ( ( (entry.flags & VCP_CONTINUOUS) && !(vflags & VCP2_CONT   ) ) ||
           ( (entry.flags & VCP_NON_CONT  ) && !(vflags & VCP2_NC     ) ) ||
           ( (entry.flags//.flags_TABLE     ) && !(vflags & VCP2_ANY_TABLE  ) )
         )
      {
         if ( (entry.code != 0xa5) && (entry.code != 0x87) ) {
            // printf("entry.flags = 0x%04x, vflags = 0x%04x\n", entry.flags, vflags);
            // printf("(entry.flags & VCP_NON_CONT) = %x%04x, (vflags & VCP2_NC) = 0x%04x\n",
            //        (entry.flags & VCP_NON_CONT),  (vflags & VCP2_NC) );
            fprintf(
                stderr,
                "code: 0x%02x, types do not match between .flags and %s\n",
                entry.code, which_flags);
            ct = -2;
         }
      }
#endif
      if (vflags & VCP2_SIMPLE_NC) {
         if (!entry.nc_sl_values) {
            fprintf(
               stderr,
               "code: 0x%02x, .flags: %s, VCP2_SIMPLE_NC set but .nc_sl_values == NULL\n",
               entry.code, which_flags);
            ct = -2;
         }
      }
      else if (vflags & VCP2_COMPLEX_NC) {
         if (!entry.nontable_formatter) {
            fprintf(
               stderr,
               "code: 0x%02x, .flags: %s, VCP2_COMPLEX_NC set but .nontable_formatter == NULL\n",
               entry.code, which_flags);
            ct = -2;
         }
      }
      else if (vflags & VCP2_COMPLEX_CONT) {
         if (!entry.nontable_formatter) {
            fprintf(
               stderr,
               "code: 0x%02x, .flags: %s, VCP2_COMPLEX_CONT set but .nontable_formatter == NULL\n",
               entry.code, which_flags);
            ct = -2;
         }
      }
      else if (vflags & VCP2_TABLE) {
         if (!entry.table_formatter) {
            fprintf(
               stderr,
               "code: 0x%02x, .flags: %s, VCP2_TABLE set but .table_formatter == NULL\n",
               entry.code, which_flags);
            ct = -2;
         }
      }
   }

   return ct;
}

void validate_vcp_feature_table() {
   // DBGMSG("Starting");
   bool ok = true;
   int ndx = 0;
   // return;       // *** TEMP ***

   for (;ndx < vcp_feature_code_count;ndx++) {
      VCP_Feature_Table_Entry entry = vcp_code_table[ndx];
#ifdef OLD
      int ct = 0;
      if (entry.flags & VCP_RO) ct++;
      if (entry.flags & VCP_WO) ct++;
      if (entry.flags & VCP_RW) ct++;
      if (ct != 1) {
         fprintf(stderr, "code: 0x%02x, exactly 1 of VCP_RO, VCP_WO, VCP_RW must be set in .flags\n", entry.code);
         ok = false;
      }
#endif

#ifdef OLD
      if (entry//.global_flags) {
         int ct = 0;
         if (entry.global_flags & VCP_RO) ct++;
         if (entry.global_flags & VCP_WO) ct++;
         if (entry.global_flags & VCP_RW) ct++;
         if (ct != 1) {
            fprintf(stderr, "code: 0x%02x, exactly 1 of VCP_RO, VCP_WO, VCP_RW must be set in //.global_flags\n", entry.code);
            ok = false;
         }
         // else
         //    printf("code: 0x%02x global flags RW checked\n", entry.code);
      }
#endif
      if (entry.v20_flags && !(entry.v20_flags & VCP2_DEPRECATED))  {
           int ct = 0;
           if (entry.v20_flags & VCP2_RO) ct++;
           if (entry.v20_flags & VCP2_WO) ct++;
           if (entry.v20_flags & VCP2_RW) ct++;
           if (ct != 1) {
              fprintf(stderr, "code: 0x%02x, exactly 1 of VCP2_RO, VCP2_WO, VCP2_RW must be set in non-zero .v20_flags\n", entry.code);
              ok = false;
           }
#ifdef OLD
           if ( !compare_rw_flags(entry.flags, entry.v20_flags) ) {
              if (entry.code != 0x82 && entry.code!= 0x84) {
              fprintf(stderr, "code: 0x%02x, RW flags in .flags vs .v20_flags do not match\n", entry.code);
              ok = false;
              }
           }
#endif

      }
      if (entry.v21_flags && !(entry.v21_flags & VCP2_DEPRECATED)) {
           int ct = 0;
           if (entry.v21_flags & VCP2_RO) ct++;
           if (entry.v21_flags & VCP2_WO) ct++;
           if (entry.v21_flags & VCP2_RW) ct++;
           if (ct != 1) {
              fprintf(stderr, "code: 0x%02x, exactly 1 of VCP2_RO, VCP2_WO, VCP2_RW must be set in non-zero .v21_flags\n", entry.code);
              ok = false;
           }
#ifdef OLD
           if ( !compare_rw_flags(entry.flags, entry.v21_flags) ) {
              if (entry.code != 0x82 && entry.code!= 0x84) {
              fprintf(stderr, "code: 0x%02x, RW flags in .flags vs .v21_flags do not match\n", entry.code);
              ok = false;
              }
           }
#endif
      }
      if (entry.v30_flags && !(entry.v30_flags & VCP2_DEPRECATED)) {
            int ct = 0;
            if (entry.v30_flags & VCP2_RO) ct++;
            if (entry.v30_flags & VCP2_WO) ct++;
            if (entry.v30_flags & VCP2_RW) ct++;
            if (ct != 1) {
               fprintf(stderr, "code: 0x%02x, exactly 1 of VCP2_RO, VCP2_WO, VCP2_RW must be set in non-zero .v30_flags\n", entry.code);
               ok = false;
            }
#ifdef OLD
            if ( !compare_rw_flags(entry.flags, entry.v30_flags) ) {
               if (entry.code != 0x82 && entry.code!= 0x84) {
               fprintf(stderr, "code: 0x%02x, RW flags in .flags vs .v30_flags do not match\n", entry.code);
               ok = false;
               }
            }
#endif
       }
       if (entry.v22_flags && !(entry.v22_flags & VCP2_DEPRECATED)) {
            int ct = 0;
            if (entry.v22_flags & VCP2_RO) ct++;
            if (entry.v22_flags & VCP2_WO) ct++;
            if (entry.v22_flags & VCP2_RW) ct++;
            if (ct != 1) {
               fprintf(stderr, "code: 0x%02x, exactly 1 of VCP2_RO, VCP2_WO, VCP2_RW must be set in non-zero .v22_flags\n", entry.code);
               ok = false;
            }
#ifdef OLD
            if ( !compare_rw_flags(entry.flags, entry.v22_flags) ) {
               if (entry.code != 0x82 && entry.code!= 0x84) {
                  fprintf(stderr, "code: 0x%02x, RW flags in .flags vs .v22_flags do not match\n", entry.code);
                  ok = false;
               }
            }
#endif
       }

#ifdef OLD
      ct = 0;
      if (entry.flags & VCP_CONTINUOUS) ct++;
      if (entry.flags & VCP_NON_CONT)   ct++;
      if (entry.flags & VCP_TABLE)      ct++;
      if (entry.flags & VCP_TYPE_V2NC_V3T) ct++;
      if (ct != 1) {
          fprintf(
             stderr,
             "code: 0x%02x, exactly 1 of VCP_CONTINUOUS, VCP_NON_CONT, VCP_TABLE, VCP_TYPE_V2NC_V3T must be set in .flags\n",
             entry.code);
          ok = false;
       }


      if ( (entry.flags & VCP_NCSL) && (entry.nontable_formatter != format_feature_detail_sl_lookup_new)) {
         fprintf(stderr, "code: 0x%02x, VCP_NCSL set but formatter != feature_detail_sl_lookup_new\n", entry.code);
         ok = false;
      }
#endif
      int total_ct = 0;
      bool really_bad = false;
      int cur_ct = check_one_version_flags(entry.v20_flags, ".v20_flags", entry);
      if (cur_ct < 0) {
         ok = false;
         if (cur_ct < -1)
            really_bad = true;
      }
      else
         total_ct += 1;
      if (total_ct == 0 && !really_bad) {
         fprintf(stderr, "code: 0x%02x, Type not specified in any vnn_flags\n", entry.code);
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
      Preparsed_Nontable_Vcp_Response * code_info,  Version_Spec vcp_version, char * buffer, int bufsz)
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
         Preparsed_Nontable_Vcp_Response * code_info,  Version_Spec vcp_version, char * buffer, int bufsz)
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
      Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
      Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
      Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
        Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
        Preparsed_Nontable_Vcp_Response * info,  Version_Spec vcp_version, char * buffer, int bufsz)
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
        Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
        Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
        Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
        Preparsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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




