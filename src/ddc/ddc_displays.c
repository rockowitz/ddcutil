/** \file ddc_displays.c
 * Access displays, whether DDC, ADL, or USB
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#include <config.h>

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>    // glib-2.0/ to make eclipse happy
#include <string.h>
#include <time.h>

#include "util/debug_util.h"
#include "util/edid.h"
#include "util/error_info.h"
#include "util/failsim.h"
#include "util/report_util.h"
#include "util/udev_usb_util.h"
#include "util/udev_util.h"
/** \endcond */

#include "base/adl_errors.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/feature_metadata.h"
#include "base/linux_errno.h"
#include "base/monitor_model_key.h"
#include "base/parms.h"

#include "vcp/vcp_feature_codes.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_do_io.h"

#include "adl/adl_shim.h"
#ifdef HAVE_ADL
#include "adl/adl_impl/adl_intf.h"
#endif

#ifdef USE_USB
#include "usb/usb_displays.h"
#endif

#include <dynvcp/dyn_dynamic_features.h>

#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_vcp_version.h"

#include "ddc/ddc_displays.h"


// Default race class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDCIO;


static GPtrArray * all_displays = NULL;    // all detected displays
static int dispno_max = 0;                 // highest assigned display number
static int async_threshold = DISPLAY_CHECK_ASYNC_THRESHOLD;
#ifdef USE_USB
static bool detect_usb_displays = true;
#else
static bool detect_usb_displays = false;
#endif

