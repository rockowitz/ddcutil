/** @file ddc_output.c
 */

// Copyright (C) 2014-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#include <config.h>

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <string.h>
#include <time.h>

#include "util/error_info.h"
#include "util/report_util.h"
/** \endcond */

#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/linux_errno.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/sleep.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_strategy_dispatcher.h"

#ifdef USE_USB
#include "usb/usb_displays.h"
#include "usb/usb_vcp.h"
#endif

#include "vcp/parse_capabilities.h"

#include "dynvcp/dyn_feature_set.h"
#include "dynvcp/dyn_feature_codes.h"

#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_vcp_version.h"

#include "ddc/ddc_output.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDC;


// Standard format strings for reporting feature codes.
const char* FMT_CODE_NAME_DETAIL_WO_NL = "VCP code 0x%02x (%-30s): %s";
const char* FMT_CODE_NAME_DETAIL_W_NL  = "VCP code 0x%02x (%-30s): %s\n";

//
// VCP Feature Table inquiry
//

#ifdef OLD
/* Checks if a feature is a table type feature, given
 * the VCP version of a monitor.
 *
 * For a handful of features, whether the feature is of
 * type Table varies based on the the VCP version. This
 * function encapsulates that check.
 *
 * Arguments:
 *    frec    pointer to VCP feature table entry
 *    dh      display handle of monitor
 *
 * Returns:
 *    true if the specified feature is a table feature,
 *    false if not
 */
bool
is_table_feature_by_dh(
      VCP_Feature_Table_Entry *  frec,
      Display_Handle *           dh)
{
   // bool debug = false;
   bool result = false;
   DDCA_MCCS_Version_Spec vcp_version = get_vcp_version_by_dh(dh);
   DDCA_Version_Feature_Flags feature_flags = get_version_sensitive_feature_flags(frec, vcp_version);
   assert(feature_flags);
   result = (feature_flags & DDCA_TABLE);
   // DBGMSF(debug, "returning: %d", result);
   return result;
}
#endif


#ifdef FUTURE
// For possible future use - currently unused
Public_Status_Code
check_valid_operation_by_feature_rec_and_version(
      VCP_Feature_Table_Entry *   frec,
      DDCA_MCCS_Version_Spec      vcp_version,
      DDCA_Version_Feature_Flags  operation_flags)
{
   DDCA_Version_Feature_Flags feature_flags
      = get_version_sensitive_feature_flags(frec, vcp_version);
   assert(feature_flags);
   ushort rwflags   = operation_flags & DDCA_RW;
   ushort typeflags = operation_flags & (DDCA_NORMAL_TABLE | DDCA_CONT | DDCA_NC);
   Public_Status_Code result = DDCRC_INVALID_OPERATION;
   if ( (feature_flags & rwflags) && (feature_flags & typeflags) )
      result = 0;
   return result;
}


Public_Status_Code
check_valid_operation_by_feature_id_and_dh(
      Byte                  feature_id,
      Display_Handle *      dh,
      DDCA_Version_Feature_Flags operation_flags)
{
   Public_Status_Code result = 0;
   VCP_Feature_Table_Entry * frec = vcp_find_feature_by_hexid(feature_id);
   if (!frec)
      result = DDCRC_UNKNOWN_FEATURE;
   else {
      DDCA_MCCS_Version_Spec vcp_version = get_vcp_version_by_dh(dh);
      result = check_valid_operation_by_feature_rec_and_version(frec, vcp_version, operation_flags);
   }
   return result;
}
#endif


//
// Get raw VCP feature values
//

/* Get the raw value (i.e. bytes) for a feature table entry.
 *
 * Convert and refine status codes, issue error messages.
 *
 * Arguments;
 *    dh                  display handle
 *    frec                pointer to VCP_Feature_Table_Entry for feature
 *    ignore_unsupported  if false, issue error message for unsupported feature
 *    pvalrec             location where to return pointer to feature value
 *    msg_fh              file handle for error messages
 *
 * Returns:
 *    status code
 */
