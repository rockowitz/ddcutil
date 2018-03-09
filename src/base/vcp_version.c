/* vcp_version.c
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \file
 * VCP/MCCS Version Specification
 *
 */

/** \cond */
#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
/** \endcond */

#include "util/data_structures.h"
#include "util/string_util.h"

#include "base/core.h"

#include "base/vcp_version.h"


//
// MCCS version constants and utilities
//

const DDCA_MCCS_Version_Spec VCP_SPEC_V10       = {1,0};   ///< MCCS version 1.0
const DDCA_MCCS_Version_Spec VCP_SPEC_V20       = {2,0};   ///< MCCS version 2.0
const DDCA_MCCS_Version_Spec VCP_SPEC_V21       = {2,1};   ///< MCCS version 2.1
const DDCA_MCCS_Version_Spec VCP_SPEC_V30       = {3,0};   ///< MCCS version 3.0
const DDCA_MCCS_Version_Spec VCP_SPEC_V22       = {2,2};   ///< MCCS version 2.2
const DDCA_MCCS_Version_Spec VCP_SPEC_UNKNOWN   = {0,0};   ///< value for monitor has been queried unsuccessfully
const DDCA_MCCS_Version_Spec VCP_SPEC_ANY       = {0,0};   ///< used as query specifier
const DDCA_MCCS_Version_Spec VCP_SPEC_UNQUERIED = {0xff, 0xff}; ///< indicates version not queried


/* Tests if a #DDCA_MCCS_Version_Spec value represents a valid MCCS version,
 * i.e. 1.0, 2.0, 2.1, 3.0, or 2.2.
 *
 *  @param   vspec       value to test
 *
 *  @return  true/false
 */
bool is_known_vcp_spec(DDCA_MCCS_Version_Spec vspec) {
   bool result = vcp_version_eq(vspec, VCP_SPEC_V10) ||
                 vcp_version_eq(vspec, VCP_SPEC_V20) ||
                 vcp_version_eq(vspec, VCP_SPEC_V21) ||
                 vcp_version_eq(vspec, VCP_SPEC_V30) ||
                 vcp_version_eq(vspec, VCP_SPEC_V22);
   return result;
}

/** \file
* Note that MCCS (VCP) versioning forms a directed graph, not a linear ordering.
*
* The v3.0 spec is an extension of v2.1, not v2.2.
* Both v3.0 and v2.2 are successors to v2.1.
*
*                      -- v3.0
*                     |
*   v1.0---v2.0---- v2.1
*                     |
*                      -- v2.2
*
*/


/** Checks if one #DDCA_MCCS_Version_Spec is less than or equal
 *  to another.
 *
 *  @param v1  first version spec
 *  @param v2  second version spec
 *
 *  @retval true  v1 is <= v2
 *  @retval false v1 > v2
 *
 *  @remark
 * Aborts if an attempt is made to compare v2.2 with v3.0
 * @remark
 * Will require modification if a new spec appears
 */
bool vcp_version_le(DDCA_MCCS_Version_Spec v1, DDCA_MCCS_Version_Spec v2) {
   bool debug = false;

   bool result = false;
   assert( is_known_vcp_spec(v1) && is_known_vcp_spec(v2) );
   assert( !(vcp_version_eq(v1, VCP_SPEC_V22) && vcp_version_eq(v2, VCP_SPEC_V30)) &&
           !(vcp_version_eq(v2, VCP_SPEC_V22) && vcp_version_eq(v1, VCP_SPEC_V30))
         );

   if (v1.major < v2.major)
      result = true;
   else if (v1.major == v2.major) {
      if (v1.minor <= v2.minor)
         result = true;
   }

   DBGMSF(debug, "v1=%d.%d <= v2=%d.%d returning: %s",
                 v1.major, v2.minor,
                 v2.major, v2.minor,
                 bool_repr(result));
   return result;
}


/** CHecks if one #DDCA_MCCS_Version_Spec is greater than another.
 *
 * See \see vcp_version_le for discussion of version comparison
 *
 * @param val  first version
 * @param min  second version
 * @return true/false
 */
