/** @file main.c
 *
 *  ddcutil standalone application mainline
 */

// Copyright (C) 2014-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <config.h>

#include <assert.h>
#include <base/base_services.h>
#include <ctype.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "util/data_structures.h"
#include "util/ddcutil_config_file.h"
#include "util/error_info.h"
#include "util/failsim.h"
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/i2c_util.h"
#ifdef USE_LIBDRM
#include "util/libdrm_util.h"
#endif
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/simple_ini_file.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_filter_functions.h"
#include "util/sysfs_i2c_util.h"
#include "util/sysfs_util.h"
#include "util/xdg_util.h"
/** \endcond */

#include "public/ddcutil_types.h"

#include "base/build_info.h"
#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/display_retry_data.h"
#include "base/displays.h"
#include "base/dsa2.h"
#include "base/linux_errno.h"
#include "base/monitor_model_key.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"
#include "base/tuned_sleep.h"

#include "i2c/i2c_sysfs.h"

#include "vcp/parse_capabilities.h"
#include "vcp/persistent_capabilities.h"
#include "vcp/vcp_feature_codes.h"

#include "dynvcp/dyn_feature_files.h"
#include "dynvcp/dyn_parsed_capabilities.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_dpms.h"
#include "i2c/i2c_strategy_dispatcher.h"
#include "i2c/i2c_sysfs.h"

#ifdef ENABLE_USB
#include "usb/usb_displays.h"
#endif

#include "ddc/ddc_common_init.h"
#include "ddc/ddc_displays.h"
#include "ddc/ddc_display_ref_reports.h"
#include "ddc/ddc_display_selection.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_serialize.h"
#include "ddc/ddc_services.h"
#include "ddc/ddc_try_data.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_watch_displays.h"

#include "cmdline/cmd_parser_aux.h"    // for parse_feature_id_or_subset(), should it be elsewhere?
#include "cmdline/cmd_parser.h"
#include "cmdline/parsed_cmd.h"

#include "test/testcases.h"

#include "app_ddcutil/app_capabilities.h"
#include "app_ddcutil/app_dynamic_features.h"
#include "app_ddcutil/app_dumpload.h"
#include "app_ddcutil/app_experimental.h"
#include "app_ddcutil/app_interrogate.h"
#include "app_ddcutil/app_probe.h"
#include "app_ddcutil/app_getvcp.h"
#include <app_ddcutil/app_ddcutil_services.h>
#include "app_ddcutil/app_setvcp.h"
#include "app_ddcutil/app_vcpinfo.h"
#include "app_ddcutil/app_watch.h"
#ifdef INCLUDE_TESTCASES
#include "app_ddcutil/app_testcases.h"
#endif

#ifdef ENABLE_ENVCMDS
#include "app_sysenv/app_sysenv_services.h"
#include "app_sysenv/query_sysenv.h"
#ifdef ENABLE_USB
#include "app_sysenv/query_sysenv_usb.h"
#endif
#endif


// Default trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_TOP;

static void add_local_rtti_functions();


//
// Report core settings and command line options
//

static void
report_performance_options(int depth)
{
      int d1 = depth+1;
      rpt_label(depth, "Performance and Retry Options:");
      rpt_vstring(d1, "Deferred sleep enabled:                 %s", sbool( is_deferred_sleep_enabled() ) );
      rpt_vstring(d1, "Dynamic sleep algorithm enabled:        %s", sbool(dsa2_is_enabled()));
      if (dsa2_is_enabled())
      rpt_vstring(d1, "Minimum dynamic sleep multiplier:    %7.2f", dsa2_get_minimum_multiplier());
      rpt_vstring(d1, "Default sleep multiplier factor:     %7.2f", pdd_get_default_sleep_multiplier_factor() );
      rpt_nl();
}


static void
report_optional_features(Parsed_Cmd * parsed_cmd, int depth) {
   rpt_vstring( depth, "%.*s%-*s%s", 0, "", 28, "Force I2C slave address:",
                       sbool(i2c_forceable_slave_addr_flag));
   rpt_vstring( depth, "%.*s%-*s%s", 0, "", 28, "User defined features:",
                       (enable_dynamic_features) ? "enabled" : "disabled" );
                       // "Enable user defined features" is too long a title
   rpt_vstring( depth, "%.*s%-*s%s", 0, "", 28, "Mock feature values:",
                       (enable_mock_data) ? "enabled" : "disabled" );
   rpt_nl();
}


static void
report_all_options(Parsed_Cmd * parsed_cmd, char * config_fn, char * default_options, int depth)
{
    bool debug = false;
    DBGMSF(debug, "Executing...");

    show_ddcutil_version();
    rpt_vstring(depth, "%.*s%-*s%s", 0, "", 28, "Configuration file:",
                         (config_fn) ? config_fn : "(none)");
    if (config_fn)
       rpt_vstring(depth, "%.*s%-*s%s", 0, "", 28, "Configuration file options:", default_options);

    // report_build_options(depth);
    show_reporting();  // uses fout()
    report_optional_features(parsed_cmd, depth);
    report_tracing(depth);
    rpt_nl();
    report_performance_options(depth);
    report_experimental_options(parsed_cmd, depth);
    report_build_options(depth);

    DBGMSF(debug, "Done");
}


//
// Initialization functions called only once but factored out of main() to clarify mainline
//

