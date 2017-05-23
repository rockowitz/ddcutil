/* ddc_swig.h
 *
 * <copyright>
 * Copyright (C) 2016-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef DDC_SWIG_H_
#define DDC_SWIG_H_

#include <Python.h>

#include "public/ddcutil_types.h"
#include "public/ddcutil_c_api.h"

// Initialization

void ddcs_init(void);


// Convert ddcutil status codes to Python exceptions

void   clear_exception();
char * check_exception();
bool   check_exception2();


//
//  Build Information
//

const char * ddcs_ddcutil_version_string();

typedef uint8_t FlagsByte;

#ifdef OLD
bool ddcs_built_with_adl(void);
bool ddcs_built_with_usb(void);
#endif

// #define DDCS_BUILT_WITH_ADL     DDCA_BUILT_WITH_ADL
// #define DDCS_BUILT_WITH_USB     DDCA_BUILT_WITH_USB
// #define DDCS_BUILT_WITH_FAILSIM DDCA_BUILT_WITH_FAILSIM

typedef enum {DDCA_HAS_ADL      = DDCA_BUILT_WITH_ADL,
              DDCA_HAS_USB      = DDCA_BUILT_WITH_USB,
              DDCA_HAS_FAILSIM  = DDCA_BUILT_WITH_FAILSIM} DDCS_Build_Flags;
FlagsByte ddcs_get_build_options(void);

#ifdef NO
// use this to get something that SWIG understands?  correct order?  ugly
typedef struct {
   unsigned int pad:                5;
   unsigned int build_with_failsim: 1;
   unsigned int built_with_usb:     1;
   unsigned int built_with_adl:     1;
} DDCS_Build_Flags_Struct;
#endif

//
// Global Settings
//

#ifndef PYTHON3
// PyFileObject not defined in Python 3, how to handle?
// void ddc_set_fout(PyFileObject *fpy);
void ddcs_set_fout(FILE * fout);
// void ddcs_set_fout(void * fpy);

void save_current_python_fout(PyFileObject * pfy);
PyFileObject * get_current_python_fout();
#endif

// Reports

int ddcs_report_active_displays(int depth);


//
// VCP Feature Information
//

typedef int DDCS_VCP_Feature_Code;

#ifdef OLD
typedef struct {
   int    major;
   int    minor;
} DDCS_MCCS_Version_Spec;
#endif

typedef DDCA_MCCS_Version_Spec DDCS_MCCS_Version_Spec;

#ifdef TO_REIMPLEMENT
Version_Feature_Flags ddcs_get_feature_info_by_vcp_version(
               DDCS_VCP_Feature_Code    feature_code,
               DDCA_MCCS_Version_Id     version_id);
#endif
char *        ddcs_get_feature_name(DDCS_VCP_Feature_Code feature_code);


//
// Display Identifiers
//

typedef void * DDCS_Display_Identifier;       // opaque

DDCS_Display_Identifier ddcs_create_dispno_display_identifier(
               int dispno);
DDCS_Display_Identifier ddcs_create_adlno_display_identifier(
               int iAdapterIndex,
               int iDisplayIndex);
DDCS_Display_Identifier ddcs_create_busno_display_identifier(
               int busno);
DDCS_Display_Identifier ddcs_create_mfg_model_sn_display_identifier(
               const char * mfg_id,
               const char * model,
               const char * sn);
DDCS_Display_Identifier ddcs_create_edid_display_identifier(
               const uint8_t * byte_buffer,
               int bytect);
DDCS_Display_Identifier ddcs_create_usb_display_identifier(
               int bus,
               int device);
void   ddcs_free_display_identifier(DDCS_Display_Identifier ddcs_did);
char * ddcs_repr_display_identifier(DDCS_Display_Identifier ddcs_did);


//
// Display References
//

typedef void * DDCS_Display_Ref;     // opaque

DDCS_Display_Ref ddcs_get_display_ref(   DDCS_Display_Identifier did);
void             ddcs_free_display_ref(  DDCS_Display_Ref dref);
char *           ddcs_repr_display_ref(  DDCS_Display_Ref dref);
void             ddcs_report_display_ref(DDCS_Display_Ref dref, int depth);


//
// Display Handles
//

typedef void * DDCS_Display_Handle;   // opaque

DDCS_Display_Handle ddcs_open_display(DDCS_Display_Ref dref);
void                ddcs_close_display(DDCS_Display_Handle dh);
char *              ddcs_repr_display_handle(DDCS_Display_Handle dh);


//
// Miscellaneous Display Specific Functions
//

DDCS_MCCS_Version_Spec ddcs_get_mccs_version(DDCS_Display_Handle dh);

#ifdef OLD
// DEPRECATED
unsigned long ddcs_get_feature_info_by_display(
               DDCS_Display_Handle    dh,
               DDCS_VCP_Feature_Code    feature_code);
#endif


//
// Capabilities
//

char * ddcs_get_capabilities_string(DDCS_Display_Handle dh);


//
// Get and Set VCP Feature Values
//

typedef struct {
   uint8_t  mh;
   uint8_t  ml;
   uint8_t  sh;
   uint8_t  sl;
   int   max_value;
   int   cur_value;
} DDCS_Non_Table_Value_Response;

DDCS_Non_Table_Value_Response ddcs_get_nontable_vcp_value(
               DDCS_Display_Handle   dh,
               DDCS_VCP_Feature_Code   feature_code);

void ddcs_set_nontable_vcp_value(
               DDCS_Display_Handle   dh,
               DDCA_Vcp_Feature_Code        feature_code,
               int                     new_value);

char * ddcs_get_profile_related_values(DDCS_Display_Handle dh);

void ddcs_set_profile_related_values(char * profile_values_string);

#endif /* DDC_SWIG_H_ */
