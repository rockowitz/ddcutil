/** @file ddc_vcp_version.c
 *
 * Functions to obtain the VCP (MCCS) version for a display.
 * These functions are in a separate source file to simplify
 * the acyclic graph of #includes within the ddc source directory.
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config.h>

/** \cond */
#include <assert.h>
#include <stdbool.h>

#include "util/error_info.h"
/** \endcond */

#include "base/core.h"
#include "base/ddc_errno.h"
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
   assert(dh);
   bool debug = false;
   // TMI
   DBGMSF(debug, "Starting. dh=%s, dh->dref->vcp_version =  %d.%d, %s",
                 dh_repr(dh), dh->dref->vcp_version.major, dh->dref->vcp_version.minor, format_vspec(dh->dref->vcp_version));
   // if (vcp_version_is_unqueried(dh->dref->vcp_version)) {
   if (vcp_version_eq(dh->dref->vcp_version, DDCA_VSPEC_UNQUERIED)) {
      if (debug) {
         DBGMSG0("Starting.  vcp_version not set");
         dbgrpt_display_handle(dh, /*msg=*/ NULL, 1);
      }
      dh->dref->vcp_version = DDCA_VSPEC_UNKNOWN;

      if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
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
            DBGMSF0(debug, "Error detecting VESA version using usb_get_vesa_version()");
         }
#else
         PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
      }
      else {    // normal case, not USB
         Single_Vcp_Value * pvalrec;

         // verbose output is distracting since this function is called when
         // querying for other things
         DDCA_Output_Level olev = get_output_level();
         if (olev == DDCA_OL_VERBOSE)
            set_output_level(DDCA_OL_NORMAL);
         Public_Status_Code psc =  0;
         Error_Info * ddc_excp = ddc_get_vcp_value(dh, 0xdf, DDCA_NON_TABLE_VCP_VALUE, &pvalrec);
         psc = ERRINFO_STATUS(ddc_excp);
         DBGMSF(debug, "get_vcp_value() returned %s", psc_desc(psc));
         if (debug && psc == DDCRC_RETRIES)
            DBGMSG("    Try errors: %s", errinfo_causes_string(ddc_excp));
         if (olev == DDCA_OL_VERBOSE)
            set_output_level(olev);

         if (psc == 0) {
            dh->dref->vcp_version.major = pvalrec->val.nc.sh;
            dh->dref->vcp_version.minor = pvalrec->val.nc.sl;
            DBGMSF(debug, "Set dh->dref->vcp_version to %d.%d, %s",
                          dh->dref->vcp_version.major,
                          dh->dref->vcp_version.minor,
                          format_vspec(dh->dref->vcp_version) );
            free_single_vcp_value(pvalrec);
         }
         else {
            // happens for pre MCCS v2 monitors
            DBGMSF(debug, "Error detecting VCP version using VCP feature 0xdf. psc=%s", psc_desc(psc) );
         }
      }
      DBGMSF(debug, "Non-cache lookup returning: %d.%d", dh->dref->vcp_version.major, dh->dref->vcp_version.minor);
   }

   assert( !vcp_version_eq(dh->dref->vcp_version, DDCA_VSPEC_UNQUERIED) );
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
   assert(dref);
   bool debug = false;
   DBGMSF(debug, "Starting. dref=%p, dref->vcp_version =  %d.%d",
                 dref, dref->vcp_version.major, dref->vcp_version.minor);

   // ddc_open_display() should not fail
   assert(dref->flags & DREF_DDC_COMMUNICATION_WORKING);

   // if (vcp_version_is_unqueried(dref->vcp_version)) {
   if (vcp_version_eq(dref->vcp_version, DDCA_VSPEC_UNQUERIED)) {
      Display_Handle * dh = NULL;
      // no need to check return code since aborting if error
      // should never fail, since open already succeeded - but what if locked?
      Public_Status_Code psc = ddc_open_display(dref, CALLOPT_ERR_MSG, &dh);
      assert(psc == 0);
      dref->vcp_version = get_vcp_version_by_display_handle(dh);
      ddc_close_display(dh);
   }

   assert( !vcp_version_eq(dref->vcp_version, DDCA_VSPEC_UNQUERIED) );
   DBGMSF(debug, "Returning: %d.%d", dref->vcp_version.major, dref->vcp_version.minor);
   return dref->vcp_version;
}

