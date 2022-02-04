/** @file vcp_feature_codes.c
 *
 *  VCP Feature Code Table and related functions
 */

// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
/** \cond */

#include "base/ddc_errno.h"
#include "base/feature_metadata.h"
#include "base/rtti.h"
#include "base/vcp_version.h"

#include "vcp/vcp_feature_codes.h"


// Direct writes to stdout,stderr:
//   in table validation functions (Benign)


static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_VCP;

// Forward references
int vcp_feature_code_count;
VCP_Feature_Table_Entry vcp_code_table[];
static DDCA_Feature_Value_Entry x14_color_preset_absolute_values[];
static DDCA_Feature_Value_Entry x14_color_preset_tolerances[];
       DDCA_Feature_Value_Entry xc8_display_controller_type_values[];
static DDCA_Feature_Value_Entry x8d_tv_audio_mute_source_values[];
static DDCA_Feature_Value_Entry x8d_sh_blank_screen_values[];
static DDCA_Feature_Value_Entry xca_osd_values[];
static DDCA_Feature_Value_Entry xca_v22_osd_button_sl_values[];
static DDCA_Feature_Value_Entry xca_v22_osd_button_sh_values[];

static bool vcp_feature_codes_initialized = false;

//
// Functions implementing the VCPINFO command
//




/* Appends a string to an existing string in a buffer.
 * If the length of the existing string is greater than 0,
 * append ", " first.
 *
 * Arguments:
 *   val       value to append
 *   buf       start of buffer
 *   bufsz     buffer size
 *
 * Returns:    buf
 *
 * Note: No check is made that buf contains a valid string.
 */
