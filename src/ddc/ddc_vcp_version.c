/** @file ddc_vcp_version.c
 *
 * Functions to obtain the VCP (MCCS) version for a display.
 * These functions are in a separate source file to simplify
 * the acyclic graph of #includes within the ddc source directory.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
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


DDCA_MCCS_Version_Spec
set_vcp_version_xdf_by_display_handle(Display_Handle * dh)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dh=%s", dh_repr_t(dh));

   dh->dref->vcp_version_xdf = DDCA_VSPEC_UNKNOWN;

   if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
#ifdef USE_USB
      // DBGMSG("Trying to get VESA version...");
      __s32 vesa_ver =  usb_get_vesa_version(dh->fd);
      DBGMSF(debug, "VESA version from usb_get_vesa_version(): 0x%08x", vesa_ver);
      if (vesa_ver) {
         DBGMSF(debug, "VESA version from usb_get_vesa_version(): 0x%08x", vesa_ver);
         dh->dref->vcp_version_xdf.major = (vesa_ver >> 8) & 0xff;
         dh->dref->vcp_version_xdf.minor = vesa_ver & 0xff;
      }
      else {
         DBGMSF(debug, "Error detecting VESA version using usb_get_vesa_version()");
      }
  #else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
  #endif
     }
     else {    // normal case, not USB
        DDCA_Any_Vcp_Value * pvalrec;

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
           dh->dref->vcp_version_xdf.major = pvalrec->val.c_nc.sh;
           dh->dref->vcp_version_xdf.minor = pvalrec->val.c_nc.sl;
           DBGMSF(debug, "Set dh->dref->vcp_version_xdf to %d.%d, %s",
                         dh->dref->vcp_version_xdf.major,
                         dh->dref->vcp_version_xdf.minor,
                         format_vspec(dh->dref->vcp_version_xdf) );
           free_single_vcp_value(pvalrec);
        }
        else {
           // happens for pre MCCS v2 monitors
           DBGMSF(debug, "Error detecting VCP version using VCP feature 0xdf. psc=%s", psc_desc(psc) );
        }
     }  // not USB

     assert( !vcp_version_eq(dh->dref->vcp_version_xdf, DDCA_VSPEC_UNQUERIED) );
     DBGMSF(debug, "Returning newly set dh->dref->vcp_version_xdf = %d.%d, %s",
                   dh->dref->vcp_version_xdf.major, dh->dref->vcp_version_xdf.minor,
                   format_vspec(dh->dref->vcp_version_xdf));
     return dh->dref->vcp_version_xdf;
}


/** Gets the VCP version.
 *
 *  Because the VCP version is used repeatedly for interpreting other
 *  VCP feature values, it is cached.
 *
 *  \param  dh     display handle
 *  \return #Version_Spec struct containing version, contains 0.0 if version
 *          could not be retrieved (pre MCCS v2)
 */
DDCA_MCCS_Version_Spec get_vcp_version_by_display_handle(Display_Handle * dh) {
   assert(dh);
   bool debug = false;
   DDCA_MCCS_Version_Spec result = DDCA_VSPEC_UNKNOWN;
   // TMI
   if (debug) {
      DBGMSG("Starting. dh=%s", dh_repr(dh) );
      DBGMSG("          dh->dref->vcp_version =  %d.%d (%s)",
             dh->dref->vcp_version.major,         dh->dref->vcp_version.minor,         format_vspec(dh->dref->vcp_version) );
      DBGMSG("          dh->dref->vcp_version_xdf = %d.%d (%s) ",
             dh->dref->vcp_version_xdf.major,     dh->dref->vcp_version_xdf.minor,     format_vspec(dh->dref->vcp_version_xdf) );
      DBGMSG("          dh->dref->vcp_version_cmdline = %d.%d (%s)",
             dh->dref->vcp_version_cmdline.major, dh->dref->vcp_version_cmdline.minor, format_vspec(dh->dref->vcp_version_cmdline) );
      if (dh->dref->dfr) {
      DBGMSG("          dh->dref->dfr->vspec = %d.%d (%s) ",
             dh->dref->dfr->vspec.major,          dh->dref->dfr->vspec.minor,          format_vspec(dh->dref->dfr->vspec) );
      }
      else {
      DBGMSG("          dh->dref->dfr == NULL");
      }
   }

   if (vcp_version_is_valid(dh->dref->vcp_version_cmdline, false)) {
      result = dh->dref->vcp_version_cmdline;
      DBGMSF(debug, "Using vcp_version_cmdline = %s", format_vspec(result));
   }

   else if (dh->dref->dfr && vcp_version_is_valid(dh->dref->dfr->vspec, false)) {
       result = dh->dref->dfr->vspec;
       DBGMSF(debug, "Using dfr->vspec = %s", format_vspec(result));
   }

   else {
      DBGMSG("WOLF 33");
      if (vcp_version_eq(dh->dref->vcp_version_xdf, DDCA_VSPEC_UNQUERIED)) {
         if (debug) {
            DBGMSG("vcp_version_xdf not set == DDCA_VSPEC_UNQUERIED");
            dbgrpt_display_handle(dh, /*msg=*/ NULL, 1);
         }

         result = set_vcp_version_xdf_by_display_handle(dh);
         assert( !vcp_version_eq(dh->dref->vcp_version_xdf, DDCA_VSPEC_UNQUERIED) );
      }
      else
         result = dh->dref->vcp_version_xdf;
   }

   DBGMSF(debug, "Returning: %d.%d (%s)", result.major, result.minor, format_vspec(result));
   return result;
}


/** Gets the VCP version.
 *
 *  Because the VCP version is used repeatedly for interpreting other
 *  VCP feature values, it is cached.
 *
 *  \param   dref     display reference
 *  \return  #Version_Spec struct containing version, contains 0.0 if version
 *           could not be retrieved (pre MCCS v2)
 */
DDCA_MCCS_Version_Spec get_vcp_version_by_display_ref(Display_Ref * dref) {
   assert(dref);
   bool debug = true;
   DBGMSF(debug, "Starting. dref=%p, dref->vcp_version =  %d.%d",
                 dref, dref->vcp_version.major, dref->vcp_version.minor);
   DBGMSF(debug, "Starting. dref=%p, dref->vcp_version_xdf =  %d.%d",
                 dref, dref->vcp_version_xdf.major, dref->vcp_version_xdf.minor);
   DBGMSF(debug, "Starting. dref=%p, dref->vcp_version_cmdline =  %d.%d",
                 dref, dref->vcp_version_cmdline.major, dref->vcp_version_cmdline.minor);

   // ddc_open_display() should not fail
   assert(dref->flags & DREF_DDC_COMMUNICATION_WORKING);

   if (vcp_version_eq(dref->vcp_version, DDCA_VSPEC_UNQUERIED)) {
      Display_Handle * dh = NULL;
      // no need to check return code since aborting if error
      // should never fail, since open already succeeded - but what if locked?
      Public_Status_Code psc = ddc_open_display(dref, CALLOPT_ERR_MSG, &dh);
      assert(psc == 0);
      // dref->vcp_version = 
      get_vcp_version_by_display_handle(dh);
      ddc_close_display(dh);
   }

   assert( !vcp_version_eq(dref->vcp_version, DDCA_VSPEC_UNQUERIED) );
   DBGMSF(debug, "Returning: %d.%d", dref->vcp_version.major, dref->vcp_version.minor);
   return dref->vcp_version;
}

