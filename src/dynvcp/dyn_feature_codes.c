/** @file dyn_feature_codes.c
 *
 * Access VCP feature code descriptions at the DDC level in order to
 * incorporate user-defined per-monitor feature information.
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>

#include "util/report_util.h"

#include "base/displays.h"
#include "base/dynamic_features.h"
#include "base/monitor_model_key.h"
#include "base/rtti.h"

#include "vcp/vcp_feature_codes.h"

#include "dynvcp/dyn_dynamic_features.h"

#include "dynvcp/dyn_feature_codes.h"


#ifdef IFM
void
dbgrpt_internal_feature_metadata(
      Internal_Feature_Metadata * intmeta,
      int                         depth)
{
   assert(intmeta);
   int d1 = depth+1;
   rpt_vstring(depth, "Internal_Feature_Metadata at %p", intmeta);
   if (intmeta->external_metadata)
      dbgrpt_ddca_feature_metadata(intmeta->external_metadata, d1);
   else
      rpt_vstring(d1, "intmeta->external_metadata == NULL !!!");
   rpt_vstring(d1, "nontable_formatter:     %p  %s", intmeta->nontable_formatter,     get_func_name_by_addr(intmeta->nontable_formatter));
   rpt_vstring(d1, "vcp_nontable_formatter: %p  %s", intmeta->vcp_nontable_formatter, get_func_name_by_addr(intmeta->vcp_nontable_formatter));
   rpt_vstring(d1, "table_formatter:        %p  %s", intmeta->table_formatter,        get_func_name_by_addr(intmeta->table_formatter));
}
#endif

#ifdef DVFI
void
version_feature_info_to_metadata(
      DDCA_Version_Feature_Info * info,
      DDCA_Feature_Metadata * meta)
{
   assert(info);
   assert(meta);
   assert( memcmp(meta->marker, DDCA_FEATURE_METADATA_MARKER, 4) == 0);
   meta->feature_code = info->feature_code;
   meta->feature_desc = strdup(info->desc);
   meta->feature_flags = info->feature_flags;
   meta->feature_name = strdup(info->feature_name);
   meta->sl_values = info->sl_values;    // need to copy?

}
#endif


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
   DBGMSF(debug, "Starting.");

   if (value_table) {
      char * s = vcp_get_feature_value_name(value_table, code_info->sl);
      if (!s)
         s = "Unrecognized value";
      snprintf(buffer, bufsz,"%s (sl=0x%02x)", s, code_info->sl);
   }
   else
      snprintf(buffer, bufsz, "0x%02x", code_info->sl);

   DBGMSF(debug, "Done.  *buffer=|%s|. Returning true", buffer);
   return true;
}



#ifdef IFM
Internal_Feature_Metadata *
dyn_get_feature_metadata_by_dfr_and_vspec(
     DDCA_Vcp_Feature_Code    feature_code,
     Dynamic_Features_Rec *   dfr,
     DDCA_MCCS_Version_Spec   vspec,
     bool                     with_default)

{
   bool debug = true;
    DBGMSF(debug, "Starting. feature_code=0x%02x, dfr=%p, vspec=%d.%d, with_default=%s",
                  feature_code, dfr, vspec.major, vspec.minor, sbool(with_default));

    Internal_Feature_Metadata * result = NULL;

    if (dfr) {
       DDCA_Feature_Metadata * dfr_metadata = get_dynamic_feature_metadata(dfr, feature_code);
       if (dfr_metadata) {
          result = calloc(1, sizeof(Internal_Feature_Metadata));
          result->external_metadata = dfr_metadata;
          if (dfr_metadata->feature_flags & DDCA_SIMPLE_NC) {
             if (dfr_metadata->sl_values)
                result->vcp_nontable_formatter = dyn_format_feature_detail_sl_lookup;  // HACK
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
            DDCA_Version_Feature_Info * info = extract_version_feature_info(pentry, vspec, /*version_sensitive*/ true);
            if (debug)
               dbgrpt_vcp_entry(pentry, 2);

            // old Display_Feature_Metadata makes conversion unnecessary
            DDCA_Feature_Metadata * meta = calloc(1, sizeof(DDCA_Feature_Metadata));
            memcpy(meta->marker, DDCA_FEATURE_METADATA_MARKER, 4);
            version_feature_info_to_metadata(info, meta);
            free_version_feature_info(info);  // safe?
            result = calloc(1, sizeof(Internal_Feature_Metadata));
            result->external_metadata = meta;
 #ifdef REF
            // Further refine the C/NC/TABLE categorization of the MCCS spec
            // Exactly 1 of the following 8 bits is set
            #define DDCA_STD_CONT     0x0080       /**< Normal continuous feature */
            #define DDCA_COMPLEX_CONT 0x0040       /**< Continuous feature with special interpretation */
            #define DDCA_SIMPLE_NC    0x0020       /**< Non-continuous feature, having a defined list of values in byte SL */
            #define DDCA_COMPLEX_NC   0x0010       /**< Non-continuous feature, having a complex interpretation using one or more of SL, SH, ML, MH */
            #define DDCA_NC_CONT      0x0800       /**< NC feature combining reserved values with continuous range */
            // For WO NC features.  There's no interpretation function or lookup table
            // Used to mark that the feature is defined for a version
            #define DDCA_WO_NC        0x0008       /**< Used internally for write-only non-continuous features */
            #define DDCA_NORMAL_TABLE 0x0004       /**< Normal RW table type feature */
            #define DDCA_WO_TABLE     0x0002       /**< Write only table feature */
 #endif
            if (meta->feature_flags & DDCA_TABLE) {
               if (pentry->table_formatter)
                  result->table_formatter = pentry->table_formatter;
               else {
                  if (meta->feature_flags & DDCA_NORMAL_TABLE) {
                     result->table_formatter = default_table_feature_detail_function;
                  }
                  else if (meta->feature_flags & DDCA_WO_TABLE) {
                     // program logic error?
                     result->table_formatter = NULL;
                  }
                  else {
                     PROGRAM_LOGIC_ERROR("Neither DDCA_NORMAL_TABLE or DDCA_WO_TABLE  set in meta->feature_flags");
                  }
               }
            }
            else {
               if (pentry->nontable_formatter)
                  result->nontable_formatter = pentry->nontable_formatter;
               else {
                  if (meta->feature_flags & DDCA_STD_CONT) {
                     result->nontable_formatter = format_feature_detail_standard_continuous;
                  }
                  else if (meta->feature_flags & DDCA_COMPLEX_CONT) {
                     result->nontable_formatter = format_feature_detail_debug_bytes;
                  }
                  else if (meta->feature_flags & DDCA_SIMPLE_NC) {
                     if (meta->sl_values) {
                        DBGMSG("format_feature_detail_sl_lookup");
                        result->nontable_formatter = format_feature_detail_sl_lookup;
                     }
                     else {
                        DBGMSG("format_feature_detail_sl_byte");
                        result->nontable_formatter = format_feature_detail_sl_byte;
                     }
                  }
                  else if (meta->feature_flags & DDCA_COMPLEX_NC) {
                     result->nontable_formatter = format_feature_detail_debug_bytes;
                  }
                  else if (meta->feature_flags & DDCA_NC_CONT) {
                     result->nontable_formatter = format_feature_detail_debug_bytes;
                  }
                  else {
                     result->nontable_formatter = format_feature_detail_debug_bytes;
                  }
               }  // pentry->nontable_formatter == NULL
            }  // DDCA_NONTABLE




 #ifdef OLD
            if (meta->feature_flags & DDCA_NON_TABLE) {
               if (pentry->nontable_formatter) {
                  DBGMSG("nontable_formatter");
                  result->nontable_formatter = pentry->nontable_formatter;
               }
               else {
                  if (meta->feature_flags & DDCA_SIMPLE_NC) {
                     if (meta->sl_values) {
                        DBGMSG("format_feature_detail_sl_lookup");
                        result->nontable_formatter = format_feature_detail_sl_lookup;
                     }
                     else {
                        DBGMSG("format_feature_detail_sl_byte");
                        result->nontable_formatter = format_feature_detail_sl_byte;
                     }
                  }
                  else if (meta->feature_flags & DDCA_STD_CONT)
                     result->nontable_formatter = format_feature_detail_standard_continuous;
                  else
                     result->nontable_formatter = format_feature_detail_debug_bytes;
               }
            }
            else if (meta->feature_flags & DDCA_TABLE) {
               result->table_formatter = pentry->table_formatter;
               if (!result->table_formatter)
                  result->table_formatter = default_table_feature_detail_function;
            }
            else
               PROGRAM_LOGIC_ERROR("Neither DDCA_TABLE nor DDCA_NON_TABLE is set");
 #endif
            if (pentry->vcp_global_flags & DDCA_SYNTHETIC)
               free_synthetic_vcp_entry(pentry);
         }
    }

    if (debug) {
       DBGMSF(debug, "Done. Returning: %p", result);
       if (result)
          dbgrpt_internal_feature_metadata(result, 1);
    }
    return result;
}
#endif


