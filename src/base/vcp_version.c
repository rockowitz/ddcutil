/* vcp_version.c
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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "util/string_util.h"

#include "base/core.h"

#include "base/vcp_version.h"


//
// MCCS version constants and utilities
//

const DDCA_MCCS_Version_Spec VCP_SPEC_V10       = {1,0};
const DDCA_MCCS_Version_Spec VCP_SPEC_V20       = {2,0};
const DDCA_MCCS_Version_Spec VCP_SPEC_V21       = {2,1};
const DDCA_MCCS_Version_Spec VCP_SPEC_V30       = {3,0};
const DDCA_MCCS_Version_Spec VCP_SPEC_V22       = {2,2};
const DDCA_MCCS_Version_Spec VCP_SPEC_UNKNOWN   = {0,0};    // value for monitor has been queried unsuccessfully
const DDCA_MCCS_Version_Spec VCP_SPEC_ANY       = {0,0};    // used as query specifier
const DDCA_MCCS_Version_Spec VCP_SPEC_UNQUERIED = {0xff, 0xff};


/* Tests if a Version_Spec value represents a valid MCCS version,
 * i.e. 1.0, 2.0, 2.1, 3.0, or 2.2.
 *
 * Arguments:
 *    vspec       value to test
 *
 * Returns:       true/false
 */
static bool is_known_vcp_spec(DDCA_MCCS_Version_Spec vspec) {
   bool result = vcp_version_eq(vspec, VCP_SPEC_V10) ||
                 vcp_version_eq(vspec, VCP_SPEC_V20) ||
                 vcp_version_eq(vspec, VCP_SPEC_V21) ||
                 vcp_version_eq(vspec, VCP_SPEC_V30) ||
                 vcp_version_eq(vspec, VCP_SPEC_V22);
   return result;
}


// Note that MCCS (VCP) versioning forms a directed graph, not a linear ordering.
//
// The v3.0 spec is an extension of v2.1, not v2.2.
// Both v3.0 and v2.2 are successors to v2.1.
//
//                      -- v3.0
//                     |
//   v1.0---v2.0---- v2.1
//                     |
//                      -- v2.2


/* Compares two version numbers.
 *
 * Aborts if an attempt is made to compare v2.2 with v3.0
 * Note: Will require modification if a new spec appears
 */
bool vcp_version_le(DDCA_MCCS_Version_Spec v1, DDCA_MCCS_Version_Spec v2) {
   bool result = false;
   assert( is_known_vcp_spec(v1) && is_known_vcp_spec(v2) );
   assert( !(vcp_version_eq(v1, VCP_SPEC_V22) && vcp_version_eq(v2, VCP_SPEC_V30)) &&
           !(vcp_version_eq(v2, VCP_SPEC_V22) && vcp_version_eq(v1, VCP_SPEC_V30))
         );

   if (v1.major < v2.major)
      result = true;
   else if (v1.major == v1.minor) {
      if (v1.minor <= v2.minor)
         result = true;
   }

#ifdef OLD
   assert (v1.major <= 3);
   assert (v2.major == 2 || v2.major == 3);

   if (v2.major == 2) {
      if (v1.major < 2)
         result = true;
      else
         result = (v1.minor <= v2.minor);
   }
   else if (v2.major == 3) {
      if (v1.major < 2)
         result = true;
      else if (v1.major == 2)
         result = (v1.minor <= 1);
      else
         result = (v1.minor <= v2.minor);
   }
   else
      PROGRAM_LOGIC_ERROR("Unsupported v2 v1 = %d.%d", v2.major, v2.minor);
#endif

   return result;
}

bool vcp_version_gt(DDCA_MCCS_Version_Spec val, DDCA_MCCS_Version_Spec min) {
   return !vcp_version_le(val,min);
}

bool vcp_version_eq(DDCA_MCCS_Version_Spec v1,  DDCA_MCCS_Version_Spec v2){
   return (v1.major == v2.major) && (v1.minor == v2.minor);
}

bool vcp_version_is_unqueried(DDCA_MCCS_Version_Spec vspec) {
   return (vspec.major == 0xff && vspec.minor == 0xff);
}