static Error_Info *
get_raw_value_for_feature_metadata(
      Display_Handle *           dh,
      Display_Feature_Metadata * frec,
      bool                       ignore_unsupported,
      DDCA_Any_Vcp_Value **      pvalrec,
      FILE *                     msg_fh)
{
   assert(frec);
   assert(dh);
   assert(dh->dref);

   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "frec=%p, feature_code=0x%02x", frec, (frec) ? frec->feature_code : 0x00);

   Error_Info * ddc_excp = NULL;
   char * feature_name = frec->feature_name;
   Byte feature_code = frec->feature_code;
   bool is_table_feature = frec->version_feature_flags & DDCA_TABLE;
   DDCA_Vcp_Value_Type feature_type = (is_table_feature) ? DDCA_TABLE_VCP_VALUE : DDCA_NON_TABLE_VCP_VALUE;
   DDCA_Output_Level output_level = get_output_level();
   DDCA_Any_Vcp_Value * valrec = NULL;
   if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
#ifdef USE_USB
     Public_Status_Code
     psc = usb_get_vcp_value(
              dh,
              feature_code,
              feature_type,
              &valrec);
     if (psc != 0)
        ddc_excp = errinfo_new(psc, __func__, NULL);
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
   }
   else {
      ddc_excp = ddc_get_vcp_value(
              dh,
              feature_code,
              feature_type,
              &valrec);
   }
   ASSERT_IFF( ddc_excp, !valrec);

   // For now, only regard -EIO as unsupported feature for the
   // single model on which this has been observed
   if (ERRINFO_STATUS(ddc_excp) ==  -EIO &&
       dh->dref->io_path.io_mode == DDCA_IO_I2C &&
       streq(dh->dref->pedid->mfg_id, "DEL")    &&
       streq(dh->dref->pedid->model_name, "AW3418DW") )
   {
      // Dell AW3418DW returns -EIO for unsupported features
      // (except for feature 0x00, which returns mh=ml=sh=sl=0) (2/2019)
      if (!ignore_unsupported) {
         f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                        feature_code, feature_name, "Unsupported feature code (EIO)");
      }
      // psc = DDCRC_DETERMINED_UNSUPPORTED;
      COUNT_STATUS_CODE(DDCRC_DETERMINED_UNSUPPORTED);
      ddc_excp = errinfo_new_with_cause(
                   DDCRC_DETERMINED_UNSUPPORTED, ddc_excp, __func__, "EIO");
   }

   else {
      Public_Status_Code psc = ERRINFO_STATUS(ddc_excp);
      switch( psc ) {
      case 0:
         break;

      case DDCRC_DDC_DATA:           // was DDCRC_INVALID_DATA
         if (output_level >= DDCA_OL_NORMAL)
            f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                            feature_code, feature_name, "Invalid response");
         break;

      case DDCRC_NULL_RESPONSE:
         // for unsupported features, some monitors return null response rather than a valid response
         // with unsupported feature indicator set
         if (!ignore_unsupported) {
            f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                           feature_code, feature_name, "Unsupported feature code (Null response)");
         }
         COUNT_STATUS_CODE(DDCRC_DETERMINED_UNSUPPORTED);
         // psc = DDCRC_DETERMINED_UNSUPPORTED;
         ddc_excp = errinfo_new_with_cause(
                     DDCRC_DETERMINED_UNSUPPORTED, ddc_excp, __func__, "DDC NULL Response");
         break;

      case DDCRC_READ_ALL_ZERO:
         // treat as invalid response if not table type?
         if (!ignore_unsupported) {
            f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                           feature_code, feature_name, "Unsupported feature code (All zero response)");
         }
         // psc = DDCRC_DETERMINED_UNSUPPORTED;
         COUNT_STATUS_CODE(DDCRC_DETERMINED_UNSUPPORTED);
         ddc_excp = errinfo_new_with_cause(
                     DDCRC_DETERMINED_UNSUPPORTED, ddc_excp, __func__, "MH=ML=SH=SL=0");
         break;

#ifdef FUTURE
      case -EIO:
         // Dell AW3418DW returns -EIO for unsupported features
         // (except for feature 0x00, which returns mh=ml=sh=sl=0) (2/2019)
         if (!ignore_unsupported) {
            f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                           feature_code, feature_name, "Unsupported feature code (EIO)");
         }
         psc = DDCRC_DETERMINED_UNSUPPORTED;
         COUNT_STATUS_CODE(DDCRC_DETERMINED_UNSUPPORTED);
         break;
