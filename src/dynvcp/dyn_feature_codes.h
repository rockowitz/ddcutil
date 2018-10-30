/** @file dyn_feature_codes.h
 *
 * Access VCP feature code descriptions at the DDC level in order to
 * incorporate user-defined per-monitor feature information.
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef DYN_FEATURE_CODES_H_
#define DYN_FEATURE_CODES_H_

#include "ddcutil_types.h"

#include "base/feature_metadata.h"

#include "vcp/vcp_feature_codes.h"

#include "ddc/ddc_vcp_version.h"

#ifdef IFM
// Extends DDCA_Feature_Metadata with  fields not exposed in API
/** Describes a VCP feature code, tailored for a specific VCP version */
typedef
struct {
   DDCA_Feature_Metadata *                external_metadata;

   // fields not in DDCA_Feature_Metadata:
   Format_Normal_Feature_Detail_Function  nontable_formatter;
   Format_Normal_Feature_Detail_Function2 vcp_nontable_formatter;
   Format_Table_Feature_Detail_Function   table_formatter;
} Internal_Feature_Metadata;
#endif

#ifdef IFM
void
dbgrpt_internal_feature_metadata(
      Internal_Feature_Metadata * intmeta,
      int                         depth);
#endif

#ifdef DVFI
void
version_feature_info_to_metadata(
      DDCA_Version_Feature_Info * info,
      DDCA_Feature_Metadata * meta);
#endif

#ifdef IFM
Internal_Feature_Metadata *
dyn_get_feature_metadata_by_mmk_and_vspec(
     DDCA_Vcp_Feature_Code    feature_code,
     DDCA_Monitor_Model_Key   mmk,
     DDCA_MCCS_Version_Spec   vspec,
     bool                     with_default);
#endif

Display_Feature_Metadata *
dyn_get_feature_metadata_by_mmk_and_vspec_dfm(
     DDCA_Vcp_Feature_Code    feature_code,
     DDCA_Monitor_Model_Key   mmk,
     DDCA_MCCS_Version_Spec   vspec,
     bool                     with_default);

#ifdef IFM
Internal_Feature_Metadata *
dyn_get_feature_metadata_by_dref(
      DDCA_Vcp_Feature_Code       id,
      Display_Ref *               dref,
      bool                        with_default);
#endif

Display_Feature_Metadata *
dyn_get_feature_metadata_by_dref_dfm(
      DDCA_Vcp_Feature_Code       id,
      Display_Ref *               dref,
      bool                        with_default);

#ifdef IFM
Internal_Feature_Metadata *
dyn_get_feature_metadata_by_dh(
      DDCA_Vcp_Feature_Code       id,
      Display_Handle *            dh,
      bool                        with_default);
#endif

Display_Feature_Metadata *
dyn_get_feature_metadata_by_dh_dfm(
      DDCA_Vcp_Feature_Code       id,
      Display_Handle *            dh,
      bool                        with_default);

#ifdef IFM
bool
dyn_format_nontable_feature_detail(
      Internal_Feature_Metadata * intmeta,
      DDCA_MCCS_Version_Spec      vcp_version,
      Nontable_Vcp_Value *        code_info,
      char *                      buffer,
      int                         bufsz);
#endif

bool
dyn_format_nontable_feature_detail_dfm(
        Display_Feature_Metadata * dfm,
        DDCA_MCCS_Version_Spec     vcp_version,
        Nontable_Vcp_Value *       code_info,
        char *                     buffer,
        int                        bufsz);


#ifdef IFM
bool
dyn_format_table_feature_detail(
      Internal_Feature_Metadata * intmeta,
      DDCA_MCCS_Version_Spec      vcp_version,
      Buffer *                    accumulated_value,
      char * *                    aformatted_data);
#endif

bool
dyn_format_table_feature_detail_dfm(
      Display_Feature_Metadata *  dfm,
       DDCA_MCCS_Version_Spec     vcp_version,
       Buffer *                   accumulated_value,
       char * *                   aformatted_data
     );


#ifdef IFM
bool
dyn_format_feature_detail(
      Internal_Feature_Metadata * intmeta,
      DDCA_MCCS_Version_Spec      vcp_version,
      DDCA_Any_Vcp_Value *        valrec,
      char * *                    aformatted_data);
#endif
bool
dyn_format_feature_detail_dfm(
       Display_Feature_Metadata * dfm,
       DDCA_MCCS_Version_Spec    vcp_version,
       DDCA_Any_Vcp_Value *      valrec,
       char * *                  aformatted_data
     );

char *
dyn_get_feature_name(
      Byte                       feature_code,
      Display_Ref*               dref);

bool dyn_format_feature_detail_sl_lookup(
        Nontable_Vcp_Value *       code_info,
        DDCA_Feature_Value_Entry * value_table,
        char *                     buffer,
        int                        bufsz);

void init_dyn_feature_codes();

#ifdef IFM
// for transition:
Internal_Feature_Metadata *
dfm_to_internal_feature_metadata(
      Display_Feature_Metadata * dfm);
#endif

#endif /* DYN_FEATURE_CODES_H_ */
