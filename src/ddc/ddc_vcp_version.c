/* ddc_vcp_version.c
 *
 * Functions to obtain the VCP (MCCS) version for a display.
 * These functions are in a separate source file to simplify
 * the acyclic graph of #includes within the ddc source directory.
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <config.h>

/** \cond */
#include <assert.h>
#include <stdbool.h>
/** \endcond */

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/ddc_error.h"
#include "base/displays.h"
#include "base/status_code_mgt.h"

#ifdef USE_USB
#include "usb/usb_vcp.h"
#endif

#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"

#include "ddc/ddc_vcp_version.h"


//
// Functions for VCP (MCCS) version
//

/* Gets the VCP version.
 *
 * Because the VCP version is used repeatedly for interpreting other
 * VCP feature values, it is cached.
 *
 * Arguments:
 *    dh     display handle
 *
 * Returns:
 *    Version_Spec struct containing version, contains 0.0 if version
 *    could not be retrieved (pre MCCS v2)
 */
DDCA_MCCS_Version_Spec get_vcp_version_by_display_handle(Display_Handle * dh) {
   bool debug = false;
   // TMI
   DBGMSF(debug, "Starting. dh=%s, dh->dref->vcp_version =  %d.%d, %s",
                 dh_repr(dh), dh->dref->vcp_version.major, dh->dref->vcp_version.minor, format_vspec(dh->dref->vcp_version));
   if (vcp_version_is_unqueried(dh->dref->vcp_version)) {
      if (debug) {
         DBGMSG("Starting.  vcp_version not set");
         report_display_handle(dh, /*msg=*/ NULL, 1);
      }
      dh->dref->vcp_version = VCP_SPEC_UNKNOWN;

      if (dh->dref->io_mode == DDCA_IO_USB) {
#ifdef USE_USB
         // DBGMSG("Trying to get VESA version...");
         __s32 vesa_ver =  usb_get_vesa_version(dh->fh);
         DBGMSF(debug, "VESA version from usb_get_vesa_version(): 0x%08x", vesa_ver);
         if (vesa_ver) {
            DBGMSF(debug, "VESA version from usb_get_vesa_version(): 0x%08x", vesa_ver);
            dh->dref->vcp_version.major = (vesa_ver >> 8) & 0xff;
            dh->dref->vcp_version.minor = vesa_ver & 0xff;
         }
         else {
            DBGMSF(debug, "Error detecting VESA version using usb_get_vesa_version()");
         }
#else
         PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
      }
      else {    // normal case, not USB
         DDCA_Single_Vcp_Value * pvalrec;

         // verbose output is distracting since this function is called when
         // querying for other things
         DDCA_Output_Level olev = get_output_level();
         if (olev == DDCA_OL_VERBOSE)
            set_output_level(DDCA_OL_NORMAL);
         Public_Status_Code psc =  0;
         Ddc_Error * ddc_excp = get_vcp_value(dh, 0xdf, DDCA_NON_TABLE_VCP_VALUE, &pvalrec);
         psc = (ddc_excp) ? ddc_excp->psc : 0;
         DBGMSF(debug, "get_vcp_value() returned %s", psc_desc(psc));
         if (debug && psc == DDCRC_RETRIES)
            DBGMSG("    Try errors: %s", ddc_error_causes_string(ddc_excp));
         if (olev == DDCA_OL_VERBOSE)
            set_output_level(olev);

         if (psc == 0) {
            dh->dref->vcp_version.major = pvalrec->val.nc.sh;
            dh->dref->vcp_version.minor = pvalrec->val.nc.sl;
            DBGMSF(debug, "Set dh->dref->vcp_version to %d.%d, %s",
                          dh->dref->vcp_version.major,
                          dh->dref->vcp_version.minor,
                          format_vspec(dh->dref->vcp_version) );
         }
         else {
            // happens for pre MCCS v2 monitors
            DBGMSF(debug, "Error detecting VCP version using VCP feature 0xdf. psc=%s", psc_desc(psc) );
         }
      }
      DBGMSF(debug, "Non-cache lookup returning: %d.%d", dh->dref->vcp_version.major, dh->dref->vcp_version.minor);
   }
   // DBGMSF(debug, "Returning: %d.%d", dh->dref->vcp_version.major, dh->dref->vcp_version.minor);
   assert( !vcp_version_eq(dh->dref->vcp_version, VCP_SPEC_UNQUERIED) );
   // if (debug) {
   //    DBGMSG("Done.");
   //    report_display_handle(dh, /*msg=*/ NULL, 1);
   // }
   DBGMSF(debug, "Returning dh->dref->vcp_version = %d.%d, %s",
                 dh->dref->vcp_version.major, dh->dref->vcp_version.minor,
                 format_vspec(dh->dref->vcp_version));
   return dh->dref->vcp_version;
}


/* Gets the VCP version.
 *
 * Because the VCP version is used repeatedly for interpreting other
 * VCP feature values, it is cached.
 *
 * Arguments:
 *    dref     display reference
 *
 * Returns:
 *    Version_Spec struct containing version, contains 0.0 if version
 *    could not be retrieved (pre MCCS v2)
 */
DDCA_MCCS_Version_Spec get_vcp_version_by_display_ref(Display_Ref * dref) {
   bool debug = false;
   DBGMSF(debug, "Starting. dref=%p, dref->vcp_version =  %d.%d",
                 dref, dref->vcp_version.major, dref->vcp_version.minor);

   if (vcp_version_is_unqueried(dref->vcp_version)) {
      Display_Handle * dh = NULL;
      // no need to check return code since aborting if error
      ddc_open_display(dref, CALLOPT_ERR_MSG | CALLOPT_ERR_ABORT, &dh);
      dref->vcp_version = get_vcp_version_by_display_handle(dh);
      ddc_close_display(dh);
   }

   assert( !vcp_version_eq(dref->vcp_version, VCP_SPEC_UNQUERIED) );
   DBGMSF(debug, "Returning: %d.%d", dref->vcp_version.major, dref->vcp_version.minor);
   return dref->vcp_version;
}

