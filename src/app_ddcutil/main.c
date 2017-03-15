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

#include <config.h>

/** \cond */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */


#include "util/data_structures.h"
#include "util/failsim.h"

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
#include "ddc/try_stats.h"

#include "cmdline/cmd_parser_aux.h"    // for parse_feature_id_or_subset(), should it be elsewhere?
#include "cmdline/cmd_parser.h"
#include "cmdline/parsed_cmd.h"

#include "app_ddcutil/app_dumpload.h"
#include "app_ddcutil/app_getvcp.h"
#include "app_ddcutil/app_setvcp.h"
#include "app_ddcutil/query_sysenv.h"
#ifdef USE_USB
#include "app_ddcutil/query_usb_sysenv.h"
#endif
#include "app_ddcutil/testcases.h"


//
// Initialization and Statistics
//

static long start_time_nanos;




void report_stats(Stats_Type stats) {
   if (stats & STATS_TRIES) {
      puts("");
      // retry related stats
      ddc_show_max_tries(stdout);
      ddc_report_write_only_stats();
      ddc_report_write_read_stats();
      ddc_report_multi_part_read_stats();
   }
   if (stats & STATS_ERRORS) {
      puts("");
      show_all_status_counts();   // error code counts
   }
   if (stats & STATS_CALLS) {
      puts("");
      report_sleep_strategy_stats(0);
      puts("");
      report_io_call_stats(0);
      puts("");
      report_sleep_stats(0);
   }

   puts("");
   long elapsed_nanos = cur_realtime_nanosec() - start_time_nanos;
   printf("Elapsed milliseconds (nanoseconds):             %10ld  (%10ld)\n",
         elapsed_nanos / (1000*1000),
         elapsed_nanos);
}


