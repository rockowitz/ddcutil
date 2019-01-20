/** \file api_feature_access.c
 *
 *  Get, set, and format feature values
 */

// Copyright (C) 2015-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <assert.h>
#include <string.h>

#include "public/ddcutil_c_api.h"
#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_types.h"
 
#include "util/error_info.h"
#include "util/report_util.h"

#include "base/displays.h"
#include "base/monitor_model_key.h"

#include "vcp/vcp_feature_values.h"

#include "dynvcp/dyn_feature_codes.h"

#include "ddc/ddc_async.h"
#include "ddc/ddc_dumpload.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"

#include "libmain/api_base_internal.h"
#include "libmain/api_displays_internal.h"

#include "libmain/api_feature_access_internal.h"

//
// Get and Set Feature Values
//

#ifdef OLD
// Was public, but eliminated from API due to problems in Python API caused by overlay.
// Retained for impedance matching.  Retained for historical interest.

/** Represents a single non-table VCP value */
typedef struct {
   DDCA_Vcp_Feature_Code  feature_code;
   union {
      struct {
         uint16_t   max_val;        /**< maximum value (mh, ml bytes) for continuous value */
         uint16_t   cur_val;        /**< current value (sh, sl bytes) for continuous value */
      }         c;                  /**< continuous (C) value */
      struct {
   // Ensure proper overlay of ml/mh on max_val, sl/sh on cur_val

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
      uint8_t    ml;            /**< ML byte for NC value */
      uint8_t    mh;            /**< MH byte for NC value */
      uint8_t    sl;            /**< SL byte for NC value */
      uint8_t    sh;            /**< SH byte for NC value */
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
      uint8_t    mh;
      uint8_t    ml;
      uint8_t    sh;
      uint8_t    sl;
#else
#error "Unexpected byte order value: __BYTE_ORDER__"
#endif
      }         nc;                /**< non-continuous (NC) value */
   } val;
} Non_Table_Value_Response;
#endif


DDCA_Status
ddca_get_non_table_vcp_value(
      DDCA_Display_Handle        ddca_dh,
      DDCA_Vcp_Feature_Code      feature_code,
      DDCA_Non_Table_Vcp_Value*  valrec)
{

   WITH_DH(ddca_dh,  {
       Error_Info * ddc_excp = NULL;
       Parsed_Nontable_Vcp_Response * code_info;
       ddc_excp = ddc_get_nontable_vcp_value(
                     dh,
                     feature_code,
                     &code_info);

       if (!ddc_excp) {
          valrec->mh = code_info->mh;
          valrec->ml = code_info->ml;;
          valrec->sh = code_info->sh;
          valrec->sl = code_info->sl;
          // DBGMSG("valrec:  mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x",
          //        valrec->mh, valrec->ml, valrec->sh, valrec->sl);
          free(code_info);
       }
       else {
          psc = ddc_excp->status_code;
          errinfo_free_with_report(ddc_excp, report_freed_exceptions, __func__);
          // errinfo_free(ddc_excp);
       }
    } );
}


// untested
DDCA_Status
ddca_get_table_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_Table_Vcp_Value **    table_value_loc)
{
   WITH_DH(ddca_dh,
      {
         Error_Info * ddc_excp = NULL;
         Buffer * p_table_bytes = NULL;
         ddc_excp =  ddc_get_table_vcp_value(dh, feature_code, &p_table_bytes);
         psc = (ddc_excp) ? ddc_excp->status_code : 0;
         errinfo_free(ddc_excp);
         if (psc == 0) {
            assert(p_table_bytes);  // avoid coverity warning
            int len = p_table_bytes->len;
            DDCA_Table_Vcp_Value * tv = calloc(1,sizeof(DDCA_Table_Vcp_Value));
            tv->bytect = len;
            if (len > 0) {
               tv->bytes = malloc(len);
               memcpy(tv->bytes, p_table_bytes->bytes, len);
            }
            *table_value_loc = tv;

            buffer_free(p_table_bytes, __func__);
         }
      }
   );
}


