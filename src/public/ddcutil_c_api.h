/* ddcutil_c_api.h
 *
 * Public C APi for ddcutil. .
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef DDCUTIL_C_API_H_
#define DDCUTIL_C_API_H_

#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>

#include "ddcutil_types.h"

#include "util/coredefs.h"

// is this the right location?
#ifdef __cplusplus
extern "C"
#endif


/** @file ddcutil_c_api.h
 *  @brief ddcutil public C API
 *
 *  Function names in the public C API begin with "ddca_"
 *
 *  Function ddca_init() must be called before all others
 *  (except for library build information).
 */

//
// Library build information
//

#ifdef UNUSED
typedef struct {
   int    major;
   int    minor;
   int    build;
} DDCA_Version_Spec;

DDCA_Version_Spec ddct_get_version(void);       // ddcutil version
#endif

/**
 * Returns the ddcutil version as a string in the form "major.minor.micro".
 */
const char * ddca_ddcutil_version_string();

/**
 * Indicates whether the ddcutil library was built with ADL support.
 *
 * (May be removed.)
 */
bool ddca_built_with_adl(void);

/**
 * Indicates whether the ddcutil library was built with support for USB connected monitors. .
 *
 * (May be removed.)
 */
bool ddca_built_with_usb(void);

/** Queries ddcutil library build options.
 *
 * @return  flags
 */
uint8_t ddca_get_build_options(void);

// TODO: Bit ids for ddca_get_build_options() - how to make connection in doxygen?
/** @brief ddcutil was built with support for AMD Display Library connected monitors */
#define DDCA_BUILT_WITH_ADL     0x01
/** ddcutil was built with support for USB connected monitors */
#define DDCA_BUILT_WITH_USB     0x02
#define DDCA_BUILT_WITH_FAILSIM 0x04  /**< @brief ddcutil was built with support for failure simulation */


//
// Initialization
//

/**
 * Initializes the ddcutil library module.
 *
 * Must be called before most other functions.
 *
 * It is not an error if this function is called more than once.
 */
void ddca_init(void);


//
// Status Code Management
//

/** ddcutil status code */
typedef int DDCA_Status;

/** Returns the name for a ddcutil status code */
char * ddca_status_code_name(DDCA_Status status_code);

/** Returns a description of a ddcutil status code */
char * ddca_status_code_desc(DDCA_Status status_code);


//
// Global Settings
//

/** To capture certain rare fatal errors in library.
 *  If not set, library aborts.
 *
 *  @param[in] jb pointer to setjmp()/longjmp() saved registers
 *
 *  Call this library function if the program using libddcutil
 *  uses setjmp() to register a longjmp() target.  The library needs
 *  to know what it is.
 */
void
ddca_register_jmp_buf(jmp_buf* jb);


/** libddcutil's internal abort function fills in this data structure
 *  when a fatal error occurs.   If a jmp_buf has been registered by
 *  ddca_register_jmp_buf(), the caller can examine this data structure
 *  after an "error" return from setjmp()
 */
DDCA_Global_Failure_Information *
ddca_get_global_failure_information();


/** Gets the I2C timeout in milliseconds for the specified timeout class.
 * @param timeout_type timeout type
 * @return timeout in milliseconds
 */
int
ddca_get_timeout_millis(
      DDCA_Timeout_Type timeout_type);

/** Sets the I2C timeout in milliseconds for the specified timeout class
 * @param timeout_type  timeout class
 * @param millisec      timeout to set, in milliseconds
 */
void
ddca_set_timeout_millis(
      DDCA_Timeout_Type timeout_type,
      int               millisec);


// I2C is an inherently unreliable protocol.  The application is responsible for
// retry management.
// The maximum number of retries can be tuned.
// There are 3 retry contexts:
// - An i2C write followed by a read.  Most DDC operations are of this form.
// - An I2C write without a subsequent read.  DDC operations to set a VCP feature value
//   are in this category.
// - Some DDC operations, such as reading the capabilities string, require multiple
//   write/read exchanges.  These multi -part exchanges have a separate retry count
//   for the entire operation.



/** Gets the upper limit on a max tries value that can be set.
 *
 * @return maximum max tries value allowed on set_max_tries()
 */
int
ddca_get_max_max_tries();

/** Gets the maximum number of I2C retries for the specified operation type.
 * @param  retry_type   I2C operation type
 * @return maximum number of retries
 */
int
ddca_get_max_tries(
      DDCA_Retry_Type retry_type);

/** Sets the maximum number of I2C retries for the specified operation type
 * @param retry_type    I2C operation type
 * @param max_tries     maximum count to set
 */
