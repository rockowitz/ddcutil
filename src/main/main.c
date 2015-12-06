/*
 ============================================================================
 Name        : main.c
 Author      :
 Version     :
 Copyright   :
 Description : Program mainline
 ============================================================================
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/data_structures.h"

#include "base/common.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/msg_control.h"
#include "base/parms.h"
#include "base/util.h"
#include "base/status_code_mgt.h"
#include "base/linux_errno.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_do_io.h"

#include "adl/adl_shim.h"
#include "adl/adl_errors.h"

#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/parse_capabilities.h"
#include "ddc/ddc_services.h"
#include "ddc/try_stats.h"
#include "ddc/vcp_feature_codes.h"

#include "cmdline/parsed_cmd.h"
#include "cmdline/cmd_parser.h"

#include "main/testcases.h"
#include "main/loadvcp.h"



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

void report_stats() {
   // retry related stats
   ddc_report_write_only_stats();
   ddc_report_write_read_stats();
   ddc_report_multi_part_read_stats();
   puts("");
   show_all_status_counts();   // error code counts
   report_sleep_strategy_stats(0);
   puts("");
   report_io_call_stats(0);
   report_sleep_stats(0);

   long elapsed_nanos = cur_realtime_nanosec() - start_time_nanos;
   printf("Elapsed milliseconds (nanoseconds):             %10ld  (%10ld)\n",
         elapsed_nanos / (1000*1000),
         elapsed_nanos);
}


//
// Mainline
//

int main(int argc, char *argv[]) {
   initialize();
   int main_rc = EXIT_FAILURE;

   Parsed_Cmd * parsed_cmd = parse_command(argc, argv);
   //Parsed_Cmd * parsed_cmd = parse_command(argc, argv);
   if (!parsed_cmd) {
      puts("Terminating execution");
      exit(EXIT_FAILURE);
   }
   // showParsedCmd(parsedCmd);

   set_trace_levels(parsed_cmd->trace);

   set_output_level(parsed_cmd->output_level);
   show_recoverable_errors = parsed_cmd->ddcdata;
   // TMI:
   // if (show_recoverable_errors)
   //    parsed_cmd->stats = true;

   if (parsed_cmd->output_level >= OL_VERBOSE)
      show_reporting();

   // where to check MAX_MAX_TRIES not exceeded?
   if (parsed_cmd->max_tries[0] > 0) {
      ddc_set_max_write_only_exchange_tries(parsed_cmd->max_tries[0]);
   }
   if (parsed_cmd->max_tries[1] > 0) {
      ddc_set_max_write_read_exchange_tries(parsed_cmd->max_tries[1]);
   }
   if (parsed_cmd->max_tries[2] > 0) {
      ddc_set_max_multi_part_read_tries(parsed_cmd->max_tries[2]);
   }

   if (parsed_cmd->cmd_id == CMDID_LISTVCP) {
      vcp_list_feature_codes();
      main_rc = EXIT_SUCCESS;
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
#ifdef OLD
      int ct = i2c_report_buses(false /* report_all */);
      ct += adl_show_active_displays();
      if (ct > 0)
         main_rc = EXIT_SUCCESS;

      if (parsed_cmd->output_level >= OL_NORMAL) {
         Display_Info_List *  all_displays = ddc_get_valid_displays();
         int displayct = all_displays->ct;
         printf("\nVCP version implemented:\n");
         int ndx;
         for (ndx=0; ndx< displayct; ndx++) {
            Display_Info * cur_info = &all_displays->info_recs[ndx];
            // temp
            char * short_name = display_ref_short_name(cur_info->dref);
            // printf("Display:       %s\n", short_name);
            // works, but TMI
            // printf("Mfg:           %s\n", cur_info->edid->mfg_id);
            Version_Spec vspec = get_vcp_version_by_display_ref(all_displays->info_recs[ndx].dref);
            // printf("VCP version:   %d.%d\n", vspec.major, vspec.minor);
            if (vspec.major == 0)
               printf("   %s:  VCP version: detection failed\n", short_name);
            else
               printf("   %s:  VCP version: %d.%d\n", short_name, vspec.major, vspec.minor);
         }
      }
