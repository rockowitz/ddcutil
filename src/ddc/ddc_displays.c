/** \file ddc_displays.c
 * Access displays, whether DDC or USB
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#include <config.h>

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <string.h>
#include <time.h>

#include "i2c/i2c_strategy_dispatcher.h"
#include "util/debug_util.h"
#include "util/edid.h"
#include "util/error_info.h"
#include "util/failsim.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"
#ifdef ENABLE_UDEV
#include "util/udev_usb_util.h"
#include "util/udev_util.h"
#endif
/** \endcond */

#include "base/core.h"
#include "base/ddc_packets.h"
#include "base/feature_metadata.h"
#include "base/linux_errno.h"
#include "base/monitor_model_key.h"
#include "base/parms.h"
#include "base/rtti.h"

#include "vcp/vcp_feature_codes.h"

#include "i2c/i2c_bus_core.h"

#ifdef USE_USB
#include "usb/usb_displays.h"
#endif

#include "dynvcp/dyn_feature_files.h"

#include "public/ddcutil_types.h"

#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_vcp_version.h"

#include "ddc/ddc_displays.h"


// Default trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDCIO;

bool check_phantom_displays = true;


static GPtrArray * all_displays = NULL;    // all detected displays
static int dispno_max = 0;                 // highest assigned display number
static int async_threshold = DISPLAY_CHECK_ASYNC_THRESHOLD_DEFAULT;
#ifdef USE_USB
static bool detect_usb_displays = true;
#else
static bool detect_usb_displays = false;
#endif


void ddc_set_async_threshold(int threshold) {
   // DBGMSG("threshold = %d", threshold);
   async_threshold = threshold;
}


static inline bool
value_bytes_zero_for_any_value(DDCA_Any_Vcp_Value * pvalrec) {
   bool result = pvalrec && pvalrec->value_type ==  DDCA_NON_TABLE_VCP_VALUE &&
                 pvalrec->val.c_nc.mh == 0 &&
                 pvalrec->val.c_nc.ml == 0 &&
                 pvalrec->val.c_nc.sh == 0 &&
                 pvalrec->val.c_nc.sl == 0;
   return result;
}


/** Collects initial monitor checks to perform them on a single open of the
 *  monitor device, and to avoid repeating them.
 *
 *  Performs the following tests:
 *  - Checks that DDC communication is working.
 *  - Checks if the monitor uses DDC Null Response to indicate invalid VCP code
 *  - Checks if the monitor uses mh=ml=sh=sl=0 to indicate invalid VCP code
 *
 *  \param dh  pointer to #Display_Handle for open monitor device
 *  \return **true** if DDC communication with the display succeeded, **false** otherwise.
 *
 *  \remark
 *  Sets bits in dh->dref->flags
 *   *  \remark
 *  It has been observed that DDC communication can fail even if slave address x37
 *  is valid on the I2C bus.
 *  \remark
 *  ADL does not notice that a reported display, e.g. Dell 1905FP, does not support
 *  DDC.
 *  \remark
 *  Monitors are supposed to set the unsupported feFFature bit in a valid DDC
 *  response, but a few monitors (mis)use the Null Response instead to indicate
 *  an unsupported feature. Others return with the unsupported feature bit not
 *  set, but all bytes (mh, ml, sh, sl) zero.
 *  \remark
 *  Note that the test here is not perfect, as a Null Response might
 *  in fact indicate a transient error, but that is rare.
 *  \remark
 *  Output level should have been set <= DDCA_OL_NORMAL prior to this call since
 *  verbose output is distracting.
 */