DDCA_Status
ddca_set_max_tries(
      DDCA_Retry_Type retry_type,
      int             max_tries);


//
// Message Control
//

/** Redirects output that normally would go to STDOUT
 */
void
ddca_set_fout(
      FILE * fout);   /**< where to write normal messages, if NULL, suppress  */

/** Redirects output that normally goes to STDOUT back to STDOUT */
void
ddca_set_fout_to_default();

/** Redirects output that normally would go to STDERR
 */
void
ddca_set_ferr(
      FILE * ferr);   /**< where to write error messages, If NULL, suppress */

/** Redirects output that normally goes to STDERR back to STDERR
 */
void
ddca_set_ferr_to_default();


/** Gets the current output level */
DDCA_Output_Level
ddca_get_output_level();

/** Sets the output level */
void
ddca_set_output_level(
      DDCA_Output_Level newval);   /**< new output level */

/** Gets the name of an output level
 * @param  val  output level id
 * @return output level name (do not free)
 */
char *
ddca_output_level_name(
      DDCA_Output_Level val);     /**< output level id */

/** Controls whether messages describing DDC protocol errors are output
 * @param onoff    if true, errors will be issued
 * */
void
ddca_enable_report_ddc_errors(
      bool onoff);

/** Checks whether messages describing DDC protocol errors are to output */
bool
ddca_is_report_ddc_errors_enabled();


//
// Display Identifier
//

/** Creates a display identifier using the display number assigned by ddcutil
 * @param[in]  dispno  display number
 * @param[out] pdid    where to return display identifier handle
 * @return status code
 * */
DDCA_Status
ddca_create_dispno_display_identifier(
      int                      dispno,
      DDCA_Display_Identifier* pdid);

/** Creates a display identifier using an ADL (adapter index, display index) pair
 * @param[in]  iAdapterIndex ADL adapter index
 * @param[in]  iDisplayIndex ADL display index
 * @param[out] pdid          where to return display identifier handle
 * @return     status code
 */
DDCA_Status
ddca_create_adlno_display_identifier(
      int                      iAdapterIndex,
      int                      iDisplayIndex,
      DDCA_Display_Identifier* pdid);

/** Creates a display identifier using an I2C bus number
 * @param[in]  busno  I2C bus number
 * @param[out] pdid   where to return display identifier handle
 * @return            status code
 * */
DDCA_Status
ddca_create_busno_display_identifier(
      int                      busno,
      DDCA_Display_Identifier* pdid);

/** Creates a display identifier using the model name string and serial number string
 * @param model  model name string
 * @param sn     serial number string
 * @param pdid   where to return display identifier handle
 * @return       status code
 */
DDCA_Status
ddca_create_model_sn_display_identifier(
      const char *             model,
      const char *             sn,
      DDCA_Display_Identifier* pdid);

/** Creates a display identifier using a 128 byte EDID
 * @param edid  pointer to 128 byte EDID
 * @param pdid  where to return display identifier handle
 * @return      status code
 */
DDCA_Status
ddca_create_edid_display_identifier(
      const Byte *              edid,
      DDCA_Display_Identifier * pdid);      // 128 byte edid

/** Creates a display identifier using a USB bus number and device number
 * @param bus    USB bus number
 * @param device USB device number
 * @param pdid   where to return display identifier handle
 * @return       status code
 */
DDCA_Status
ddca_create_usb_display_identifier(
      int                      bus,
      int                      device,
      DDCA_Display_Identifier* pdid);

/** Release the memory of a display identifier */
DDCA_Status
ddca_free_display_identifier(
      DDCA_Display_Identifier did);

/** Returns a string representation of a display identifier
 *  @param[in]  did    display indentifier
 *  @return     string representation of display identifier, NULL if invalid
 */
char *
ddca_repr_display_identifier(
      DDCA_Display_Identifier did);


//
// Display Reference
//

/** Creates a display reference from a display identifier.
 * @param[in]  did display identifier
 * @param[out] pdref where to return display reference
 * @return     status code
 */
DDCA_Status
ddca_create_display_ref(
      DDCA_Display_Identifier did,
      DDCA_Display_Ref*       pdref);

/** Frees a display reference.
 * @param dref  display reference to free
 * @return status code
 */
DDCA_Status
ddca_free_display_ref(
      DDCA_Display_Ref dref);

/** Returns a string representation of a display reference
 * @param[in]   dref display reference
 * @return      string representation of display reference, NULL if invalid
 * */
char *
ddca_repr_display_ref(
      DDCA_Display_Ref dref);

/** Writes a report on the specified display reference to the current FOUT device
 * @param dref   display reference
 * @param depth  logical indentation depth
 */
void
ddca_report_display_ref(
      DDCA_Display_Ref dref,
      int              depth);


