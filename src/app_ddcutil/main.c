/* main.c
 *
 * Program mainline
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

/** \file
 * ddcutil standalone application mainline
 */

/** \cond */
#include <config.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/data_structures.h"
#include "util/error_info.h"
#include "util/failsim.h"
#include "util/sysfs_util.h"
/** \endcond */

#include "base/adl_errors.h"
#include "base/base_init.h"
#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/linux_errno.h"
#include "base/parms.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"

#include "vcp/parse_capabilities.h"
#include "vcp/vcp_feature_codes.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_do_io.h"

#include "ddc/ddc_try_stats.h"

#include "adl/adl_shim.h"

#include "usb/usb_displays.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_services.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"
#include "cmdline/cmd_parser_aux.h"    // for parse_feature_id_or_subset(), should it be elsewhere?
#include "cmdline/cmd_parser.h"
#include "cmdline/parsed_cmd.h"

#include "app_ddcutil/app_dumpload.h"
#include "app_ddcutil/app_getvcp.h"
#include "app_ddcutil/app_setvcp.h"

#include "app_sysenv/query_sysenv.h"
#ifdef USE_USB
#include "app_sysenv/query_sysenv_usb.h"
#endif

#ifdef INCLUDE_TESTCASES
#include "test/testcases.h"
#endif

#ifdef USE_API
#include "public/ddcutil_c_api.h"
#endif


//
// Initialization and Statistics
//

// static long start_time_nanos;


static
void reset_stats() {
   ddc_reset_stats_main();
}



static
void report_stats(DDCA_Stats_Type stats) {
   ddc_report_stats_main(stats, 0);

   // Report the elapsed time in ddc_report_stats_main().
   // The start time used there is that at the time of stats initialization,
   // which is slightly later than start_time_nanos, but the difference is
   // less than a tenth of a millisecond.  Using that start time allows for
   // elapsed time to be used from library functions.

   // puts("");
   // long elapsed_nanos = cur_realtime_nanosec() - start_time_nanos;
   // printf("Elapsed milliseconds (nanoseconds):             %10ld  (%10ld)\n",
   //       elapsed_nanos / (1000*1000),
   //       elapsed_nanos);
}


// TODO: refactor
//       originally just displayed capabilities, now returns parsed capabilities as well
//       these actions should be separated
Parsed_Capabilities *
perform_get_capabilities_by_display_handle(Display_Handle * dh) {
   bool debug = false;
   Parsed_Capabilities * pcap = NULL;
   char * capabilities_string;
   Error_Info * ddc_excp = get_capabilities_string(dh, &capabilities_string);
   Public_Status_Code psc =  ERRINFO_STATUS(ddc_excp);
   assert( (ddc_excp && psc!=0) || (!ddc_excp && psc==0) );

   if (ddc_excp) {
      switch(psc) {
      case DDCRC_REPORTED_UNSUPPORTED:       // should not happen
      case DDCRC_DETERMINED_UNSUPPORTED:
         printf("Unsupported request\n");
         break;
      case DDCRC_RETRIES:
         f0printf(FOUT, "Unable to get capabilities for monitor on %s.  Maximum DDC retries exceeded.\n",
                 dh_repr(dh));
         break;
      default:
         f0printf(FOUT, "(%s) !!! Unable to get capabilities for monitor on %s\n",
                __func__, dh_repr(dh));
         DBGMSG("Unexpected status code: %s", psc_desc(psc));
      }
      // errinfo_free(ddc_excp);
      ERRINFO_FREE_WITH_REPORT(ddc_excp, debug || report_freed_exceptions);
   }
   else {
      assert(capabilities_string);
      // pcap is always set, but may be damaged if there was a parsing error
      pcap = parse_capabilities_string(capabilities_string);
      DDCA_Output_Level output_level = get_output_level();
      if (output_level <= DDCA_OL_TERSE) {
         f0printf(FOUT,
                  "%s capabilities string: %s\n",
                  (dh->dref->io_mode == DDCA_IO_USB) ? "Synthesized unparsed" : "Unparsed",
                  capabilities_string);
      }
      else {
         if (dh->dref->io_mode == DDCA_IO_USB)
            pcap->raw_value_synthesized = true;
         // report_parsed_capabilities(pcap, dh->dref->io_mode);    // io_mode no longer needed
         report_parsed_capabilities(pcap);
         // free_parsed_capabilities(pcap);
      }
   }
   DBGMSF(debug, "Returning: %p", pcap);
   return pcap;
}