void ddc_set_async_threshold(int threshold) {
   // DBGMSG("threshold = %d", threshold);
   async_threshold = threshold;
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
 *  Monitors are supposed to set the unsupported feature bit in a valid DDC
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
bool initial_checks_by_dh(Display_Handle * dh) {
   bool debug = true;
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%s", dh_repr_t(dh));
   assert(dh);
   DDCA_Any_Vcp_Value * pvalrec;

   if (!(dh->dref->flags & DREF_DDC_COMMUNICATION_CHECKED)) {
      Public_Status_Code psc = 0;
      Error_Info * ddc_excp = ddc_get_vcp_value(dh, 0x00, DDCA_NON_TABLE_VCP_VALUE, &pvalrec);
      psc = (ddc_excp) ? ddc_excp->status_code : 0;
      DBGTRC(debug, TRACE_GROUP, "ddc_get_vcp_value() for feature 0x00 returned: %s, pvalrec=%p",
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
         assert( (psc == 0 && pvalrec) || (psc != 0 && !pvalrec) );

      }
      else {
         assert(psc != DDCRC_DETERMINED_UNSUPPORTED);  // only set at higher levels, unless USB

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
               assert( psc == 0);
               assert(pvalrec);

            }
            assert( (psc == 0 && pvalrec) || (psc != 0 && !pvalrec) );
         }

         if (psc == 0) {

            assert( pvalrec->value_type == DDCA_NON_TABLE_VCP_VALUE );
            if (debug || IS_TRACING()) {
               DBGMSG("pvalrec:");
               dbgrpt_single_vcp_value(pvalrec, 1);
            }

            DBGTRC(debug, TRACE_GROUP, "value_type=%d, mh=%d, ml=%d, sh=%d, sl=%d",
                     pvalrec->value_type,
                     pvalrec->val.c_nc.mh,
                     pvalrec->val.c_nc.ml,
                     pvalrec->val.c_nc.sh,
                     pvalrec->val.c_nc.sl);
            if ( pvalrec->val.c_nc.mh == 0 &&
                 pvalrec->val.c_nc.ml == 0 &&
                 pvalrec->val.c_nc.sh == 0 &&
                 pvalrec->val.c_nc.sl == 0
              )
            {
               DBGTRC(debug, TRACE_GROUP, "Setting DREF_DDC_USES_MH_ML_SH_SL_ZERO_FOR_UNSUPPORTED");
               dh->dref->flags |= DREF_DDC_USES_MH_ML_SH_SL_ZERO_FOR_UNSUPPORTED;
            }
            else {
               DBGTRC(debug, TRACE_GROUP, "Setting DREF_DDC_DOES_NOT_INDICATE_UNSUPPORTED");
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
      if ( vcp_version_eq(dh->dref->vcp_version, DDCA_VSPEC_UNQUERIED)) {
         dh->dref->vcp_version = get_vcp_version_by_display_handle(dh);
      }
   }

   DBGTRC(debug, TRACE_GROUP, "dh=%s, Returning: %s",
                 dh_repr_t(dh), sbool(communication_working));
   return communication_working;
}


/** Given a #Display_Ref, opens the monitor device and calls #initial_checks_by_dh()
 *  to perform initial monitor checks.
 *
 *  \param dref pointer to #Display_Ref for monitor
 *  \return **true** if DDC communication with the display succeeded, **false** otherwise.
 */
bool initial_checks_by_dref(Display_Ref * dref) {
   bool debug = true;
   DBGMSF(debug, "Starting. dref=%s", dref_repr_t(dref) );
   bool result = false;
   Display_Handle * dh = NULL;
   Public_Status_Code psc = 0;

   psc = ddc_open_display(dref, CALLOPT_ERR_MSG, &dh);   // deleted CALLOPT_ERR_ABORT
   if (psc == 0)  {
      result = initial_checks_by_dh(dh);
      ddc_close_display(dh);
   }

   DBGMSF(debug, "Done. dref = %s, returning %s", dref_repr_t(dref), sbool(result) );
   return result;
}


// function to be run in thread
void * threaded_initial_checks_by_dref(gpointer data) {
   bool debug = true;

   Display_Ref * dref = data;
   assert(memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );
   DBGMSF(debug, "Starting. dref = %s", dref_repr_t(dref) );

   initial_checks_by_dref(dref);
   // g_thread_exit(NULL);
   DBGMSF(debug, "Done. dref = %s, returning NULL", dref_repr_t(dref) );
   return NULL;
}


//
// Functions to get display information
//

/** Gets a list of all detected displays, whether they support DDC or not.
 *
 *  Initializes the list of detected monitors if necessary.
 *
 *  \return **GPtrArray of #Display_Ref instances
 */
GPtrArray * ddc_get_all_displays() {
   // ddc_ensure_displays_detected();
   assert(all_displays);

   return all_displays;
}


/** Gets the controller firmware version as a string
 *
 * \param dh  pointer to display handle
 * \return pointer to character string, which is valid until the next
 * call to this function.
 *
 * \remark
 * Consider caching the value in dh->dref
 */
static char * get_firmware_version_string_t(Display_Handle * dh) {
   bool debug = false;

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
      }
   }
   else {
      g_snprintf(version, 40, "%d.%d", valrec->val.c_nc.sh, valrec->val.c_nc.sl);
      free_single_vcp_value(valrec);
   }
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
   DBGMSF(debug, "Starting. dh = %s", dh_repr(dh));

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
   }
   else {
      if (debug) {
         DBGMSG("get_nontable_vcp_value(0xc8) returned %s", psc_desc(ddcrc));
         DBGMSG("    Try errors: %s", errinfo_causes_string(ddc_excp));
      }
      mfg_name = "DDC communication failed";
    }

   DBGMSF(debug, "Returning: %s", mfg_name);
   return mfg_name;
}