static char * str_comma_cat_r(char * val, char * buf, int bufsz) {
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



char *
vcp_interpret_global_feature_flags(
      DDCA_Global_Feature_Flags   flags,
      char*                       buf,
      int                         buflen)
{
   // DBGMSG("flags: 0x%04x", flags);
   char * synmsg = "";
   if (flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY)
      synmsg = "Synthetic VCP Feature Table Entry";

   char * synmsg2 = "";
   // if (flags & DDCA_SYNTHETIC_DDCA_FEATURE_METADATA)
   //    synmsg = "Fully-Synthetic ";

   if (flags & DDCA_SYNTHETIC)    // should not occur for a VCP feature table entry
      synmsg = "Synthetic ";

   char * synmsg3 = "";            // should not occur for a VCP feature table entry
     if (flags & DDCA_PERSISTENT_METADATA)
        synmsg = "Persistent ";


   char * dynmsg = "";
   if (flags & DDCA_USER_DEFINED)     // should not occur for a VCP feature table entry
      dynmsg = "Dynamic ";

   g_snprintf(buf, buflen, "%s%s%s%s", synmsg, synmsg2, synmsg3, dynmsg);
   return buf;
}



// End of VCPINFO related functions




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


char * get_feature_name_by_id_and_vcp_version(Byte feature_id, DDCA_MCCS_Version_Spec vspec) {
   bool debug = false;

   char * result = NULL;
   VCP_Feature_Table_Entry * vcp_entry = vcp_find_feature_by_hexid(feature_id);
   if (vcp_entry) {
      result = get_version_sensitive_feature_name(vcp_entry, vspec);
      if (!result)
         result = get_non_version_specific_feature_name(vcp_entry);    // fallback
   }
   else if (0xe0 <= feature_id && feature_id <= 0xff)
      result = "Manufacturer specific feature";
   else
      result = "Unrecognized feature";

   DBGMSF(debug, "feature_id=0x%02x, vspec=%d.%d, returning: %s",
                 feature_id, vspec.major, vspec.minor, result);
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
DDCA_Version_Feature_Flags
get_version_specific_feature_flags(
       VCP_Feature_Table_Entry *  pvft_entry,
       DDCA_MCCS_Version_Spec     vcp_version)
{
   bool debug = false;
   DDCA_Version_Feature_Flags result = 0;
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

#ifdef NO
   // this is what get_version_sensisitve_feature_flags() is for
   if (!result) {
      DDCA_MCCS_Version_Spec version_used = {0,0};
      result = pvft_entry->v21_flags;
      if (result) {
         version_used.major = 2;
         version_used.minor = 1;
      }
      else {
         result = pvft_entry->v22_flags;
         if (result) {
            version_used.major = 2;
            version_used.minor = 2;
         }
         else {
            result = pvft_entry->v30_flags;
            assert(result);
            version_used.major = 3;
            version_used.minor = 0;
         }
      }
      char buf[20];
      if (vcp_version.major < 1)
         strcpy(buf, "Undefined");
      else
         snprintf(buf, 20, "%d.%d", vcp_version.major, vcp_version.minor);

      DBGTRC(true, TRACE_GROUP,
            "Monitor version %s is less than earliest MCCS version for which VCP feature code %02x is defined.",
            buf, pvft_entry->code);
      DBGTRC(true, TRACE_GROUP,
            "Using definition for MCCS version %d.%d", version_used.major, version_used.minor);
   }
#endif

   DBGMSF(debug, "Feature = 0x%02x, vcp version=%d.%d, returning 0x%02x",
          pvft_entry->code, vcp_version.major, vcp_version.minor, result);
   return result;
}


bool is_feature_supported_in_version(
      VCP_Feature_Table_Entry *  pvft_entry,
      DDCA_MCCS_Version_Spec     vcp_version)
{
   bool debug = false;
   bool result = false;
   DDCA_Version_Feature_Flags vflags = get_version_specific_feature_flags(pvft_entry, vcp_version);
   result = (vflags && !(vflags&DDCA_DEPRECATED));
   DBGMSF(debug, "Feature = 0x%02x, vcp versinon=%d.%d, returning %s",
                 pvft_entry->code, vcp_version.major, vcp_version.minor, sbool(result) );
   return result;
}


/* Gets appropriate VCP flags value for a feature, given
 * the VCP version for the monitor. If the VCP version specified is less than
 * the first version for which the feature is defined, returns the flags for the
 * first version for which the feature is defined.  This situation can arise when scanning
 * all possible VCP codes.
 *
 * Arguments:
 *   pvft_entry  vcp_feature_table entry
 *   vcp_version VCP version for monitor
 *
 * Returns:
 *   flags
 */
DDCA_Version_Feature_Flags
get_version_sensitive_feature_flags(
       VCP_Feature_Table_Entry * pvft_entry,
       DDCA_MCCS_Version_Spec    vcp_version)
{
   bool debug = false;
   DDCA_Version_Feature_Flags result = get_version_specific_feature_flags(pvft_entry, vcp_version);

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
         assert(false);
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

/* Returns the highest version number for which a feature is not deprecated
 */
DDCA_MCCS_Version_Spec
get_highest_non_deprecated_version(
      VCP_Feature_Table_Entry *  vfte)
{
   DDCA_MCCS_Version_Spec vspec = {0,0};
   if ( vfte->v22_flags && !(vfte->v22_flags & DDCA_DEPRECATED) ) {
      vspec.major = 2;
      vspec.minor = 2;
   }
   else if ( vfte->v30_flags && !(vfte->v30_flags & DDCA_DEPRECATED) ) {
      vspec.major = 3;
      vspec.minor = 0;
   }
   else if ( vfte->v21_flags && !(vfte->v21_flags & DDCA_DEPRECATED) ) {
      vspec.major = 2;
      vspec.minor = 1;
   }
   else if ( vfte->v20_flags && !(vfte->v20_flags & DDCA_DEPRECATED) ) {
      vspec.major = 2;
      vspec.minor = 0;
   }
   else {
      PROGRAM_LOGIC_ERROR("Feature 0x%02x is deprecated for all versions", vfte->code);
      assert(false);
   }

   return vspec;
}


// convenience function
bool is_feature_readable_by_vcp_version(
       VCP_Feature_Table_Entry *  vfte,
       DDCA_MCCS_Version_Spec     vcp_version)
{
   bool debug = false;
   bool result = (get_version_sensitive_feature_flags(vfte, vcp_version) & DDCA_READABLE );
   DBGMSF(debug, "code=0x%02x, vcp_version=%d.%d, returning %d",
                 vfte->code, vcp_version.major, vcp_version.minor, result);
   return result;
}


// convenience function
bool is_feature_writable_by_vcp_version(
       VCP_Feature_Table_Entry *  vfte,
       DDCA_MCCS_Version_Spec     vcp_version)
{
   return (get_version_sensitive_feature_flags(vfte, vcp_version) & DDCA_WRITABLE );
}


// convenience function
bool is_table_feature_by_vcp_version(
       VCP_Feature_Table_Entry *  vfte,
       DDCA_MCCS_Version_Spec     vcp_version)
{
   return (get_version_sensitive_feature_flags(vfte, vcp_version) & DDCA_NORMAL_TABLE );
}


// Checks if the table/non-table choice for a feature is version sensitive

bool is_version_conditional_vcp_type(VCP_Feature_Table_Entry * vfte) {
   bool result = false;

   Byte allflags = vfte->v30_flags |
                   vfte->v22_flags |
                   vfte->v21_flags |
                   vfte->v20_flags;

   bool some_nontable = allflags & (DDCA_CONT | DDCA_NC);
   bool some_table    = allflags & DDCA_NORMAL_TABLE;
   result = some_nontable && some_table;

   return result;
}


DDCA_Feature_Value_Entry *
get_version_specific_sl_values(
       VCP_Feature_Table_Entry *  vfte,
       DDCA_MCCS_Version_Spec     vcp_version)
{
   bool debug = false;
   DBGMSF(debug, "feature= 0x%02x, vcp_version = %d.%d", vfte->code, vcp_version.major, vcp_version.minor);
   DDCA_Feature_Value_Entry * result = NULL;
   if (vcp_version.major >= 3)
      result = vfte->v30_sl_values;
   else if (vcp_version.major == 2 && vcp_version.minor >= 2)
      result = vfte->v22_sl_values;

   if (!result && (vcp_version.major >= 3 || (vcp_version.major == 2 && vcp_version.minor == 1)) )
         result = vfte->v21_sl_values;

   if (!result)
      result = vfte->default_sl_values;

   DBGMSF(debug, "Feature = 0x%02x, vcp version=%d.%d, returning %p",
          vfte->code, vcp_version.major, vcp_version.minor, result);
   return result;
}


DDCA_Feature_Value_Entry *
get_version_sensitive_sl_values(
       VCP_Feature_Table_Entry *  vfte,
       DDCA_MCCS_Version_Spec     vcp_version)
{
   bool debug = false;
   DDCA_Feature_Value_Entry * result =
                                get_version_specific_sl_values(vfte, vcp_version);

    if (!result) {
       // vcp_version is lower than the first version level at which the field
       // was defined.  This can occur e.g. if scanning.  Pick the best
       // possible flags by scanning up in versions.
       if (vfte->v21_sl_values)
          result = vfte->v21_sl_values;
       else if (vfte->v30_sl_values)
          result = vfte->v30_sl_values;
       else if (vfte->v22_sl_values)
          result = vfte->v22_sl_values;

       // should it be a fatal error if not found?
       // if (!result) {
       //    PROGRAM_LOGIC_ERROR(
       //       "Feature = 0x%02x, Version=%d.%d: No version sensitive sl values",
       //       pvft_entry->code, vcp_version.major, vcp_version.minor);
      //  }
    }

   DBGMSF(debug, "Feature = 0x%02x, vcp version=%d.%d, returning %p",
          vfte->code, vcp_version.major, vcp_version.minor, result);
   return result;
}

#ifdef UNUSED
DDCA_Feature_Value_Entry *
get_highest_version_sl_values(
       VCP_Feature_Table_Entry *  vfte)
{
   bool debug = false;

   DDCA_MCCS_Version_Id version_found = DDCA_MCCS_VNONE;

   DDCA_Feature_Value_Entry * result = vfte->v22_sl_values;
   if (result)
      version_found = DDCA_MCCS_V22;
   else {
      result = vfte->v30_sl_values;
      if (result)
         version_found = DDCA_MCCS_V30;

      else {
         result = vfte->v21_sl_values;
         if (result)
            version_found = DDCA_MCCS_V21;
         else {
            result = vfte->default_sl_values;
            if (result)
               version_found = DDCA_MCCS_V20;
         }
      }
   }

   if (debug) {
      if (result)
         DBGMSG("Feature = 0x%02x, Returning sl value list for version %s",
                vfte->code, format_vcp_version_id(version_found));
      else
         DBGMSG("Feature = 0x%02x, No SL value table found", vfte->code);
   }
   return result;
}
#endif


/** Returns the version specific feature name from a feature table entry.
 *
 *  @param  vfte          feature table entry
 *  @param  vcp_version   VCP feature version
 *  @return feature name
 *
 *  @remark
 *  Returns a pointer into an internal data structure.  Caller should not free.
 */
char *
get_version_specific_feature_name(
       VCP_Feature_Table_Entry *  vfte,
       DDCA_MCCS_Version_Spec     vcp_version)
{
   bool debug = false;
   char * result = NULL;
   if (vcp_version.major >= 3)
      result = vfte->v30_name;
   else if (vcp_version.major == 2 && vcp_version.minor >= 2)
      result = vfte->v22_name;

   if (!result &&
       (vcp_version.major >= 3 || (vcp_version.major == 2 && vcp_version.minor >= 1))
      )
         result = vfte->v21_name;

   if (!result)
      result = vfte->v20_name;

   DBGMSF(debug, "Feature = 0x%02x, vcp version=%d.%d, returning %s",
          vfte->code, vcp_version.major, vcp_version.minor, result);
   return result;
}


/** Returns a version sensitive feature name from a feature table entry.
 *
 *  @param  vfte          feature table entry
 *  @param  vcp_version   VCP feature version
 *  @return feature name
 *
 *  @remark
 *  Returns a pointer into an internal data structure.  Caller should not free.
 */
char *
get_version_sensitive_feature_name(
       VCP_Feature_Table_Entry *  vfte,
       DDCA_MCCS_Version_Spec     vcp_version)
{
   bool debug = false;
   char * result = get_version_specific_feature_name(vfte, vcp_version);

   if (!result) {
      //    DBGMSG("Using original name field");
      //    result = pvft_entry->name;
      // vcp_version is lower than the first version level at which the field
      // was defined.  This can occur e.g. if scanning.  Pick the best
      // possible name by scanning up in versions.
      if (vfte->v21_name)
         result = vfte->v21_name;
      else if (vfte->v30_name)
         result = vfte->v30_name;
      else if (vfte->v22_name)
         result = vfte->v22_name;

      if (!result)
         DBGMSG("Feature = 0x%02x, Version=%d.%d: No version sensitive feature name found",
                vfte->code, vcp_version.major, vcp_version.minor);
   }

   DBGMSF(debug, "Feature = 0x%02x, vcp version=%d.%d, returning %s",
          vfte->code, vcp_version.major, vcp_version.minor, result);
   return result;
}


/** Returns a feature name from a feature table entry without specifying the VCP version.
 *
 *  For use when we don't know the version or just need a generic
 *  name, as in the vcpinfo command.
 *
 *  @param  vfte          feature table entry
 *  @return feature name
 *
 *  @remark
 *  Returns a pointer into an internal data structure.  Caller should not free.
 */
char *
get_non_version_specific_feature_name(
      VCP_Feature_Table_Entry * vfte)
{
   DDCA_MCCS_Version_Spec vspec = {2,2};
   return get_version_sensitive_feature_name(vfte, vspec);
}


/** Given a #VCP_Feature_Table_Entry, creates a VCP-version specific
 *  #Display_Feature_Metadata.
 *
 *  @param  vfte feature table entry
 *  @param  vspec VCP version
 *  @param  version_sensitive  if true, creation is version sensitive,
 *                             if false, version specific
 *  @return newly allocated #Display_Feature_Metadata, caller must free
 */
Display_Feature_Metadata *
extract_version_feature_info_from_feature_table_entry(
      VCP_Feature_Table_Entry *  vfte,
      DDCA_MCCS_Version_Spec     vspec,
      bool                       version_sensitive)
{
   bool debug = false;
   DBGMSF(debug, "vspec=%d.%d, version_sensitive=%s",
                 vspec.major, vspec.minor, sbool(version_sensitive));
   assert(vfte);
   // DDCA_MCCS_Version_Id version_id = mccs_version_spec_to_id(vspec);
   // if (debug)
   //    dbgrpt_vcp_entry(vfte, 2);

   Display_Feature_Metadata * dfm = dfm_new(vfte->code);

   // redundant, for now
   // info->version_id   = mccs_version_spec_to_id(vspec);
   dfm->vcp_version        = vspec;

   dfm->feature_flags = (version_sensitive)
         ? get_version_sensitive_feature_flags(vfte, vspec)
         : get_version_specific_feature_flags(vfte, vspec);

   dfm->feature_desc = (dfm->feature_desc) ? strdup(vfte->desc) : NULL;

   char * feature_name = (version_sensitive)
           ? get_version_sensitive_feature_name(vfte, vspec)
           : get_version_specific_feature_name(vfte, vspec);
   dfm->feature_name = strdup(feature_name);

   dfm->feature_flags |= vfte->vcp_global_flags;
   DDCA_Feature_Value_Entry * sl_values = (version_sensitive)
         ? get_version_sensitive_sl_values(vfte, vspec)
         // ? get_highest_version_sl_values(vfte)
         : get_version_specific_sl_values(vfte, vspec);
   dfm->sl_values = copy_sl_value_table(sl_values);
   // dfm->latest_sl_values = copy_sl_value_table(get_highest_version_sl_values(vfte));

   DBG_RET_STRUCT(debug, Display_Feature_Metadata, dbgrpt_display_feature_metadata, dfm);
   return dfm;
}


#ifdef UNUSED
/** Gets information about a VCP feature.
 *
 *  @param feature_code
 *  @param mccs_version_id
 *  @param with_default   if feature code not recognized, return dummy information,
 *                        otherwise return NULL
 *  @param version_sensitive
 *
 *  @retval pointer to DDCA_Version_Feature_Info
 *  @retval NULL if feature not found and with_default == false
 */
Display_Feature_Metadata *
get_version_feature_info_by_version_id_dfm(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_MCCS_Version_Id    mccs_version_id,
      bool                    with_default,
      bool                    version_sensitive)
{
   bool debug = false;
   DBGMSF(debug, "feature_code=0x%02x, mccs_version_id=%d(%s), with_default=%s, version_sensitive=%s",
         feature_code,
         mccs_version_id,
         vcp_version_id_name(mccs_version_id),
         sbool(with_default),
         sbool(version_sensitive));

   DDCA_MCCS_Version_Spec vspec = mccs_version_id_to_spec(mccs_version_id);

   return get_version_feature_info_by_vspec_dfm(feature_code, vspec, with_default, version_sensitive);
}
#endif



/** Given a VCP feature code and VCP version, creates a VCP-version specific
 *  #Display_Feature_Metadata.
 *
 *  @param  feature_code  VCP feature code
 *  @param  vspec         VCP version
 *  @param  with_default  synthesize an entry if no feature table entry
 *                        found for the feature code
 *  @param  version_sensitive  if true, creation is version sensitive,
 *                             if false, version specific
 *  @return newly allocated #Display_Feature_Metadata, caller must free
 */
Display_Feature_Metadata *
get_version_feature_info_by_vspec_dfm(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_MCCS_Version_Spec  vspec,
      bool                    with_default,
      bool                    version_sensitive)
{
   bool debug = false;
   DBGMSF(debug, "feature_code=0x%02x, mccs_version=%d.%d, with_default=%s, version_sensitive=%s",
         feature_code,
         vspec.major, vspec.minor,
         sbool(with_default),
         sbool(version_sensitive));

   Display_Feature_Metadata * dfm = NULL;

   VCP_Feature_Table_Entry * pentry =
         (with_default) ? vcp_find_feature_by_hexid_w_default(feature_code)
                        : vcp_find_feature_by_hexid(feature_code);
   if (pentry) {
      dfm = extract_version_feature_info_from_feature_table_entry(pentry, vspec, version_sensitive);

      if (pentry->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY)
         free_synthetic_vcp_entry(pentry);
   }

   if (debug) {
      if (dfm) {
         DBGMSG("Success.  feature info:");
         dbgrpt_display_feature_metadata(dfm, 1);
      }
      DBGMSG("Returning: %p", dfm);
   }
   return dfm;
}


//
// Functions that return a function for formatting a feature value
//

// Functions that lookup a value contained in a VCP_Feature_Table_Entry,
// returning a default if the value is not set for that entry.

Format_Normal_Feature_Detail_Function
get_nontable_feature_detail_function(
   VCP_Feature_Table_Entry *  vfte,
   DDCA_MCCS_Version_Spec     vcp_version)
{
   assert(vfte);
   bool debug = false;
   DBGMSF(debug, "Starting. feature = 0x%02x, vcp_version = %d.%d",
         vfte->code,
         vcp_version.major, vcp_version.minor);

   DDCA_Version_Feature_Flags version_specific_flags =
         get_version_sensitive_feature_flags(vfte, vcp_version);
   DBGMSF(debug, "version_specific_flags = 0x%04x = %s",
         version_specific_flags,
         interpret_feature_flags_t(version_specific_flags));
   assert(version_specific_flags);
   assert(version_specific_flags & DDCA_NON_TABLE);
   Format_Normal_Feature_Detail_Function func = NULL;
   if (version_specific_flags & DDCA_STD_CONT)
      func = format_feature_detail_standard_continuous;
   else if (version_specific_flags & DDCA_SIMPLE_NC)
      func = format_feature_detail_sl_lookup;
   else if (version_specific_flags & DDCA_WO_NC)
      func = NULL;      // but should never be called for this case
   else {
      assert(version_specific_flags & (DDCA_COMPLEX_CONT | DDCA_COMPLEX_NC | DDCA_NC_CONT));
      func = vfte->nontable_formatter;
      assert(func);
   }

   DBGMSF(debug, "Returning: %p", func);
   return func;
}


Format_Table_Feature_Detail_Function
get_table_feature_detail_function(
      VCP_Feature_Table_Entry *  vfte,
      DDCA_MCCS_Version_Spec     vcp_version)
{
   assert(vfte);

   // TODO:
   // if VCP_V2NC_V3T, then get version id
   // based on version id, choose .formatter or .formatter_v3
   // NO - test needs to be set in caller, this must return a Format_Feature_Detail_Function, which is not for Table

   Format_Table_Feature_Detail_Function func = vfte->table_formatter;
   if (!func)
      func = default_table_feature_detail_function;
   return func;
}


// Functions that apply formatting

/** Obtains the formatting function from a feature table entry
 *  based on the VCP version of the monitor, then applies that
 *  function to a non-table value to return that value as a
 *  string.
 *
 *  @param  vfte        feature table entry
 *  @param  vcp_version VCP version of monitor
 *  @param  code_info   non-table feature value
 *  @param  buffer      address of buffer in which to return formatted value
 *  @param  bufsz       buffer string
 *
 *  @return true if successful, false if not
 */

bool
vcp_format_nontable_feature_detail(
        VCP_Feature_Table_Entry *  vfte,
        DDCA_MCCS_Version_Spec     vcp_version,
        Nontable_Vcp_Value *       code_info,
        char *                     buffer,
        int                        bufsz)
{
   bool debug = false;
   DBGMSF(debug, "Starting. Code=0x%02x, vcp_version=%d.%d",
                 vfte->code, vcp_version.major, vcp_version.minor);

   Format_Normal_Feature_Detail_Function ffd_func =
         get_nontable_feature_detail_function(vfte, vcp_version);
   bool ok = ffd_func(code_info, vcp_version,  buffer, bufsz);
   return ok;
}

bool
vcp_format_table_feature_detail(
       VCP_Feature_Table_Entry *  vfte,
       DDCA_MCCS_Version_Spec     vcp_version,
       Buffer *                   accumulated_value,
       char * *                   aformatted_data
     )
{
   Format_Table_Feature_Detail_Function ffd_func =
         get_table_feature_detail_function(vfte, vcp_version);
   bool ok = ffd_func(accumulated_value, vcp_version, aformatted_data);
   return ok;
}


// Used only in deprecated API function ddca_get_formatted_vcp_value()
// to be iftested out once that function is deleted.

/* Given a feature table entry and a raw feature value,
 * return a formatted string interpretation of the value.
 *
 * Arguments:
 *    vcp_entry        vcp_feature_table_entry
 *    vcp_version      monitor VCP version
 *    valrec           feature value
 *    aformatted_data  location where to return formatted string value
 *
 * Returns:
 *    true if formatting successful, false if not
 *
 * It is the caller's responsibility to free the returned string.
 */
bool
vcp_format_feature_detail(
       VCP_Feature_Table_Entry * vfte,
       DDCA_MCCS_Version_Spec    vcp_version,
       DDCA_Any_Vcp_Value *      valrec,
       char * *                  aformatted_data
     )
{
   bool debug = false;
   DBGMSF(debug, "Starting");
   bool ok = true;
   *aformatted_data = NULL;

   DBGMSF(debug, "valrec->value_type = %d", valrec->value_type);
   char * formatted_data = NULL;
   if (valrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
      DBGMSF(debug, "DDCA_NON_TABLE_VCP_VALUE");
      Nontable_Vcp_Value* nontable_value = single_vcp_value_to_nontable_vcp_value(valrec);
      char workbuf[200];
      ok = vcp_format_nontable_feature_detail(
              vfte,
              vcp_version,
              nontable_value,
              workbuf,
              200);
      free(nontable_value);
      if (ok)
         formatted_data = strdup(workbuf);
   }
   else {        // TABLE_VCP_CALL
      DBGMSF(debug, "DDCA_TABLE_VCP_VALUE");
      ok = vcp_format_table_feature_detail(
            vfte,
            vcp_version,
            buffer_new_with_value(valrec->val.t.bytes, valrec->val.t.bytect, __func__),
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
// Functions that return or destroy a VCP_Feature_Table_Entry
//

/* Free a dynamically created VCP_Feature_Table_Entry..
 * Does nothing if the entry is in the permanently allocated
 * vcp_code_table.
 */
void free_synthetic_vcp_entry(VCP_Feature_Table_Entry * pfte) {
   // DBGMSG("pfte = %p", pfte);
   assert(memcmp(pfte->marker, VCP_FEATURE_TABLE_ENTRY_MARKER, 4) == 0);
   // DBGMSG("code=0x%02x", pfte->code);
   // report_vcp_feature_table_entry(pfte, 1);
   if (pfte->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY) {
#ifdef NO
      // if synthetic, strings were not malloced
      DBGMSG("pfte->desc=%p", pfte->desc);
      DBGMSG("pfte->v20_name=%p", pfte->v20_name);
      DBGMSG("pfte->v21_name=%p", pfte->v21_name);
      DBGMSG("pfte->v30_name=%p", pfte->v30_name);
      DBGMSG("pfte->v22_name=%p", pfte->v22_name);
      if (pfte->desc)
         free(pfte->desc);
      if (pfte->v20_name)
         free(pfte->v20_name);
      if (pfte->v21_name)
          free(pfte->v21_name);
      if (pfte->v30_name)
          free(pfte->v30_name);
      if (pfte->v22_name)
          free(pfte->v22_name);
#endif
      free(pfte);
   }
}


static VCP_Feature_Table_Entry *
vcp_new_feature_table_entry(DDCA_Vcp_Feature_Code id) {
   VCP_Feature_Table_Entry* pentry = calloc(1, sizeof(VCP_Feature_Table_Entry) );
   pentry->code = id;
   memcpy(pentry->marker, VCP_FEATURE_TABLE_ENTRY_MARKER, 4);
   // DBGMSG("id=0x%02x.  Returning: %p", id, pentry);
   return pentry;
}


/* Returns an entry in the VCP feature table based on its index in the table.
 *
 * Arguments:
 *    ndx     table index
 *
 * Returns:
 *    VCP_Feature_Table_Entry
 */
VCP_Feature_Table_Entry *
vcp_get_feature_table_entry(int ndx) {
   // DBGMSG("ndx=%d, vcp_code_count=%d  ", ndx, vcp_code_count );
   assert( 0 <= ndx && ndx < vcp_feature_code_count);
   return &vcp_code_table[ndx];
}


#ifdef UNUSED
VCP_Feature_Table_Entry *
vcp_create_dynamic_feature(
      DDCA_Vcp_Feature_Code   id,
      DDCA_Feature_Metadata * dynamic_metadata)
{
   bool debug = false;
   DBGMSF(debug, "Starting. id=0x%02x", id);
   VCP_Feature_Table_Entry * pentry = vcp_new_feature_table_entry(id);
   pentry->v20_name = dynamic_metadata->feature_name;
   pentry->desc = dynamic_metadata->feature_desc;

   pentry->v20_flags = dynamic_metadata->feature_flags;
   if (pentry->v20_flags & DDCA_SIMPLE_NC) {
      if (dynamic_metadata->sl_values) {
         // WRONG needs to a function that can find the lookup table
         pentry->nontable_formatter = format_feature_detail_sl_lookup;
           //   dyn_format_nontable_feature_detail
         pentry->default_sl_values = dynamic_metadata->sl_values;   // need to copy?
      }
      else {
         pentry->nontable_formatter = format_feature_detail_sl_byte;
      }
   }
   else if (pentry->v20_flags & DDCA_STD_CONT) {
      pentry->nontable_formatter = format_feature_detail_standard_continuous;
   }
   else {
      // 3/2018: complex cont may not work for API callers
      pentry->nontable_formatter = format_feature_detail_debug_bytes;
   }
   pentry->vcp_global_flags = DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY;   // indicates caller should free
   pentry->vcp_global_flags |= DDCA_USER_DEFINED;
   DBGMSF(debug, "Done");
   return pentry;
}
#endif


/* Creates a dummy VCP feature table entry for a feature code.
 * It is the responsibility of the caller to free this memory.
 *
 * Arguments:
 *    id     feature id
 *
 * Returns:
 *   created VCP_Feature_Table_Entry
 */
VCP_Feature_Table_Entry *
vcp_create_dummy_feature_for_hexid(DDCA_Vcp_Feature_Code id) {
   // DBGMSG("Starting. id=0x%02x", id);
   VCP_Feature_Table_Entry * pentry = vcp_new_feature_table_entry(id);

   if (id >= 0xe0) {
      pentry->v20_name = "Manufacturer Specific";
      pentry->desc     = "Feature code reserved for manufacturer use";
   }
   else {
      pentry->v20_name = "Unknown feature";
      pentry->desc     = "Undefined feature code";
   }
   // 3/2018: complex cont may not work for API callers
   pentry->nontable_formatter = format_feature_detail_debug_bytes;
   pentry->v20_flags = DDCA_RW | DDCA_COMPLEX_NC;
   pentry->vcp_global_flags = DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY;   // indicates caller should free
   pentry->vcp_global_flags |= DDCA_SYNTHETIC;                          // indicates generated feature metadata
   return pentry;
}


/* Creates a table type dummy VCP_Feature_Table_Entry for a feature code.
 * It is the responsibility of the caller to free this memory.
 *
 * Arguments:
 *    id     feature id
 *
 * Returns:
 *   created VCP_Feature_Table_Entry
 */
VCP_Feature_Table_Entry *
vcp_create_table_dummy_feature_for_hexid(DDCA_Vcp_Feature_Code id) {
   VCP_Feature_Table_Entry * pentry = vcp_new_feature_table_entry(id);
   if (id >= 0xe0) {
      pentry->v20_name = "Manufacturer Specific";
   }
   else {
      pentry->v20_name = "Unknown feature";
   }
   pentry->table_formatter = default_table_feature_detail_function,
   pentry->v20_flags = DDCA_RW | DDCA_NORMAL_TABLE;
   pentry->vcp_global_flags = DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY;   // indicates caller should free
   pentry->vcp_global_flags |= DDCA_SYNTHETIC;                          // indicates generated feature metadata
   return pentry;
}


/* Returns an entry in the VCP feature table based on the hex value
 * of its feature code.
 *
 * Arguments:
 *    id    feature id
 *
 * Returns:
 *    VCP_Feature_Table_Entry, NULL if not found
 *    Note this is a pointer into the VCP feature data structures.
 *    It should NOT be freed by the caller.
 */
VCP_Feature_Table_Entry *
vcp_find_feature_by_hexid(DDCA_Vcp_Feature_Code id) {
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
VCP_Feature_Table_Entry *
vcp_find_feature_by_hexid_w_default(DDCA_Vcp_Feature_Code id) {
   // DBGMSG("Starting. id=0x%02x ", id );
   VCP_Feature_Table_Entry * result = vcp_find_feature_by_hexid(id);
   if (!result)
      result = vcp_create_dummy_feature_for_hexid(id);
   // DBGMSG("Done.  ndx=%d. returning %p", ndx, result);
   return result;
}


////////////////////////////////////////////////////////////////////////
//
//  Functions to format Table values
//
///////////////////////////////////////////////////////////////////////


/* Value formatting function for use with table features when we don't
 * understand how to interpret the values.
 *
 * Arguments:
 *   data         byte buffer
 *   vcp_version  VCP Version spec
 *   presult      where to return formatted value
 *
 * Returns:
 *   Newly allocated formatted string.   It is the responsiblity of the
 *   caller to free this string.
 */
bool default_table_feature_detail_function(
      Buffer *                data,
      DDCA_MCCS_Version_Spec  vcp_version,
      char **                 presult)
{
   *presult = hexstring2(data->bytes, data->len, " " /*spacer*/, false /* upper case */, NULL, 0);
   return true;
}


//
// Functions applicable to multiple Table feature codes
//

// none so far


//
// Functions to format specific Table feature values
//

// x73
bool
format_feature_detail_x73_lut_size(
        Buffer *                data_bytes,
        DDCA_MCCS_Version_Spec  vcp_version,
        char **                 pformatted_result)
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

/* Returns the feature value table for a feature.   In a few cases, the table
 * is VCP version sensitive.
 *
 * Arguments:
 *    feature_code    VCP feature id
 *    vcp_version     VCP version of monitor
 *
 * Returns:
 *   pointer to feature value table, NULL if not found
 */
static DDCA_Feature_Value_Entry *
find_feature_value_table(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_MCCS_Version_Spec  vcp_version)
{
   bool debug = false;
   DBGMSF(debug, "Starting. feature_code=0x%02x, vcp_version=%d.%d",
                 feature_code, vcp_version.major, vcp_version.minor);

   DDCA_Feature_Value_Entry * result = NULL;
   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);

   // may not be found if called for capabilities and it's a mfg specific code
   if (pentry) {
      if (debug)
         dbgrpt_vcp_entry(pentry, 1);
      DDCA_Version_Feature_Flags feature_flags = get_version_sensitive_feature_flags(pentry, vcp_version);
      // if (feature_code == 0x66)                           // *** TEMP ***
    	//   feature_flags = DDCA_RW | VCP2_SIMPLE_NC;
      assert(feature_flags);

      // feature 0xca is of type DDCA_COMPLEX_NC when vcp version = 2.2,
      // it uses the sl byte for one lookup table, and the sh byte for another
      // This hack lets capabilities interpretation look up the sl byte
      // Normal interpretation of function xca uses dedicated function
      if ( (feature_flags & DDCA_SIMPLE_NC) || feature_code == 0xca) {
         result = get_version_specific_sl_values(pentry, vcp_version);
      }
   }

   DBGMSF(debug, "Done. feature_code=0x%02x. Returning feature value table at: %p",
                 feature_code, result);
   return result;
}


// hack to handle x14, where the sl values are not stored in the vcp feature table
// used by CAPABILITIES command
DDCA_Feature_Value_Entry *
find_feature_values_for_capabilities(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_MCCS_Version_Spec  vcp_version)
{
   bool debug = false;
   if (debug)
      DBGMSG("Starting. feature_code=0x%02x", feature_code);
   // ugh .. need to know the version number here
   // for now just assume vcp version < 3, return the table for v2
   DDCA_Feature_Value_Entry * result = NULL;
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
      result = find_feature_value_table(feature_code, vcp_version);
   }

   if (debug)
      DBGMSG("Starting. feature_code=0x%02x. Returning: %p", feature_code, result);
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
static
char * lookup_value_name(
          DDCA_Vcp_Feature_Code   feature_code,
          DDCA_MCCS_Version_Spec  vcp_version,
          Byte                    sl_value)
{
   bool debug = false;
   DBGMSF(debug, "feature_code=0x%02x, vcp_version=%d.%d, sl_value=-0x%02x",
                 feature_code, vcp_version.major, vcp_version.minor, sl_value);

   DDCA_Feature_Value_Entry * values_for_feature = find_feature_value_table(feature_code, vcp_version);
   assert(values_for_feature);
   char * name = sl_value_table_lookup(values_for_feature, sl_value);
   if (!name)
      name = "Invalid value";

   DBGMSF(debug, "Done. Returning: %s", name);
   return name;
}


////////////////////////////////////////////////////////////////////////
//
//  Functions to format Non-Table values
//
///////////////////////////////////////////////////////////////////////

//
// Value formatting functions for use with non-table features when we don't
// understand how to interpret the values for a feature.
//

// used when the value is calculated using the SL and SH bytes, but we haven't
// written a full interpretation function
bool format_feature_detail_debug_sl_sh(
        Nontable_Vcp_Value *     code_info,
        DDCA_MCCS_Version_Spec   vcp_version,
        char *                   buffer,
        int                      bufsz)
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
        Nontable_Vcp_Value *     code_info,
        DDCA_MCCS_Version_Spec   vcp_version,
        char *                   buffer,
        int                      bufsz)
{
   snprintf(buffer, bufsz,
            "mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x, max value = %5d, cur value = %5d",
            code_info->mh,        code_info->ml,
            code_info->sh,        code_info->sl,
            code_info->max_value, code_info->cur_value);
   return true;
}


bool format_feature_detail_debug_bytes(
        Nontable_Vcp_Value * code_info, DDCA_MCCS_Version_Spec vcp_version, char * buffer, int bufsz)
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
        Nontable_Vcp_Value *     code_info,
        DDCA_MCCS_Version_Spec   vcp_version,
        char *                   buffer,
        int                      bufsz)
{
    bool debug = false;
    DBGMSF(debug, "vcp_code=0x%02x, sl=0x%02x", code_info->vcp_code, code_info->sl);

    snprintf(buffer, bufsz, "Value: 0x%02x", code_info->sl);

    DBGMSF(debug, "Returning true, buffer=%s", buffer);
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
        Nontable_Vcp_Value *     code_info,
        DDCA_MCCS_Version_Spec   vcp_version,
        char *                   buffer,
        int                      bufsz)
{
   // TODO: lookup feature code in dynamic_sl_value_table
   char * s = lookup_value_name(code_info->vcp_code, vcp_version, code_info->sl);
   snprintf(buffer, bufsz,"%s (sl=0x%02x)", s, code_info->sl);
   return true;
}

// wrong, needs to be per-display
void register_dynamic_sl_values(
      DDCA_Vcp_Feature_Code feature_code,
      DDCA_Feature_Value_Entry * table)
{
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
        Nontable_Vcp_Value *     code_info,
        DDCA_MCCS_Version_Spec   vcp_version,
        char *                   buffer,
        int                      bufsz)
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
        Nontable_Vcp_Value *    code_info,
        DDCA_MCCS_Version_Spec  vcp_version,
        char *                  buffer,
        int                     bufsz)
{
   int cv = code_info->cur_value;
   snprintf(buffer, bufsz, "%5d (0x%04x)", cv, cv);
   return true;
}


//
// Custom functions for specific non-table VCP Feature Codes
//

// 0x02
static bool
format_feature_detail_x02_new_control_value(    // 0x02
        Nontable_Vcp_Value *    code_info,
        DDCA_MCCS_Version_Spec  vcp_version,
        char *                  buffer,
        int                     bufsz)
{
   char * name = NULL;
   switch(code_info->sl) {
      case 0x01: name = "No new control values";                            break;
      case 0x02: name = "One or more new control values have been saved";   break;
      case 0xff: name = "No user controls are present";                     break;
      default:   name = "<reserved code, must be ignored>";
   }
   snprintf(buffer, bufsz,
            "%s (0x%02x)" ,
            name, code_info->sl);
   // perhaps set to false if a reserved code
   return true;
}

// 0x0b
static bool
format_feature_detail_x0b_color_temperature_increment(
      Nontable_Vcp_Value *      code_info,
      DDCA_MCCS_Version_Spec    vcp_version,
      char *                    buffer,
      int                       bufsz)
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
static bool
format_feature_detail_x0c_color_temperature_request(
      Nontable_Vcp_Value *      code_info,
      DDCA_MCCS_Version_Spec    vcp_version,
      char *                    buffer,
      int                       bufsz)
{
   // int increments = code_info->cur_value;
   snprintf(buffer, bufsz,
            "3000 + %d * (feature 0B color temp increment) degree(s) Kelvin",
            code_info->cur_value);

   return true;
}

// 0x14
static bool
format_feature_detail_x14_select_color_preset(
      Nontable_Vcp_Value *      code_info,
      DDCA_MCCS_Version_Spec    vcp_version,
      char *                    buffer,
      int                       bufsz)
{
   bool debug = false;
   if (debug)
      DBGMSG("vcp_version=%d.%d", vcp_version.major, vcp_version.minor);

   bool ok = true;

   // char buf2[100];
   char * sl_msg = NULL;
   Byte sl = code_info->sl;
   if (sl == 0x00 || sl >= 0xe0) {
      sl_msg = "Invalid SL value.";
      ok = false;
   }

   // In 3.0 and 2.2, MH indicates the tolerance, with MH = 0x00 indicating no
   // tolerance specified.  Per the spec, if MH = 0x00, then SL values 0x03..0x0a
   // indicate relative adjustments from warmer to cooler. If MH != 0x00,
   // i.e. a tolerance is specified, the SL byte indicates color temperature as usual.
   // However, on the one v2.2 monitor that has been seen,
   // an HP Z22i, the SL values listed in the capabilities string
   // are clearly those for the usual color temperatures.
   // Therefore, we always treat the SL byte as absolute temperatures,
   // and for v3.0 and v2.2, we report the ML byte for tolerance.  (10/2019)

   // as observed:
   else {
      sl_msg = sl_value_table_lookup(x14_color_preset_absolute_values, code_info->sl);
      if (!sl_msg) {
         sl_msg = "Invalid SL value";
         ok = false;
      }
   }
#ifdef PER_THE_SPEC
   else if (vcp_version.major < 3 || code_info->mh == 0x00) {
      sl_msg = sl_value_table_lookup(x14_color_preset_absolute_values, code_info->sl);
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
#endif

   if ( vcp_version_le(vcp_version, DDCA_VSPEC_V21) ) {
      snprintf(buffer, bufsz,
              "%s (0x%02x)",
              sl_msg, sl);
   }
   else {
      char * mh_msg = NULL;

#ifdef ALT
      Byte mh = code_info->mh;
      char buf0[100];
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
#endif
      mh_msg = sl_value_table_lookup(x14_color_preset_tolerances, code_info->mh);
      if (!mh_msg) {
         mh_msg = "Invalid MH value";
         ok = false;
      }
      snprintf(buffer, bufsz,
               "%s (0x%02x), Tolerance: %s (0x%02x)",
               sl_msg, sl, mh_msg, code_info->mh);

   }
   return ok;
}

// 0x62
static bool
format_feature_detail_x62_audio_speaker_volume(
      Nontable_Vcp_Value *   code_info,
      DDCA_MCCS_Version_Spec vcp_version,
      char *                 buffer,
      int                    bufsz)
{
  assert (code_info->vcp_code == 0x62);
  // Continuous in 2.0, assume 2.1 is same
  // v2.2: doc lists as both C and NC, but documents special values, treat as NC
  // v3.0: NC with special x00 and xff values

  if (vcp_version_le(vcp_version,DDCA_VSPEC_V21)) {
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

// 0x72
static bool
format_feature_detail_x72_gamma(
      Nontable_Vcp_Value *     code_info,
      DDCA_MCCS_Version_Spec   vcp_version,
      char *                   buffer,
      int                      bufsz)
{
   assert (code_info->vcp_code == 0x72);

   char formatted_sh_sl[20];
   g_snprintf(formatted_sh_sl, 20, "0x%02x%02x", code_info->sh, code_info->sl);

   char * ssl = NULL;
   switch(code_info->sl) {
   case 0x00:    ssl = "White absolute adjustment";       break;
   case 0x01:    ssl = "Red absolute adjustment";         break;
   case 0x02:    ssl = "Green absolute adjustment";       break;
   case 0x03:    ssl = "Blue absolute adjustment";        break;
   case 0x04:    ssl = "White relative adjustment";       break;
   case 0x05:    ssl = "Disable all gamma correction in display";  break;
   default:      ssl = "Reserved, ignored";
   }

   // if absolute adjustment
   if (code_info->sl == 0x00  ||
       code_info->sl == 0x01  ||
       code_info->sl == 0x02  ||
       code_info->sl == 0x03  )
   {
      int    igamma = code_info->sh + 100;
      char   sgamma[10];
      char   sgamma2[10];
      g_snprintf (sgamma, 10, "%d", igamma);
      int slen = strlen(sgamma);
      char * a =  substr(sgamma, 0, slen-2);
      char * b = substr(sgamma, slen-2, 2);
      g_snprintf(sgamma2, 10, "%s.%s",a, b);
      free(a); free(b);
      g_snprintf(buffer, bufsz, "%s - Mode: %s (sl=0x%02x), gamma=%s (sh=0x%02x)",
                                 formatted_sh_sl,
                                 ssl, code_info->sl, sgamma2, code_info->sh);
   }
   else if (code_info->sl == 0x04) {  // relative adjustment
      char * ssh = NULL;
      switch(code_info->sh) {
      case 0x00:   ssh = "Display default gamma";    break;
      case 0x01:   ssh = "Default gamma - 0.1";      break;
      case 0x02:   ssh = "Default gamma - 0.2";      break;
      case 0x03:   ssh = "Default gamma - 0.3";      break;
      case 0x04:   ssh = "Default gamma - 0.4";      break;
      case 0x05:   ssh = "Default gamma - 0.5";      break;
      case 0x06:   ssh = "Default gamma - 0.6";      break;
      case 0x07:   ssh = "Default gamma - 0.7";      break;
      case 0x08:   ssh = "Default gamma - 0.8";      break;
      case 0x09:   ssh = "Default gamma - 0.9";      break;
      case 0x0a:   ssh = "Default gamma - 1.0";      break;

      case 0x11:   ssh = "Default gamma + 0.1";      break;
      case 0x12:   ssh = "Default gamma + 0.2";      break;
      case 0x13:   ssh = "Default gamma + 0.3";      break;
      case 0x14:   ssh = "Default gamma + 0.4";      break;
      case 0x15:   ssh = "Default gamma + 0.5";      break;
      case 0x16:   ssh = "Default gamma + 0.6";      break;
      case 0x17:   ssh = "Default gamma + 0.7";      break;
      case 0x18:   ssh = "Default gamma + 0.8";      break;
      case 0x19:   ssh = "Default gamma + 0.9";      break;
      case 0x1a:   ssh = "Default gamma + 1.0";      break;

      case 0x20:   ssh = "Disable all gamma correction";      break;

      default:     ssh = "Invalid SH value";
      }
      g_snprintf(buffer, bufsz, "%s - %s (sl=0x%02x) %s (sh=0x%02x)",
                                formatted_sh_sl,
                                ssl, code_info->sl, ssh, code_info->sh);

   }
   else if (code_info->sl == 0x05) {
      g_snprintf(buffer, bufsz, "%s - Mode: gamma correction disabled (sl=0x%02x), sh=0x%02x",
                                 formatted_sh_sl,
                                 code_info->sl, code_info->sh);
   }
   else {
      g_snprintf(buffer, bufsz, "%s - Invalid sl value. sl=0x%02x, sh=0x%02x",
                                formatted_sh_sl,
                                code_info->sl, code_info->sh);
   }

   return true;
}


// 0x8d
static bool
format_feature_detail_x8d_mute_audio_blank_screen(
        Nontable_Vcp_Value *     code_info,
        DDCA_MCCS_Version_Spec   vcp_version,
        char *                   buffer,
        int                      bufsz)
{
   assert (code_info->vcp_code == 0x8d);

   // As of v2.2, SH byte contains screen blank settings

   DDCA_Feature_Value_Entry * sl_values = x8d_tv_audio_mute_source_values;
   DDCA_Feature_Value_Entry * sh_values = x8d_sh_blank_screen_values;

   char * sl_name = sl_value_table_lookup(sl_values, code_info->sl);
   if (!sl_name)
      sl_name = "Invalid value";

   if (vcp_version_eq(vcp_version, DDCA_VSPEC_V22)) {
      char * sh_name = sl_value_table_lookup(sh_values, code_info->sh);
      if (!sh_name)
         sh_name = "Invalid value";
      snprintf(buffer, bufsz,"%s (sl=0x%02x), %s (sh=0x%02x)",
               sl_name, code_info->sl,
               sh_name, code_info->sh);
   }
   else {
      snprintf(buffer, bufsz,"%s (sl=0x%02x)",
               sl_name, code_info->sl);
   }
   return true;
}

// 0x8f, 0x91
static bool
format_feature_detail_x8f_x91_audio_treble_bass(
      Nontable_Vcp_Value *    code_info,
      DDCA_MCCS_Version_Spec  vcp_version,
      char *                  buffer,
      int                     bufsz)
{
  assert (code_info->vcp_code == 0x8f || code_info->vcp_code == 0x91);
  // Continuous in 2.0, assume 2.1 same as 2.0,
  // NC with reserved values x00 and xff values reserved in VCP 3.0, 2.2
  // This function should not be called if VCP2_STD_CONT

  // This function should not be called for VCP 2.0, 2.1
  // Standard continuous processing should be applied.
  // But as documentation, handle the C case as well.
  assert ( vcp_version_gt(vcp_version, DDCA_VSPEC_V21) );
  bool ok = true;
  if ( vcp_version_le(vcp_version, DDCA_VSPEC_V21))
  {
     snprintf(buffer, bufsz, "%d", code_info->sl);
  }
  else {
     if (code_info->sl == 0x00 || code_info->sl == 0xff) {
        snprintf(buffer, bufsz, "Invalid value: 0x%02x", code_info->sl );
        ok = false;
     }
     else if (code_info->sl < 0x80)
        snprintf(buffer, bufsz, "%d: Decreased (0x%02x = neutral - %d)",
                 code_info->sl, code_info->sl, 0x80 - code_info->sl);
     else if (code_info->sl == 0x80)
        snprintf(buffer, bufsz, "%d: Neutral (0x%02x)",
                 code_info->sl, code_info->sl);
     else
        snprintf(buffer, bufsz, "%d: Increased (0x%02x = neutral + %d)",
                 code_info->sl, code_info->sl, code_info->sl - 0x80);
  }
  return ok;
}

// 0x93
static bool
format_feature_detail_x93_audio_balance(
      Nontable_Vcp_Value *    code_info,
      DDCA_MCCS_Version_Spec  vcp_version,
      char *                  buffer,
      int                     bufsz)
{
  assert (code_info->vcp_code == 0x93);
  // Continuous in 2.0, NC in 3.0, 2.2, assume 2.1 same as 2.0
  // NC with reserved x00 and special xff values in 3.0,

  // This function should not be called if VCP_STD_CONT,
  // but leave v2 code in for completeness
  assert ( vcp_version_gt(vcp_version, DDCA_VSPEC_V21) );
  bool ok = true;
  if ( vcp_version_le(vcp_version, DDCA_VSPEC_V21))
  {
     snprintf(buffer, bufsz, "%d", code_info->sl);
  }
  else {
     if (code_info->sl == 0x00 ||
         // WTF: in VCP 2.2, value xff is reserved, in 3.0 it's part of the continuous range
         (code_info->sl == 0xff && vcp_version_eq(vcp_version, DDCA_VSPEC_V22) ) )
     {
        snprintf(buffer, bufsz, "Invalid value: 0x%02x", code_info->sl );
        ok = false;
     }
     else if (code_info->sl < 0x80)
        snprintf(buffer, bufsz, "%d: Left channel dominates (0x%02x = centered - %d)",
                 code_info->sl, code_info->sl, 0x80-code_info->sl);
     else if (code_info->sl == 0x80)
        snprintf(buffer, bufsz, "%d: Centered (0x%02x)",
                 code_info->sl, code_info->sl);
     else
        snprintf(buffer, bufsz, "%d Right channel dominates (0x%02x = centered + %d)",
                 code_info->sl, code_info->sl, code_info->sl-0x80);
  }
  return ok;
}

// 0xac
static bool
format_feature_detail_xac_horizontal_frequency(
      Nontable_Vcp_Value *    code_info,
      DDCA_MCCS_Version_Spec  vcp_version,
      char *                  buffer,
      int                     bufsz)
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
static bool
format_feature_detail_6_axis_hue(
      Nontable_Vcp_Value *     code_info,
      DDCA_MCCS_Version_Spec   vcp_version,
      char *                   buffer,
      int                      bufsz)
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
static bool
format_feature_detail_xae_vertical_frequency(
      Nontable_Vcp_Value *    code_info,
      DDCA_MCCS_Version_Spec  vcp_version,
      char *                  buffer,
      int                     bufsz)
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
static bool
format_feature_detail_xbe_link_control(
        Nontable_Vcp_Value *    code_info,
        DDCA_MCCS_Version_Spec  vcp_version,
        char *                  buffer,
        int                     bufsz)
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
static bool
format_feature_detail_xc0_display_usage_time(
        Nontable_Vcp_Value * code_info, DDCA_MCCS_Version_Spec vcp_version, char * buffer, int bufsz)
{
   assert (code_info->vcp_code == 0xc0);
   uint usage_time;
   // DBGMSG("vcp_version=%d.%d", vcp_version.major, vcp_version.minor);

   // TODO: Control with Output_Level
   // v2 spec says this is a 2 byte value, says nothing about mh, ml
   if (vcp_version.major >= 3) {
      if (code_info->mh != 0x00) {
         SEVEREMSG("Data error.  Mh byte = 0x%02x, should be 0x00 for display usage time",
                    code_info->mh );
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
static bool
format_feature_detail_x6c_application_enable_key(
        Nontable_Vcp_Value * code_info,
        DDCA_MCCS_Version_Spec vcp_version,
        char * buffer,
        int bufsz)
{
   assert (code_info->vcp_code == 0xc6);

   snprintf(buffer, bufsz, "0x%02x%02x", code_info->sh, code_info->sl);
   return true;
 }

// 0xc8
static bool
format_feature_detail_xc8_display_controller_type(
        Nontable_Vcp_Value * info,  DDCA_MCCS_Version_Spec vcp_version, char * buffer, int bufsz)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   assert(info->vcp_code == 0xc8);
   bool ok = true;
   Byte mfg_id = info->sl;
   char *sl_msg = NULL;
   sl_msg = sl_value_table_lookup(xc8_display_controller_type_values, info->sl);
   if (!sl_msg) {
      sl_msg = "Unrecognized";
      ok = true;
   }

   // ushort controller_number = info->ml << 8 | info->sh;
   // spec is inconsistent, controller number can either be ML/SH or MH/ML
   // observation suggests it's ml and sh
   snprintf(buffer, bufsz,
            "Mfg: %s (sl=0x%02x), controller number: mh=0x%02x, ml=0x%02x, sh=0x%02x",
            sl_msg, mfg_id, info->mh, info->ml, info->sh);
   DBGTRC_RET_BOOL(debug, TRACE_GROUP, ok, "buffer = |%s|", buffer);
   return ok;
}

// xc9, xdf
static bool
format_feature_detail_xc9_xdf_version(
        Nontable_Vcp_Value * code_info, DDCA_MCCS_Version_Spec vcp_version, char * buffer, int bufsz)
{
   int version_number  = code_info->sh;
   int revision_number = code_info->sl;
   snprintf(buffer, bufsz, "%d.%d", version_number, revision_number);
   return true;
}

// 0xca
static bool
format_feature_detail_xca_osd_button_control(
      Nontable_Vcp_Value * info,  DDCA_MCCS_Version_Spec vcp_version, char * buffer, int bufsz)
{
   if (vcp_version_eq(vcp_version, DDCA_VSPEC_V22)) {
      char * sl_name = sl_value_table_lookup(xca_v22_osd_button_sl_values, info->sl);
      if (!sl_name)
         sl_name = "Invalid value";
      char * sh_name = sl_value_table_lookup(xca_v22_osd_button_sh_values, info->sh);
      if (!sh_name)
         sh_name = "Invalid value";
      g_snprintf(buffer, bufsz,"%s (sl=0x%02x), %s (sh=0x%02x)",
                 sl_name, info->sl,
                 sh_name, info->sh);
   }
   else {
      char * sl_name = sl_value_table_lookup(xca_osd_values, info->sl);
      if (!sl_name)
         sl_name = "Invalid value";
      g_snprintf(buffer, bufsz,"%s (sl=0x%02x)",
                 sl_name, info->sl);
   }

   return true;
}

// 0xce
static bool
format_feature_detail_xce_aux_display_size(
        Nontable_Vcp_Value *      code_info,
        DDCA_MCCS_Version_Spec    vcp_version,
        char *                    buffer,
        int                       bufsz)
{
   assert (code_info->vcp_code == 0xce);

   int rows = (code_info->sl & 0xc0) >> 6;
   int chars_per_row = code_info->sl & 0x3f;
   snprintf(buffer, bufsz, "Rows=%d, characters/row=%d (sl=0x%02x)",
            rows, chars_per_row, code_info->sl);
   return true;
 }


/////////////////////////////////////////////////////////////////////////
//
// Feature_Value_Entry tables (SL byte value lookup)
// Used for Simple NC features
//
/////////////////////////////////////////////////////////////////////////

// {0x00,NULL} is the end of list marker. 0x00 might be a valid value, but NULL never is

// 0x02
static DDCA_Feature_Value_Entry x02_new_control_values[] = {
      // values identical in 2.0, 3.0, 2.2 specs
      {0x01, "No new control values"},
      {0x02, "One or more new control values have been saved"},
      {0xff, "No user controls are present"},
      {0x00, NULL}
};

static DDCA_Feature_Value_Entry x03_soft_controls_values[] = {
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
static DDCA_Feature_Value_Entry x14_color_preset_absolute_values[] = {
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

// MH byte for V2.2, V3.0
static DDCA_Feature_Value_Entry x14_color_preset_tolerances[] = {
     {0x00, "Unspecified"},
     {0x01, "1%"},
     {0x02, "2%"},
     {0x03, "3%"},
     {0x04, "4%"},
     {0x05, "5%"},
     {0x06, "6%"},
     {0x07, "7%"},
     {0x08, "8%"},
     {0x09, "9%"},
     {0x0a, "10%"},
     {0x00, NULL}       // end of list marker
};

// 0x1e, 0x1f
static DDCA_Feature_Value_Entry x1e_x1f_auto_setup_values[] = {
      {0x00, "Auto setup not active"},
      {0x01, "Performing auto setup"},
      // end of values for 0x1e, v2.0
      {0x02, "Enable continuous/periodic auto setup"},
      {0x00, NULL}       // end of list marker
};

// 0x60: These are MCCS V2 values.   In V3, x60 is type table.
// see also EloView Remote Mgt Local Cmd Set document
DDCA_Feature_Value_Entry x60_v2_input_source_values[] = {
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
DDCA_Feature_Value_Entry x63_speaker_select_values[] = {
      {0x00,  "Front L/R"},
      {0x01,  "Side L/R"},
      {0x02,  "Rear L/R"},
      {0x03,  "Center/Subwoofer"},
      {0x00,  NULL}
};

// 0x66
DDCA_Feature_Value_Entry x66_ambient_light_sensor_values[] = {
      {0x01,  "Disabled"},
      {0x02,  "Enabled"},
      {0x00,  NULL}
};

// 0x82: Horizontal Mirror
DDCA_Feature_Value_Entry x82_horizontal_flip_values[] = {
      {0x00, "Normal mode"},
      {0x01, "Mirrored horizontally mode"},
      {0x00,  NULL}
};

// 0x84: Horizontal Mirror
DDCA_Feature_Value_Entry x84_vertical_flip_values[] = {
      {0x00, "Normal mode"},
      {0x01, "Mirrored vertically mode"},
      {0x00,  NULL}
};

// 0x8b
DDCA_Feature_Value_Entry x8b_tv_channel_values[] = {
      {0x01, "Increment channel"},
      {0x02, "Decrement channel"},
      {0x00, NULL}
};

// 0x8d: Audio Mute
static DDCA_Feature_Value_Entry x8d_tv_audio_mute_source_values[] = {
      {0x01, "Mute the audio"},
      {0x02, "Unmute the audio"},
      {0x00,  NULL}
};

// 0x8d: SH byte values only apply in v2.2
static DDCA_Feature_Value_Entry x8d_sh_blank_screen_values[] = {
      {0x01, "Blank the screen"},
      {0x02, "Unblank the screen"},
      {0x00,  NULL}
};

// 0x86: Display Scaling
DDCA_Feature_Value_Entry x86_display_scaling_values[] = {
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
DDCA_Feature_Value_Entry x87_sharpness_values[] = {
      {0x01,  "Filter function 1"},
      {0x02,  "Filter function 2"},
      {0x03,  "Filter function 3"},
      {0x04,  "Filter function 4"},
      {0x00,  NULL}
};

// 0x94
DDCA_Feature_Value_Entry x94_audio_stereo_mode_values[] = {
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
DDCA_Feature_Value_Entry x99_window_control_values[] = {
      {0x00,  "No effect"},
      {0x01,  "Off"},
      {0x02,  "On"},
      {0x00,  NULL}
};

// 0xa2
DDCA_Feature_Value_Entry xa2_auto_setup_values[] = {
      {0x01,  "Off"},
      {0x02,  "On"},
      {0x00,  NULL}
};

// 0xa5
static DDCA_Feature_Value_Entry xa5_window_select_values[] = {
      {0x00, "Full display image area selected except active windows"},
      {0x01, "Window 1 selected"},
      {0x02, "Window 2 selected"},
      {0x03, "Window 3 selected"},
      {0x04, "Window 4 selected"},
      {0x05, "Window 5 selected"},
      {0x06, "Window 6 selected"},
      {0x07, "Window 7 selected"},
      {0x00, NULL}     // terminator
};

// 0xaa
static DDCA_Feature_Value_Entry xaa_screen_orientation_values[] = {
      {0x01, "0 degrees"},
      {0x02, "90 degrees"},
      {0x03, "180 degrees"},
      {0x04, "270 degrees"},
      {0xff, "Display cannot supply orientation"},
      {0x00, NULL}     // terminator
};

// 0xb0
static  DDCA_Feature_Value_Entry xb0_settings_values[] =
   {
     {0x01, "Store current settings in the monitor"},
     {0x02, "Restore factory defaults for current mode"},
     {0x00, NULL}    // termination entry
};

// 0xb2
static DDCA_Feature_Value_Entry xb2_flat_panel_subpixel_layout_values[] = {
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
      {0x00, NULL}     // terminator
};

// 0xb6
static DDCA_Feature_Value_Entry xb6_v20_display_technology_type_values[] = {
          { 0x01, "CRT (shadow mask)"},
          { 0x02, "CRT (aperture grill)"},
          { 0x03, "LCD (active matrix)"},   // TFT in 2.0
          { 0x04, "LCos"},
          { 0x05, "Plasma"},
          { 0x06, "OLED"},
          { 0x07, "EL"},
          { 0x08, "MEM"},     // MEM in 2.0
          {0x00, NULL}     // terminator
};

// 0xb6
static DDCA_Feature_Value_Entry xb6_display_technology_type_values[] = {
          { 0x01, "CRT (shadow mask)"},
          { 0x02, "CRT (aperture grill)"},
          { 0x03, "LCD (active matrix)"},   // TFT in 2.0
          { 0x04, "LCos"},
          { 0x05, "Plasma"},
          { 0x06, "OLED"},
          { 0x07, "EL"},
          { 0x08, "Dynamic MEM"},     // MEM in 2.0
          { 0x09, "Static MEM"},      // not in 2.0
          {0x00, NULL}     // terminator
};

// 0xc8
DDCA_Feature_Value_Entry xc8_display_controller_type_values[] = {
   {0x01,  "Conexant"},
   {0x02,  "Genesis"},
   {0x03,  "Macronix"},
   {0x04,  "IDT"},    // was MRT, 2.2a and 3.0 update have IDT
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
   {0x14,  "Silicon Optics"},  // added in 3.0 Update Document 3/20/2007
   // end of MCCS 3.0 values, beginning of values added in 2.2:
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
   {0x00, NULL}     // terminator
};
DDCA_Feature_Value_Entry * pxc8_display_controller_type_values = xc8_display_controller_type_values;

// 0xca
static DDCA_Feature_Value_Entry xca_osd_values[] = {
      {0x01, "OSD Disabled"},
      {0x02, "OSD Enabled"},
      {0xff, "Display cannot supply this information"},
      {0x00, NULL}     // terminator
};

// 0xca, v2.2
static DDCA_Feature_Value_Entry xca_v22_osd_button_sl_values[] = {
      {0x00, "Host OSD control unsupported"},
      {0x01, "OSD disabled, button events enabled"},
      {0x02, "OSD enabled, button events enabled"},
      {0x03, "OSD disabled, button events disabled"},
      {0xff, "Display cannot supply this information"},
      {0x00, NULL}    // terminator
};

// 0xca, v2.2
static DDCA_Feature_Value_Entry xca_v22_osd_button_sh_values[] = {
      {0x00, "Host control of power unsupported"},
      {0x01, "Power button disabled, power button events enabled"},
      {0x02, "Power button enabled, power button events enabled"},
      {0x03, "Power button disabled, power button events disabled"},
      {0x00, NULL}    // terminator
};

// 0xcc
// Note in v2.2 spec:
//   Typo in Version 2.1.  10h should read 0Ah.  If a parser
//   encounters a display with MCCS v2.1 using 10h it should
//   auto-correct to 0Ah.
static DDCA_Feature_Value_Entry xcc_osd_language_values[] = {
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
          {0x16, "Finnish"},
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
DDCA_Feature_Value_Entry xd0_v2_output_select_values[] = {
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
static  DDCA_Feature_Value_Entry xd6_power_mode_values[] =
   { {0x01, "DPM: On,  DPMS: Off"},
     {0x02, "DPM: Off, DPMS: Standby"},
     {0x03, "DPM: Off, DPMS: Suspend"},
     {0x04, "DPM: Off, DPMS: Off" },
     {0x05, "Write only value to turn off display"},
     {0x00, NULL}    // termination entry
};

// 0xd7
static  DDCA_Feature_Value_Entry xd7_aux_power_output_values[] =
   { {0x01, "Disable auxiliary power"},
     {0x02, "Enable Auxiliary power"},
     {0x00, NULL}    // termination entry
};

// 0xda
static  DDCA_Feature_Value_Entry xda_scan_mode_values[] =
   { {0x00, "Normal operation"},
     {0x01, "Underscan"},
     {0x02, "Overscan"},
     {0x03, "Widescreen" },                        // in 2.0 spec, not in 3.0
     {0x00, NULL}    // termination entry
};

// 0xdb
static  DDCA_Feature_Value_Entry xdb_image_mode_values[] =
   { {0x00, "No effect"},
     {0x01, "Full mode"},
     {0x02, "Zoom mode"},
     {0x03, "Squeeze mode" },
     {0x04, "Variable"},
     {0x00, NULL}    // termination entry
};

static DDCA_Feature_Value_Entry xdc_display_application_values[] = {
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
   {0x00, NULL}     // terminator
};


#pragma GCC diagnostic push
// not suppressing warning, why?, but removing static does avoid warning
#pragma GCC diagnostic ignored "-Wunused-variable"
// 0xde         // write-only feature
DDCA_Feature_Value_Entry xde_wo_operation_mode_values[] =
   { {0x01, "Stand alone"},
     {0x02, "Slave (full PC control)"},
     {0x00, NULL}    // termination entry
};
#pragma GCC diagnostic pop


////////////////////////////////////////////////////////////////////////
//
//  Virtual Control Panel (VCP) Feature Code Master Table
//
////////////////////////////////////////////////////////////////////////

//TODO:
// In 2.0 spec, only the first letter of the first word of a name is capitalized
// In 3.0/2.2, the first letter of each word of a name is capitalized
// Need to make this consistent throughout the table

/** Feature Code Master Table */
VCP_Feature_Table_Entry vcp_code_table[] = {
   {  .code=0x01,
      // defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_MISC,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Causes a CRT to perform a degauss cycle",
      .v20_flags = DDCA_WO |DDCA_WO_NC,
      .v20_name = "Degauss",
   },
   {  .code=0x02,
      .vcp_spec_groups = VCP_SPEC_MISC,
      // defined in 2.0, identical in 3.0, 2.2
      .nontable_formatter = format_feature_detail_x02_new_control_value,   // ??
      .default_sl_values = x02_new_control_values,  // ignored, hardcoded in nontable_formatter
      .desc = "Indicates that a display user control (other than power) has been "
              "used to change and save (or autosave) a new value.",
      .v20_flags = DDCA_RW | DDCA_COMPLEX_NC,
      .v20_name = "New control value",
   },
   {  .code=0x03,                        // defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_MISC,
      .default_sl_values = x03_soft_controls_values,
      .desc = "Allows display controls to be used as soft keys",
      .v20_flags =  DDCA_RW | DDCA_SIMPLE_NC,
      .v20_name = "Soft controls",
   },
   {  .code=0x04,                        // Defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_PRESET,
      .desc = "Restore all factory presets including brightness/contrast, "
              "geometry, color, and TV defaults.",
      .vcp_subsets = VCP_SUBSET_COLOR,                // but note WO
      .v20_flags =  DDCA_WO | DDCA_WO_NC,
      .v20_name = "Restore factory defaults",
   },
   {  .code=0x05,                        // Defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_PRESET,
      .vcp_subsets = VCP_SUBSET_COLOR,                // but note WO
      .desc = "Restore factory defaults for brightness and contrast",
      .v20_flags =  DDCA_WO | DDCA_WO_NC,
      .v20_name = "Restore factory brightness/contrast defaults",
   },
   {  .code=0x06,                        // Defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_PRESET,
      .desc = "Restore factory defaults for geometry adjustments",
      .v20_flags =  DDCA_WO | DDCA_WO_NC,
      .v20_name = "Restore factory geometry defaults",
   },
   {  .code=0x08,                        // Defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_PRESET,
      .vcp_subsets = VCP_SUBSET_COLOR,                   // but note WO
      .desc = "Restore factory defaults for color settings.",
      .v20_flags =  DDCA_WO | DDCA_WO_NC,
      .v20_name = "Restore color defaults",
   },
   {  .code=0x0A,                        // Defined in 2.0, identical in 3.0
      .vcp_spec_groups = VCP_SPEC_PRESET,
      .vcp_subsets = VCP_SUBSET_TV,
      .desc = "Restore factory defaults for TV functions.",
      .v20_flags =  DDCA_WO | DDCA_WO_NC,
      .v20_name = "Restore factory TV defaults",
   },
   {  .code=0x0b,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // Defined in 2.0
      .nontable_formatter=format_feature_detail_x0b_color_temperature_increment,
      // from 2.0 spec:
      // .desc="Allows the display to specify the minimum increment in which it can "
      //       "adjust the color temperature.",
      // simpler:
      .desc="Color temperature increment used by feature 0Ch Color Temperature Request",
      .vcp_subsets = VCP_SUBSET_COLOR,
      .v20_flags =  DDCA_RO | DDCA_COMPLEX_NC,
      .v20_name="Color temperature increment",
   },
   {  .code=0x0c,
      //.name="Color temperature request",
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // Defined in 2.0
      .nontable_formatter=format_feature_detail_x0c_color_temperature_request,
      .desc="Specifies a color temperature (degrees Kelvin)",   // my desc
      .vcp_subsets = VCP_SUBSET_COLOR,
      .v20_flags = DDCA_RW | DDCA_COMPLEX_CONT,
      .v20_name="Color temperature request",
   },
   {  .code=0x0e,                              // Defined in 2.0
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .desc="Increase/decrease the sampling clock frequency.",
      .v20_flags =  DDCA_RW | DDCA_STD_CONT,
      .v20_name="Clock",
   },
   {  .code=0x10,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // Defined in 2.0, name changed in 3.0, what is it in 2.1?
      //.name="Luminosity",
     .desc="Increase/decrease the brightness of the image.",
     .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
     .v20_flags =  DDCA_RW | DDCA_STD_CONT,
     .v20_name = "Brightness",
     .v30_name = "Luminosity",
   },
   {  .code=0x11,
      // not in 2.0, is in 3.0, assume introduced in 2.1
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .nontable_formatter=format_feature_detail_debug_bytes,
      .desc = "Select contrast enhancement algorithm respecting flesh tone region",
      .vcp_subsets = VCP_SUBSET_COLOR,
      .v21_flags = DDCA_RW | DDCA_COMPLEX_NC,
      .v21_name = "Flesh tone enhancement",
   },
   {  .code=0x12,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // Defined in 2.0, identical in 3.0
      .desc="Increase/decrease the contrast of the image.",
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Contrast",
   },
   {  .code=0x13,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // not in 2.0, is in 3.0, assume first defined in 2.1
      // deprecated in 2.2
      .nontable_formatter=format_feature_detail_debug_bytes,
      .desc = "Increase/decrease the specified backlight control value",
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v21_flags = DDCA_RW | DDCA_COMPLEX_CONT,
      .v21_name  = "Backlight control",
      // DDCA_RW | DDCA_COMPLEX_CONT included in v22 flags so that
      // information is available in ddcui to display, also in
      // case the feature is implemented on a V22 monitor, even though
      // it is deprecated
      .v22_flags = DDCA_DEPRECATED | DDCA_RW | DDCA_COMPLEX_CONT,
   },
   {  .code=0x14,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // Defined in 2.0, different in 3.0, 2.2
      // what is appropriate choice for 2.1 ?
      // interpretation varies depending on VCP version
      .nontable_formatter=format_feature_detail_x14_select_color_preset,
      .default_sl_values= x14_color_preset_absolute_values,  // ignored, referenced in nontable_formatter
      .desc="Select a specified color temperature",
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v20_flags = DDCA_RW | DDCA_SIMPLE_NC,
      .v20_name  = "Select color preset",
      .v30_flags = DDCA_RW | DDCA_COMPLEX_NC,
      .v22_flags = DDCA_RW | DDCA_COMPLEX_NC,
   },
   {  .code=0x16,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .desc="Increase/decrease the luminesence of red pixels",   // my simplification
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Video gain: Red",
   },
   {  .code=0x17,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // not in 2.0, is in 3.0 and 2.2, assume valid for 2.1
      .desc="Increase/decrease the degree of compensation",   // my simplification
      .vcp_subsets = VCP_SUBSET_COLOR,
      .v21_flags = DDCA_RW | DDCA_STD_CONT,
      .v21_name = "User color vision compensation",
   },
   {  .code=0x18,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .desc="Increase/decrease the luminesence of green pixels",   // my simplification
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Video gain: Green",
   },
   {  .code=0x1a,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .desc="Increase/decrease the luminesence of blue pixels",   // my simplification
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Video gain: Blue",
   },
   {  .code=0x1c,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // defined in 2.0, identical in 3.0
      .desc="Increase/decrease the focus of the image",  // my simplification
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Focus",
   },
   {  .code=0x1e,                                                // Done
      .vcp_spec_groups = VCP_SPEC_IMAGE,    // 2.0, 3.0, 2.2
      // Defined in 2.0, additional value in 3.0, 2.2
      .default_sl_values = x1e_x1f_auto_setup_values,
      .desc="Perform autosetup function (H/V position, clock, clock phase, "
            "A/D converter, etc.",
      .v20_flags = DDCA_RW | DDCA_SIMPLE_NC,
      .v20_name = "Auto setup",
   },
   {  .code=0x1f,                                               // Done
      // not defined in 2.0, is defined in 3.0, 2.2, assume introduced in 2.1
      .vcp_spec_groups = VCP_SPEC_IMAGE,   // 3.0
      .default_sl_values = x1e_x1f_auto_setup_values,
      .desc="Perform color autosetup function (R/G/B gain and offset, A/D setup, etc. ",
      .vcp_subsets = VCP_SUBSET_COLOR,
      .v21_flags = DDCA_RW | DDCA_SIMPLE_NC,
      .v21_name = "Auto color setup",
   },
   {  .code=0x20,        // Done
      // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2 except for name
      // When did name change to include "(phase)"?  Assuming 2.1
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increasing (decreasing) this value moves the image toward "
              "the right (left) of the display.",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name="Horizontal Position",
      .v21_name="Horizontal Position (Phase)",
   },
   {  .code=0x22,         // Done
      // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increase/decrease the width of the image.",
      .v20_name="Horizontal Size",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
   },
   {  .code=0x24,       // Done
      // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increasing (decreasing) this value causes the right and left "
              "sides of the image to become more (less) convex.",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name="Horizontal Pincushion",
   },
   {  .code=0x26,              // Done
      // Group 8.4 Geometry, identical in 2.0, 3.0, 2.2
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increasing (decreasing) this value moves the center section "
              "of the image toward the right (left) side of the display.",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
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
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
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
      .v21_flags=DDCA_RW | DDCA_STD_CONT,
   },
   {  .code = 0x2a,           // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry, identical in 3.0, 2.2
      .desc = "Increase/decrease the density of pixels in the image center.",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name="Horizontal Linearity",
   },
   {  .code = 0x2c,               // Done
      // Group 8.4 Geometry
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increasing (decreasing) this value shifts the density of pixels "
              "from the left (right) side to the right (left) side of the image.",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Horizontal Linearity Balance",
   },
   {  .code=0x2e,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // not defined in 2.0, is defined in 3.0
      // assume new in 2.1
      .vcp_subsets = VCP_SUBSET_COLOR,
      .nontable_formatter=format_feature_detail_debug_bytes,
      .desc = "Gray Scale Expansion",
      .v21_flags = DDCA_RW |  DDCA_COMPLEX_NC,
      .v21_name = "Gray scale expansion",
   },
   {  .code=0x30,                // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increasing (decreasing) this value moves the image toward "
              "the top (bottom) edge of the display.",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name="Vertical Position",
      .v21_name="Vertical Position (Phase)",
   },
   {  .code=0x32,                // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry.  Did name change with 2.1 or 3.0/2.2? - assuming 2.1
      .desc = "Increase/decreasing the height of the image.",
      .v20_flags=DDCA_RW |  DDCA_STD_CONT,
      .v20_name="Vertical Size",
   },
   {  .code=0x34,                                  // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry.  Identical in 2.0, 3.0, 2.2
      .desc = "Increasing (decreasing) this value will cause the top and "
              "bottom edges of the image to become more (less) convex.",
      .v20_flags =  DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Vertical Pincushion",
   },
   {  .code=0x36,                                 // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Increasing (decreasing) this value will move the center "
              "section of the image toward the top (bottom) edge of the display.",
      .v20_flags =  DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Vertical Pincushion Balance",
   },
   {  .code=0x38,                                 // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry.  Assume name changed with 2.1
      .desc = "Increasing (decreasing) this value shifts the red pixels up (down) "
              "across the image and the blue pixels down (up) across the image "
              "with respect to the green pixels.",
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
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
      .v21_flags= DDCA_RW | DDCA_STD_CONT,
   },
   {  .code=0x3a,                                  // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry
      .desc = "Increase/decease the density of scan lines in the image center.",
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Vertical Linearity",
   },
   {  .code=0x3c,                                       // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // Group 8.4 Geometry
      .desc = "Increase/decrease the density of scan lines in the image center.",
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Vertical Linearity Balance",
   },
   {  .code=0x3e,
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_MISC,     // 2.0: MISC
      // Defined in 2.0, identical in 3.0
      .desc="Increase/decrease the sampling clock phase shift",
      .v20_flags =  DDCA_RW | DDCA_STD_CONT,
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
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
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
      .v21_flags= DDCA_RW | DDCA_STD_CONT,
      .v21_name="Vertical Parallelogram",
   },
   {  .code=0x42,                                  // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // When did name change from 2.0?   assume 2.1
      .desc = "Increasing (decreasing) this value will increase (decrease) the "
              "ratio between the horizontal size at the top of the image and the "
              "horizontal size at the bottom of the image.",
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
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
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
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
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
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
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
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
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
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
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
      .v20_name="Bottom Corner",
      .v21_name="Bottom Corner Flare",   // ??
   },
   {  .code=0x4c,                                          // Done
      .vcp_spec_groups = VCP_SPEC_GEOMETRY,
      .vcp_subsets = VCP_SUBSET_CRT,
      // name is different in 3.0, assume changed in 2.1
      .desc = "Increasing (decreasing) this value moves the bottom end of the "
              "image to the right (left).",
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
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
      .v20_flags = DDCA_RO |  DDCA_COMPLEX_NC,
      .v20_name  = "Active control",
   },
   {  .code= 0x54,
      .vcp_spec_groups = VCP_SPEC_MISC,
      // not defined in 2.0, defined in 3.0, 2.2 identical to 3.0, assume new in 2.1
      .nontable_formatter=format_feature_detail_debug_bytes,    // TODO: write formatter
      .desc = "Controls features aimed at preserving display performance",
      .v21_flags =  DDCA_RW | DDCA_COMPLEX_NC,
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
      .v20_flags = DDCA_RW |  DDCA_STD_CONT,
      .v20_name="Horizontal Moire",
   },
   {  .code=0x58,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // defined in 2.0
      .desc="Increase/decrease vertical moire cancellation.",  // my simplification
      .v20_flags = DDCA_RW |  DDCA_STD_CONT,
      .v20_name="Vertical Moire",
   },
   {  .code=0x59,                                                    // DONE
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // not in 2.0, defined in 3.0, assume new as of 2.1
      // observed in Dell U3011, which is VCP 2.1
      // .nontable_formatter=format_feature_detail_sl_byte,
      // Per spec, values range from x00..xff
      // On U3011 monitor, values range from 0..100, returned max value is 100
      // Change the .desc to fit observed reality
      // Same comments apply to other saturation codes
      //.desc = "Value < 127 decreases red saturation, 127 nominal (default) value, "
      //          "> 127 increases red saturation",
      .desc="Increase/decrease red saturation",
      .v21_flags = DDCA_RW | DDCA_STD_CONT,
      .v21_name = "6 axis saturation: Red",
   },
   {  .code=0x5a,                                                    // DONE
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // not in 2.0, defined in 3.0, assume new as of 2.1
      // .nontable_formatter=format_feature_detail_sl_byte,
      // .desc = "Value < 127 decreases yellow saturation, 127 nominal (default) value, "
      //         "> 127 increases yellow saturation",
      .desc="Increase/decrease yellow saturation",
      .v21_flags = DDCA_RW | DDCA_STD_CONT,
      .v21_name = "6 axis saturation: Yellow",
   },
   {  .code=0x5b,                                                   // DONE
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // not in 2.0, defined in 3.0, assume new as of 2.1
      // .nontable_formatter=format_feature_detail_sl_byte,
      // .desc = "Value < 127 decreases green saturation, 127 nominal (default) value, "
      //           "> 127 increases green saturation",
      .desc="Increase/decrease green saturation",
      .v21_flags = DDCA_RW | DDCA_STD_CONT,
      .v21_name = "6 axis saturation: Green",
   },
   {  .code=0x5c,                                                   // DONE
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // not in 2.0, defined in 3.0, assume new as of 2.1
      // .nontable_formatter=format_feature_detail_sl_byte,
      // .desc = "Value < 127 decreases cyan saturation, 127 nominal (default) value, "
      //          "> 127 increases cyan saturation",
      .desc="Increase/decrease cyan saturation",
      .v21_flags = DDCA_RW | DDCA_STD_CONT,
      .v21_name = "6 axis saturation: Cyan",
   },
   {  .code=0x5d,                                                   // DONE
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // not in 2.0, defined in 3.0, assume new as of 2.1
      // .nontable_formatter=format_feature_detail_sl_byte,
      // .desc = "Value < 127 decreases blue saturation, 127 nominal (default) value, "
      //          "> 127 increases blue saturation",
      .desc="Increase/decrease blue saturation",
      .v21_flags = DDCA_RW | DDCA_STD_CONT,
      .v21_name = "6 axis saturation: Blue",
   },
   {  .code=0x5e,                                                  // DONE
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      // not in 2.0, defined in 3.0, assume new as of 2.1
      // .nontable_formatter=format_feature_detail_sl_byte,
      // .desc = "Value < 127 decreases magenta saturation, 127 nominal (default) value, "
      //            "> 127 increases magenta saturation",
      .desc="Increase/decrease magenta saturation",
      .v21_flags = DDCA_RW | DDCA_STD_CONT,
      .v21_name = "6 axis saturation: Magenta",
   },
   {  .code=0x60,
      .vcp_spec_groups = VCP_SPEC_MISC,
      // MCCS 2.0, 2.2: NC, MCCS 3.0: T
      .default_sl_values = x60_v2_input_source_values,     // used for all but v3.0
      .desc = "Selects active video source",
      .v20_flags =  DDCA_RW | DDCA_SIMPLE_NC,
      .v20_name = "Input Source",
      .v30_flags = DDCA_RW | DDCA_NORMAL_TABLE,
      .v22_flags = DDCA_RW | DDCA_SIMPLE_NC
   },
   {  .code=0x62,
      .vcp_spec_groups = VCP_SPEC_AUDIO,
      .vcp_subsets = VCP_SUBSET_AUDIO,
      // is in 2.0, special coding as of 3.0 assume changed as of 3.0
      // In MCCS spec v2.2a, summary table 7.4 Audio Adjustments lists this
      // feature as type C, but detailed documentation in section
      // 8.6 Audio Adjustments lists it as type NC and documents
      // reserved values.  Treat as NC.
      // .nontable_formatter=format_feature_detail_standard_continuous,
      .nontable_formatter=format_feature_detail_x62_audio_speaker_volume,
      // requires special handling for V3, mix of C and NC, SL byte only
      .desc = "Adjusts speaker volume",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v30_flags = DDCA_RW | DDCA_NC_CONT,
      .v22_flags = DDCA_RW | DDCA_NC_CONT,
      .v20_name = "Audio speaker volume",
   },
   {  .code=0x63,
      .vcp_spec_groups = VCP_SPEC_AUDIO,
      // not in 2.0, is in 3.0, assume new as of 2.1
      .vcp_subsets = VCP_SUBSET_AUDIO,
      .desc="Selects a group of speakers",
      .v21_flags = DDCA_RW | DDCA_SIMPLE_NC,
      .default_sl_values = x63_speaker_select_values,
      .v21_name = "Speaker Select",
   },
   {  .code=0x64,
      .vcp_spec_groups = VCP_SPEC_AUDIO,
      // is in 2.0, n. unlike x62, this code is identically defined in 2.0, 30
      .vcp_subsets = VCP_SUBSET_AUDIO,
      .desc = "Increase/decrease microphone gain",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Audio: Microphone Volume",
   },
   {  .code=0x66,
      .vcp_spec_groups = VCP_SPEC_MISC,
      // not in 2.0, assume new in 2.1
      // however, seen on NEC PA241W, which reports VCP version as 2.0
      .nontable_formatter=format_feature_detail_debug_bytes,
      .desc = "Enable/Disable ambient light sensor",
      .v21_flags = DDCA_RW | DDCA_SIMPLE_NC,
      .v21_name = "Ambient light sensor",
      .default_sl_values = x66_ambient_light_sensor_values,
   },
   {  .code=0x6b,
      // First defined in MCCS 2.2,
      .vcp_spec_groups=VCP_SPEC_IMAGE,
      .desc="Increase/decrease the white backlight level",
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v22_name = "Backlight Level: White",
      .v22_flags = DDCA_RW | DDCA_STD_CONT,
   },
   { .code=0x6c,
     .vcp_spec_groups = VCP_SPEC_IMAGE,
     .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
     // Defined in 2.0, name change in ?
     //.name="Video black level: Red",
     // .nontable_formatter=format_feature_detail_standard_continuous,
     .desc="Increase/decrease the black level of red pixels",  // my simplification
     .v20_flags =  DDCA_RW |DDCA_STD_CONT,
     .v20_name = "Video black level: Red",
   },
   {  .code=0x6d,
      // First defined in MCCS 2.2,
      .vcp_spec_groups=VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .desc="Increase/decrease the red backlight level",
      .v22_name = "Backlight Level: Red",
      .v22_flags = DDCA_RW | DDCA_STD_CONT,
   },
   { .code=0x6e,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      // Defined in 2.0, name change in ?
      .desc="Increase/decrease the black level of green pixels",  // my simplification
      .v20_flags =  DDCA_RW |DDCA_STD_CONT,
      .v20_name = "Video black level: Green",
   },
   {  .code=0x6f,
      // First defined in MCCS 2.2,
      .vcp_spec_groups=VCP_SPEC_IMAGE,
      .desc="Increase/decrease the green backlight level",
      // .nontable_formatter=format_feature_detail_standard_continuous,
      .v22_name = "Backlight Level: Green",
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v22_flags = DDCA_RW | DDCA_STD_CONT,
   },
   {  .code=0x70,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // Defined in 2.0, name change in ?
      .desc="Increase/decrease the black level of blue pixels",  // my simplification
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v20_flags =  DDCA_RW |DDCA_STD_CONT,
      .v20_name = "Video black level: Blue",
   },
   {  .code=0x71,
      // First defined in MCCS 2.2,
      .vcp_spec_groups=VCP_SPEC_IMAGE,
      .desc="Increase/decrease the blue backlight level",
      .v22_name = "Backlight Level: Blue",
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      .v22_flags = DDCA_RW | DDCA_STD_CONT,
   },
   {  .code=0x72,
      // 2.0: undefined, 3.0 & 2.2: defined, assume defined in 2.1
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_COLOR,
      .desc="Select relative or absolute gamma",
      // .nontable_formatter=format_feature_detail_debug_sl_sh,  // TODO implement function
      .nontable_formatter=format_feature_detail_x72_gamma,
      .v21_flags = DDCA_RW | DDCA_COMPLEX_NC,
      .v21_name = "Gamma",
   },
   {  .code=0x73,
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_LUT,
      // VCP_SPEC_MISC in 2.0, VCP_SPEC_IMAGE in 3.0, 2.1? 2.2
      .table_formatter=format_feature_detail_x73_lut_size,
      .desc = "Provides the size (number of entries and number of bits/entry) "
              "for the Red, Green, and Blue LUT in the display.",
      .v20_flags = DDCA_RO| DDCA_NORMAL_TABLE,
      .v20_name  = "LUT Size",
   },
   {  .code=0x74,
      // VCP_SPEC_MISC in 2.0, VCP_SPEC_IMAGE in 3.0, 2.2
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_LUT,
      .table_formatter = default_table_feature_detail_function,
      .desc = "Writes a single point within the display's LUT, reads a single point from the LUT",
      .v20_flags = DDCA_RW | DDCA_NORMAL_TABLE,
      .v20_name = "Single point LUT operation",
   },
   {  .code=0x75,
      // VCP_SPEC_MISC in 2.0, VCP_SPEC_IMAGE in 3.0, 2.2
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_LUT,
      .table_formatter = default_table_feature_detail_function,
      .desc = "Load (read) multiple values into (from) the display's LUT",
      .v20_flags = DDCA_RW | DDCA_NORMAL_TABLE,
      .v20_name = "Block LUT operation",
   },
   {  .code=0x76,
      // defined in 2.0, 3.0
      .vcp_spec_groups = VCP_SPEC_MISC,
      .vcp_subsets = VCP_SUBSET_LUT,
      .desc = "Initiates a routine resident in the display",
      .v20_flags = DDCA_WO | DDCA_WO_TABLE,
      .v20_name = "Remote Procedure Call",
   },
   {  .code=0x78,
      .vcp_spec_groups = VCP_SPEC_MISC,
      // Not in 2.0
      // defined in 3.0, 2.2 - name and description vary, but content identical
      // what to do about 2.1?  assume it's defined in 2.1 and identical to 3.0
      .desc = "Causes a selected 128 byte block of Display Identification Data "
              "(EDID or Display ID) to be read",

      .v21_flags =  DDCA_RO | DDCA_NORMAL_TABLE,
      .v21_name  = "EDID operation",
      .v30_flags = DDCA_RO | DDCA_NORMAL_TABLE,
      .v30_name  = "EDID operation",
      .v22_flags = DDCA_RO | DDCA_NORMAL_TABLE,
      .v22_name = "Display Identification Operation",
   },
   {  .code=0x7a,
      .vcp_spec_groups = VCP_SPEC_IMAGE,      // v2.0
      // defined in 2.0, not in 3.0 or 2.2, what to do for 2.1?
      .desc="Increase/decrease the distance to the focal plane of the image",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Adjust Focal Plane",
      .v30_flags = DDCA_DEPRECATED | DDCA_RW | DDCA_STD_CONT,
      .v22_flags = DDCA_DEPRECATED | DDCA_RW | DDCA_STD_CONT,
   },
   {  .code=0x7c,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // defined in 2.0, is in 3.0
      // my simplification, merger of 2.0 and 3.0 descriptions:
      .desc="Increase/decrease the distance to the zoom function of the projection lens (optics)",
      .v20_flags =  DDCA_RW | DDCA_STD_CONT,
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
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
      .v30_flags = DDCA_DEPRECATED | DDCA_RW | DDCA_STD_CONT,
      .v22_flags = DDCA_DEPRECATED | DDCA_RW | DDCA_STD_CONT,
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
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
      .v20_name="Keystone",
      .v21_flags = DDCA_DEPRECATED | DDCA_RW | DDCA_STD_CONT,
   },
   {  .code=0x82,
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,   // 2.0: Image, 3.0: Geometry
      .default_sl_values = x82_horizontal_flip_values,
      .desc="Flip picture horizontally",
      // DESIGN ISSUE!!!
      // This feature is WO in 2.0 spec, RW in 3.0, what is it in 2.2
      // implies cannot use global_flags to store RO/RW/WO
      .v20_flags =  DDCA_WO | DDCA_WO_NC,
      .v20_name = "HorFlip",
      .v21_name = "Horizontal Mirror (Flip)",
      .v21_flags =  DDCA_RW | DDCA_SIMPLE_NC,
      .v21_sl_values = x82_horizontal_flip_values,
   },
   {  .code=0x84,
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,   // 2.0: Image, 3.0: Geometry
      .default_sl_values = x84_vertical_flip_values,
      // DESIGN ISSUE!!!
      // This feature is WO in 2.0 spec, RW in 3.0, what is it in 2.2
      // implies cannot use global_flags to store RO/RW/WO
      .desc="Flip picture vertically",
      .v20_flags =  DDCA_WO | DDCA_WO_NC,
      .v20_name = "VertFlip",
      .v21_name = "Vertical Mirror (Flip)",
      .v21_flags =  DDCA_RW | DDCA_SIMPLE_NC,
      .v21_sl_values = x84_vertical_flip_values,
   },
   {  .code=0x86,                                              // Done
      // v2.0 spec cross ref lists as MISC, but defined in IMAGE table, assume IMAGE
      .vcp_spec_groups = VCP_SPEC_IMAGE,              // 2.0: IMAGE
      //.name="DisplayScaling",
      //.flags = VCP_RW | VCP_NON_CONT,
      .default_sl_values = x86_display_scaling_values,
      .desc = "Control the scaling (input vs output) of the display",
      .v20_flags = DDCA_RW | DDCA_SIMPLE_NC,
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
      .v20_flags=DDCA_RW | DDCA_SIMPLE_NC,
      .v20_name="Sharpness",
      .v21_flags=DDCA_RW | DDCA_STD_CONT,
   },
   {  .code=0x88,
      // 2.0 cross ref lists this as IMAGE, but defined in MISC table
      // 2.2: Image
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_IMAGE,
      .vcp_subsets = VCP_SUBSET_CRT,    // ???
      .desc = "Increase (decrease) the velocity modulation of the horizontal "
              "scan as a function of the change in luminescence level",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Velocity Scan Modulation",
   },
   {  .code=0x8a,
      .vcp_spec_groups = VCP_SPEC_IMAGE,     // 2.0, 2.2, 3.0
      // Name differs in 3.0 vs 2.0, assume 2.1 same as 2.0
      .vcp_subsets = VCP_SUBSET_TV | VCP_SUBSET_COLOR,
      .desc = "Increase/decrease the amplitude of the color difference "
              "components of the video signal",
      .v20_flags =DDCA_RW |  DDCA_STD_CONT,
      .v20_name  = "TV Color Saturation",
      .v21_name  = "Color Saturation",
   },
   {  .code=0x8b,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .vcp_subsets = VCP_SUBSET_TV,
      .desc = "Increment (1) or decrement (2) television channel",
      .v20_flags=DDCA_WO | DDCA_WO_NC,
      .v20_name="TV Channel Up/Down",
      .default_sl_values=x8b_tv_channel_values,
   },
   {  .code = 0x8c,
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_IMAGE,  // 2.0: SPEC, 2.2: IMAGE
      .vcp_subsets = VCP_SUBSET_TV,
      .desc = "Increase/decrease the amplitude of the high frequency components  "
              "of the video signal",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "TV Sharpness",
   },
   {  .code=0x8d,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      // v3.0 same as v2.0
      // v2.2 adds SH byte for screen blank
      .vcp_subsets = VCP_SUBSET_TV | VCP_SUBSET_AUDIO,
      .desc = "Mute/unmute audio, and (v2.2) screen blank",
      .nontable_formatter=format_feature_detail_x8d_mute_audio_blank_screen,
       .default_sl_values = x8d_tv_audio_mute_source_values, // ignored, table hardcoded in nontable_formatter
      .v20_flags = DDCA_RW | DDCA_SIMPLE_NC,
      .v20_name = "Audio Mute",
      .v22_flags = DDCA_RW | DDCA_COMPLEX_NC,
      .v22_name = "Audio mute/Screen blank",
   },
   {  .code=0x8e,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .vcp_subsets = VCP_SUBSET_TV,
      .desc = "Increase/decrease the ratio between blacks and whites in the image",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "TV Contrast",
   },
   {  .code=0x8f,
      .vcp_spec_groups = VCP_SPEC_AUDIO,    // v2.0, 2.2, 3.0
      .vcp_subsets = VCP_SUBSET_AUDIO,
      .desc="Emphasize/de-emphasize high frequency audio",
      // n. for v2.0 name in spec is "TV-audio treble".  "TV" prefix is
      // dropped in 2.2, 3.0.  just use the more generic term for all versions
      // similar comments apply to x91, x93
      // In MCCS spec v2.2a, summary table 7.4 Audio Adjustments lists this
      // feature as type C, but detailed documentation in section
      // 8.6 Audio Adjustments lists it as type NC and documents
      // reserved values.  Treat as NC.
      .v20_name="Audio Treble",
      // requires special handling for V3, mix of C and NC, SL byte only
      .nontable_formatter=format_feature_detail_x8f_x91_audio_treble_bass,
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v30_flags = DDCA_RW | DDCA_NC_CONT,
      .v22_flags = DDCA_RW | DDCA_NC_CONT,
   },
   {  .code=0x90,
      .vcp_spec_groups = VCP_SPEC_MISC,     // 2.0
      .vcp_subsets = VCP_SUBSET_TV | VCP_SUBSET_COLOR,
      .desc = "Increase/decrease the wavelength of the color component of the video signal. "
              "AKA tint.  Applies to currently active interface",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "Hue",
   },
   {  .code=0x91,
      .vcp_spec_groups = VCP_SPEC_AUDIO,    // v2.0, 2.2, 3.0
      .vcp_subsets = VCP_SUBSET_AUDIO,
      .desc="Emphasize/de-emphasize low frequency audio",
      .v20_name="Audio Bass",
      // requires special handling for V3.0 and v2.2: mix of C and NC, SL byte only
      // In MCCS spec v2.2a, summary table 7.4 Audio Adjustments lists this
      // feature as type C, but detailed documentation in section
      // 8.6 Audio Adjustments lists it as type NC and documents
      // reserved values.  Treat as NC.
      .nontable_formatter=format_feature_detail_x8f_x91_audio_treble_bass,
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v30_flags = DDCA_RW | DDCA_NC_CONT,
      .v22_flags = DDCA_RW | DDCA_NC_CONT,
   },
   {  .code=0x92,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .vcp_subsets = VCP_SUBSET_TV,
      .desc = "Increase/decrease the black level of the video",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "TV Black level/Brightness",
      .v21_name = "TV Black level/Luminesence",
   },
   {  .code=0x93,
      .vcp_spec_groups = VCP_SPEC_AUDIO,    // v2.0, 2.2, 3.0
      .vcp_subsets = VCP_SUBSET_AUDIO,
      .desc="Controls left/right audio balance",
      .v20_name="Audio Balance L/R",
      // requires special handling for V3 and v2.2, mix of C and NC, SL byte only
      .nontable_formatter=format_feature_detail_x93_audio_balance,
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v30_flags = DDCA_RW | DDCA_NC_CONT,
      .v22_flags = DDCA_RW | DDCA_NC_CONT,
   },
   {  .code=0x94,
      .vcp_spec_groups = VCP_SPEC_AUDIO,    // v2.0
      // name changed in 3.0, assume applies to 2.1
      .vcp_subsets = VCP_SUBSET_TV | VCP_SUBSET_AUDIO,
      .desc="Select audio mode",
      .v20_name="Audio Stereo Mode",
      .v21_name="Audio Processor Mode",
      .v20_flags = DDCA_RW | DDCA_SIMPLE_NC,
      .default_sl_values=x94_audio_stereo_mode_values,
   },
   {  .code=0x95,                               // Done
      .vcp_spec_groups = VCP_SPEC_WINDOW | VCP_SPEC_GEOMETRY,  // 2.0: WINDOW, 3.0: GEOMETRY
      .vcp_subsets = VCP_SUBSET_WINDOW,
      .desc="Top left X pixel of an area of the image",
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
      .v20_name="Window Position(TL_X)",
   },
   {  .code=0x96,                                             // Done
      .vcp_spec_groups = VCP_SPEC_WINDOW | VCP_SPEC_GEOMETRY,  // 2.0: WINDOW, 3.0: GEOMETRY
      .vcp_subsets = VCP_SUBSET_WINDOW,
      .desc="Top left Y pixel of an area of the image",
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
      .v20_name="Window Position(TL_Y)",
   },
   {  .code=0x97,                                            // Done
      .vcp_spec_groups = VCP_SPEC_WINDOW | VCP_SPEC_GEOMETRY,  // 2.0: WINDOW, 3.0: GEOMETRY
      .vcp_subsets = VCP_SUBSET_WINDOW,
      .desc="Bottom right X pixel of an area of the image",
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
      .v20_name="Window Position(BR_X)",
   },
   {  .code=0x98,                                                    // Done
      .vcp_spec_groups = VCP_SPEC_WINDOW | VCP_SPEC_GEOMETRY,  // 2.0: WINDOW, 3.0: GEOMETRY
      .vcp_subsets = VCP_SUBSET_WINDOW,
      .desc="Bottom right Y pixel of an area of the image",
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
      .v20_name="Window Position(BR_Y)",
   },
   {  .code=0x99,
      .vcp_spec_groups = VCP_SPEC_WINDOW,     // 2.0: WINDOW
      // in 2.0,  not in 3.0 or 2.2, what is correct choice for 2.1?
      .vcp_subsets = VCP_SUBSET_WINDOW,
      .default_sl_values = x99_window_control_values,
      .desc="Enables the brightness and color within a window to be different "
            "from the desktop.",
      .v20_flags= DDCA_RW | DDCA_SIMPLE_NC,
      .v20_name="Window control on/off",
      .v22_flags = DDCA_DEPRECATED | DDCA_RW | DDCA_SIMPLE_NC,
      .v30_flags = DDCA_DEPRECATED | DDCA_RW | DDCA_SIMPLE_NC,
   },
   {  .code=0x9a,
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,   // 2.0: WINDOW, 3.0, 2.2: IMAGE
      .vcp_subsets = VCP_SUBSET_WINDOW,               // VCP_SUBSET_COLOR?
      // 2.2 spec notes:
      // 1) should be used in conjunction with VCP 99h
      // 2) Not recommended for new designs, see A5h for alternate
      .desc="Changes the contrast ratio between the area of the window and the "
            "rest of the desktop",
      .v20_flags= DDCA_RW | DDCA_STD_CONT,
      .v20_name="Window background",
   },
   {  .code=0x9b,                                             // in 2.0, same in 3.0
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,    // 2.0: WINDOW, 3.0: IMAGE
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      // U3011 doesn't implement the spec that puts the midpoint at 127,
      // Just interpret this and the other hue fields as standard continuous
      // .desc = "Value < 127 shifts toward magenta, 127 no effect, "
      //         "> 127 shifts toward yellow",
      // .nontable_formatter=format_feature_detail_6_axis_hue,
      // .v20_flags =DDCA_RW |  VCP2_COMPLEX_CONT,
      .desc = "Decrease shifts toward magenta, "
              "increase shifts toward yellow",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "6 axis hue control: Red",
   },
   {  .code=0x9c,                                             // in 2.0, same in 3.0
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,    // 2.0: WINDOW, 3.0: IMAGE
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      // .nontable_formatter=format_feature_detail_6_axis_hue,
      // .desc = "Value < 127 shifts toward green, 127 no effect, "
      //         "> 127 shifts toward red",
      // .v20_flags = DDCA_RW | VCP2_COMPLEX_CONT,
      .desc = "Decrease shifts toward green, "
              "increase shifts toward red",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "6 axis hue control: Yellow",
   },
   {  .code=0x9d,                                             // in 2.0, same in 3.0
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,    // 2.0: WINDOW, 3.0: IMAGE
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      // .nontable_formatter=format_feature_detail_6_axis_hue,
      // .desc = "Value < 127 shifts toward yellow, 127 no effect, "
      //        "> 127 shifts toward cyan",
      // .v20_flags =DDCA_RW |  VCP2_COMPLEX_CONT,
      .desc = "Decrease shifts toward yellow, "
              "increase shifts toward cyan",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "6 axis hue control: Green",
   },
   {  .code=0x9e,                                             // in 2.0, same in 3.0
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,    // 2.0: WINDOW, 3.0: IMAGE
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      // .nontable_formatter=format_feature_detail_6_axis_hue,
      // .desc = "Value < 127 shifts toward green, 127 no effect, "
      //         "> 127 shifts toward blue",
      // .v20_flags = DDCA_RW | VCP2_COMPLEX_CONT,               // VCP_SUBSET_COLORMGT?
      .desc = "Decrease shifts toward green, "
              "increase shifts toward blue",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "6 axis hue control: Cyan",
   },
   {  .code=0x9f,
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      // .nontable_formatter=format_feature_detail_6_axis_hue,
      // .desc = "Value < 127 shifts toward cyan, 127 no effect, "
      //         "> 127 shifts toward magenta",
      // .v20_flags =DDCA_RW |  VCP2_COMPLEX_CONT,
      .desc = "Decrease shifts toward cyan, "
              "increase shifts toward magenta",
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .v20_name = "6 axis hue control: Blue",
   },
   {  .code=0xa0,
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_WINDOW,
      .vcp_subsets = VCP_SUBSET_COLOR | VCP_SUBSET_PROFILE,
      // .nontable_formatter=format_feature_detail_6_axis_hue,
      // .desc = "Value < 127 shifts toward blue, 127 no effect, "
      //         "> 127 shifts toward red",
      // .v20_flags = DDCA_RW | VCP2_COMPLEX_CONT,
      .v20_flags = DDCA_RW | DDCA_STD_CONT,
      .desc = "Decrease shifts toward blue, 127 no effect, "
              "increase shifts toward red",
      .v20_name = "6 axis hue control: Magenta",
   },
   {  .code=0xa2,                             // Defined in 2.0, same in 3.0
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      .desc="Turn on/off an auto setup function",
      .default_sl_values=xa2_auto_setup_values,
      .v20_flags = DDCA_WO | DDCA_WO_NC,
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
      .v20_flags = DDCA_RW | DDCA_COMPLEX_NC,
      .v30_flags = DDCA_RW | DDCA_NORMAL_TABLE,
      .v22_flags = DDCA_RW | DDCA_NORMAL_TABLE,
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
      .v20_flags = DDCA_RW | DDCA_SIMPLE_NC,
      .v20_name = "Change the selected window",
   },
   {  .code=0xaa,                                          // Done
      .vcp_spec_groups = VCP_SPEC_IMAGE | VCP_SPEC_GEOMETRY,    // 3.0: IMAGE, 2.0: GEOMETRY
      .default_sl_values=xaa_screen_orientation_values,
      .desc="Indicates screen orientation",
      .v20_flags=DDCA_RO | DDCA_SIMPLE_NC,
      .v20_name="Screen Orientation",
   },
   {  .code=0xac,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .nontable_formatter=format_feature_detail_xac_horizontal_frequency,
      .desc = "Horizontal sync signal frequency as determined by the display",
      // 2.0: 0xff 0xff 0xff indicates the display cannot supply this info
      .v20_flags = DDCA_RO | DDCA_COMPLEX_CONT,
      .v20_name  = "Horizontal frequency",
   },
   {  .code=0xae,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .nontable_formatter=format_feature_detail_xae_vertical_frequency,
      .desc = "Vertical sync signal frequency as determined by the display, "
              "in .01 hz",
      // 2.0: 0xff 0xff indicates the display cannot supply this info
      .v20_flags =DDCA_RO |  DDCA_COMPLEX_CONT,
      .v20_name  = "Vertical frequency",
   },
   {  .code=0xb0,
      .vcp_spec_groups = VCP_SPEC_PRESET,
      // Defined in 2.0, v3.0 spec clarifies that value to be set is in SL byte
      //.name="(Re)Store user saved values for cur mode",   // this was my name from the explanation
      .default_sl_values = xb0_settings_values,
      .desc = "Store/restore the user saved values for the current mode.",
      .v20_flags = DDCA_WO | DDCA_WO_NC,
      .v20_name = "Settings",
   },
   {  .code=0xb2,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .default_sl_values=xb2_flat_panel_subpixel_layout_values,
      .desc = "LCD sub-pixel structure",
      .v20_flags = DDCA_RO | DDCA_SIMPLE_NC,
      .v20_name = "Flat panel sub-pixel layout",
   },
   {  .code = 0xb4,
      .vcp_spec_groups = VCP_SPEC_CONTROL,       // 3.0
      .desc = "Indicates timing mode being sent by host",
      // not defined in 2.0, is defined in 3.0 and 2.2 as T
      // seen on U3011 which is v2.1
      // should this be seen as T or NC for 2.1?
      // if set T for 2.1, monitor returns NULL response, which
      // ddcutil interprets as unsupported.
      // if set NC for 2.1, response looks valid,
      // supported opcode is set
      .v21_name = "Source Timing Mode",
      .nontable_formatter = format_feature_detail_debug_bytes,
      .v21_flags = DDCA_RW | DDCA_COMPLEX_NC,
      .v30_flags = DDCA_RW | DDCA_NORMAL_TABLE,
      .v22_flags = DDCA_RW | DDCA_NORMAL_TABLE,
   },
   {  .code=0xb6,                                               // DONE
      .vcp_spec_groups = VCP_SPEC_MISC,     // 2.0, 3.0
      // v3.0 table not upward compatible w 2.0, assume changed as of 2.1
      .desc = "Indicates the base technology type",
      .default_sl_values=xb6_v20_display_technology_type_values,
      .v21_sl_values = xb6_display_technology_type_values,
      .v20_flags = DDCA_RO | DDCA_SIMPLE_NC,
      .v20_name = "Display technology type",
   },
   {  .code=0xb7,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .desc = "Video mode and status of a DPVL capable monitor",
      .v20_name = "Monitor status",
      .v20_flags = DDCA_RO | DDCA_COMPLEX_NC,
      .nontable_formatter = format_feature_detail_sl_byte,    //TODO: implement proper function
   },
   {  .code=0xb8,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .v20_name = "Packet count",
      .desc = "Counter for DPVL packets received",
      .v20_flags = DDCA_RW | DDCA_COMPLEX_CONT,
      .nontable_formatter = format_feature_detail_ushort,
   },
   {  .code=0xb9,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .v20_name = "Monitor X origin",
      .desc = "X origin of the monitor in the vertical screen",
      .v20_flags = DDCA_RW | DDCA_COMPLEX_CONT,
      .nontable_formatter = format_feature_detail_ushort,
   },
   {  .code=0xba,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .v20_name = "Monitor Y origin",
      .desc = "Y origin of the monitor in the vertical screen",
      .v20_flags = DDCA_RW | DDCA_COMPLEX_CONT,
      .nontable_formatter = format_feature_detail_ushort,
   },
   {  .code=0xbb,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .desc = "Error counter for the DPVL header",
      .v20_name = "Header error count",
      .v20_flags = DDCA_RW | DDCA_COMPLEX_CONT,
      .nontable_formatter = format_feature_detail_ushort,
   },
   {  .code=0xbc,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .desc = "CRC error counter for the DPVL body",
      .v20_name = "Body CRC error count",
      .v20_flags = DDCA_RW | DDCA_COMPLEX_CONT,
      .nontable_formatter = format_feature_detail_ushort,
   },
   {  .code=0xbd,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .desc = "Assigned identification number for the monitor",
      .v20_name = "Client ID",
      .v20_flags = DDCA_RW | DDCA_COMPLEX_CONT,
      .nontable_formatter = format_feature_detail_ushort,
   },
   {  .code=0xbe,
      .vcp_spec_groups = VCP_SPEC_DPVL,
      .vcp_subsets = VCP_SUBSET_DPVL,
      .desc = "Indicates status of the DVI link",
      .v20_name = "Link control",
      .v20_flags = DDCA_RW | DDCA_COMPLEX_NC,
      .nontable_formatter = format_feature_detail_xbe_link_control,
   },
   {  .code=0xc0,
      .vcp_spec_groups = VCP_SPEC_MISC,
      .nontable_formatter=format_feature_detail_xc0_display_usage_time,
      .desc = "Active power on time in hours",
      .v20_flags =DDCA_RO |  DDCA_COMPLEX_CONT,
      .v20_name = "Display usage time",
   },
   {  .code=0xc2,
      .vcp_spec_groups = VCP_SPEC_MISC,    // 2.0
      .desc = "Length in bytes of non-volatile storage in the display available "
              "for writing a display descriptor, max 256",
      .v20_flags = DDCA_RO | DDCA_STD_CONT,
      // should there be a different flag when we know value uses only SL?
      // or could this value be exactly 256?
      .v20_name = "Display descriptor length",
   },
   {  .code=0xc3,
      .vcp_spec_groups = VCP_SPEC_MISC, // 2.0
      .table_formatter = default_table_feature_detail_function,
      .desc="Reads (writes) a display descriptor from (to) non-volatile storage "
            "in the display.",
      .v20_flags = DDCA_RW | DDCA_NORMAL_TABLE,
      .v20_name = "Transmit display descriptor",
   },
   {  .code = 0xc4,
      .vcp_spec_groups = VCP_SPEC_MISC,  // 2.0
      .nontable_formatter=format_feature_detail_debug_bytes,
      .desc = "If enabled, the display descriptor shall be displayed when no video "
              "is being received.",
      .v20_flags = DDCA_RW | DDCA_COMPLEX_NC,
      // need to handle "All other values.  The display descriptor shall not be displayed"
      .v20_name = "Enable display of \'display descriptor\'",
   },
   {  .code=0xc6,
      .vcp_spec_groups = VCP_SPEC_MISC, // 2.0
      .nontable_formatter=format_feature_detail_x6c_application_enable_key,
      .desc = "A 2 byte value used to allow an application to only operate with known products.",
      .v20_flags = DDCA_RO | DDCA_COMPLEX_NC,
      .v20_name = "Application enable key",
   },
   {  .code=0xc8,
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_CONTROL,    // 2.0: MISC, 3.0: CONTROL
      .nontable_formatter=format_feature_detail_xc8_display_controller_type,
      .default_sl_values=xc8_display_controller_type_values, // ignored, hardcoded in nontable_formatter
      .desc = "Mfg id of controller and 2 byte manufacturer-specific controller type",
      .v20_flags = DDCA_RO | DDCA_COMPLEX_NC,
      .v20_name = "Display controller type",
   },
   {  .code=0xc9,
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_CONTROL,    // 2.: MISC, 3.0: CONTROL
      .nontable_formatter=format_feature_detail_xc9_xdf_version,
      .desc = "2 byte firmware level",
      .v20_flags = DDCA_RO | DDCA_COMPLEX_NC,
      .v20_name = "Display firmware level",
   },
   {  .code=0xca,
      // Says the v2.2 spec: A new feature added to V3.0 and expanded in V2.2
      // BUT: xCA is present in 2.0 spec, defined identically to 3.0 spec
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_CONTROL,   // 2.0: MISC, 3.0: CONTROL
      .default_sl_values=xca_osd_values,  // tables specified in nontable_formatter
      .v22_sl_values=xca_v22_osd_button_sl_values,
      // .desc = "Indicates whether On Screen Display is enabled",
      .desc = "Sets and indicates the current operational state of OSD (and buttons in v2.2)",
      .v20_flags = DDCA_RW | DDCA_SIMPLE_NC,
      .v20_name = "OSD",
      .v22_flags = DDCA_RW | DDCA_COMPLEX_NC,
      // for v3.0:
      .nontable_formatter = format_feature_detail_xca_osd_button_control,
      .v22_name = "OSD/Button Control"
   },
   {  .code=0xcc,
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_CONTROL,   // 2.0: MISC, 3.0: CONTROL
      .default_sl_values=xcc_osd_language_values,
      .desc = "On Screen Display language",
      .v20_flags  = DDCA_RW | DDCA_SIMPLE_NC,
      .v20_name = "OSD Language",
   },
   {  .code=0xcd,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 3.0: MISC
      // not in 2.0, is in 3.0, assume exists in 2.1
      .desc = "Control up to 16 LED (or similar) indicators to indicate system status",
      .nontable_formatter = format_feature_detail_debug_sl_sh,
      .v21_flags = DDCA_RW | DDCA_COMPLEX_NC,
      .v21_name = "Status Indicators",
   },
   {  .code=0xce,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .desc = "Rows and characters/row of auxiliary display",
      .v20_flags  = DDCA_RO | DDCA_COMPLEX_NC,
      .v20_name = "Auxiliary display size",
      .nontable_formatter =  format_feature_detail_xce_aux_display_size,
   },
   {  .code=0xcf,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .desc = "Sets contents of auxiliary display device",
      .v20_flags  = DDCA_WO | DDCA_WO_TABLE,
      .v20_name = "Auxiliary display data",
   },
   {  .code=0xd0,
      .vcp_spec_groups = VCP_SPEC_MISC,
      .desc = "Selects the active output",
      .v20_name = "Output select",
      .v20_flags = DDCA_RW | DDCA_SIMPLE_NC,
      .default_sl_values = xd0_v2_output_select_values,
      .table_formatter = default_table_feature_detail_function,  // TODO: implement proper function
      .v30_flags = DDCA_RW | DDCA_NORMAL_TABLE,
      .v22_flags = DDCA_RW | DDCA_SIMPLE_NC,
   },
   {  .code=0xd2,
      // exists in 3.0, not in 2.0, assume exists in 2.1
      .vcp_spec_groups = VCP_SPEC_MISC,
      .desc = "Read an Asset Tag to/from the display",
      .v21_name = "Asset Tag",
      .v21_flags = DDCA_RW | DDCA_NORMAL_TABLE,
      .table_formatter = default_table_feature_detail_function,
   },
   {  .code=0xd4,
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_IMAGE,        // 2.0: MISC, 3.0: IMAGE
      .desc="Stereo video mode",
      .v20_name = "Stereo video mode",
      .v20_flags = DDCA_RW | DDCA_COMPLEX_NC,
      .nontable_formatter = format_feature_detail_sl_byte,     // TODO: implement proper function
   },
   {  .code=0xd6,                           // DONE
      .vcp_spec_groups = VCP_SPEC_MISC | VCP_SPEC_CONTROL,   // 2.0: MISC, 3.0: CONTROL
      .default_sl_values = xd6_power_mode_values,
      .desc = "DPM and DPMS status",
      .v20_flags = DDCA_RW | DDCA_SIMPLE_NC,
      .v20_name = "Power mode",
   },
   {  .code=0xd7,                          // DONE - identical in 2.0, 3.0, 2.2
      .vcp_spec_groups = VCP_SPEC_MISC,    // 2.0, 3.0, 2.2
      .default_sl_values = xd7_aux_power_output_values,
      .desc="Controls an auxiliary power output from a display to a host device",
      .v20_flags = DDCA_RW | DDCA_SIMPLE_NC,
      .v20_name = "Auxiliary power output",
   },
   {  .code=0xda,                                                   // DONE
      .vcp_spec_groups = VCP_SPEC_GEOMETRY | VCP_SPEC_IMAGE,         // 2.0: IMAGE, 3.0: GEOMETRY
      .vcp_subsets = VCP_SUBSET_CRT,
      .desc = "Controls scan characteristics (aka format)",
      .default_sl_values = xda_scan_mode_values,
      .v20_flags = DDCA_RW | DDCA_SIMPLE_NC,
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
      .v21_flags = DDCA_RW | DDCA_SIMPLE_NC,
      .v21_name  = "Image Mode",
   },
   {  .code=0xdc,
      .vcp_spec_groups = VCP_SPEC_IMAGE,
      // defined in 2.0, 3.0 has different name, more values
      //.name="Display application",
      .default_sl_values=xdc_display_application_values,
      .desc="Type of application used on display",  // my desc
      .vcp_subsets = VCP_SUBSET_COLOR,         // observed on U32H750 to interact with color settings
      .v20_flags = DDCA_RW | DDCA_SIMPLE_NC,
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
      .v20_flags = DDCA_WO | DDCA_WO_NC,
      .v20_name  = "Operation Mode",
      .v21_flags = DDCA_RW | DDCA_COMPLEX_NC,
      .v21_name  = "Scratch Pad",
   },
   {  .code=0xdf,
      .vcp_spec_groups = VCP_SPEC_MISC,   // 2.0
      .nontable_formatter=format_feature_detail_xc9_xdf_version,
      .desc = "MCCS version",
      .v20_flags = DDCA_RO | DDCA_COMPLEX_NC,
      .v20_name  = "VCP Version",
   }
};
// #pragma GCC diagnostic pop
int vcp_feature_code_count = sizeof(vcp_code_table)/sizeof(VCP_Feature_Table_Entry);


#ifdef DEVELOPMENT_ONLY
//
// Functions for validating vcp_code_table[]
//
// Intended for use only during development
//

int check_one_version_flags(
      DDCA_Version_Feature_Flags     vflags,
      char *                    which_flags,
      VCP_Feature_Table_Entry * pentry)
{
   int ct = 0;
   if (vflags && !(vflags & DDCA_DEPRECATED))  {
      if (vflags & DDCA_STD_CONT)     ct++;
      if (vflags & DDCA_COMPLEX_CONT) ct++;
      if (vflags & DDCA_SIMPLE_NC)    ct++;
      if (vflags & DDCA_COMPLEX_NC)   ct++;
      if (vflags & DDCA_WO_NC)        ct++;
      if (vflags & DDCA_NORMAL_TABLE)        ct++;
      if (vflags & DDCA_WO_TABLE)     ct++;
      if (ct != 1) {
          fprintf(
             stderr,
             "code: 0x%02x, exactly 1 of VCP2_STD_CONT, VCP2_COMPLEX_CONT, VCP2_SIMPLE_NC, "
             "VCP2_COMPLEX_NC, VCP2_TABLE, VCP2_WO_TABLE must be set in %s\n",
             pentry->code, which_flags);
          ct = -1;
       }


      if (vflags & DDCA_SIMPLE_NC) {
         if (!pentry->default_sl_values) {
            fprintf(
               stderr,
               "code: 0x%02x, .flags: %s, VCP2_SIMPLE_NC set but .default_sl_values == NULL\n",
               pentry->code, which_flags);
            ct = -2;
         }
      }
      else

      if (vflags & DDCA_COMPLEX_NC) {
         if (!pentry->nontable_formatter) {
            fprintf(
               stderr,
               "code: 0x%02x, .flags: %s, VCP2_COMPLEX_NC set but .nontable_formatter == NULL\n",
               pentry->code, which_flags);
            ct = -2;
         }
      }
      else if (vflags & DDCA_COMPLEX_CONT) {
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
 * exactly 1 of DDCA_RO, DDCA_WO, DDCA_RW is set
 *
 * Returns: 1 of a value is set, 0 if no value set, -1 if
 *          more than 1 value set
 */
int check_version_rw_flags(
      DDCA_Version_Feature_Flags vflags,
      char * which_flags,
      VCP_Feature_Table_Entry * entry)
{
   int ct = 0;
   if (vflags && !(vflags & DDCA_DEPRECATED))  {
        if (vflags & DDCA_RO) ct++;
        if (vflags & DDCA_WO) ct++;
        if (vflags & DDCA_RW) ct++;
        if (ct != 1) {
           fprintf(stderr,
                   "code: 0x%02x, exactly 1 of DDCA_RO, DDCA_WO, DDCA_RW must be set in non-zero %s_flags\n",
                   entry->code, which_flags);
           ct = -1;
        }
   }
   return ct;
}


void validate_vcp_feature_table() {
   bool debug = false;
   DBGMSF(debug, "Starting");
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
      // assert(false);
      PROGRAM_LOGIC_ERROR(NULL);
}

// End of functions for validating vcp_code_table

#endif

#ifdef REF
typedef
struct {
   char                                  marker[4];
   Byte                                  code;
   char *                                desc;
   Format_Normal_Feature_Detail_Function nontable_formatter;
   Format_Table_Feature_Detail_Function  table_formatter;
   DDCA_Global_Feature_Flags             vcp_global_flags;
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
#endif


/** Output a debug report for a specified #VCP_Feature_Table_Entry.
 *
 *  @param pfte  feature table entry
 *  @param depth logical indentation depth
 */
void dbgrpt_vcp_entry(VCP_Feature_Table_Entry * pfte, int depth) {
   rpt_vstring(depth, "VCP_Feature_Table_Entry at %p:", pfte);
   // show_backtrace(2);
   assert(pfte && memcmp(pfte->marker, VCP_FEATURE_TABLE_ENTRY_MARKER, 4) == 0);
   int d1 = depth+1;
   const int bufsz = 100;
   char buf[bufsz];

   rpt_vstring(d1, "code:       0x%02x", pfte->code);
   rpt_vstring(d1, "desc:       %s", pfte->desc);
   rpt_vstring(d1, "nontable_formatter: %p %s", pfte->nontable_formatter,
                                                rtti_get_func_name_by_addr(pfte->nontable_formatter));
   rpt_vstring(d1, "table_formatter:    %p %s", pfte->table_formatter,
                                                rtti_get_func_name_by_addr(pfte->table_formatter));
   rpt_vstring(d1, "vcp_global_flags:   0x%02x - %s",
                   pfte->vcp_global_flags,
                   vcp_interpret_global_feature_flags(pfte->vcp_global_flags, buf, bufsz));
   rpt_vstring(d1, "vcp_spec_groups:   0x%04x - %s",
                   pfte->vcp_spec_groups,
                   spec_group_names_r(pfte, buf, bufsz));
   rpt_vstring(d1, "vcp_subsets:   0x%04x - %s",
                   pfte->vcp_subsets,
                   feature_subset_names(pfte->vcp_subsets));
   rpt_vstring(d1, "v20_name:          %s", pfte->v20_name);
   rpt_vstring(d1, "v21_name:          %s", pfte->v21_name);
   rpt_vstring(d1, "v30_name:          %s", pfte->v30_name);
   rpt_vstring(d1, "v22_name:          %s", pfte->v22_name);
//   rpt_vstring(d1, "v20_flags:         0x%04x - %s",
//                   pfte->v20_flags,
//                   vcp_interpret_version_feature_flags(pfte->v20_flags, buf, bufsz));
   rpt_vstring(d1, "v20_flags:         0x%04x - %s",
                   pfte->v20_flags,
                   interpret_feature_flags_t(pfte->v20_flags));
//   rpt_vstring(d1, "v21_flags:         0x%04x - %s",
//                   pfte->v21_flags,
//                   vcp_interpret_version_feature_flags(pfte->v21_flags, buf, bufsz));
   rpt_vstring(d1, "v21_flags:         0x%04x - %s",
                   pfte->v21_flags,
                   interpret_feature_flags_t(pfte->v21_flags));
//   rpt_vstring(d1, "v30_flags:         0x%04x - %s",
//                   pfte->v30_flags,
//                   vcp_interpret_version_feature_flags(pfte->v30_flags, buf, bufsz));
   rpt_vstring(d1, "v30_flags:         0x%04x - %s",
                   pfte->v30_flags,
                   interpret_feature_flags_t(pfte->v30_flags));
//   rpt_vstring(d1, "v22_flags:         0x%04x - %s",
//                   pfte->v22_flags,
//                   vcp_interpret_version_feature_flags(pfte->v22_flags, buf, bufsz));
   rpt_vstring(d1, "v22_flags:         0x%04x - %s",
                   pfte->v22_flags,
                   interpret_feature_flags_t(pfte->v22_flags));
   dbgrpt_sl_value_table(pfte->default_sl_values, "default_sl_values", d1);
   dbgrpt_sl_value_table(pfte->v21_sl_values, "v21_sl_values", d1);
   dbgrpt_sl_value_table(pfte->v30_sl_values, "v30_sl_values", d1);
   dbgrpt_sl_value_table(pfte->v22_sl_values, "v22_sl_values", d1);
}


static void init_func_name_table() {
   RTTI_ADD_FUNC(vcp_format_nontable_feature_detail);
   RTTI_ADD_FUNC(vcp_format_table_feature_detail);
   RTTI_ADD_FUNC(vcp_format_feature_detail);
   RTTI_ADD_FUNC(default_table_feature_detail_function);
   RTTI_ADD_FUNC(format_feature_detail_x73_lut_size);
   RTTI_ADD_FUNC(format_feature_detail_debug_sl_sh);
   RTTI_ADD_FUNC(format_feature_detail_debug_continuous);
   RTTI_ADD_FUNC(format_feature_detail_debug_bytes );
   RTTI_ADD_FUNC(format_feature_detail_sl_byte);
   RTTI_ADD_FUNC(format_feature_detail_sl_lookup);
   RTTI_ADD_FUNC(format_feature_detail_standard_continuous);
   RTTI_ADD_FUNC(format_feature_detail_ushort);
   RTTI_ADD_FUNC(format_feature_detail_x02_new_control_value);
   RTTI_ADD_FUNC(format_feature_detail_x0b_color_temperature_increment);
   RTTI_ADD_FUNC(format_feature_detail_x0c_color_temperature_request);
   RTTI_ADD_FUNC(format_feature_detail_x14_select_color_preset);
   RTTI_ADD_FUNC(format_feature_detail_x62_audio_speaker_volume);
   RTTI_ADD_FUNC(format_feature_detail_x8d_mute_audio_blank_screen);
   RTTI_ADD_FUNC(format_feature_detail_x8f_x91_audio_treble_bass);
   RTTI_ADD_FUNC(format_feature_detail_x93_audio_balance);
   RTTI_ADD_FUNC(format_feature_detail_xac_horizontal_frequency);
   RTTI_ADD_FUNC(format_feature_detail_6_axis_hue);
   RTTI_ADD_FUNC(format_feature_detail_xae_vertical_frequency);
   RTTI_ADD_FUNC(format_feature_detail_xbe_link_control);
   RTTI_ADD_FUNC(format_feature_detail_xc0_display_usage_time);
   RTTI_ADD_FUNC(format_feature_detail_xca_osd_button_control);
   RTTI_ADD_FUNC(format_feature_detail_x6c_application_enable_key);
   RTTI_ADD_FUNC(format_feature_detail_xc8_display_controller_type);
   RTTI_ADD_FUNC(format_feature_detail_xc9_xdf_version);
}


/** Initialize the vcp_feature_codes module.
 *  Must be called before any other function in this file.
 */
void init_vcp_feature_codes() {
#ifdef DEVELOPMENT_ONLY
   validate_vcp_feature_table();  // enable for development
#endif
   for (int ndx=0; ndx < vcp_feature_code_count; ndx++) {
      memcpy( vcp_code_table[ndx].marker, VCP_FEATURE_TABLE_ENTRY_MARKER, 4);
   }
   init_func_name_table();
   // dbgrpt_func_name_table(0);
   vcp_feature_codes_initialized = true;
}