bool vcp_version_gt(DDCA_MCCS_Version_Spec val, DDCA_MCCS_Version_Spec min) {
   return !vcp_version_le(val,min);
}


/** Test if two DDCA_MCCS_VersionSpec values are identical.
 * @param v1 first version
 * @param v2 second version
 * @return true/false
 */
bool vcp_version_eq(DDCA_MCCS_Version_Spec v1,  DDCA_MCCS_Version_Spec v2){
   return (v1.major == v2.major) && (v1.minor == v2.minor);
}


/** Tests if a #DDCA_MCCS_Version_Spec represents "unqueried".
 *
 *  Encapsulates the use of a magic number.
 *
 *  @param vspec  version spec to test
 *  @result true/false
 */
bool vcp_version_is_unqueried(DDCA_MCCS_Version_Spec vspec) {
   return (vspec.major == 0xff && vspec.minor == 0xff);
}


/** Converts a #DDCA_MCCS_Version_Spec to a printable string,
 *  handling the special values for unqueried and unknown.
 *
 *  @param vspec version spec
 *  @return string in the form "2.0"
 *  @retval "Unqueried" for VCP_SPEC_UNQUERIED
 *  @retval "Unknown" for VCP_SPEC_UNKNOWN
 */
char * format_vspec(DDCA_MCCS_Version_Spec vspec) {
   static GPrivate  format_vspec_key = G_PRIVATE_INIT(g_free);
   char * private_buffer = get_thread_fixed_buffer(&format_vspec_key, 20);

   if ( vcp_version_eq(vspec, VCP_SPEC_UNQUERIED) )
      g_strlcpy(private_buffer,  "Unqueried", 20);  // g_strlcpy() to quiet coverity
   else if ( vcp_version_eq(vspec, VCP_SPEC_UNKNOWN) )
      strcpy(private_buffer,  "Unknown");     // will coverity flag this?
   else
      SAFE_SNPRINTF(private_buffer, 20, "%d.%d", vspec.major, vspec.minor);
   // DBGMSG("Returning: |%s|", private_buffer);
   return private_buffer;
}


// new way:

Value_Name_Title_Table version_id_table = {
      VNT(DDCA_MCCS_V10,   "1.0"),
      VNT(DDCA_MCCS_V20,   "2.0"),
      VNT(DDCA_MCCS_V21,   "2.1"),
      VNT(DDCA_MCCS_V30,   "3.0"),
      VNT(DDCA_MCCS_V22,   "2.2"),
      VNT(DDCA_MCCS_VNONE, "unknown"),
      VNT_END
};



/** Returns a #DDCA_MCCS_Version_Id in a form suitable for display,
 *  e.g. "2.0".
 *
 * @param version_id version id value
 * @return value in external form.
 */
char * format_vcp_version_id(DDCA_MCCS_Version_Id version_id) {
   char * result = NULL;
   switch (version_id) {
   case DDCA_MCCS_V10:    result = "1.0";     break;
   case DDCA_MCCS_V20:    result = "2.0";     break;
   case DDCA_MCCS_V21:    result = "2.1";     break;
   case DDCA_MCCS_V30:    result = "3.0";     break;
   case DDCA_MCCS_V22:    result = "2.2";     break;
   case DDCA_MCCS_VNONE:  result = "unknown"; break;
   case DDCA_MCCS_VANY:   result = "any";     break;
   }
   char * result2 = vnt_title(version_id_table, version_id);
   assert(streq(result, result2));
   return result;
}

#ifdef OLD
char * vcp_version_id_name0(DDCA_MCCS_Version_Id version_id) {
   bool debug = false;
   DBGMSF(debug, "Starting. version_id=%d", version_id);

   char * result = NULL;
   switch (version_id) {
   case DDCA_MCCS_V10:    result = "DDCA_V10";     break;
   case DDCA_MCCS_V20:    result = "DDCA_V20";     break;
   case DDCA_MCCS_V21:    result = "DDCA_V21";     break;
   case DDCA_MCCS_V30:    result = "DDCA_V30";     break;
   case DDCA_MCCS_V22:    result = "DDCA_V22";     break;
   case DDCA_MCCS_VNONE:  result = "DDCA_VNONE";   break;
   }

   char * result2 = vnt_name(version_id_table, version_id);
   assert(streq(result, result2));

   DBGMSF(debug, "Returning: %s", result);
   return result;
}
#endif