Display_Feature_Metadata *
dyn_get_feature_metadata_by_dfr_and_vspec_dfm(
     DDCA_Vcp_Feature_Code    feature_code,
     Dynamic_Features_Rec *   dfr,
     DDCA_MCCS_Version_Spec   vspec,
     bool                     with_default)

{
    bool debug = false;
    DBGMSF(debug, "Starting. feature_code=0x%02x, dfr=%p, vspec=%d.%d, with_default=%s",
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
            result = extract_version_feature_info_dfm(pentry, vspec, /*version_sensitive*/ true);
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
            else {
               if (pentry->nontable_formatter)
                  result->nontable_formatter = pentry->nontable_formatter;
               else {
                  if (result->feature_flags & DDCA_STD_CONT) {
                     result->nontable_formatter = format_feature_detail_standard_continuous;
                  }
                  else if (result->feature_flags & DDCA_COMPLEX_CONT) {
                     result->nontable_formatter = format_feature_detail_debug_bytes;
                  }
                  else if (result->feature_flags & DDCA_SIMPLE_NC) {
                     if (result->sl_values) {
                        // DBGMSG("format_feature_detail_sl_lookup");
                        result->nontable_formatter = format_feature_detail_sl_lookup;
                     }
                     else {
                        DBGMSG("format_feature_detail_sl_byte");
                        result->nontable_formatter = format_feature_detail_sl_byte;
                     }
                  }
                  else if (result->feature_flags & DDCA_COMPLEX_NC) {
                     result->nontable_formatter = format_feature_detail_debug_bytes;
                  }
                  else if (result->feature_flags & DDCA_NC_CONT) {
                     result->nontable_formatter = format_feature_detail_debug_bytes;
                  }
                  else {
                     result->nontable_formatter = format_feature_detail_debug_bytes;
                  }
               }  // pentry->nontable_formatter == NULL
            }  // DDCA_NONTABLE

            if (pentry->vcp_global_flags & DDCA_SYNTHETIC)
               free_synthetic_vcp_entry(pentry);
         }
    }

    if (debug) {
       DBGMSG("Done. Returning: %p", result);
       if (result)
          dbgrpt_display_feature_metadata(result, 1);
    }
    return result;
}





