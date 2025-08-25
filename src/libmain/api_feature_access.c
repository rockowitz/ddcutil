/** @file api_feature_access.c
 *
 *  Get, set, and format feature values
 */

// Copyright (C) 2015-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <assert.h>
#include <glib-2.0/glib.h>
#include <string.h>
#include <unistd.h>

#include "public/ddcutil_c_api.h"
#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_types.h"

#include "util/error_info.h"
#include "util/report_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/monitor_model_key.h"
#include "base/rtti.h"

#include "vcp/vcp_feature_values.h"

#include "dynvcp/dyn_feature_codes.h"

#include "ddc/ddc_dumpload.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"

#include "libmain/api_error_info_internal.h"
#include "libmain/api_base_internal.h"
#include "libmain/api_displays_internal.h"

#include "libmain/api_feature_access_internal.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_API;

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
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, true, "ddca_dh=%p, feature_code=0x%02x, valrec=%p",
                               ddca_dh, feature_code, valrec );
   DDCA_Status psc = API_PRECOND_RVALUE(valrec);
   if (psc != 0)
      goto bye;

   WITH_VALIDATED_DH3(ddca_dh, psc,  {
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
          // DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, psc,
          //       "valrec:  mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x",
          //       valrec->mh, valrec->ml, valrec->sh, valrec->sl);
       }
       else {
          psc = ddc_excp->status_code;
          save_thread_error_detail(error_info_to_ddca_detail(ddc_excp));
          ERRINFO_FREE_WITH_REPORT(ddc_excp, IS_DBGTRC(debug, DDCA_TRC_API));
          // DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, psc, "");
       }
    } );

bye:
   if (psc == 0)
      API_EPILOG_BEFORE_RETURN(debug, true, psc,
            "valrec:  mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x",
             valrec->mh, valrec->ml, valrec->sh, valrec->sl);
   else
      API_EPILOG_BEFORE_RETURN(debug, true, psc, "");
   return psc;
}


// untested
DDCA_Status
ddca_get_table_vcp_value(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Table_Vcp_Value ** table_value_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, true,
         "ddca_dh=%p, feature_code=0x%02x, table_value_loc=%p",
         ddca_dh, feature_code, table_value_loc);
   DDCA_Status psc = API_PRECOND_RVALUE(table_value_loc);
   if (psc != 0)
      goto bye;

   WITH_VALIDATED_DH3(ddca_dh, psc,
      {
         assert(table_value_loc);
         Error_Info * ddc_excp = NULL;
         Buffer * p_table_bytes = NULL;
         ddc_excp =  ddc_get_table_vcp_value(dh, feature_code, &p_table_bytes);
         psc = (ddc_excp) ? ddc_excp->status_code : 0;
         save_thread_error_detail(error_info_to_ddca_detail(ddc_excp));
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
         TRACED_ASSERT_IFF(psc==0, *table_value_loc);
         // DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, psc,
         //        "ddca_dh=%p->%s, feature_code=0x%02x, *table_value_loc=%p",
         //        ddca_dh, dh_repr(ddca_dh), feature_code, *table_value_loc);
      }
   );
bye:
   API_EPILOG_BEFORE_RETURN(debug, true, psc,
         "ddca_dh=%p->%s, feature_code=0x%02x, *table_value_loc=%p",
         ddca_dh, dh_repr(ddca_dh), feature_code, *table_value_loc);
   return psc;
}


static
DDCA_Status
ddci_get_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_Vcp_Value_Type    call_type,   // why is this needed?   look it up from dh and feature_code
      DDCA_Any_Vcp_Value **  pvalrec)
{

   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API,
          "ddca_dh=%p, feature_code=0x%02x, call_type=%d, pvalrec=%p",
          ddca_dh, feature_code, call_type, pvalrec);

   Error_Info * ddc_excp = NULL;
   DDCA_Status psc = 0;
   WITH_VALIDATED_DH3(ddca_dh, psc,
         {
               *pvalrec = NULL;
               ddc_excp = ddc_get_vcp_value(dh, feature_code, call_type, pvalrec);
               psc = (ddc_excp) ? ddc_excp->status_code : 0;
               save_thread_error_detail(error_info_to_ddca_detail(ddc_excp));
               errinfo_free(ddc_excp);
               DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, psc, "*pvalrec=%p", *pvalrec);
         }
   );
   DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, psc, "");
   return psc;
}


static DDCA_Status
get_value_type(
      DDCA_Display_Handle         ddca_dh,
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Vcp_Value_Type *       p_value_type)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "ddca_dh=%p, feature_code=0x%02x", ddca_dh, feature_code);

   DDCA_Status ddcrc = DDCRC_NOT_FOUND;
   DDCA_MCCS_Version_Spec vspec     = get_vcp_version_by_dh(ddca_dh);
   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
   if (pentry) {
      DDCA_Version_Feature_Flags flags = get_version_sensitive_feature_flags(pentry, vspec);
      // Version_Feature_Flags flags = feature_info->internal_feature_flags;
      // n. will default to NON_TABLE_VCP_VALUE if not a known code
      *p_value_type = (flags & DDCA_TABLE) ?  DDCA_TABLE_VCP_VALUE : DDCA_NON_TABLE_VCP_VALUE;
      ddcrc = 0;
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "");
   return ddcrc;
}