//
// Display Handle
//

/** Open a display
 * @param[in]  dref display reference for display to open
 * @param[out] pdh where to return display handle
 * @return     status code
 */
DDCA_Status
ddca_open_display(
      DDCA_Display_Ref      ddca_dref,
      DDCA_Display_Handle * p_ddca_dh);

/** Close an open display
 * @param[in] dh   display handle
 * @return     status code
 */
DDCA_Status
ddca_close_display(
      DDCA_Display_Handle   ddca_dh);

/** Returns a string representation of a display handle.
 *  The string is valid until the next call to this function.
 *
 * @param ddca_dh  display handle
 * @return string  representation of display handle, NULL if
 *                 argument is NULL or not a display handle
 */
char *
ddca_repr_display_handle(
      DDCA_Display_Handle   ddca_dh);


//
// Display Descriptions
//

/** Gets a list of the detected displays.
 *
 *  Displays that do not support DDC are not included.
 *
 *  @return list of display summaries
 */
DDCA_Display_Info_List *
ddca_get_displays();

/** Presents a report on a single display.
 *  The report is written to the current FOUT device.
 *
 *  @param[in]  dinfo  pointer to a DDCA_Display_Info struct
 *  @param[in]  depth  logical indentation depth
 */
void
ddca_report_display_info(
      DDCA_Display_Info * dinfo,
      int                 depth);

/** Reports on all displays in a list of displays.
 *  The report is written to the current FOUT device
 *
 *  @param[in]  dlist  pointer to a DDCA_Display_Info_List
 *  @param[in]  depth  logical indentation depth
 */
void
ddca_report_display_info_list(
      DDCA_Display_Info_List * dlist,
      int                      depth);


/** Reports on all active displays.
 *  This function hooks into the code used by command "ddcutil detect"
 *
 *  @param[in] depth  logical indentation depth
 *  @return    number of MCCS capable displays
 */
int
ddca_report_active_displays(
      int depth);


//
// Monitor Capabilities
//

/** Retrieves the capabilities string for a monitor.
 *
 *  @param[in]  ddca_dh     display handle
 *  @param[out] pcaps       address at which to return pointer to capabilities string.
 *  @return     status code
 *
 *  It is the responsibility of the caller to free the returned string.
 */
DDCA_Status
ddca_get_capabilities_string(
      DDCA_Display_Handle     ddca_dh,
      char**                  buffer);

/** Parse the capabilities string.
 *
 *  @param[in] capabilities_string
 *  @param[out] address at which to return pointer to newly allocated
 *              DDCA_Capabilities struct
 *  @return     status code
 *
 *  It is the responsibility of the caller to free the returned struct
 *  using ddca_free_parsed_capabilities().
 *
 *  This function currently parses the VCP feature codes and MCCS version.
 *  It could be extended to parse additional information such as cmds if necessary.
 */
DDCA_Status
ddca_parse_capabilities_string(
      char *                   capabilities_string,
      DDCA_Capabilities **     p_parsed_capabilities);

/** Frees a DDCA_Capabilities struct
 *
 *  @param[in]  pcaps  pointer to struct to free.
 *                     Does nothing if NULL.
 */
void
ddca_free_parsed_capabilities(
      DDCA_Capabilities *      pcaps);

/** Reports the contents of a DDCA_Capabilities struct.
 *
 *  The report is written to the current FOUT location.
 *
 *  This function is intended for debugging use.
 *
 *  @param[in]  pcaps  pointer to DDCA_Capabilities struct
 *  @param[in]  depth  logical indentation depth
 */
void
ddca_report_parsed_capabilities(
      DDCA_Capabilities *      pcaps,
      int                      depth);


//
// VCP Feature Information, Monitor Independent
//

/** Gets information for a VCP feature.
 *
 * VCP characteristics (C vs NC, RW vs RO, etc) can vary by MCCS version.
 *
 * @param[in]  feature_code    VCP feature code
 * @param[in]  mccs_version_id MCCS version id, may be DDCA_VCP_VANY??
 * @param[out] p_info          where to return Version_Feature_Info struct
 * @return     status code
 */
DDCA_Status
ddca_get_feature_info_by_vcp_version(
      VCP_Feature_Code          feature_code,
   // DDCT_MCCS_Version_Spec    vspec,
      DDCA_MCCS_Version_Id      mccs_version_id,
      Version_Feature_Info**    p_info);


/** Gets the VCP feature name.  If different MCCS versions use different names
 * for the feature, this function makes a best guess.
 *
 * @param[in]  feature_code
 * @return     pointer to feature name(do not free), NULL if unknown feature code
 */