#ifdef IFM
Internal_Feature_Metadata *
dyn_get_feature_metadata_by_mmk_and_vspec(
     DDCA_Vcp_Feature_Code    feature_code,
     DDCA_Monitor_Model_Key   mmk,
     DDCA_MCCS_Version_Spec   vspec,
     bool                     with_default)

{
   bool debug = false;
    DBGMSF(debug, "Starting. feature_code=0x%02x, mmk=%s, vspec=%d.%d, with_default=%s",
                  feature_code, mmk_repr(mmk), vspec.major, vspec.minor, sbool(with_default));

    Dynamic_Features_Rec * dfr = NULL;

    Error_Info * erec =
    dfr_load_by_mmk(
          mmk,
          &dfr);
    if (erec) {
       // TODO test for not found, that's ok
       errinfo_report(erec,1);
    }

    Internal_Feature_Metadata * result =
          dyn_get_feature_metadata_by_dfr_and_vspec(feature_code, dfr, vspec, with_default);

    return result;
 }
#endif

Display_Feature_Metadata *
dyn_get_feature_metadata_by_mmk_and_vspec_dfm(
     DDCA_Vcp_Feature_Code    feature_code,
     DDCA_Monitor_Model_Key   mmk,
     DDCA_MCCS_Version_Spec   vspec,
     bool                     with_default)