static bool
ddc_initial_checks_by_dh(Display_Handle * dh) {
   bool debug = false;
   TRACED_ASSERT(dh && dh->dref);
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s", dh_repr(dh));
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "communication flags: %s", dref_basic_flags_t(dh->dref->flags));

   DDCA_Any_Vcp_Value * pvalrec;

   if (!(dh->dref->flags & DREF_DDC_COMMUNICATION_CHECKED)) {
      Public_Status_Code psc = 0;
      Error_Info * ddc_excp = ddc_get_vcp_value(dh, 0x00, DDCA_NON_TABLE_VCP_VALUE, &pvalrec);
      psc = (ddc_excp) ? ddc_excp->status_code : 0;
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "ddc_get_vcp_value() for feature 0x00 returned: %s, pvalrec=%p",
                             //    psc_desc(psc),
                                 errinfo_summary(ddc_excp),
                                 pvalrec);
      if (psc == DDCRC_RETRIES && debug)
         DBGMSG("    Try errors: %s", errinfo_causes_string(ddc_excp));
      if (ddc_excp)
         errinfo_free(ddc_excp);

      DDCA_IO_Mode io_mode = dh->dref->io_path.io_mode;
      if (io_mode == DDCA_IO_USB) {
         if (psc == 0 || psc == DDCRC_DETERMINED_UNSUPPORTED) {
            dh->dref->flags |= DREF_DDC_COMMUNICATION_WORKING;

         }
         TRACED_ASSERT( (psc == 0 && pvalrec) || (psc != 0 && !pvalrec) );

      }
      else {
         TRACED_ASSERT(psc != DDCRC_DETERMINED_UNSUPPORTED);  // only set at higher levels, unless USB

         // What if returns -EIO?  Dell AW3418D returns -EIO for unsupported features
         // EXCEPT that it returns mh=ml=sh=sl=0 for feature 0x00  (2/2019)

#ifdef FORCE_SUCCESS
         // for testing:
         if (psc == DDCRC_RETRIES) {
            DBGTRC(debug, TRACE_GROUP, "Forcing DDC communication success");
            psc = 0;
            dh->dref->flags |= DREF_DDC_COMMUNICATION_WORKING;
            dh->dref->flags |= DREF_DDC_DOES_NOT_INDICATE_UNSUPPORTED;
            dh->dref->flags |= DREF_DDC_COMMUNICATION_CHECKED;
            dh->dref->flags |= DREF_DDC_NULL_RESPONSE_CHECKED;    // redundant with refactoring
            goto bye;
         }
#endif

         if ( psc == DDCRC_NULL_RESPONSE        ||
              psc == DDCRC_ALL_RESPONSES_NULL   ||
              psc == 0                          ||
              psc == DDCRC_REPORTED_UNSUPPORTED )
         {
            dh->dref->flags |= DREF_DDC_COMMUNICATION_WORKING;

            if (psc == DDCRC_REPORTED_UNSUPPORTED)
               dh->dref->flags |= DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED;

            else if (psc == DDCRC_NULL_RESPONSE || psc == DDCRC_ALL_RESPONSES_NULL)
               dh->dref->flags |= DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED;

            else {
               TRACED_ASSERT( psc == 0);
               TRACED_ASSERT(pvalrec);
            }
            TRACED_ASSERT( (psc == 0 && pvalrec) || (psc != 0 && !pvalrec) );
         }

         if (psc == 0) {
            TRACED_ASSERT(pvalrec && pvalrec->value_type == DDCA_NON_TABLE_VCP_VALUE );
            if (debug || IS_TRACING()) {
               DBGMSG("pvalrec:");
               dbgrpt_single_vcp_value(pvalrec, 1);
            }

            DBGTRC_NOPREFIX(debug, TRACE_GROUP, "value_type=%d, mh=%d, ml=%d, sh=%d, sl=%d",
                     pvalrec->value_type,
                     pvalrec->val.c_nc.mh,
                     pvalrec->val.c_nc.ml,
                     pvalrec->val.c_nc.sh,
                     pvalrec->val.c_nc.sl);
            if (value_bytes_zero_for_any_value(pvalrec))
            {
               DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Setting DREF_DDC_USES_MH_ML_SH_SL_ZERO_FOR_UNSUPPORTED");
               dh->dref->flags |= DREF_DDC_USES_MH_ML_SH_SL_ZERO_FOR_UNSUPPORTED;
            }
            else {
               DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Setting DREF_DDC_DOES_NOT_INDICATE_UNSUPPORTED");
               dh->dref->flags |= DREF_DDC_DOES_NOT_INDICATE_UNSUPPORTED;
            }
         }
      }
      dh->dref->flags |= DREF_DDC_COMMUNICATION_CHECKED;
      dh->dref->flags |= DREF_DDC_NULL_RESPONSE_CHECKED;    // redundant with refactoring
   }

#ifdef FORCE_SUCCESS
bye:
   ;   // o.w. error that a label can only be part of a statement
#endif
   bool communication_working = dh->dref->flags & DREF_DDC_COMMUNICATION_WORKING;

   // Would prefer to defer checking version until actually needed to avoid
   // additional DDC io during monitor detection
   // Unfortunately, introduces ddc_open_display(), with possible error states,
   // into other functions, e.g. ddca_get_feature_list_by_dref()
   if (communication_working) {
      if ( vcp_version_eq(dh->dref->vcp_version_xdf, DDCA_VSPEC_UNQUERIED)) {
         set_vcp_version_xdf_by_dh(dh);
      }
   }
   if (!communication_working && i2c_force_bus) {
      dh->dref->flags |= DREF_DDC_COMMUNICATION_WORKING;
      communication_working = true;
      DBGTRC_NOPREFIX(debug || true , TRACE_GROUP, "dh=%s, Forcing DDC communication success.",
            dh_repr_t(dh) );
      dh->dref->flags |= DREF_DDC_COMMUNICATION_WORKING;
      dh->dref->flags |= DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED;   // good_enuf_for_test
      dh->dref->flags |= DREF_DDC_COMMUNICATION_CHECKED;
      dh->dref->vcp_version_xdf = DDCA_VSPEC_V22;   // good enuf for test
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, communication_working, "dh=%s", dh_repr(dh));
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "communication flags: %s", dref_basic_flags_t(dh->dref->flags));
   return communication_working;
}


/** Given a #Display_Ref, opens the monitor device and calls #initial_checks_by_dh()
 *  to perform initial monitor checks.
 *
 *  \param dref pointer to #Display_Ref for monitor
 *  \return **true** if DDC communication with the display succeeded, **false** otherwise.
 */
bool ddc_initial_checks_by_dref(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%s, communication flags: %s",
                                       dref_repr_t(dref), dref_basic_flags_t(dref->flags));
   bool result = false;
   Display_Handle * dh = NULL;
   Public_Status_Code psc = 0;

   psc = ddc_open_display(dref, CALLOPT_ERR_MSG, &dh);
   if (psc == 0)  {
      result = ddc_initial_checks_by_dh(dh);
      ddc_close_display(dh);
   }
   else {
     dref->flags |= DREF_DDC_COMMUNICATION_CHECKED;
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %s. dref = %s", sbool(result), dref_repr_t(dref) );
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "communication flags: %s", dref_basic_flags_t(dref->flags));
   return result;
}


/** Performs initial checks in a thread
 *
 *  \param data display reference
 */
void * threaded_initial_checks_by_dref(gpointer data) {
   bool debug = false;

   Display_Ref * dref = data;
   TRACED_ASSERT(memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref = %s", dref_repr_t(dref) );

   ddc_initial_checks_by_dref(dref);
   // g_thread_exit(NULL);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning NULL. dref = %s,", dref_repr_t(dref) );
   return NULL;
}


//
// Functions to get display information
//

/** Gets a list of all detected displays, whether they support DDC or not.
 *
 *  Detection must already have occurred.
 *
 *  \return **GPtrArray of #Display_Ref instances
 */
GPtrArray * ddc_get_all_displays() {
   // ddc_ensure_displays_detected();
   TRACED_ASSERT(all_displays);

   return all_displays;
}

GPtrArray * ddc_get_filtered_displays(bool include_invalid_displays) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "include_invalid_displays=%s", sbool(include_invalid_displays));
   TRACED_ASSERT(all_displays);
   GPtrArray * result = g_ptr_array_sized_new(all_displays->len);
   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * cur = g_ptr_array_index(all_displays, ndx);
      if (include_invalid_displays || cur->dispno > 0) {
         g_ptr_array_add(result, cur);
      }
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning array of size %d", result->len);
   if (debug || IS_TRACING()) {
      ddc_dbgrpt_display_refs(result, 2);
   }
   return result;
}