STATIC DDCA_Status
ddci_get_any_vcp_value_using_explicit_type(
       DDCA_Display_Handle         ddca_dh,
       DDCA_Vcp_Feature_Code       feature_code,
       DDCA_Vcp_Value_Type         call_type,
       DDCA_Any_Vcp_Value **       valrec_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
          "ddca_dh=%p, feature_code=0x%02x, call_type=%d, valrec_loc=%p",
          ddca_dh, feature_code, call_type, valrec_loc);
   assert(valrec_loc);
   *valrec_loc = NULL;

   DDCA_Any_Vcp_Value * valrec2 = NULL;
   DDCA_Status rc = ddci_get_vcp_value(ddca_dh, feature_code, call_type, &valrec2);
   if (rc == 0) {
      *valrec_loc = valrec2;
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP,  rc, "*valrec_loc=%p", *valrec_loc);
   ASSERT_IFF(rc == 0, *valrec_loc);
   return rc;
}


DDCA_Status
ddca_get_any_vcp_value_using_explicit_type(
       DDCA_Display_Handle         ddca_dh,
       DDCA_Vcp_Feature_Code       feature_code,
       DDCA_Vcp_Value_Type         call_type,
       DDCA_Any_Vcp_Value **       valrec_loc)
{
   bool debug = false;
   free_thread_error_detail();

   API_PROLOGX(debug, true,
          "Starting. ddca_dh=%p, feature_code=0x%02x, call_type=%d, valrec_loc=%p",
          ddca_dh, feature_code, call_type, valrec_loc);
   assert(valrec_loc);
   *valrec_loc = NULL;
   DDCA_Status ddcrc = ddci_get_any_vcp_value_using_explicit_type(
         ddca_dh, feature_code, call_type, valrec_loc);

   API_EPILOG_BEFORE_RETURN(debug, true, ddcrc, "*valrec_loc=%p", *valrec_loc);
   ASSERT_IFF(ddcrc == 0, *valrec_loc);
   return ddcrc;
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
   DBGMSF(debug, "Done.     Returning %s, *pvalrec=%p", psc_desc(rc), *pvalrec);
   return rc;
}
#endif


DDCA_Status
ddca_get_any_vcp_value_using_implicit_type(
       DDCA_Display_Handle         ddca_dh,
       DDCA_Vcp_Feature_Code       feature_code,
       DDCA_Any_Vcp_Value **       valrec_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, true, "feature_code = 0x%02x", feature_code);
   assert(valrec_loc);

   DDCA_Vcp_Value_Type call_type;
   DDCA_Status ddcrc = get_value_type(ddca_dh, feature_code, &call_type);
   if (ddcrc == 0) {
      ddcrc = ddci_get_any_vcp_value_using_explicit_type(
                 ddca_dh,
                 feature_code,
                 call_type,
                 valrec_loc);
   }
   ASSERT_IFF(ddcrc==0, *valrec_loc);
   API_EPILOG_BEFORE_RETURN(debug, true, ddcrc, "");
   return ddcrc;
}


void
ddca_free_table_vcp_value(
      DDCA_Table_Vcp_Value * table_value)
{
   if (table_value) {
      free(table_value->bytes);
      free(table_value);
   }
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


// not published
/** Produces a debugging report of a #DDCA_Any_Vcp_Value instance.
 *  The report is written to the current FOUT device.
 *  @param[in]  valrec  instance to report
 *  @param[in]  depth   logical indentation depth
 *  @since 0.9.0
 */
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


#ifdef DEPRECATED
// deprecated, does not support user defined features
/** Returns a string containing a formatted representation of the VCP value
 *  of a feature.  It is the responsibility of the caller to free this value.
 *
 *  @param[in]  ddca_dh             Display handle
 *  @param[in]  feature_code        VCP feature code
 *  @param[out] formatted_value_loc Address at which to return the formatted value
 *  @return     status code, 0 if success
 *  @since 0.9.0
 *  @deprecated Does not support user-supplied feature definitions
 */
DDCA_Status
ddca_get_formatted_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      char**                 formatted_value_loc)
{
   bool debug = false;
   API_PROLOG(debug, "feature_code=0x%02x, formatted_value_loc=%p",
                      feature_code, formatted_value_loc);

   DDCA_Status psc = API_PRECOND_RVALUE(formatted_value_loc);
   Error_Info * ddc_excp = NULL;
   if (psc != 0)
      goto bye;

   WITH_VALIDATED_DH3(ddca_dh, psc,
         {
               *formatted_value_loc = NULL;
               DDCA_MCCS_Version_Spec vspec      = get_vcp_version_by_dh(dh);
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
               // DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, psc, "");
         }
   )

bye:
   API_EPILOG_BEFORE_RETURN(debug, psc, "");
   return psc;
}
#endif


   /** Returns a formatted representation of a VCP value of any type
    *  It is the responsibility of the caller to free the returned string.
    *
    *  @param[in]  feature_code        VCP feature code
    *  @param[in]  vspec               MCCS version
    *  @param[in]  valrec              non-table VCP value
    *  @param[out] formatted_value_loc address at which to return the formatted value.
    *  @return                         status code, 0 if success
    *
    *  @remark
    *  If the returned status code is != 0, the string returned will
    *  contain an explanation of the error.
    *
    *  @since 0.9.0
    */