// TODO: refactor
//       originally just displayed capabilities, now returns parsed capabilities as weel
//       these actions should be separated
Parsed_Capabilities * perform_get_capabilities_by_display_handle(Display_Handle * dh) {
   bool debug = false;
   Parsed_Capabilities * pcap = NULL;
   char * capabilities_string;
   int rc = get_capabilities_string(dh, &capabilities_string);

   if (rc < 0) {
      switch(rc) {
      case DDCRC_REPORTED_UNSUPPORTED:       // should not happen
      case DDCRC_DETERMINED_UNSUPPORTED:
         printf("Unsupported request\n");
         break;
      case DDCRC_RETRIES:
         printf("Unable to get capabilities for monitor on %s.  Maximum DDC retries exceeded.\n",
                 display_handle_repr(dh));
         break;
      default:
         printf("(%s) !!! Unable to get capabilities for monitor on %s\n",
                __func__, display_handle_repr(dh));
         DBGMSG("Unexpected status code: %s", gsc_desc(rc));
      }
   }
   else {
      assert(capabilities_string);
      // pcap is always set, but may be damaged if there was a parsing error
      pcap = parse_capabilities_string(capabilities_string);
      DDCA_Output_Level output_level = get_output_level();
      if (output_level <= OL_TERSE) {
         printf("%s capabilities string: %s\n",
               (dh->io_mode == USB_IO) ? "Synthesized unparsed" : "Unparsed",
               capabilities_string);
      }
      else {
         if (dh->io_mode == USB_IO)
            pcap->raw_value_synthesized = true;
         // report_parsed_capabilities(pcap, dh->io_mode);    // io_mode no longer needed
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
   DBGMSF(debug, "Starting. dh=%s", display_handle_repr(dh));
   Global_Status_Code gsc = 0;

   printf("\nCapabilities for display %s\n", display_handle_repr(dh) );
      // not needed, causes confusing messages if get_vcp_version fails but get_capabilities succeeds
      DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
      // if (vspec.major < 2) {
      //    printf("VCP (aka MCCS) version for display is less than 2.0. Output may not be accurate.\n");
      // }
      // reports capabilities, and if successful returns Parsed_Capabilities
      Parsed_Capabilities * pcaps = perform_get_capabilities_by_display_handle(dh);

      // how to pass this information down into app_show_vcp_subset_values_by_display_handle()?
      bool table_reads_possible = parsed_capabilities_may_support_table_commands(pcaps);
      printf("\nMay support table reads:   %s\n", bool_repr(table_reads_possible));


      // *** VCP Feature Scan ***
      // printf("\n\nScanning all VCP feature codes for display %d\n", dispno);
      printf("\n\nScanning all VCP feature codes for display %s\n", display_handle_repr(dh) );
      Byte_Bit_Flags features_seen = bbf_create();
      app_show_vcp_subset_values_by_display_handle(
            dh, VCP_SUBSET_SCAN, /* show_unsupported */ true, features_seen);

      if (pcaps) {
         printf("\n\nComparing declared capabilities to observed features...\n");
         Byte_Bit_Flags features_declared =
               parsed_capabilities_feature_ids(pcaps, /*readable_only=*/true);
         char * s0 = bbf_to_string(features_declared, NULL, 0);
         printf("\nReadable features declared in capabilities string: %s\n", s0);
         free(s0);

         Byte_Bit_Flags caps_not_seen = bbf_subtract(features_declared, features_seen);
         Byte_Bit_Flags seen_not_caps = bbf_subtract(features_seen, features_declared);

         printf("\nMCCS (VCP) version reported by capabilities: %s\n",
                  format_vspec(pcaps->parsed_mccs_version));
         printf("MCCS (VCP) version reported by feature 0xDf: %s\n",
                  format_vspec(vspec));
         if (!vcp_version_eq(pcaps->parsed_mccs_version, vspec))
            printf("Versions do not match!!!\n");

         if (bbf_count_set(caps_not_seen) > 0) {
            printf("\nFeatures declared as readable capabilities but not found by scanning:\n");
            for (int code = 0; code < 256; code++) {
               if (bbf_is_set(caps_not_seen, code)) {
                  VCP_Feature_Table_Entry * vfte = vcp_find_feature_by_hexid_w_default(code);
                  char * feature_name = get_version_sensitive_feature_name(vfte, pcaps->parsed_mccs_version);
                  printf("   Feature x%02x - %s\n", code, feature_name);
               }
            }
         }
         else
            printf("\nAll readable features declared in capabilities were found by scanning.\n");

         if (bbf_count_set(seen_not_caps) > 0) {
            printf("\nFeatures found by scanning but not declared as capabilities:\n");
            for (int code = 0; code < 256; code++) {
               if (bbf_is_set(seen_not_caps, code)) {
                  VCP_Feature_Table_Entry * vfte = vcp_find_feature_by_hexid_w_default(code);
                  char * feature_name = get_version_sensitive_feature_name(vfte, vspec);
                  printf("   Feature x%02x - %s\n", code, feature_name);
               }
            }
         }
         else
            printf("\nAll features found by scanning were declared in capabilities.\n");

         bbf_free(features_declared);
         bbf_free(caps_not_seen);
         bbf_free(seen_not_caps);
         free_parsed_capabilities(pcaps);
      }
      else {
         printf("\n\nUnable to read or parse capabilities.\n");
         printf("Skipping comparison of declared capabilities to observed features\n");
      }
      bbf_free(features_seen);


      puts("");
      // get VCP 0B
      Single_Vcp_Value * valrec;
      int color_temp_increment = 0;
      int color_temp_units = 0;
      gsc =  get_vcp_value(
               dh,
             0x0b,              // color temperature increment,
             NON_TABLE_VCP_VALUE,
             &valrec);
      if (gsc == 0) {
         if (debug)
            printf("Value returned for feature x0b: %s\n", summarize_single_vcp_value(valrec) );
         color_temp_increment = valrec->val.c.cur_val;

         gsc =  get_vcp_value(
               dh,
             0x0c,              // color temperature request
             NON_TABLE_VCP_VALUE,
             &valrec);
         if (gsc == 0) {
            if (debug)
               printf("Value returned for feature x0c: %s\n", summarize_single_vcp_value(valrec) );
            color_temp_units = valrec->val.c.cur_val;
            int color_temp = 3000 + color_temp_units * color_temp_increment;
            printf("Color temperature increment (x0b) = %d degrees Kelvin\n", color_temp_increment);
            printf("Color temperature request   (x0c) = %d\n", color_temp_units);
            printf("Requested color temperature = (3000 deg Kelvin) + %d * (%d degrees Kelvin)"
                  " = %d degrees Kelvin\n",
                  color_temp_units,
                  color_temp_increment,
                  color_temp);
         }
      }
      if (gsc != 0)
         printf("Unable to calculate color temperature from VCP features x0B and x0C\n");

      // get VCP 14
      // report color preset

   DBGMSF(debug, "Done.");
}


void probe_display_by_dref(Display_Ref * dref) {
   Display_Handle * dh = NULL;
   Public_Status_Code psc = ddc_open_display(dref, CALLOPT_ERR_MSG, &dh);
   if (psc != 0) {
      printf("Unable to open display %s, status code %s",
             dref_short_name(dref), psc_desc(psc) );
   }
   else {
      probe_display_by_dh(dh);
      ddc_close_display(dh);
   }
}



//
// Mainline
//

int main(int argc, char *argv[]) {
   start_time_nanos = cur_realtime_nanosec();

   // For aborting out of shared library
   jmp_buf abort_buf;
   int jmprc = setjmp(abort_buf);
   if (jmprc) {
      fprintf(stderr, "Aborting. Internal status code = %d\n", jmprc);
      exit(EXIT_FAILURE);
   }
   register_jmp_buf(&abort_buf);

   // set_trace_levels(TRC_ADL);   // uncomment to enable tracing during initialization
   init_base_services();  // so tracing related modules are initialized
   Parsed_Cmd * parsed_cmd = parse_command(argc, argv);
   if (!parsed_cmd) {
      // puts("Terminating execution");
      exit(EXIT_FAILURE);
   }
   if (parsed_cmd->timestamp_trace)         // timestamps on debug and trace messages?
      dbgtrc_show_time = true;              // extern in core.h
   set_trace_levels(parsed_cmd->trace);
#ifdef ENABLE_FAILSIM
   fsim_set_name_to_number_funcs(gsc_name_to_modulated_number, gsc_name_to_unmodulated_number);
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

   init_ddc_services();
   // overrides setting in init_ddc_services():
   i2c_set_io_strategy(DEFAULT_I2C_IO_STRATEGY);

#ifndef HAVE_ADL
   if ( is_module_loaded_using_sysfs("fglrx") ) {
      fprintf(stdout, "WARNING: AMD proprietary video driver fglrx is loaded,");
      fprintf(stdout, "but this copy of ddcutil was built without fglrx support.");
   }
#endif

   int main_rc = EXIT_FAILURE;

   Call_Options callopts = CALLOPT_NONE;
   // TODO: remove CALLOPT_FORCE_SLAVE from callopts
   // if (parsed_cmd->force_slave_addr)
   //    callopts |= CALLOPT_FORCE_SLAVE;
   i2c_force_slave_addr_flag = parsed_cmd->force_slave_addr;
   if (parsed_cmd->force)
      callopts |= CALLOPT_FORCE;

   set_output_level(parsed_cmd->output_level);
   show_recoverable_errors = parsed_cmd->ddcdata;
   // TMI:
   // if (show_recoverable_errors)
   //    parsed_cmd->stats = true;

   if (parsed_cmd->output_level >= OL_VERBOSE) {
      show_reporting();
      f0printf( FOUT, "%.*s%-*s%s\n",
                0,"",
                28, "Force I2C slave address:",
                bool_repr(i2c_force_slave_addr_flag));
      f0puts("\n", FOUT);
   }

   // n. MAX_MAX_TRIES checked during command line parsing
   if (parsed_cmd->max_tries[0] > 0) {
      ddc_set_max_write_only_exchange_tries(parsed_cmd->max_tries[0]);
   }
   if (parsed_cmd->max_tries[1] > 0) {
      ddc_set_max_write_read_exchange_tries(parsed_cmd->max_tries[1]);
   }
   if (parsed_cmd->max_tries[2] > 0) {
      ddc_set_max_multi_part_read_tries(parsed_cmd->max_tries[2]);
   }
   if (parsed_cmd->sleep_strategy >= 0)
      set_sleep_strategy(parsed_cmd->sleep_strategy);

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
         if (parsed_cmd->output_level <= OL_TERSE)
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

   else if (parsed_cmd->cmd_id == CMDID_LISTTESTS) {
      show_test_cases();
      main_rc = EXIT_SUCCESS;
   }

   else if (parsed_cmd->cmd_id == CMDID_DETECT) {
#ifdef OLD
      if (parsed_cmd->programmatic_output)
         set_output_format(OUTPUT_PROG_BUSINFO);
#endif
      // new way:
      ddc_report_active_displays(0);
   }

   else if (parsed_cmd->cmd_id == CMDID_TESTCASE) {
      int testnum;
      bool ok = true;
      int ct = sscanf(parsed_cmd->args[0], "%d", &testnum);
      if (ct != 1) {
         printf("Invalid test number: %s\n", parsed_cmd->args[0]);
         ok = false;
      }
      else {
         if (!parsed_cmd->pdid)
            parsed_cmd->pdid = create_dispno_display_identifier(1);   // default monitor
         ok = execute_testcase(testnum, parsed_cmd->pdid);
      }
      main_rc = (ok) ? EXIT_SUCCESS : EXIT_FAILURE;
   }

   else if (parsed_cmd->cmd_id == CMDID_LOADVCP) {
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
      main_rc = (ok) ? EXIT_SUCCESS : EXIT_FAILURE;
   }

   else if (parsed_cmd->cmd_id == CMDID_ENVIRONMENT) {
      printf("The following tests probe the runtime environment using multiple overlapping methods.\n");
      // DBGMSG("Exploring runtime environment...\n");
      query_sysenv();
      main_rc = EXIT_SUCCESS;
   }

   else if (parsed_cmd->cmd_id == CMDID_USBENV) {
#ifdef USE_USB
      printf("The following tests probe for USB connected monitors.\n");
      // DBGMSG("Exploring runtime environment...\n");
      query_usbenv();
      main_rc = EXIT_SUCCESS;
#else
      printf("ddcutil was not built with support for USB connected monitors\n");
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
      printf("Setting output level verbose...\n");
      set_output_level(OL_VERBOSE);
      printf("Setting maximum retries...\n");
      printf("Forcing --stats...\n");
      parsed_cmd->stats_types = STATS_ALL;
      printf("Forcing --force-slave-address..\n");
      i2c_force_slave_addr_flag = true;
      printf("This command will take a while to run...\n\n");
      ddc_set_max_write_read_exchange_tries(MAX_MAX_TRIES);
      ddc_set_max_multi_part_read_tries(MAX_MAX_TRIES);
      query_sysenv();
#ifdef USE_USB
      query_usbenv();
#endif
      printf("\n*** Detected Displays ***\n");
      int display_ct = ddc_report_active_displays(0 /* logical depth */);
      // printf("Detected: %d displays\n", display_ct);   // not needed
      int dispno = 1;
      // dispno = 2;      // TEMP FOR TESTING
      for (; dispno <= display_ct; dispno++) {
         printf("\nProbing display %d\n", dispno);
         Display_Identifier * did = create_dispno_display_identifier(dispno);
         Display_Ref * dref = get_display_ref_for_display_identifier(did, CALLOPT_ERR_MSG);
         if (!dref) {
            PROGRAM_LOGIC_ERROR("get_display_ref_for_display_identifier() failed for display %d", dispno);
         }

         probe_display_by_dref(dref);
      }
      printf("\nDisplay scanning complete.\n");

      main_rc = EXIT_SUCCESS;
   }

   else {     // commands that require display identifier
      if (!parsed_cmd->pdid)
         parsed_cmd->pdid = create_dispno_display_identifier(1);   // default monitor
      // assert(parsed_cmd->pdid);
      // returns NULL if not a valid display:
      Call_Options callopts = CALLOPT_ERR_MSG;        // emit error messages
      if (parsed_cmd->force)
         callopts |= CALLOPT_FORCE;
      Display_Ref * dref = get_display_ref_for_display_identifier(
                              parsed_cmd->pdid, callopts);
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
               if (vspec.major < 2) {
                  printf("VCP (aka MCCS) version for display is undetected or less than 2.0. "
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
                  Global_Status_Code gsc = app_show_feature_set_values_by_display_handle(
                        dh,
                        parsed_cmd->fref,
                        parsed_cmd->show_unsupported,
                        parsed_cmd->force);
                  main_rc = (gsc==0) ? EXIT_SUCCESS : EXIT_FAILURE;
               }
               break;

            case CMDID_SETVCP:
               if (parsed_cmd->argct % 2 != 0) {
                  printf("SETVCP command requires even number of arguments");
                  main_rc = EXIT_FAILURE;
               }
               else {
                  main_rc = EXIT_SUCCESS;
                  int argNdx;
                  Global_Status_Code rc = 0;
                  for (argNdx=0; argNdx < parsed_cmd->argct; argNdx+= 2) {
                     rc = app_set_vcp_value(
                             dh,
                             parsed_cmd->args[argNdx],
                             parsed_cmd->args[argNdx+1],
                             parsed_cmd->force);
                     if (rc != 0) {
                        main_rc = EXIT_FAILURE;   // ???
                        break;
                     }
                  }
               }
               break;

            case CMDID_DUMPVCP:
               {
                  Global_Status_Code gsc = dumpvcp_as_file(dh, (parsed_cmd->argct > 0) ? parsed_cmd->args[0] : NULL );
                  main_rc = (gsc==0) ? EXIT_SUCCESS : EXIT_FAILURE;
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
      }
   }

   if (parsed_cmd->stats_types != STATS_NONE) {
      report_stats(parsed_cmd->stats_types);
      // report_timestamp_history();  // debugging function
   }

   return main_rc;
}