static
DDCA_Status
ddca_get_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_Vcp_Value_Type    call_type,   // why is this needed?   look it up from dh and feature_code
      DDCA_Any_Vcp_Value **  pvalrec)
{
   Error_Info * ddc_excp = NULL;

   WITH_DH(ddca_dh,
         {
               bool debug = false;
               DBGMSF(debug, "Starting. ddca_dh=%p, feature_code=0x%02x, call_type=%d, pvalrec=%p",
                      ddca_dh, feature_code, call_type, pvalrec);
               *pvalrec = NULL;
               ddc_excp = ddc_get_vcp_value(dh, feature_code, call_type, pvalrec);
               psc = (ddc_excp) ? ddc_excp->status_code : 0;
               errinfo_free(ddc_excp);
               DBGMSF(debug, "*pvalrec=%p", *pvalrec);
         }
   );
}


static DDCA_Status
get_value_type(
      DDCA_Display_Handle         ddca_dh,
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Vcp_Value_Type *       p_value_type)
{
   bool debug = false;
   DBGMSF(debug, "Starting. ddca_dh=%p, feature_code=0x%02x", ddca_dh, feature_code);

   DDCA_Status ddcrc = DDCRC_NOT_FOUND;
   DDCA_MCCS_Version_Spec vspec     = get_vcp_version_by_display_handle(ddca_dh);
   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
   if (pentry) {
      DDCA_Version_Feature_Flags flags = get_version_sensitive_feature_flags(pentry, vspec);
      // Version_Feature_Flags flags = feature_info->internal_feature_flags;
      // n. will default to NON_TABLE_VCP_VALUE if not a known code
      *p_value_type = (flags & DDCA_TABLE) ?  DDCA_TABLE_VCP_VALUE : DDCA_NON_TABLE_VCP_VALUE;
      ddcrc = 0;
   }

   DBGMSF(debug, "Returning %d", ddcrc);
   return ddcrc;
}


DDCA_Status
ddca_get_any_vcp_value_using_explicit_type(
       DDCA_Display_Handle         ddca_dh,
       DDCA_Vcp_Feature_Code       feature_code,
       DDCA_Vcp_Value_Type         call_type,
       DDCA_Any_Vcp_Value **       pvalrec)
{
   bool debug = false;
   DBGMSF(debug, "Starting. ddca_dh=%p, feature_code=0x%02x, call_type=%d, pvalrec=%p",
          ddca_dh, feature_code, call_type, pvalrec);
   *pvalrec = NULL;
   DDCA_Status rc = DDCRC_ARG;

   DDCA_Any_Vcp_Value * valrec2 = NULL;
   rc = ddca_get_vcp_value(ddca_dh, feature_code, call_type, &valrec2);
   if (rc == 0) {
      *pvalrec = valrec2;
   }

   DBGMSF(debug, "Done. Returning %s, *pvalrec=%p", psc_desc(rc), *pvalrec);
   return rc;
}


#ifdef ALT
DDCA_Status
ddca_get_any_vcp_value_using_explicit_type_new(
       DDCA_Display_Handle         ddca_dh,
       DDCA_Vcp_Feature_Code       feature_code,
       DDCA_Vcp_Value_Type_Parm    call_type,
       DDCA_Any_Vcp_Value **       pvalrec)
{
   bool debug = false;
   DBGMSF(debug, "Starting. ddca_dh=%p, feature_code=0x%02x, call_type=%d, pvalrec=%p",
          ddca_dh, feature_code, call_type, pvalrec);
   *pvalrec = NULL;
   DDCA_Status rc = DDCRC_ARG;

   if (call_type == DDCA_UNSET_VCP_VALUE_TYPE_PARM) {
      call_type = get_value_type_parm(ddca_dh, feature_code, DDCA_UNSET_VCP_VALUE_TYPE_PARM);
   }
   if (call_type != DDCA_UNSET_VCP_VALUE_TYPE_PARM) {

      Single_Vcp_Value *  valrec2 = NULL;
      rc = ddca_get_vcp_value(ddca_dh, feature_code, call_type, &valrec2);
      if (rc == 0) {
         DDCA_Any_Vcp_Value * valrec = single_vcp_value_to_any_vcp_value(valrec2);
         free(valrec2);   // n. does not free table bytes, which are now pointed to by valrec
         *pvalrec = valrec;
      }
   }
   DBGMSF(debug, "Done. Returning %s, *pvalrec=%p", psc_desc(rc), *pvalrec);
   return rc;
}
#endif