static DDCA_Status
ddci_format_any_vcp_value(
      DDCA_Vcp_Feature_Code    feature_code,
      DDCA_MCCS_Version_Spec   vspec,
      Monitor_Model_Key * mmid,
      DDCA_Any_Vcp_Value *     anyval,
      char **                  formatted_value_loc)
{
   bool debug = false;
   free_thread_error_detail();
   DBGTRC_STARTING(debug, TRACE_GROUP, "feature_code=0x%02x, vspec=%d.%d, mmid=%p -> %s",
                 feature_code,
                 vspec.major, vspec.minor,
                 mmid,
                 (mmid) ? mmk_repr(*mmid) : "NULL"
                 );
   assert(formatted_value_loc);
   DDCA_Status ddcrc = 0;

   *formatted_value_loc = NULL;
   Display_Feature_Metadata * dfm = NULL;

   if (!mmid) {
      *formatted_value_loc = g_strdup("Programming error. mmid not specified");
      ddcrc = DDCRC_ARG;
      goto bye;
   }

   dfm = dyn_get_feature_metadata_by_mmk_and_vspec(
            feature_code, *mmid, vspec, /* use_udf=*/ true, /*with_default=*/ true);
   if (!dfm) {
      ddcrc = DDCRC_ARG;
      *formatted_value_loc = g_strdup_printf("Unrecognized feature code 0x%02x", feature_code);
      goto bye;
   }
   DDCA_Version_Feature_Flags version_flags = dfm->version_feature_flags;

   if (!(version_flags & DDCA_READABLE)) {
      if (version_flags & DDCA_DEPRECATED)
         *formatted_value_loc = g_strdup_printf("Feature %02x is deprecated in MCCS %d.%d",
                                           feature_code, vspec.major, vspec.minor);
      else
         *formatted_value_loc = g_strdup_printf("Feature %02x is not readable", feature_code);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "%s", *formatted_value_loc);
      ddcrc = DDCRC_INVALID_OPERATION;
      goto bye;
   }

   // Version_Feature_Flags flags = feature_info->internal_feature_flags;
   // n. will default to NON_TABLE_VCP_VALUE if not a known code
   DDCA_Vcp_Value_Type call_type = (version_flags & DDCA_TABLE)
                                        ? DDCA_TABLE_VCP_VALUE
                                        : DDCA_NON_TABLE_VCP_VALUE;
   if (call_type != anyval->value_type) {
       *formatted_value_loc = g_strdup_printf(
             "Feature type in value does not match feature code");
       ddcrc = DDCRC_ARG;
       goto bye;
   }
   bool ok = dyn_format_feature_detail(dfm, vspec, anyval,formatted_value_loc);
   if (!ok) {
       ddcrc = DDCRC_ARG;    // ??
       assert(!*formatted_value_loc);
       *formatted_value_loc = g_strdup_printf("Unable to format value for feature 0x%02x", feature_code);
   }

bye:
   if (dfm)
      dfm_free(dfm);
   // API_EPILOG_BEFORE_RETURN(debug, NORESPECT_QUIESCE, ddcrc, "formatted_value_loc -> %s", *formatted_value_loc);
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "formatted_value_loc -> %s", *formatted_value_loc);
   // 7/2019: wrong, *formatted_value_loc always set, why did this ever work?
   // assert( (ddcrc==0 && *formatted_value_loc) || (ddcrc!=0 &&!*formatted_value_loc) );
   return ddcrc;
}