/** Gets the controller firmware version as a string
 *
 * \param dh  pointer to display handle
 * \return    pointer to character string, which is valid until the next
 *            call to this function.
 *
 * \remark
 * Consider caching the value in dh->dref
 */
static char * get_firmware_version_string_t(Display_Handle * dh) {
   bool debug = false;

   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s", dh_repr_t(dh));
   static GPrivate  firmware_version_key = G_PRIVATE_INIT(g_free);
   char * version = get_thread_fixed_buffer(&firmware_version_key, 40);

   DDCA_Any_Vcp_Value * valrec = NULL;
   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = ddc_get_vcp_value(
                               dh,
                               0xc9,                     // firmware detection
                               DDCA_NON_TABLE_VCP_VALUE,
                               &valrec);
   psc = (ddc_excp) ? ddc_excp->status_code : 0;
   if (psc != 0) {
      strcpy(version, "Unspecified");
      if (psc != DDCRC_REPORTED_UNSUPPORTED && psc != DDCRC_DETERMINED_UNSUPPORTED) {
         DBGMSF(debug, "get_vcp_value(0xc9) returned %s", psc_desc(psc));
         strcpy(version, "DDC communication failed");
         if (debug || IS_TRACING() || is_report_ddc_errors_enabled())
            errinfo_report(ddc_excp, 1);
      }
      errinfo_free(ddc_excp);
   }
   else {
      g_snprintf(version, 40, "%d.%d", valrec->val.c_nc.sh, valrec->val.c_nc.sl);
      free_single_vcp_value(valrec);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %s", version);
   return version;
}


/** Gets the controller manufacturer name for an open display.
 *
 * \param dh  pointer to display handle
 * \return pointer to character string, which is valid until the next
 * call to this function.
 *
 * \remark
 * Consider caching the value in dh->dref
 */
static char * get_controller_mfg_string_t(Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh = %s", dh_repr(dh));

   const int MFG_NAME_BUF_SIZE = 100;

   static GPrivate  buf_key = G_PRIVATE_INIT(g_free);
   char * mfg_name_buf = get_thread_fixed_buffer(&buf_key, MFG_NAME_BUF_SIZE);

   char * mfg_name = NULL;
   DDCA_Any_Vcp_Value * valrec;

   DDCA_Status ddcrc = 0;
   Error_Info * ddc_excp = ddc_get_vcp_value(dh, 0xc8, DDCA_NON_TABLE_VCP_VALUE, &valrec);
   ddcrc = (ddc_excp) ? ddc_excp->status_code : 0;

   if (ddcrc == 0) {
      DDCA_Feature_Value_Entry * vals = pxc8_display_controller_type_values;
      mfg_name =  sl_value_table_lookup(
                            vals,
                            valrec->val.c_nc.sl);
      if (!mfg_name) {
         g_snprintf(mfg_name_buf, MFG_NAME_BUF_SIZE,
                       "Unrecognized manufacturer code 0x%02x",
                       valrec->val.c_nc.sl);

         mfg_name = mfg_name_buf;
      }
      free_single_vcp_value(valrec);
   }
   else if (ddcrc == DDCRC_REPORTED_UNSUPPORTED || ddcrc == DDCRC_DETERMINED_UNSUPPORTED) {
      mfg_name = "Unspecified";
      errinfo_free(ddc_excp);
   }
   else {
      // if (debug) {
      //    DBGMSG("get_nontable_vcp_value(0xc8) returned %s", psc_desc(ddcrc));
      //    DBGMSG("    Try errors: %s", errinfo_causes_string(ddc_excp));
      // }
      ERRINFO_FREE_WITH_REPORT(ddc_excp, debug || IS_TRACING() || is_report_ddc_errors_enabled() );
      mfg_name = "DDC communication failed";
    }

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %s", mfg_name);
   return mfg_name;
}


/** Shows information about a display, specified by a #Display_Ref
 *
 *  This function is used by the DISPLAY command.
 *
 *  Output is written using report functions
 *
 * \param dref   pointer to display reference
 * \param depth  logical indentation depth
 *
 * \remark
 * The detail level shown is controlled by the output level setting
 * for the current thread.
 */
void
ddc_report_display_by_dref(Display_Ref * dref, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%s, communication flags: %s",
                 dref_repr_t(dref), dref_basic_flags_t(dref->flags));
   TRACED_ASSERT(dref && memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
   int d1 = depth+1;

   switch(dref->dispno) {
   case DISPNO_REMOVED:  // -3
      rpt_vstring(depth, "Removed display");
      break;
   case DISPNO_PHANTOM:    // -2
      rpt_vstring(depth, "Phantom display");
      break;
   case DISPNO_INVALID:   // -3
      rpt_vstring(depth, "Invalid display");
      break;
   case 0:          // valid display, no assigned display number
      d1 = depth;   // adjust indent  ??
      break;
   default:         // normal case
      rpt_vstring(depth, "Display %d", dref->dispno);
   }

   switch(dref->io_path.io_mode) {
   case DDCA_IO_I2C:
      {
         I2C_Bus_Info * curinfo = dref->detail;
         TRACED_ASSERT(curinfo && memcmp(curinfo, I2C_BUS_INFO_MARKER, 4) == 0);

         i2c_report_active_display(curinfo, d1);
      }
      break;
   case DDCA_IO_ADL:
      PROGRAM_LOGIC_ERROR("ADL implementation removed");
      break;
   case DDCA_IO_USB:
#ifdef USE_USB
      usb_show_active_display_by_dref(dref, d1);
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
      break;
   }

   TRACED_ASSERT(dref->flags & DREF_DDC_COMMUNICATION_CHECKED);

   DDCA_Output_Level output_level = get_output_level();

   if (output_level >= DDCA_OL_NORMAL) {
      if (!(dref->flags & DREF_DDC_COMMUNICATION_WORKING) ) {
         rpt_vstring(d1, "DDC communication failed");
         char msgbuf[100] = {0};
         char * msg = NULL;
         if (dref->dispno == DISPNO_PHANTOM) {
            if (dref->actual_display) {
               snprintf(msgbuf, 100, "Use non-phantom device %s",
                        dref_short_name_t(dref->actual_display));
               msg = msgbuf;
            }
            else {
               // should never occur
               msg = "Use non-phantom device";
            }
         }
         else {
            if (dref->io_path.io_mode == DDCA_IO_I2C)
            {
                I2C_Bus_Info * curinfo = dref->detail;
                if (curinfo->flags & I2C_BUS_EDP)
                    msg = "This is an eDP laptop display. Laptop displays do not support DDC/CI.";
                else if (curinfo->flags & I2C_BUS_LVDS)
                     msg = "This is a LVDS laptop display. Laptop displays do not support DDC/CI.";
                else if ( is_embedded_parsed_edid(dref->pedid) )
                    msg = "This appears to be a laptop display. Laptop displays do not support DDC/CI.";
            }
            if (output_level >= DDCA_OL_VERBOSE) {
               if (!msg) {
                  msg = "Is DDC/CI enabled in the monitor's on-screen display?";
               }
            }
         }
         if (msg) {
            rpt_vstring(d1, msg);
         }
      }
      else {
         DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_dref(dref);
         // DBGMSG("vspec = %d.%d", vspec.major, vspec.minor);
         if ( vspec.major   == 0)
            rpt_vstring(d1, "VCP version:         Detection failed");
         else
            rpt_vstring(d1, "VCP version:         %d.%d", vspec.major, vspec.minor);

         if (output_level >= DDCA_OL_VERBOSE) {
            // n. requires write access since may call get_vcp_value(), which does a write
            Display_Handle * dh = NULL;
            Public_Status_Code psc = ddc_open_display(dref, CALLOPT_ERR_MSG, &dh);
            if (psc != 0) {
               rpt_vstring(d1, "Error opening display %s, error = %s",
                                  dref_short_name_t(dref), psc_desc(psc));
            }
            else {
               // display controller mfg, firmware version
               rpt_vstring(d1, "Controller mfg:      %s", get_controller_mfg_string_t(dh) );
               rpt_vstring(d1, "Firmware version:    %s", get_firmware_version_string_t(dh));;
               ddc_close_display(dh);
            }

            if (dref->io_path.io_mode != DDCA_IO_USB) {
               rpt_vstring(d1, "Monitor returns DDC Null Response for unsupported features: %s",
                                  sbool(dref->flags & DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED));
               // rpt_vstring(d1, "Monitor returns success with mh=ml=sh=sl=0 for unsupported features: %s",
               //                    sbool(dref->flags & DREF_DDC_USES_MH_ML_SH_SL_ZERO_FOR_UNSUPPORTED));
            }
         }
      }
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


/** Returns the number of detected displays.
 *
 *  \param  include_invalid_displays
 *  \return number of displays, 0 if display detection has not yet occurred.
 */
int
ddc_get_display_count(bool include_invalid_displays) {
   int display_ct = -1;
   if (all_displays) {
      display_ct = 0;
      for (int ndx=0; ndx<all_displays->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
         TRACED_ASSERT(memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
         if (dref->dispno > 0 || include_invalid_displays) {
            display_ct++;
         }
      }
   }
   return display_ct;
}


/** Reports all displays found.
 *
 * Output is written to the current report destination using report functions.
 *
 * @param   include_invalid_displays  if false, report only valid displays\n
 *                                    if true,  report all displays
 * @param   depth       logical indentation depth
 *
 * @return total number of displays reported
 */
int
ddc_report_displays(bool include_invalid_displays, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");

   ddc_ensure_displays_detected();

   int display_ct = 0;
   for (int ndx=0; ndx<all_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
      TRACED_ASSERT(memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
      if (dref->dispno > 0 || include_invalid_displays) {
         display_ct++;
         ddc_report_display_by_dref(dref, depth);
         rpt_title("",0);
      }
   }
   if (display_ct == 0) {
      rpt_vstring(depth, "No %sdisplays found.", (!include_invalid_displays) ? "active " : "");
      if ( get_output_level() >= DDCA_OL_NORMAL ) {
         rpt_label(depth, "Is DDC/CI enabled in the monitor's on screen display?");
         rpt_label(depth, "Run \"ddcutil environment\" to check for system configuration problems.");
      }
   }

   DBGMSF(debug, "Done.     Returning: %d", display_ct);
   return display_ct;
}


/** Debugging function to display the contents of a #Display_Ref.
 *
 * \param dref  pointer to #Display_Ref
 * \param depth logical indentation depth
 */
void ddc_dbgrpt_display_ref(Display_Ref * dref, int depth) {
   int d1 = depth+1;
   int d2 = depth+2;
   // no longer needed for i2c_dbgreport_bus_info()
   // DDCA_Output_Level saved_output_level = get_output_level();
   // set_output_level(DDCA_OL_VERBOSE);
   rpt_structure_loc("Display_Ref", dref, depth);
   rpt_int("dispno", NULL, dref->dispno, d1);

   // rpt_vstring(d1, "dref: %p:", dref->dref);
   dbgrpt_display_ref(dref, d1);

   rpt_vstring(d1, "edid: %p (Skipping report)", dref->pedid);
   // report_parsed_edid(drec->edid, false, d1);

   rpt_vstring(d1, "io_mode: %s", io_mode_name(dref->io_path.io_mode));
   // rpt_vstring(d1, "flags:   0x%02x", drec->flags);
   switch(dref->io_path.io_mode) {
   case(DDCA_IO_I2C):
         rpt_vstring(d1, "I2C bus information: ");
         I2C_Bus_Info * businfo = dref->detail;
         TRACED_ASSERT( memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0);
         i2c_dbgrpt_bus_info(businfo, d2);
         break;
   case(DDCA_IO_ADL):
         PROGRAM_LOGIC_ERROR("ADL implementation removed");
         break;
   case(DDCA_IO_USB):
#ifdef USE_USB
         rpt_vstring(d1, "USB device information: ");
         Usb_Monitor_Info * moninfo = dref->detail;
         TRACED_ASSERT(memcmp(moninfo->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
         dbgrpt_usb_monitor_info(moninfo, d2);
#else
         PROGRAM_LOGIC_ERROR("Built without USB support");
#endif
   break;
   }

   // set_output_level(saved_output_level);
}


// TODO: consolidate

/** Debugging function to report a collection of #Display_Ref.
 *
 * \param recs    pointer to collection of #Display_Ref
 * \param depth   logical indentation depth
 */
void ddc_dbgrpt_display_refs(GPtrArray * recs, int depth) {
   TRACED_ASSERT(recs);
   rpt_vstring(depth, "Reporting %d Display_Ref instances", recs->len);
   for (int ndx = 0; ndx < recs->len; ndx++) {
      Display_Ref * drec = g_ptr_array_index(recs, ndx);
      TRACED_ASSERT( memcmp(drec->marker, DISPLAY_REF_MARKER, 4) == 0);
      rpt_nl();
      ddc_dbgrpt_display_ref(drec, depth+1);
   }
}


/** Emits a debug report a GPtrArray of display references
 *
 *  \param msg       initial message line
 *  \param ptrarray  array of pointers to #Display_Ref
 *  \param depth     logical indentation depth
 */
void dbgrpt_dref_ptr_array(char * msg, GPtrArray * ptrarray, int depth) {
   int d1 = depth + 1;
   rpt_vstring(depth, "%s", msg);
   if (ptrarray->len == 0)
      rpt_vstring(d1, "None");
   else {
      for (int ndx = 0; ndx < ptrarray->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(ptrarray, ndx);
         dbgrpt_display_ref(dref, d1);
      }
   }
}



//
// Monitor selection
//

/** Display selection criteria */
typedef struct {
   int     dispno;
   int     i2c_busno;
   int     iAdapterIndex;
   int     iDisplayIndex;
   int     hiddev;
   int     usb_busno;
   int     usb_devno;
   char *  mfg_id;
   char *  model_name;
   char *  serial_ascii;
   Byte *  edidbytes;
} Display_Criteria;


/** Allocates a new #Display_Criteria and initializes it to contain no criteria.
 *
 * \return initialized #Display_Criteria
 */
static Display_Criteria *
new_display_criteria() {
   bool debug = false;
   DBGMSF(debug, "Starting");
   Display_Criteria * criteria = calloc(1, sizeof(Display_Criteria));
   criteria->dispno = -1;
   criteria->i2c_busno  = -1;
   criteria->iAdapterIndex = -1;
   criteria->iDisplayIndex = -1;
   criteria->hiddev = -1;
   criteria->usb_busno = -1;
   criteria->usb_devno = -1;
   DBGMSF(debug, "Done.    Returning: %p", criteria);
   return criteria;
}


/** Checks if a given #Display_Ref satisfies all the criteria specified in a
 *  #Display_Criteria struct.
 *
 *  \param  drec     pointer to #Display_Ref to test
 *  \param  criteria pointer to criteria
 *  \retval true     all specified criteria match
 *  \retval false    at least one specified criterion does not match
 *
 *  \remark
 *  In the degenerate case that no criteria are set in **criteria**, returns true.
 */
static bool
ddc_check_display_ref(Display_Ref * dref, Display_Criteria * criteria) {
   TRACED_ASSERT(dref && criteria);
   bool result = false;

   if (criteria->dispno >= 0 && criteria->dispno != dref->dispno)
      goto bye;

   if (criteria->i2c_busno >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_I2C || dref->io_path.path.i2c_busno != criteria->i2c_busno)
         goto bye;
   }

#ifdef USE_USB
   if (criteria->hiddev >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_USB)
         goto bye;
      char buf[40];
      snprintf(buf, 40, "%s/hiddev%d", usb_hiddev_directory(), criteria->hiddev);
      Usb_Monitor_Info * moninfo = dref->detail;
      TRACED_ASSERT(memcmp(moninfo->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
      if (!streq( moninfo->hiddev_device_name, buf))
         goto bye;
   }

   if (criteria->usb_busno >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_USB)
         goto bye;
      // Usb_Monitor_Info * moninfo = drec->detail2;
      // assert(memcmp(moninfo->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
      // if ( moninfo->hiddev_devinfo->busnum != criteria->usb_busno )
      if ( dref->usb_bus != criteria->usb_busno )
         goto bye;
   }

   if (criteria->usb_devno >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_USB)
         goto bye;
      // Usb_Monitor_Info * moninfo = drec->detail2;
      // assert(memcmp(moninfo->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
      // if ( moninfo->hiddev_devinfo->devnum != criteria->usb_devno )
      if ( dref->usb_device != criteria->usb_devno )
         goto bye;
   }

   if (criteria->hiddev >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_USB)
         goto bye;
      if ( dref->io_path.path.hiddev_devno != criteria->hiddev )
         goto bye;
   }
#endif

   if (criteria->mfg_id && (strlen(criteria->mfg_id) > 0) &&
         !streq(dref->pedid->mfg_id, criteria->mfg_id) )
      goto bye;

   if (criteria->model_name && (strlen(criteria->model_name) > 0) &&
         !streq(dref->pedid->model_name, criteria->model_name) )
      goto bye;

   if (criteria->serial_ascii && (strlen(criteria->serial_ascii) > 0) &&
         !streq(dref->pedid->serial_ascii, criteria->serial_ascii) )
      goto bye;

   if (criteria->edidbytes && memcmp(dref->pedid->bytes, criteria->edidbytes, 128) != 0)
      goto bye;

   result = true;

bye:
   return result;
}


/**
 *  \param all_displays #GPtrArray of pointers to #Display_Ref
 */
void ddc_async_scan(GPtrArray * all_displays) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "all_displays=%p, display_count=%d", all_displays, all_displays->len);

   GPtrArray * threads = g_ptr_array_new();
   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
      TRACED_ASSERT( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );

      GThread * th =
      g_thread_new(
            dref_repr_t(dref),
            threaded_initial_checks_by_dref,
            dref);
      g_ptr_array_add(threads, th);
   }
   DBGMSF(debug, "Started %d threads", threads->len);
   for (int ndx = 0; ndx < threads->len; ndx++) {
      GThread * thread = g_ptr_array_index(threads, ndx);
      g_thread_join(thread);  // implicitly unrefs the GThread
   }
   DBGMSF(debug, "Threads joined");
   g_ptr_array_free(threads, true);

#ifdef OLD
   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
      assert( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );
      if (dref->flags & DREF_DDC_COMMUNICATION_WORKING) {
         dref->dispno = ++dispno_max;
      }
      else {
         dref->dispno = -1;
      }
   }
#endif
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void ddc_non_async_scan(GPtrArray * all_displays) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "checking %d displays", all_displays->len);

   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
      TRACED_ASSERT( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );
      ddc_initial_checks_by_dref(dref);