#endif

      case DDCRC_RETRIES:
         f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                         feature_code, feature_name, "Maximum retries exceeded");
         break;

      case DDCRC_REPORTED_UNSUPPORTED:
         if (!ignore_unsupported) {
            f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                            feature_code, feature_name, "Unsupported feature code");
         }
         break;

      case DDCRC_DETERMINED_UNSUPPORTED:
         if (!ignore_unsupported) {
            char text[100];
            g_snprintf(text, 100, "Unsupported feature code (%s)", ddc_excp->detail);
            f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                            feature_code, feature_name, text);
         }
         break;

      default:
         {
            char buf[200];
            snprintf(buf, 200, "Invalid response. status code=%s, %s", psc_name_code(psc), dh_repr(dh));
            f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                             feature_code, feature_name, buf);
         }
      }
   }

   *pvalrec = valrec;
   ASSERT_IFF(!ddc_excp, *pvalrec);;
   DBGTRC_RET_ERRINFO_STRUCT(debug, TRACE_GROUP, ddc_excp, pvalrec, dbgrpt_single_vcp_value);
   return ddc_excp;
}


#ifdef IN_PROGRESS
Public_Status_Code
get_raw_value_for_feature_metadata_dfm(
      Display_Handle *           dh,
      DDCA_Feature_Metadata *    frec,
      bool                       ignore_unsupported,
      DDCA_Any_Vcp_Value ** pvalrec,
      FILE *                     msg_fh)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting", NULL);

   assert(dh);
   assert(dh->dref);

   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;

   // DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_dh(dh);
   // char * feature_name = get_version_sensitive_feature_name(frec, vspec);
   char * feature_name = frec->feature_name;

   Byte feature_code = frec->feature_code;
   // bool is_table_feature = is_table_feature_by_dh(frec, dh);
   bool is_table_feature = frec->feature_flags & DDCA_TABLE;
   DDCA_Vcp_Value_Type feature_type = (is_table_feature) ? DDCA_TABLE_VCP_VALUE : DDCA_NON_TABLE_VCP_VALUE;
   DDCA_Output_Level output_level = get_output_level();
   DDCA_Any_Vcp_Value * valrec = NULL;
   if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
#ifdef USE_USB
     psc = usb_get_vcp_value(
              dh,
              feature_code,
              feature_type,
              &valrec);
     if (psc != 0)
        ddc_excp = errinfo_new(psc, __func__);
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
   }
   else {
      ddc_excp = ddc_get_vcp_value(
              dh,
              feature_code,
              feature_type,
              &valrec);
      psc = ERRINFO_STATUS(ddc_excp);
   }
   assert ( (psc==0 && valrec) || (psc!=0 && !valrec) );

   switch(psc) {
   case 0:
      break;

   case DDCRC_DDC_DATA:           // was DDCRC_INVALID_DATA
      if (output_level >= DDCA_OL_NORMAL)
         f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                         feature_code, feature_name, "Invalid response");
      break;

   case DDCRC_NULL_RESPONSE:
      // for unsupported features, some monitors return null response rather than a valid response
      // with unsupported feature indicator set
      if (!ignore_unsupported) {
         f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                        feature_code, feature_name, "Unsupported feature code (Null response)");
      }
      COUNT_STATUS_CODE(DDCRC_DETERMINED_UNSUPPORTED);
      psc = DDCRC_DETERMINED_UNSUPPORTED;
      break;

   case DDCRC_READ_ALL_ZERO:
      // treat as invalid response if not table type?
      if (!ignore_unsupported) {
         f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                        feature_code, feature_name, "Unsupported feature code (All zero response)");
      }
      psc = DDCRC_DETERMINED_UNSUPPORTED;
      COUNT_STATUS_CODE(DDCRC_DETERMINED_UNSUPPORTED);
      break;

   case DDCRC_RETRIES:
      f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                      feature_code, feature_name, "Maximum retries exceeded");
      break;

   case DDCRC_REPORTED_UNSUPPORTED:
   case DDCRC_DETERMINED_UNSUPPORTED:
      if (!ignore_unsupported) {
         f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                         feature_code, feature_name, "Unsupported feature code");
      }
      break;

   default:
   {
      char buf[200];
      snprintf(buf, 200, "Invalid response. status code=%s, %s", psc_desc(psc), dh_repr(dh));
      f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                       feature_code, feature_name, buf);
   }
   }

   *pvalrec = valrec;
   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s, *pvalrec=%p", psc_desc(psc), *pvalrec);
   assert( (psc == 0 && *pvalrec) || (psc != 0 && !*pvalrec) );
   if (*pvalrec && (debug || IS_TRACING())) {
      dbgrpt_single_vcp_value(*pvalrec, 1);
   }
   if (ddc_excp) {
#ifdef OLD
      if (debug || IS_TRACING() || report_freed_exceptions) {
         DBGMSG("Freeing exception:");
         errinfo_report(ddc_excp, 1);
      }
      errinfo_free(ddc_excp);
#endif
      ERRINFO_FREE_WITH_REPORT(ddc_excp, debug || IS_TRACING() || report_freed_exceptions);
   }
   return psc;
}
#endif