void probe_display_by_dh(Display_Handle * dh)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dh=%s", dh_repr(dh));
   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;
   char dref_name_buf[DREF_SHORT_NAME_BUF_SIZE];
   dref_short_name_r(dh->dref, dref_name_buf, sizeof(dref_name_buf));

   f0printf(FOUT,
            "\nMfg id: %s, model: %s, sn: %s\n",
            dh->dref->pedid->mfg_id, dh->dref->pedid->model_name, dh->dref->pedid->serial_ascii);

   // printf("\nCapabilities for display %s\n", display_handle_repr(dh) );
   f0printf(FOUT, "\nCapabilities for display on %s\n", dref_name_buf);

   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   // not needed, message causes confusing messages if get_vcp_version fails but get_capabilities succeeds
   // if (vspec.major < 2) {
   //    printf("VCP (aka MCCS) version for display is less than 2.0. Output may not be accurate.\n");
   // }

   // reports capabilities, and if successful returns Parsed_Capabilities
   Parsed_Capabilities * pcaps = perform_get_capabilities_by_display_handle(dh);

   // how to pass this information down into app_show_vcp_subset_values_by_display_handle()?
   bool table_reads_possible = parsed_capabilities_may_support_table_commands(pcaps);
   f0printf(FOUT, "\nMay support table reads:   %s\n", bool_repr(table_reads_possible));

   // *** VCP Feature Scan ***
   // printf("\n\nScanning all VCP feature codes for display %d\n", dispno);
   f0printf(FOUT, "\nScanning all VCP feature codes for display %s\n", dh_repr(dh) );
   Byte_Bit_Flags features_seen = bbf_create();
   app_show_vcp_subset_values_by_display_handle(
         dh, VCP_SUBSET_SCAN, /* show_unsupported */ true, features_seen);

   if (pcaps) {
      f0printf(FOUT, "\n\nComparing declared capabilities to observed features...\n");
      Byte_Bit_Flags features_declared =
            parsed_capabilities_feature_ids(pcaps, /*readable_only=*/true);
      char * s0 = bbf_to_string(features_declared, NULL, 0);
      f0printf(FOUT, "\nReadable features declared in capabilities string: %s\n", s0);
      free(s0);

      Byte_Bit_Flags caps_not_seen = bbf_subtract(features_declared, features_seen);
      Byte_Bit_Flags seen_not_caps = bbf_subtract(features_seen, features_declared);

      f0printf(FOUT, "\nMCCS (VCP) version reported by capabilities: %s\n",
               format_vspec(pcaps->parsed_mccs_version));
      f0printf(FOUT, "MCCS (VCP) version reported by feature 0xDf: %s\n",
               format_vspec(vspec));
      if (!vcp_version_eq(pcaps->parsed_mccs_version, vspec))
         f0printf(FOUT, "Versions do not match!!!\n");

      if (bbf_count_set(caps_not_seen) > 0) {
         f0printf(FOUT, "\nFeatures declared as readable capabilities but not found by scanning:\n");
         for (int code = 0; code < 256; code++) {
            if (bbf_is_set(caps_not_seen, code)) {
               VCP_Feature_Table_Entry * vfte = vcp_find_feature_by_hexid_w_default(code);
               char * feature_name = get_version_sensitive_feature_name(vfte, pcaps->parsed_mccs_version);
               f0printf(FOUT, "   Feature x%02x - %s\n", code, feature_name);
            }
         }
      }
      else
         f0printf(FOUT, "\nAll readable features declared in capabilities were found by scanning.\n");

      if (bbf_count_set(seen_not_caps) > 0) {
         f0printf(FOUT, "\nFeatures found by scanning but not declared as capabilities:\n");
         for (int code = 0; code < 256; code++) {
            if (bbf_is_set(seen_not_caps, code)) {
               VCP_Feature_Table_Entry * vfte = vcp_find_feature_by_hexid_w_default(code);
               char * feature_name = get_version_sensitive_feature_name(vfte, vspec);
               f0printf(FOUT, "   Feature x%02x - %s\n", code, feature_name);
            }
         }
      }
      else
         f0printf(FOUT, "\nAll features found by scanning were declared in capabilities.\n");

      bbf_free(features_declared);
      bbf_free(caps_not_seen);
      bbf_free(seen_not_caps);
      free_parsed_capabilities(pcaps);
   }
   else {
      f0printf(FOUT, "\n\nUnable to read or parse capabilities.\n");
      f0printf(FOUT, "Skipping comparison of declared capabilities to observed features\n");
   }
   bbf_free(features_seen);


   puts("");
   // get VCP 0B
   DDCA_Single_Vcp_Value * valrec;
   int color_temp_increment = 0;
   int color_temp_units = 0;
   ddc_excp =  get_vcp_value(
            dh,
          0x0b,              // color temperature increment,
          DDCA_NON_TABLE_VCP_VALUE,
          &valrec);
   psc = ERRINFO_STATUS(ddc_excp);
   if (psc == 0) {
      if (debug)
         f0printf(FOUT, "Value returned for feature x0b: %s\n", summarize_single_vcp_value(valrec) );
      color_temp_increment = valrec->val.c.cur_val;

      ddc_excp =  get_vcp_value(
            dh,
          0x0c,              // color temperature request
          DDCA_NON_TABLE_VCP_VALUE,
          &valrec);
      psc = ERRINFO_STATUS(ddc_excp);
      if (psc == 0) {
         if (debug)
            f0printf(FOUT, "Value returned for feature x0c: %s\n", summarize_single_vcp_value(valrec) );
         color_temp_units = valrec->val.c.cur_val;
         int color_temp = 3000 + color_temp_units * color_temp_increment;
         f0printf(FOUT, "Color temperature increment (x0b) = %d degrees Kelvin\n", color_temp_increment);
         f0printf(FOUT, "Color temperature request   (x0c) = %d\n", color_temp_units);
         f0printf(FOUT, "Requested color temperature = (3000 deg Kelvin) + %d * (%d degrees Kelvin)"
               " = %d degrees Kelvin\n",
               color_temp_units,
               color_temp_increment,
               color_temp);
      }
   }
   if (psc != 0) {
      f0printf(FOUT, "Unable to calculate color temperature from VCP features x0B and x0C\n");
      // errinfo_free(ddc_excp);
      ERRINFO_FREE_WITH_REPORT(ddc_excp, debug || report_freed_exceptions);
   }
   // get VCP 14
   // report color preset

   DBGMSF(debug, "Done.");
}