#endif
      // new way:
      ddc_show_active_displays(0);
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
      // printf("(%s) Processing command loadvcp.  fn=%s\n", __func__, fn );
      bool ok = loadvcp(fn);
      main_rc = (ok) ? EXIT_SUCCESS : EXIT_FAILURE;
   }

   else {     // commands that require display identifier
      assert(parsed_cmd->pdid);
      // returns NULL if not a valid display:
      Display_Ref * dref = get_display_ref_for_display_identifier(parsed_cmd->pdid, true /* emit_error_msg */);
      if (dref) {
         Version_Spec vspec = get_vcp_version_by_display_ref(dref);
         if (vspec.major < 2) {
            printf("VCP version for display is less than MCCS 2.0. Output may not be accurate.\n");
         }
         switch(parsed_cmd->cmd_id) {

         case CMDID_CAPABILITIES:
            {                   // needed for declared variables
               // Buffer * capabilities = NULL;
               char * capabilities_string;
               // returns Global_Status_Code, but testing capabilities == NULL also checks for success
               // int rc = get_capabilities_buffer_by_display_ref(dref, &capabilities);
               int rc = get_capabilities_string_by_display_ref(dref, &capabilities_string);

               if (rc < 0) {
                  char buf[100];
                  switch(rc) {
                  case DDCRC_REPORTED_UNSUPPORTED:       // should not happen
                  case DDCRC_DETERMINED_UNSUPPORTED:
                       printf("Unsupported request\n");
                       break;
                  case DDCRC_RETRIES:
                       printf("Unable to get capabilities for monitor on %s.  Maximum DDC retries exceeded.\n",
                               display_ref_short_name_r(dref, buf, 100));
                       break;
                  default:
                       printf("(%s) !!! Unable to get capabilities for monitor on %s\n",
                                 __func__, display_ref_short_name_r(dref, buf, 100));
                       printf("(%s) Unexpected status code: %s\n", __func__, gsc_desc(rc));
                  }
                  main_rc = EXIT_FAILURE;
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
                  main_rc = EXIT_SUCCESS;
               }
            }
            break;

         case CMDID_GETVCP:
            {
               // printf("(%s) CMD)D_GETVCP  \n", __func__ );
               char * us = strdup( parsed_cmd->args[0] );
               char * p = us;
               while (*p) {*p=toupper(*p); p++; }

               if ( streq(us,"ALL" )) {
                  // returns void
                  show_vcp_values_by_display_ref(dref, SUBSET_ALL, NULL);
               }
               else if ( is_abbrev(us,"SUPPORTED",3 )) {
                   // returns void
                   show_vcp_values_by_display_ref(dref, SUBSET_SUPPORTED, NULL);
                }
               else if ( is_abbrev(us,"SCAN",3 )) {
                         // returns void
                         show_vcp_values_by_display_ref(dref, SUBSET_SCAN, NULL);
                      }
               else if ( is_abbrev(us, "COLORMGT",3) ) {
                  // returns void
                  show_vcp_values_by_display_ref(dref, SUBSET_COLORMGT, NULL);
               }
               else if ( is_abbrev(us, "PROFILE",3) ) {
                  // printf("(%s) calling setGlobalMsgLevel(%d), new value: %s   \n", __func__, TERSE, msgLevelName(TERSE) );
#ifdef OLD
                  if (parsed_cmd->programmatic_output)
                     set_output_format(OUTPUT_PROG_VCP);
#endif
                  if (dref->ddc_io_mode == DDC_IO_DEVI2C)
                     i2c_report_bus(dref->busno);
                  // returns void
                  show_vcp_values_by_display_ref(dref, SUBSET_PROFILE, NULL);
               }
               else {
                  // returns void
                  show_single_vcp_value_by_display_ref(dref, parsed_cmd->args[0], parsed_cmd->force);
               }
               free(us);
               main_rc = EXIT_SUCCESS;
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
                  rc = set_vcp_value_top(dref, parsed_cmd->args[argNdx], parsed_cmd->args[argNdx+1]);
                  if (rc != 0) {
                     main_rc = EXIT_FAILURE;   // ???
                     break;
                  }
               }
            }
            break;

         case CMDID_DUMPVCP:
            {
               bool ok = dumpvcp(dref, (parsed_cmd->argct > 0) ? parsed_cmd->args[0] : NULL );
               main_rc = (ok) ? EXIT_SUCCESS : EXIT_FAILURE;
               break;
            }
         }
      }
   }

   if (parsed_cmd->stats) {
      report_stats();
      // report_timestamp_history();  // debugging function
   }

   return main_rc;
}