#ifdef OLD
      if (dref->flags & DREF_DDC_COMMUNICATION_WORKING) {
         dref->dispno = ++dispno_max;
      }
      else {
         dref->dispno = -1;
      }
#endif
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


static Display_Ref *
ddc_find_display_ref_by_criteria(Display_Criteria * criteria) {
   Display_Ref * result = NULL;
   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * drec = g_ptr_array_index(all_displays, ndx);
      TRACED_ASSERT(memcmp(drec->marker, DISPLAY_REF_MARKER, 4) == 0);
      if (ddc_check_display_ref(drec, criteria)) {
         result = drec;
         break;
      }
   }
   return result;
}


bool is_phantom_display(Display_Ref* invalid_dref, Display_Ref * valid_dref) {
   bool debug = false;
   char * invalid_repr = strdup(dref_repr_t(invalid_dref));
   char *   valid_repr = strdup(dref_repr_t(valid_dref));
   DBGTRC_STARTING(debug, TRACE_GROUP, "invalid_dref=%s, valid_dref=%s",
                 invalid_repr, valid_repr);
   free(invalid_repr);
   free(valid_repr);

   bool result = false;
   if (memcmp(invalid_dref->pedid, valid_dref->pedid, 128) == 0) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "EDIDs match");
      if (invalid_dref->io_path.io_mode == DDCA_IO_I2C &&
            valid_dref->io_path.io_mode == DDCA_IO_I2C)
      {
         int busno = invalid_dref->io_path.path.i2c_busno;
         // int valid_busno = valid_dref->io_path.path.i2c_busno;
         char buf0[40];
         snprintf(buf0, 40, "/sys/bus/i2c/devices/i2c-%d", busno);
         bool old_silent = set_rpt_sysfs_attr_silent(!(debug|| IS_TRACING()));
         char * rpath = NULL;
         bool ok = RPT_ATTR_REALPATH(0, &rpath, buf0, "device");
         if (ok) {
            result = true;
            char * attr_value = NULL;
            ok = RPT_ATTR_TEXT(0, &attr_value, rpath, "status");
            if (!ok  || !streq(attr_value, "disconnected"))
               result = false;
            ok = RPT_ATTR_TEXT(0, &attr_value, rpath, "enabled");
            if (!ok  || !streq(attr_value, "disabled"))
               result = false;
            GByteArray * edid;
            ok = RPT_ATTR_EDID(0, &edid, rpath, "edid");    // is "edid" needed
            if (ok) {
               result = false;
               g_byte_array_free(edid, true);
            }
         }
         set_rpt_sysfs_attr_silent(old_silent);
      }
   }
   DBGTRC_DONE(debug, TRACE_GROUP,    "Returning: %s", sbool(result) );
   return result;
}