#ifdef UNUSED
#ifdef TARGET_LINUX

static bool
validate_environment_using_libkmod()
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   bool ok = false;
   if (is_module_loaded_using_sysfs("i2c_dev")) {  // only finds loadable modules, not those built into kernel
      ok = true;
   }
   else {
      int module_status = module_status_using_libkmod("i2c-dev");
      if (module_status == 0) {   // MODULE_STATUS_NOT_FOUND
         ok = false;
         fprintf(stderr, "Module i2c-dev is not loaded and not built into the kernel.\n");
      }
      else if (module_status == KERNEL_MODULE_BUILTIN) {   // 1
         ok = true;
      }
      else if (module_status == KERNEL_MODULE_LOADABLE_FILE) {   //

         int rc = is_module_loaded_using_libkmod("i2c_dev");
         if (rc == 0) {
            fprintf(stderr, "Loadable module i2c-dev exists but is not loaded.\n");
            ok = false;
         }
         else if (rc == 1) {
            ok = true;
         }
         else {
            assert(rc < 0);
            fprintf(stderr, "ddcutil cannot determine if loadable module i2c-dev is loaded.\n");
            ok = true;  // make this just a warning, we'll fail later if not in kernel
            fprintf(stderr, "Execution may fail.\n");
         }
      }

      else {
         assert(module_status < 0);
         fprintf(stderr, "ddcutil cannot determine if module i2c-dev is loaded or built into the kernel.\n");
         ok = true;  // make this just a warning, we'll fail later if not in kernel
         fprintf(stderr, "Execution may fail.\n");
      }
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, ok, "");
   return ok;
}
#endif
#endif


static bool
validate_environment()
{
   bool debug = false;
   DBGMSF(debug, "Starting");
   bool ok;

   ok = dev_i2c_devices_exist();
   if (!ok)
      fprintf(stderr, "No /dev/i2c devices exist.\n");

#ifdef OLD
#ifdef TARGET_LINUX
   if (is_module_loaded_using_sysfs("i2c_dev")) {
      ok = true;
   }
   else {
      ok = validate_environment_using_libkmod();
   }
#else
   ok = true;
#endif
#endif

   if (!ok) {
      fprintf(stderr, "ddcutil requires module i2c-dev.\n");
      // DBGMSF(debug, "Forcing ok = true");
      // ok = true;  // make it just a warning in case we're wrong
   }

   DBGMSF(debug, "Done.    Returning: %s", sbool(ok));
   return ok;
}


STATIC int
verify_i2c_access() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   Bit_Set_256 buses_without_devices = EMPTY_BIT_SET_256;
   Bit_Set_256 inaccessible_devices  = EMPTY_BIT_SET_256;
   int buses_ct = 0;
   int buses_without_devices_ct = 0;
   int inaccessible_devices_ct = 0;

   Bit_Set_256 buses = get_possible_ddc_ci_bus_numbers();  //sysfs bus numbers, not dev-i2c
   buses_ct = bs256_count(buses);
   DBGTRC(debug, TRACE_GROUP, "/sys/bus/i2c/devices to check: %s",
                              bs256_to_string_decimal_t(buses, "i2c-", ", "));
   if (buses_ct == 0) {
      fprintf(stderr, "No /sys/bus/i2c buses that might be used for DDC/CI communication found.\n");
      fprintf(stderr, "No display adapters with i2c buses appear to exist.\n");
   }
   else {
      Bit_Set_256_Iterator iter = bs256_iter_new(buses);
      int busno;
      while ( (busno = bs256_iter_next(iter)) >= 0)  {
         char fnbuf[20];   // oversize to avoid -Wformat-truncation error
         snprintf(fnbuf, sizeof(fnbuf), "/dev/i2c-%d", busno);
         if ( access(fnbuf, R_OK|W_OK) < 0 ) {
            int errsv = errno;   // EACCESS if lack permissions, ENOENT if file doesn't exist
            if (errsv == ENOENT) {
               buses_without_devices = bs256_insert(buses_without_devices, busno);
               fprintf(stderr, "Device %s does not exist. Error = %s\n",
                              fnbuf, linux_errno_desc(errsv));
            }
            else {
               inaccessible_devices = bs256_insert(inaccessible_devices, busno);
               fprintf(stderr, "Device %s is not readable and writable.  Error = %s\n",
                           fnbuf, linux_errno_desc(errsv) );
               include_open_failures_reported(busno);
            }
         }
      }
      bs256_iter_free(iter);
      buses_without_devices_ct = bs256_count(buses_without_devices);
      inaccessible_devices_ct = bs256_count(inaccessible_devices);

      if (buses_without_devices_ct > 0) {
         fprintf(stderr, "/sys/bus/i2c buses without /dev/i2c-N devices: %s\n",
                bs256_to_string_decimal_t(buses_without_devices, "/sys/bus/i2c/devices/i2c-", " ") );
         fprintf(stderr, "Driver i2c_dev must be loaded or builtin\n");
         fprintf(stderr, "See https://www.ddcutil.com/kernel_module\n");
      }
      if (inaccessible_devices_ct > 0) {
         fprintf(stderr, "Devices possibly used for DDC/CI communication cannot be opened: %s\n",
                bs256_to_string_decimal_t(inaccessible_devices, "/dev/i2c-", " "));
         fprintf(stderr, "See https://www.ddcutil.com/i2c_permissions\n");
      }
   }

   int result = buses_ct - (buses_without_devices_ct + inaccessible_devices_ct);
   DBGTRC_DONE(debug, TRACE_GROUP,
         "Returning %d. Total potential buses: %d, buses without devices: %d, inaccessible devices: %d",
         result, buses_ct, buses_without_devices_ct, inaccessible_devices_ct);
   return result;
}