/** Shows information about a display, specified by a #Display_Ref
 *
 * Output is written using report functions
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
   DBGMSF(debug, "Starting");
   assert(dref);
   assert(memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
   int d1 = depth+1;

   switch(dref->dispno) {
   case -1:
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
         assert(curinfo);
         assert(memcmp(curinfo, I2C_BUS_INFO_MARKER, 4) == 0);

         i2c_report_active_display(curinfo, d1);
      }
      break;
   case DDCA_IO_ADL:
      adlshim_report_active_display_by_display_ref(dref, d1);
      break;
   case DDCA_IO_USB:
#ifdef USE_USB
      usb_show_active_display_by_display_ref(dref, d1);
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
      break;
   }

   assert( dref->flags & DREF_DDC_COMMUNICATION_CHECKED);

   DDCA_Output_Level output_level = get_output_level();

   if (output_level >= DDCA_OL_NORMAL) {
      if (!(dref->flags & DREF_DDC_COMMUNICATION_WORKING) ) {
         rpt_vstring(d1, "DDC communication failed");
         char * msg = NULL;
         if (dref->io_path.io_mode == DDCA_IO_I2C)
         {
             I2C_Bus_Info * curinfo = dref->detail;
             if (curinfo->flags & I2C_BUS_EDP)
                 msg = "This is an eDP laptop display. Laptop displays do not support DDC/CI.";
             else if ( is_embedded_parsed_edid(dref->pedid) )
                 msg = "This appears to be a laptop display. Laptop displays do not support DDC/CI.";
         }
         if (output_level >= DDCA_OL_VERBOSE) {
            if (!msg) {
               msg = "Is DDC/CI enabled in the monitor's on-screen display?";
            }
         }
         if (msg) {
            rpt_vstring(d1, msg);
         }
      }
      else {
         DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_ref(dref);
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
                                  bool_repr(dref->flags & DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED));
               // rpt_vstring(d1, "Monitor returns success with mh=ml=sh=sl=0 for unsupported features: %s",
               //                    bool_repr(dref->flags & DREF_DDC_USES_MH_ML_SH_SL_ZERO_FOR_UNSUPPORTED));
            }
         }
      }
   }

   DBGMSF(debug, "Done");
}


/** Reports all displays found.
 *
 * Output is written to the current report destination using
 * report functions.
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
      assert(memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
      if (dref->dispno > 0 || include_invalid_displays) {
         display_ct++;
         ddc_report_display_by_dref(dref, depth);
         rpt_title("",0);
      }
   }
   if (display_ct == 0)
      rpt_vstring(depth, "No %sdisplays found", (!include_invalid_displays) ? "active " : "");

   DBGMSF(debug, "Done.  Returning: %d", display_ct);
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
         assert( memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0);
         i2c_dbgrpt_bus_info(businfo, d2);
         break;
   case(DDCA_IO_ADL):
#ifdef HAVE_ADL
      rpt_vstring(d1, "ADL device information: ");
      ADL_Display_Detail * adl_detail = dref->detail;
      assert(memcmp(adl_detail->marker, ADL_DISPLAY_DETAIL_MARKER, 4) == 0);
      adlshim_report_adl_display_detail(adl_detail, d2);
#endif
      break;
   case(DDCA_IO_USB):
#ifdef USE_USB
         rpt_vstring(d1, "USB device information: ");
         Usb_Monitor_Info * moninfo = dref->detail;
         assert(memcmp(moninfo->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
         report_usb_monitor_info(moninfo, d2);
#else
         PROGRAM_LOGIC_ERROR("Built without USB support");
#endif
   break;
   }

   // set_output_level(saved_output_level);
}


/** Debugging function to report a collection of #Display_Ref.
 *
 * \param recs    pointer to collection of #Display_Ref
 * \param depth   logical indentation depth
 */