void filter_phantom_displays(GPtrArray * all_displays) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "all_displays->len = %d", all_displays->len);
   GPtrArray* valid_displays   = g_ptr_array_sized_new(all_displays->len);
   GPtrArray* invalid_displays = g_ptr_array_sized_new(all_displays->len);
   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
      TRACED_ASSERT( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );
      if (dref->dispno < 0)     // DISPNO_INVALID, DISPNO_PHANTOM, DISPNO_REMOVED
         g_ptr_array_add(invalid_displays, dref);
      else
         g_ptr_array_add(valid_displays, dref);
   }
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "%d valid displays, %d invalid displays",
                              valid_displays->len, invalid_displays->len);
   if (invalid_displays->len > 0 || valid_displays->len == 0 ) {
      for (int invalid_ndx = 0; invalid_ndx < invalid_displays->len; invalid_ndx++) {
         Display_Ref * invalid_ref = g_ptr_array_index(invalid_displays, invalid_ndx);
         for (int valid_ndx = 0; valid_ndx < valid_displays->len; valid_ndx++) {
            Display_Ref *  valid_ref = g_ptr_array_index(valid_displays, valid_ndx);
            if (is_phantom_display(invalid_ref, valid_ref)) {
               invalid_ref->dispno = DISPNO_PHANTOM;    // -2
               invalid_ref->actual_display = valid_ref;
            }
         }
      }
   }
   g_ptr_array_free(invalid_displays, true);
   g_ptr_array_free(valid_displays, true);
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


