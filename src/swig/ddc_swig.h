/* ddc_swig.h
 *
 * <copyright>
 * Copyright (C) 2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "libmain/ddct_public.h"


//
// Convert ddcutil status codes to Python exceptions
//

void   clear_exception();
char * check_exception();


//
// Global Settings
//

const char * ddcutil_version(void);
bool ddcs_built_with_adl(void);


//
// Reports
//

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

typedef DDCT_MCCS_Version_Spec DDCS_MCCS_Version_Spec;

unsigned long ddcs_get_feature_info_by_vcp_version(
               DDCS_VCP_Feature_Code    feature_code,
               DDCS_MCCS_Version_Spec   vspec);
char *        ddcs_get_feature_name(DDCS_VCP_Feature_Code feature_code);


//
// Display Identifiers
//

typedef void * DDCS_Display_Identifier_p;       // opaque

DDCS_Display_Identifier_p ddcs_create_dispno_display_identifier(
               int dispno);
DDCS_Display_Identifier_p ddcs_create_adlno_display_identifier(
               int iAdapterIndex,
               int iDisplayIndex);
DDCS_Display_Identifier_p ddcs_create_busno_display_identifier(
               int busno);
DDCS_Display_Identifier_p ddcs_create_model_sn_display_identifier(
               const char * model,
               const char * sn);
DDCS_Display_Identifier_p ddcs_create_edid_display_identifier(
               const Byte * edid);
DDCS_Display_Identifier_p ddcs_create_usb_display_identifier(
               int bus,
               int device);
void ddcs_free_display_identifier(DDCS_Display_Identifier_p ddcs_did);
char * ddcs_repr_display_identifier(DDCS_Display_Identifier_p ddcs_did);


//
// Display References
//

typedef void * DDCS_Display_Ref_p;     // opaque

DDCS_Display_Ref_p ddcs_get_display_ref(   DDCS_Display_Identifier_p did);
void               ddcs_free_display_ref(  DDCS_Display_Ref_p dref);
char *             ddcs_repr_display_ref(  DDCS_Display_Ref_p dref);
void               ddcs_report_display_ref(DDCS_Display_Ref_p dref, int depth);


//
// Display Handles
//

typedef void * DDCS_Display_Handle_p;   // opaque

DDCS_Display_Handle_p ddcs_open_display(DDCS_Display_Ref_p dref);
void ddcs_close_display(DDCS_Display_Handle_p dh);
char * ddcs_repr_display_handle(DDCS_Display_Handle_p dh);


//
// Miscellaneous Display Specific Functions
//

DDCS_MCCS_Version_Spec ddcs_get_mccs_version(DDCS_Display_Handle_p dh);

// DEPRECATED
unsigned long ddcs_get_feature_info_by_display(
               DDCS_Display_Handle_p    dh,
               DDCS_VCP_Feature_Code    feature_code);


//
// Capabilities
//

char * ddcs_get_capabilities_string(DDCS_Display_Handle_p dh);


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
} DDCS_Non_Table_Value_Response;

DDCS_Non_Table_Value_Response ddcs_get_nontable_vcp_value(
               DDCS_Display_Handle_p   dh,
               DDCS_VCP_Feature_Code   feature_code);

void ddcs_set_nontable_vcp_value(
               DDCS_Display_Handle_p   dh,
               VCP_Feature_Code        feature_code,
               int                     new_value);

char * ddcs_get_profile_related_values(DDCS_Display_Handle_p dh);

void ddcs_set_profile_related_values(char * profile_values_string);

#endif /* DDC_SWIG_H_ */
