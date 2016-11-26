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

#include <stdbool.h>
#include <stdio.h>

#include "util/coredefs.h"


/** @file ddct_public.h
 *  @brief ddcutil public C API
 *
 *  Function names in public API begin with "ddca_"
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
 * Indicates whether the ddcutil library was built with ADL support. .
 */
bool ddca_built_with_adl(void);

/**
 * Indicates whether the ddcutil library was built with support for USB connected monitors. .
 */
bool ddca_built_with_usb(void);

/** @brief ddcutil was built with support for AMD Display Library connected monitors */
#define DDCA_BUILT_WITH_ADL     0x01
/** ddcutil was built with support for USB connected monitors */
#define DDCA_BUILT_WITH_USB     0x02
#define DDCA_BUILT_WITH_FAILSIM 0x04  /**< @brief ddcutil was built with support for failure simulation */


/** Queries ddcutil library build options.
 *
 */
unsigned long ddca_get_build_options(void);


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

typedef int DDCA_Status;
/** Returns the name for a ddcutil status code */
char * ddca_status_code_name(DDCA_Status status_code);
/** Returns a description of a ddcutil status code */
char * ddca_status_code_desc(DDCA_Status status_code);


// Global Settings

// Get and set timeouts
typedef enum{DDCA_TIMEOUT_STANDARD, DDCA_TIMEOUT_TABLE_RETRY} DDCA_Timeout_Type;
int  ddca_get_timeout_millis(DDCA_Timeout_Type timeout_type);
void ddca_set_timeout_millis(DDCA_Timeout_Type timeout_type, int millisec);

// Get and set max retries
typedef enum{DDCA_WRITE_ONLY_TRIES, DDCA_WRITE_READ_TRIES, DDCA_MULTI_PART_TRIES} DDCA_Retry_Type;
int         ddca_get_max_tries(DDCA_Retry_Type retry_type);
DDCA_Status ddca_set_max_tries(DDCA_Retry_Type retry_type, int max_tries);


//
// Message Control
//

/**
 * Redirects output that normally would go to STDOUT
 */
void ddca_set_fout(
      FILE * fout);   /**< where to write normal messages, if NULL, suppress  */

/** Redirect output that normally goes to STDOUT back to STDOUT */
void ddca_set_fout_to_default();

/**
 * Redirects output that normally would go to STDERR
 */
void ddca_set_ferr(
      FILE * ferr);   /**< where to write error messages, If NULL, suppress */

/**
 * Redirect output that normally goes to STDERR back to STDERR
 */

/** Output Level
 *
 * Values assigned to constants allow them to be or'd in bit flags.
 */
 // matches enum Output_Level in core.h
typedef enum {DDCA_OL_DEFAULT=0x01,   /**< used how? */
              DDCA_OL_PROGRAM=0x02,   /**< Special level for programmatic use */
              DDCA_OL_TERSE  =0x04,   /**< Brief   output  */
              DDCA_OL_NORMAL =0x08,   /**< Normal  output */
              DDCA_OL_VERBOSE=0x10    /**< Verbose output */
} DDCA_Output_Level;

/** Gets the current output level */
DDCA_Output_Level ddca_get_output_level();

/** Sets the output level */
void              ddca_set_output_level(
                      DDCA_Output_Level newval);   /**< new output level */

/** Gets the name of an output level
 * @param  val  output level id
 * @return output level name (do not free)
 */
char *            ddca_output_level_name(
      DDCA_Output_Level val);     /**< output level id */

// DDC Error reporting
/** Control messages describing DDC protocol errors
 * @param onoff    if true, errors will be issued
 * */
void ddca_enable_report_ddc_errors(bool onoff);

/** Checks whether messages describing DDC protocol errors are to output */
bool ddca_is_report_ddc_errors_enabled();



//
// Display Identifiers
//

/** Opaque handle to display identifier */
typedef void * DDCA_Display_Identifier;

/** Creates a display identifier using the display number assigned by ddcutil
 * @param[in]  dispno  display number
 * @param[out] pdid    where to return display identifier handle
 * @return status code
 * */
DDCA_Status ddca_create_dispno_display_identifier(
               int dispno,
               DDCA_Display_Identifier* pdid);

/** Creates a display identifier using an ADL (adapter index, display index) pair
 * @param[in]  iAdapterIndex ADL adapter index
 * @param[in]  iDisplayIndex ADL display index
 * @param[out] pdid          where to return display identifier handle
 * @return     status code
 */