/** Searches the master display list for a display matching the
 *  specified #Display_Identifier, returning its #Display_Ref
 *
 *  \param did display identifier to search for
 *  \return #Display_Ref for the display, NULL if not found or
 *          display doesn't support DDC
 *
 * \remark
 * The returned value is a pointer into an internal data structure
 * and should not be freed by the caller.
 */
Display_Ref *
ddc_find_display_ref_by_display_identifier(Display_Identifier * did) {
   bool debug = false;
   DBGMSF(debug, "Starting. did=%s", did_repr(did));
   if (debug)
      dbgrpt_display_identifier(did, 1);

   Display_Ref * result = NULL;

   Display_Criteria * criteria = new_display_criteria();

   switch(did->id_type) {
   case DISP_ID_BUSNO:
      criteria->i2c_busno = did->busno;
      break;
   case DISP_ID_ADL:
      PROGRAM_LOGIC_ERROR("ADL implementation removed");
      break;
   case DISP_ID_MONSER:
      criteria->mfg_id = did->mfg_id;
      criteria->model_name = did->model_name;
      criteria->serial_ascii = did->serial_ascii;
      break;
   case DISP_ID_EDID:
      criteria->edidbytes = did->edidbytes;
      break;
   case DISP_ID_DISPNO:
      criteria->dispno = did->dispno;
      break;
   case DISP_ID_USB:
      criteria->usb_busno = did->usb_bus;
      criteria->usb_devno = did->usb_device;
      break;
   case DISP_ID_HIDDEV:
      criteria->hiddev = did->hiddev_devno;
   }

   result = ddc_find_display_ref_by_criteria(criteria);

   // Is this the best location in the call chain to make this check?
   if (result && (result->dispno < 0)) {
      DBGMSF(debug, "Found a display that doesn't support DDC.  Ignoring.");
      result = NULL;
   }

   free(criteria);   // do not free pointers in criteria, they are owned by Display_Identifier

   if (debug) {
      if (result) {
         DBGMSG("Done.     Returning: ");
         ddc_dbgrpt_display_ref(result, 1);
      }
      else
         DBGMSG("Done.     Returning NULL");
   }

   return result;
}


