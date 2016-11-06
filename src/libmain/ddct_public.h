/* ddct_public.h
 *
 * Initial cut at a public library API.
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

#ifndef DDCT_PUBLIC_H_
#define DDCT_PUBLIC_H_

#include <stdbool.h>
#include <stdio.h>

#include "util/coredefs.h"

// #include "base/ddc_base_defs.h"
// #include "base/displays.h"
// #include "base/ddc_packets.h"      // temp, for Interpreted_Vcp_Code


//
// General
//

typedef struct {
   int    major;
   int    minor;
   int    build;
} DDCT_Version_Spec;

typedef struct {
   int    major;
   int    minor;
} DDCT_MCCS_Version_Spec;


void ddct_init(void);

const char * ddct_ddcutil_version_string();
#ifdef UNIMPLEMENTED
DDCT_Version_Spec ddct_get_version(void);       // ddcutil version
#endif

bool ddct_built_with_adl(void);
#ifdef UNIMPLEMENTED
#define  DDCT_BUILT_WITH_ADL  0x01
unsigned long ddct_get_global_flags(void);
// or: more generalizable: return a byte of flags, one of which is DDCT_SUPPORTS_ADL
#endif


void ddct_set_fout(FILE * fout);
void ddct_set_ferr(FILE * ferr);


typedef int DDCT_Status;    // for now
char * ddct_status_code_name(DDCT_Status status_code);
char * ddct_status_code_desc(DDCT_Status status_code);

typedef Byte VCP_Feature_Code;


// Get and set max retries
typedef enum{DDCT_WRITE_ONLY_TRIES, DDCT_WRITE_READ_TRIES, DDCT_MULTI_PART_TRIES} DDCT_Retry_Type;
int  ddct_get_max_tries(DDCT_Retry_Type retry_type);
DDCT_Status ddct_set_max_tries(DDCT_Retry_Type retry_type, int max_tries);

// Get and set timeouts
typedef enum{DDCT_TIMEOUT_STANDARD, DDCT_TIMEOUT_TABLE_RETRY} DDCT_Timeout_Type;
#ifdef UNIMPLEMENTED
// Unimplemented:
int  ddct_get_timeout_millis(DDCT_Timeout_Type timeout_type);
void ddct_set_timeout_millis(DDCT_Timeout_Type timeout_type, int millisec);
#endif


//
// Message level control
//

// Values assigned to constants allow them to be or'd in bit flags
// Values are ascending in order of verbosity, except for OL_DEFAULT
// matches enum Output_Level in core.h
typedef enum {DDCT_OL_DEFAULT=0x01,
              DDCT_OL_PROGRAM=0x02,
              DDCT_OL_TERSE  =0x04,
              DDCT_OL_NORMAL =0x08,
              DDCT_OL_VERBOSE=0x10
} DDCT_Output_Level;

DDCT_Output_Level ddct_get_output_level();
void         ddct_set_output_level(DDCT_Output_Level newval);
char *       ddct_output_level_name(DDCT_Output_Level val);

// DDC Error reporting
void ddct_set_report_ddc_errors(bool onoff);
bool ddct_get_report_ddc_errors();


//
// Reports
//

int ddct_report_active_displays(int depth);



//
// VCP Feature Description
//

// flags for ddct_get_feature_info():
#define DDCT_CONTINUOUS   0x4000
#define DDCT_SIMPLE_NC    0x2000
#define DDCT_COMPLEX_NC   0x1000
#define DDCT_NC           (DDCT_SIMPLE_NC | DDCT_COMPLEX_NC)
#define DDCT_TABLE        0x0800
#define DDCT_KNOWN        (DDCT_CONTINUOUS | DDCT_NC | DDCT_TABLE)
#define DDCT_RO           0x0400
#define DDCT_WO           0x0200
#define DDCT_RW           0x0100
#define DDCT_READABLE     (DDCT_RO | DDCT_RW)
#define DDCT_WRITABLE     (DDCT_WO | DDCT_RW)

DDCT_Status ddct_get_feature_info_by_vcp_version(
      VCP_Feature_Code feature_code,
      DDCT_MCCS_Version_Spec     vspec,
      unsigned long *  flags);

char *      ddct_get_feature_name(VCP_Feature_Code feature_code);

typedef void * Feature_Value_Table;   // temp


#ifdef UNIMPLEMENTED

// Unimplemented
DDCT_Status ddct_get_feature_sl_value_table(
               DDCT_Display_Handle   ddct_dh,
               VCP_Feature_Code      feature_code,
               Feature_Value_Table * value_table);

// Unimplemented
DDCT_Status ddct_get_supported_feature_sl_value_table(
               DDCT_Display_Handle   ddct_dh,
               VCP_Feature_Code      feature_code,
               Feature_Value_Table * value_table);

// Unimplemented
DDCT_Status ddct_is_feature_supported(
      DDCT_Display_Handle   ddct_dh,
      VCP_Feature_Code      feature_code,
      bool *                answer);

#endif



// ===





// Display_Info_List ddct_get_displays();


//
// Display Identifiers
//

typedef void * DDCT_Display_Identifier;      // opaque
DDCT_Status ddct_create_dispno_display_identifier(
               int dispno,
               DDCT_Display_Identifier* pdid);
DDCT_Status ddct_create_adlno_display_identifier(
               int iAdapterIndex,
               int iDisplayIndex,
               DDCT_Display_Identifier* pdid);
DDCT_Status ddct_create_busno_display_identifier(
               int busno,
               DDCT_Display_Identifier* pdid);
DDCT_Status ddct_create_model_sn_display_identifier(
               const char * model,
               const char * sn,
               DDCT_Display_Identifier* pdid);
DDCT_Status ddct_create_edid_display_identifier(
               const Byte * edid,
               DDCT_Display_Identifier * pdid);      // 128 byte edid
DDCT_Status ddct_create_usb_display_identifier(
               int bus,
               int device,
               DDCT_Display_Identifier* pdid);
DDCT_Status ddct_free_display_identifier(DDCT_Display_Identifier ddct_did);
DDCT_Status ddct_repr_display_identifier(DDCT_Display_Identifier ddct_did, char** repr);


//
// Display References
//

typedef void * DDCT_Display_Ref;
DDCT_Status ddct_get_display_ref(DDCT_Display_Identifier did, DDCT_Display_Ref* ddct_dref);
DDCT_Status ddct_free_display_ref(DDCT_Display_Ref ddct_ref);
DDCT_Status ddct_repr_display_ref(DDCT_Display_Ref ddct_dref, char** repr);
void        ddct_report_display_ref(DDCT_Display_Ref ddct_dref, int depth);


//
// Display Handles
//

typedef void * DDCT_Display_Handle;
DDCT_Status ddct_open_display(DDCT_Display_Ref dref, DDCT_Display_Handle * pdh);
DDCT_Status ddct_close_display(DDCT_Display_Handle ddct_dh);
DDCT_Status ddct_repr_display_handle(DDCT_Display_Handle ddct_dh, char** repr);


//
//  Miscellaneous Monitor Specific Functions
//

DDCT_Status ddct_get_mccs_version(DDCT_Display_Handle ddct_dh, DDCT_MCCS_Version_Spec* pspec);

// DDCT_Status ddct_get_edid(DDCT_Display_Handle * dh, Byte * edid_buffer);    // edid_buffer must be >= 128 bytes
DDCT_Status ddct_get_edid_by_display_ref(DDCT_Display_Ref ddct_dref, Byte ** pbytes);   // pointer into ddcutil data structures, do not free

// or return a struct?
DDCT_Status ddct_get_feature_info_by_display(
               DDCT_Display_Handle ddct_dh,
               VCP_Feature_Code    feature_code,
               unsigned long *     flags);




//
// Monitor Capabilities
//

DDCT_Status ddct_get_capabilities_string(DDCT_Display_Handle ddct_dh, char** buffer);

#ifdef UNIMPLEMENTED
// Unimplemented.  Parsed capabilities has a complex data structure.  How to make visible?
typedef void DDCT_Parsed_Capabilities;    // TEMP
DDCT_Status ddct_parse_capabilities_string(char * capabilities_string, DDCT_Parsed_Capabilities ** parsed_capabilities);
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

DDCT_Status ddct_get_nontable_vcp_value(
               DDCT_Display_Handle             ddct_dh,
               VCP_Feature_Code                feature_code,
               DDCT_Non_Table_Value_Response * response);

DDCT_Status ddct_set_continuous_vcp_value(
               DDCT_Display_Handle  ddct_dh,
               VCP_Feature_Code     feature_code,
               int                  new_value);

DDCT_Status ddct_set_simple_nc_vcp_value(
               DDCT_Display_Handle  ddct_dh,
               VCP_Feature_Code     feature_code,
               Byte                 new_value);

DDCT_Status ddct_set_raw_vcp_value(
               DDCT_Display_Handle  ddct_dh,
               VCP_Feature_Code     feature_code,
               Byte                 hi_byte,
               Byte                 lo_byte);

// Implemented, but untested
DDCT_Status ddct_get_table_vcp_value(
               DDCT_Display_Handle ddct_dh,
               VCP_Feature_Code    feature_code,
               int *               value_len,
               Byte**              value_bytes);

#ifdef UNIMPLEMENTED
// Unimplemented
DDCT_Status ddct_set_table_vcp_value(
               DDCT_Display_Handle  ddct_dh,
               VCP_Feature_Code     feature_code,
               int                  value_len,
               Byte *               value_bytes);
#endif

DDCT_Status ddct_get_profile_related_values(DDCT_Display_Handle ddct_dh, char** pprofile_values_string);

DDCT_Status ddct_set_profile_related_values(char * profile_values_string);


#endif /* DDCT_PUBLIC_H_ */
