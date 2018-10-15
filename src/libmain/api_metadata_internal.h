// api_metadata.h

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 

#ifndef API_METADATA_INTERNAL_H_
#define API_METADATA_INTERNAL_H_

#include "ddcutil_status_codes.h"
#include "ddcutil_types.h"

// needed by api_capabilities.c:
#ifdef DUPLICATE
DDCA_Status
ddca_get_simple_sl_value_table_by_vspec(
      DDCA_Vcp_Feature_Code      feature_code,
      DDCA_MCCS_Version_Spec     vspec,
      const DDCA_Monitor_Model_Key *   p_mmid,   // currently ignored
      DDCA_Feature_Value_Entry** value_table_loc);
#endif

DDCA_Status
ddca_get_simple_nc_feature_value_name_by_table(
      DDCA_Feature_Value_Entry *  feature_value_table,
      uint8_t                     feature_value,
      char**                      value_name_loc);


// Feature Lists

// NEVER PUBLISHED, USED INTERNALLY
/** Given a feature set id, returns a #DDCA_Feature_List specifying all the
 *  feature codes in the set.
 *
 *  @param[in]  feature_set_id
 *  @param[in]  vcp_version
 *  @param[in]  include_table_features if true, Table type features are included
 *  @param[out] points to feature list to be filled in
 *
 *  @since 0.9.0
 */
DDCA_Status
ddca_get_feature_list(
      DDCA_Feature_Subset_Id  feature_set_id,
      DDCA_MCCS_Version_Spec  vcp_version,
      bool                    include_table_features,
      DDCA_Feature_List*      p_feature_list);




// NEVER PUBLISHED
/** Gets the value id/name table of the allowed values for a simple NC feature.
 *
 * @param[in]  vspec             MCCS version
 * @param[in]  feature_code      VCP feature code
 * @param[in]  feature_value     single byte feature value
 * @param[out] feature_name_loc  where to return feature name
 * @return     status code
 *
 * @remark
 * If the feature value cannot be found in the lookup table for
 * the specified MCCS version, tables for later versions, if they
 * exist, are checked as well.
 *
 * @since 0.9.0
 */
DDCA_Status
ddca_get_simple_nc_feature_value_name_by_vspec(
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_MCCS_Version_Spec vspec,
      const DDCA_Monitor_Model_Key * p_mmid,
      uint8_t                feature_value,
      char**                 feature_name_loc);






// UNPUBLISHED, USED INTERNALLY
/** Gets the value id/name table of the allowed values for a simple NC feature.
 *
 * @param[in]  feature_code      VCP feature code
 * @param[in]  vspec             MCCS version
 * @param[in]  p_mmid            pointer to monitor model identifier, may be NULL
 * @param[out] value_table_loc   where to return pointer to array of DDCA_Feature_Value_Entry
 * @return     status code
 * @retval     0                       success
 * @retval     DDCRC_UNKNOWN_FEATURE   unrecognized feature code
 * @retval     DDCRC_INVALID_OPERATION feature not simple NC
 *
 *@remark p_mmid currently ignored
 * @since 0.9.0
 */
DDCA_Status
ddca_get_simple_sl_value_table_by_vspec(
      DDCA_Vcp_Feature_Code      feature_code,
      DDCA_MCCS_Version_Spec     vspec,
      const DDCA_Monitor_Model_Key *   p_mmid,   // currently ignored
      DDCA_Feature_Value_Entry** value_table_loc);




//
//  Metadata
//

#ifdef UNIMPLEMENTED
// Unimplemented
// alt: can check status code for ddca_get_feature_info_by_dh()
DDCA_Status ddca_is_feature_supported(
      DDCA_Display_Handle    dh,
      DDCA_Vcp_Feature_Code  feature_code,
      bool *                 answer_loc);   // or return status code?

#endif



// UNPUBLISHED
/** Gets the value id/name table of the allowed values for a simple NC feature.
 *
 * @param[in]  feature_code      VCP feature code
 * @param[in]  dref              display reference
 * @param[out] value_table_loc   where to return pointer to array of DDCA_Feature_Value_Entry
 * @return     status code
 * @retval     0                       success
 * @retval     DDCRC_UNKNOWN_FEATURE   unrecognized feature code
 * @retval     DDCRC_INVALID_OPERATION feature not simple NC
 *
 * @since 0.9.0
 */
DDCA_Status
ddca_get_simple_sl_value_table_by_dref(
      DDCA_Vcp_Feature_Code      feature_code,
      DDCA_Display_Ref           dref,
      DDCA_Feature_Value_Entry** value_table_loc);




// New master functions for feature metadata


// Granular functions for metadata

// NEVER PUBLISHED
// used in ddcui
// returns pointer into permanent internal data structure, caller should not free
/** Gets the VCP feature name, which may vary by MCCS version.
 *
 * @param[in]  feature_code  feature code
 * @param[in]  vspec         MCCS version
 * @param[in]  p_mmid        pointer to monitor model identifier, may be null
 * @return     pointer to feature name (do not free), NULL if unknown feature code
 *
 * @remark **p_mmid** currently ignored
 * @since 0.9.0
 */
char *
ddca_feature_name_by_vspec(
      DDCA_Vcp_Feature_Code    feature_code,
      DDCA_MCCS_Version_Spec   vspec,
      DDCA_Monitor_Model_Key * p_mmid);







#endif /* API_METADATA_INTERNAL_H_ */