/** Searches the detected displays for one matching the criteria in a
 *  #Display_Identifier.
 *
 *  \param pdid  pointer to a #Display_Identifier
 *  \param callopts  standard call options
 *  \return pointer to #Display_Ref for the display, NULL if not found
 *
 *  \todo
 *  If the criteria directly specify an access path
 *  (e.g. I2C bus number) and CALLOPT_FORCE specified, then create a
 *  temporary #Display_Ref, bypassing the list of detected monitors.
 */
Display_Ref *
get_display_ref_for_display_identifier(
                Display_Identifier* pdid,
                Call_Options        callopts)
{
   Display_Ref * dref = ddc_find_display_ref_by_display_identifier(pdid);
   if ( !dref && (callopts & CALLOPT_ERR_MSG) ) {
      f0printf(ferr(), "Display not found\n");
   }

   return dref;
}


/** Detects all connected displays by querying the I2C and USB subsystems.
 *
 * \return array of #Display_Ref
 */
// static
GPtrArray *
ddc_detect_all_displays() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "check_phantom_displays=%s", sbool(check_phantom_displays));
   dispno_max = 0;

   GPtrArray * display_list = g_ptr_array_new();

   int busct = i2c_detect_buses();
   DBGMSF(debug, "i2c_detect_buses() returned: %d", busct);
   uint busndx = 0;
   for (busndx=0; busndx < busct; busndx++) {
      I2C_Bus_Info * businfo = i2c_get_bus_info_by_index(busndx);
      if ( (businfo->flags & I2C_BUS_ADDR_0X50)  && businfo->edid ) {
         Display_Ref * dref = create_bus_display_ref(businfo->busno);
         dref->dispno = DISPNO_INVALID;   // -1
         dref->pedid = businfo->edid;    // needed?
         dref->mmid  = monitor_model_key_new(
                          dref->pedid->mfg_id,
                          dref->pedid->model_name,
                          dref->pedid->product_code);

         // drec->detail.bus_detail = businfo;
         dref->detail = businfo;
         dref->flags |= DREF_DDC_IS_MONITOR_CHECKED;
         dref->flags |= DREF_DDC_IS_MONITOR;
         g_ptr_array_add(display_list, dref);
      }
   }