DDCA_Status
ddca_get_any_vcp_value_using_implicit_type(
       DDCA_Display_Handle         ddca_dh,
       DDCA_Vcp_Feature_Code       feature_code,
       DDCA_Any_Vcp_Value **       valrec_loc)
{

   DDCA_Vcp_Value_Type call_type;

   DDCA_Status ddcrc = get_value_type(ddca_dh, feature_code, &call_type);

   if (ddcrc == 0) {
      ddcrc = ddca_get_any_vcp_value_using_explicit_type(
                 ddca_dh,
                 call_type,
                 feature_code,
                 valrec_loc);
   }
   return ddcrc;
}


void
ddca_free_any_vcp_value(
      DDCA_Any_Vcp_Value * valrec)
{
   if (valrec) {
      if (valrec->value_type == DDCA_TABLE_VCP_VALUE) {
         free(valrec->val.t.bytes);
      }
      free(valrec);
   }
}


void
dbgrpt_any_vcp_value(
      DDCA_Any_Vcp_Value * valrec,
      int                  depth)
{
   int d1 = depth+1;
   rpt_vstring(depth, "DDCA_Any_Vcp_Value at %p:", valrec);
   rpt_vstring(d1, "opcode=0x%02x, value_type=%s (0x%02x)",
                   valrec->opcode, vcp_value_type_name(valrec->value_type), valrec->value_type);
   if (valrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
      rpt_vstring(d1, "mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x",
                      valrec->val.c_nc.mh, valrec->val.c_nc.ml,
                      valrec->val.c_nc.sh, valrec->val.c_nc.sl);
      uint16_t max_val = valrec->val.c_nc.mh << 8 | valrec->val.c_nc.ml;
      uint16_t cur_val = valrec->val.c_nc.sh << 8 | valrec->val.c_nc.sl;
      rpt_vstring(d1, "max_val=%d (0x%04x), cur_val=%d (0x%04x)",
                      max_val,
                      max_val,
                      cur_val,
                      cur_val);
   }
   else if (valrec->value_type == DDCA_TABLE_VCP_VALUE) {
      rpt_hex_dump(valrec->val.t.bytes, valrec->val.t.bytect, d1);
   }
   else {
      rpt_vstring(d1, "Unrecognized value type: %d", valrec->value_type);
   }
}

