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
set_vcp_version_xdf_by_dh(Display_Handle * dh)
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


DDCA_MCCS_Version_Spec get_saved_vcp_version(
      Display_Ref * dref)
{
   bool debug = false;
   DDCA_MCCS_Version_Spec result = DDCA_VSPEC_UNKNOWN;
   // TMI
   if (debug) {
      DBGMSG("Starting. dref=%s", dref_repr_t(dref) );

      DBGMSG("          dref->vcp_version (old) =  %s",  format_vspec_verbose(dref->vcp_version_old));
      DBGMSG("          dref->vcp_version_cmdline = %s", format_vspec_verbose(dref->vcp_version_cmdline));
      if (dref->dfr) {
      DBGMSG("          dref->dfr->vspec = %s ",         format_vspec_verbose(dref->dfr->vspec));
      }
      else {
      DBGMSG("          dref->dfr == NULL");
      }
      DBGMSG("          dref->vcp_version_xdf = %s",     format_vspec_verbose(dref->vcp_version_xdf));

   }

   if (vcp_version_is_valid(dref->vcp_version_cmdline, false)) {
       result = dref->vcp_version_cmdline;
       DBGMSF(debug, "Using dref->vcp_version_cmdline = %s", format_vspec(result));
    }

    else if (dref->dfr && vcp_version_is_valid(dref->dfr->vspec, /* allow_unknown */ false)) {
        result = dref->dfr->vspec;
        DBGMSF(debug, "Using dref->dfr->vspec = %s", format_vspec_verbose(result));
    }

    else {
       result = dref->vcp_version_xdf;
       DBGMSF(debug, "Using dref->vcp_version_xdf = %s", format_vspec_verbose(result));
    }

    DBGMSF(debug, "Returning: %s", format_vspec_verbose(result));
    return result;
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
DDCA_MCCS_Version_Spec get_vcp_version_by_dh(Display_Handle * dh) {
   assert(dh);
   bool debug = false;
   DDCA_MCCS_Version_Spec result = DDCA_VSPEC_UNKNOWN;
   // TMI
   if (debug) {
      DBGMSG("Starting. dh=%s, dh->dref=%s", dh_repr(dh), dref_repr_t(dh->dref) );

     DBGMSG("          dh->dref->vcp_version_old =  %s",     format_vspec_verbose(dh->dref->vcp_version_old));

      DBGMSG("          dh->dref->vcp_version_cmdline = %s", format_vspec_verbose(dh->dref->vcp_version_cmdline));

      if (dh->dref->dfr) {
      DBGMSG("          dh->dref->dfr->vspec = %s",          format_vspec_verbose(dh->dref->dfr->vspec));
      }
      else {
      DBGMSG("          dh->dref->dfr == NULL");
      }

      DBGMSG("          dh->dref->vcp_version_xdf = %s ",
                 format_vspec_verbose(dh->dref->vcp_version_xdf));
   }

   result = get_saved_vcp_version(dh->dref);
   if (vcp_version_eq(result, DDCA_VSPEC_UNQUERIED)) {
      result = set_vcp_version_xdf_by_dh(dh);
      assert( !vcp_version_eq(dh->dref->vcp_version_xdf, DDCA_VSPEC_UNQUERIED) );
   }

   DBGMSF(debug, "Returning: %s", format_vspec_verbose(result));
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
DDCA_MCCS_Version_Spec get_vcp_version_by_dref(Display_Ref * dref) {
   assert(dref);
   bool debug = false;

   if (debug) {
      DBGMSG(   "Starting. dref=%s, dref->vcp_version_old (original) =  %s",
                    dref_repr_t(dref),    format_vspec_verbose(dref->vcp_version_old));

      DBGMSG(   "          dref->vcp_version_cmdline =  %s",
                                          format_vspec_verbose(dref->vcp_version_cmdline));
      if (dref->dfr)
         DBGMSG("          dref->dfr->vspec = ",  format_vspec_verbose(dref->dfr->vspec));
      else
         DBGMSG("          dref->dfr is null");

      DBGMSG(   "          dref->vcp_version_xdf = %s",
                                          format_vspec_verbose (dref->vcp_version_xdf));
      if (!(dref->flags & DREF_DDC_COMMUNICATION_WORKING)) {
            dbgrpt_dref_flags(dref->flags, 2);
      }
   }

   assert(dref->flags & DREF_DDC_COMMUNICATION_WORKING);

   DDCA_MCCS_Version_Spec result = get_saved_vcp_version(dref);
   if (vcp_version_eq(result, DDCA_VSPEC_UNQUERIED)) {
      Display_Handle * dh = NULL;
      // ddc_open_display() should not fail
      Public_Status_Code psc = ddc_open_display(dref, CALLOPT_ERR_MSG, &dh);
      assert(psc == 0);
      result = set_vcp_version_xdf_by_dh(dh);
      assert( !vcp_version_eq(dh->dref->vcp_version_xdf, DDCA_VSPEC_UNQUERIED) );
      ddc_close_display(dh);
   }

   assert( !vcp_version_eq(result, DDCA_VSPEC_UNQUERIED) );
   DBGMSF(debug, "Done.    Returning: %s", format_vspec_verbose(result));
   return result;
}

