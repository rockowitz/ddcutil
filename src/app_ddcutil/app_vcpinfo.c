// app_vcpinfo.c

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "public/ddcutil_types.h"

#include "util/report_util.h"

#include "vcp/vcp_feature_set.h"
#include "vcp/vcp_feature_codes.h"

#include "app_ddcutil/app_vcpinfo.h"




/* Creates humanly readable interpretation of VCP feature flags.
 * The result is returned in a buffer supplied by the caller.
 *
 * Arguments:
 *    flags
 *    buf
 *    buflen
 *
 * Returns:   buf
 */
static char *
vcp_interpret_version_feature_flags(
      DDCA_Version_Feature_Flags  flags,
      char*                       buf,
      int                         buflen)
{
   // DBGMSG("flags: 0x%04x", flags);
   char * rwmsg = "";
   if (flags & DDCA_RO)
      rwmsg = "ReadOnly ";
   else if (flags & DDCA_WO)
      rwmsg = "WriteOnly";
   else if (flags & DDCA_RW)
      rwmsg = "ReadWrite";

   char * typemsg = "";
   // NEED TO ALSO HANDLE TABLE TYPE
   if (flags & DDCA_CONT)
      typemsg = "Continuous";
   else if (flags & DDCA_NC)
      typemsg = "Non-continuous";
   else if (flags & DDCA_NORMAL_TABLE)
      typemsg = "Table";
   // else if (flags & VCP_TYPE_V2NC_V3T)
   //    typemsg = "V2:NC, V3:Table";
   else if (flags & DDCA_DEPRECATED)
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





/* Implements command LISTVCP
 *
 * Arguments:
 *   fh      where to write output
 */
void app_listvcp(FILE * fh) {
   fprintf(fh, "Recognized VCP feature codes:\n");
   char buf[200];
   char buf2[234];
   //  TODO make listvcp respect display to get version?
   int ct = vcp_get_feature_code_count();
   for (int ndx = 0; ndx < ct ; ndx++) {
      VCP_Feature_Table_Entry * entry = vcp_get_feature_table_entry(ndx);
      // DBGMSG("code=0x%02x, flags: 0x%04x", entry.code, entry.flags);
      DDCA_MCCS_Version_Spec vspec = get_highest_non_deprecated_version(entry);
      DDCA_Version_Feature_Flags vflags = get_version_specific_feature_flags(entry, vspec);
      vcp_interpret_version_feature_flags(vflags, buf, sizeof(buf));
      char * vermsg = "";
      if (has_version_specific_features(entry))
         vermsg = " (Version specific interpretation)";
      snprintf(buf2, sizeof(buf2), "%s%s", buf, vermsg);

      fprintf(fh, "  %02x - %-40s  %s\n",
                  entry->code,
                  get_non_version_specific_feature_name(entry),
                  // vcp_interpret_feature_flags(entry.flags, buf, 200)   // *** TODO: HOW TO HANDLE THIS w/o version?
                  buf2
             );
   }
}


/* Returns a byte of flags indicating those MCCS versions for which the
 * specified VCP feature is defined.
 *
 * Arguments:
 *   pentry      pointer to VCP_Feature_Table_Entry for VCP feature
 *
 * Returns:      byte of flags
 */
static Byte valid_versions(VCP_Feature_Table_Entry * pentry) {
   Byte result = 0x00;

   if (pentry->v20_flags)
      result |= DDCA_MCCS_V20;
   if (pentry->v21_flags) {
      if ( !(pentry->v21_flags & DDCA_DEPRECATED) )
         result |= DDCA_MCCS_V21;
   }
   else {
      if (result & DDCA_MCCS_V20)
         result |= DDCA_MCCS_V21;
   }
   if (pentry->v30_flags) {
      if ( !(pentry->v30_flags & DDCA_DEPRECATED) )
         result |= DDCA_MCCS_V30;
   }
   else {
      if (result & DDCA_MCCS_V21)
         result |= DDCA_MCCS_V30;
   }
   if (pentry->v22_flags) {
      if ( !(pentry->v22_flags & DDCA_DEPRECATED) )
         result |= DDCA_MCCS_V22;
   }
   else {
      if (result & DDCA_MCCS_V21)
         result |= DDCA_MCCS_V22;
   }
   return result;
}


/* Given a byte of flags indicating MCCS versions, return a string containing a
 * comma delimited list of MCCS version names.
 *
 * Arguments:
 *   valid_version_flags
 *   version_name_buf      buffer in which to return string
 *   bufsz                 buffer size
 *
 * Returns:                version_name_buf
 *
 * Note: MCCS 1.0 is not reported
 */
static char * valid_version_names_r(
      Byte valid_version_flags,
      char *             version_name_buf,
      int                bufsz)
{
   assert(bufsz >= (4*5));        // max 4 version names, 5 chars/name
   *version_name_buf = '\0';

   if (valid_version_flags & DDCA_MCCS_V20)
      strcpy(version_name_buf, "2.0");
   if (valid_version_flags & DDCA_MCCS_V21) {
      if (strlen(version_name_buf) > 0)
         strcat(version_name_buf, ", ");
      strcat(version_name_buf, "2.1");
   }
   if (valid_version_flags & DDCA_MCCS_V30) {
      if (strlen(version_name_buf) > 0)
         strcat(version_name_buf, ", ");
      strcat(version_name_buf, "3.0");
   }
   if (valid_version_flags & DDCA_MCCS_V22) {
      if (strlen(version_name_buf) > 0)
         strcat(version_name_buf, ", ");
      strcat(version_name_buf, "2.2");
   }

   return version_name_buf;
}


#ifdef OLD
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
#endif

#ifdef OLD
static char * subset_names_r(VCP_Feature_Table_Entry * pentry, char * buf, int bufsz) {
   *buf = '\0';

   int kk = 0;
   for(;kk < vcp_subset_count; kk++) {
      Vcp_Subset_Desc cur_desc = vcp_subset_desc[kk];
      if (pentry->vcp_subsets & cur_desc.subset_id)
         str_comma_cat_r(cur_desc.public_name, buf, bufsz);
   }

   return buf;
}
#endif

#ifdef OLD
static char * subset_names_r(VCP_Feature_Table_Entry * pentry, char * buf, int bufsz) {
   *buf = '\0';

   int kk = 0;
   for(;kk < vcp_subset_count; kk++) {
      Value_Name_Title cur_desc = vcp_subset_table[kk];
      if (pentry->vcp_subsets & cur_desc.value)
         str_comma_cat_r(cur_desc.title, buf, bufsz);
   }

   return buf;
}
#endif


static void report_sl_values(DDCA_Feature_Value_Entry * sl_values, int depth) {
   while (sl_values->value_name != NULL) {
      rpt_vstring(depth, "0x%02x: %s", sl_values->value_code, sl_values->value_name);
      sl_values++;
   }
}

#ifdef UNUSED
static void report_sl_values_table(DDCA_Feature_Value_Entry * sl_values, int depth) {
   if (sl_values) {
      rpt_vstring(depth, "Values:");
      while (sl_values->value_name != NULL) {
         rpt_vstring(depth+1, "0x%02x: %s", sl_values->value_code, sl_values->value_name);
         sl_values++;
      }
   }
   else {
      rpt_vstring(depth, "Values:  none");
   }
}
#endif


static char * interpret_ddca_version_feature_flags_readwrite(
      DDCA_Version_Feature_Flags feature_flags)
{
   char * result = NULL;
   if (feature_flags & DDCA_RW)
      result = "Read Write";
   else if (feature_flags & DDCA_RO)
      result = "Read Only";
   else if (feature_flags & DDCA_WO)
      result = "Write Only";
   else {
      PROGRAM_LOGIC_ERROR("No read/write bits set");
      result = "PROGRAM LOGIC ERROR: No read/write bits set";
   }
   return result;
}


static char * interpret_ddca_version_feature_flags_type(
      DDCA_Version_Feature_Flags feature_flags)
{
   char * result = NULL;
   if (feature_flags & DDCA_STD_CONT)
      result = "Continuous (normal)";
   else if (feature_flags & DDCA_COMPLEX_CONT)
      result = "Continuous (complex)";
   else if (feature_flags & DDCA_SIMPLE_NC)
      result = "Non-Continuous (simple)";
   else if (feature_flags & DDCA_COMPLEX_NC)
      result = "Non-Continuous (complex)";
   else if (feature_flags & DDCA_NC_CONT)
      result = "Non-Continuous with continuous subrange";
   else if (feature_flags & DDCA_WO_NC)
      result = "Non-Continuous (write-only)";
   else if (feature_flags & DDCA_NORMAL_TABLE)
      result = "Table (normal)";
   else if (feature_flags & DDCA_WO_TABLE)
      result = "Table (write-only)";
   else {
      result = "PROGRAM LOGIC ERROR: No C/NC/T subtype bit set";
      PROGRAM_LOGIC_ERROR("No C/NC/T subtype bit set");
   }
   return result;
}

#ifdef OLD  // use vcp_interpret_global_feature_flags
static char * interpret_ddca_global_feature_flags(
      DDCA_Version_Feature_Flags feature_flags)
{
   char * result = "";
   if (feature_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY)
      result = "Synthetic";
   return result;
}
#endif


static char * interpret_feature_flags_r(
      DDCA_Version_Feature_Flags vflags,
      char *                     workbuf,
      int                        bufsz)
{
   bool debug = false;
   DBGMSF(debug, "vflags=0x%04x", vflags);
   assert(bufsz >= 100);     //  bigger than we'll need
   *workbuf = '\0';
   if (vflags & DDCA_DEPRECATED) {
      strcpy(workbuf, "Deprecated, ");
   }
   else {
     strcpy(workbuf, interpret_ddca_version_feature_flags_readwrite(vflags));
     strcat(workbuf, ", ");
     strcat(workbuf, interpret_ddca_version_feature_flags_type(vflags));

     char buf[80];
     char * s = vcp_interpret_global_feature_flags(vflags, buf, 80);
     if (s && strlen(s) > 0) {
        strcat(workbuf, ", ");
        strcat(workbuf, s);
     }

   }
   return workbuf;
}


// report function specifically for use by report_vcp_feature_table_entry()
static void report_feature_table_entry_flags(
        VCP_Feature_Table_Entry * pentry,
        DDCA_MCCS_Version_Spec    vcp_version,
        int                       depth)
{
   char workbuf[200];
   DDCA_Version_Feature_Flags vflags = get_version_specific_feature_flags(pentry, vcp_version);
   if (vflags) {
      interpret_feature_flags_r(vflags, workbuf, sizeof(workbuf));
      rpt_vstring(depth, "Attributes (v%d.%d): %s", vcp_version.major, vcp_version.minor, workbuf);
   }
}


/* Emits a report on a VCP_Feature_Table_Entry.  This function is used by the
 * VCPINFO command.  The report is written to the current report destination.
 *
 * Arguments:
 *    pentry   pointer to feature table entry
 *    depth    logical indentation depth
 */
void report_vcp_feature_table_entry(VCP_Feature_Table_Entry * pentry, int depth) {
   char workbuf[200];

   int d1 = depth+1;
   DDCA_Output_Level output_level = get_output_level();
   DDCA_MCCS_Version_Spec vspec = get_highest_non_deprecated_version(pentry);
   DDCA_Version_Feature_Flags vflags = get_version_specific_feature_flags(pentry, vspec);
   char * feature_name = get_non_version_specific_feature_name(pentry);
   rpt_vstring(depth, "VCP code %02X: %s", pentry->code, feature_name);
   rpt_vstring(d1, "%s", pentry->desc);
   valid_version_names_r(valid_versions(pentry), workbuf, sizeof(workbuf));
   rpt_vstring(d1, "MCCS versions: %s", workbuf);
   if (output_level >= DDCA_OL_VERBOSE)
      rpt_vstring(d1, "MCCS specification groups: %s",
                      spec_group_names_r(pentry, workbuf, sizeof(workbuf)));
   char * subset_names = feature_subset_names(pentry->vcp_subsets);
   rpt_vstring(d1, "ddcutil feature subsets: %s", subset_names);
   free(subset_names);
#ifdef OLD
   rpt_vstring(d1, "ddcutil feature subsets: %s",
                   subset_names_r(pentry, workbuf, sizeof(workbuf)));
#endif
   if (has_version_specific_features(pentry)) {
      // rpt_vstring(d1, "VERSION SPECIFIC FLAGS");
      report_feature_table_entry_flags(pentry, DDCA_VSPEC_V20, d1);
      report_feature_table_entry_flags(pentry, DDCA_VSPEC_V21, d1);
      report_feature_table_entry_flags(pentry, DDCA_VSPEC_V30, d1);
      report_feature_table_entry_flags(pentry, DDCA_VSPEC_V22, d1);
   }
   else {
      interpret_feature_flags_r(vflags, workbuf, sizeof(workbuf));
      rpt_vstring(d1, "Attributes: %s", workbuf);
   }

   if (pentry->default_sl_values && output_level >= DDCA_OL_VERBOSE) {
      rpt_vstring(d1, "Simple NC values:");
      report_sl_values(pentry->default_sl_values, d1+1);
   }
}


bool
app_vcpinfo(
      Feature_Set_Ref * fref,
      DDCA_MCCS_Version_Spec mccs_vspec,
      Feature_Set_Flags fsflags)
{
   bool vcpinfo_ok = true;

   VCP_Feature_Set * fset = create_feature_set_from_feature_set_ref(
                               fref,
                               mccs_vspec,
                               fsflags);
     if (!fset) {
        vcpinfo_ok = false;
     }
     else {
        if ( get_output_level() <= DDCA_OL_TERSE)
           report_feature_set(fset, 0);
        else {
           int ct =  get_feature_set_size(fset);
           int ndx = 0;
           for (;ndx < ct; ndx++) {
              VCP_Feature_Table_Entry * pentry = get_feature_set_entry(fset, ndx);
              report_vcp_feature_table_entry(pentry, 0);
           }
        }
        free_vcp_feature_set(fset);
     }
   return vcpinfo_ok;
}


