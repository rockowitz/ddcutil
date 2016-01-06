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
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/msg_control.h"

#include "ddc/vcp_feature_codes.h"

// Direct writes to stdout,stderr:
//   in function: bool format_feature_detail_display_usage_time()
//   in table validation functions (Benign)

// Standard formatting string for reporting feature codes.
// Not actually used in this file, but will be used by callers.
// This seems as good a place as any to put the constant.
const char* FMT_CODE_NAME_DETAIL_WO_NL = "VCP code 0x%02x (%-30s): %s";
const char* FMT_CODE_NAME_DETAIL_W_NL  = "VCP code 0x%02x (%-30s): %s\n";

// Forward references
int vcp_feature_code_count;
VCP_Feature_Table_Entry vcp_code_table[];
static Feature_Value_Entry x14_color_preset_absolute_values[];
       Feature_Value_Entry xc8_display_controller_type_values[];
static Feature_Value_Entry x8d_tv_audio_mute_source_values[];
static Feature_Value_Entry x8d_sh_blank_screen_values[];
bool default_table_feature_detail_function(
        Buffer *                       data,
        Version_Spec                   vcp_version,
        char**                         presult);
bool format_feature_detail_debug_continuous(
        Parsed_Nontable_Vcp_Response * code_info,
        Version_Spec                   vcp_version,
        char *                         buffer,
        int                            bufsz);
bool format_feature_detail_standard_continuous(
        Parsed_Nontable_Vcp_Response * code_info,
        Version_Spec                   vcp_version,
        char *                         buffer,
        int                            bufsz);
bool format_feature_detail_sl_lookup(
        Parsed_Nontable_Vcp_Response * code_info,
        Version_Spec                   vcp_version,
        char *                         buffer,
        int                            bufsz);


//
// Utility functions
//

const Version_Spec VCP_SPEC_V20 = {2,0};
const Version_Spec VCP_SPEC_V21 = {2,1};
const Version_Spec VCP_SPEC_V30 = {3,0};
const Version_Spec VCP_SPEC_V22 = {2,2};

// addresses the fact that v3.0 spec is not a direct superset of 2.2
// both are greater than 2.1
// will require modificiation if a new spec appears
bool vcp_version_le(Version_Spec val, Version_Spec max) {
   bool result = false;
   assert (val.major <= 3);
   assert (max.major == 2 || max.major == 3);

   if (max.major == 2) {
      if (val.major < 2)
         result = true;
      else
         result = (val.minor <= max.minor);
   }
   else if (max.major == 3) {
      if (val.major < 2)
         result = true;
      else if (val.major == 2)
         result = (val.minor <= 1);
      else
         result = (val.minor <= max.minor);
   }
   else
      PROGRAM_LOGIC_ERROR("Unsupported max val = %d.%d", max.major, max.minor);

   return result;
}

bool vcp_version_gt(Version_Spec val, Version_Spec min) {
   return !vcp_version_le(val,min);
}





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



void vcp_list_feature_codes(FILE * fh) {
   fprintf(fh, "Recognized VCP feature codes:\n");
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

      fprintf(fh, "  %02x - %-40s  %s\n",
                  entry.code,
                  get_non_version_specific_feature_name(&entry),
                  // vcp_interpret_feature_flags(entry.flags, buf, 200)   // *** TODO: HOW TO HANDLE THIS w/o version?
                  buf2
             );
   }
}







Byte valid_versions(VCP_Feature_Table_Entry * pentry) {
   Byte result = 0x00;

   if (pentry->v20_flags)
      result |= MCCS_V20;
   if (pentry->v21_flags) {
      if ( !(pentry->v21_flags & VCP2_DEPRECATED) )
         result |= MCCS_V21;
   }
   else {
      if (result & MCCS_V20)
         result |= MCCS_V21;
   }
   if (pentry->v30_flags) {
      if ( !(pentry->v30_flags & VCP2_DEPRECATED) )
         result |= MCCS_V30;
   }
   else {
      if (result & MCCS_V21)
         result |= MCCS_V30;
   }
   if (pentry->v22_flags) {
      if ( !(pentry->v22_flags & VCP2_DEPRECATED) )
         result |= MCCS_V22;
   }
   else {
      if (result & MCCS_V21)
         result |= MCCS_V22;
   }
   return result;
}



char * valid_version_names_r(Byte valid_version_flags, char * version_name_buf, int bufsz) {
   assert(bufsz >= (4*5));        // max 4 version names, 5 chars/name
   *version_name_buf = '\0';

   if (valid_version_flags & MCCS_V20)
      strcpy(version_name_buf, "2.0");
   if (valid_version_flags & MCCS_V21) {
      if (strlen(version_name_buf) > 0)
         strcat(version_name_buf, ", ");
      strcat(version_name_buf, "2.1");
   }
   if (valid_version_flags & MCCS_V30) {
      if (strlen(version_name_buf) > 0)
         strcat(version_name_buf, ", ");
      strcat(version_name_buf, "3.0");
   }
   if (valid_version_flags & MCCS_V22) {
      if (strlen(version_name_buf) > 0)
         strcat(version_name_buf, ", ");
      strcat(version_name_buf, "2.2");
   }

   return version_name_buf;
}


char * str_comma_cat_r(char * val, char * buf, int bufsz) {
   int cursz = strlen(buf);
   assert(cursz + 2 + strlen(val) + 1 <= bufsz);
   if (cursz > 0)
      strcat(buf, ", ");
   strcat(buf, val);
   return buf;
}

char * spec_group_names_r(VCP_Feature_Table_Entry * pentry, char * buf, int bufsz) {
   *buf = '\0';
   if (pentry->vcp_spec_groups & VCP_SPEC_PRESET)
      str_comma_cat_r("Preset", buf, bufsz);
   if (pentry->vcp_spec_groups & VCP_SPEC_IMAGE)
      str_comma_cat_r("Image", buf, bufsz);
   if (pentry->vcp_spec_groups & VCP_SPEC_CONTROL)
      str_comma_cat_r("Control", buf, bufsz);
   if (pentry->vcp_spec_groups & VCP_SPEC_GEOMETRY)
      str_comma_cat_r("Geometry", buf, bufsz);
   if (pentry->vcp_spec_groups & VCP_SPEC_MISC)
      str_comma_cat_r("Miscellaneous", buf, bufsz);
   if (pentry->vcp_spec_groups & VCP_SPEC_AUDIO)
      str_comma_cat_r("Audio", buf, bufsz);
   if (pentry->vcp_spec_groups & VCP_SPEC_DPVL)
      str_comma_cat_r("DPVL", buf, bufsz);
   if (pentry->vcp_spec_groups & VCP_SPEC_MFG)
      str_comma_cat_r("Manufacturer specific", buf, bufsz);
   if (pentry->vcp_spec_groups & VCP_SPEC_WINDOW)
      str_comma_cat_r("Window", buf, bufsz);
   return buf;
}

char * subset_names_r(VCP_Feature_Table_Entry * pentry, char * buf, int bufsz) {
   *buf = '\0';
   if (pentry->vcp_subsets & VCP_SUBSET_PROFILE)
      str_comma_cat_r("PROFILE", buf, bufsz);
   if (pentry->vcp_subsets & VCP_SUBSET_COLOR)
      str_comma_cat_r("COLOR", buf, bufsz);
   if (pentry->vcp_subsets & VCP_SUBSET_LUT)
      str_comma_cat_r("LUT", buf, bufsz);
   if (pentry->vcp_subsets & VCP_SUBSET_CRT)
      str_comma_cat_r("CRT", buf, bufsz);
   if (pentry->vcp_subsets & VCP_SUBSET_TV)
      str_comma_cat_r("TV", buf, bufsz);
   if (pentry->vcp_subsets & VCP_SUBSET_AUDIO)
      str_comma_cat_r("AUDIO", buf, bufsz);
   if (pentry->vcp_subsets & VCP_SUBSET_WINDOW)
      str_comma_cat_r("WINDOW", buf, bufsz);
   if (pentry->vcp_subsets & VCP_SUBSET_DPVL)
      str_comma_cat_r("DPVL", buf, bufsz);
   return buf;
}

#ifdef REF
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


#endif


void report_sl_values(Feature_Value_Entry * sl_values, int depth) {
   while (sl_values->value_name != NULL) {
      rpt_vstring(depth, "0x%02x: %s", sl_values->value_code, sl_values->value_name);
      sl_values++;
   }
}

char * interpret_feature_flags_r(Version_Feature_Flags vflags, char * workbuf, int bufsz) {
   bool debug = false;
   DBGMSF(debug, "vflags=0x%04x", vflags);
   assert(bufsz >= 50);     //  bigger than we'll need
   *workbuf = '\0';
   if (vflags & VCP2_DEPRECATED) {
      strcpy(workbuf, "Deprecated");
   }
   else {
     if (vflags & VCP2_RO)
        strcpy(workbuf, "Read Only, ");
     else if (vflags & VCP2_WO)
        strcpy(workbuf, "Write Only, ");
     else if (vflags & VCP2_RW)
        strcpy(workbuf, "Read Write, ");
     else
        PROGRAM_LOGIC_ERROR("No read/write bits set");

     if (vflags & VCP2_STD_CONT)
        strcat(workbuf, "Continuous (standard)");
     else if (vflags & VCP2_COMPLEX_CONT)
        strcat(workbuf, "Continuous (complex)");
     else if (vflags & VCP2_SIMPLE_NC)
        strcat(workbuf, "Non-Continuous (simple)");
     else if (vflags & VCP2_COMPLEX_NC)
        strcat(workbuf, "Non-Continuous (complex)");
     else if (vflags & VCP2_WO_NC)
        strcat(workbuf, "Non-Continuous (write-only)");
     else if (vflags & VCP2_ANY_TABLE)
        strcat(workbuf, "Table");
     else
        PROGRAM_LOGIC_ERROR("No type bits set");
   }
   return workbuf;
}

// report function specifically for use by report_vcp_feature_table_entry()
void report_feature_table_entry_flags(
        VCP_Feature_Table_Entry * pentry,
        Version_Spec              vcp_version,
        int                       depth)
{
   char workbuf[200];
   Version_Feature_Flags vflags = get_version_specific_feature_flags(pentry, vcp_version);
   if (vflags) {
      interpret_feature_flags_r(vflags, workbuf, sizeof(workbuf));
      rpt_vstring(depth, "Attributes (v%d.%d): %s", vcp_version.major, vcp_version.minor, workbuf);
   }
}

void report_vcp_feature_table_entry(VCP_Feature_Table_Entry * pentry, int depth) {
   char workbuf[200];

   int d1 = depth+1;
   Output_Level output_level = get_output_level();
   Version_Spec vspec = get_highest_non_deprecated_version(pentry);
   Version_Feature_Flags vflags = get_version_specific_feature_flags(pentry, vspec);
   char * feature_name = get_non_version_specific_feature_name(pentry);
   rpt_vstring(depth, "VCP code %02X: %s", pentry->code, feature_name);
   rpt_vstring(d1, "%s", pentry->desc);
   valid_version_names_r(valid_versions(pentry), workbuf, sizeof(workbuf));
   rpt_vstring(d1, "MCCS versions: %s", workbuf);
   rpt_vstring(d1, "MCCS specification groups: %s",
                   spec_group_names_r(pentry, workbuf, sizeof(workbuf)));
   rpt_vstring(d1, "ddctool feature subsets: %s",
                   subset_names_r(pentry, workbuf, sizeof(workbuf)));
   if (has_version_specific_features(pentry)) {
      // rpt_vstring(d1, "VERSION SPECIFIC FLAGS");
      report_feature_table_entry_flags(pentry, VCP_SPEC_V20, d1);
      report_feature_table_entry_flags(pentry, VCP_SPEC_V21, d1);
      report_feature_table_entry_flags(pentry, VCP_SPEC_V30, d1);
      report_feature_table_entry_flags(pentry, VCP_SPEC_V22, d1);
   }
   else {
      interpret_feature_flags_r(vflags, workbuf, sizeof(workbuf));
      rpt_vstring(d1, "Attributes: %s", workbuf);
   }

   if (pentry->default_sl_values && output_level >= OL_VERBOSE) {
      rpt_vstring(d1, "Simple NC values:");
      report_sl_values(pentry->default_sl_values, d1+1);
   }

}