{
   bool debug = true;
    DBGMSF(debug, "Starting. feature_code=0x%02x, mmk=%s, vspec=%d.%d, with_default=%s",
                  feature_code, mmk_repr(mmk), vspec.major, vspec.minor, sbool(with_default));

    Dynamic_Features_Rec * dfr = NULL;
    Error_Info * erec = dfr_load_by_mmk(mmk, &dfr);
    if (erec) {
       // TODO test for not found, that's ok
       errinfo_report(erec,1);
    }

    Display_Feature_Metadata * result =
          dyn_get_feature_metadata_by_dfr_and_vspec_dfm(feature_code, dfr, vspec, with_default);

    if (debug) {
       DBGMSG("Returning Display_Feature_Metadata at %p", result);
       if (result)
          dbgrpt_display_feature_metadata(result, 1);
    }

    return result;
 }



#ifdef IFM
Internal_Feature_Metadata *
dyn_get_feature_metadata_by_dref(
      DDCA_Vcp_Feature_Code feature_code,
      Display_Ref *         dref,
      bool                  with_default)
{
   bool debug = true;
   DBGMSF(debug, "Starting. feature_code=0x%02x, dref=%s, with_default=%s",
                 feature_code, dref_repr_t(dref), sbool(with_default));
   DBGMSF(debug, "dref->dfr=%p", dref->dfr);

   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_ref(dref);

   Internal_Feature_Metadata * result =
         dyn_get_feature_metadata_by_dfr_and_vspec(feature_code, dref->dfr, vspec, with_default);

   return result;
}
#endif

Display_Feature_Metadata *
dyn_get_feature_metadata_by_dref_dfm(
      DDCA_Vcp_Feature_Code feature_code,
      Display_Ref *         dref,
      bool                  with_default)
{
   bool debug = false;
   DBGMSF(debug, "Starting. feature_code=0x%02x, dref=%s, with_default=%s",
                 feature_code, dref_repr_t(dref), sbool(with_default));
   DBGMSF(debug, "dref->dfr=%p", dref->dfr);

   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_ref(dref);

   Display_Feature_Metadata * result =
         dyn_get_feature_metadata_by_dfr_and_vspec_dfm(feature_code, dref->dfr, vspec, with_default);
   if (result)
      result->display_ref = dref;

   DBG_RET_STRUCT(debug, Display_Feature_Metadata, dbgrpt_display_feature_metadata, result);
   return result;
}



