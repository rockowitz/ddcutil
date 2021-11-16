/** @file dyn_feature_codes.c
 *
 * Access VCP feature code descriptions at the DDC level in order to
 * incorporate user-defined per-monitor feature information.
 */

// Copyright (C) 2014-2020Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <string.h>

#include "util/report_util.h"
/** \endcond */

#include "base/displays.h"
#include "base/dynamic_features.h"
#include "base/feature_metadata.h"
#include "base/monitor_model_key.h"
#include "base/rtti.h"

#include "vcp/vcp_feature_codes.h"

#include "dynvcp/dyn_feature_codes.h"
#include "dynvcp/dyn_feature_files.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_UDF;

/* Formats the name of a non-continuous feature whose value is returned in byte SL.
 *
 * Arguments:
 *    code_info   parsed feature data
 *    value_table lookup table, if NULL, create generic name
 *    buffer      buffer in which to store output
 *    bufsz       buffer size
 *
 * Returns:
 *    true if formatting successful, false if not
 */
bool dyn_format_feature_detail_sl_lookup(
        Nontable_Vcp_Value *       code_info,
        DDCA_Feature_Value_Entry * value_table,
        char *                     buffer,
        int                        bufsz)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   if (value_table) {
      char * s = sl_value_table_lookup(value_table, code_info->sl);
      if (!s)
         s = "Unrecognized value";
      snprintf(buffer, bufsz,"%s (sl=0x%02x)", s, code_info->sl);
   }
   else
      snprintf(buffer, bufsz, "0x%02x", code_info->sl);

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning true..  *buffer=|%s|", buffer);
   return true;
}


/** Returns a #Display_Feature_Metadata record for a specified feature, first
 *  checking for a user supplied feature definition, and then from the internal
 *  feature definition tables.
 *
 * @param  feature_code   feature code
 * @param  dfr            if non-NULL, points to Dynamic_Features_Record for the display
 * @param  vspec          VCP version of the display
 * @param  with_default   create default value if not found
 * @return Display_Feature_Metadata for the feature, caller must free,
 *         NULL if feature not found either in the user supplied feature definitions
 *         (Dynamic_Features_Record) or in the internal feature definitions
 */
