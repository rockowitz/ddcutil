/** @file api_feature_access_internal.h
 *
 *  Contains declarations of functions used only by other api_... files,
 *  and of otherwise unpublished and archived functions.
 */

// Copyright (C) 2015-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef API_FEATURE_ACCESS_INTERNAL_H_
#define API_FEATURE_ACCESS_INTERNAL_H_

#include "public/ddcutil_types.h"
#include "private/ddcutil_types_private.h"


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
 *  @since 0.9.0
 */
DDCA_Status
ddca_format_any_vcp_value(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_MCCS_Version_Spec  vspec,
      DDCA_Monitor_Model_Key * mmid,
      DDCA_Any_Vcp_Value *    valrec,
      char **                 formatted_value_loc);





//
// Experimental - Not for public use
//
// Used in exploratory Python APIs
//

DDCA_Status
ddca_start_get_any_vcp_value(
      DDCA_Display_Handle         ddca_dh,
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Vcp_Value_Type         call_type,
      DDCA_Notification_Func      callback_func);


// unimplemented
/** Registers a callback function to call when a VCP value changes */
DDCA_Status
ddca_register_callback(
      DDCA_Notification_Func func,
      uint8_t                callback_options);   // type is a placeholder

DDCA_Status
ddca_pass_callback(
      Simple_Callback_Func  func,
      int                   parm
      );

// unimplemeted
DDCA_Status
ddca_queue_get_non_table_vcp_value(
      DDCA_Display_Handle      ddca_dh,
      DDCA_Vcp_Feature_Code    feature_code
);



#endif /* API_FEATURE_ACCESS_INTERNAL_H_ */