char *
ddca_get_feature_name(VCP_Feature_Code feature_code);

/** Gets the value id/name table of the allowed values for a simple NC feature.
 *
 * @param[in]  feature_code      VCP feature code
 * @param[in]  mccs_version_id   MCCS version id
 * @param[out] p_value_table     where to return pointer to array of DDCA_Feature_Value_Entry
 * @return     status code
 */
DDCA_Status
ddca_get_simple_sl_value_table(
      VCP_Feature_Code            feature_code,
      DDCA_MCCS_Version_Id        mccs_version_id,
      DDCA_Feature_Value_Table *  pvalue_table);   // DDCA_Feature_Value_Entry **


//
// VCP Feature Information, Monitor Dependent
//

// TODO: keep only 1 of the 2 get_mccs_version() variants

DDCA_Status
ddca_get_mccs_version(
      DDCA_Display_Handle     ddca_dh,
      DDCA_MCCS_Version_Spec* pspec);

DDCA_Status
ddca_get_mccs_version_id(
      DDCA_Display_Handle     ddca_dh,
      DDCA_MCCS_Version_Id*   p_id);

char * ddca_repr_mccs_version_id(DDCA_MCCS_Version_Id version_id);
char * ddca_mccs_version_id_string(DDCA_MCCS_Version_Id version_id);

#ifdef UNIMPLEMENTED

// Unimplemented
// alt: can check status code for ddca_get_feature_info_by_display()
DDCA_Status ddct_is_feature_supported(
      DDCA_Display_Handle   dh,
      VCP_Feature_Code      feature_code,
      bool *                answer);

#endif


// This is a convenience function. Keep?
DDCA_Status
ddca_get_feature_info_by_display(
      DDCA_Display_Handle      ddca_dh,
      VCP_Feature_Code         feature_code,
      Version_Feature_Info **  p_info);




//
//  Miscellaneous Monitor Specific Functions
//


// DDCT_Status ddct_get_edid(DDCA_Display_Handle * dh, Byte * edid_buffer);    // edid_buffer must be >= 128 bytes
// Keep?   Can get from
DDCA_Status
ddca_get_edid_by_display_ref(
      DDCA_Display_Ref ddca_dref,
      uint8_t **       pbytes);   // pointer into ddcutil data structures, do not free




//
// Get and Set VCP Feature Values
//

void
ddct_free_table_value_response(
      DDCA_Table_Value_Response * table_value_response);


// TODO: Choose between ddca_get_nontable_vcp_value()/ddca_get_table_vcp_value() vs ddca_get_vcp_value()

DDCA_Status
ddca_get_nontable_vcp_value(
       DDCA_Display_Handle             ddca_dh,
       VCP_Feature_Code                feature_code,
       DDCA_Non_Table_Value_Response * response);


// Implemented, but untested
DDCA_Status
ddca_get_table_vcp_value(
       DDCA_Display_Handle     ddca_dh,
       VCP_Feature_Code        feature_code,
       int *                   value_len,
       Byte**                  value_bytes);

DDCA_Status
ddca_get_vcp_value(
       DDCA_Display_Handle     ddca_dh,
       VCP_Feature_Code        feature_code,
       Vcp_Value_Type          call_type,   // TODO: elminate
       Single_Vcp_Value **     pvalrec);


DDCA_Status
ddca_get_formatted_vcp_value(
       DDCA_Display_Handle *   ddca_dh,
       VCP_Feature_Code        feature_code,
       char**                  p_formatted_value);


DDCA_Status
ddca_set_continuous_vcp_value(
      DDCA_Display_Handle      ddca_dh,
      VCP_Feature_Code         feature_code,
      int                      new_value);

DDCA_Status
ddca_set_simple_nc_vcp_value(
      DDCA_Display_Handle      ddca_dh,
      VCP_Feature_Code         feature_code,
      Byte                     new_value);

DDCA_Status
ddca_set_raw_vcp_value(
      DDCA_Display_Handle      ddca_dh,
      VCP_Feature_Code         feature_code,
      Byte                     hi_byte,
      Byte                     lo_byte);


#ifdef UNIMPLEMENTED
// Unimplemented
DDCA_Status
ddct_set_table_vcp_value(
      DDCA_Display_Handle  ddct_dh,
      VCP_Feature_Code     feature_code,
      int                  value_len,
      Byte *               value_bytes);
#endif

DDCA_Status
ddca_get_profile_related_values(
      DDCA_Display_Handle  ddca_dh,
      char**               pprofile_values_string);

DDCA_Status
ddca_set_profile_related_values(char *
      profile_values_string);



#ifdef __cplusplus
}
#endif
#endif /* DDCUTIL_C_API_H_ */