/** Master initialization function
 *
 *   \param  parsed_cmd  parsed command line
 *   \return ok if successful, false if error
 */
STATIC bool
master_initializer(Parsed_Cmd * parsed_cmd) {
   bool debug = false;
   DBGMSF(debug, "Starting ...");
   bool ok = false;
   if (!submaster_initializer(parsed_cmd))    // shared with libddcutil
      goto bye;

#ifdef ENABLE_ENVCMDS
   if (parsed_cmd->cmd_id != CMDID_ENVIRONMENT) {
      // will be reported by the environment command
      if (!validate_environment())
         goto bye;
   }
#else
   if (!validate_environment())
      goto bye;
#endif

   if (!init_experimental_options(parsed_cmd))
      goto bye;
   ok = true;

bye:
   DBGMSF(debug, "Done");
   return ok;
}


STATIC void
ensure_vcp_version_set(Display_Handle * dh)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dh=%s", dh_repr(dh));
   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_dh(dh);
   if (vspec.major < 2 && get_output_level() >= DDCA_OL_NORMAL) {
      f0printf(stdout, "VCP (aka MCCS) version for display is undetected or less than 2.0. "
            "Interpretation may not be accurate.\n");
   }
   DBGMSF(debug, "Done");
}


typedef enum {
   DISPLAY_ID_REQUIRED,
   DISPLAY_ID_USE_DEFAULT,
   DISPLAY_ID_OPTIONAL
} Displayid_Requirement;


const char *
displayid_requirement_name(Displayid_Requirement id) {
   char * result = NULL;
   switch (id) {
   case DISPLAY_ID_REQUIRED:    result = "DISPLAY_ID_REQUIRED";     break;
   case DISPLAY_ID_USE_DEFAULT: result = "DISPLAY_ID_USE_DEFAULT";  break;
   case DISPLAY_ID_OPTIONAL:    result = "DISPLAY_ID_OPTIONAL";     break;
   }
   return result;
}


/** Returns a display reference for the display specified on the command line,
 *  or, if a display is not optional for the command, a reference to the
 *  default display (--display 1).
 *
 *  \param  parsed_cmd  parsed command line
 *  \param  displayid_required how to handle no display specified on command line
 *  \param  dref_loc  where to return display reference
 *  \retval DDCRC_OK
 *  \retval DDCRC_INVALID_DISPLAY
 */
Status_Errno_DDC
find_dref(
      Parsed_Cmd * parsed_cmd,
      Displayid_Requirement displayid_required,
      Display_Ref ** dref_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "did: %s, displayid_required: %s",
                                    did_repr(parsed_cmd->pdid),
                                    displayid_requirement_name(displayid_required));
   FILE * outf = fout();
   Status_Errno_DDC final_result = DDCRC_OK;
   Display_Ref * dref = NULL;

   Display_Identifier * did_work = parsed_cmd->pdid;
   if (did_work && did_work->id_type == DISP_ID_BUSNO) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Special handling for explicit --busno");
      int busno = did_work->busno;
      // is this really a monitor?
      I2C_Bus_Info * businfo = i2c_detect_single_bus(busno);
      if (businfo) {
         if (businfo->flags & I2C_BUS_ADDR_0X50)  {
            dref = create_bus_display_ref(busno);
            dref->dispno = DISPNO_INVALID;      // or should it be DISPNO_NOT_SET?
            dref->pedid = copy_parsed_edid(businfo->edid);
            dref->mmid  = monitor_model_key_new(
                             dref->pedid->mfg_id,
                             dref->pedid->model_name,
                             dref->pedid->product_code);
            // dref->driver_name = get_i2c_device_sysfs_driver(busno);
            // DBGMSG("driver_name = %p -> %s", dref->driver_name, dref->driver_name);
            dref->drm_connector = g_strdup(businfo->drm_connector_name);

            // dref->pedid = i2c_get_parsed_edid_by_busno(did_work->busno);
            dref->detail = businfo;
            dref->flags |= DREF_DDC_IS_MONITOR_CHECKED;
            dref->flags |= DREF_DDC_IS_MONITOR;
            dref->flags |= DREF_TRANSIENT;
            if (!ddc_initial_checks_by_dref(dref)) {
               f0printf(outf, "DDC communication failed for monitor on bus /dev/i2c-%d\n", busno);
               free_display_ref(dref);
               i2c_free_bus_info(businfo);
               dref = NULL;
               final_result = DDCRC_INVALID_DISPLAY;
            }
            else {
               DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Synthetic Display_Ref");
               final_result = DDCRC_OK;
            }
            if (dref && (dref->flags&DREF_DPMS_SUSPEND_STANDBY_OFF)) {
               // should go nowhere, but just in case:
               f0printf(outf, "Monitor on bus /dev/i2c-%d is in a DPMS sleep mode. Expect DDC errors.", busno);
            }
         }  // has edid
         else {   // no EDID found
            f0printf(fout(), "No monitor detected on bus /dev/i2c-%d\n", busno);
            i2c_free_bus_info(businfo);
            final_result = DDCRC_INVALID_DISPLAY;
         }
      }    // businfo allocated
      else {
         f0printf(fout(), "Bus /dev/i2c-%d not found\n", busno);
         final_result = DDCRC_INVALID_DISPLAY;
      }
   }       // DISP_ID_BUSNO
   else {
      if (!did_work && displayid_required == DISPLAY_ID_OPTIONAL) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "No monitor specified, none required for command");
         dref = NULL;
         final_result = DDCRC_OK;
      }
      else {
         bool temporary_did_work = false;
         if (!did_work) {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "No monitor specified, treat as  --display 1");
            did_work = create_dispno_display_identifier(1);   // default monitor
            temporary_did_work = true;
         }
         // assert(did_work);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Detecting displays...");
         ddc_ensure_displays_detected();
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "display detection complete");
         dref = get_display_ref_for_display_identifier(did_work, CALLOPT_NONE);
         if (temporary_did_work)
            free_display_identifier(did_work);
         if (!dref)
            f0printf(ferr(), "Display not found\n");
         final_result = (dref) ? DDCRC_OK : DDCRC_INVALID_DISPLAY;
      }
   }  // !DISP_ID_BUSNO

   *dref_loc = dref;
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, final_result,
                 "*dref_loc = %p -> %s",
                 *dref_loc,
                 dref_repr_t(*dref_loc) );
   return final_result;
}