/* Gather values for the features in a feature set.
 *
 * Arguments:
 *    dh                  display handle
 *    feature_set         feature set identifying features to be queried
 *    vset                append values retrieved to this value set
 *    ignore_unsupported  unsupported features are not an error
 *    msg_fh              destination for error messages
 *
 * Returns:
 *    status code
 */
Public_Status_Code
collect_raw_feature_set_values2_dfm(
      Display_Handle *      dh,
      Dyn_Feature_Set*      feature_set,
      Vcp_Value_Set         vset,
      bool                  ignore_unsupported,  // if false, is error if unsupported
      FILE *                msg_fh)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s, msg_fh=%p", dh_repr(dh), msg_fh);

   Public_Status_Code master_status_code = 0;
   int features_ct = dyn_get_feature_set_size(feature_set);
   // needed when called from C API, o.w. get get NULL response for first feature
   // DBGMSG("Inserting sleep() before first call to get_raw_value_for_feature_table_entry()");
   // sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, "initial");
   int ndx;
   for (ndx=0; ndx< features_ct; ndx++) {
      Display_Feature_Metadata * dfm = dyn_get_feature_set_entry(feature_set, ndx);
      DBGMSF(debug,"ndx=%d, feature = 0x%02x", ndx, dfm->feature_code);
      DDCA_Any_Vcp_Value *  pvalrec;
      // DDCA_Feature_Metadata * ddca_meta = dfm_to_ddca_feature_metadata(dfm);

      Error_Info *  cur_ddc_excp =
            get_raw_value_for_feature_metadata(
                  dh,
                  dfm,    // ddca_meta,
                  ignore_unsupported,
                  &pvalrec,
                   msg_fh);
      // todo: free ddca_meta
      Public_Status_Code cur_status_code = ERRINFO_STATUS(cur_ddc_excp);

      if (!cur_ddc_excp) {   // changed from (cur_status_code == 0) to avoid coverity complaint re resource leak
         vcp_value_set_add(vset, pvalrec);
      }
      else if ( (cur_status_code == DDCRC_REPORTED_UNSUPPORTED ||
                 cur_status_code == DDCRC_DETERMINED_UNSUPPORTED
                ) && ignore_unsupported
              )
      {
         // no problem
         ERRINFO_FREE_WITH_REPORT(cur_ddc_excp, IS_DBGTRC(debug, TRACE_GROUP) || report_freed_exceptions);
      }
      else {
         ERRINFO_FREE_WITH_REPORT(cur_ddc_excp, IS_DBGTRC(debug, TRACE_GROUP) || report_freed_exceptions);
         master_status_code = cur_status_code;
         break;
      }
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, master_status_code, "");
   return master_status_code;
}


/* Gather values for the features in a named feature subset
 *
 * Arguments:
 *    dh                 display handle
 *    subset             feature set identifier
 *    vset               append values retrieved to this value set
 *    ignore_unsupported  unsupported features are not an error
 *    msg_fh             destination for error messages
 *
 * Returns:
 *    status code
 */