// deprecated, does not support user defined features
DDCA_Status
ddca_get_formatted_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      char**                 formatted_value_loc)
{
   bool debug = false;
   DBGMSF(debug, "Starting. feature_code=0x%02x", feature_code);
   Error_Info * ddc_excp = NULL;
   WITH_DH(ddca_dh,
         {
               *formatted_value_loc = NULL;
               DDCA_MCCS_Version_Spec vspec      = get_vcp_version_by_display_handle(dh);
               // DDCA_MCCS_Version_Id   version_id = mccs_version_spec_to_id(vspec);

               VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
               if (!pentry) {
#ifdef ALT
               DDCA_Version_Feature_Info * feature_info =
               get_version_specific_feature_info(
                     feature_code,
                     true,                    //    with_default
                     version_id);
               assert(feature_info);
               if (!feature_info) {
#endif
                  psc = DDCRC_ARG;
               }
               else {
                  DDCA_Version_Feature_Flags flags = get_version_sensitive_feature_flags(pentry, vspec);
                  if (!(flags & DDCA_READABLE)) {
                     if (flags & DDCA_DEPRECATED)
                        *formatted_value_loc = g_strdup_printf("Feature %02x is deprecated in MCCS %d.%d\n",
                                                          feature_code, vspec.major, vspec.minor);
                     else
                        *formatted_value_loc = g_strdup_printf("Feature %02x is not readable\n", feature_code);
                     DBGMSF(debug, "%s", *formatted_value_loc);
                     psc = DDCRC_INVALID_OPERATION;
                  }
                  else {
                     // Version_Feature_Flags flags = feature_info->internal_feature_flags;
                      // n. will default to NON_TABLE_VCP_VALUE if not a known code
                      DDCA_Vcp_Value_Type call_type = (flags & DDCA_TABLE) ?  DDCA_TABLE_VCP_VALUE : DDCA_NON_TABLE_VCP_VALUE;
                      DDCA_Any_Vcp_Value * pvalrec;
                      ddc_excp = ddc_get_vcp_value(dh, feature_code, call_type, &pvalrec);
                      psc = (ddc_excp) ? ddc_excp->status_code : 0;
                      errinfo_free(ddc_excp);
                      if (psc == 0) {
                         bool ok =
                         vcp_format_feature_detail(
                                pentry,
                                vspec,
                                pvalrec,
                                formatted_value_loc
                              );
                         if (!ok) {
                            psc = DDCRC_OTHER;    // ** WRONG CODE ***
                            assert(!formatted_value_loc);
                         }
                      }
                  }
               }
         }
   )
}


DDCA_Status
ddca_format_any_vcp_value(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_MCCS_Version_Spec  vspec,
      DDCA_Monitor_Model_Key * mmid,
      DDCA_Any_Vcp_Value *    anyval,
      char **                 formatted_value_loc)
{
   bool debug = false;
   DBGMSF(debug, "Starting. feature_code=0x%02x, vspec=%d.%d, mmid=%p -> %s",
                 feature_code,
                 vspec.major, vspec.minor,
                 mmid,
                 (mmid) ? mmk_repr(*mmid) : "NULL"
                 );
   DDCA_Status psc = 0;
   free_thread_error_detail();

   *formatted_value_loc = NULL;

   if (!mmid) {
      *formatted_value_loc = strdup("Programming error. mmid not specified");
      psc = DDCRC_ARG;
      goto bye;
   }

   Display_Feature_Metadata * dfm =
   dyn_get_feature_metadata_by_mmk_and_vspec_dfm(
        feature_code, *mmid, vspec, /*with_default=*/ true);
   if (!dfm) {
      psc = DDCRC_ARG;
      *formatted_value_loc = g_strdup_printf("Unrecognized feature code 0x%02x", feature_code);
      goto bye;
   }
   DDCA_Feature_Flags flags = dfm->feature_flags;

   if (!(flags & DDCA_READABLE)) {
      if (flags & DDCA_DEPRECATED)
         *formatted_value_loc = g_strdup_printf("Feature %02x is deprecated in MCCS %d.%d",
                                           feature_code, vspec.major, vspec.minor);
      else
         *formatted_value_loc = g_strdup_printf("Feature %02x is not readable", feature_code);
      DBGMSF(debug, "%s", *formatted_value_loc);
      psc = DDCRC_INVALID_OPERATION;
      goto bye;
   }

   // Version_Feature_Flags flags = feature_info->internal_feature_flags;
   // n. will default to NON_TABLE_VCP_VALUE if not a known code
   DDCA_Vcp_Value_Type call_type = (flags & DDCA_TABLE)
                                        ? DDCA_TABLE_VCP_VALUE
                                        : DDCA_NON_TABLE_VCP_VALUE;
   if (call_type != anyval->value_type) {
       *formatted_value_loc = g_strdup_printf(
             "Feature type in value does not match feature code");
       psc = DDCRC_ARG;
       goto bye;
   }
   bool ok = dyn_format_feature_detail_dfm(dfm, vspec, anyval,formatted_value_loc);
   if (!ok) {
       psc = DDCRC_ARG;    // ??
       assert(!formatted_value_loc);
       *formatted_value_loc = g_strdup_printf("Unable to format value for feature 0x%02x", feature_code);
   }

bye:
   // TODO: free ifr ?
   // if (pentry)
   //    free_synthetic_vcp_entry(pentry);   // does nothing if not synthetic

   DBGMSF(debug, "Returning: %s, formatted_value_loc -> %s", psc_desc(psc), *formatted_value_loc);
   return psc;
}


DDCA_Status
ddca_format_any_vcp_value_by_dref(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Display_Ref        ddca_dref,
      DDCA_Any_Vcp_Value *    valrec,
      char **                 formatted_value_loc)
{
   WITH_DR(ddca_dref,
         {
               bool debug = false;
               if (debug) {
                  DBGMSG("feature_code=0x%02x, ddca_dref=%s, valrec=%s",
                         feature_code,
                         dref_repr_t(dref),
                         summarize_single_vcp_value(valrec) );
                  dbgrpt_display_ref( dref, 1);
               }
               return ddca_format_any_vcp_value(
                         feature_code,
                         dref->vcp_version,
                         dref->mmid,
                         valrec,
                         formatted_value_loc);
         }
   )
}


DDCA_Status
ddca_format_non_table_vcp_value(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_MCCS_Version_Spec      vspec,
      DDCA_Monitor_Model_Key *    mmid,
      DDCA_Non_Table_Vcp_Value *  valrec,
      char **                     formatted_value_loc)
{
   bool debug = false;
   if (debug) {
      DBGMSG("feature_code=0x%02x, vspec=%d.%d, mmid=%s",
             feature_code,
             vspec.major, vspec.minor,
             (mmid) ? mmk_repr(*mmid) : "NULL");
   }

   // free_thread_error_detail();   // unnecessary, done by ddca_format_any_vcp_value();
   DDCA_Any_Vcp_Value anyval;
   anyval.opcode = feature_code;
   anyval.value_type = DDCA_NON_TABLE_VCP_VALUE;
   anyval.val.c_nc.mh = valrec->mh;
   anyval.val.c_nc.ml = valrec->ml;
   anyval.val.c_nc.sh = valrec->sh;
   anyval.val.c_nc.sl = valrec->sl;

   return ddca_format_any_vcp_value(feature_code, vspec, mmid, &anyval, formatted_value_loc);
}

DDCA_Status
ddca_format_non_table_vcp_value_by_dref(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Display_Ref            ddca_dref,
      DDCA_Non_Table_Vcp_Value *  valrec,
      char **                     formatted_value_loc)
{
   WITH_DR(ddca_dref,
         {
               bool debug = false;
               if (debug) {
                  DBGMSG("feature_code=0x%02x, ddca_dref=%s",
                         feature_code,
                         dref_repr_t(dref) );
                         // summarize_single_vcp_value(valrec) );
                  dbgrpt_display_ref( dref, 1);
               }
               return ddca_format_non_table_vcp_value(
                         feature_code,
                         dref->vcp_version,
                         dref->mmid,
                         valrec,
                         formatted_value_loc);
         }
   )
}


DDCA_Status
ddca_format_table_vcp_value(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_MCCS_Version_Spec  vspec,
      DDCA_Monitor_Model_Key * mmid,
      DDCA_Table_Vcp_Value *  table_value,
      char **                 formatted_value_loc)
{
   // free_thread_error_detail();   // unnecessary, done by ddca_format_any_vcp_value();
   DDCA_Any_Vcp_Value anyval;
   anyval.opcode = feature_code;
   anyval.value_type = DDCA_TABLE_VCP_VALUE;
   anyval.val.t.bytect = table_value->bytect;
   anyval.val.t.bytes  = table_value->bytes;   // n. copying pointer, not duplicating bytes

   return ddca_format_any_vcp_value(
             feature_code, vspec, mmid, &anyval, formatted_value_loc);
}


DDCA_Status
ddca_format_table_vcp_value_by_dref(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Display_Ref        ddca_dref,
      DDCA_Table_Vcp_Value *  table_value,
      char **                 formatted_value_loc)
{
   WITH_DR(ddca_dref,
         {
               return ddca_format_table_vcp_value(
                         feature_code,
                         dref->vcp_version,
                         dref->mmid,
                         table_value,
                         formatted_value_loc);
         }
   )
}


static
DDCA_Status
set_single_vcp_value(
      DDCA_Display_Handle  ddca_dh,
      DDCA_Any_Vcp_Value *   valrec,
      DDCA_Any_Vcp_Value **  verified_value_loc)
{
      Error_Info * ddc_excp = NULL;
      WITH_DH(ddca_dh,  {
            ddc_excp = ddc_set_vcp_value(dh, valrec, verified_value_loc);
            psc = (ddc_excp) ? ddc_excp->status_code : 0;
            errinfo_free(ddc_excp);
         } );
}

// UNPUBLISHED
/** Sets a Continuous VCP value.
 *
 *  @param[in]  ddca_dh             display_handle
 *  @param[in]  feature_code        VCP feature code
 *  @param[in]  new_value           value to set (sign?)
 *  @param[out] verified_value_loc  if non-null, return verified value here
 *  @return status code
 *
 * @remark
 *  Verification is performed if **verified_value_loc** is non-NULL and
 *  verification has been enabled (see #ddca_enable_verify()).
 *  @remark
 *  If verification is performed, the value of the feature is read after being
 *  written. If the returned status code is either DDCRC_OK (0) or DDCRC_VERIFY,
 *  the verified value is returned in **verified_value_loc**.
 *  @remark
 *  This is essentially a convenience function, since a Continuous value can be
 *  set by passing its high and low bytes to #ddca_set_non_table_vcp_value_verify().
 */
DDCA_Status
ddca_set_continuous_vcp_value_verify(
      DDCA_Display_Handle   ddca_dh,
      DDCA_Vcp_Feature_Code feature_code,
      uint16_t              new_value,
      uint16_t *            verified_value_loc)
{
   DDCA_Status rc = 0;
   free_thread_error_detail();

   DDCA_Any_Vcp_Value valrec;
   valrec.opcode = feature_code;
   valrec.value_type = DDCA_NON_TABLE_VCP_VALUE;
   valrec.val.c_nc.sh = (new_value >> 8) & 0xff;
   valrec.val.c_nc.sl = new_value & 0xff;

   if (verified_value_loc) {
      DDCA_Any_Vcp_Value * verified_single_value;
      rc = set_single_vcp_value(ddca_dh, &valrec, &verified_single_value);
      if (verified_single_value)
      *verified_value_loc = VALREC_CUR_VAL(verified_single_value);
   }
   else {
      rc = set_single_vcp_value(ddca_dh, &valrec, NULL);
   }

   return rc;
}

// DEPRECATED AS OF 0.9.0
/** Sets a Continuous VCP value.
 *
 *  @param[in]  ddca_dh             display_handle
 *  @param[in]  feature_code        VCP feature code
 *  @param[in]  new_value           value to set (sign?)
 *  @return status code
 *
 * @remark
 *  This is essentially a convenience function, since a Continuous value
 *  can be set by passing its high and low bytes to #ddca_set_non_table_vcp_value().
 */
DDCA_Status
ddca_set_continuous_vcp_value(
      DDCA_Display_Handle   ddca_dh,
      DDCA_Vcp_Feature_Code feature_code,
      uint16_t              new_value)
{
   return ddca_set_continuous_vcp_value_verify(ddca_dh, feature_code, new_value, NULL);
}


/** \deprecated */
DDCA_Status
ddca_set_simple_nc_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      Byte                   new_value)
{
   return ddca_set_continuous_vcp_value_verify(ddca_dh, feature_code, new_value, NULL);
}