DDCA_Status
ddca_format_any_vcp_value_by_dref(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Display_Ref        ddca_dref,
      DDCA_Any_Vcp_Value *    valrec,
      char **                 formatted_value_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, NORESPECT_QUIESCE, "feature_code=0x%02x, ddca_dref=%p, valrec=%s",
             feature_code,
             ddca_dref,
             summarize_single_vcp_value(valrec) );
   assert(formatted_value_loc);
   DDCA_Status ddcrc = 0;
   WITH_VALIDATED_DR4(ddca_dref, ddcrc, DREF_VALIDATE_BASIC_ONLY,
         {
               if (debug || IS_TRACING()) {
                  DBGTRC_NOPREFIX(debug, TRACE_GROUP, "dref = %s", dref_repr_t(dref));
                  dbgrpt_display_ref(dref,true, 1);
               }
               ddcrc = ddci_format_any_vcp_value(
                         feature_code,
                         get_vcp_version_by_dref(dref),
                         // dref->vcp_version,
                         dref->mmid,
                         valrec,
                         formatted_value_loc);
               // no, if psc != 0, ddca_format_any_vcp_value() returns an error message
               // assert( (psc==0 && *formatted_value_loc) || (psc!=0 &&!*formatted_value_loc) );
         }
   )
   API_EPILOG_BEFORE_RETURN(debug, NORESPECT_QUIESCE, ddcrc, "*formatted_value_loc = %p -> |%s|",
                                         *formatted_value_loc, *formatted_value_loc);
   return ddcrc;
}


/** Returns a formatted representation of a non-table VCP value.
 *  It is the responsibility of the caller to free the returned string.
 *
 *  @param[in]  feature_code        VCP feature code
 *  @param[in]  vspec               MCCS version
 *  @param[in]  valrec              non-table VCP value
 *  @param[out] formatted_value_loc address at which to return the formatted value.
 *  @return                         status code, 0 if success
*/
static DDCA_Status
ddci_format_non_table_vcp_value(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_MCCS_Version_Spec      vspec,
      Monitor_Model_Key *         mmid,
      DDCA_Non_Table_Vcp_Value *  valrec,
      char **                     formatted_value_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "feature_code=0x%02x, vspec=%d.%d, mmid=%s, formatted_value_loc=%p",
             feature_code,
             vspec.major, vspec.minor,
             (mmid) ? mmk_repr(*mmid) : "NULL",
             formatted_value_loc);
   DDCA_Status ddcrc = API_PRECOND_RVALUE(formatted_value_loc);
   if (ddcrc != 0) {
      DBGTRC(debug, DDCA_TRC_API, "API_PRECOND_RVALUE() returned %d", ddcrc);
      // DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, ddcrc, "");
      goto bye;
   }

   *formatted_value_loc = NULL;

   // free_thread_error_detail();   // unnecessary, done by ddca_format_any_vcp_value();
   DDCA_Any_Vcp_Value anyval;
   anyval.opcode = feature_code;
   anyval.value_type = DDCA_NON_TABLE_VCP_VALUE;
   anyval.val.c_nc.mh = valrec->mh;
   anyval.val.c_nc.ml = valrec->ml;
   anyval.val.c_nc.sh = valrec->sh;
   anyval.val.c_nc.sl = valrec->sl;

   ddcrc =  ddci_format_any_vcp_value(
                          feature_code, vspec, mmid, &anyval, formatted_value_loc);
   // assert( (ddcrc==0 &&*formatted_value_loc) || (ddcrc!=0 && !*formatted_value_loc) );

#ifdef OUT
   if (ddcrc == 0)
      API_EPILOG_BEFORE_RETURN(debug, false, ddcrc,
            "*formatted_value_loc=%p->%s", *formatted_value_loc, *formatted_value_loc);
   else
      API_EPILOG_BEFORE_RETURN(debug, false, ddcrc,
            "*formatted_value_loc=%p", *formatted_value_loc);
#endif

   // if (ddcrc == 0)
   //    DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, ddcrc,
   //           "*formatted_value_loc=%p->%s", *formatted_value_loc, *formatted_value_loc);
   // else
   //    DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, ddcrc,  "*formatted_value_loc=%p", *formatted_value_loc);

bye:
   // DISABLE_API_CALL_TRACING();
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "");
   return ddcrc;
}

DDCA_Status
ddca_format_non_table_vcp_value_by_dref(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Display_Ref            ddca_dref,
      DDCA_Non_Table_Vcp_Value *  valrec,
      char **                     formatted_value_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE, "feature_code=0x%02x, ddca_dref=%p",
                          feature_code, ddca_dref);
   assert(formatted_value_loc);
   DDCA_Status ddcrc = 0;
   WITH_VALIDATED_DR4(ddca_dref, ddcrc, DREF_VALIDATE_BASIC_ONLY,
         {
               if (debug || IS_TRACING()) {
                  DBGTRC_NOPREFIX(debug, TRACE_GROUP, "dref = %s", dref_repr_t(dref));
                  dbgrpt_display_ref(dref, true, 1);
               }
               ddcrc = ddci_format_non_table_vcp_value(
                         feature_code,
                         // dref->vcp_version,
                         get_vcp_version_by_dref(dref),
                         dref->mmid,
                         valrec,
                         formatted_value_loc);
               // assert( (psc==0 &&*formatted_value_loc) || (psc!=0 && !*formatted_value_loc) );
         }
   )

   API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, ddcrc, "*formatted_value_loc = %p -> |%s|",
                                               *formatted_value_loc, *formatted_value_loc);
   return ddcrc;
}