DDCA_Status ddca_create_adlno_display_identifier(
               int iAdapterIndex,
               int iDisplayIndex,
               DDCA_Display_Identifier* pdid);

/** Creates a display identifier using an I2C bus number
 * @param[in]  busno  I2C bus number
 * @param[out] pdid   where to return display identifier handle
 * @return            status code
 * */
DDCA_Status ddca_create_busno_display_identifier(
               int busno,
               DDCA_Display_Identifier* pdid);

/** Creates a display identifier using the model name string and serial number string
 * @param model  model name string
 * @param sn     serial number string
 * @param pdid   where to return display identifier handle
 * @return       status code
 */
DDCA_Status ddca_create_model_sn_display_identifier(
               const char * model,
               const char * sn,
               DDCA_Display_Identifier* pdid);

/** Creates a display identifier using a 128 byte EDID
 * @param edid  pointer to 128 byte EDID
 * @param pdid  where to return display identifier handle
 * @return      status code
 */
DDCA_Status ddca_create_edid_display_identifier(
               const Byte * edid,
               DDCA_Display_Identifier * pdid);      // 128 byte edid

/** Creates a display identifier using a USB bus number and device number
 * @param bus    USB bus number
 * @param device USB device number
 * @param pdid   where to return display identifier handle
 * @return       status code
 */
DDCA_Status ddca_create_usb_display_identifier(
               int bus,
               int device,
               DDCA_Display_Identifier* pdid);

/** Release the memory of a display identifier */
DDCA_Status ddca_free_display_identifier(DDCA_Display_Identifier ddca_did);

/** Returns a string representation of a display identifier */
DDCA_Status ddca_repr_display_identifier(DDCA_Display_Identifier ddca_did, char** repr);


//
// Display References
//

typedef void * DDCT_Display_Ref;
DDCA_Status ddca_create_display_ref(DDCA_Display_Identifier did, DDCT_Display_Ref* ddct_dref);
DDCA_Status ddca_free_display_ref(DDCT_Display_Ref ddct_ref);
DDCA_Status ddca_repr_display_ref(DDCT_Display_Ref ddct_dref, char** repr);
void        ddct_report_display_ref(DDCT_Display_Ref ddct_dref, int depth);


//
// Display Handles
//

typedef void * DDCT_Display_Handle;
DDCA_Status ddct_open_display(DDCT_Display_Ref dref, DDCT_Display_Handle * pdh);
DDCA_Status ddct_close_display(DDCT_Display_Handle ddct_dh);
DDCA_Status ddct_repr_display_handle(DDCT_Display_Handle ddct_dh, char** repr);







// VCP Feature Information

typedef struct {
   Byte    major;
   Byte    minor;
} DDCT_MCCS_Version_Spec;

// in sync w constants MCCS_V.. in vcp_feature_codes.c
/** MCCS (VCP) Feature Version IDs */
typedef enum {DDCA_VANY = 0,          /**< Match any MCCS version */
              DDCA_V10  = 1,          /**< MCCS v1.0 */
              DDCA_V20  = 2,          /**< MCCS v2.0 */
              DDCA_V21  = 4,          /**< MCCS v2.1 */
              DDCA_V30  = 8,          /**< MCCS v3.0 */
              DDCA_V22  =16           /**< MCCS v2.2 */
} DDCA_MCCS_Version_Id;

#define DDCA_VUNK DDCA_VANY


typedef Byte VCP_Feature_Code;

//
// VCP Feature Description
//

// flags for ddca_get_feature_info():
#define DDCA_CONTINUOUS   0x4000    /**< Continuous feature */
#define DDCA_SIMPLE_NC    0x2000    /**< Non-continuous feature, having a define list of values in byte SL */
#define DDCA_COMPLEX_NC   0x1000    /**< Non-continuous feature, having a complex interpretation using one or more of SL, SH, ML, MH */
#define DDCA_NC           (DDCA_SIMPLE_NC | DDCA_COMPLEX_NC)  /**< Non-continous feature, of any type */
#define DDCA_TABLE        0x0800    /**< Table type feature */
#define DDCA_KNOWN        (DDCA_CONTINUOUS | DDCA_NC | DDCA_TABLE)
#define DDCA_RO           0x0400    /**< Read only feature */
#define DDCA_WO           0x0200    /**< Write only feature */
#define DDCA_RW           0x0100    /**< Feature is both readable and writable */
#define DDCA_READABLE     (DDCA_RO | DDCA_RW)  /**< Feature is either RW or RO */
#define DDCA_WRITABLE     (DDCA_WO | DDCA_RW)  /**< Feature is either RW or WO */