Display_Feature_Metadata *
dyn_get_feature_metadata_by_dfr_and_vspec_dfm(
     DDCA_Vcp_Feature_Code    feature_code,
     Dynamic_Features_Rec *   dfr,
     DDCA_MCCS_Version_Spec   vspec,
     bool                     with_default)
{
    bool debug = false;
    DBGTRC_STARTING(debug, TRACE_GROUP,
                  "feature_code=0x%02x, dfr=%p, vspec=%d.%d, with_default=%s",
                  feature_code, dfr, vspec.major, vspec.minor, sbool(with_default));

    Display_Feature_Metadata * result = NULL;

    if (dfr) {
       DDCA_Feature_Metadata * dfr_metadata = get_dynamic_feature_metadata(dfr, feature_code);
       if (dfr_metadata) {
          result = dfm_from_ddca_feature_metadata(dfr_metadata);
          result->vcp_version = vspec;    // ??

          if (dfr_metadata->feature_flags & DDCA_SIMPLE_NC) {
             if (dfr_metadata->sl_values)
                result->nontable_formatter_sl = dyn_format_feature_detail_sl_lookup;  // HACK
             else
                result->nontable_formatter = format_feature_detail_sl_byte;
          }
          else if (dfr_metadata->feature_flags & DDCA_STD_CONT)
             result->nontable_formatter = format_feature_detail_standard_continuous;
          else if (dfr_metadata->feature_flags & DDCA_TABLE)
             result->table_formatter = default_table_feature_detail_function;
          else
             result->nontable_formatter = format_feature_detail_debug_bytes;
       }
    }

    if (!result) {
       // returns dref->vcp_version if already cached, queries monitor if not
       // DBGMSG("vspec=%d.%d", vspec.major, vspec.minor);

        VCP_Feature_Table_Entry * pentry =
               (with_default) ? vcp_find_feature_by_hexid_w_default(feature_code)
                              : vcp_find_feature_by_hexid(feature_code);
         if (pentry) {
            result = extract_version_feature_info_from_feature_table_entry(pentry, vspec, /*version_sensitive*/ true);
            if (debug)
               dbgrpt_vcp_entry(pentry, 2);

            if (result->feature_flags & DDCA_TABLE) {
               if (pentry->table_formatter)
                  result->table_formatter = pentry->table_formatter;
               else {
                  if (result->feature_flags & DDCA_NORMAL_TABLE) {
                     result->table_formatter = default_table_feature_detail_function;
                  }
                  else if (result->feature_flags & DDCA_WO_TABLE) {
                     // program logic error?
                     result->table_formatter = NULL;
                  }
                  else {
                     PROGRAM_LOGIC_ERROR("Neither DDCA_NORMAL_TABLE or DDCA_WO_TABLE  set in meta->feature_flags");
                  }
               }
            }
            else if (result->feature_flags & DDCA_NON_TABLE)  {
               if (result->feature_flags & DDCA_STD_CONT) {
                  result->nontable_formatter = format_feature_detail_standard_continuous;
                  // DBGMSG("DDCA_STD_CONT");
               }
               else if (result->feature_flags & DDCA_SIMPLE_NC) {
                  if (result->sl_values) {
                     // DBGMSG("format_feature_detail_sl_lookup");
                     result->nontable_formatter = format_feature_detail_sl_lookup;
                  }
                  else {
                     //  DBGMSG("format_feature_detail_sl_byte");
                     result->nontable_formatter = format_feature_detail_sl_byte;
                  }
               }
               else if (result->feature_flags & DDCA_WO_NC) {
                  result->nontable_formatter = NULL;      // but should never be called for this case
               }

               else {
                  assert(result->feature_flags & (DDCA_COMPLEX_CONT | DDCA_COMPLEX_NC | DDCA_NC_CONT));
                  if (pentry->nontable_formatter)
                     result->nontable_formatter = pentry->nontable_formatter;
                  else
                     result->nontable_formatter = format_feature_detail_debug_bytes;
               }
            }  // DDCA_NON_TABLE

            else {
               assert (result->feature_flags & DDCA_DEPRECATED);
               result->nontable_formatter = format_feature_detail_debug_bytes;   // ??
            }

            if (pentry->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY)
               free_synthetic_vcp_entry(pentry);
         }
    }

    DBGTRC_RET_STRUCT(debug, TRACE_GROUP, Display_Feature_Metadata, dbgrpt_display_feature_metadata, result);
    return result;
}


/** Returns a #Dynamic_Feature_Metadata record for a specified feature, first
 *  checking for a user supplied feature definition using the specified
 *  #DDCA_Monitor_Model_Key, and then from the internal feature definition tables.
 *
 * @param  feature_code   feature code
 * @param  mmk            monitor model key
 * @param  vspec          VCP version of the display
 * @param  with_default   create default value if not found
 * @return Display_Feature_Metadata for the feature, caller must free,
 *         NULL if feature not found either in the user supplied feature definitions
 *         (Dynamic_Features_Record) or in the internal feature definitions
 *
 * @remark
 * Ensures user supplied features have been loaded by calling #dfr_load_by_mmk()
 */
Display_Feature_Metadata *
dyn_get_feature_metadata_by_mmk_and_vspec(
     DDCA_Vcp_Feature_Code    feature_code,
     DDCA_Monitor_Model_Key   mmk,
     DDCA_MCCS_Version_Spec   vspec,
     bool                     with_default)
{
    bool debug = false;
    DBGTRC_STARTING(debug, TRACE_GROUP,
                  "feature_code=0x%02x, mmk=%s, vspec=%d.%d, with_default=%s",
                  feature_code, mmk_repr(mmk), vspec.major, vspec.minor, sbool(with_default));

    Dynamic_Features_Rec * dfr = NULL;
    Error_Info * erec = dfr_load_by_mmk(mmk, &dfr);
    if (erec) {
       if (erec->status_code != DDCRC_NOT_FOUND || debug)
          errinfo_report(erec,1);
       errinfo_free(erec);
    }

    Display_Feature_Metadata * result =
          dyn_get_feature_metadata_by_dfr_and_vspec_dfm(feature_code, dfr, vspec, with_default);

    if (dfr)
       dfr_free(dfr);

    if (debug || IS_TRACING()) {
       DBGTRC_DONE(debug, TRACE_GROUP, "Returning Display_Feature_Metadata at %p", result);
       if (result)
          dbgrpt_display_feature_metadata(result, 1);
    }

    return result;
 }