// NEVER PUBLISHED, USED INTERNALLY
/** Returns a formatted representation of a table VCP value.
 *  It is the responsibility of the caller to free the returned string.
 *
 *  @param[in]  feature_code        VCP feature code
 *  @param[in]  vspec               MCCS version
 *  @param[in]  table_value         table VCP value
 *  @param[out] formatted_value_loc address at which to return the formatted value.
 *  @return                         status code, 0 if success
 */
static DDCA_Status
ddci_format_table_vcp_value(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_MCCS_Version_Spec  vspec,
      Monitor_Model_Key * mmid,
      DDCA_Table_Vcp_Value *  table_value,
      char **                 formatted_value_loc)
{
   // free_thread_error_detail();   // unnecessary, done by ddca_format_any_vcp_value();
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   // API_PROLOG(debug, "");
   DDCA_Any_Vcp_Value anyval;
   anyval.opcode = feature_code;
   anyval.value_type = DDCA_TABLE_VCP_VALUE;
   anyval.val.t.bytect = table_value->bytect;
   anyval.val.t.bytes  = table_value->bytes;   // n. copying pointer, not duplicating bytes

   DDCA_Status ddcrc = ddci_format_any_vcp_value(
             feature_code, vspec, mmid, &anyval, formatted_value_loc);
   // API_EPILOG_BEFORE_RETURN(debug, false, ddcrc, "");
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "");
   return ddcrc;
}


DDCA_Status
ddca_format_table_vcp_value_by_dref(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Display_Ref        ddca_dref,
      DDCA_Table_Vcp_Value *  table_value,
      char **                 formatted_value_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE, "feature_code=0x%02x, ddca_dref=%p",
                                           feature_code, ddca_dref);
   assert(formatted_value_loc);
   DDCA_Status ddcrc = 0;
   WITH_VALIDATED_DR4(ddca_dref, ddcrc, DREF_VALIDATE_BASIC_ONLY,
         {
               if (debug || IS_TRACING()) {
                  DBGTRC_NOPREFIX(debug, TRACE_GROUP, "dref = %s", dref_repr_t(dref));
                  dbgrpt_display_ref(dref,true,1);
               }
              ddcrc = ddci_format_table_vcp_value(
                         feature_code,
                         // dref->vcp_version,
                         get_vcp_version_by_dref(dref),
                         dref->mmid,
                         table_value,
                         formatted_value_loc);
         }
   )
   API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, ddcrc,
                               "*formatted_value_loc = %p -> |%s|",
                               *formatted_value_loc, *formatted_value_loc);
   return ddcrc;
}


static
DDCA_Status
ddci_set_single_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Any_Vcp_Value *   valrec,
      DDCA_Any_Vcp_Value **  verified_value_loc)  // NULL => do not return value
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API, "ddca_dh=%p, valrec=%p, verified_value_loc = %p",
                               ddca_dh, valrec, verified_value_loc);
   DDCA_Status psc = 0;
   free_thread_error_detail();
   WITH_VALIDATED_DH3(ddca_dh, psc, {
         Error_Info * ddc_excp = ddc_set_verified_vcp_value_with_retry(dh, valrec, verified_value_loc);
         psc = (ddc_excp) ? ddc_excp->status_code : 0;
         save_thread_error_detail(error_info_to_ddca_detail(ddc_excp));
         ERRINFO_FREE_WITH_REPORT(ddc_excp, IS_DBGTRC(debug, DDCA_TRC_API));
      } );
   DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, psc, "");
   return psc;
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
static
DDCA_Status
ddci_set_continuous_vcp_value_verify(
      DDCA_Display_Handle   ddca_dh,
      DDCA_Vcp_Feature_Code feature_code,
      uint16_t              new_value,
      uint16_t *            verified_value_loc)
{
   DDCA_Status rc = 0;

   DDCA_Any_Vcp_Value valrec;
   valrec.opcode = feature_code;
   valrec.value_type = DDCA_NON_TABLE_VCP_VALUE;
   valrec.val.c_nc.sh = (new_value >> 8) & 0xff;
   valrec.val.c_nc.sl = new_value & 0xff;

   if (verified_value_loc) {
      DDCA_Any_Vcp_Value * verified_single_value;
      rc = ddci_set_single_vcp_value(ddca_dh, &valrec, &verified_single_value);
      if (verified_single_value)
      *verified_value_loc = VALREC_CUR_VAL(verified_single_value);
   }
   else {
      rc = ddci_set_single_vcp_value(ddca_dh, &valrec, NULL);
   }

   return rc;
}