void probe_display_by_dref(Display_Ref * dref) {
   Display_Handle * dh = NULL;
   Public_Status_Code psc = ddc_open_display(dref, CALLOPT_ERR_MSG, &dh);
   if (psc != 0) {
      char buf[DREF_SHORT_NAME_BUF_SIZE];
      f0printf(FOUT, "Unable to open display %s, status code %s",
             dref_short_name_r(dref, buf, sizeof(buf)), psc_desc(psc) );
   }
   else {
      probe_display_by_dh(dh);
      ddc_close_display(dh);
   }
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
int main(int argc, char *argv[]) {

#ifdef OBSOLETE
   // For aborting out of shared library
   jmp_buf abort_buf;
   int jmprc = setjmp(abort_buf);
   if (jmprc) {
      fprintf(stderr, "Aborting. Internal status code = %d\n", jmprc);
      exit(EXIT_FAILURE);
   }

   register_jmp_buf(&abort_buf);
#endif

   // set_trace_levels(TRC_ADL);   // uncomment to enable tracing during initialization
   init_base_services();  // so tracing related modules are initialized
   Parsed_Cmd * parsed_cmd = parse_command(argc, argv);
   if (!parsed_cmd) {
      exit(EXIT_FAILURE);
   }
   if (parsed_cmd->timestamp_trace)         // timestamps on debug and trace messages?
      dbgtrc_show_time = true;              // extern in core.h
   report_freed_exceptions = parsed_cmd->report_freed_exceptions;   // extern in core.h
   set_trace_levels(parsed_cmd->trace);
   if (parsed_cmd->traced_functions) {
      for (int ndx = 0; ndx < ntsa_length(parsed_cmd->traced_functions); ndx++)
         add_traced_function(parsed_cmd->traced_functions[ndx]);
   }
   if (parsed_cmd->traced_files) {
      for (int ndx = 0; ndx < ntsa_length(parsed_cmd->traced_files); ndx++)
         add_traced_file(parsed_cmd->traced_files[ndx]);
   }
#ifdef ENABLE_FAILSIM
   fsim_set_name_to_number_funcs(
         status_name_to_modulated_number,
         status_name_to_unmodulated_number);
   if (parsed_cmd->failsim_control_fn) {
      bool ok = fsim_load_control_file(parsed_cmd->failsim_control_fn);
      if (!ok) {
         fprintf(stderr, "Error loading failure simulation control file %s.\n",
                         parsed_cmd->failsim_control_fn);
         exit(EXIT_FAILURE);
      }
      fsim_report_error_table(0);
   }
#endif

   init_ddc_services();  // n. initializes start timestamp
   // overrides setting in init_ddc_services():
   i2c_set_io_strategy(DEFAULT_I2C_IO_STRATEGY);

   set_verify_setvcp(parsed_cmd->verify_setvcp);

#ifndef HAVE_ADL
   if ( is_module_loaded_using_sysfs("fglrx") ) {
      fprintf(stdout, "WARNING: AMD proprietary video driver fglrx is loaded,");
      fprintf(stdout, "but this copy of ddcutil was built without fglrx support.");
   }
#endif

   int main_rc = EXIT_FAILURE;

   Call_Options callopts = CALLOPT_NONE;
   i2c_force_slave_addr_flag = parsed_cmd->force_slave_addr;
   if (parsed_cmd->force)
      callopts |= CALLOPT_FORCE;

   set_output_level(parsed_cmd->output_level);
   report_ddc_errors = parsed_cmd->ddcdata;
   // TMI:
   // if (show_recoverable_errors)
   //    parsed_cmd->stats = true;

   if (parsed_cmd->output_level >= DDCA_OL_VERBOSE) {
      show_reporting();
      f0printf( FOUT, "%.*s%-*s%s\n",
                0,"",
                28, "Force I2C slave address:",
                bool_repr(i2c_force_slave_addr_flag));
      f0puts("\n", FOUT);
   }

   // n. MAX_MAX_TRIES checked during command line parsing
   if (parsed_cmd->max_tries[0] > 0) {
#ifdef USE_API
      ddca_set_max_tries(DDCA_WRITE_ONLY_TRIES, parsed_cmd->max_tries[0]);
#else
      ddc_set_max_write_only_exchange_tries(parsed_cmd->max_tries[0]);
#endif
   }

   if (parsed_cmd->max_tries[1] > 0) {
#ifdef USE_API
      ddca_set_max_tries(DDCA_WRITE_READ_TRIES, parsed_cmd->max_tries[1]);
#else
      ddc_set_max_write_read_exchange_tries(parsed_cmd->max_tries[1]);
#endif
   }

   if (parsed_cmd->max_tries[2] > 0) {
#ifdef USE_API
      ddca_set_max_tries(DDCA_MULTI_PART_TRIES, parsed_cmd->max_tries[2]);
#else
      ddc_set_max_multi_part_read_tries(parsed_cmd->max_tries[2]);
      ddc_set_max_multi_part_write_tries(parsed_cmd->max_tries[2]);
#endif
   }

   if (parsed_cmd->sleep_strategy >= 0)
      set_sleep_strategy(parsed_cmd->sleep_strategy);

   int threshold = DISPLAY_CHECK_ASYNC_NEVER;
   if (parsed_cmd->async)
      threshold = DISPLAY_CHECK_ASYNC_THRESHOLD;
   ddc_set_async_threshold(threshold);

   if (parsed_cmd->cmd_id == CMDID_LISTVCP) {
      vcp_list_feature_codes(stdout);
      main_rc = EXIT_SUCCESS;
   }

   else if (parsed_cmd->cmd_id == CMDID_VCPINFO) {
      bool ok = true;

      DDCA_MCCS_Version_Spec vcp_version_any = {0,0};
      VCP_Feature_Set fset = create_feature_set_from_feature_set_ref(
         // &feature_set_ref,
         parsed_cmd->fref,
         vcp_version_any,
         false);       // force
      if (!fset) {
         ok = false;
      }
      else {
         if (parsed_cmd->output_level <= DDCA_OL_TERSE)
            report_feature_set(fset, 0);
         else {
            int ct =  get_feature_set_size(fset);
            int ndx = 0;
            for (;ndx < ct; ndx++) {
               VCP_Feature_Table_Entry * pentry = get_feature_set_entry(fset, ndx);
               report_vcp_feature_table_entry(pentry, 0);
            }
         }
      }

      if (ok) {
         main_rc = EXIT_SUCCESS;
      }
      else {
         // printf("Unrecognized VCP feature code or group: %s\n", val);
         main_rc = EXIT_FAILURE;
      }
   }

#ifdef INCLUDE_TESTCASES
   else if (parsed_cmd->cmd_id == CMDID_LISTTESTS) {
      show_test_cases();
      main_rc = EXIT_SUCCESS;
   }
#endif

   // start of commands that actually access monitors

   else if (parsed_cmd->cmd_id == CMDID_DETECT) {
      ddc_ensure_displays_detected();
      ddc_report_displays(DDC_REPORT_ALL_DISPLAYS, 0);

   }

#ifdef INCLUDE_TESTCASES
   else if (parsed_cmd->cmd_id == CMDID_TESTCASE) {
      int testnum;
      bool ok = true;
      int ct = sscanf(parsed_cmd->args[0], "%d", &testnum);
      if (ct != 1) {
         f0printf(FOUT, "Invalid test number: %s\n", parsed_cmd->args[0]);
         ok = false;
      }
      else {
         ddc_ensure_displays_detected();

         if (!parsed_cmd->pdid)
            parsed_cmd->pdid = create_dispno_display_identifier(1);   // default monitor
         ok = execute_testcase(testnum, parsed_cmd->pdid);
      }
      main_rc = (ok) ? EXIT_SUCCESS : EXIT_FAILURE;
   }
#endif

   else if (parsed_cmd->cmd_id == CMDID_LOADVCP) {
      ddc_ensure_displays_detected();

      char * fn = strdup( parsed_cmd->args[0] );
      // DBGMSG("Processing command loadvcp.  fn=%s", fn );
      Display_Handle * dh   = NULL;
      bool ok = true;
      if (parsed_cmd->pdid) {
         Display_Ref * dref = get_display_ref_for_display_identifier(
                                 parsed_cmd->pdid, callopts | CALLOPT_ERR_MSG);
         if (!dref)
            ok = false;
         else {
            ddc_open_display(dref, callopts | CALLOPT_ERR_MSG, &dh);  // rc == 0 iff dh, removed CALLOPT_ERR_ABORT
            if (!dh)
               ok = false;
         }
      }
      if (ok)
         ok = loadvcp_by_file(fn, dh);

      // if we opened the display, we close it
      if (dh)
         ddc_close_display(dh);
      free(fn);
      main_rc = (ok) ? EXIT_SUCCESS : EXIT_FAILURE;
   }

   else if (parsed_cmd->cmd_id == CMDID_ENVIRONMENT) {
      dup2(1,2);   // redirect stderr to stdout
      ddc_ensure_displays_detected();   // *** NEEDED HERE ??? ***

      f0printf(FOUT, "The following tests probe the runtime environment using multiple overlapping methods.\n");
      query_sysenv();
      main_rc = EXIT_SUCCESS;
   }

   else if (parsed_cmd->cmd_id == CMDID_USBENV) {
#ifdef USE_USB
      dup2(1,2);   // redirect stderr to stdout
      ddc_ensure_displays_detected();   // *** NEEDED HERE ??? ***
      f0printf(FOUT, "The following tests probe for USB connected monitors.\n");
      // DBGMSG("Exploring USB runtime environment...\n");
      query_usbenv();
      main_rc = EXIT_SUCCESS;
#else
      f0printf(FOUT, "ddcutil was not built with support for USB connected monitors\n");
      main_rc = EXIT_FAILURE;
#endif
   }

   else if (parsed_cmd->cmd_id == CMDID_CHKUSBMON) {
#ifdef USE_USB
      // DBGMSG("Processing command chkusbmon...\n");
      bool is_monitor = check_usb_monitor( parsed_cmd->args[0] );
      main_rc = (is_monitor) ? EXIT_SUCCESS : EXIT_FAILURE;
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
   }

   else if (parsed_cmd->cmd_id == CMDID_INTERROGATE) {
      dup2(1,2);   // redirect stderr to stdout
      // set_ferr(FOUT);    // ensure that all messages are collected - made unnecessary by dup2()
      f0printf(FOUT, "Setting output level verbose...\n");
      set_output_level(DDCA_OL_VERBOSE);
      f0printf(FOUT, "Setting maximum retries...\n");
      f0printf(FOUT, "Forcing --stats...\n");
      parsed_cmd->stats_types = DDCA_STATS_ALL;
      f0printf(FOUT, "Forcing --force-slave-address..\n");
      i2c_force_slave_addr_flag = true;
      f0printf(FOUT, "This command will take a while to run...\n\n");
      ddc_set_max_write_read_exchange_tries(MAX_MAX_TRIES);
      ddc_set_max_multi_part_read_tries(MAX_MAX_TRIES);

      ddc_ensure_displays_detected();    // *** ???

      query_sysenv();
#ifdef USE_USB
      // 7/2017: disable, USB attached monitors are rare, and this just
      // clutters the output
      f0printf(FOUT, "\nSkipping USB environment exploration.\n");
      f0printf(FOUT, "Issue command \"ddcutil usbenvironment --verbose\" if there are any USB attached monitors.\n");
      // query_usbenv();
#endif
      f0printf(FOUT, "\nStatistics for environment exploration:\n");
      report_stats(DDCA_STATS_ALL);
      reset_stats();

      f0printf(FOUT, "\n*** Detected Displays ***\n");
      /* int display_ct =  */ ddc_report_displays(DDC_REPORT_ALL_DISPLAYS, 0 /* logical depth */);
      // printf("Detected: %d displays\n", display_ct);   // not needed
      f0printf(FOUT, "\nStatistics for display detection:\n");
      report_stats(DDCA_STATS_ALL);
      reset_stats();

      f0printf(FOUT, "Setting output level normal  Table features will be skipped...\n");
      set_output_level(DDCA_OL_NORMAL);

      GPtrArray * all_displays = ddc_get_all_displays();
      for (int ndx=0; ndx < all_displays->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
         assert( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
         if (dref->dispno < 0) {
            f0printf(FOUT, "\nSkipping invalid display on %s\n", dref_short_name_t(dref));
         }
         else {
            f0printf(FOUT, "\nProbing display %d\n", dref->dispno);
            probe_display_by_dref(dref);
            f0printf(FOUT, "\nStatistics for probe of display %d:\n", dref->dispno);
            report_stats(DDCA_STATS_ALL);
         }
         reset_stats();
      }
      f0printf(FOUT, "\nDisplay scanning complete.\n");

      main_rc = EXIT_SUCCESS;
   }

   // *** Commands that require Display Identifier ***
   else {
      if (!parsed_cmd->pdid)
         parsed_cmd->pdid = create_dispno_display_identifier(1);   // default monitor
      // assert(parsed_cmd->pdid);
      // returns NULL if not a valid display:
      Call_Options callopts = CALLOPT_ERR_MSG;        // emit error messages
      if (parsed_cmd->force)
         callopts |= CALLOPT_FORCE;

      // If --nodetect and --bus options were specified,skip scan for all devices.
      // --nodetect option not needed, just do it
      // n. useful even if not much speed up, since avoids cluttering stats
      // with all the failures during detect
      Display_Ref * dref = NULL;
      if (parsed_cmd->pdid->id_type == DISP_ID_BUSNO && parsed_cmd->nodetect) {
  //  if (parsed_cmd->pdid->id_type == DISP_ID_BUSNO) {
         int busno = parsed_cmd->pdid->busno;
         // is this really a monitor?
         I2C_Bus_Info * businfo = detect_single_bus(busno);
         if ( businfo && (businfo->flags & I2C_BUS_ADDR_0X50) ) {
            dref = create_bus_display_ref(busno);
            dref->dispno = -1;     // should use some other value for unassigned vs invalid
            dref->pedid = businfo->edid;    // needed?
            // dref->pedid = i2c_get_parsed_edid_by_busno(parsed_cmd->pdid->busno);
            dref->detail2 = businfo;
            dref->flags |= DREF_DDC_IS_MONITOR_CHECKED;
            dref->flags |= DREF_DDC_IS_MONITOR;
            dref->flags |= DREF_TRANSIENT;
            if (!initial_checks_by_dref(dref)) {
               f0printf(FOUT, "DDC communication failed for monitor on I2C bus /dev/i2c-%d\n", busno);
               free_display_ref(dref);
               dref = NULL;
            }
            // DBGMSG("Synthetic Display_Ref");
         }
         else {
            f0printf(FOUT, "No monitor detected on I2C bus /dev/i2c-%d\n", busno);
         }
      }
      else {
         ddc_ensure_displays_detected();
         dref = get_display_ref_for_display_identifier(parsed_cmd->pdid, callopts);
      }

      if (dref) {
         Display_Handle * dh = NULL;
         callopts |=  CALLOPT_ERR_MSG;    // removed CALLOPT_ERR_ABORT
         ddc_open_display(dref, callopts, &dh);

         if (dh) {
            if (// parsed_cmd->cmd_id == CMDID_CAPABILITIES ||
                parsed_cmd->cmd_id == CMDID_GETVCP       ||
                parsed_cmd->cmd_id == CMDID_READCHANGES
               )
            {
               DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
               if (vspec.major < 2 && get_output_level() >= DDCA_OL_NORMAL) {
                  f0printf(FOUT, "VCP (aka MCCS) version for display is undetected or less than 2.0. "
                        "Output may not be accurate.\n");
               }
            }

            switch(parsed_cmd->cmd_id) {

            case CMDID_CAPABILITIES:
               {
                  Parsed_Capabilities * pcaps = perform_get_capabilities_by_display_handle(dh);
                  main_rc = (pcaps) ? EXIT_SUCCESS : EXIT_FAILURE;
                  free(pcaps);
                  break;
               }

            case CMDID_GETVCP:
               {
                  Public_Status_Code psc = app_show_feature_set_values_by_display_handle(
                        dh,
                        parsed_cmd->fref,
                        parsed_cmd->show_unsupported,
                        parsed_cmd->force);
                  main_rc = (psc==0) ? EXIT_SUCCESS : EXIT_FAILURE;
               }
               break;

            case CMDID_SETVCP:
               if (parsed_cmd->argct % 2 != 0) {
                  f0printf(FOUT, "SETVCP command requires even number of arguments\n");
                  main_rc = EXIT_FAILURE;
               }
               else {
                  main_rc = EXIT_SUCCESS;
                  int argNdx;
                  // Public_Status_Code rc = 0;
                  Error_Info * ddc_excp;
                  for (argNdx=0; argNdx < parsed_cmd->argct; argNdx+= 2) {
                     ddc_excp = app_set_vcp_value(
                             dh,
                             parsed_cmd->args[argNdx],
                             parsed_cmd->args[argNdx+1],
                             parsed_cmd->force);
                     // rc =  ERRINFO_STATUS(ddc_excp);
                     if (ddc_excp) {
                        // errinfo_free(ddc_excp);
                        ERRINFO_FREE_WITH_REPORT(ddc_excp, report_freed_exceptions);
                        main_rc = EXIT_FAILURE;   // ???
                        break;
                     }
                  }
               }
               break;

            case CMDID_SAVE_SETTINGS:
               if (parsed_cmd->argct != 0) {
                  f0printf(FOUT, "SCS command takes no arguments\n");
                  main_rc = EXIT_FAILURE;
               }
               else if (dh->dref->io_mode == DDCA_IO_USB) {
                  f0printf(FOUT, "SCS command not supported for USB devices\n");
                  main_rc = EXIT_FAILURE;
               }
               else {
                  main_rc = EXIT_SUCCESS;
                  Error_Info * ddc_excp = save_current_settings(dh);
                  if (ddc_excp)  {
                     f0printf(FOUT, "Save current settings failed. rc=%s\n", psc_desc(ddc_excp->status_code));
                     if (ddc_excp->status_code == DDCRC_RETRIES)
                        f0printf(FOUT, "    Try errors: %s", errinfo_causes_string(ddc_excp) );
                     errinfo_report(ddc_excp, 0);   // ** ALTERNATIVE **/
                     errinfo_free(ddc_excp);
                     // ERRINFO_FREE_WITH_REPORT(ddc_excp, report_exceptions);
                     main_rc = EXIT_FAILURE;
                  }
               }
               break;

            case CMDID_DUMPVCP:
               {
                  Public_Status_Code psc =
                        dumpvcp_as_file(dh, (parsed_cmd->argct > 0)
                                               ? parsed_cmd->args[0]
                                               : NULL );
                  main_rc = (psc==0) ? EXIT_SUCCESS : EXIT_FAILURE;
                  break;
               }

            case CMDID_READCHANGES:
               // DBGMSG("Case CMDID_READCHANGES");
               // report_parsed_cmd(parsed_cmd,0);
               app_read_changes_forever(dh);
               break;

            case CMDID_PROBE:
               probe_display_by_dh(dh);
               break;

            default:
              break;
            }

            ddc_close_display(dh);
         }
         if (dref->flags & DREF_TRANSIENT)
            free_display_ref(dref);
      }
   }

   if (parsed_cmd->stats_types != DDCA_STATS_NONE && parsed_cmd->cmd_id != CMDID_INTERROGATE) {
      report_stats(parsed_cmd->stats_types);
      // report_timestamp_history();  // debugging function
   }
   // DBGMSG("Done");

   return main_rc;
}