Public_Status_Code
ddc_collect_raw_subset_values(
        Display_Handle *    dh,
        VCP_Feature_Subset  subset,
        Vcp_Value_Set       vset,
        bool                ignore_unsupported,
        FILE *              msg_fh)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "subset=%s  dh=%s, msg_fn=%p",
                 feature_subset_name(subset), dh_repr(dh), msg_fh );

   assert(subset == VCP_SUBSET_PROFILE);  // currently the only use of this function,
                                          // will need to reconsider handling of Feature_Set_Flags if other
                                          // uses arise
   Public_Status_Code psc = 0;
   Feature_Set_Flags flags = FSF_NOTABLE;
   if (subset == VCP_SUBSET_PROFILE)
      flags |= FSF_RW_ONLY;
   Dyn_Feature_Set * feature_set = dyn_create_feature_set(
                                     subset,
                                     dh->dref,          // vcp_version,
                                     flags);
   if (debug)
      dbgrpt_dyn_feature_set(feature_set, true, 0);

   psc = collect_raw_feature_set_values2_dfm(
            dh, feature_set, vset,
            ignore_unsupported, msg_fh);

   dyn_free_feature_set(feature_set);

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, psc, "");
   return psc;
}


//
// Get formatted feature values
//

/** Queries the monitor for a VCP feature value, and returns
 *  a formatted interpretation of the value.
 *
 * \param  dh         handle for open display
 * \param  dfm        feature metadata
 * \param  suppress_unsupported
 *                    if true, do not report unsupported features
 * \param  prefix_value_with_feature_code
 *                    include feature code in formatted value
 * \param  formatted_value_loc
 *                    where to return pointer to formatted value
 * \param msg_fh      where to write extended messages for verbose
 *                    value retrieval, etc.
 * \return status code
 *
 * \remark
 * This function is a kitchen sink of functionality, extracted from
 * earlier code.  It needs refactoring.
 */
Public_Status_Code
ddc_get_formatted_value_for_dfm(
      Display_Handle *            dh,
      Display_Feature_Metadata *  dfm,
      bool                        suppress_unsupported,
      bool                        prefix_value_with_feature_code,
      char **                     formatted_value_loc,
      FILE *                      msg_fh)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "suppress_unsupported=%s", sbool(suppress_unsupported));
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "dfm->global_feature_flags = %s",
         interpret_ddca_global_feature_flags_symbolic_t(dfm->global_feature_flags));
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "dfm->version_feature_flags = %s",
         interpret_ddca_version_feature_flags_symbolic_t(dfm->version_feature_flags));

   Public_Status_Code psc = 0;
   Error_Info * ddc_excp;
   *formatted_value_loc = NULL;

   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_dh(dh);
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "vspec=%d.%d", vspec.major, vspec.minor);
   // DDCA_Feature_Metadata* extmeta = dfm_to_ddca_feature_metadata(dfm);
   Byte feature_code = dfm->feature_code;
   char * feature_name = dfm->feature_name;
   bool is_table_feature = dfm->version_feature_flags & DDCA_TABLE;
#ifndef NDEBUG
   DDCA_Vcp_Value_Type feature_type = (is_table_feature) ? DDCA_TABLE_VCP_VALUE : DDCA_NON_TABLE_VCP_VALUE;