#ifdef USE_USB
   if (detect_usb_displays) {
      GPtrArray * usb_monitors = get_usb_monitor_list();
      // DBGMSF(debug, "Found %d USB displays", usb_monitors->len);
      for (int ndx=0; ndx<usb_monitors->len; ndx++) {
         Usb_Monitor_Info  * curmon = g_ptr_array_index(usb_monitors,ndx);
         TRACED_ASSERT(memcmp(curmon->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
         Display_Ref * dref = create_usb_display_ref(
                                   curmon->hiddev_devinfo->busnum,
                                   curmon->hiddev_devinfo->devnum,
                                   curmon->hiddev_device_name);
         dref->dispno = DISPNO_INVALID;   // -1
         dref->pedid = curmon->edid;
         if (dref->pedid)
            dref->mmid  = monitor_model_key_new(
                             dref->pedid->mfg_id,
                             dref->pedid->model_name,
                             dref->pedid->product_code);
         else
            dref->mmid = monitor_model_key_new("UNK", "UNK", 0);
         // drec->detail.usb_detail = curmon;
         dref->detail = curmon;
         dref->flags |= DREF_DDC_IS_MONITOR_CHECKED;
         dref->flags |= DREF_DDC_IS_MONITOR;
         g_ptr_array_add(display_list, dref);
      }
   }
#endif

   // verbose output is distracting within scans
   // saved and reset here so that async threads are not adjusting output level
   DDCA_Output_Level olev = get_output_level();
   if (olev == DDCA_OL_VERBOSE)
      set_output_level(DDCA_OL_NORMAL);

   DBGMSF(debug, "display_list->len=%d, async_threshold=%d",
                 display_list->len, async_threshold);
   if (display_list->len >= async_threshold)
      ddc_async_scan(display_list);
   else
      ddc_non_async_scan(display_list);

   if (olev == DDCA_OL_VERBOSE)
      set_output_level(olev);

   // assign display numbers
   for (int ndx = 0; ndx < display_list->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(display_list, ndx);
      TRACED_ASSERT( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );
      if (dref->flags & DREF_DDC_COMMUNICATION_WORKING) {
         dref->dispno = ++dispno_max;

         // check_dynamic_features(dref);    // wrong location for hook
      }
      else {
         dref->dispno = DISPNO_INVALID;   // -1;
      }
   }

   if (check_phantom_displays)      // for testing
      filter_phantom_displays(display_list);

   if (debug) {
      DBGMSG("Displays detected:");
      dbgrpt_dref_ptr_array("display_list:", display_list, 1);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %p, Detected %d valid displays",
                display_list, dispno_max);
   return display_list;
}


/** Initializes the master display list.
 *
 *  Does nothing if the list has already been initialized.
 */
void
ddc_ensure_displays_detected() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   if (!all_displays) {
      i2c_detect_buses();
      all_displays = ddc_detect_all_displays();
   }
   DBGTRC_DONE(debug, TRACE_GROUP,
               "all_displays=%p, all_displays has %d displays",
               all_displays, all_displays->len);
}


void
ddc_discard_detected_displays() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   // grab locks to prevent any opens?
   ddc_close_all_displays();
   if (all_displays) {
      for (int ndx = 0; ndx < all_displays->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
         dref->flags |= DREF_TRANSIENT;  // hack to allow all Display References to be freed
#ifndef NDEBUG
         DDCA_Status ddcrc = free_display_ref(dref);
         TRACED_ASSERT(ddcrc==0);
#endif
      }
      g_ptr_array_free(all_displays, true);
      all_displays = NULL;
   }
   i2c_discard_buses();
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void
ddc_redetect_displays() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "all_displays=%p", all_displays);
   ddc_discard_detected_displays();
   i2c_detect_buses();
   all_displays = ddc_detect_all_displays();
   if (debug) {
      dbgrpt_dref_ptr_array("all_displays:", all_displays, 1);
      // dbgrpt_valid_display_refs(1);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "all_displays=%p, all_displays->len = %d",
                                   all_displays, all_displays->len);
}


bool
ddc_is_valid_display_ref(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%p -> %s", dref, dref_repr_t(dref));
   bool result = false;
   if (all_displays) {
      for (int ndx = 0; ndx < all_displays->len; ndx++) {
         Display_Ref* cur = g_ptr_array_index(all_displays, ndx);
         DBGMSF(debug, "Checking vs valid dref %p", cur);

         if (cur == dref) {
            // if (cur->dispno > 0)  // why?
               result = true;
            break;
         }
      }
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %s. dref=%p, dispno=%d", sbool(result), dref, dref->dispno);
   return result;
}

void dbgrpt_valid_display_refs(int depth) {
   rpt_vstring(depth, "Valid display refs = all_displays:");
   if (all_displays) {
      if (all_displays->len == 0)
         rpt_vstring(depth+1, "None");
      else {
         for (int ndx = 0; ndx < all_displays->len; ndx++) {
            Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
            rpt_vstring(depth+1, "%p, dispno=%d", dref, dref->dispno);
         }
      }
   }
   else
      rpt_vstring(depth+1, "all_displays == NULL");
}


/** Indicates whether displays have already been detected
 *
 *  @return true/false
 */
bool
ddc_displays_already_detected()
{
   return all_displays;
}


/** Controls whether USB displays are to be detected.
 *
 *  Must be called before any function that triggers display detection.
 *
 *  @param  onoff  true if USB displays are to be detected, false if not
 *  @retval DDCRC_OK  normal
 *  @retval DDCRC_INVALID_OPERATION function called after displays have been detected
 *  @retval DDCRC_UNIMPLEMENTED ddcutil was not built with USB support
 *
 *  @remark
 *  If this function is not called, the default (if built with USB support) is on
 */
DDCA_Status
ddc_enable_usb_display_detection(bool onoff) {
   bool debug = false;
   DBGMSF(debug, "Starting. onoff=%s", sbool(onoff));

   DDCA_Status rc = DDCRC_UNIMPLEMENTED;
#ifdef USE_USB
   if (ddc_displays_already_detected()) {
      rc = DDCRC_INVALID_OPERATION;
   }
   else {
      detect_usb_displays = onoff;
      rc = DDCRC_OK;
   }
#endif
   DBGMSF(debug, "Done.     Returning %s", psc_name_code(rc));
   return rc;
}


/** Indicates whether USB displays are to be detected
 *
 *  @return true/false
 */
bool
ddc_is_usb_display_detection_enabled() {
   return detect_usb_displays;
}


void
init_ddc_displays() {
   RTTI_ADD_FUNC(ddc_async_scan);
   RTTI_ADD_FUNC(ddc_redetect_displays);
   RTTI_ADD_FUNC(ddc_detect_all_displays);
   RTTI_ADD_FUNC(filter_phantom_displays);
   RTTI_ADD_FUNC(ddc_initial_checks_by_dh);
   RTTI_ADD_FUNC(ddc_initial_checks_by_dref);
   RTTI_ADD_FUNC(is_phantom_display);
   RTTI_ADD_FUNC(ddc_non_async_scan);
   RTTI_ADD_FUNC(threaded_initial_checks_by_dref);
   RTTI_ADD_FUNC(ddc_is_valid_display_ref);
   RTTI_ADD_FUNC(get_controller_mfg_string_t);
   RTTI_ADD_FUNC(get_firmware_version_string_t);
}