#ifdef REMOVED
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
   bool debug = false;
   API_PROLOG(debug, "feature_code=0x%02x", feature_code);
   DDCA_Status ddcrc = ddci_set_continuous_vcp_value_verify(ddca_dh, feature_code, new_value, NULL);
   API_EPILOG_BEFORE_RETURN(debug, ddcrc, "");
   return ddcrc;
}
#endif

#ifdef REMOVED
/** \deprecated */
DDCA_Status
ddca_set_simple_nc_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      Byte                   new_value)
{
   bool debug = false;
   API_PROLOG(debug, "feature_code=0x%02x", feature_code);
   DDCA_Status ddcrc = ddci_set_continuous_vcp_value_verify(ddca_dh, feature_code, new_value, NULL);
   API_EPILOG_BEFORE_RETURN(debug, ddcrc, "");
   return ddcrc;
}
#endif


// UNPUBLISHED
/** Sets a non-table VCP value by specifying it's high and low bytes individually.
 *  Optionally returns the values set by reading the feature code after writing.
 *
 *  @param[in]   ddca_dh             display handle
 *  @param[in]   feature_code        feature code
 *  @param[in]   hi_byte             high byte of new value
 *  @param[in]   lo_byte             low byte of new value
 *  @param[out]  p_verified_hi_byte  where to return high byte of verified value
 *  @param[out]  p_verified_lo_byte  where to return low byte of verified value
 *  @return      status code
 *
 *  @remark
 *  Either both **verified_hi_byte_loc** and **verified_lo_byte_loc** should be
 *  set, or neither. Otherwise, status code **DDCRC_ARG** is returned.
 *  @remark
 *  Verification is performed only it has been enabled (see #ddca_enable_verify()) and
 *  both **verified_hi_byte** and **verified_lo_byte** are set.
 *  @remark
 *  Verified values are returned if the status code is either 0 (success),
 *  or **DDCRC_VERIFY**, i.e. the write succeeded but verification failed.
 */
static
DDCA_Status
ddci_set_non_table_vcp_value_verify(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      Byte                   hi_byte,
      Byte                   lo_byte,
      Byte *                 verified_hi_byte_loc,
      Byte *                 verified_lo_byte_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API,
          "ddca_dh=%p, feature_code=0x%02x, hi_byte=0x%02x, lo_byte=0x%02x",
          ddca_dh, feature_code, hi_byte, lo_byte);
   DDCA_Status rc = 0;
   free_thread_error_detail();
   if ( ( verified_hi_byte_loc && !verified_lo_byte_loc) ||
        (!verified_hi_byte_loc &&  verified_lo_byte_loc )
      )
   {
      rc = DDCRC_ARG;
   }
   else {
      // unwrap into 2 cases to clarify logic and avoid compiler warning

      if (verified_hi_byte_loc) {
         uint16_t verified_c_value = 0;
         rc = ddci_set_continuous_vcp_value_verify(
                             ddca_dh,
                             feature_code, hi_byte << 8 | lo_byte,
                             &verified_c_value);
         *verified_hi_byte_loc = verified_c_value >> 8;
         *verified_lo_byte_loc = verified_c_value & 0xff;
      }
      else {
         rc = ddci_set_continuous_vcp_value_verify(
                             ddca_dh,
                             feature_code, hi_byte << 8 | lo_byte,
                             NULL);
      }
   }
   DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, rc, "");
   return rc;
}

DDCA_Status
ddca_set_non_table_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      Byte                   hi_byte,
      Byte                   lo_byte)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE, "feature_code=0x%02x", feature_code);
   DDCA_Status ddcrc = ddci_set_non_table_vcp_value_verify(ddca_dh, feature_code, hi_byte, lo_byte, NULL, NULL);
   API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, ddcrc, "");
   return ddcrc;
}

// UNPUBLISHED
/** Sets a table VCP value.
 *  Optionally returns the value set by reading the feature code after writing.
 *
 *  @param[in]   ddca_dh             display handle
 *  @param[in]   feature_code        feature code
 *  @param[in]   new_value           value to set
 *  @param[out]  verified_value_loc  where to return verified value
 *  @return      status code
 *
 *  @remark
 *  Verification is performed only it has been enabled (see #ddca_enable_verify()) and
 *  **verified_value** is set.
 *  @remark
 *  A verified value is returned if either the status code is either 0 (success),
 *  or **DDCRC_VERIFY**, i.e. the write succeeded but verification failed.
 */