// UNPUBLISHED
/** Sets a non-table VCP value by specifying it's high and low bytes individually.
 *  Optionally returns the values set by reading the feature code after writing.
 *
 *  \param[in]   ddca_dh             display handle
 *  \param[in]   feature_code        feature code
 *  \param[in]   hi_byte             high byte of new value
 *  \param[in]   lo_byte             low byte of new value
 *  \param[out]  p_verified_hi_byte  where to return high byte of verified value
 *  \param[out]  p_verified_lo_byte  where to return low byte of verified value
 *  \return      status code
 *
 *  \remark
 *  Either both **verified_hi_byte_loc** and **verified_lo_byte_loc** should be
 *  set, or neither. Otherwise, status code **DDCRC_ARG** is returned.
 *  \remark
 *  Verification is performed only it has been enabled (see #ddca_enable_verify()) and
 *  both **verified_hi_byte** and **verified_lo_byte** are set.
 *  \remark
 *  Verified values are returned if the status code is either 0 (success),
 *  or **DDCRC_VERIFY**, i.e. the write succeeded but verification failed.
 */
DDCA_Status
ddca_set_non_table_vcp_value_verify(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      Byte                   hi_byte,
      Byte                   lo_byte,
      Byte *                 verified_hi_byte_loc,
      Byte *                 verified_lo_byte_loc)
{
   free_thread_error_detail();
   if ( ( verified_hi_byte_loc && !verified_lo_byte_loc) ||
        (!verified_hi_byte_loc &&  verified_lo_byte_loc )
      )
      return DDCRC_ARG;

   // unwrap into 2 cases to clarify logic and avoid compiler warning
   DDCA_Status rc = 0;
   if (verified_hi_byte_loc) {
      uint16_t verified_c_value = 0;
      rc = ddca_set_continuous_vcp_value_verify(
                          ddca_dh,
                          feature_code, hi_byte << 8 | lo_byte,
                          &verified_c_value);
      *verified_hi_byte_loc = verified_c_value >> 8;
      *verified_lo_byte_loc = verified_c_value & 0xff;
   }
   else {
      rc = ddca_set_continuous_vcp_value_verify(
                          ddca_dh,
                          feature_code, hi_byte << 8 | lo_byte,
                          NULL);
   }

   return rc;
}

