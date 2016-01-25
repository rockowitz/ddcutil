/* main.c
 *
 * Author: rock
 *
 * Program mainline
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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../app_ddctool/loadvcp.h"
#include "../app_ddctool/query_sysenv.h"
#include "../app_ddctool/testcases.h"
#include "../ddc/ddc_output.h"
#include "util/data_structures.h"

#include "base/common.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/msg_control.h"
#include "base/linux_errno.h"
#include "base/parms.h"
#include "base/util.h"
#include "base/status_code_mgt.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_do_io.h"

#include "adl/adl_shim.h"
#include "adl/adl_errors.h"

#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/parse_capabilities.h"
#include "ddc/try_stats.h"
#include "ddc/vcp_feature_codes.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_displays.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_services.h"

#include "cmdline/cmd_parser_aux.h"    // for parse_feature_id_or_subset(), should it be elsewhere?
#include "cmdline/parsed_cmd.h"
#include "cmdline/cmd_parser.h"

#include "app_ddctool/app_setvcp.h"
#include "app_ddctool/app_getvcp.h"



//
// Initialization and Statistics
//

static long start_time_nanos;

void initialize() {
   start_time_nanos = cur_realtime_nanosec();
   init_ddc_services();

   // overrides setting in init_ddc_services():
   i2c_set_io_strategy(DEFAULT_I2C_IO_STRATEGY);
}

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



bool perform_get_capabilities_by_display_handle(Display_Handle * dh) {
   bool ok = true;
   // Buffer * capabilities = NULL;
   char * capabilities_string;
   // returns Global_Status_Code, but testing capabilities == NULL also checks for success
   // int rc = get_capabilities_buffer_by_display_ref(dref, &capabilities);
   int rc = get_capabilities_string(dh, &capabilities_string);

   if (rc < 0) {
      // char buf[100];
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
      ok = false;
   }
   else {
      // assert(capabilities);
      assert(capabilities_string);
      // pcap is always set, but may be damaged if there was a parsing error
      // Parsed_Capabilities * pcap = parse_capabilities_buffer(capabilities);
      // Parsed_Capabilities * pcap = parse_capabilities_string(capabilities->bytes);
      Parsed_Capabilities * pcap = parse_capabilities_string(capabilities_string);
      // buffer_free(capabilities, "capabilities");
      report_parsed_capabilities(pcap);
      free_parsed_capabilities(pcap);
      ok = true;
   }

   return ok;
}



//
// Mainline
//

int main(int argc, char *argv[]) {
   // set_trace_levels(TRC_ADL);   // uncomment to enable tracing during initialization
   initialize();
   int main_rc = EXIT_FAILURE;

   Parsed_Cmd * parsed_cmd = parse_command(argc, argv);
   if (!parsed_cmd) {
      puts("Terminating execution");
      exit(EXIT_FAILURE);
   }

   set_trace_levels(parsed_cmd->trace);

   set_output_level(parsed_cmd->output_level);
   show_recoverable_errors = parsed_cmd->ddcdata;
   // TMI:
   // if (show_recoverable_errors)
   //    parsed_cmd->stats = true;

   if (parsed_cmd->output_level >= OL_VERBOSE)
      show_reporting();

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

      Version_Spec vcp_version_any = {0,0};
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
         main_rc = EXIT_FAILURE;     // ?? What should value be?
      }
      ok = execute_testcase(testnum, parsed_cmd->pdid);
      main_rc = (ok) ? EXIT_SUCCESS : EXIT_FAILURE;
   }

   else if (parsed_cmd->cmd_id == CMDID_LOADVCP) {
      char * fn = strdup( parsed_cmd->args[0] );
      // DBGMSG("Processing command loadvcp.  fn=%s", fn );
      bool ok = loadvcp_from_file(fn);
      main_rc = (ok) ? EXIT_SUCCESS : EXIT_FAILURE;
   }

   else if (parsed_cmd->cmd_id == CMDID_ENVIRONMENT) {
      printf("The following tests probe the runtime environment using multiple overlapping methods.\n");
      // printf("Exploring runtime environment...\n");
      query_sysenv();
      main_rc = true;
   }

   else if (parsed_cmd->cmd_id == CMDID_INTERROGATE) {
      printf("Setting output level verbose...\n");
      set_output_level(OL_VERBOSE);
      printf("Setting maximum retries...\n");
      printf("Forcing --stats...\n");
      parsed_cmd->stats_types = STATS_ALL;
      printf("This command will take a while to run...\n\n");
      ddc_set_max_write_read_exchange_tries(MAX_MAX_TRIES);
      ddc_set_max_multi_part_read_tries(MAX_MAX_TRIES);
      query_sysenv();
      printf("\nDetected displays:\n");
      int display_ct = ddc_report_active_displays(1 /* logical depth */);
      int dispno;
      for (dispno=1; dispno <= display_ct; dispno++) {
         printf("\nCapabilities for display %d\n", dispno);
         Display_Identifier * did = create_dispno_display_identifier(dispno);
         Display_Ref * dref = get_display_ref_for_display_identifier(did, true /* emit_error_msg */);
         if (!dref) {
            PROGRAM_LOGIC_ERROR("get_display_ref_for_display_identifier() failed for display %d", dispno);
         }
         Display_Handle * dh = ddc_open_display(dref, EXIT_IF_FAILURE);
         Version_Spec vspec = get_vcp_version_by_display_handle(dh);
         if (vspec.major < 2) {
            printf("VCP (aka MCCS) version for display is less than 2.0. Output may not be accurate.\n");
         }
         perform_get_capabilities_by_display_handle(dh);

         printf("\n\nScanning all VCP feature codes for display %d\n", dispno);
         app_show_vcp_subset_values_by_display_handle(dh, VCP_SUBSET_SCAN, true);
         ddc_close_display(dh);
      }
      printf("\nDisplay scanning complete.\n");

      main_rc = EXIT_SUCCESS;
   }

   else {     // commands that require display identifier
      assert(parsed_cmd->pdid);
      // returns NULL if not a valid display:
      Display_Ref * dref = get_display_ref_for_display_identifier(
                              parsed_cmd->pdid, true /* emit_error_msg */);
      if (dref) {
         Display_Handle * dh = ddc_open_display(dref, EXIT_IF_FAILURE);
         Version_Spec vspec = get_vcp_version_by_display_handle(dh);
         if (vspec.major < 2) {
            printf("VCP (aka MCCS) version for display is less than 2.0. Output may not be accurate.\n");
         }

         switch(parsed_cmd->cmd_id) {

         case CMDID_CAPABILITIES:
            {;
               bool ok = perform_get_capabilities_by_display_handle(dh);
               main_rc = (ok) ? EXIT_SUCCESS : EXIT_FAILURE;
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
                  rc = app_set_vcp_value_by_display_handle(
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
               bool ok = dumpvcp_to_file_new(dh, (parsed_cmd->argct > 0) ? parsed_cmd->args[0] : NULL );
               main_rc = (ok) ? EXIT_SUCCESS : EXIT_FAILURE;
               break;
            }

         case CMDID_READCHANGES:
            // DBGMSG("Case CMDID_READCHANGES");
            // report_parsed_cmd(parsed_cmd,0);
            app_read_changes_forever(dh);
            break;

         default:
           break;
         }

         ddc_close_display(dh);
      }
   }

   if (parsed_cmd->stats_types != STATS_NONE) {
      report_stats(parsed_cmd->stats_types);
      // report_timestamp_history();  // debugging function
   }

   return main_rc;
}
