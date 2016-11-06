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

const Version_Spec VCP_SPEC_V20       = {2,0};
const Version_Spec VCP_SPEC_V21       = {2,1};
const Version_Spec VCP_SPEC_V30       = {3,0};
const Version_Spec VCP_SPEC_V22       = {2,2};
const Version_Spec VCP_SPEC_UNKNOWN   = {0,0};    // value for monitor has been queried unsuccessfully
const Version_Spec VCP_SPEC_ANY       = {0,0};    // used as query specifier
const Version_Spec VCP_SPEC_UNQUERIED = {0xff, 0xff};

// addresses the fact that v3.0 spec is not a direct superset of 2.2
// both are greater than 2.1
// will require modification if a new spec appears
bool vcp_version_le(Version_Spec val, Version_Spec max) {
   bool result = false;
   assert (val.major <= 3);
   assert (max.major == 2 || max.major == 3);

   if (max.major == 2) {
      if (val.major < 2)
         result = true;
      else
         result = (val.minor <= max.minor);
   }
   else if (max.major == 3) {
      if (val.major < 2)
         result = true;
      else if (val.major == 2)
         result = (val.minor <= 1);
      else
         result = (val.minor <= max.minor);
   }
   else
      PROGRAM_LOGIC_ERROR("Unsupported max val = %d.%d", max.major, max.minor);

   return result;
}

bool vcp_version_gt(Version_Spec val, Version_Spec min) {
   return !vcp_version_le(val,min);
}

bool vcp_version_eq(Version_Spec v1,  Version_Spec v2){
   return (v1.major == v2.major) && (v1.minor == v2.minor);
}

bool is_vcp_version_unqueried(Version_Spec vspec) {
   return (vspec.major == 0xff && vspec.minor == 0xff);
}


char * format_vspec(Version_Spec vspec) {
   static char private_buffer[20];
   if ( vcp_version_eq(vspec, VCP_SPEC_UNQUERIED) )
      SAFE_STRNCPY(private_buffer,  "Unqueried", sizeof(private_buffer));
   else if ( vcp_version_eq(vspec, VCP_SPEC_UNKNOWN) )
      strcpy(private_buffer,  "Unknown");     // will coverity flag this?
   else
      snprintf(private_buffer, 20, "%d.%d", vspec.major, vspec.minor);
   return private_buffer;
}


Version_Spec parse_vspec(char * s) {
   Version_Spec vspec;
   int ct = sscanf(s, "%hhd . %hhd", &vspec.major, &vspec.minor);
   if (ct != 2 || vspec.major > 3 || vspec.minor > 2) {
      vspec = VCP_SPEC_UNKNOWN;
   }
   return vspec;
}