#ifdef OLD
Internal_Feature_Metadata *
dyn_get_feature_metadata_by_dref0(
      DDCA_Vcp_Feature_Code feature_code,
      Display_Ref *         dref,
      bool                  with_default)
{
   bool debug = false;
   DBGMSF(debug, "Starting. feature_code=0x%02x, dref=%s, with_default=%s",
                 feature_code, dref_repr_t(dref), sbool(with_default));
   DBGMSF(debug, "dref->dfr=%p", dref->dfr);

   Internal_Feature_Metadata * result = NULL;

   if (dref->dfr) {
      DDCA_Feature_Metadata * dfr_metadata = get_dynamic_feature_metadata(dref->dfr, feature_code);
      if (dfr_metadata) {
         result = calloc(1, sizeof(Internal_Feature_Metadata));
         result->external_metadata = dfr_metadata;
         if (dfr_metadata->feature_flags & DDCA_SIMPLE_NC) {
            if (dfr_metadata->sl_values)
               result->vcp_nontable_formatter = dyn_format_feature_detail_sl_lookup;  // HACK
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
      DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_ref(dref);
      // DBGMSG("vspec=%d.%d", vspec.major, vspec.minor);

       VCP_Feature_Table_Entry * pentry =
              (with_default) ? vcp_find_feature_by_hexid_w_default(feature_code)
                             : vcp_find_feature_by_hexid(feature_code);
        if (pentry) {
           DDCA_Version_Feature_Info * info = extract_version_feature_info(pentry, vspec, /*version_sensitive*/ true);
           if (debug)
              dbgrpt_vcp_entry(pentry, 2);
           DDCA_Feature_Metadata * meta = calloc(1, sizeof(DDCA_Feature_Metadata));
           memcpy(meta->marker, DDCA_FEATURE_METADATA_MARKER, 4);
           version_feature_info_to_metadata(info, meta);
           free_version_feature_info(info);  // safe?
           result = calloc(1, sizeof(Internal_Feature_Metadata));
           result->external_metadata = meta;
#ifdef REF
           // Further refine the C/NC/TABLE categorization of the MCCS spec
           // Exactly 1 of the following 8 bits is set
           #define DDCA_STD_CONT     0x0080       /**< Normal continuous feature */
           #define DDCA_COMPLEX_CONT 0x0040       /**< Continuous feature with special interpretation */
           #define DDCA_SIMPLE_NC    0x0020       /**< Non-continuous feature, having a defined list of values in byte SL */
           #define DDCA_COMPLEX_NC   0x0010       /**< Non-continuous feature, having a complex interpretation using one or more of SL, SH, ML, MH */
           #define DDCA_NC_CONT      0x0800       /**< NC feature combining reserved values with continuous range */
           // For WO NC features.  There's no interpretation function or lookup table
           // Used to mark that the feature is defined for a version
           #define DDCA_WO_NC        0x0008       /**< Used internally for write-only non-continuous features */
           #define DDCA_NORMAL_TABLE 0x0004       /**< Normal RW table type feature */
           #define DDCA_WO_TABLE     0x0002       /**< Write only table feature */
#endif
           if (meta->feature_flags & DDCA_TABLE) {
              if (pentry->table_formatter)
                 result->table_formatter = pentry->table_formatter;
              else {
                 if (meta->feature_flags & DDCA_NORMAL_TABLE) {
                    result->table_formatter = default_table_feature_detail_function;
                 }
                 else if (meta->feature_flags & DDCA_WO_TABLE) {
                    // program logic error?
                    result->table_formatter = NULL;
                 }
                 else {
                    PROGRAM_LOGIC_ERROR("Neither DDCA_NORMAL_TABLE or DDCA_WO_TABLE  set in meta->feature_flags");
                 }
              }
           }
           else {
              if (pentry->nontable_formatter)
                 result->nontable_formatter = pentry->nontable_formatter;
              else {
                 if (meta->feature_flags & DDCA_STD_CONT) {
                    result->nontable_formatter = format_feature_detail_standard_continuous;
                 }
                 else if (meta->feature_flags & DDCA_COMPLEX_CONT) {
                    result->nontable_formatter = format_feature_detail_debug_bytes;
                 }
                 else if (meta->feature_flags & DDCA_SIMPLE_NC) {
                    if (meta->sl_values) {
                       DBGMSG("format_feature_detail_sl_lookup");
                       result->nontable_formatter = format_feature_detail_sl_lookup;
                    }
                    else {
                       DBGMSG("format_feature_detail_sl_byte");
                       result->nontable_formatter = format_feature_detail_sl_byte;
                    }
                 }
                 else if (meta->feature_flags & DDCA_COMPLEX_NC) {
                    result->nontable_formatter = format_feature_detail_debug_bytes;
                 }
                 else if (meta->feature_flags & DDCA_NC_CONT) {
                    result->nontable_formatter = format_feature_detail_debug_bytes;
                 }
                 else {
                    result->nontable_formatter = format_feature_detail_debug_bytes;
                 }
              }  // pentry->nontable_formatter == NULL
           }  // DDCA_NONTABLE




#ifdef OLD
           if (meta->feature_flags & DDCA_NON_TABLE) {
              if (pentry->nontable_formatter) {
                 DBGMSG("nontable_formatter");
                 result->nontable_formatter = pentry->nontable_formatter;
              }
              else {
                 if (meta->feature_flags & DDCA_SIMPLE_NC) {
                    if (meta->sl_values) {
                       DBGMSG("format_feature_detail_sl_lookup");
                       result->nontable_formatter = format_feature_detail_sl_lookup;
                    }
                    else {
                       DBGMSG("format_feature_detail_sl_byte");
                       result->nontable_formatter = format_feature_detail_sl_byte;
                    }
                 }
                 else if (meta->feature_flags & DDCA_STD_CONT)
                    result->nontable_formatter = format_feature_detail_standard_continuous;
                 else
                    result->nontable_formatter = format_feature_detail_debug_bytes;
              }
           }
           else if (meta->feature_flags & DDCA_TABLE) {
              result->table_formatter = pentry->table_formatter;
              if (!result->table_formatter)
                 result->table_formatter = default_table_feature_detail_function;
           }
           else
              PROGRAM_LOGIC_ERROR("Neither DDCA_TABLE nor DDCA_NON_TABLE is set");
#endif
           if (pentry->vcp_global_flags & DDCA_SYNTHETIC)
              free_synthetic_vcp_entry(pentry);
        }
   }

   if (debug) {
      DBGMSF(debug, "Done. Returning: %p", result);
      if (result)
         dbgrpt_internal_feature_metadata(result, 1);
   }
   return result;
}
#endif

#ifdef IFM
Internal_Feature_Metadata *
dyn_get_feature_metadata_by_dh(
      DDCA_Vcp_Feature_Code id,
      Display_Handle *      dh,
      bool                  with_default)
{
   bool debug = true;
   DBGMSF(debug, "Starting. id=0x%02x, dh=%s, with_default=%s",
                 id, dh_repr_t(dh), sbool(with_default) );

   // ensure dh->dref->vcp_version set without incurring additional open/close
   get_vcp_version_by_display_handle(dh);
   Internal_Feature_Metadata * result = dyn_get_feature_metadata_by_dref(id, dh->dref, with_default);

   DBGMSF(debug, "Done. Returning: %p", result);
   return result;

}
#endif



Display_Feature_Metadata *
dyn_get_feature_metadata_by_dh_dfm(
      DDCA_Vcp_Feature_Code id,
      Display_Handle *      dh,
      bool                  with_default)
{
   bool debug = false;
   DBGMSF(debug, "Starting. id=0x%02x, dh=%s, with_default=%s",
                 id, dh_repr_t(dh), sbool(with_default) );

   // ensure dh->dref->vcp_version set without incurring additional open/close
   get_vcp_version_by_display_handle(dh);
   Display_Feature_Metadata * result = dyn_get_feature_metadata_by_dref_dfm(id, dh->dref, with_default);

   DBGMSF(debug, "Done. Returning: %p", result);
   return result;

}





// Functions that apply formatting
#ifdef IFM
bool
dyn_format_nontable_feature_detail(
        Internal_Feature_Metadata *  intmeta,
        DDCA_MCCS_Version_Spec     vcp_version,
        Nontable_Vcp_Value *       code_info,
        char *                     buffer,
        int                        bufsz)
{
   bool debug = true;
   DBGMSF(debug, "Starting. Code=0x%02x, vcp_version=%d.%d",
                 intmeta->external_metadata->feature_code, vcp_version.major, vcp_version.minor);

   DDCA_Feature_Metadata * extmeta = intmeta->external_metadata;

   bool ok = false;
   if (intmeta->nontable_formatter) {
      Format_Normal_Feature_Detail_Function ffd_func = intmeta->nontable_formatter;
        // get_nontable_feature_detail_function(vfte, vcp_version);
      ok = ffd_func(code_info, vcp_version,  buffer, bufsz);
   }
   else if (intmeta->vcp_nontable_formatter) {
      Format_Normal_Feature_Detail_Function2 ffd_func = intmeta->vcp_nontable_formatter;
      ok = ffd_func(code_info, extmeta->sl_values, buffer, bufsz);
   }
   else
      PROGRAM_LOGIC_ERROR("Neither nontable_formatter nor vcp_nontable_formatter set");

   DBGMSF(debug, "Returning: %s", sbool(ok));
   return ok;
}
#endif

bool
dyn_format_nontable_feature_detail_dfm(
        Display_Feature_Metadata * dfm,
        DDCA_MCCS_Version_Spec     vcp_version,
        Nontable_Vcp_Value *       code_info,
        char *                     buffer,
        int                        bufsz)
{
   bool debug = false;
   DBGMSF(debug, "Starting. Code=0x%02x, vcp_version=%d.%d",
                 dfm->feature_code, vcp_version.major, vcp_version.minor);

   bool ok = false;
   if (dfm->nontable_formatter) {
      Format_Normal_Feature_Detail_Function ffd_func = dfm->nontable_formatter;
        // get_nontable_feature_detail_function(vfte, vcp_version);
      ok = ffd_func(code_info, vcp_version,  buffer, bufsz);
   }
   else if (dfm->nontable_formatter_sl) {
      Format_Normal_Feature_Detail_Function2 ffd_func = dfm->nontable_formatter_sl;
      ok = ffd_func(code_info, dfm->sl_values, buffer, bufsz);
   }
   else
      PROGRAM_LOGIC_ERROR("Neither nontable_formatter nor vcp_nontable_formatter set");

   DBGMSF(debug, "Returning: %s", sbool(ok));
   return ok;
}


#ifdef IFM
bool
dyn_format_table_feature_detail(
      Internal_Feature_Metadata * intmeta,
      DDCA_MCCS_Version_Spec      vcp_version,
      Buffer *                    accumulated_value,
      char * *                    aformatted_data
     )
{
   Format_Table_Feature_Detail_Function ffd_func = intmeta->table_formatter;
      //   get_table_feature_detail_function(vfte, vcp_version);
   bool ok = ffd_func(accumulated_value, vcp_version, aformatted_data);
   return ok;
}
#endif

bool
dyn_format_table_feature_detail_dfm(
       Display_Feature_Metadata *  dfm,
       DDCA_MCCS_Version_Spec     vcp_version,
       Buffer *                   accumulated_value,
       char * *                   aformatted_data
     )
{
   Format_Table_Feature_Detail_Function ffd_func = dfm->table_formatter;
      //   get_table_feature_detail_function(vfte, vcp_version);
   bool ok = ffd_func(accumulated_value, vcp_version, aformatted_data);
   return ok;
}


#ifdef APPARENT_DUPLICATE
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
      Internal_Feature_Metadata *  intmeta,
       DDCA_MCCS_Version_Spec    vcp_version,
       Single_Vcp_Value *        valrec,
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
      ok = dyn_format_nontable_feature_detail(
              intmeta,
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
      ok = dyn_format_table_feature_detail(
            intmeta,
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
#endif


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
#ifdef IFM
bool
dyn_format_feature_detail(
       Internal_Feature_Metadata * intmeta,
       DDCA_MCCS_Version_Spec    vcp_version,
       DDCA_Any_Vcp_Value *      valrec,
       char * *                  aformatted_data
     )
{
   bool debug = true;
   DBGMSF(debug, "Starting");

   bool ok = true;
   *aformatted_data = NULL;

   DBGMSF(debug, "valrec->value_type = %d", valrec->value_type);
   char * formatted_data = NULL;
   if (valrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
      DBGMSF(debug, "DDCA_NON_TABLE_VCP_VALUE");
      Nontable_Vcp_Value* nontable_value = single_vcp_value_to_nontable_vcp_value(valrec);
      char workbuf[200];
      ok = dyn_format_nontable_feature_detail(
              intmeta,
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
      ok = dyn_format_table_feature_detail(
            intmeta,
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
#endif

bool
dyn_format_feature_detail_dfm(
       Display_Feature_Metadata * dfm,
       DDCA_MCCS_Version_Spec    vcp_version,
       DDCA_Any_Vcp_Value *      valrec,
       char * *                  aformatted_data
     )
{
   bool debug = true;
   DBGMSF(debug, "Starting");

   bool ok = true;
   *aformatted_data = NULL;

   DBGMSF(debug, "valrec->value_type = %d", valrec->value_type);
   char * formatted_data = NULL;
   if (valrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
      DBGMSF(debug, "DDCA_NON_TABLE_VCP_VALUE");
      Nontable_Vcp_Value* nontable_value = single_vcp_value_to_nontable_vcp_value(valrec);
      char workbuf[200];
      ok = dyn_format_nontable_feature_detail_dfm(
              dfm,
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
      ok = dyn_format_table_feature_detail_dfm(
            dfm,
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
         DDCA_MCCS_Version_Spec vspec = dref->vcp_version;   // TODO use function call in case not set
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
   rtti_func_name_table_add(dyn_format_nontable_feature_detail_dfm, "dyn_format_nontable_feature_detail_dfm");
   rtti_func_name_table_add(dyn_format_table_feature_detail_dfm,    "dyn_format_table_feature_detail_dfm");
   rtti_func_name_table_add(dyn_format_feature_detail_dfm,          "dyn_format_feature_detail_dfm");
   rtti_func_name_table_add(dyn_format_feature_detail_sl_lookup,    "dyn_format_feature_detail_sl_lookup");
   // dbgrpt_func_name_table(0);
}

#ifdef IFM
// for transition
Internal_Feature_Metadata *
dfm_to_internal_feature_metadata(
      Display_Feature_Metadata * dfm)
{
   Internal_Feature_Metadata * ifm = calloc(1, sizeof(Internal_Feature_Metadata));
   ifm->external_metadata = dfm_to_ddca_feature_metadata(dfm);
   ifm->nontable_formatter = dfm->nontable_formatter;
   ifm->vcp_nontable_formatter = dfm->nontable_formatter_sl;
   ifm->table_formatter = dfm->table_formatter;
   return ifm;
}
#endif