#endif
   DDCA_Output_Level output_level = get_output_level();
   if (output_level >= DDCA_OL_VERBOSE) {
      fprintf(msg_fh, "\nGetting data for %s VCP code 0x%02x - %s:\n",
                            (is_table_feature) ? "table" : "non-table",
                            feature_code,
                            feature_name);
   }

   DDCA_Any_Vcp_Value *  pvalrec = NULL;

   // bool ignore_unsupported = !(output_level >= DDCA_OL_NORMAL && !suppress_unsupported);
   bool ignore_unsupported = suppress_unsupported;

   ddc_excp = get_raw_value_for_feature_metadata(
            dh,
            dfm,    // extmeta,
            ignore_unsupported,
            &pvalrec,
            (output_level == DDCA_OL_TERSE) ? NULL : msg_fh);
            // msg_fh);

   psc = ERRINFO_STATUS(ddc_excp);
   assert( (!ddc_excp && (feature_type == pvalrec->value_type)) || (psc!=0 && !pvalrec) );
   if (!ddc_excp) {      // changed from (psc == 0) to avoid avoid coverity complaint re resource leak
      // if (!is_table_feature && output_level >= OL_VERBOSE) {
      // if (!is_table_feature && debug) {
      if (output_level >= DDCA_OL_VERBOSE || debug) {
         rpt_push_output_dest(msg_fh);
         // report_single_vcp_value(pvalrec, 0);
         // rpt_vstring(0, "Raw value: %s", summarize_single_vcp_value(pvalrec));
         fprintf(msg_fh, "Raw value: %s\n", summarize_single_vcp_value(pvalrec));
         rpt_pop_output_dest();
      }

      if (output_level == DDCA_OL_TERSE) {
         if (is_table_feature) {
            // output VCP code  hex values of bytes
            int bytect = pvalrec->val.t.bytect;
            int hexbufsize = bytect * 3;
            char * hexbuf = calloc(hexbufsize, sizeof(char));
            char space = ' ';
            // n. buffer passed to hexstring2(), so no allocation
            hexstring2(pvalrec->val.t.bytes, bytect, &space, false /* upper case */, hexbuf, hexbufsize);
            char * formatted = calloc(hexbufsize + 20, sizeof(char));
            snprintf(formatted, hexbufsize+20, "VCP %02X T x%s\n", feature_code, hexbuf);
            *formatted_value_loc = formatted;
            free(hexbuf);
         }
         else {                                // OL_TERSE, not table feature
            DDCA_Version_Feature_Flags vflags = dfm->version_feature_flags;
            // =   get_version_sensitive_feature_flags(vcp_entry, vspec);
            char buf[200];
            assert(vflags & (DDCA_CONT | DDCA_SIMPLE_NC | DDCA_EXTENDED_NC | DDCA_COMPLEX_NC | DDCA_NC_CONT));
            if (vflags & DDCA_CONT) {
               snprintf(buf, 200, "VCP %02X C %d %d",
                                  feature_code,
               VALREC_CUR_VAL(pvalrec), VALREC_MAX_VAL(pvalrec));
            }
            else if (vflags & DDCA_SIMPLE_NC) {
               snprintf(buf, 200, "VCP %02X SNC x%02x",
               feature_code, pvalrec->val.c_nc.sl);
            }
            else if (vflags & DDCA_EXTENDED_NC) {
               snprintf(buf, 200, "VCP %02X SNC x%02x x%02x",
               feature_code, pvalrec->val.c_nc.sh, pvalrec->val.c_nc.sl);
            }
            else {
               assert(vflags & (DDCA_COMPLEX_NC|DDCA_NC_CONT));
               snprintf(buf, 200, "VCP %02X CNC x%02x x%02x x%02x x%02x",
                                  feature_code,
                                  pvalrec->val.c_nc.mh,
                                  pvalrec->val.c_nc.ml,
                                  pvalrec->val.c_nc.sh,
                                  pvalrec->val.c_nc.sl
                                  );
            }
            *formatted_value_loc = g_strdup(buf);
         }
      }

      else  {    // output_level >= DDCA_OL_NORMAL
         bool ok;
         char * formatted_data = NULL;

         ok = dyn_format_feature_detail(
                 dfm,
                 vspec,
                 pvalrec,
                 &formatted_data);
         // DBGMSG("vcp_format_feature_detail set formatted_data=|%s|", formatted_data);
         if (!ok) {
            char msg[100];
            if (pvalrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
               g_snprintf(
                     msg, 100,
                     "!!! UNABLE TO FORMAT OUTPUT. mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x",
                     pvalrec->val.c_nc.mh,  pvalrec->val.c_nc.ml,
                     pvalrec->val.c_nc.sh,  pvalrec->val.c_nc.sl);
            }
            else {
               strcpy(msg,  "!!! UNABLE TO FORMAT OUTPUT");
            }
            f0printf(msg_fh, FMT_CODE_NAME_DETAIL_W_NL,
                            feature_code, feature_name, msg);
            psc = DDCRC_INTERPRETATION_FAILED;
            ddc_excp = errinfo_new(DDCRC_INTERPRETATION_FAILED, __func__, msg);
            // TODO: retry with default output function
         }

         if (ok) {
            if (prefix_value_with_feature_code) {
               *formatted_value_loc = calloc(1, strlen(formatted_data) + 50);
               snprintf(*formatted_value_loc, strlen(formatted_data) + 49,
                        FMT_CODE_NAME_DETAIL_WO_NL,
                        feature_code, feature_name, formatted_data);
               free(formatted_data);
            }
            else {
                *formatted_value_loc = formatted_data;
             }
         }
      }         // normal (non OL_PROGRAM) output
   }

   else {   // error
      // if output_level >= DDCA_OL_NORMAL, get_raw_value_for_feature_table_entry() already issued message
      if (output_level == DDCA_OL_TERSE && !suppress_unsupported) {
         f0printf(msg_fh, "VCP %02X ERR\n", feature_code);
      }
   }

   if (pvalrec)
      free_single_vcp_value(pvalrec);

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, psc,
          "*formatted_value_loc=%p -> %s",
          *formatted_value_loc, *formatted_value_loc);

   ASSERT_IFF(psc == 0, !ddc_excp);
   ERRINFO_FREE_WITH_REPORT(ddc_excp, debug || IS_TRACING() || report_freed_exceptions);
   return psc;
}