/** Execute commands that either require a display or for which a display is optional.
 *  If a display is required, it has been opened and its display handle is passed
 *  as an argument.
 *
 *  \param parsed_cmd  parsed command line
 *  \param dh          display handle, if NULL no display was specified on the
 *                     command line and the command does not require a display
 *  \retval EXIT_SUCCESS
 *  \retval EXIT_FAILURE
 */
int
execute_cmd_with_optional_display_handle(
      Parsed_Cmd *     parsed_cmd,
      Display_Handle * dh)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh = %p -> %s", dh, dh_repr(dh));
   int main_rc = EXIT_SUCCESS;

   if (dh) {
      if (!vcp_version_eq(parsed_cmd->mccs_vspec, DDCA_VSPEC_UNKNOWN)) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Forcing mccs_vspec=%d.%d",
                            parsed_cmd->mccs_vspec.major, parsed_cmd->mccs_vspec.minor);
         dh->dref->vcp_version_cmdline = parsed_cmd->mccs_vspec;
      }
   }

   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "%s", cmdid_name(parsed_cmd->cmd_id));
   switch(parsed_cmd->cmd_id) {

   case CMDID_LOADVCP:
      {
         // loadvcp will search monitors to find the one matching the
         // identifiers in the record
         ddc_ensure_displays_detected();
         Status_Errno_DDC ddcrc = app_loadvcp_by_file(parsed_cmd->args[0], dh);
         main_rc = (ddcrc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
         break;
      }

   case CMDID_CAPABILITIES:
      {
         assert(dh);
         app_check_dynamic_features(dh->dref);
         ensure_vcp_version_set(dh);

         DDCA_Status ddcrc = app_capabilities(dh);
         main_rc = (ddcrc==0) ? EXIT_SUCCESS : EXIT_FAILURE;
         break;
      }

   case CMDID_GETVCP:
      {
         assert(dh);
         app_check_dynamic_features(dh->dref);
         ensure_vcp_version_set(dh);

         Public_Status_Code psc = app_show_feature_set_values_by_dh(dh, parsed_cmd);
         main_rc = (psc==0) ? EXIT_SUCCESS : EXIT_FAILURE;
      }
      break;

   case CMDID_SETVCP:
      {
         assert(dh);
         app_check_dynamic_features(dh->dref);
         ensure_vcp_version_set(dh);

         int rc = app_setvcp(parsed_cmd, dh);
         main_rc = (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
      }
      break;

   case CMDID_SAVE_SETTINGS:
      assert(dh);
      if (parsed_cmd->argct != 0) {
         f0printf(fout(), "SCS command takes no arguments\n");
         main_rc = EXIT_FAILURE;
      }
      else if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
         f0printf(fout(), "SCS command is not supported for USB devices\n");
         main_rc = EXIT_FAILURE;
      }
      else {
         main_rc = EXIT_SUCCESS;
         Error_Info * ddc_excp = ddc_save_current_settings(dh);
         if (ddc_excp)  {
            f0printf(fout(), "Save current settings failed. rc=%s\n", psc_desc(ddc_excp->status_code));
            if (ddc_excp->status_code == DDCRC_RETRIES)
               f0printf(fout(), "    Try errors: %s", errinfo_causes_string(ddc_excp) );
            errinfo_report(ddc_excp, 0);   // ** ALTERNATIVE **/
            errinfo_free(ddc_excp);
            // ERRINFO_FREE_WITH_REPORT(ddc_excp, report_exceptions);
            main_rc = EXIT_FAILURE;
         }
      }
      break;

   case CMDID_DUMPVCP:
      {
         assert(dh);
         // MCCS vspec can affect whether a feature is NC or TABLE
         app_check_dynamic_features(dh->dref);
         ensure_vcp_version_set(dh);

         Public_Status_Code psc =
               app_dumpvcp_as_file(dh, (parsed_cmd->argct > 0)
                                      ? parsed_cmd->args[0]
                                      : NULL );
         main_rc = (psc==0) ? EXIT_SUCCESS : EXIT_FAILURE;
         break;
      }

   case CMDID_READCHANGES:
      assert(dh);
      app_check_dynamic_features(dh->dref);
      ensure_vcp_version_set(dh);

      app_read_changes_forever(dh, parsed_cmd->flags & CMD_FLAG_X52_NO_FIFO);     // only returns if fatal error
      main_rc = EXIT_FAILURE;
      break;

   case CMDID_PROBE:
      assert(dh);
      app_check_dynamic_features(dh->dref);
      ensure_vcp_version_set(dh);

      app_probe_display_by_dh(dh);
      main_rc = EXIT_SUCCESS;
      break;

   default:
      main_rc = EXIT_FAILURE;
      break;
   }    // switch

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %s(%d)",
                                   (main_rc == 0) ? "EXIT_SUCCESS" : "EXIT_FAILURE",
                                   main_rc);
   return main_rc;
}