/** Returns a #Display_Feature_Metadata record for a specified feature, first
 *  checking for a user supplied feature definition, and then from the internal
 *  feature definition tables.
 *
 * @param  feature_code   feature code
 * @param  dref           display reference
 * @param  with_default   create default value if not found
 * @return Display_Feature_Metadata for the feature, caller must free,
 *         NULL if feature not found either in the user supplied feature definitions
 *         (Dynamic_Features_Record) or in the internal feature definitions
 */
Display_Feature_Metadata *
dyn_get_feature_metadata_by_dref(
      DDCA_Vcp_Feature_Code feature_code,
      Display_Ref *         dref,
      bool                  with_default)
{
   bool debug = false;
   if (debug  || IS_TRACING()) {
      DBGMSG("Starting. feature_code=0x%02x, dref=%s, with_default=%s",
                 feature_code, dref_repr_t(dref), sbool(with_default));
      DBGMSG("dref->dfr=%p", dref->dfr);
      DBGMSG("DREF_OPEN: %s", sbool(dref->flags & DREF_OPEN));
   }

   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_dref(dref);

   Display_Feature_Metadata * result =
         dyn_get_feature_metadata_by_dfr_and_vspec_dfm(feature_code, dref->dfr, vspec, with_default);
   if (result)
      result->display_ref = dref;

   DBG_RET_STRUCT(debug || IS_TRACING(), Display_Feature_Metadata, dbgrpt_display_feature_metadata, result);
   return result;
}


/** Returns a #Display_Feature_Metadata record for a specified feature, first
 *  checking for a user supplied feature definition, and then from the internal
 *  feature definition tables.
 *
 * @param  feature_code   feature code
 * @param  dh             display handle
 * @param  with_default   create default value if not found
 * @return Display_Feature_Metadata for the feature, caller must free,
 *         NULL if feature not found either in the user supplied feature definitions
 *         (Dynamic_Features_Record) or in the internal feature definitions
 */
Display_Feature_Metadata *
dyn_get_feature_metadata_by_dh(
      DDCA_Vcp_Feature_Code id,
      Display_Handle *      dh,
      bool                  with_default)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
                 "id=0x%02x, dh=%s, with_default=%s",
                 id, dh_repr_t(dh), sbool(with_default) );

   // ensure dh->dref->vcp_version set without incurring additional open/close
   DDCA_MCCS_Version_Spec vspec =
   get_vcp_version_by_dh(dh);
   // Display_Feature_Metadata * result = dyn_get_feature_metadata_by_dref_dfm(id, dh->dref, with_default);
   Display_Feature_Metadata * result =
         dyn_get_feature_metadata_by_dfr_and_vspec_dfm(id, dh->dref->dfr, vspec, with_default);
   if (result)
      result->display_ref = dh->dref;    // needed?

   if (debug || IS_TRACING()) {
      DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %p", result);
      dbgrpt_display_feature_metadata(result, 2);
   }
   return result;
}


// Functions that apply formatting

bool
dyn_format_nontable_feature_detail(
        Display_Feature_Metadata * dfm,
        // DDCA_MCCS_Version_Spec     vcp_version,
        Nontable_Vcp_Value *       code_info,
        char *                     buffer,
        int                        bufsz)
{
   bool debug = false;
   DDCA_MCCS_Version_Spec vcp_version = dfm->vcp_version;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Code=0x%02x, vcp_version=%d.%d",
                              dfm->feature_code, vcp_version.major, vcp_version.minor);

   // assert(vcp_version_eq(dfm->vcp_version, vcp_version));   // check before eliminating vcp_version parm

   bool ok = false;
   buffer[0] = '\0';
   if (dfm->nontable_formatter) {
      Format_Normal_Feature_Detail_Function ffd_func = dfm->nontable_formatter;
        // get_nontable_feature_detail_function(vfte, vcp_version);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
            "Using normal feature detail function: %s", rtti_get_func_name_by_addr(ffd_func) );
      ok = ffd_func(code_info, vcp_version,  buffer, bufsz);
   }
   else if (dfm->nontable_formatter_sl) {
      Format_Normal_Feature_Detail_Function2 ffd_func = dfm->nontable_formatter_sl;
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
            "Using SL lookup feature detail function: %s", rtti_get_func_name_by_addr(ffd_func) );
      ok = ffd_func(code_info, dfm->sl_values, buffer, bufsz);
   }
   else
      PROGRAM_LOGIC_ERROR("Neither nontable_formatter nor vcp_nontable_formatter set");

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, ok, "buffer=|%s|", buffer);
   return ok;
}