char * format_vspec(DDCA_MCCS_Version_Spec vspec) {
   static char private_buffer[20];
   if ( vcp_version_eq(vspec, VCP_SPEC_UNQUERIED) )
      SAFE_STRNCPY(private_buffer,  "Unqueried", sizeof(private_buffer));
   else if ( vcp_version_eq(vspec, VCP_SPEC_UNKNOWN) )
      strcpy(private_buffer,  "Unknown");     // will coverity flag this?
   else
      snprintf(private_buffer, 20, "%d.%d", vspec.major, vspec.minor);
   return private_buffer;
}


char * format_vcp_version_id(DDCA_MCCS_Version_Id version_id) {
   char * result = NULL;
   switch (version_id) {
   case DDCA_V10:    result = "1.0";     break;
   case DDCA_V20:    result = "2.0";     break;
   case DDCA_V21:    result = "2.1";     break;
   case DDCA_V30:    result = "3.0";     break;
   case DDCA_V22:    result = "2.2";     break;
   case DDCA_VNONE:  result = "unknown"; break;
   }
   return result;
}

char * vcp_version_id_name(DDCA_MCCS_Version_Id version_id) {
   char * result = NULL;
   switch (version_id) {
   case DDCA_V10:    result = "DDCA_V10";     break;
   case DDCA_V20:    result = "DDCA_V20";     break;
   case DDCA_V21:    result = "DDCA_V21";     break;
   case DDCA_V30:    result = "DDCA_V30";     break;
   case DDCA_V22:    result = "DDCA_V22";     break;
   case DDCA_VNONE:  result = "DDCA_VNONE";   break;
   }
   return result;
}


DDCA_MCCS_Version_Spec parse_vspec(char * s) {
   DDCA_MCCS_Version_Spec vspec;
   int ct = sscanf(s, "%hhd . %hhd", &vspec.major, &vspec.minor);
   if (ct != 2 || vspec.major > 3 || vspec.minor > 2) {
      vspec = VCP_SPEC_UNKNOWN;
   }
   return vspec;
}



DDCA_MCCS_Version_Id mccs_version_spec_to_id(DDCA_MCCS_Version_Spec vspec) {
   DDCA_MCCS_Version_Id result = DDCA_VUNK;    // initialize to avoid compiler warning

   if (vspec.major == 1 && vspec.minor == 0)
      result = DDCA_V10;
   else if (vspec.major == 2 && vspec.minor == 0)
      result = DDCA_V20;
   else if (vspec.major == 2 && vspec.minor == 1)
      result = DDCA_V21;
   else if (vspec.major == 3 && vspec.minor == 0)
      result = DDCA_V30;
   else if (vspec.major == 2 && vspec.minor == 2)
      result = DDCA_V22;
   else if (vspec.major == 2 && vspec.minor == 1)
      result = DDCA_V21;
   else if (vspec.major == 0 && vspec.minor == 0)
      result = DDCA_VUNK;
   // case UNQUERIED should never arise
   else
      PROGRAM_LOGIC_ERROR("Unexpected version spec value %d.%d", vspec.major, vspec.minor);

   return result;
}


DDCA_MCCS_Version_Spec mccs_version_id_to_spec(DDCA_MCCS_Version_Id id) {
   DDCA_MCCS_Version_Spec vspec = VCP_SPEC_ANY;
   // use table instead?
   switch(id) {
   case DDCA_VANY:   vspec = VCP_SPEC_ANY;    break;
   case DDCA_V10:    vspec = VCP_SPEC_V10;    break;
   case DDCA_V20:    vspec = VCP_SPEC_V20;    break;
   case DDCA_V21:    vspec = VCP_SPEC_V21;    break;
   case DDCA_V30:    vspec = VCP_SPEC_V30;    break;
   case DDCA_V22:    vspec = VCP_SPEC_V22;    break;
   }
   DDCA_MCCS_Version_Spec converted;
   converted.major = vspec.major;
   converted.minor = vspec.minor;

   return converted;
}