char * vcp_version_id_name(DDCA_MCCS_Version_Id version_id) {
   bool debug = false;
   DBGMSF(debug, "Starting. version_id=%d", version_id);

   char * result = vnt_name(version_id_table, version_id);

   DBGMSF(debug, "Returning: %s", result);
   return result;
}


/** Converts a string representation of an MCCS version, e.g. "2.2"
 *  to a version spec (integer pair).
 *
 *  @param s  string to convert
 *  @return integer pair of major and minor versions
 *  retval DDCA_UNKNOWN if invalid string
 */
DDCA_MCCS_Version_Spec parse_vspec(char * s) {
   DDCA_MCCS_Version_Spec vspec;
   int ct = sscanf(s, "%hhd . %hhd", &vspec.major, &vspec.minor);
   if (ct != 2 || vspec.major > 3 || vspec.minor > 2) {
      vspec = VCP_SPEC_UNKNOWN;
   }
   return vspec;
}


/** Converts a MCCS version spec (integer pair) to a version id (enumeration).
 *
 * @param vspec version spec
 * @return version id
 * @retval #DDCA_VUNK if vspec does not represent a valid MCCS version
 */
DDCA_MCCS_Version_Id mccs_version_spec_to_id(DDCA_MCCS_Version_Spec vspec) {
   DDCA_MCCS_Version_Id result = DDCA_MCCS_VUNK;    // initialize to avoid compiler warning

   if (vspec.major == 1 && vspec.minor == 0)
      result = DDCA_MCCS_V10;
   else if (vspec.major == 2 && vspec.minor == 0)
      result = DDCA_MCCS_V20;
   else if (vspec.major == 2 && vspec.minor == 1)
      result = DDCA_MCCS_V21;
   else if (vspec.major == 3 && vspec.minor == 0)
      result = DDCA_MCCS_V30;
   else if (vspec.major == 2 && vspec.minor == 2)
      result = DDCA_MCCS_V22;
   else if (vspec.major == 2 && vspec.minor == 1)
      result = DDCA_MCCS_V21;
   else if (vspec.major == 0 && vspec.minor == 0)
      result = DDCA_MCCS_VUNK;
   // case UNQUERIED should never arise
   else {
      PROGRAM_LOGIC_ERROR("Unexpected version spec value %d.%d", vspec.major, vspec.minor);
      assert(false);
      result = DDCA_MCCS_VUNK;   // in case assertions turned off
   }

   return result;
}


/** Converts a MCCS version id (enumerated value) to
 *  a version spec (integer pair).
 *
 *  @param id version id
 *  @return version spec
 */
DDCA_MCCS_Version_Spec mccs_version_id_to_spec(DDCA_MCCS_Version_Id id) {
   bool debug = false;
   DBGMSF(debug, "Starting.  id=%d", id);

   DDCA_MCCS_Version_Spec vspec = VCP_SPEC_ANY;
   // use table instead?
   switch(id) {
   case DDCA_MCCS_VNONE:  vspec = VCP_SPEC_UNKNOWN; break;
   case DDCA_MCCS_VANY:   vspec = VCP_SPEC_ANY;     break;
   case DDCA_MCCS_V10:    vspec = VCP_SPEC_V10;     break;
   case DDCA_MCCS_V20:    vspec = VCP_SPEC_V20;     break;
   case DDCA_MCCS_V21:    vspec = VCP_SPEC_V21;     break;
   case DDCA_MCCS_V30:    vspec = VCP_SPEC_V30;     break;
   case DDCA_MCCS_V22:    vspec = VCP_SPEC_V22;     break;
   }

   DBGMSF(debug, "Returning: %d.%d", debug, vspec.major, vspec.minor);
   return vspec;
}