DDCA_Syslog_Level preparse_syslog_level(int argc, char** argv) {
   bool debug = false;
   DDCA_Syslog_Level result = DDCA_SYSLOG_NOT_SET;
   char * syslog_arg = NULL;
   int syslog_pos = ntsa_find(argv, "--syslog");
   if (syslog_pos >= 0 && syslog_pos < (argc-1))
      syslog_arg = argv[syslog_pos+1];
   else {
      syslog_pos = ntsa_findx(argv, "--syslog=", str_starts_with);
      if (syslog_pos >= 0)
         syslog_arg = argv[syslog_pos] + strlen("--syslog");
   }
   if (syslog_arg) {
      DBGMSF(debug, "Parsing initial log level: |%s|", syslog_arg);
      GPtrArray *sink = g_ptr_array_new();
      g_ptr_array_set_free_func(sink, g_free);
      DDCA_Syslog_Level parsed_level;
      bool ok_level = parse_syslog_level(syslog_arg, &parsed_level, sink);
      g_ptr_array_free(sink, true);
      if (ok_level)
         result = parsed_level;
   }
   DBGMSF(debug, "Returning: %s", syslog_level_name(result));
   return result;
}


//
// Mainline
//

/** **ddcutil** program mainline.
  *
  * @param argc   number of command line arguments
  * @param argv   pointer to array of argument strings
  *
  * @retval  EXIT_SUCCESS normal exit
  * @retval  EXIT_FAILURE an error occurred
  */