bool
dyn_format_table_feature_detail(
       Display_Feature_Metadata *  dfm,
       // DDCA_MCCS_Version_Spec     vcp_version,
       Buffer *                   accumulated_value,
       char * *                   aformatted_data
     )
{
   DDCA_MCCS_Version_Spec     vcp_version = dfm->vcp_version;
   Format_Table_Feature_Detail_Function ffd_func = dfm->table_formatter;
      //   get_table_feature_detail_function(vfte, vcp_version);
   bool ok = ffd_func(accumulated_value, vcp_version, aformatted_data);
   return ok;
}


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
dyn_format_feature_detail(
       Display_Feature_Metadata * dfm,
       DDCA_MCCS_Version_Spec    vcp_version,
       DDCA_Any_Vcp_Value *      valrec,
       char * *                  aformatted_data
     )
{
   bool debug = false;
   if (debug || IS_TRACING() ) {
      DBGTRC_STARTING(debug, TRACE_GROUP, "valrec: ");
      dbgrpt_single_vcp_value(valrec, 2);
   }

   bool ok = true;
   *aformatted_data = NULL;

   // DBGTRC(debug, TRACE_GROUP, "valrec->value_type = %d", valrec->value_type);
   if (valrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "DDCA_NON_TABLE_VCP_VALUE");
      Nontable_Vcp_Value* nontable_value = single_vcp_value_to_nontable_vcp_value(valrec);
      char workbuf[200];
      ok = dyn_format_nontable_feature_detail(
              dfm,
              // vcp_version,
              nontable_value,
              workbuf,
              200);
      free(nontable_value);
      if (ok) {
         *aformatted_data = strdup(workbuf);
      }
   }
   else {        // TABLE_VCP_CALL
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "DDCA_TABLE_VCP_VALUE");
      ok = dyn_format_table_feature_detail(
            dfm,
            // cp_version,
            buffer_new_with_value(valrec->val.t.bytes, valrec->val.t.bytect, __func__),
            aformatted_data);
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, ok, "*aformatted_data=%s", *aformatted_data);
   assert( (ok && *aformatted_data) || (!ok && !*aformatted_data) );
   return ok;
}


char *
dyn_get_feature_name(
      Byte            feature_code,
      Display_Ref *   dref)
{
   bool debug = false;
   DBGMSF(debug, "feature_code=0x%02x, dref=%s", feature_code, dref_repr_t(dref));

   char * result = NULL;
   if (dref) {
      DBGMSF(debug, "dref->dfr=%s", dfr_repr_t(dref->dfr));
      if (dref->dfr) {
         DDCA_Feature_Metadata * dfr_metadata = get_dynamic_feature_metadata(dref->dfr, feature_code);
         if (dfr_metadata)
            result = dfr_metadata->feature_name;
      }
      if (!result) {
         DDCA_MCCS_Version_Spec //  vspec = dref->vcp_version;   // TODO use function call in case not set
         vspec = get_vcp_version_by_dref(dref);
         result = get_feature_name_by_id_and_vcp_version(feature_code, vspec);
      }
   }
   else {
      result = get_feature_name_by_id_only(feature_code);
   }

   DBGMSF(debug, "Done. Returning: %s", result);
   return result;
}


void init_dyn_feature_codes() {
   RTTI_ADD_FUNC(dyn_format_nontable_feature_detail);
   RTTI_ADD_FUNC(dyn_get_feature_metadata_by_dfr_and_vspec_dfm);
   RTTI_ADD_FUNC(dyn_get_feature_metadata_by_mmk_and_vspec);
   RTTI_ADD_FUNC(dyn_get_feature_metadata_by_dref);
   RTTI_ADD_FUNC(dyn_get_feature_metadata_by_dh);
   RTTI_ADD_FUNC(dyn_format_feature_detail);
   RTTI_ADD_FUNC(dyn_format_feature_detail_sl_lookup);
   // dbgrpt_func_name_table(0);
}

