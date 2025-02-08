/** @file ddc_vcp_version.c
 *
 * Functions to obtain the VCP (MCCS) version for a display.
 * These functions are in a separate source file to simplify
 * the acyclic graph of #includes within the ddc source directory.
 */

// Copyright (C) 2014-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config.h>

/** \cond */
#include <assert.h>
#include <stdbool.h>

#include "util/error_info.h"
#include "util/traced_function_stack.h"
/** \endcond */

#include "util/debug_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/displays.h"
#include "base/rtti.h"
#include "base/status_code_mgt.h"

#ifdef ENABLE_USB
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
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dh=%s", dh_repr(dh));

   dh->dref->vcp_version_xdf = DDCA_VSPEC_UNKNOWN;

   if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
#ifdef ENABLE_USB
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
#ifdef REF
        Parsed_Nontable_Vcp_Response * parsed_response_loc = NULL;
           rpt_vstring(1, "Getting value of feature 0x%02x", feature_code);;
           Error_Info * ddc_excp = ddc_get_nontable_vcp_value(dh, feature_code, &parsed_response_loc);
           ASSERT_IFF(!ddc_excp, parsed_response_loc);
           if (ddc_excp) {
              rpt_vstring(2, "ddc_get_nontable_vcp_value() for feature 0x%02x returned: %s",
                    feature_code, errinfo_summary(ddc_excp));
              free(ddc_excp);
           }
           else {
              if (!parsed_response_loc->valid_response)
                 rpt_vstring(2, "Invalid Response");
              else if (!parsed_response_loc->supported_opcode)
                 rpt_vstring(2, "Unsupported feature code");
              else {
                 rpt_vstring(2, "getvcp 0x%02x succeeded", feature_code);
                 rpt_vstring(2, "mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x",
                       parsed_response_loc->mh, parsed_response_loc->ml,
                       parsed_response_loc->sh, parsed_response_loc->sl);
              }
              free(parsed_response_loc);
           }
#endif

        Parsed_Nontable_Vcp_Response * parsed_response_loc = NULL;

        // verbose output is distracting since this function is called when
        // querying for other things
        DDCA_Output_Level olev = get_output_level();
        if (olev == DDCA_OL_VERBOSE)
           set_output_level(DDCA_OL_NORMAL);

        Error_Info * ddc_excp = ddc_get_nontable_vcp_value(dh, 0xdf, &parsed_response_loc);
        ASSERT_IFF(!ddc_excp, parsed_response_loc);

        if (olev == DDCA_OL_VERBOSE)
           set_output_level(olev);

        const char * e1 = "Error detecting VCP version using VCP feature xDF:";
        if (ddc_excp) {
           MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "%s %s", e1, errinfo_summary(ddc_excp));
           ERRINFO_FREE(ddc_excp);
        }
        else {
           if (!parsed_response_loc->valid_response)
              MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "%s Invalid response", e1);
           else if (!parsed_response_loc->supported_opcode) {
              // happens for pre MCCS v2 monitors
              MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "%s Unsupported feature code", e1);
           }
           else {
              dh->dref->vcp_version_xdf.major = parsed_response_loc->sh; // pvalrec->val.c_nc.sh;
              dh->dref->vcp_version_xdf.minor = parsed_response_loc->sl; // pvalrec->val.c_nc.sl;
              DBGMSF(debug, "Set dh->dref->vcp_version_xdf to %d.%d, %s",
                         dh->dref->vcp_version_xdf.major,
                         dh->dref->vcp_version_xdf.minor,
                         format_vspec(dh->dref->vcp_version_xdf) );
           }
           free(parsed_response_loc);
        }
     }  // not USB

     assert( !vcp_version_eq(dh->dref->vcp_version_xdf, DDCA_VSPEC_UNQUERIED) );
     DBGTRC_DONE(debug, DDCA_TRC_NONE, "dh=%s, Returning newly set dh->dref->vcp_version_xdf = %s",
                   dh_repr(dh), format_vspec(dh->dref->vcp_version_xdf));
     return dh->dref->vcp_version_xdf;
}


DDCA_MCCS_Version_Spec get_overriding_vcp_version(
      Display_Ref * dref)
{
   bool debug = false;
   // TMI
   if (debug) {
      DBGMSG("          dh->dref->vcp_version_cmdline = %s", format_vspec_verbose(dref->vcp_version_cmdline));
      if (dref->dfr)
         DBGMSG("          dh->dref->dfr->vspec = %s",          format_vspec_verbose(dref->dfr->vspec));
      else
         DBGMSG("          dh->dref->dfr == NULL");
   }

   DDCA_MCCS_Version_Spec result = DDCA_VSPEC_UNQUERIED;

   if (vcp_version_is_valid(dref->vcp_version_cmdline, false)) {
      result = dref->vcp_version_cmdline;
      DBGMSF(debug, "Using dref->vcp_version_cmdline = %s", format_vspec(result));
   }

   else if (dref->dfr && vcp_version_is_valid(dref->dfr->vspec, /* allow_unknown */ false)) {
      result = dref->dfr->vspec;
      DBGMSF(debug, "Using dref->dfr->vspec = %s", format_vspec_verbose(result));
   }

   return result;
}