DDCA_Status
ddca_set_non_table_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      Byte                   hi_byte,
      Byte                   lo_byte)
{
   return ddca_set_non_table_vcp_value_verify(ddca_dh, feature_code, hi_byte, lo_byte, NULL, NULL);
}

// UNPUBLISHED
/** Sets a table VCP value.
 *  Optionally returns the value set by reading the feature code after writing.
 *
 *  \param[in]   ddca_dh             display handle
 *  \param[in]   feature_code        feature code
 *  \param[in]   new_value           value to set
 *  \param[out]  verified_value_loc  where to return verified value
 *  \return      status code
 *
 *  \remark
 *  Verification is performed only it has been enabled (see #ddca_enable_verify()) and
 *  **verified_value** is set.
 *  \remark
 *  A verified value is returned if either the status code is either 0 (success),
 *  or **DDCRC_VERIFY**, i.e. the write succeeded but verification failed.
 */
// untested
DDCA_Status
ddca_set_table_vcp_value_verify(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Table_Vcp_Value *      table_value,
      DDCA_Table_Vcp_Value **     verified_value_loc)
{
    free_thread_error_detail();
    DDCA_Status rc = 0;

    DDCA_Any_Vcp_Value valrec;
    valrec.opcode = feature_code;
    valrec.value_type = DDCA_TABLE_VCP_VALUE;
    valrec.val.t.bytect = table_value->bytect;
    valrec.val.t.bytes  = table_value->bytes;  // copies pointer, not bytes

    if (verified_value_loc) {
       DDCA_Any_Vcp_Value * verified_single_value = NULL;
       rc = set_single_vcp_value(ddca_dh, &valrec, &verified_single_value);
       if (verified_single_value) {
          DDCA_Table_Vcp_Value * verified_table_value = calloc(1,sizeof(DDCA_Table_Vcp_Value));
          verified_table_value->bytect = verified_single_value->val.t.bytect;
          verified_table_value->bytes  = verified_single_value->val.t.bytes;
          free(verified_single_value);  // n. does not free bytes
          *verified_value_loc = verified_table_value;
       }
    }
    else {
       rc = set_single_vcp_value(ddca_dh, &valrec, NULL);
    }

    return rc;
}

