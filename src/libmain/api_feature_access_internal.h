/** @file api_feature_access_internal.h
 *
 *  Contains declarations of functions used only by other api_... files,
 *  and of otherwise unpublished and archived functions.
 */

// Copyright (C) 2015-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef API_FEATURE_ACCESS_INTERNAL_H_
#define API_FEATURE_ACCESS_INTERNAL_H_

#include "public/ddcutil_types.h"

#include "base/ddcutil_types_internal.h"
#include "base/monitor_model_key.h"


// NEVER PUBLISHED, USED INTERNALLY
/** Returns a formatted representation of a non-table VCP value.
 *  It is the responsibility of the caller to free the returned string.
 *
 *  @param[in]  feature_code        VCP feature code
 *  @param[in]  vspec               MCCS version
 *  @param[in]  valrec              non-table VCP value
 *  @param[out] formatted_value_loc address at which to return the formatted value.
 *  @return                         status code, 0 if success
 *  @since 0.9.0
 */
DDCA_Status
ddca_format_non_table_vcp_value(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_MCCS_Version_Spec      vspec,
      DDCA_Monitor_Model_Key *     mmid,
      DDCA_Non_Table_Vcp_Value *  valrec,
      char **                     formatted_value_loc);


// NEVER PUBLISHED, USED INTERNALLY
/** Returns a formatted representation of a table VCP value.
 *  It is the responsibility of the caller to free the returned string.
 *
 *  @param[in]  feature_code        VCP feature code
 *  @param[in]  vspec               MCCS version
 *  @param[in]  table_value         table VCP value
 *  @param[out] formatted_value_loc address at which to return the formatted value.
 *  @return                         status code, 0 if success
 *  @since 0.9.0
 */
DDCA_Status
ddca_format_table_vcp_value(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_MCCS_Version_Spec  vspec,
      DDCA_Monitor_Model_Key * mmid,
      DDCA_Table_Vcp_Value *  table_value,
      char **                 formatted_value_loc);


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
DDCA_Status
ddca_format_any_vcp_value(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_MCCS_Version_Spec  vspec,
      DDCA_Monitor_Model_Key * mmid,
      DDCA_Any_Vcp_Value *    valrec,
      char **                 formatted_value_loc);


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
__attribute__ ((deprecated ("Does not support user-supplied feature definitions")))
DDCA_Status
ddca_get_formatted_vcp_value(
       DDCA_Display_Handle     ddca_dh,
       DDCA_Vcp_Feature_Code   feature_code,
       char**                  formatted_value_loc);


void init_api_feature_access();

#endif /* API_FEATURE_ACCESS_INTERNAL_H_ */