//
// Miscellaneous VCP_Feature_Table lookup functions
//

char * get_feature_name_by_id_only(Byte feature_id) {
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
      result = get_version_sensitive_feature_name(vcp_entry, vspec);
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
 *   flags, 0 if feature is not defined for version
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
       (vcp_version.major >= 3 || (vcp_version.major == 2 && vcp_version.minor >= 1))
      )
         result = pvft_entry->v21_flags;

   if (!result)
      result = pvft_entry->v20_flags;

   DBGMSF(debug, "Feature = 0x%02x, vcp version=%d.%d, returning 0x%02x",
          pvft_entry->code, vcp_version.major, vcp_version.minor, result);
   return result;
}


bool is_feature_supported_in_version(
      VCP_Feature_Table_Entry * pvft_entry,
      Version_Spec              vcp_version)
{
   bool debug = false;
   bool result = false;
   Version_Feature_Flags vflags = get_version_specific_feature_flags(pvft_entry, vcp_version);
   result = (vflags && !(vflags&VCP2_DEPRECATED));
   DBGMSF(debug, "Feature = 0x%02x, vcp versinon=%d.%d, returning %s",
                 pvft_entry->code, vcp_version.major, vcp_version.minor, bool_repr(result) );
   return result;
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
get_version_sensitive_feature_flags(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec              vcp_version)
{
   bool debug = false;
   Version_Feature_Flags result = get_version_specific_feature_flags(pvft_entry, vcp_version);

   if (!result) {
      // vcp_version is lower than the first version level at which the field
      // was defined.  This can occur e.g. if scanning.  Pick the best
      // possible flags by scanning up in versions.
      if (pvft_entry->v21_flags)
         result = pvft_entry->v21_flags;
      else if (pvft_entry->v30_flags)
         result = pvft_entry->v30_flags;
      else if (pvft_entry->v22_flags)
         result = pvft_entry->v22_flags;
      if (!result) {
         PROGRAM_LOGIC_ERROR(
            "Feature = 0x%02x, Version=%d.%d: No version sensitive feature flags found",
            pvft_entry->code, vcp_version.major, vcp_version.minor);
      }
   }

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
   bool result = (get_version_sensitive_feature_flags(pvft_entry, vcp_version) & VCP2_READABLE );
   // DBGMSG("code=0x%02x, vcp_version=%d.%d, returning %d",
   //        pvft_entry->code, vcp_version.major, vcp_version.minor, result);
   return result;
}

// convenience function
bool is_feature_writable_by_vcp_version(
       VCP_Feature_Table_Entry * pvft_entry,
       Version_Spec vcp_version)
{
   return (get_version_sensitive_feature_flags(pvft_entry, vcp_version) & VCP2_WRITABLE );
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
      result = pvft_entry->default_sl_values;

   DBGMSF(debug, "Feature = 0x%02x, vcp version=%d.%d, returning %p",
          pvft_entry->code, vcp_version.major, vcp_version.minor, result);
   return result;
}



char * get_version_sensitive_feature_name(
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
      // vcp_version is lower than the first version level at which the field
      // was defined.  This can occur e.g. if scanning.  Pick the best
      // possible name by scanning up in versions.
      if (pvft_entry->v21_name)
         result = pvft_entry->v21_name;
      else if (pvft_entry->v30_name)
         result = pvft_entry->v30_name;
      else if (pvft_entry->v22_name)
         result = pvft_entry->v22_name;
      if (!result)
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
   return get_version_sensitive_feature_name(pvft_entry, vspec);
}


//
// Functions that return a function for formatting a feature value
//

// Functions that lookup a value contained in a VCP_Feature_Table_Entry,
// returning a default if the value is not set for that entry.

Format_Normal_Feature_Detail_Function
get_nontable_feature_detail_function(
   VCP_Feature_Table_Entry * pvft_entry,
   Version_Spec vcp_version)
{
   assert(pvft_entry);
   bool debug = false;
   DBGMSF(debug, "Starting");

   Version_Feature_Flags version_specific_flags =
         get_version_sensitive_feature_flags(pvft_entry, vcp_version);
   assert(version_specific_flags);
   assert(version_specific_flags & VCP2_NON_TABLE);
   Format_Normal_Feature_Detail_Function func = NULL;
   if (version_specific_flags & VCP2_STD_CONT)
      func = format_feature_detail_standard_continuous;
   else if (version_specific_flags & VCP2_SIMPLE_NC)
      func = format_feature_detail_sl_lookup;
   else if (version_specific_flags & VCP2_WO_NC)
      func = NULL;      // but should never be called for this case
   else {
      assert(version_specific_flags & (VCP2_COMPLEX_CONT | VCP2_COMPLEX_NC));
      func = pvft_entry->nontable_formatter;
      assert(func);
   }

   DBGMSF(debug, "Returning: %p", func);
   return func;
}



Format_Table_Feature_Detail_Function
get_table_feature_detail_function( VCP_Feature_Table_Entry * pvft_entry, Version_Spec vcp_version) {
   assert(pvft_entry);

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
        Parsed_Nontable_Vcp_Response *    code_info,
        char *                    buffer,
        int                       bufsz)
{
   bool debug = false;
   DBGMSF(debug, "Starting. Code=0x%02x, vcp_version=%d.%d",
                 vcp_entry->code, vcp_version.major, vcp_version.minor);

   Format_Normal_Feature_Detail_Function ffd_func =
         get_nontable_feature_detail_function(vcp_entry, vcp_version);
   bool ok = ffd_func(code_info, vcp_version,  buffer, bufsz);
   return ok;
}

bool vcp_format_table_feature_detail(
       VCP_Feature_Table_Entry * vcp_entry,
       Version_Spec              vcp_version,
       Buffer *                  accumulated_value,
       char * *                  aformatted_data
     )
{
   Format_Table_Feature_Detail_Function ffd_func =
         get_table_feature_detail_function(vcp_entry, vcp_version);
   bool ok = ffd_func(accumulated_value, vcp_version, aformatted_data);
   return ok;
}


bool vcp_format_feature_detail(
       VCP_Feature_Table_Entry * vcp_entry,
       Version_Spec              vcp_version,
       Parsed_Vcp_Response *     raw_data,
       char * *                  aformatted_data
     )
{
   bool debug = false;
   DBGMSF(debug, "Starting");
   bool ok = true;
   *aformatted_data = NULL;

   char * formatted_data = NULL;
   if (raw_data->response_type == NON_TABLE_VCP_CALL) {
      char workbuf[200];
      ok = vcp_format_nontable_feature_detail(
              vcp_entry,
              vcp_version,
              raw_data->non_table_response,
              workbuf,
              200);
      if (ok)
         formatted_data = strdup(workbuf);
   }
   else {        // TABLE_VCP_CALL
      ok = vcp_format_table_feature_detail(
            vcp_entry,
            vcp_version,
            raw_data->table_response,
            &formatted_data);
   }

   if (ok) {
      *aformatted_data = formatted_data;
      assert(*aformatted_data);
   }
   else {
      if (formatted_data)
         free(formatted_data);
      assert(!*aformatted_data);
   }

   DBGMSF(debug, "Done.  Returning %d, *aformatted_data=%p", ok, *aformatted_data);
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
VCP_Feature_Table_Entry * vcp_create_dummy_feature_for_hexid(Byte id) {
   // memory leak
   VCP_Feature_Table_Entry* pentry = calloc(1, sizeof(VCP_Feature_Table_Entry) );
   pentry->code = id;
   if (id >= 0xe0) {
      pentry->v20_name = "Manufacturer Specific";
   }
   else {
      pentry->v20_name = "Unknown feature";
   }
   pentry->nontable_formatter = format_feature_detail_debug_continuous;
   pentry->v20_flags = VCP2_RW | VCP2_STD_CONT;
   pentry->vcp_global_flags = VCP2_SYNTHETIC;   // indicates caller should free
   return pentry;
}

#ifdef DEPRECATED
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
#endif

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

#ifdef DEPRECATED
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
#endif


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
//            assert(pentry->v20_sl_values);
//            result = pentry->v20_sl_values;
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
char * get_feature_value_name(Feature_Value_Entry * value_entries, Byte value_id) {
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
   char * name = get_feature_value_name(values_for_feature, sl_value);
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
        Parsed_Nontable_Vcp_Response * code_info,  Version_Spec vcp_version, char * buffer, int bufsz)
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
        Parsed_Nontable_Vcp_Response * code_info,  Version_Spec vcp_version, char * buffer, int bufsz)
{
   snprintf(buffer, bufsz,
            "mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x, max value = %5d, cur value = %5d",
            code_info->mh,        code_info->ml,
            code_info->sh,        code_info->sl,
            code_info->max_value, code_info->cur_value);
   return true;
}


bool format_feature_detail_debug_bytes(
        Parsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
        Parsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
bool format_feature_detail_sl_lookup(
        Parsed_Nontable_Vcp_Response *  code_info,
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
        Parsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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



/* Standard feature detail formatting function for a feature marked
 * as Continuous for which the Sh/Sl bytes represent an integer in
 * the range 0..65535 and max value is not relevant.
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
bool format_feature_detail_ushort(
        Parsed_Nontable_Vcp_Response * code_info,
        Version_Spec                   vcp_version,
        char *                         buffer,
        int bufsz)
{
   int cv = code_info->cur_value;
   snprintf(buffer, bufsz, "%5d (0x%04x)", cv, cv);
   return true;
}




//
// Functions for specific non-table VCP Feature Codes
//

// 0x02
bool format_feature_detail_new_control_value(    // 0x02
        Parsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
      Parsed_Nontable_Vcp_Response * code_info,
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
      Parsed_Nontable_Vcp_Response * code_info,
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
      Parsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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
      sl_msg = get_feature_value_name(x14_color_preset_absolute_values, code_info->sl);
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


// 0x62
bool format_feature_detail_audio_speaker_volume_v30(
      Parsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
  assert (code_info->vcp_code == 0x62);
  // Continous in 2.0, 2,2, assume 2.1 is same
  // NC with special x00 and xff values in 3.0
  assert(vcp_version.major >= 3);

  // leave v2 code in case logic changes
  if (vcp_version.major < 3)
  {
     snprintf(buffer, bufsz, "%d", code_info->sl);
  }
  else {
     if (code_info->sl == 0x00)
        snprintf(buffer, bufsz, "Fixed (default) level (0x00)" );
     else if (code_info->sl == 0xff)
        snprintf(buffer, bufsz, "Mute (0xff)");
     else
        snprintf(buffer, bufsz, "Volume level: %d (00x%02x)", code_info->sl, code_info->sl);
  }
  return true;
}

bool format_feature_detail_x8d_v22_mute_audio_blank_screen(
        Parsed_Nontable_Vcp_Response * code_info,
        Version_Spec                   vcp_version,
        char *                         buffer,
        int                            bufsz)
{
   assert (code_info->vcp_code == 0x8d);
   assert (vcp_version.major == 2 && vcp_version.minor >= 2);

   // As of v2.2, SH byte contains screen blank settings

   Feature_Value_Entry * sl_values = x8d_tv_audio_mute_source_values;
   Feature_Value_Entry * sh_values = x8d_sh_blank_screen_values;

   char * sl_name = get_feature_value_name(sl_values, code_info->sl);
   char * sh_name = get_feature_value_name(sh_values, code_info->sh);
   if (!sl_name)
      sl_name = "Invalid value";
   if (!sh_name)
      sh_name = "Invalid value";

   snprintf(buffer, bufsz,"%s (sl=0x%02x), %s (sh=0x%02x)",
            sl_name, code_info->sl,
            sh_name, code_info->sh);
   return true;
}


// 0x8f, 0x91
bool format_feature_detail_audio_treble_bass_v30(
      Parsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
  assert (code_info->vcp_code == 0x8f || code_info->vcp_code == 0x91);
  // Continous in 2.0, assume 2.1 same as 2.0,
  // NC with reserved x00 and special xff values in 3.0, 2.2
  // This function should not be called if VCP2_STD_CONT

  assert ( vcp_version_gt(vcp_version, VCP_SPEC_V21) );
  // leave v2 code in in case things change
  bool ok = true;
  if ( vcp_version_le(vcp_version, VCP_SPEC_V21))
  {
     snprintf(buffer, bufsz, "%d", code_info->sl);
  }
  else {
     if (code_info->sl == 0x00) {
        snprintf(buffer, bufsz, "Invalid value: 0x00" );
        ok = false;
     }
     else if (code_info->sl < 0x80)
        snprintf(buffer, bufsz, "%d: Decreased (0x%02x, neutral - %d)",
                 code_info->sl, code_info->sl, 0x80 - code_info->sl);
     else if (code_info->sl == 0x80)
        snprintf(buffer, bufsz, "%d: Neutral (0x%02x)",
                 code_info->sl, code_info->sl);
     else
        snprintf(buffer, bufsz, "%d: Increased (0x%02x, neutral + %d)",
                 code_info->sl, code_info->sl, code_info->sl - 0x80);
  }
  return ok;
}


// 0x93
bool format_feature_detail_audio_balance_v30(
      Parsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
  assert (code_info->vcp_code == 0x93);
  // Continous in 2.0, NC in 3.0, 2.2, assume 2.1 same as 2.0
  // NC with reserved x00 and special xff values in 3.0,
  // This function should not be called if VCP2_STD_CONT
  assert ( vcp_version_gt(vcp_version, VCP_SPEC_V21) );
  // leave v2 code in in case things change
  bool ok = true;
  if ( vcp_version_le(vcp_version, VCP_SPEC_V21))
  {
     snprintf(buffer, bufsz, "%d", code_info->sl);
  }
  else {
     if (code_info->sl == 0x00) {
        snprintf(buffer, bufsz, "Invalid value: 0x00" );
        ok = false;
     }
     else if (code_info->sl < 0x80)
        snprintf(buffer, bufsz, "%d: Left channel dominates (0x%02x, centered - %d)",
                 code_info->sl, code_info->sl, 0x80-code_info->sl);
     else if (code_info->sl == 0x80)
        snprintf(buffer, bufsz, "%d: Centered (0x%02x)",
                 code_info->sl, code_info->sl);
     else
        snprintf(buffer, bufsz, "%d Right channel dominates (0x%02x, centered + %d)",
                 code_info->sl, code_info->sl, code_info->sl-0x80);
  }
  return ok;
}


// 0xac
bool format_feature_detail_xac_horizontal_frequency(
      Parsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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



// This function implements the MCCS interpretation in MCCS 2.0 and 3.0.
// However, the Dell U3011 returns a "nominal" value of 50 and a max
// value of 100.  Therefore, this function is not used.  Instead, the
// 6-axis hue values are interpreted as standard continuous values.

// 0x9b..0xa0
bool format_feature_detail_6_axis_hue(
      Parsed_Nontable_Vcp_Response * code_info,
      Version_Spec                   vcp_version,
      char *                         buffer,
      int                            bufsz)
{
   Byte vcp_code = code_info->vcp_code;
   Byte sl       = code_info->sl;

   assert (0x9b <= vcp_code && vcp_code <= 0xa0);

   struct Names {
      Byte   id;
      char * hue_name;
      char * more_name;
      char * less_name;
   };

   struct Names names[] = {
         {0x9b,  "red",     "yellow",  "magenta"},
         {0x9c,  "yellow",  "green",   "red"},
         {0x9d,  "green",   "cyan",    "yellow"},
         {0x9e,  "cyan",    "blue",    "green"},
         {0x9f,  "blue",    "magenta", "cyan"},
         {0xa0,  "magenta", "red",     "blue"},
   };

   struct Names curnames = names[vcp_code-0x9b];

   if (sl < 0x7f)
      snprintf(buffer, bufsz, "%d: Shift towards %s (0x%02x, nominal-%d)",
               sl, curnames.less_name, sl, 0x7f-sl);
   else if (sl == 0x7f)
      snprintf(buffer, bufsz, "%d: Nominal (default) value (0x%02x)",
               sl, sl);
   else
      snprintf(buffer, bufsz, "%d Shift towards %s (0x%02x, nominal+%d)",
               sl, curnames.more_name, sl, sl-0x7f);

   return true;
}


// 0xae
bool format_feature_detail_xae_vertical_frequency(
      Parsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
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


// 0xbe
bool format_feature_detail_xbe_link_control(
        Parsed_Nontable_Vcp_Response * code_info,
        Version_Spec vcp_version,
        char * buffer,
        int bufsz)
{
   // test bit 0
   // but in MCCS spec is bit 0 the high order bit or the low order bit?,
   // i.e. 0x80 or 0x01?
   // since spec refers to "Bits 7..1 reserved", implies that 0 is least
   // significant bit
   char * s = (code_info->sl & 0x01) ? "enabled" : "disabled";
   snprintf(buffer, bufsz, "Link shutdown is %s (0x%02x)", s, code_info->sl);

   return true;
}

// 0xc0
bool format_feature_detail_xc0_display_usage_time(
        Parsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert (code_info->vcp_code == 0xc0);
   uint usage_time;
   // DBGMSG("vcp_version=%d.%d", vcp_version.major, vcp_version.minor);

   // TODO: Control with Output_Level
   // v2 spec says this is a 2 byte value, says nothing about mh, ml
   if (vcp_version.major >= 3) {
      if (code_info->mh != 0x00) {
         // FIXME: *** DIRECT WRITE TO SYSOUT ***
         printf("(%s) Data error.  Mh byte = 0x%02x, should be 0x00 for display usage time\n",
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
        Parsed_Nontable_Vcp_Response * code_info,
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
        Parsed_Nontable_Vcp_Response * info,  Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert(info->vcp_code == 0xc8);
   bool ok = true;
   Byte mfg_id = info->sl;
   char *sl_msg = NULL;
   sl_msg = get_feature_value_name(xc8_display_controller_type_values, info->sl);
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
        Parsed_Nontable_Vcp_Response * code_info, Version_Spec vcp_version, char * buffer, int bufsz)
{
   int version_number  = code_info->sh;
   int revision_number = code_info->sl;
   snprintf(buffer, bufsz, "%d.%d", version_number, revision_number);
   return true;
}

// 0xce
bool format_feature_detail_xce_aux_display_size(
        Parsed_Nontable_Vcp_Response * code_info,
        Version_Spec                   vcp_version,
        char *                         buffer,
        int                            bufsz)
{
   assert (code_info->vcp_code == 0xce);

   int rows = (code_info->sl & 0xc0) >> 6;
   int chars_per_row = code_info->sl & 0x3f;
   snprintf(buffer, bufsz, "Rows=%d, characters/row=%d (sl=0x%02x)",
            rows, chars_per_row, code_info->sl);
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
// see also EloView Remote Mgt Local Cmd Set document
Feature_Value_Entry x60_v2_input_source_values[] = {
      {0x01,  "VGA-1"},    // aka Analog video (R/G/B) 1
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

// 0x8b
Feature_Value_Entry x8b_tv_channel_values[] = {
      {0x01, "Increment channel"},
      {0x02, "Decrement channel"},
      {0x00, NULL}
};

// 0x8d: Audio Mute
static Feature_Value_Entry x8d_tv_audio_mute_source_values[] = {
      {0x01, "Mute the audio"},
      {0x02, "Unmute the audio"},
      {0x00,  NULL}
};

// 0x8d: SH byte values only apply in v2.2
static Feature_Value_Entry x8d_sh_blank_screen_values[] = {
      {0x01, "Blank the screen"},
      {0x02, "Unblank the screen"},
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



// 0x94
Feature_Value_Entry x94_audio_stereo_mode_values[] = {
      {0x00,  "Speaker off/Audio not supported"},
      {0x01,  "Mono"},
      {0x02,  "Stereo"},
      {0x03,  "Stereo expanded"},
      // end of v20 values
      // 3.0 values:
      {0x11,  "SRS 2.0"},
      {0x12,  "SRS 2.1"},
      {0x13,  "SRS 3.1"},
      {0x14,  "SRS 4.1"},
      {0x15,  "SRS 5.1"},
      {0x16,  "SRS 6.1"},
      {0x17,  "SRS 7.1"},

      {0x21,  "Dolby 2.0"},
      {0x22,  "Dolby 2.1"},
      {0x23,  "Dolby 3.1"},
      {0x24,  "Dolby 4.1"},
      {0x25,  "Dolby 5.1"},
      {0x26,  "Dolby 6.1"},
      {0x27,  "Dolby 7.1"},

      {0x31,  "THX 2.0"},
      {0x32,  "THX 2.1"},
      {0x33,  "THX 3.1"},
      {0x34,  "THX 4.1"},
      {0x35,  "THX 5.1"},
      {0x36,  "THX 6.1"},
      {0x37,  "THX 7.1"},
      // end of v3.0 values
      {0x00,  NULL}
};



// 0x99
Feature_Value_Entry x99_window_control_values[] = {
      {0x00,  "No effect"},
      {0x01,  "Off"},
      {0x02,  "On"},
      {0x00,  NULL}
};


// 0xa2
Feature_Value_Entry xa2_auto_setup_values[] = {
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

// 0xa5
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


// 0xb0
static  Feature_Value_Entry xb0_settings_values[] =
   {
     {0x01, "Store current settings in the monitor"},
     {0x02, "Restore factory defaults for current mode"},
     {0x00, NULL}    // termination entry
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
   // end of MCCS 3.0 values, beginning of values added in 2.2:
   {0x14,  "Silicon Optics"},
   {0x15,  "Texas Instruments"},
   {0x16,  "Analogix"},
   {0x17,  "Quantum Data"},
   {0x18,  "NXP Semiconductors"},
   {0x19,  "Chrontel"},
   {0x1a,  "Parade Technologies"},
   {0x1b,  "THine Electronics"},
   {0x1c,  "Trident"},
   {0x1d,  "Micros"},
   // end of values added in MCCS 2.2
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
// Note in v2.2 spec:
//   Typo in Version 2.1.  10h should read 0Ah.  If a parser
//   encounters a display with MCCS v2.1 using 10h it should
//   auto-correct to 0Ah.
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
          {0x00,  NULL}       // end of list marker
};

// TODO: consolidate with x60_v2_input_source_values
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
      {0x0f,  "DisplayPort-1"},
      {0x10,  "DisplayPort-2"},
      {0x11,  "HDMI-1"},
      {0x12,  "HDMI-2"},
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

// 0xdb
static  Feature_Value_Entry xdb_image_mode_values[] =
   { {0x00, "No effect"},
     {0x01, "Full mode"},
     {0x02, "Zoom mode"},
     {0x03, "Squeeze mode" },
     {0x04, "Variable"},
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
// not suppressing warning, why?, but removing static does avoid warning
#pragma GCC diagnostic ignored "-Wunused-variable"
// 0xde         // write-only feature
Feature_Value_Entry xde_wo_operation_mode_values[] =
   { {0x01, "Stand alone"},
     {0x02, "Slave (full PC control)"},
     {0x00, NULL}    // termination entry
};
#pragma GCC diagnostic pop


//
// DDC Virtual Control Panel (VCP) Feature Code Table
//

//TODO:
// In 2.0 spec, only the first letter of the first word of a name is capitalized
// In 3.0/2.2, the first letter of each word of a name is capitalized
// Need to make this consistent thoughout the table

VCP_Feature_Table_Entry vcp_code_table[] = {
   {  .code=0x01,
      // defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_MISC,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Causes a CRT to perform a degauss cycle",
      .v20_flags = VCP2_WO |VCP2_WO_NC,
      .v20_name = "Degauss",
   },
   {  .code=0x02,
      .vcp_spec_groups = VCP_SPEC_MISC,
      // defined in 2.0, identical in 3.0, 2.2
      .nontable_formatter = format_feature_detail_new_control_value,   // ??
      .default_sl_values = x02_new_control_values,
      .desc = "Indicates that a display user control (other than power) has been "
              "used to change and save (or autosave) a new value.",
      .v20_flags = VCP2_RW | VCP2_COMPLEX_NC,
      .v20_name = "New control value",
   },
   {  .code=0x03,                        // defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_MISC,
      .default_sl_values = x03_soft_controls_values,
      .desc = "Allows display controls to be used as soft keys",
      .v20_flags =  VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "Soft controls",
   },
   {  .code=0x04,                        // Defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_PRESET,
      .desc = "Restore all factor presets including brightness/contrast, "
              "geometry, color, and TV defaults.",
      .vcp_subsets = VCP_SUBSET_COLOR,                // but note WO
      .v20_flags =  VCP2_WO | VCP2_WO_NC,
      .v20_name = "Restore factory defaults",
   },
   {  .code=0x05,                        // Defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_PRESET,
      .vcp_subsets = VCP_SUBSET_COLOR,                // but note WO
      .desc = "Restore factory defaults for brightness and contrast",
      .v20_flags =  VCP2_WO | VCP2_WO_NC,
      .v20_name = "Restore factory brightness/contrast defaults",
   },
   {  .code=0x06,                        // Defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_PRESET,
      .desc = "Restore factory defaults for geometry adjustments",
      .v20_flags =  VCP2_WO | VCP2_WO_NC,
      .v20_name = "Restore factory geometry defaults",
   },
   {  .code=0x08,                        // Defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_PRESET,
      .vcp_subsets = VCP_SUBSET_COLOR,                   // but note WO
      .desc = "Restore factory defaults for color settings.",
      .v20_flags =  VCP2_WO | VCP2_WO_NC,
      .v20_name = "Restore color defaults",
   },
   {  .code=0x0A,                        // Defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_PRESET,
      .vcp_subsets = VCP_SUBSET_TV,
      .desc = "Restore factory defaults for TV functions.",
      .v20_flags =  VCP2_WO | VCP2_WO_NC,
      .v20_name = "Restore factory TV defaults",
   },
   {  .code=0x0b,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // Defined in 2.0
      .nontable_formatter=x0b_format_feature_detail_color_temperature_increment,
      // from 2.0 spec:
      // .desc="Allows the display to specify the minimum increment in which it can "
      //       "adjust the color temperature.",
      // simpler:
      .desc="Color temperature increment used by feature 0Ch Color Temperature Request",
      .vcp_subsets = VCP_SUBSET_COLOR,
      .v20_flags =  VCP2_RO | VCP2_COMPLEX_NC,
      .v20_name="Color temperature increment",
   },
   {  .code=0x0c,
      //.name="Color temperature request",
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // Defined in 2.0
      .nontable_formatter=x0c_format_feature_detail_color_temperature_request,
      .desc="Specifies a color temperature (degrees Kelvin)",   // my desc
      .vcp_subsets = VCP_SUBSET_COLOR,
      .v20_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .v20_name="Color temperature request",
   },
   {  .code=0x0e,                              // Defined in 2.0
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .desc="Increase/decrease the sampling clock frequency.",
      .v20_flags =  VCP2_RW | VCP2_STD_CONT,
      .v20_name="Clock",
   },
   {  .code=0x10,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // Defined in 2.0, name changed in 3.0, what is it in 2.1?
      //.name="Luminosity",
     .desc="Increase/decrease the brightness of the image.",
     .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
     .v20_flags =  VCP2_RW | VCP2_STD_CONT,
     .v20_name = "Brightness",
     .v30_name = "Luminosity",
   },
   {  .code=0x11,
      // not in 2.0, is in 3.0, assume introduced in 2.1
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .nontable_formatter=format_feature_detail_debug_bytes,
      .desc = "Select contrast enhancement algorithm respecting flesh tone region",
      .vcp_subsets = VCP_SUBSET_COLOR,
      .v21_flags = VCP2_RW | VCP2_COMPLEX_NC,
      .v21_name = "Flesh tone enhancement",
   },
   {  .code=0x12,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // Defined in 2.0, identical in 3.0
      .desc="Increase/decrease the contrast of the image.",
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Contrast",
   },
   {  .code=0x13,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // not in 2.0, is in 3.0, assume first defined in 2.1
      // deprecated in 2.2
      .nontable_formatter=format_feature_detail_debug_bytes,
      .desc = "Increase/decrease the specified backlight control value",
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v21_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .v21_name  = "Backlight control",
      .v22_flags = VCP2_DEPRECATED,
   },
   {  .code=0x14,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // Defined in 2.0, different in 3.0, 2.2
      // what is appropriate choice for 2.1 ?
      // interpretation varies depending on VCP version
      .nontable_formatter=format_feature_detail_select_color_preset,
      .default_sl_values= x14_color_preset_absolute_values,
      .desc="Select a specified color temperature",
      .vcp_subsets = VCP_SUBSET_COLOR,
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name  = "Select color preset",
      .v30_flags = VCP2_RW | VCP2_COMPLEX_NC,
      .v22_flags = VCP2_RW | VCP2_COMPLEX_NC,
   },
   {  .code=0x16,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .desc="Increase/decrease the luminesence of red pixels",   // my simplification
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Video gain: Red",
   },
   {  .code=0x17,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // not in 2.0, is in 3.0 and 2.2, assume valid for 2.1
      .desc="Increase/decrease the degree of compensation",   // my simplification
      .vcp_subsets = VCP_SUBSET_COLOR,
      .v21_flags = VCP2_RW | VCP2_STD_CONT,
      .v21_name = "User color vision compensation",
   },
   {  .code=0x18,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .desc="Increase/decrease the luminesence of green pixels",   // my simplification
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Video gain: Green",
   },
   {  .code=0x1a,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .desc="Increase/decrease the luminesence of blue pixels",   // my simplification
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Video gain: Blue",
   },
   {  .code=0x1c,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // defined in 2.0, identical in 3.0
      .desc="Increase/decrease the focus of the image",  // my simplification
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Focus",
   },
   {  .code=0x1e,                                                // Done
      .vcp_spec_groups = VCP_SPEC_IMAGE,    // 2.0, 3.0, 2.2
      // Defined in 2.0, additional value in 3.0, 2.2
      .default_sl_values = x1e_x1f_auto_setup_values,
      .desc="Perform autosetup function (H/V position, clock, clock phase, "
            "A/D converter, etc.",
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "Auto setup",
   },
   {  .code=0x1f,                                               // Done
      // not defined in 2.0, is defined in 3.0, 2.2, assume introduced in 2.1
      .vcp_spec_groups = VCP_SPEC_IMAGE,   // 3.0
      .default_sl_values = x1e_x1f_auto_setup_values,
      .desc="Perform color autosetup function (R/G/B gain and offset, A/D setup, etc. ",
      .vcp_subsets = VCP_SUBSET_COLOR,
      .v21_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v21_name = "Auto color setup",
   },
   {  .code=0x20,        // Done
      // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2 except for name
      // When did name change to include "(phase)"?  Assuming 2.1
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increasing (decreasing) this value moves the image toward "
              "the right (left) of the display.",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name="Horizontal Position",
      .v21_name="Horizontal Position (Phase)",
   },
   {  .code=0x22,         // Done
      // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increase/decrease the width of the image.",
      .v20_name="Horizontal Size",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
   },
   {  .code=0x24,       // Done
      // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increasing (decreasing) this value causes the right and left "
              "sides of the image to become more (less) convex.",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name="Horizontal Pincushion",
   },
   {  .code=0x26,              // Done
      // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increasing (decreasing) this value moves the center section "
              "of the image toward the right (left) side of the display.",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name="Horizontal Pincushion Balance",
   },
   {  .code=0x28,            // Done
      // Group 8.4 Geometry, name changed in 3.0 & 2.2 vs 2.0  what should it be for 2.1?
      // assume it changed in 2.1
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // description identical in 3.0, 2.0 even though name changed
      .desc = "Increasing (decreasing) this value shifts the red pixels to "
              "the right (left) and the blue pixels left (right) across the "
              "image with respect to the green pixels.",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name="Horizontal Convergence",
      .v21_name="Horizontal Convergence R/B",
   },
   {  .code=0x29,              // Done
      // not in 2.0, when was this added?  2.1 or 3.0?   assuming 2.1
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increasing (decreasing) this value shifts the magenta pixels to "
              "the right (left) and the green pixels left (right) across the "
              "image with respect to the magenta (sic) pixels.",
      .v21_name="Horizontal Convergence M/G",
      .v21_flags=VCP2_RW | VCP2_STD_CONT,
   },
   {  .code = 0x2a,           // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry, identical in 3.0, 2.2
      .desc = "Increase/decrease the density of pixels in the image center.",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name="Horizontal Linearity",
   },
   {  .code = 0x2c,               // Done
      // Group 8.4 Geometry
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increasing (decreasing) this value shifts the density of pixels "
              "from the left (right) side to the right (left) side of the image.",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Horizontal Linearity Balance",
   },
   {  .code=0x2e,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // not defined in 2.0, is defined in 3.0
      // assume new in 2.1
      .vcp_subsets = VCP_SUBSET_COLOR,
      .nontable_formatter=format_feature_detail_debug_bytes,
      .desc = "Gray Scale Expansion",
      .v21_flags = VCP2_RW |  VCP2_COMPLEX_NC,
      .v21_name = "Gray scale expansion",
   },
   {  .code=0x30,                // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increasing (decreasing) this value moves the image toward "
              "the top (bottom) edge of the display.",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name="Vertical Position",
      .v21_name="Vertical Position (Phase)",
   },
   {  .code=0x32,                // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry.  Did name change with 2.1 or 3.0/2.2? - assuming 2.1
      .desc = "Increase/decreasing the height of the image.",
      .v20_flags=VCP2_RW |  VCP2_STD_CONT,
      .v20_name="Vertical Size",
   },
   {  .code=0x34,                                  // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry.  Identical in 2.0, 3.0, 2.2
      .desc = "Increasing (decreasing) this value will cause the top and "
              "bottom edges of the image to become more (less) convex.",
      .v20_flags =  VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Vertical Pincushion",
   },
   {  .code=0x36,                                 // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increasing (decreasing) this value will move the center "
              "section of the image toward the top (bottom) edge of the display.",
      .v20_flags =  VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Vertical Pincushion Balance",
   },
   {  .code=0x38,                                 // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry.  Assume name changed with 2.1
      .desc = "Increasing (decreasing) this value shifts the red pixels up (down) "
              "across the image and the blue pixels down (up) across the image "
              "with respect to the green pixels.",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Vertical Convergence",
      .v21_name="Vertical Convergence R/B",
   },
   {  .code=0x39,                                 // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry.  Not in 2.0.  Assume added in 2.1
      .desc = "Increasing (decreasing) this value shifts the magenta pixels up (down) "
              "across the image and the green pixels down (up) across the image "
              "with respect to the magenta (sic) pixels.",
      .v21_name="Vertical Convergence M/G",
      .v21_flags= VCP2_RW | VCP2_STD_CONT,
   },
   {  .code=0x3a,                                  // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry
      .desc = "Increase/decease the density of scan lines in the image center.",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Vertical Linearity",
   },
   {  .code=0x3c,                                       // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry
      .desc = "Increase/decrease the density of scan lines in the image center.",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Vertical Linearity Balance",
   },
   {  .code=0x3e,
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_MISC,     // 2.0: MISC
      // Defined in 2.0, identical in 3.0
      .desc="Increase/decrease the sampling clock phase shift",
      .v20_flags =  VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Clock phase",
   },
   {  .code=0x40,                                   // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // When did name change from 2.0?   assume 2.1
      //.name="Horizontal Parallelogram",
      .desc = "Increasing (decreasing) this value shifts the top section of "
              "the image to the right (left) with respect to the bottom section "
              "of the image.",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Key Balance",  // 2.0
      .v21_name="Horizontal Parallelogram",
   },
   {  .code=0x41,
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // not defined in 2.0, assume defined in 2.1
      // 3.0 doc has same description for x41 as x40, is this right?
      // 2.2 spec has the same identical description in x41 and x40
      .desc = "Increasing (decreasing) this value shifts the top section of "
              "the image to the right (left) with respect to the bottom section "
              "of the image. (sic)",
      .v21_flags= VCP2_RW | VCP2_STD_CONT,
      .v21_name="Vertical Parallelogram",
   },
   {  .code=0x42,                                  // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // When did name change from 2.0?   assume 2.1
      .desc = "Increasing (decreasing) this value will increase (decrease) the "
              "ratio between the horizontal size at the top of the image and the "
              "horizontal size at the bottom of the image.",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Horizontal Trapezoid",
      .v21_name="Horizontal Keystone",   // ??
   },
   {  .code=0x43,                             // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry
      // When did name change from 2.0?   assume 2.1
      .desc = "Increasing (decreasing) this value will increase (decrease) the "
              "ratio between the vertical size at the left of the image and the "
              "vertical size at the right of the image.",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Vertical Trapezoid",
      .v21_name="Vertical Keystone",   // ??
   },
   {  .code=0x44,                                // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry
      // When did name change from 2.0?   assume 2.1
      .desc = "Increasing (decreasing) this value rotates the image (counter) "
              "clockwise around the center point of the screen.",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Tilt (rotation)",
      .v21_name="Rotation",   // ??
   },
   {  .code=0x46,                          // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry
      // When did name change from 2.0?   assume 2.1
      .desc = "Increase/decrease the distance between the left and right sides "
              "at the top of the image.",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Top Corner",
      .v21_name="Top Corner Flare",   // ??
   },
   {  .code=0x48,                              // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // name is different in 3.0, assume changed in 2.1
     //.name="Placeholder",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      // .nontable_formatter=format_feature_detail_standard_continuous,

      .desc = "Increasing (decreasing) this value moves the top of the "
            "image to the right (left).",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Top Corner Balance",
      .v21_name="Top Corner Hook",
   },
   {  .code=0x4a,                                             //Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry
      // When did name change from 2.0?   assume 2.1
      .desc = "Increase/decrease the distance between the left "
              "and right sides at the bottom of the image.",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Bottom Corner",
      .v21_name="Bottom Corner Flare",   // ??
   },
   {  .code=0x4c,                                          // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // name is different in 3.0, assume changed in 2.1
      .desc = "Increasing (decreasing) this value moves the bottom end of the "
              "image to the right (left).",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Bottom Corner Balance",
      .v21_name="Bottom Corner Hook",
   },

   // code 0x4e:
   // what is going on with 0x4e?
   // In 2.0 spec, the cross ref lists it as Trapezoid, in Table Geometry
   // However, the Geometry table lists 7E as Trapezoid
   // In 3.0 and 2.2, neither 4E or 7E are defined

   // code: 0x50
   // listed as Keystone, table GEOMETRY in 2.0
   // But Geometry table lists 0x80 as being keystone, not 0x4e
   // In 3.0 and 2.2, neither x50 nor x80 are defined

   {  .code=0x52,
      .vcp_spec_groups = VCP_SPEC_MISC,
      // defined in 2.0, 3.0 has extended consistent explanation
      .nontable_formatter = format_feature_detail_sl_byte, // TODO: write proper function
      .desc= "Read id of one feature that has changed, 0x00 indicates no more",  // my desc
      .v20_flags = VCP2_RO |  VCP2_COMPLEX_NC,
      .v20_name  = "Active control",
   },
   {  .code= 0x54,
      .vcp_spec_groups = VCP_SPEC_MISC,
      // not defined in 2.0, defined in 3.0, 2.2 identical to 3.0, assume new in 2.1
      .nontable_formatter=format_feature_detail_debug_bytes,    // TODO: write formatter
      .desc = "Controls features aimed at preserving display performance",
      .v21_flags =  VCP2_RW | VCP2_COMPLEX_NC,
      .v21_name = "Performance Preservation",
   },
   {  .code=0x56,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // defined in 2.0
     //.name="Horizontal Moire",
     //.flags=VCP_RW | VCP_CONTINUOUS,
      // .nontable_formatter=format_feature_detail_standard_continuous,

      .desc="Increase/decrease horizontal moire cancellation.",  // my simplification
      //.global_flags = VCP_RW,
      .v20_flags = VCP2_RW |  VCP2_STD_CONT,
      .v20_name="Horizontal Moire",
   },
   {  .code=0x58,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // defined in 2.0
      .desc="Increase/decrease vertical moire cancellation.",  // my simplification
      .v20_flags = VCP2_RW |  VCP2_STD_CONT,
      .v20_name="Vertical Moire",
   },
   {  .code=0x59,                                                    // DONE
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // not in 2.0, defined in 3.0, assume new as of 2.1
      .nontable_formatter=format_feature_detail_sl_byte,
      // Per spec, values range from x00..xff
      // On U3011 monitor, values range from 0..100, returned max value is 100
      // Change the .desc to fit observed reality
      // Same comments apply to other saturation codes
      //.desc = "Value < 127 decreases red saturation, 127 nominal (default) value, "
      //          "> 127 increases red saturation",
      .desc="Increase/decrease red saturation",
      .v21_flags = VCP2_RW | VCP2_STD_CONT,
      .v21_name = "6 axis saturation: Red",
   },
   {  .code=0x5a,                                                    // DONE
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // not in 2.0, defined in 3.0, assume new as of 2.1
      .nontable_formatter=format_feature_detail_sl_byte,
      // .desc = "Value < 127 decreases yellow saturation, 127 nominal (default) value, "
      //         "> 127 increases yellow saturation",
      .desc="Increase/decrease yellow saturation",
      .v21_flags = VCP2_RW | VCP2_STD_CONT,
      .v21_name = "6 axis saturation: Yellow",
   },
   {  .code=0x5b,                                                   // DONE
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // not in 2.0, defined in 3.0, assume new as of 2.1
      .nontable_formatter=format_feature_detail_sl_byte,
      // .desc = "Value < 127 decreases green saturation, 127 nominal (default) value, "
      //           "> 127 increases green saturation",
      .desc="Increase/decrease green saturation",
      .v21_flags = VCP2_RW | VCP2_STD_CONT,
      .v21_name = "6 axis saturation: Green",
   },
   {  .code=0x5c,                                                   // DONE
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // not in 2.0, defined in 3.0, assume new as of 2.1
      .nontable_formatter=format_feature_detail_sl_byte,
      // .desc = "Value < 127 decreases cyan saturation, 127 nominal (default) value, "
      //          "> 127 increases cyan saturation",
      .desc="Increase/decrease cyan saturation",
      .v21_flags = VCP2_RW | VCP2_STD_CONT,
      .v21_name = "6 axis saturation: Cyan",
   },
   {  .code=0x5d,                                                   // DONE
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // not in 2.0, defined in 3.0, assume new as of 2.1
      .nontable_formatter=format_feature_detail_sl_byte,
      // .desc = "Value < 127 decreases blue saturation, 127 nominal (default) value, "
      //          "> 127 increases blue saturation",
      .desc="Increase/decrease blue saturation",
      .v21_flags = VCP2_RW | VCP2_STD_CONT,
      .v21_name = "6 axis saturation: Blue",
   },
   {  .code=0x5e,                                                  // DONE
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // not in 2.0, defined in 3.0, assume new as of 2.1
      .nontable_formatter=format_feature_detail_sl_byte,
      // .desc = "Value < 127 decreases magenta saturation, 127 nominal (default) value, "
      //            "> 127 increases magenta saturation",
      .desc="Increase/decrease magenta saturation",
      .v21_flags = VCP2_RW | VCP2_STD_CONT,
      .v21_name = "6 axis saturation: Magenta",
   },
   {  .code=0x60,
      .vcp_spec_groups = VCP_SPEC_MISC,
      // MCCS 2.0, 2.2: NC, MCCS 3.0: T
      .default_sl_values = x60_v2_input_source_values,     // used for all but v3.0
      .desc = "Selects active video source",
      .v20_flags =  VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "Input Source",
      .v30_flags = VCP2_RW | VCP2_TABLE,
      .v22_flags = VCP2_RW | VCP2_SIMPLE_NC
   },
   {  .code=0x62,
      .vcp_spec_groups = VCP_SPEC_AUDIO,
      .vcp_subsets = VCP_SUBSET_AUDIO,
      // is in 2.0, special coding as of 3.0 assume changed as of 3.0
      // .nontable_formatter=format_feature_detail_standard_continuous,
      .nontable_formatter=format_feature_detail_audio_speaker_volume_v30,
      // requires special handling for V3, mix of C and NC, SL byte only
      .desc = "Adjusts speaker volume",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v30_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .v22_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Audio speaker volume",
   },
   {  .code=0x63,
      .vcp_spec_groups = VCP_SPEC_AUDIO,
      // not in 2.0, is in 3.0, assume new as of 2.1
      .vcp_subsets = VCP_SUBSET_AUDIO,
      .desc="Selects a group of speakers",
      .v21_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .default_sl_values = x63_speaker_select_values,
      .v21_name = "Speaker Select",
   },
   {  .code=0x64,
      .vcp_spec_groups = VCP_SPEC_AUDIO,
      // is in 2.0, n. unlike x62, this code is identically defined in 2.0, 30
      .vcp_subsets = VCP_SUBSET_AUDIO,
      .desc = "Increase/decrease microphone gain",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Audio: Microphone Volume",
   },
   {  .code=0x66,
      .vcp_spec_groups = VCP_SPEC_MISC,
      // not in 2.0, assume new in 2.1
      .nontable_formatter=format_feature_detail_debug_bytes,
      .desc = "Enable/Disable ambient light sensor",
      .v21_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v21_name = "Ambient light sensor",
      .default_sl_values = x66_ambient_light_sensor_values,
   },
   {  .code=0x6b,
      // First defined in MCCS 2.2,
      .vcp_spec_groups=VCP_SPEC_IMAGE,
      .desc="Increase/decrease the white backlight level",
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v22_name = "Backlight Level: White",
      .v22_flags = VCP2_RW | VCP2_STD_CONT,
   },
   { .code=0x6c,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
     // Defined in 2.0, name change in ?
     //.name="Video black level: Red",
     // .nontable_formatter=format_feature_detail_standard_continuous,
     .desc="Increase/decrease the black level of red pixels",  // my simplification
     .v20_flags =  VCP2_RW |VCP2_STD_CONT,
     .v20_name = "Video black level: Red",
   },
   {  .code=0x6d,
      // First defined in MCCS 2.2,
      .vcp_spec_groups=VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .desc="Increase/decrease the red backlight level",
      .v22_name = "Backlight Level: Red",
      .v22_flags = VCP2_RW | VCP2_STD_CONT,
   },
   { .code=0x6e,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      // Defined in 2.0, name change in ?
      .desc="Increase/decrease the black level of green pixels",  // my simplification
      .v20_flags =  VCP2_RW |VCP2_STD_CONT,
      .v20_name = "Video black level: Green",
   },
   {  .code=0x6f,
      // First defined in MCCS 2.2,
      .vcp_spec_groups=VCP_SPEC_IMAGE,
      .desc="Increase/decrease the green backlight level",
      // .nontable_formatter=format_feature_detail_standard_continuous,
      .v22_name = "Backlight Level: Green",
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v22_flags = VCP2_RW | VCP2_STD_CONT,
   },
   {  .code=0x70,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // Defined in 2.0, name change in ?
      .desc="Increase/decrease the black level of blue pixels",  // my simplification
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v20_flags =  VCP2_RW |VCP2_STD_CONT,
      .v20_name = "Video black level: Blue",
   },
   {  .code=0x71,
      // First defined in MCCS 2.2,
      .vcp_spec_groups=VCP_SPEC_IMAGE,
      .desc="Increase/decrease the blue backlight level",
      .v22_name = "Backlight Level: Blue",
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v22_flags = VCP2_RW | VCP2_STD_CONT,
   },
   {  .code=0x72,
      // 2.0: undefined, 3.0 & 2.2: defined, assume defined in 2.1
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      .desc="Select relative or absolute gamma",
      .nontable_formatter=format_feature_detail_debug_sl_sh,
      .v21_flags = VCP2_RW | VCP2_COMPLEX_NC,    // TODO implement function
      .v21_name = "Gamma",
   },
   {  .code=0x73,
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_LUT,
      // VCP_SPEC_MISC in 2.0, VCP_SPEC_IMAGE in 3.0, 2.1? 2.2
      .table_formatter=format_feature_detail_x73_lut_size,
      .desc = "Provides the size (number of entries and number of bits/entry) "
              "for the Red, Green, and Blue LUT in the display.",
      .v20_flags = VCP2_RO| VCP2_TABLE,
      .v20_name  = "LUT Size",
   },
   {  .code=0x74,
      // VCP_SPEC_MISC in 2.0, VCP_SPEC_IMAGE in 3.0, 2.2
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_LUT,
      .table_formatter = default_table_feature_detail_function,
      .desc = "Writes a single point within the display's LUT, reads a single point from the LUT",
      .v20_flags = VCP2_RW | VCP2_TABLE,
      .v20_name = "Single point LUT operation",
   },
   {  .code=0x75,
      // VCP_SPEC_MISC in 2.0, VCP_SPEC_IMAGE in 3.0, 2.2
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_LUT,
      .table_formatter = default_table_feature_detail_function,
      .desc = "Load (read) multiple values into (from) the display's LUT",
      .v20_flags = VCP2_RW | VCP2_TABLE,
      .v20_name = "Block LUT operation",
   },
   {  .code=0x76,
      // defined in 2.0, 3.0
      .vcp_spec_groups = VCP_SPEC_MISC,
      .vcp_subsets = VCP_SUBSET_LUT,
      .desc = "Initiates a routine resident in the display",
      .v20_flags = VCP2_WO | VCP2_WO_TABLE,
      .v20_name = "Remote Procedure Call",
   },
   {  .code=0x78,
      .vcp_spec_groups = VCP_SPEC_MISC,
      // Not in 2.0
      // defined in 3.0, 2.2 - name and description vary, but content identical
      // what to do about 2.1?  assume it's defined in 2.1 and identical to 3.0
      .desc = "Causes a selected 128 byte block of Display Identification Data "
              "(EDID or Display ID) to be read",

      .v21_flags =  VCP2_RO | VCP2_TABLE,
      .v21_name  = "EDID operation",
      .v30_flags = VCP2_RO | VCP2_TABLE,
      .v30_name  = "EDID operation",
      .v22_flags = VCP2_RO | VCP2_TABLE,
      .v22_name = "Display Identification Operation",
   },
   {  .code=0x7a,
      .vcp_spec_groups = VCP_SPEC_IMAGE,      // v2.0
      // defined in 2.0, not in 3.0 or 2.2, what to do for 2.1?
      .desc="Increase/decrease the distance to the focal plane of the image",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Adjust Focal Plane",
      .v30_flags = VCP2_DEPRECATED,
      .v22_flags = VCP2_DEPRECATED,
   },
   {  .code=0x7c,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // defined in 2.0, is in 3.0
      // my simplification, merger of 2.0 and 3.0 descriptions:
      .desc="Increase/decrease the distance to the zoom function of the projection lens (optics)",
      .v20_flags =  VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Adjust Zoom",
   },
   {  .code=0x7e,                                    // TODO: CHECK 2.2 SPEC
      // Is this really a valid v2.0 code?   See earlier comments
      // Is in V2.0 Geometry table, but not cross reference.
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // data from v2.0 spec, not in v3.0 spec
      // when was it deleted?  v3.0 or v2.1?   For safety assume 3.0
      .desc = "Increase/decrease the trapezoid distortion in the image",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v30_flags=VCP2_DEPRECATED,
      .v22_flags=VCP2_DEPRECATED,
      .v20_name="Trapezoid",
   },
   {  .code=0x80,                                    // TODO: CHECK 2.2 SPEC
      // The v2.0 cross ref lists 0x50 as Keystone, group Geometry
      // However, the Geometry table lists Keystone as x50, not x80
      // Neither x50 nor x80 are defined in v3.0
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // in 2.0 spec, not in 3.0, or 2.2
      // assume not in 2.1
      .desc="Increase/decrease the keystone distortion in the image.",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Keystone",
      .v21_flags = VCP2_DEPRECATED,
   },
   {  .code=0x82,
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,   // 2.0: Image, 3.0: Geometry
      .default_sl_values = x82_horizontal_flip_values,
      .desc="Flip picture horizontally",
      // DESIGN ISSUE!!!
      // This feature is WO in 2.0 spec, RW in 3.0, what is it in 2.2
      // implies cannot use global_flags to store RO/RW/WO
      .v20_flags =  VCP2_WO | VCP2_WO_NC,
      .v20_name = "HorFlip",
      .v21_name = "Horizontal Mirror (Flip)",
      .v21_flags =  VCP2_RW | VCP2_SIMPLE_NC,
      .v21_sl_values = x82_horizontal_flip_values,
   },
   {  .code=0x84,
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,   // 2.0: Image, 3.0: Geometry
      .default_sl_values = x84_vertical_flip_values,
      // DESIGN ISSUE!!!
      // This feature is WO in 2.0 spec, RW in 3.0, what is it in 2.2
      // implies cannot use global_flags to store RO/RW/WO
      .desc="Flip picture vertically",
      .v20_flags =  VCP2_WO | VCP2_WO_NC,
      .v20_name = "VertFlip",
      .v21_name = "Vertical Mirror (Flip)",
      .v21_flags =  VCP2_RW | VCP2_SIMPLE_NC,
      .v21_sl_values = x84_vertical_flip_values,
   },
   {  .code=0x86,                                              // Done
      // v2.0 spec cross ref lists as MISC, but defined in IMAGE table, assume IMAGE
      .vcp_spec_groups = VCP_SPEC_IMAGE,              // 2.0: IMAGE
      //.name="DisplayScaling",
      //.flags = VCP_RW | VCP_NON_CONT,
      .default_sl_values = x86_display_scaling_values,
      .desc = "Control the scaling (input vs output) of the display",
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "Display Scaling",
   },
   {  .code=0x87,                                                 // Done
      .vcp_spec_groups = VCP_SPEC_IMAGE ,    // 2.0 IMAGE, 3.0 Image, 2.2 Image
      // defined in 2.0, is C in 3.0 and 2.2, assume 2.1 is C as well
      .default_sl_values = x87_sharpness_values,
      // .desc = "Specifies one of a range of algorithms",
      .desc = "Selects one of a range of algorithms. "
              "Increasing (decreasing) the value must increase (decrease) "
              "the edge sharpness of image features.",
      .v20_flags=VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name="Sharpness",
      .v21_flags=VCP2_RW | VCP2_STD_CONT,
   },
   {  .code=0x88,
      // 2.0 cross ref lists this as IMAGE, but defined in MISC table
      // 2.2: Image
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_CRT,    // ???
      .desc = "Increase (decrease) the velocity modulation of the horizontal "
              "scan as a function of the change in luminescence level",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Velocity Scan Modulation",
   },
   {  .code=0x8a,
      .vcp_spec_groups = VCP_SPEC_IMAGE,     // 2.0, 2.2, 3.0
      // Name differs in 3.0 vs 2.0, assume 2.1 same as 2.0
      .vcp_subsets = VCP_SUBSET_TV | VCP_SUBSET_COLOR,
      .desc = "Increase/decrease the amplitude of the color difference "
              "components of the video signal",
      .v20_flags =VCP2_RW |  VCP2_STD_CONT,
      .v20_name  = "TV Color Saturation",
      .v21_name  = "Color Saturation",
   },
   {  .code=0x8b,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .vcp_subsets = VCP_SUBSET_TV,
      .desc = "Increment (1) or decrement (2) television channel",
      .v20_flags=VCP2_WO | VCP2_WO_NC,
      .v20_name="TV Channel Up/Down",
      .default_sl_values=x8b_tv_channel_values,
   },
   {  .code = 0x8c,
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_IMAGE,  // 2.0: SPEC, 2.2: IMAGE
      .vcp_subsets = VCP_SUBSET_TV,
      .desc = "Increase/decrease the amplitude of the high frequency components  "
              "of the video signal",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "TV Sharpness",
   },
   {  .code=0x8d,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      // v3.0 same as v2.0
      // v2.2 adds SH byte for screen blank
      .vcp_subsets = VCP_SUBSET_TV | VCP_SUBSET_AUDIO,
      .desc = "Mute/unmute audio, and (v2.2) screen blank",
      .nontable_formatter=format_feature_detail_x8d_v22_mute_audio_blank_screen,
      .default_sl_values = x8d_tv_audio_mute_source_values,
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "Audio Mute",
      .v22_flags = VCP2_RW | VCP2_COMPLEX_NC,
      .v22_name = "Audio mute/Screen blank",
   },
   {  .code=0x8e,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .vcp_subsets = VCP_SUBSET_TV,
      .desc = "Increase/decrease the ratio between blacks and whites in the image",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "TV Contrast",
   },
   {  .code=0x8f,
      .vcp_spec_groups = VCP_SPEC_AUDIO,    // v2.0, 2.2, 3.0
      .vcp_subsets = VCP_SUBSET_AUDIO,
      .desc="Emphasize/de-emphasize high frequency audio",
      // n. for v2.0 name in spec is "TV-audio treble".  "TV" prefix is
      // dropped in 2.2, 3.0.  just use the more generic term for all versions
      // similar comments apply to x91, x93
      .v20_name="Audio Treble",
      // requires special handling for V3, mix of C and NC, SL byte only
      .nontable_formatter=format_feature_detail_audio_treble_bass_v30,
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v30_flags = VCP2_RW | VCP2_COMPLEX_NC,
      .v22_flags = VCP2_RW | VCP2_COMPLEX_NC,
   },
   {  .code=0x90,
      .vcp_spec_groups = VCP_SPEC_MISC,     // 2.0
      .vcp_subsets = VCP_SUBSET_TV | VCP_SUBSET_COLOR,
      .desc = "Increase/decrease the wavelength of the color component of the video signal. "
              "AKA tint.  Applies to currently active interface",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "Hue",
   },
   {  .code=0x91,
      .vcp_spec_groups = VCP_SPEC_AUDIO,    // v2.0, 2.2, 3.0
      .vcp_subsets = VCP_SUBSET_AUDIO,
      .desc="Emphasize/de-emphasize low frequency audio",
      .v20_name="Audio Bass",
      // requires special handling for V3.0 and v2.2: mix of C and NC, SL byte only
      .nontable_formatter=format_feature_detail_audio_treble_bass_v30,
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v30_flags = VCP2_RW | VCP2_COMPLEX_NC,
      .v22_flags = VCP2_RW | VCP2_COMPLEX_NC,
   },
   {  .code=0x92,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .vcp_subsets = VCP_SUBSET_TV,
      .desc = "Increase/decrease the black level of the video",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "TV Black level/Brightness",
      .v21_name = "TV Black level/Luminesence",
   },
   {  .code=0x93,
      .vcp_spec_groups = VCP_SPEC_AUDIO,    // v2.0, 2.2, 3.0
      .vcp_subsets = VCP_SUBSET_AUDIO,
      .desc="Controls left/right audio balance",
      .v20_name="Audio Balance L/R",
      // requires special handling for V3 and v2.2, mix of C and NC, SL byte only
      .nontable_formatter=format_feature_detail_audio_treble_bass_v30,
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v30_flags = VCP2_RW | VCP2_COMPLEX_NC,
      .v22_flags = VCP2_RW | VCP2_COMPLEX_NC,
   },
   {  .code=0x94,
      .vcp_spec_groups = VCP_SPEC_AUDIO,    // v2.0
      // name changed in 3.0, assume applies to 2.1
      .vcp_subsets = VCP_SUBSET_TV | VCP_SUBSET_AUDIO,
      .desc="Select audio mode",
      .v20_name="Audio Stereo Mode",
      .v21_name="Audio Processor Mode",
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .default_sl_values=x94_audio_stereo_mode_values,
   },
   {  .code=0x95,                               // Done
      .vcp_spec_groups = VCP_SPEC_WINDOW | VCP_SPEC_GEOMETRY,  // 2.0: WINDOW, 3.0: GEOMETRY
      .vcp_subsets = VCP_SUBSET_WINDOW,
      .desc="Top left X pixel of an area of the image",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Window Position(TL_X)",
   },
   {  .code=0x96,                                             // Done
      .vcp_spec_groups = VCP_SPEC_WINDOW | VCP_SPEC_GEOMETRY,  // 2.0: WINDOW, 3.0: GEOMETRY
      .vcp_subsets = VCP_SUBSET_WINDOW,
      .desc="Top left Y pixel of an area of the image",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Window Position(TL_Y)",
   },
   {  .code=0x97,                                            // Done
      .vcp_spec_groups = VCP_SPEC_WINDOW | VCP_SPEC_GEOMETRY,  // 2.0: WINDOW, 3.0: GEOMETRY
      .vcp_subsets = VCP_SUBSET_WINDOW,
      .desc="Bottom right X pixel of an area of the image",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Window Position(BR_X)",
   },
   {  .code=0x98,                                                    // Done
      .vcp_spec_groups = VCP_SPEC_WINDOW | VCP_SPEC_GEOMETRY,  // 2.0: WINDOW, 3.0: GEOMETRY
      .vcp_subsets = VCP_SUBSET_WINDOW,
      .desc="Bottom right Y pixel of an area of the image",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Window Position(BR_Y)",
   },
   {  .code=0x99,
      .vcp_spec_groups = VCP_SPEC_WINDOW,     // 2.0: WINDOW
      // in 2.0,  not in 3.0 or 2.2, what is correct choice for 2.1?
      .vcp_subsets = VCP_SUBSET_WINDOW,
      .default_sl_values = x99_window_control_values,
      .desc="Enables the brightness and color within a window to be different "
            "from the desktop.",
      .v20_flags= VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name="Window control on/off",
      .v30_flags = VCP2_DEPRECATED,
      .v30_flags = VCP2_DEPRECATED,
   },
   {  .code=0x9a,
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,   // 2.0: WINDOW, 3.0, 2.2: IMAGE
      .vcp_subsets = VCP_SUBSET_WINDOW,               // VCP_SUBSET_COLOR?
      // 2.2 spec notes:
      // 1) should be used in conjunction with VCP 99h
      // 2) Not recommended for new designs, see A5h for alternate
      .desc="Changes the contrast ratio between the area of the window and the "
            "rest of the desktop",
      .v20_flags= VCP2_RW | VCP2_STD_CONT,
      .v20_name="Window background",
   },
   {  .code=0x9b,                                             // in 2.0, same in 3.0
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,    // 2.0: WINDOW, 3.0: IMAGE
      .vcp_subsets = VCP_SUBSET_COLOR,
      // U3011 doesn't implement the spec that puts the midpoint at 127,
      // Just interpret this and the other hue fields as standard continuous
      // .desc = "Value < 127 shifts toward magenta, 127 no effect, "
      //         "> 127 shifts toward yellow",
      // .nontable_formatter=format_feature_detail_6_axis_hue,
      // .v20_flags =VCP2_RW |  VCP2_COMPLEX_CONT,
      .desc = "Decrease shifts toward magenta, "
              "increase shifts toward yellow",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "6 axis hue control: Red",
   },
   {  .code=0x9c,                                             // in 2.0, same in 3.0
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,    // 2.0: WINDOW, 3.0: IMAGE
      .vcp_subsets = VCP_SUBSET_COLOR,
      // .nontable_formatter=format_feature_detail_6_axis_hue,
      // .desc = "Value < 127 shifts toward green, 127 no effect, "
      //         "> 127 shifts toward red",
      // .v20_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .desc = "Decrease shifts toward green, "
              "increase shifts toward red",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "6 axis hue control: Yellow",
   },
   {  .code=0x9d,                                             // in 2.0, same in 3.0
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,    // 2.0: WINDOW, 3.0: IMAGE
      .vcp_subsets = VCP_SUBSET_COLOR,
      // .nontable_formatter=format_feature_detail_6_axis_hue,
      // .desc = "Value < 127 shifts toward yellow, 127 no effect, "
      //        "> 127 shifts toward cyan",
      // .v20_flags =VCP2_RW |  VCP2_COMPLEX_CONT,
      .desc = "Decrease shifts toward yellow, "
              "increase shifts toward cyan",
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .v20_name = "6 axis hue control: Green",
   },
   {  .code=0x9e,                                             // in 2.0, same in 3.0
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,    // 2.0: WINDOW, 3.0: IMAGE
      .vcp_subsets = VCP_SUBSET_COLOR,
      // .nontable_formatter=format_feature_detail_6_axis_hue,
      // .desc = "Value < 127 shifts toward green, 127 no effect, "
      //         "> 127 shifts toward blue",
      // .v20_flags = VCP2_RW | VCP2_COMPLEX_CONT,               // VCP_SUBSET_COLORMGT?
      .desc = "Decrease shifts toward green, "
              "increase shifts toward blue",
      .v20_flags = VCP2_RW | VCP2_STD_CONT | VCP_SUBSET_COLOR,
      .v20_name = "6 axis hue control: Cyan",
   },
   {  .code=0x9f,
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // .nontable_formatter=format_feature_detail_6_axis_hue,
      // .desc = "Value < 127 shifts toward cyan, 127 no effect, "
      //         "> 127 shifts toward magenta",
      // .v20_flags =VCP2_RW |  VCP2_COMPLEX_CONT,
      .desc = "Decrease shifts toward cyan, "
              "increase shifts toward magenta",
      .v20_flags = VCP2_RW | VCP2_STD_CONT | VCP_SUBSET_COLOR,
      .v20_name = "6 axis hue control: Blue",
   },
   {  .code=0xa0,
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // .nontable_formatter=format_feature_detail_6_axis_hue,
      // .desc = "Value < 127 shifts toward blue, 127 no effect, "
      //         "> 127 shifts toward red",
      // .v20_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .v20_flags = VCP2_RW | VCP2_STD_CONT,
      .desc = "Decrease shifts toward blue, 127 no effect, "
              "increase shifts toward red",
      .v20_name = "6 axis hue control: Magenta",
   },
   {  .code=0xa2,                             // Defined in 2.0, same in 3.0
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .desc="Turn on/off an auto setup function",
      .default_sl_values=xa2_auto_setup_values,
      .v20_flags = VCP2_WO | VCP2_WO_NC,
      .v20_name = "Auto setup on/off",
   },
   {  .code=0xa4,                                 // Complex interpretation, to be implemented
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_WINDOW,
      // 2.0 spec says: "This command structure is recommended, in conjunction with VCP A5h
      // for all new designs"
      // type NC in 2.0, type T in 3.0, 2.2, what is correct choice for 2.1?
      //.name="Window control on/off",
      //.flags = VCP_RW | VCP_NON_CONT,
      .nontable_formatter = format_feature_detail_debug_sl_sh,   // TODO: write proper function
      .table_formatter = default_table_feature_detail_function,  // TODO: write proper function
      .desc = "Turn selected window operation on/off, window mask",
      .v20_flags = VCP2_RW | VCP2_COMPLEX_NC,
      .v30_flags = VCP2_RW | VCP2_TABLE,
      .v22_flags = VCP2_RW | VCP2_TABLE,
      .v20_name = "Turn the selected window operation on/off",
      .v30_name = "Window mask control",
      .v22_name = "Window mask control",
   },
   {  .code=0xa5,
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,
      .vcp_subsets = VCP_SUBSET_WINDOW,
      // 2.0 spec says: "This command structure is recommended, in conjunction with VCP A4h
      // for all new designs
      // designated as C, but only takes specific values
      // v3.0 appears to be identical
      .default_sl_values = xa5_window_select_values,
      .desc = "Change selected window (as defined by 95h..98h)",
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "Change the selected window",
   },
   {  .code=0xaa,                                          // Done
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,    // 3.0: IMAGE, 2.0: GEOMETRY
      .default_sl_values=xaa_screen_orientation_values,
      .desc="Indicates screen orientation",
      .v20_flags=VCP2_RO | VCP2_SIMPLE_NC,
      .v20_name="Screen Orientation",
   },
   {  .code=0xac,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .nontable_formatter=format_feature_detail_xac_horizontal_frequency,
      .desc = "Horizontal sync signal frequency as determined by the display",
      // 2.0: 0xff 0xff 0xff indicates the display cannot supply this info
      .v20_flags = VCP2_RO | VCP2_COMPLEX_CONT,
      .v20_name  = "Horizontal frequency",
   },
   {  .code=0xae,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .nontable_formatter=format_feature_detail_xae_vertical_frequency,
      .desc = "Vertical sync signal frequency as determined by the display, "
              "in .01 hz",
      // 2.0: 0xff 0xff indicates the display cannot supply this info
      .v20_flags =VCP2_RO |  VCP2_COMPLEX_CONT,
      .v20_name  = "Vertical frequency",
   },
   {  .code=0xb0,
      .vcp_spec_groups = VCP_SPEC_PRESET,
      // Defined in 2.0, v3.0 spec clarifies that value to be set is in SL byte
      //.name="(Re)Store user saved values for cur mode",   // this was my name from the explanation
      .default_sl_values = xb0_settings_values,
      .desc = "Store/restore the user saved values for the current mode.",
      .v20_flags = VCP2_WO | VCP2_WO_NC,
      .v20_name = "Settings",
   },
   {  .code=0xb2,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .default_sl_values=xb2_flat_panel_subpixel_layout_values,
      .desc = "LCD sub-pixel structure",
      .v20_flags = VCP2_RO | VCP2_SIMPLE_NC,
      .v20_name = "Flat panel sub-pixel layout",
   },
   {  .code=0xb6,                                               // DONE
      .vcp_spec_groups = VCP_SPEC_MISC,     // 2.0, 3.0
      // v3.0 table not upward compatible w 2.0, assume changed as of 2.1
      .desc = "Indicates the base technology type",
      .default_sl_values=xb6_v20_display_technology_type_values,
      .v21_sl_values = xb6_display_technology_type_values,
      .v20_flags = VCP2_RO | VCP2_SIMPLE_NC,
      .v20_name = "Display technology type",
   },
   {  .code=0xb7,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .desc = "Video mode and status of a DPVL capable monitor",
      .v20_name = "Monitor status",
      .v20_flags = VCP2_RO | VCP2_COMPLEX_NC,
      .nontable_formatter = format_feature_detail_sl_byte,    //TODO: implement proper function
   },
   {  .code=0xb8,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .v20_name = "Packet count",
      .desc = "Counter for DPVL packets received",
      .v20_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .nontable_formatter = format_feature_detail_ushort,
   },
   {  .code=0xb9,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .v20_name = "Monitor X origin",
      .desc = "X origin of the monitor in the vertical screen",
      .v20_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .nontable_formatter = format_feature_detail_ushort,
   },
   {  .code=0xba,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .v20_name = "Monitor Y origin",
      .desc = "Y origin of the monitor in the vertical screen",
      .v20_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .nontable_formatter = format_feature_detail_ushort,
   },
   {  .code=0xbb,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .desc = "Error counter for the DPVL header",
      .v20_name = "Header error count",
      .v20_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .nontable_formatter = format_feature_detail_ushort,
   },
   {  .code=0xbc,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .desc = "CRC error counter for the DPVL body",
      .v20_name = "Body CRC error count",
      .v20_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .nontable_formatter = format_feature_detail_ushort,
   },
   {  .code=0xbd,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .desc = "Assigned identification number for the monitor",
      .v20_name = "Client ID",
      .v20_flags = VCP2_RW | VCP2_COMPLEX_CONT,
      .nontable_formatter = format_feature_detail_ushort,
   },
   {  .code=0xbe,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .desc = "Indicates status of the DVI link",
      .v20_name = "Link control",
      .v20_flags = VCP2_RW | VCP2_COMPLEX_NC,
      .nontable_formatter = format_feature_detail_xbe_link_control,
   },
   {  .code=0xc0,
      .vcp_spec_groups = VCP_SPEC_MISC,
      .nontable_formatter=format_feature_detail_xc0_display_usage_time,
      .desc = "Active power on time in hours",
      .v20_flags =VCP2_RO |  VCP2_COMPLEX_CONT,
      .v20_name = "Display usage time",
   },
   {  .code=0xc2,
      .vcp_spec_groups = VCP_SPEC_MISC,    // 2.0
      .desc = "Length in bytes of non-volatile storage in the display available "
              "for writing a display descriptor, max 256",
      .v20_flags = VCP2_RO | VCP2_STD_CONT,
      // should there be a different flag when we know value uses only SL?
      // or could this value be exactly 256?
      .v20_name = "Display descriptor length",
   },
   {  .code=0xc3,
      .vcp_spec_groups = VCP_SPEC_MISC, // 2.0
      .table_formatter = default_table_feature_detail_function,
      .desc="Reads (writes) a display descriptor from (to) non-volatile storage "
            "in the display.",
      .v20_flags = VCP2_RW | VCP2_TABLE,
      .v20_name = "Transmit display descriptor",
   },
   {  .code = 0xc4,
      .vcp_spec_groups = VCP_SPEC_MISC,  // 2.0
      .nontable_formatter=format_feature_detail_debug_bytes,
      .desc = "If enabled, the display descriptor shall be displayed when no video "
              "is being received.",
      .v20_flags = VCP2_RW | VCP2_COMPLEX_NC,
      // need to handle "All other values.  The display descriptor shall not be displayed"
      .v20_name = "Enable display of \'display descriptor\'",
   },
   {  .code=0xc6,
      .vcp_spec_groups = VCP_SPEC_MISC, // 2.0
      .nontable_formatter=format_feature_detail_application_enable_key,
      .desc = "A 2 byte value used to allow an application to only operate with known products.",
      .v20_flags = VCP2_RO | VCP2_COMPLEX_NC,
      .v20_name = "Application enable key",
   },
   {  .code=0xc8,
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_CONTROL,    // 2.0: MISC, 3.0: CONTROL
      .nontable_formatter=format_feature_detail_display_controller_type,
      .default_sl_values=xc8_display_controller_type_values,
      .desc = "Mfg id of controller and 2 byte manufacturer-specific controller type",
      .v20_flags = VCP2_RW | VCP2_COMPLEX_NC,
      .v20_name = "Display controller type",
   },
   {  .code=0xc9,
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_CONTROL,    // 2.: MISC, 3.0: CONTROL
      .nontable_formatter=format_feature_detail_version,
      .desc = "2 byte firmware level",
      .v20_flags = VCP2_RO | VCP2_COMPLEX_NC,
      .v20_name = "Display firmware level",
   },
   {  .code=0xca,
      // Says the v2.2 spec: A new feature added to V3.0 and expanded in V2.2
      // BUT: xCA is present in 2.0 spec, defined identically to 3.0 spec
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_CONTROL,   // 2.0: MISC, 3.0: CONTROL
      .default_sl_values=xca_osd_values,
      // .desc = "Indicates whether On Screen Display is enabled",
      .desc = "Sets and indicates the current operational state of OSD (and buttons in v2.2)",
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "OSD",
      .v22_flags = VCP2_RW | VCP2_COMPLEX_NC,
      // for v3.0:
      .nontable_formatter = format_feature_detail_debug_sl_sh,   // TODO: write proper function for v3.0
      .v22_name = "OSD/Button Control"
   },
   {  .code=0xcc,
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_CONTROL,   // 2.0: MISC, 3.0: CONTROL
      .default_sl_values=xcc_osd_language_values,
      .desc = "On Screen Display languge",
      .v20_flags  = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "OSD Language",
   },
   {  .code=0xcd,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 3.0: MISC
      // not in 2.0, is in 3.0, assume exists in 2.1
      .desc = "Control up to 16 LED (or similar) indicators to indicate system status",
      .nontable_formatter = format_feature_detail_debug_sl_sh,
      .v21_flags = VCP2_RW | VCP2_COMPLEX_NC,
      .v21_name = "Status Indicators",
   },
   {  .code=0xce,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .desc = "Rows and characters/row of auxiliary display",
      .v20_flags  = VCP2_RO | VCP2_COMPLEX_NC,
      .v20_name = "Auxiliary display size",
      .nontable_formatter =  format_feature_detail_xce_aux_display_size,
   },
   {  .code=0xcf,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .desc = "Sets contents of auxiliary display device",
      .v20_flags  = VCP2_WO | VCP2_WO_TABLE,
      .v20_name = "Auxiliary display data",
   },
   {  .code=0xd0,
      .vcp_spec_groups = VCP_SPEC_MISC,
      .desc = "Selects the active output",
      .v20_name = "Output select",
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .default_sl_values = xd0_v2_output_select_values,
      .table_formatter = default_table_feature_detail_function,  // TODO: implement proper function
      .v30_flags = VCP2_RW | VCP2_TABLE,
      .v22_flags = VCP2_RW | VCP2_SIMPLE_NC,
   },
   {  .code=0xd2,
      // exists in 3.0, not in 2.0, assume exists in 2.1
      .vcp_spec_groups = VCP_SPEC_MISC,
      .desc = "Read an Asset Tag to/from the display",
      .v21_name = "Asset Tag",
      .v21_flags = VCP2_RW | VCP2_TABLE,
      .table_formatter = default_table_feature_detail_function,
   },
   {  .code=0xd4,
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_IMAGE,        // 2.0: MISC, 3.0: IMAGE
      .desc="Stereo video mode",
      .v20_name = "Stereo video mode",
      .v20_flags = VCP2_RW | VCP2_COMPLEX_NC,
      .nontable_formatter = format_feature_detail_sl_byte,     // TODO: implement proper function
   },
   {  .code=0xd6,                           // DONE
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_CONTROL,   // 2.0: MISC, 3.0: CONTROL
      .default_sl_values = xd6_power_mode_values,
      .desc = "DPM and DPMS status",
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "Power mode",
   },
   {  .code=0xd7,                          // DONE - identical in 2.0, 3.0, 2.2
      .vcp_spec_groups = VCP_SPEC_MISC,    // 2.0, 3.0, 2.2
      .default_sl_values = xd7_aux_power_output_values,
      .desc="Controls an auxiliary power output from a display to a host device",
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "Auxiliary power output",
   },
   {  .code=0xda,                                                   // DONE
      .vcp_spec_groups = VCP_SPEC_GEOMETRY | VCP_SPEC_IMAGE,         // 2.0: IMAGE, 3.0: GEOMETRY
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Controls scan characteristics (aka format)",
      .default_sl_values = xda_scan_mode_values,
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name  = "Scan format",
      // name differs in 3.0, assume changed as of 2.1
      .v21_name  = "Scan mode",
   },
   {  .code=0xdb,
      .vcp_spec_groups = VCP_SPEC_CONTROL,         // 3.0
      // defined in 3.0, not in 2.0, assume present in 2.1
      .vcp_subsets = VCP_SUBSET_TV,
      .desc = "Controls aspects of the displayed image (TV applications)",
      .default_sl_values = xdb_image_mode_values,
      .v21_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v21_name  = "Image Mode",
   },
   {  .code=0xdc,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // defined in 2.0, 3.0 has different name, more values
      //.name="Display application",
      .default_sl_values=xdc_display_application_values,
      .desc="Type of application used on display",  // my desc
      .v20_flags = VCP2_RW | VCP2_SIMPLE_NC,
      .v20_name = "Display Mode",
      .v30_name = "Display Application",
   },
   {  .code=0xde,
      // code 0xde has a completely different name and definition in v2.0
      // vs v3.0/2.2
      // 2.0: Operation Mode, W/O single byte value per xde_wo_operation_mode_values
      // 3.0, 2.2: Scratch Pad: 2 bytes of volatile storage for use of software applications
      // Did the definition really change so radically, or is the 2.0 spec a typo.
      // What to do for 2.1?  Assume same as 3.0,2.2
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0, 3.0, 2.2
      .desc = "Operation mode (2.0) or scratch pad (3.0/2.2)",
      .nontable_formatter = format_feature_detail_debug_sl_sh,
      .v20_flags = VCP2_WO | VCP2_WO_NC,
      .v20_name  = "Operation Mode",
      .v21_flags = VCP2_RW | VCP2_COMPLEX_NC,
      .v21_name  = "Scratch Pad",
   },
   {  .code=0xdf,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .nontable_formatter=format_feature_detail_version,
      .desc = "MCCS version",
      .v20_flags = VCP2_RO | VCP2_COMPLEX_NC,
      .v20_name  = "VCP Version",
   }
};
// #pragma GCC diagnostic pop
int vcp_feature_code_count = sizeof(vcp_code_table)/sizeof(VCP_Feature_Table_Entry);



int check_one_version_flags(
      Version_Feature_Flags     vflags,
      char *                    which_flags,
      VCP_Feature_Table_Entry * pentry)
{
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
             pentry->code, which_flags);
          ct = -1;
       }


      if (vflags & VCP2_SIMPLE_NC) {
         if (!pentry->default_sl_values) {
            fprintf(
               stderr,
               "code: 0x%02x, .flags: %s, VCP2_SIMPLE_NC set but .default_sl_values == NULL\n",
               pentry->code, which_flags);
            ct = -2;
         }
      }
      else

      if (vflags & VCP2_COMPLEX_NC) {
         if (!pentry->nontable_formatter) {
            fprintf(
               stderr,
               "code: 0x%02x, .flags: %s, VCP2_COMPLEX_NC set but .nontable_formatter == NULL\n",
               pentry->code, which_flags);
            ct = -2;
         }
      }
      else if (vflags & VCP2_COMPLEX_CONT) {
         if (!pentry->nontable_formatter) {
            fprintf(
               stderr,
               "code: 0x%02x, .flags: %s, VCP2_COMPLEX_CONT set but .nontable_formatter == NULL\n",
               pentry->code, which_flags);
            ct = -2;
         }
      }
//      // no longer an error.   get_table_feature_detail_function() sets default
//      else if (vflags & VCP2_TABLE) {
//         if (!pentry->table_formatter) {
//            fprintf(
//               stderr,
//               "code: 0x%02x, .flags: %s, VCP2_TABLE set but .table_formatter == NULL\n",
//               pentry->code, which_flags);
//            ct = -2;
//         }
//      }
   }

   return ct;
}


/*  If the flags has anything set, and is not deprecated, checks that
 * exactly 1 of VCP2_RO, VCP2_WO, VCP2_RW is set
 *
 * Returns: 1 of a value is set, 0 if no value set, -1 if
 *          more than 1 value set
 */
int check_version_rw_flags(
      Version_Feature_Flags vflags,
      char * which_flags,
      VCP_Feature_Table_Entry * entry)
{
   int ct = 0;
   if (vflags && !(vflags & VCP2_DEPRECATED))  {
        if (vflags & VCP2_RO) ct++;
        if (vflags & VCP2_WO) ct++;
        if (vflags & VCP2_RW) ct++;
        if (ct != 1) {
           fprintf(stderr,
                   "code: 0x%02x, exactly 1 of VCP2_RO, VCP2_WO, VCP2_RW must be set in non-zero %s_flags\n",
                   entry->code, which_flags);
           ct = -1;
        }
   }
   return ct;
}


void validate_vcp_feature_table() {
   // DBGMSG("Starting");
   bool ok = true;
   bool ok2 = true;
   int ndx = 0;
   // return;       // *** TEMP ***

   int total_ct = 0;
   for (;ndx < vcp_feature_code_count;ndx++) {
      VCP_Feature_Table_Entry * pentry = &vcp_code_table[ndx];
      int cur_ct;
      cur_ct = check_version_rw_flags(pentry->v20_flags, "v20_flags", pentry);
      if (cur_ct < 0) ok = false; else total_ct += cur_ct;
      cur_ct = check_version_rw_flags(pentry->v21_flags, "v21_flags", pentry);
      if (cur_ct < 0) ok = false; else total_ct += cur_ct;
      cur_ct = check_version_rw_flags(pentry->v30_flags, "v30_flags", pentry);
      if (cur_ct < 0) ok = false; else total_ct += cur_ct;
      cur_ct = check_version_rw_flags(pentry->v22_flags, "v22_flags", pentry);
      if (cur_ct < 0) ok = false; else total_ct += cur_ct;

      if (ok && total_ct == 0) {
         fprintf(stderr,
                 "RW, RO, RW not set in any version specific flags for feature 0x%02x\n",
                 pentry->code);
         ok = false;
      }

      total_ct = 0;
      cur_ct = 0;
      cur_ct = check_one_version_flags(pentry->v20_flags, ".v20_flags", pentry);
      if (cur_ct < 0) ok2 = false; else total_ct += 1;
      cur_ct = check_one_version_flags(pentry->v21_flags, ".v21_flags", pentry);
      if (cur_ct < 0) ok2 = false; else total_ct += 1;
      cur_ct = check_one_version_flags(pentry->v30_flags, ".v30_flags", pentry);
      if (cur_ct < 0) ok2 = false; else total_ct += 1;
      cur_ct = check_one_version_flags(pentry->v22_flags, ".v22_flags", pentry);
      if (cur_ct < 0) ok2 = false; else total_ct += 1;

      if (total_ct == 0 && ok2) {
         fprintf(stderr, "code: 0x%02x, Type not specified in any vnn_flags\n", pentry->code);
         ok2 = false;
      }

   }
   if (!(ok && ok2))
      PROGRAM_LOGIC_ERROR(NULL);
}

void init_vcp_feature_codes() {
   validate_vcp_feature_table();
}