// untested
static
DDCA_Status
ddci_set_table_vcp_value_verify(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Table_Vcp_Value *      table_value,
      DDCA_Table_Vcp_Value **     verified_value_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API, "feature_code=0x%02x", feature_code);
    DDCA_Status rc = 0;

    DDCA_Any_Vcp_Value valrec;
    valrec.opcode = feature_code;
    valrec.value_type = DDCA_TABLE_VCP_VALUE;
    valrec.val.t.bytect = table_value->bytect;
    valrec.val.t.bytes  = table_value->bytes;  // copies pointer, not bytes

    if (verified_value_loc) {
       DDCA_Any_Vcp_Value * verified_single_value = NULL;
       rc = ddci_set_single_vcp_value(ddca_dh, &valrec, &verified_single_value);
       if (verified_single_value) {
          DDCA_Table_Vcp_Value * verified_table_value = calloc(1,sizeof(DDCA_Table_Vcp_Value));
          verified_table_value->bytect = verified_single_value->val.t.bytect;
          verified_table_value->bytes  = verified_single_value->val.t.bytes;
          free(verified_single_value);  // n. does not free bytes
          *verified_value_loc = verified_table_value;
       }
    }
    else {
       rc = ddci_set_single_vcp_value(ddca_dh, &valrec, NULL);
    }

    DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, rc, "");
    return rc;
}

DDCA_Status
ddca_set_table_vcp_value(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Table_Vcp_Value *  table_value)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE, "feature_code=0x%02x", feature_code);
   DDCA_Status ddcrc = ddci_set_table_vcp_value_verify(ddca_dh, feature_code, table_value, NULL);
   API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, ddcrc, "");
   return ddcrc;
}

// UNPUBLISHED
/** Sets a VCP value of any type.
 *  Optionally returns the values se by reading the feature code after writing.
 *
 *  @param[in]   ddca_dh        display handle
 *  @param[in]   feature_code   feature code
 *  @param[in]   new_value      value to set
 *  @param[out]  verified_value where to return verified value
 *  @return      status code
 *
 *  @remark
 *  Verification is performed only it has been enabled (see #ddca_enable_verify()) and
 *  **verified_value** is set.
 *  @remark
 *  A verified value is returned if either the status code is either 0 (success),
 *  or **DDCRC_VERIFY**, i.e. the write succeeded but verification failed.
 */
// untested for table values
static
DDCA_Status
ddci_set_any_vcp_value_verify(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Any_Vcp_Value *    new_value,
      DDCA_Any_Vcp_Value **   verified_value_loc)
{
   DDCA_Status rc = 0;

   if (verified_value_loc) {
      DDCA_Any_Vcp_Value * verified_single_value = NULL;
      rc = ddci_set_single_vcp_value(ddca_dh, new_value, &verified_single_value);
      if (verified_single_value) {
         *verified_value_loc = verified_single_value;       // do in need to make a copy for client?
      }
   }
   else {
      rc = ddci_set_single_vcp_value(ddca_dh, new_value, NULL);
   }

   return rc;
}


DDCA_Status
ddca_set_any_vcp_value(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Any_Vcp_Value *    new_value)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE, "feature_code=0x%02x", feature_code);
   DDCA_Status ddcrc = ddci_set_any_vcp_value_verify(ddca_dh, feature_code, new_value, NULL);
   API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, ddcrc, "");
   return ddcrc;
}


DDCA_Status
ddca_get_profile_related_values(
      DDCA_Display_Handle ddca_dh,
      char**              profile_values_string_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE,
          "ddca_dh=%p, profile_values_string_loc=%p",
          ddca_dh, profile_values_string_loc);
   DDCA_Status psc = API_PRECOND_RVALUE(profile_values_string_loc);
   if (psc != 0)
      goto bye;

   WITH_VALIDATED_DH3(ddca_dh, psc,
      {
         psc = dumpvcp_as_string(dh, profile_values_string_loc);
         TRACED_ASSERT_IFF(psc==0, *profile_values_string_loc);
         DBGTRC_RET_DDCRC(debug, TRACE_GROUP, psc,
                                 "*profile_values_string_loc=%p -> %s",
                                 *profile_values_string_loc,
                                 *profile_values_string_loc);
      }
   );
bye:
   API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, psc, "");
   return psc;
}


DDCA_Status
ddca_set_profile_related_values(
      DDCA_Display_Handle  ddca_dh,
      char *               profile_values_string)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE, "ddca_h=%p, profile_values_string = %s",
                     ddca_dh, profile_values_string);
   DDCA_Status psc = 0;
   WITH_VALIDATED_DH3(ddca_dh, psc,
      {
         Error_Info * ddc_excp = loadvcp_by_string(profile_values_string, dh);
         psc = (ddc_excp) ? ddc_excp->status_code : 0;
         if (ddc_excp) {
            save_thread_error_detail(error_info_to_ddca_detail(ddc_excp));
            errinfo_free(ddc_excp);
         }
         DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, psc, "");
      }
   );
   API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, psc, "");
   return psc;
}


#ifdef REMOVED
//
// Vestiges of old experimental async API.
// Never published, retained for ABI compatibility.
//