DDCA_MCCS_Version_Spec get_saved_vcp_version(
      Display_Ref * dref)
{
   bool debug = false;

   DDCA_MCCS_Version_Spec result = DDCA_VSPEC_UNKNOWN;
   result = get_overriding_vcp_version(dref);
   if (vcp_version_eq(result, DDCA_VSPEC_UNQUERIED)) {
       result = dref->vcp_version_xdf;
       DBGMSF(debug, "Using dref->vcp_version_xdf = %s", format_vspec_verbose(result));
   }

    DBGMSF(debug, "dref=%s, Returning: %s", dref_repr_t(dref), format_vspec_verbose(result));
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
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dh=%s, dref=%s", dh_repr(dh), dref_repr_t(dh->dref));

   DDCA_MCCS_Version_Spec result = DDCA_VSPEC_UNKNOWN;
   result = get_saved_vcp_version(dh->dref);
   if (vcp_version_eq(result, DDCA_VSPEC_UNQUERIED)) {
      result = set_vcp_version_xdf_by_dh(dh);
      assert( !vcp_version_eq(dh->dref->vcp_version_xdf, DDCA_VSPEC_UNQUERIED) );
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning: %s", format_vspec_verbose(result));
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
 *
 *   Precedence of VCP versions:
 *   - version specified on the command line
 *   - version in a dynamic feature record for the display
 *   - version returned by feature xDF
 */
DDCA_MCCS_Version_Spec get_vcp_version_by_dref(Display_Ref * dref) {
   assert(dref);
   bool debug = false;

   DBGTRC_STARTING(debug, DDCA_TRC_NONE,"dref=%s", dref_repr_t(dref));

   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      DBGTRC_NOPREFIX(true, DDCA_TRC_NONE,"dref->vcp_version_cmdline =  %s",
                        format_vspec_verbose(dref->vcp_version_cmdline));
      if (dref->dfr)
         DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "dref->dfr->vspec = ",
                        format_vspec_verbose(dref->dfr->vspec));
      else
         DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "dref->dfr is null");

      DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "dref->vcp_version_xdf = %s",
                        format_vspec_verbose (dref->vcp_version_xdf));

      if (!(dref->flags & DREF_DDC_COMMUNICATION_WORKING)) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "flags: %s", interpret_dref_flags_t(dref->flags) );
      }
   }

   DDCA_MCCS_Version_Spec result = get_saved_vcp_version(dref);
   if (vcp_version_eq(result, DDCA_VSPEC_UNQUERIED)) {
      if (!(dref->flags & DREF_DDC_COMMUNICATION_WORKING)) {
         DBGMSG( "DREF_DDC_COMMUNICATION_WORKING not set. dref=%s", dref_repr_t(dref));
         dbgrpt_display_ref(dref,  true,  2);
         debug_current_traced_function_stack(/*reverse*/ true);
         SYSLOG2(DDCA_SYSLOG_ERROR, "DREF_DDC_COMMUNICATION_WORKING not set. dref=%s", dref_repr_t(dref));
         current_traced_function_stack_to_syslog(LOG_ERR, /* reverse */ true);
         backtrace_to_syslog(LOG_ERR, 0);

         // ASSERT_WITH_BACKTRACE(false);
         // ASSERT_WITH_BACKTRACE(dref->flags & DREF_DDC_COMMUNICATION_WORKING) ;
         result = DDCA_VSPEC_UNKNOWN;
      }
      else {
         Display_Handle * dh = NULL;
         // ddc_open_display() should not fail
         // 2/2020: but it can return -EBUSY
         // DBGMSF(debug, "Calling ddc_open_display() ...");
         Error_Info * ddc_excp = ddc_open_display(dref, CALLOPT_ERR_MSG, &dh);
         if (!ddc_excp) {
            result = set_vcp_version_xdf_by_dh(dh);
            assert( !vcp_version_eq(dh->dref->vcp_version_xdf, DDCA_VSPEC_UNQUERIED) );
            // DBGMSF(debug, "Calling ddc_close_display() ...");
            ddc_close_display_wo_return(dh);
         }
         else {
            // DBGMSF(debug, "ddc_open_display() failed");
            SYSLOG2((ddc_excp->status_code == -EBUSY) ? DDCA_SYSLOG_INFO : DDCA_SYSLOG_ERROR,
                    "Unable to open display %s: %s", dref_repr_t(dref), psc_desc(ddc_excp->status_code));
            dh->dref->vcp_version_xdf = DDCA_VSPEC_UNKNOWN;
            errinfo_free(ddc_excp);
         }
      }
   }

   assert( !vcp_version_eq(result, DDCA_VSPEC_UNQUERIED) );
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "dref=%s, Returning: %s", dref_repr_t(dref), format_vspec_verbose(result));
   return result;
}


void init_ddc_vcp_version() {
   RTTI_ADD_FUNC(set_vcp_version_xdf_by_dh);
   RTTI_ADD_FUNC(get_vcp_version_by_dref);
   RTTI_ADD_FUNC(get_vcp_version_by_dh);
}