int
main(int argc, char *argv[]) {
   bool main_debug = false;
   char * s = getenv("DDCUTIL_DEBUG_MAIN");
   if (s && strlen(s) > 0)
      main_debug = true;

   int main_rc = EXIT_FAILURE;
   bool start_time_reported = false;

   bool explicit_syslog_level = false;
   syslog_level = DEFAULT_DDCUTIL_SYSLOG_LEVEL;
   bool syslog_opened = false;
   bool preparse_verbose = false;
   bool skip_config = false;
   Parsed_Cmd * parsed_cmd = NULL;

   time_t program_start_time = time(NULL);
   char * program_start_time_s = asctime(localtime(&program_start_time));
   if (program_start_time_s[strlen(program_start_time_s)-1] == 0x0a)
        program_start_time_s[strlen(program_start_time_s)-1] = 0;

   add_local_rtti_functions();      // add entries for this file
   init_base_services();            // so tracing related modules are initialized
   init_ddc_services();             // initializes i2c, usb, ddc, vcp, dynvcp
   init_app_ddcutil_services();
#ifdef ENABLE_ENVCMDS
   init_app_sysenv_services();
#endif
   DBGMSF(main_debug, "init_base_services() complete, ol = %s",
                      output_level_name(get_output_level()) );
   // dbgrpt_rtti_func_name_table(3);

   char ** new_argv = NULL;
   int     new_argc = 9;
   char *  untokenized_cmd_prefix = NULL;
   char *  configure_fn = NULL;

   DDCA_Syslog_Level preparsed_level = preparse_syslog_level(argc, argv);
   if (preparsed_level != DDCA_SYSLOG_NOT_SET) {
      DBGMSF(main_debug, "Setting syslog_level = %s", syslog_level_name(preparsed_level));
      syslog_level = preparsed_level;
      explicit_syslog_level = true;
   }
   DBGMSF(main_debug, "syslog_level=%s, explicit_syslog_level=%s",
                      syslog_level_name(syslog_level),  sbool(explicit_syslog_level));

   preparse_verbose = ntsa_find(argv, "--verbose") >= 0 || ntsa_find(argv, "-v") >= 0;

   skip_config = (ntsa_find(argv, "--noconfig") >= 0);
   if (skip_config) {
      // DBGMSG("Skipping config file");
      new_argv = ntsa_copy(argv, true);
      new_argc = argc;
   }
   else {
      GPtrArray * config_file_errs = g_ptr_array_new_with_free_func(g_free);
      int apply_config_rc = apply_config_file(
                       "ddcutil",     // use this section of config file
                       argc,
                       argv,
                       &new_argc,
                       &new_argv,
                       &untokenized_cmd_prefix,
                       &configure_fn,
                       config_file_errs);
      DBGMSF(main_debug, "apply_config_file() returned %s", psc_desc(apply_config_rc));
      DBGMSF(main_debug, "syslog_level=%s, explicit_syslog_level=%s",
                         syslog_level_name(syslog_level), SBOOL(explicit_syslog_level));
      if (config_file_errs->len > 0) {
         if (syslog_level > DDCA_SYSLOG_NEVER) {
            openlog("ddcutil",    // prepended to every log message
                     LOG_CONS |   // write to system console if error sending to system logger
                     LOG_PID,     // include caller's process id
                     LOG_USER);   // generic user program, syslogger can use to determine how to handle
            syslog_opened = true;
            DBGMSF(main_debug, "openlog() executed");
         }
         f0printf(ferr(), "Error(s) reading ddcutil configuration from file %s:\n", configure_fn);
         if (syslog_opened)
            syslog(LOG_ERR, "Error(s) reading ddcutil configuration from file %s:\n", configure_fn);
         for (int ndx = 0; ndx < config_file_errs->len; ndx++) {
            char * s = g_strdup_printf("   %s\n", (char *) g_ptr_array_index(config_file_errs, ndx));
            f0printf(ferr(), s);
            if (syslog_opened)
               syslog(LOG_ERR, "%s", s);
            free(s);
         }
      }

      g_ptr_array_free(config_file_errs, true);

      if (apply_config_rc < 0)
         goto bye;

      if (preparse_verbose) {
         if (untokenized_cmd_prefix && strlen(untokenized_cmd_prefix) > 0) {
            fprintf(fout(), "Applying ddcutil options from %s: %s\n", configure_fn, untokenized_cmd_prefix);
            if (syslog_opened)
               syslog(LOG_INFO, "Applying ddcutil options from %s: %s\n", configure_fn, untokenized_cmd_prefix);
         }
      }
   }

   assert(new_argc == ntsa_length(new_argv));

   if (main_debug) {
      DBGMSG("new_argc = %d, new_argv:", new_argc);
      rpt_ntsa(new_argv, 1);
   }

   parsed_cmd = parse_command(new_argc, new_argv, MODE_DDCUTIL, NULL);
   DBGMSF(main_debug, "parse_command() returned %p", parsed_cmd);
   ntsa_free(new_argv, true);
   if (!parsed_cmd)
      goto bye;      // main_rc == EXIT_FAILURE

   if (parsed_cmd->cmd_id == CMDID_CHKUSBMON) {
      // prevent io
      parsed_cmd->flags &= ~(CMD_FLAG_DSA2 | CMD_FLAG_ENABLE_CACHED_CAPABILITIES | CMD_FLAG_ENABLE_CACHED_DISPLAYS);
   }

   Error_Info * errs = init_tracing(parsed_cmd);
   if (errs) {
      for (int ndx = 0; ndx < errs->cause_ct; ndx++) {
         Error_Info * cur = errs->causes[ndx];
         fprintf(stderr, "%s\n", cur->detail);
         if (syslog_opened)
            syslog(LOG_ERR, "%s\n", cur->detail);
      }
      goto bye;
   }
   if (preparse_verbose)
      parsed_cmd->output_level = DDCA_OL_VERBOSE;

   if (explicit_syslog_level)
      parsed_cmd->syslog_level = explicit_syslog_level;

   if (parsed_cmd->syslog_level > DDCA_SYSLOG_NEVER && !syslog_opened) {
      if (parsed_cmd->syslog_level > DDCA_SYSLOG_NEVER ) {   // global
         openlog("ddcutil",          // prepended to every log message
                 LOG_CONS |          // write to system console if error sending to system logger
                 LOG_PID,            // include caller's process id
                 LOG_USER);          // generic user program, syslogger can use to determine how to handle
         syslog_opened = true;
         DBGMSF(main_debug, "openlog() executed");
      }
   }
   else if (parsed_cmd->syslog_level == DDCA_SYSLOG_NEVER && syslog_opened) {
      // oops
      DBGMSF(main_debug, "parsed_cmd=>syslog_level == DDCA_SYSLOG_NEVER, calling closelog()");
      closelog();
      syslog_opened = false;
   }

   // tracing is sufficiently initialized, can report start time
   start_time_reported = parsed_cmd->traced_groups    ||
                         parsed_cmd->traced_files     ||
                         IS_TRACING()                 ||
                         main_debug;
   DBGMSF(main_debug, "start_time_reported = %s", SBOOL(start_time_reported));
   DBGMSF(start_time_reported, "Starting %s execution, %s",
               parser_mode_name(parsed_cmd->parser_mode),
               program_start_time_s);

   SYSLOG2(DDCA_SYSLOG_NOTICE, "Starting.  ddcutil version %s", get_full_ddcutil_version());

   if (preparse_verbose) {
      if (untokenized_cmd_prefix && strlen(untokenized_cmd_prefix) > 0) {
         SYSLOG2(DDCA_SYSLOG_NOTICE,"Applying ddcutil options from %s: %s",   configure_fn, untokenized_cmd_prefix);
      }
   }

#ifdef UNUSED
#ifdef USE_X11
   if (!(parsed_cmd->flags&CMD_FLAG_F12)) {
      dpms_check_x11_asleep();
      if (dpms_state & DPMS_STATE_X11_ASLEEP) {
         // DBGMSF(true, "DPMS sleep mode is active. Terminating execution.");
         MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE, "DPMS sleep mode is active.  Terminating execution");
        goto bye;
      }
   }