Public_Status_Code
show_feature_set_values2_dfm(
      Display_Handle *      dh,
      Dyn_Feature_Set*      feature_set,
      GPtrArray *           collector,     // if null, write to current stdout device
      Feature_Set_Flags     flags,
      Bit_Set_256 *         features_seen)     // if non-null, collect list of features seen
{
   bool debug = false;
   char * s0 = feature_set_flag_names_t(flags);
   DBGTRC_STARTING(debug, TRACE_GROUP, "flags=%s, collector=%p", s0, collector);

   Public_Status_Code master_status_code = 0;

   FILE * outf = fout();

   VCP_Feature_Subset subset_id = feature_set->subset;
   DDCA_Output_Level output_level = get_output_level();
   bool show_unsupported = false;
   if ( (flags & FSF_SHOW_UNSUPPORTED)  ||
        output_level >= DDCA_OL_VERBOSE ||
        subset_id == VCP_SUBSET_SINGLE_FEATURE
#ifdef FUTURE
        ||        subset_id == VCP_SUBSET_MULTI_FEATURES
#endif
       )
       show_unsupported = true;
   bool suppress_unsupported = !show_unsupported;

   // DDCA_MCCS_Version_Spec vcp_version = get_vcp_version_by_dh(dh);
   bool prefix_value_with_feature_code = true;    // TO FIX
   FILE * msg_fh = outf;                        // TO FIX
   int features_ct = dyn_get_feature_set_size(feature_set);
   DBGMSF(debug, "features_ct=%d", features_ct);
   int ndx;
   for (ndx=0; ndx< features_ct; ndx++) {
      Display_Feature_Metadata * dfm = dyn_get_feature_set_entry(feature_set, ndx);
      // DDCA_Feature_Metadata * extmeta = ifm->external_metadata;
      DBGMSF(debug,"ndx=%d, feature = 0x%02x", ndx, dfm->feature_code);
      if ( !(dfm->version_feature_flags & DDCA_READABLE) ) {
         // confuses the output if suppressing unsupported
         if (show_unsupported) {
            char * feature_name =  dfm->feature_name;
            char * msg = (dfm->version_feature_flags & DDCA_DEPRECATED) ? "Deprecated" : "Write-only feature";
            f0printf(outf, FMT_CODE_NAME_DETAIL_W_NL,
                          dfm->feature_code, feature_name, msg);
         }
      }
      else {
         bool skip_feature = false;
#ifdef NO
         if (subset_id != VCP_SUBSET_SINGLE_FEATURE &&
             is_feature_table_by_vcp_version(entry, vcp_version) &&
             (feature_flags & FSF_NOTABLE) )
         {
            skip_feature = true;
         }
#endif
         if (!skip_feature) {

            char * formatted_value = NULL;
            Public_Status_Code psc =
            ddc_get_formatted_value_for_dfm(
                  dh,
                  dfm,
                  suppress_unsupported,
                  prefix_value_with_feature_code,
                  &formatted_value,
                  msg_fh);
            assert( (psc==0 && formatted_value) || (psc!=0 && !formatted_value) );
            if (psc == 0) {
               if (collector)
                  g_ptr_array_add(collector, formatted_value);
               else
                  f0printf(outf, "%s\n", formatted_value);
               free(formatted_value);
               if (features_seen)
                  *features_seen = bs256_insert(*features_seen, dfm->feature_code);  // note that feature was read
            }
            else {
               // or should I check features_ct == 1?
               VCP_Feature_Subset subset_id = feature_set->subset;
               if (subset_id == VCP_SUBSET_SINGLE_FEATURE)
                  master_status_code = psc;
               else {
                  if ( (psc != DDCRC_REPORTED_UNSUPPORTED) && (psc != DDCRC_DETERMINED_UNSUPPORTED) ) {
                     if (master_status_code == 0)
                        master_status_code = psc;
                  }
               }
            }
         }   // !skip_feature
      }
      DBGMSF(debug,"ndx=%d, feature = 0x%02x Done", ndx, dfm->feature_code);
   }   // loop over features

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, master_status_code, "");
   return master_status_code;
}