DDCA_Status ddca_get_feature_info_by_vcp_version(
      VCP_Feature_Code           feature_code,
      // DDCT_MCCS_Version_Spec     vspec,
      DDCA_MCCS_Version_Id       mccs_version_id,
      unsigned long *            flags);

char *      ddca_get_feature_name(VCP_Feature_Code feature_code);

typedef void * Feature_Value_Table;   // temp


#ifdef UNIMPLEMENTED

// Unimplemented
DDCA_Status ddct_get_feature_sl_value_table(
               DDCT_Display_Handle   ddct_dh,
               VCP_Feature_Code      feature_code,
               Feature_Value_Table * value_table);

// Unimplemented
DDCA_Status ddct_get_supported_feature_sl_value_table(
               DDCT_Display_Handle   ddct_dh,
               VCP_Feature_Code      feature_code,
               Feature_Value_Table * value_table);

// Unimplemented
DDCA_Status ddct_is_feature_supported(
      DDCT_Display_Handle   ddct_dh,
      VCP_Feature_Code      feature_code,
      bool *                answer);

#endif



//
//  Miscellaneous Monitor Specific Functions
//

DDCA_Status ddct_get_mccs_version(DDCT_Display_Handle ddct_dh, DDCT_MCCS_Version_Spec* pspec);

// DDCT_Status ddct_get_edid(DDCT_Display_Handle * dh, Byte * edid_buffer);    // edid_buffer must be >= 128 bytes
DDCA_Status ddct_get_edid_by_display_ref(DDCT_Display_Ref ddct_dref, Byte ** pbytes);   // pointer into ddcutil data structures, do not free

// or return a struct?
DDCA_Status ddca_get_feature_info_by_display(
               DDCT_Display_Handle ddct_dh,
               VCP_Feature_Code    feature_code,
               unsigned long *     flags);





//
// Reports
//

int ddca_report_active_displays(int depth);



// Display_Info_List ddct_get_displays();




//
// Monitor Capabilities
//

DDCA_Status ddct_get_capabilities_string(DDCT_Display_Handle ddct_dh, char** buffer);

#ifdef UNIMPLEMENTED
// Unimplemented.  Parsed capabilities has a complex data structure.  How to make visible?
typedef void DDCT_Parsed_Capabilities;    // TEMP
DDCA_Status ddct_parse_capabilities_string(char * capabilities_string, DDCT_Parsed_Capabilities ** parsed_capabilities);
#endif


//
// Get and Set VCP Feature Values
//

typedef struct {
   Byte  mh;
   Byte  ml;
   Byte  sh;
   Byte  sl;
   int   max_value;
   int   cur_value;
} DDCT_Non_Table_Value_Response;

typedef struct {
   int   bytect;
   Byte  bytes[];     // or Byte * ?
} DDCT_Table_Value_Response;

void ddct_free_table_value_response(DDCT_Table_Value_Response * table_value_response);

DDCA_Status ddct_get_nontable_vcp_value(
               DDCT_Display_Handle             ddct_dh,
               VCP_Feature_Code                feature_code,
               DDCT_Non_Table_Value_Response * response);

DDCA_Status ddct_set_continuous_vcp_value(
               DDCT_Display_Handle  ddct_dh,
               VCP_Feature_Code     feature_code,
               int                  new_value);

DDCA_Status ddct_set_simple_nc_vcp_value(
               DDCT_Display_Handle  ddct_dh,
               VCP_Feature_Code     feature_code,
               Byte                 new_value);

DDCA_Status ddct_set_raw_vcp_value(
               DDCT_Display_Handle  ddct_dh,
               VCP_Feature_Code     feature_code,
               Byte                 hi_byte,
               Byte                 lo_byte);

// Implemented, but untested
DDCA_Status ddct_get_table_vcp_value(
               DDCT_Display_Handle ddct_dh,
               VCP_Feature_Code    feature_code,
               int *               value_len,
               Byte**              value_bytes);

#ifdef UNIMPLEMENTED
// Unimplemented
DDCA_Status ddct_set_table_vcp_value(
               DDCT_Display_Handle  ddct_dh,
               VCP_Feature_Code     feature_code,
               int                  value_len,
               Byte *               value_bytes);
#endif

DDCA_Status ddct_get_profile_related_values(DDCT_Display_Handle ddct_dh, char** pprofile_values_string);

DDCA_Status ddct_set_profile_related_values(char * profile_values_string);


#endif /* DDCUTIL_C_API_H_ */