#endif
#endif

   if (!master_initializer(parsed_cmd))
      goto bye;
   if (parsed_cmd->flags&CMD_FLAG_SHOW_SETTINGS)
      report_all_options(parsed_cmd, configure_fn, untokenized_cmd_prefix, 0);

   // xdg_tests(); // for development

   if (parsed_cmd->flags & CMD_FLAG_F2) {
      consolidated_i2c_sysfs_report(0);
      // rpt_label(0, "*** Tests Done ***");
      // rpt_nl();
   }

   Call_Options callopts = CALLOPT_NONE;
   i2c_forceable_slave_addr_flag = parsed_cmd->flags & CMD_FLAG_FORCE_SLAVE_ADDR;

#ifdef ENABLE_USB
   usb_ignore_hiddevs(parsed_cmd->ignored_hiddevs);
   Vid_Pid_Value * values = (parsed_cmd->ignored_usb_vid_pid_ct == 0) ? NULL : parsed_cmd->ignored_usb_vid_pids;
   usb_ignore_vid_pid_values(parsed_cmd->ignored_usb_vid_pid_ct, values);
#endif

   main_rc = EXIT_SUCCESS;     // from now on assume success;
   DBGTRC_NOPREFIX(main_debug, TRACE_GROUP, "Initialization complete, process commands");

   if (parsed_cmd->cmd_id == CMDID_LISTVCP) {    // vestigial
      app_listvcp(stdout);
      main_rc = EXIT_SUCCESS;
   }

   else if (parsed_cmd->cmd_id == CMDID_VCPINFO) {
      bool vcpinfo_ok = app_vcpinfo(parsed_cmd);
      main_rc = (vcpinfo_ok) ? EXIT_SUCCESS : EXIT_FAILURE;
   }

   else if (parsed_cmd->cmd_id == CMDID_LIST_RTTI) {
      report_rtti_func_name_table(0, "Functions traceable by name:");
      main_rc = EXIT_SUCCESS;
   }

   // else if (parsed_cmd->cmd_id == CMDID_DISCARD_CACHE) {
   //    i2c_discard_caches(parsed_cmd->discarded_cache_types);
   // }

   else if (parsed_cmd->cmd_id == CMDID_C1) {
      DBGMSG("Executing temporarily defined command C1");
      if (!drm_enabled) {
         DBGMSG("Requires DRM capable video drivers.");
         main_rc = EXIT_FAILURE;
      }
      else {
         ddc_ensure_displays_detected();
         DDCA_Display_Event_Class event_classes = DDCA_EVENT_CLASS_ALL;
         if (parsed_cmd->flags&CMD_FLAG_F13)
            event_classes = DDCA_EVENT_CLASS_DISPLAY_CONNECTION;
         if (parsed_cmd->flags&CMD_FLAG_F14)
            event_classes = DDCA_EVENT_CLASS_DPMS;
         Error_Info * erec = ddc_start_watch_displays(event_classes);
         if (erec) {
            DBGMSG(erec->detail);
            ERRINFO_FREE_WITH_REPORT(erec, true);
            main_rc = EXIT_FAILURE;
         }
         else {
            DBGMSG("Sleeping for 60 minutes");
            sleep(60*60);
            main_rc = EXIT_SUCCESS;
         }
      }
   }

   else if (parsed_cmd->cmd_id == CMDID_C2) {
      DBGMSG("Executing temporarily defined command C2: noop");
      main_rc = EXIT_SUCCESS;
   }

   else if (parsed_cmd->cmd_id == CMDID_C3 || parsed_cmd->cmd_id == CMDID_C4) {
      Cmd_Desc * desc = get_command(parsed_cmd->cmd_id);
      DBGMSG("Unrecognized command: %s", desc->cmd_name);
      main_rc = EXIT_FAILURE;
   }

#ifdef INCLUDE_TESTCASES
   else if (parsed_cmd->cmd_id == CMDID_LISTTESTS) {
      show_test_cases();
      main_rc = EXIT_SUCCESS;
   }
#endif

   // start of commands that actually access monitors

   else if (parsed_cmd->cmd_id == CMDID_DETECT) {
      DBGTRC_NOPREFIX(main_debug, TRACE_GROUP, "Detecting displays...");
      verify_i2c_access();

      if ( parsed_cmd->flags & CMD_FLAG_F4) {
         test_display_detection_variants();
      }
      else {     // normal case
         ddc_ensure_displays_detected();
         ddc_report_displays(/*include_invalid_displays=*/ true, 0);
      }
      DBGTRC_NOPREFIX(main_debug, TRACE_GROUP, "Display detection complete");
      main_rc = EXIT_SUCCESS;
   }

#ifdef INCLUDE_TESTCASES
   else if (parsed_cmd->cmd_id == CMDID_TESTCASE) {
      bool ok = app_testcases(parsed_cmd);
      main_rc = (ok) ? EXIT_SUCCESS : EXIT_FAILURE;
   }
#endif


#ifdef ENABLE_ENVCMDS
   else if (parsed_cmd->cmd_id == CMDID_ENVIRONMENT) {
      DBGTRC_NOPREFIX(main_debug, TRACE_GROUP, "Processing command ENVIRONMENT...");
      dup2(1,2);   // redirect stderr to stdout
      query_sysenv(parsed_cmd->flags & CMD_FLAG_QUICK);
      main_rc = EXIT_SUCCESS;
   }

   else if (parsed_cmd->cmd_id == CMDID_USBENV) {
#ifdef ENABLE_USB
      DBGTRC_NOPREFIX(main_debug, TRACE_GROUP, "Processing command USBENV...");
      dup2(1,2);   // redirect stderr to stdout
      query_usbenv();
      main_rc = EXIT_SUCCESS;
#else
      f0printf(fout(), "ddcutil was not built with support for USB connected monitors\n");
      main_rc = EXIT_FAILURE;
#endif
   }