#ifdef FUTURE
//typedef bool (*VCP_Feature_Set_Filter_Func)(VCP_Feature_Table_Entry * ventry);
bool hack42(VCP_Feature_Table_Entry * ventry) {
   bool debug = false;
   bool result = true;

   // if (ventry->code >= 0xe0)  {     // is everything promoted to int before comparison?
   if ( (ventry->vcp_global_flags & DDCA_SYNTHETIC) &&
        (ventry->v20_flags & DDCA_NORMAL_TABLE)
      )
   {
      result = false;
      DBGMSF(debug, "Returning false for vcp code 0x%02x", ventry->code);
   }
   return result;
}
#endif


/** Shows the VCP values for all features in a VCP feature subset.
 *
 *  @param  dh         display handle for open display
 *  @param  subset     feature subset id
 *  @param  collector  accumulates output    // if null, write to current stdout device
 *  @param  flags      feature set flags
 *  @param  features_seen   collects ids of features that exist
 *  @return status code
 */
// 11/2019: only call is from app_getvcp.c, move there?
Public_Status_Code
ddc_show_vcp_values(
        Display_Handle *    dh,
        VCP_Feature_Subset  subset,
        GPtrArray *         collector,    // not used
        Feature_Set_Flags   flags,
        Bit_Set_256 *       features_seen)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "subset=%s, flags=%s,  dh=%s",
         feature_subset_name(subset), feature_set_flag_names_t(flags), dh_repr(dh) );

   Public_Status_Code psc = 0;

   // DDCA_MCCS_Version_Spec vcp_version = get_vcp_version_by_dh(dh);
   // DBGMSG("VCP version = %d.%d", vcp_version.major, vcp_version.minor);

   Dyn_Feature_Set* feature_set = dyn_create_feature_set(
                                    subset,
                                    dh->dref,   // vcp_version,
                                    flags);

#ifdef FUTURE
   Parsed_Capabilities * pcaps = NULL;   // TODO: HOW TO GET Parsed_Capabilities?, will only be set for probe/interrogate
   // special case, if scanning, don't try to do a table read of manufacturer specific
   // features if it's clear that table read commands are unavailable

   // convoluted solution to avoid passing additional argument to create_feature_set()
   if (subset == VCP_SUBSET_SCAN && !parsed_capabilities_may_support_table_commands(pcaps)) {
      filter_feature_set(feature_set, hack42);
   }
#endif

   if (IS_DBGTRC(debug, TRACE_GROUP)) {
      DBGTRC(true, TRACE_GROUP, "feature_set:");
      dbgrpt_dyn_feature_set(feature_set, true, 0);
   }
   psc = show_feature_set_values2_dfm(
            dh, feature_set, collector, flags, features_seen);
   dyn_free_feature_set(feature_set);
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, psc, "");
   return psc;
}


void init_ddc_output() {
   RTTI_ADD_FUNC(get_raw_value_for_feature_metadata);
   RTTI_ADD_FUNC(collect_raw_feature_set_values2_dfm);
   RTTI_ADD_FUNC(ddc_collect_raw_subset_values);
   RTTI_ADD_FUNC(ddc_get_formatted_value_for_dfm);
   RTTI_ADD_FUNC(show_feature_set_values2_dfm);
   RTTI_ADD_FUNC(ddc_show_vcp_values);
   
}