DDCA_Status
ddca_set_table_vcp_value(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Table_Vcp_Value *      table_value)
{
   return ddca_set_table_vcp_value_verify(ddca_dh, feature_code, table_value, NULL);
}

// UNPUBLISHED
/** Sets a VCP value of any type.
 *  Optionally returns the values se by reading the feature code after writing.
 *
 *  \param[in]   ddca_dh        display handle
 *  \param[in]   feature_code   feature code
 *  \param[in]   new_value      value to set
 *  \param[out]  verified_value where to return verified value
 *  \return      status code
 *
 *  \remark
 *  Verification is performed only it has been enabled (see #ddca_enable_verify()) and
 *  **verified_value** is set.
 *  \remark
 *  A verified value is returned if either the status code is either 0 (success),
 *  or **DDCRC_VERIFY**, i.e. the write succeeded but verification failed.
 */
// untested for table values
DDCA_Status
ddca_set_any_vcp_value_verify(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Any_Vcp_Value *    new_value,
      DDCA_Any_Vcp_Value **   verified_value_loc)
{
   free_thread_error_detail();
   DDCA_Status rc = 0;

   if (verified_value_loc) {

      DDCA_Any_Vcp_Value * verified_single_value = NULL;
      rc = set_single_vcp_value(ddca_dh, new_value, &verified_single_value);
      if (verified_single_value) {
         *verified_value_loc = verified_single_value;       // do in need to make a copy for client?
      }
   }
   else {
      rc = set_single_vcp_value(ddca_dh, new_value, NULL);
   }

   return rc;
}