void ddc_dbgrpt_display_refs(GPtrArray * recs, int depth) {
   assert(recs);
   rpt_vstring(depth, "Reporting %d Display_Ref instances", recs->len);
   for (int ndx = 0; ndx < recs->len; ndx++) {
      Display_Ref * drec = g_ptr_array_index(recs, ndx);
      assert( memcmp(drec->marker, DISPLAY_REF_MARKER, 4) == 0);
      rpt_nl();
      ddc_dbgrpt_display_ref(drec, depth+1);
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
   Display_Criteria * criteria = calloc(1, sizeof(Display_Criteria));
   criteria->dispno = -1;
   criteria->i2c_busno  = -1;
   criteria->iAdapterIndex = -1;
   criteria->iDisplayIndex = -1;
   criteria->hiddev = -1;
   criteria->usb_busno = -1;
   criteria->usb_devno = -1;
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
   assert(dref && criteria);
   bool result = false;

   if (criteria->dispno >= 0 && criteria->dispno != dref->dispno)
      goto bye;

   if (criteria->i2c_busno >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_I2C || dref->io_path.path.i2c_busno != criteria->i2c_busno)
         goto bye;
   }

   if (criteria->iAdapterIndex >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_ADL || dref->io_path.path.adlno.iAdapterIndex != criteria->iAdapterIndex)
         goto bye;
   }

   if (criteria->iDisplayIndex >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_ADL || dref->io_path.path.adlno.iDisplayIndex != criteria->iDisplayIndex)
         goto bye;
   }

   if (criteria->hiddev >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_USB)
         goto bye;
      char buf[40];
      snprintf(buf, 40, "%s/hiddev%d", usb_hiddev_directory(), criteria->hiddev);
      Usb_Monitor_Info * moninfo = dref->detail;
      assert(memcmp(moninfo->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
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
void async_scan(GPtrArray * all_displays) {
   bool debug = true;
   DBGTRC(debug, TRACE_GROUP, "Starting. all_displays=%p, display_count=%d", all_displays, all_displays->len);

   GPtrArray * threads = g_ptr_array_new();
   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
      assert( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );

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
   DBGTRC(debug, TRACE_GROUP, "Done");
}


void non_async_scan(GPtrArray * all_displays) {
   bool debug = true;
   DBGTRC(debug, TRACE_GROUP, "Starting. checking %d displays", all_displays->len);

   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
      assert( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );
      initial_checks_by_dref(dref);

#ifdef OLD
      if (dref->flags & DREF_DDC_COMMUNICATION_WORKING) {
         dref->dispno = ++dispno_max;
      }
      else {
         dref->dispno = -1;
      }
#endif
   }
   DBGTRC(debug, TRACE_GROUP, "Done");
}



static Display_Ref *
ddc_find_display_ref_by_criteria(Display_Criteria * criteria) {
   Display_Ref * result = NULL;
   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * drec = g_ptr_array_index(all_displays, ndx);
      assert(memcmp(drec->marker, DISPLAY_REF_MARKER, 4) == 0);
      if (ddc_check_display_ref(drec, criteria)) {
         result = drec;
         break;
      }
   }
   return result;
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
   DBGMSF(debug, "Starting");
   if (debug)
      dbgrpt_display_identifier(did, 1);

   Display_Ref * result = NULL;

   Display_Criteria * criteria = new_display_criteria();

   switch(did->id_type) {
   case DISP_ID_BUSNO:
      criteria->i2c_busno = did->busno;
      break;
   case DISP_ID_ADL:
      criteria->iAdapterIndex = did->iAdapterIndex;
      criteria->iDisplayIndex = did->iDisplayIndex;
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
         DBGMSG0("Done.  Returning: ");
         ddc_dbgrpt_display_ref(result, 1);
      }
      else
         DBGMSG0("Done.  Returning NULL");
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


/** Detects all connected displays by querying the I2C, ADL, and USB subsystems.
 *
 * \return array of #Display_Ref
 */
static
GPtrArray *
ddc_detect_all_displays() {
   bool debug = true;
   DBGTRC(debug, TRACE_GROUP, "Starting");

   GPtrArray * display_list = g_ptr_array_new();

   int busct = i2c_detect_buses();
   // DBGMSF(debug, "i2c_detect_buses() returned: %d", busct);
   int busndx = 0;
   for (busndx=0; busndx < busct; busndx++) {
      I2C_Bus_Info * businfo = i2c_get_bus_info_by_index(busndx);
      if ( (businfo->flags & I2C_BUS_ADDR_0X50)  && businfo->edid ) {
         Display_Ref * dref = create_bus_display_ref(businfo->busno);
         dref->dispno = -1;
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

  GPtrArray * all_adl_details = adlshim_get_valid_display_details();
  int adlct = all_adl_details->len;
  for (int ndx = 0; ndx < adlct; ndx++) {
     ADL_Display_Detail * detail = g_ptr_array_index(all_adl_details, ndx);
     Display_Ref * dref = create_adl_display_ref(detail->iAdapterIndex, detail->iDisplayIndex);
     dref->dispno = -1;
     dref->pedid = detail->pEdid;   // needed?
     dref->mmid  = monitor_model_key_new(
                      dref->pedid->mfg_id,
                      dref->pedid->model_name,
                      dref->pedid->product_code);
     // drec->detail.adl_detail = detail;
     dref->detail = detail;
     dref->flags |= DREF_DDC_IS_MONITOR_CHECKED;
     dref->flags |= DREF_DDC_IS_MONITOR;
     g_ptr_array_add(display_list, dref);
  }
  // Unlike businfo and usb_monitors, which point to persistent data structures,
  // all_adl_details points to a transitory data structure created by
  // adlshim_get_valid_display_details() and must be freed
  g_ptr_array_free(all_adl_details, true);

#ifdef USE_USB
   if (detect_usb_displays) {
      GPtrArray * usb_monitors = get_usb_monitor_list();
      // DBGMSF(debug, "Found %d USB displays", usb_monitors->len);
      for (int ndx=0; ndx<usb_monitors->len; ndx++) {
         Usb_Monitor_Info  * curmon = g_ptr_array_index(usb_monitors,ndx);
         assert(memcmp(curmon->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
         Display_Ref * dref = create_usb_display_ref(
                                   curmon->hiddev_devinfo->busnum,
                                   curmon->hiddev_devinfo->devnum,
                                   curmon->hiddev_device_name);
         dref->dispno = -1;
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

   DBGMSF(debug, "display_list->len=%d, async_threshold=%d, adlct=%d",
                 display_list->len, async_threshold, adlct);
   // ADL displays do not support async scan.  Not worth implementing.
   if (display_list->len >= async_threshold && adlct == 0)
      async_scan(display_list);
   else
      non_async_scan(display_list);

   if (olev == DDCA_OL_VERBOSE)
      set_output_level(olev);

   // assign display numbers
   for (int ndx = 0; ndx < display_list->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(display_list, ndx);
      assert( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );
      if (dref->flags & DREF_DDC_COMMUNICATION_WORKING) {
         dref->dispno = ++dispno_max;

         // check_dynamic_features(dref);    // wrong location for hook

      }
      else {
         dref->dispno = -1;
      }
   }

   // if (debug) {
   //    DBGMSG("Displays detected:");
   //    report_display_recs(display_list, 1);
   // }
   DBGTRC(debug, TRACE_GROUP, "Done. Detected %d valid displays", dispno_max);
   return display_list;
}


/** Initializes the master display list.
 *
 *  Does nothing if the list has already been initialized.
 */
void
ddc_ensure_displays_detected() {
   bool debug = true;
   DBGMSF(debug, "Starting.");
   if (!all_displays) {
      i2c_detect_buses();
      all_displays = ddc_detect_all_displays();

   }
   DBGMSF(debug, "all_displays has %d displays", all_displays->len);

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
#ifdef USE_USB
   DDCA_Status rc = DDCRC_INVALID_OPERATION;
   if (!ddc_displays_already_detected()) {
      detect_usb_displays = onoff;
      rc = DDCRC_OK;
      // DBGMSG("detect_usb_displays = %s", sbool(detect_usb_displays));
   }
   return rc;
#else
   return DDCRC_UNIMPLEMENTED;
#endif
}


/** Indicates whether USB displays are to be detected
 *
 *  @return true/false
 */
bool
ddc_is_usb_display_detection_enabled() {
   return detect_usb_displays;
}