DDCA_Status
ddca_start_get_any_vcp_value(
      DDCA_Display_Handle         ddca_dh,
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Vcp_Value_Type         call_type,
      DDCA_Notification_Func      callback_func)
{
   return DDCRC_UNIMPLEMENTED;
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
#endif


#ifdef REMOVED
//
// CFFI
//
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
#endif



#ifdef FUTURE
// which header file would this go in?
void init_api_access_feature_codes() {
   rtti_func_name_table_add(ddci_format_any_vcp_value, "dyn_format_nontable_feature_detail_dfm");
   // dbgrpt_func_name_table(0);
}
#endif


void init_api_feature_access() {
   // DBGMSG("Executing");
   RTTI_ADD_FUNC(ddca_get_non_table_vcp_value);
   RTTI_ADD_FUNC(ddca_set_non_table_vcp_value);
   RTTI_ADD_FUNC(ddci_set_single_vcp_value);
   RTTI_ADD_FUNC(ddca_check_brightness_support);
}


DDCA_Status
ddca_check_brightness_support(
      DDCA_Display_Handle  ddca_dh,
      bool*               is_supported,
      uint16_t*           current_value,
      uint16_t*           max_value)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, true, "ddca_dh=%p, is_supported=%p, current_value=%p, max_value=%p",
                               ddca_dh, is_supported, current_value, max_value);

   DDCA_Status psc = API_PRECOND_RVALUE(is_supported) || API_PRECOND_RVALUE(current_value) || API_PRECOND_RVALUE(max_value);
   if (psc != 0)
      goto bye;

   WITH_VALIDATED_DH3(ddca_dh, psc, {
      // 1. Read current brightness value
      Parsed_Nontable_Vcp_Response * parsed_response;
      Error_Info * ddc_excp = ddc_get_nontable_vcp_value(dh, 0x10, &parsed_response);
      if (ddc_excp) {
         psc = ddc_excp->status_code;
         save_thread_error_detail(error_info_to_ddca_detail(ddc_excp));
         ERRINFO_FREE_WITH_REPORT(ddc_excp, IS_DBGTRC(debug, DDCA_TRC_API));
         goto bye;
      }

      *current_value = RESPONSE_CUR_VALUE(parsed_response);
      *max_value = RESPONSE_MAX_VALUE(parsed_response);

      // Check if maximum value is 0
      if (*max_value == 0) {
         *is_supported = false;
         psc = 0;
         goto bye;
      }

      // 2. Calculate test value (minimum change)
      uint16_t test_value;
      if (*current_value < *max_value) {
         test_value = *current_value + 1;
      } else {
         test_value = *current_value - 1;
      }

      // 3. Save original verification setting
      bool original_verify = ddc_get_verify_setvcp();

      // 4. Disable verification to avoid verification failures affecting the test
      ddc_set_verify_setvcp(false);

      // 5. Attempt to set test value
      ddc_excp = ddc_set_nontable_vcp_value(dh, 0x10, test_value);
      if (ddc_excp) {
         // Set failed, brightness control not supported
         *is_supported = false;
         psc = 0;  // Test completed, return success
         ddc_set_verify_setvcp(original_verify);
         ERRINFO_FREE_WITH_REPORT(ddc_excp, IS_DBGTRC(debug, DDCA_TRC_API));
         goto bye;
      }

      // 6. Verify if the set operation actually took effect (key improvement!)
      usleep(50000);  // 50ms wait

      Parsed_Nontable_Vcp_Response * verify_parsed_response;
      ddc_excp = ddc_get_nontable_vcp_value(dh, 0x10, &verify_parsed_response);

      if (!ddc_excp) {
         uint16_t verify_value = RESPONSE_CUR_VALUE(verify_parsed_response);
         // Verify if the set value actually took effect
         *is_supported = verify_value == test_value;
         free(verify_parsed_response);
      } else {
         // Read verification failed, but set command succeeded, assume brightness control supported
         *is_supported = true;
         ERRINFO_FREE_WITH_REPORT(ddc_excp, IS_DBGTRC(debug, DDCA_TRC_API));
      }

      // 7. Restore original value
      ddc_excp = ddc_set_nontable_vcp_value(dh, 0x10, *current_value);

      // 8. Restore verification setting
      ddc_set_verify_setvcp(original_verify);

      if (ddc_excp) {
         // Restore failed, but set succeeded, so brightness control is supported
         *is_supported = true;
         psc = 0;  // Test completed, return success
         ERRINFO_FREE_WITH_REPORT(ddc_excp, IS_DBGTRC(debug, DDCA_TRC_API));
         goto bye;
      }

      // Free memory
      free(parsed_response);

      psc = 0;  // Test completed, return success
   });

bye:
   if (psc == 0)
      API_EPILOG_BEFORE_RETURN(debug, true, psc,
            "is_supported=%s, current_value=%d, max_value=%d",
             *is_supported ? "true" : "false", *current_value, *max_value);
   else
      API_EPILOG_BEFORE_RETURN(debug, true, psc, "");
   return psc;
}