DDCA_Status
ddca_set_any_vcp_value(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Any_Vcp_Value *    new_value)
{
   return ddca_set_any_vcp_value_verify(ddca_dh, feature_code, new_value, NULL);
}



DDCA_Status
ddca_get_profile_related_values(
      DDCA_Display_Handle ddca_dh,
      char**              profile_values_string_loc)
{
   WITH_DH(ddca_dh,
      {
         bool debug = false;
         DBGMSF(debug, "Before dumpvcp_to_string_by_display_handle(), pprofile_values_string=%p,"
                       " *profile_values_string_loc=%p",
               profile_values_string_loc, *profile_values_string_loc);
         psc = dumpvcp_as_string(dh, profile_values_string_loc);
         DBGMSF(debug, "After dumpvcp_to_string_by_display_handle(), pprofile_values_string=%p,"
                       " *profile_values_string_loc=%p",
               profile_values_string_loc, *profile_values_string_loc);
         DBGMSF(debug, "*profile_values_string_loc = |%s|", *profile_values_string_loc);
      }
   );
}


DDCA_Status
ddca_set_profile_related_values(
      DDCA_Display_Handle  ddca_dh,
      char * profile_values_string)
{
   WITH_DH(ddca_dh,
      {
         free_thread_error_detail();
         Error_Info * ddc_excp = loadvcp_by_string(profile_values_string, dh);
         psc = (ddc_excp) ? ddc_excp->status_code : 0;
         if (ddc_excp) {
            save_thread_error_detail(error_info_to_ddca_detail(ddc_excp));
            errinfo_free(ddc_excp);
         }
      }
   );
}


//
// Async operation - experimental
//

DDCA_Status
ddca_start_get_any_vcp_value(
      DDCA_Display_Handle         ddca_dh,
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Vcp_Value_Type         call_type,
      DDCA_Notification_Func      callback_func)
{
   bool debug = false;
   DBGMSF(debug, "Starting. ddca_dh=%p, feature_code=0x%02x, call_type=%d",
                 ddca_dh, feature_code, call_type);
   // DDCA_Status rc = DDCRC_ARG;
   Error_Info * ddc_excp = NULL;

   WITH_DH(ddca_dh,
       {
          ddc_excp = start_get_vcp_value(dh, feature_code, call_type, callback_func);
          psc = (ddc_excp) ? ddc_excp->status_code : 0;
          errinfo_free(ddc_excp);
       }
      );
}


DDCA_Status
ddca_register_callback(
      DDCA_Notification_Func func,
      uint8_t                callback_options) // type is a placeholder
{
   return DDCRC_UNIMPLEMENTED;
}

DDCA_Status
ddca_queue_get_non_table_vcp_value(
      DDCA_Display_Handle      ddca_dh,
      DDCA_Vcp_Feature_Code    feature_code)
{
   return DDCRC_UNIMPLEMENTED;
}


// CFFI
DDCA_Status
ddca_pass_callback(
      Simple_Callback_Func  func,
      int                   parm)
{
   DBGMSG("parm=%d", parm);
   int callback_rc = func(parm+2);
   DBGMSG("returning %d", callback_rc);
   return callback_rc;
}

