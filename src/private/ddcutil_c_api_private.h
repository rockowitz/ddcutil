/* ddcutil_c_api_private.h
 *
 * <copyright>
 * Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#ifndef DDCUTIL_C_API_PRIVATE_H_
#define DDCUTIL_C_API_PRIVATE_H_

// Declarations of API functions that haven't yet been published or that have
// been removed from ddcutil_c_api.h

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



// Monitor Model Key - UNPUBLISHED, USED INTERNALLY

//
// Monitor Model Identifier
//

/** Special reserved value indicating value undefined.
 * @since 0.9.0
 */
const extern DDCA_Monitor_Model_Key DDCA_UNDEFINED_MONITOR_MODEL_KEY;

/** Creates a monitor model identifier.
 *
 *  @param  mfg_id
 *  @param  model_name
 *  @param  product_code
 *  @return identifier (note the value returned is the actual identifier,
 *                     not a pointer)
 *  @retval DDCA_UNDEFINED_MONITOR_MODEL_KEY if parms are invalid
 *  @since 0.9.0
 */
DDCA_Monitor_Model_Key
ddca_mmk(
      const char * mfg_id,
      const char * model_name,
      uint16_t     product_code);

/** Tests if 2 #Monitor_Model_Key identifiers specify the
 *  same monitor model.
 *
 *  @param  mmk1   first identifier
 *  @param  mmk2   second identifier
 *
 *  @remark
 *  The identifiers are considered equal if both are defined.
 *  @since 0.9.0
 */

bool
ddca_mmk_eq(
      DDCA_Monitor_Model_Key mmk1,
      DDCA_Monitor_Model_Key mmk2);

/** Tests if a #Monitor_Model_Key value
 *  represents a defined identifier.
 *
 *  @param mmk
 *  @return true/false
 *  @since 0.9.0
 */
bool
ddca_mmk_is_defined(
      DDCA_Monitor_Model_Key mmk);


/** Extracts the monitor model identifier for a display represented by
 *  a #DDCA_Display_Ref.
 *
 *  @param ddca_dref
 *  @return monitor model identifier
 *  @since 0.9.0
 */
DDCA_Monitor_Model_Key
ddca_mmk_from_dref(
      DDCA_Display_Ref   ddca_dref);


// CHANGE NAME?  _for_dh()?   ddca_mmid_for_dh()
/** Returns the monitor model identifier for an open display.
 *
 *  @param  ddca_dh   display handle
 *  @return #DDCA_Monitor_Model_Key for the handle,
 *          NULL if invalid display handle
 *
 *  @since 0.9.0
 */
DDCA_Monitor_Model_Key
ddca_mmk_from_dh(
      DDCA_Display_Handle   ddca_dh);



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




/** Parses a capabilities string, and reports the parsed string
 *  using the code of command "ddcutil capabilities".
 *
 *  The report is written to the current FOUT location.
 *
 *  The detail level written is sensitive to the current output level.
 *
 *  @param[in]  capabilities_string  capabilities string
 *  @param[in]  depth  logical       indentation depth
 *
 *  @remark
 *  This function exists as a development aide.  Internally, ddcutil uses
 *  a different data structure than DDCA_Parsed_Capabilities.  That
 *  data structure uses internal collections that are not exposed at the
 *  API level.
 *  @since 0.9.0
 */
void ddca_parse_and_report_capabilities(
      char *                    capabilities_string,
      DDCA_Monitor_Model_Key *  mmid,
      int                       depth);


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


#endif /* DDCUTIL_C_API_PRIVATE_H_ */
