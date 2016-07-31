/* ddc_vcp_version.c
 *
 * Created on: Dec 31, 2015
 *
 * Functions to obtain the VCP (MCCS) version for a display.
 * These functions are in a separate source file to simplify
 * the acyclic graph of #includes within the ddc source directory.
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <stdbool.h>

#include "base/core.h"
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
Version_Spec get_vcp_version_by_display_handle(Display_Handle * dh) {
   bool debug = false;
   // TMI
   // DBGMSF(debug, "Starting. dh=%p, dh->vcp_version =  %d.%d",
   //               dh, dh->vcp_version.major, dh->vcp_version.minor);
   if (is_version_unqueried(dh->vcp_version)) {
      dh->vcp_version.major = 0;
      dh->vcp_version.minor = 0;
#ifdef OLD
      Parsed_Nontable_Vcp_Response * pinterpreted_code;
#endif
      Single_Vcp_Value * pvalrec;

      // verbose output is distracting since this function is called when
      // querying for other things
      Output_Level olev = get_output_level();
      if (olev == OL_VERBOSE)
         set_output_level(OL_NORMAL);
#ifdef OLD
      Global_Status_Code  gsc = get_nontable_vcp_value(dh, 0xdf, &pinterpreted_code);
#endif
      Global_Status_Code gsc = get_vcp_value(dh, 0xdf, NON_TABLE_VCP_VALUE, &pvalrec);
      if (olev == OL_VERBOSE)
         set_output_level(olev);

      if (gsc == 0) {
#ifdef OLD
         dh->vcp_version.major = pinterpreted_code->sh;
         dh->vcp_version.minor = pinterpreted_code->sl;
#endif
         dh->vcp_version.major = pvalrec->val.nc.sh;
         dh->vcp_version.minor = pvalrec->val.nc.sl;
      }
      else {
         // happens for pre MCCS v2 monitors
         DBGMSF(debug, "Error detecting VCP version using VCP feature 0xdf. gsc=%s\n", gsc_desc(gsc) );

#ifdef USE_USB
         if (dh->io_mode == USB_IO) {
            // DBGMSG("Trying to get VESA version...");
            __s32 vesa_ver =  usb_get_vesa_version(dh->fh);
            DBGMSF(debug, "VESA version from usb_get_vesa_version(): 0x%08x", vesa_ver);
            if (vesa_ver) {
               DBGMSF(debug, "VESA version from usb_get_vesa_version(): 0x%08x", vesa_ver);
               dh->vcp_version.major = (vesa_ver >> 8) & 0xff;
               dh->vcp_version.minor = vesa_ver & 0xff;
            }
         }
#else
         PROGRAM_LOGIC_ERROR("ddctool not build with USB support");
#endif
      }
      DBGMSF(debug, "Non-cache lookup returning: %d.%d", dh->vcp_version.major, dh->vcp_version.minor);
   }
   // DBGMSF(debug, "Returning: %d.%d", dh->vcp_version.major, dh->vcp_version.minor);
   return dh->vcp_version;
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
Version_Spec get_vcp_version_by_display_ref(Display_Ref * dref) {
   // printf("(%s) Starting. dref=%p, dref->vcp_version =  %d.%d\n",
   //        __func__, dref, dref->vcp_version.major, dref->vcp_version.minor);

   if (is_version_unqueried(dref->vcp_version)) {
      Display_Handle * dh = ddc_open_display(dref, CALLOPT_ERR_MSG | CALLOPT_ERR_ABORT);
      dref->vcp_version = get_vcp_version_by_display_handle(dh);
      ddc_close_display(dh);
   }

   // DBGMSG("Returning: %d.%d", dref->vcp_version.major, vspec.minor);
   return dref->vcp_version;
}