#endif

   else if (parsed_cmd->cmd_id == CMDID_CHKUSBMON) {
#ifdef ENABLE_USB
      // DBGMSG("Processing command chkusbmon...\n");
      DBGTRC_NOPREFIX(main_debug, TRACE_GROUP, "Processing command CHKUSBMON...");
      bool is_monitor = check_usb_monitor( parsed_cmd->args[0] );
      main_rc = (is_monitor) ? EXIT_SUCCESS : EXIT_FAILURE;
#else
      main_rc = EXIT_FAILURE;
#endif
   }

#ifdef ENABLE_ENVCMDS
   else if (parsed_cmd->cmd_id == CMDID_INTERROGATE) {
      app_interrogate(parsed_cmd);
      main_rc = EXIT_SUCCESS;
   }
#endif

   // *** Commands that may require Display Identifier ***
   else {
      verify_i2c_access();
      Display_Ref * dref = NULL;
      Status_Errno_DDC  rc =
      find_dref(parsed_cmd,
               (parsed_cmd->cmd_id == CMDID_LOADVCP) ? DISPLAY_ID_OPTIONAL : DISPLAY_ID_REQUIRED,
               &dref);
      if (rc != DDCRC_OK) {
         main_rc = EXIT_FAILURE;
      }
      else {
         Display_Handle * dh = NULL;
         if (dref) {
            DBGMSF(main_debug,
                   "mainline - display detection complete, about to call ddc_open_display() for dref" );
            Error_Info* err = ddc_open_display(dref, callopts, &dh);
            ASSERT_IFF( !err, dh);
            if (!dh) {
               f0printf(ferr(), "Error opening %s: %s\n", dref_repr_t(dref), psc_name(err->status_code));
               errinfo_free(err);
               main_rc = EXIT_FAILURE;
            }
         }  // dref

         if (main_rc == EXIT_SUCCESS) {
            main_rc = execute_cmd_with_optional_display_handle(parsed_cmd, dh);
         }

         if (dh) {
            Error_Info * err = ddc_close_display(dh);
            if (err) {
               MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "%s: %s", err->detail, psc_desc(err->status_code));
               errinfo_free(err);
            }
         }
         if (dref && (dref->flags & DREF_TRANSIENT))
            free_display_ref(dref);
      }
   }
   DBGTRC(main_debug, DDCA_TRC_TOP, "After command processing");

   if (parsed_cmd->stats_types != DDCA_STATS_NONE
         && ( ddc_displays_already_detected() ||
              (parsed_cmd->pdid && parsed_cmd->pdid->id_type == DISP_ID_BUSNO)
            )
#ifdef ENABLE_ENVCMDS
         && parsed_cmd->cmd_id != CMDID_INTERROGATE
#endif
      )
   {
      ddc_report_stats_main(
            parsed_cmd->stats_types,
            parsed_cmd->flags & CMD_FLAG_VERBOSE_STATS,
            parsed_cmd->flags & CMD_FLAG_INTERNAL_STATS,
            parsed_cmd->flags & CMD_FLAG_STATS_TO_SYSLOG,
            0);
      // report_timestamp_history();  // debugging function
   }

bye:
   DBGTRC(main_debug, DDCA_TRC_TOP, "at label bye");
   free(untokenized_cmd_prefix);
   free(configure_fn);
   free_regex_hash_table();
   if (parsed_cmd && parsed_cmd->cmd_id != CMDID_CHKUSBMON && parsed_cmd->cmd_id != CMDID_DISCARD_CACHE) {
      if (dsa2_is_enabled())
         dsa2_save_persistent_stats();
      if (display_caching_enabled)
         ddc_store_displays_cache();
   }
   if (parsed_cmd)
      free_parsed_cmd(parsed_cmd);

   DBGTRC_DONE(main_debug, TRACE_GROUP, "main_rc=%d", main_rc);

   time_t end_time = time(NULL);
   char * end_time_s = asctime(localtime(&end_time));
   if (end_time_s[strlen(end_time_s)-1] == 0x0a)
      end_time_s[strlen(end_time_s)-1] = 0;
   DBGMSF(start_time_reported, "ddcutil execution complete, %s", end_time_s);

   DBGMSF(main_debug, "syslog_opened=%s", sbool(syslog_opened));
   if (syslog_opened) {
      SYSLOG2(DDCA_SYSLOG_NOTICE, "Terminating. Returning %d", main_rc);
      closelog();
   }

   terminate_ddc_services();
   terminate_base_services();

   return main_rc;
}


static void add_local_rtti_functions() {
   RTTI_ADD_FUNC(main);
   RTTI_ADD_FUNC(execute_cmd_with_optional_display_handle);
   RTTI_ADD_FUNC(find_dref);
   RTTI_ADD_FUNC(verify_i2c_access);
#ifdef UNUSED
#ifdef TARGET_LINUX
   RTTI_ADD_FUNC(validate_environment_using_libkmod);
#endif
#endif
}
