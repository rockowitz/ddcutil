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

#include <util/data_structures.h>

#include <base/common.h>
#include <base/ddc_errno.h>
#include <base/ddc_packets.h>
#include <base/displays.h>
#include <base/msg_control.h>
#include <base/parms.h>
#include <base/util.h>
#include <base/status_code_mgt.h>
#include <base/linux_errno.h>

#include <i2c/i2c_bus_core.h>
#include <i2c/i2c_do_io.h>

#include <adl/adl_intf.h>
#include <adl/adl_errors.h>

#include <ddc/ddc_multi_part_io.h>
#include <ddc/ddc_packet_io.h>
#include <ddc/ddc_vcp.h>
#include <ddc/parse_capabilities.h>
#include <ddc/ddc_services.h>
#include <ddc/try_stats.h>
#include <ddc/vcp_feature_codes.h>

#include <main/cmd_parser_popt.h>
#include <main/loadvcp.h>
#include <main/testcases.h>


//
// Statistics
//

static long start_time_nanos;
// static I2C_Call_Stats* pi2c_call_stats;
// static ADL_Call_Stats* padl_call_stats;

void reset_stats() {
   ddc_reset_write_only_stats();
   ddc_reset_write_read_stats();
   reset_multi_part_read_stats();
   // init_status_counts();

   start_time_nanos = cur_realtime_nanosec();
   init_sleep_stats();  // debug.c

#ifdef OLD
   pi2c_call_stats = new_i2c_call_stats();
   // printf("(%s) Calling init_i2c_bus_stats(%p)\n", __func__, pi2c_call_stats);
   init_i2c_bus_stats(pi2c_call_stats);     // i2c_io.h

   padl_call_stats = new_adl_call_stats();
   init_adl_call_stats(padl_call_stats);     // adl_intf.c
#endif
   init_call_stats();

#ifdef FUTURE
   reset_status_code_counts();   // currently does nothing
#endif
}


void report_stats(int cmd_id) {
   // retry related stats
   ddc_report_write_only_stats();
   ddc_report_write_read_stats();
   // if (cmd_id == CMDID_CAPABILITIES)
      report_multi_part_read_stats();
   puts("");

   // Msg_Level msg_lvl = get_global_msg_level();

   // error code counts
   show_status_counts();


   report_sleep_strategy_stats(0);

   // os and ADL driver call stats
#ifdef OLD
   if (pi2c_call_stats)
      report_i2c_call_stats(pi2c_call_stats, 0);
   if (padl_call_stats)
      report_adl_call_stats(padl_call_stats, 0);
#endif
   puts("");
   report_call_stats(0);
   report_sleep_stats(0);


   long elapsed_nanos = cur_realtime_nanosec() - start_time_nanos;
   printf("Elapsed milliseconds (nanoseconds):            %10ld  (%10ld)\n",
         elapsed_nanos / (1000*1000),
         elapsed_nanos);
}


//
//  Display specification
//

/* Converts the display identifiers passed on the command line to a logical
 * identifier for an I2C or ADL display.  If a bus number of ADL adapter.display
 * number is specified, the translation is direct.  If a model name/serial number
 * pair or an EDID is specified, the attached displays are searched.
 *
 * Arguments:
 *    pdid      display identifiers
 *    validate  if searching was not necessary, validate that that bus number or
 *              ADL number does in fact reference an attached display
 *
 * Returns:
 *    DisplayRef instance specifying the display using either an I2C bus number
 *    or an ADL adapter.display number, NULL if display not found
 */
Display_Ref* get_display_ref_for_display_identifier(Display_Identifier* pdid, bool validate) {
   Display_Ref* dref = NULL;
   bool validated = true;

   switch (pdid->id_type) {
   case DISP_ID_BUSNO:
      dref = create_bus_display_ref(pdid->busno);
#ifdef OLD
      dref = calloc(1,sizeof(Display_Ref));
      dref->ddc_io_mode = DDC_IO_DEVI2C;
      dref->busno = pdid->busno;
#endif
      validated = false;
      break;
   case DISP_ID_ADL:
      dref = create_adl_display_ref(pdid->iAdapterIndex, pdid->iDisplayIndex);
#ifdef OLD
      dref = calloc(1,sizeof(Display_Ref));
      dref->ddc_io_mode = DDC_IO_ADL;
      dref->iAdapterIndex = pdid->iAdapterIndex;
      dref->iDisplayIndex = pdid->iDisplayIndex;
#endif
      validated = false;
      break;
   case DISP_ID_MONSER:
      dref = ddc_find_display_by_model_and_sn(pdid->model_name, pdid->serial_ascii);  // in ddc_packet_io
      if (!dref) {
         fprintf(stderr, "Unable to find monitor with the specified model and serial number\n");
      }
      break;
   case DISP_ID_EDID:
      dref = ddc_find_display_by_edid(pdid->edidbytes);
      if (!dref) {
         fprintf(stderr, "Unable to find monitor with the specified EDID\n" );
      }
      break;
   default:
      PROGRAM_LOGIC_ERROR("Invalid DisplayIdType value: %d\n", pdid->id_type);
   }  // switch

   if (dref) {
      if (!validated)      // DISP_ID_BUSNO or DISP_ID_ADL
        validated = ddc_is_valid_display_ref(dref);
      if (!validated) {
         free(dref);
         dref = NULL;
      }
   }

   // printf("(%s) Returning: %s\n", __func__, (pdref)?"non-null": "NULL" );
   return dref;
}


//
// Mainline
//

int main(int argc, char *argv[]) {
   reset_stats();
   init_status_code_mgt();
   init_linux_errno();
   init_adl_errors();
   init_vcp_feature_codes();
   // init_i2c_io();   // currently does nothing

   set_i2c_io_strategy(DEFAULT_I2C_IO_STRATEGY);

   int main_rc = EXIT_FAILURE;

   Parsed_Cmd * parsed_cmd = parse_command(argc, argv);
   if (!parsed_cmd) {
      puts("Terminating execution");
      exit(EXIT_FAILURE);
   }
   // showParsedCmd(parsedCmd);

   set_trace_levels(parsed_cmd->trace);
   // delay initializing ADL until after trace levels are set so
   // that tracing during ADL initialization can be controlled.
   adl_initialize();
   init_ddc_packets();   // 11/2015: does nothing

   set_output_level(parsed_cmd->output_level);
   show_recoverable_errors = parsed_cmd->ddcdata;
   if (show_recoverable_errors)
      parsed_cmd->stats = true;

   if (parsed_cmd->output_level >= OL_VERBOSE)
      show_reporting();

   if (parsed_cmd->cmd_id == CMDID_LISTVCP) {
      list_feature_codes();
      main_rc = EXIT_SUCCESS;
   }

   else if (parsed_cmd->cmd_id == CMDID_LISTTESTS) {
      showTestCases();
      main_rc = EXIT_SUCCESS;
   }

   else if (parsed_cmd->cmd_id == CMDID_DETECT) {
#ifdef OLD
      if (parsed_cmd->programmatic_output)
         set_output_format(OUTPUT_PROG_BUSINFO);
#endif
      int ct = report_i2c_buses(false /* report_all */);
      ct += show_active_adl_displays();
      if (ct > 0)
         main_rc = EXIT_SUCCESS;

      if (parsed_cmd->output_level >= OL_NORMAL) {
         Display_Info_List *  all_displays = get_valid_ddc_displays();
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
      Display_Ref * dref = get_display_ref_for_display_identifier(parsed_cmd->pdid, !parsed_cmd->force);
      if (dref) {
         Version_Spec vspec = get_vcp_version_by_display_ref(dref);
         if (vspec.major < 2) {
            printf("VCP version for display is less than MCCS 2.0. Output may not be accurate.\n");
         }
         switch(parsed_cmd->cmd_id) {

         case CMDID_CAPABILITIES:
            {                   // needed for declared variables
               Buffer * capabilities = NULL;
               // returns Global_Status_Code, but testing capabilities == NULL also checks for success
               int rc = get_capabilities(dref, &capabilities);

               if (rc < 0) {
                  char buf[100];
                  switch(rc) {
                  case DDCRC_UNSUPPORTED:
                       printf("Unsupported request\n");
                       break;
                  case DDCRC_RETRIES:
                       printf("Unable to get capabilities for monitor on %s.  Maximum DDC retries exceeded.\n",
                               display_ref_short_name_r(dref, buf, 100));
                       break;
                  default:
                       printf("(%s) !!! Unable to get capabilities for monitor on %s\n",
                                 __func__, display_ref_short_name_r(dref, buf, 100));
                       printf("(%s) Unexpected status code: %s\n", __func__, global_status_code_description(rc));
                  }
                  main_rc = EXIT_FAILURE;
               }
               else {
                  assert(capabilities);
                  // pcap is always set, but may be damaged if there was a parsing error
                  Parsed_Capabilities * pcap = parse_capabilities_buffer(capabilities);
                  buffer_free(capabilities, "capabilities");
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
                  show_vcp_values_by_display_ref(dref, SUBSET_ALL, stdout);
               }
               else if ( is_abbrev(us,"SUPPORTED",3 )) {
                   // returns void
                   show_vcp_values_by_display_ref(dref, SUBSET_SUPPORTED, stdout);
                }
               else if ( is_abbrev(us,"SCAN",3 )) {
                         // returns void
                         show_vcp_values_by_display_ref(dref, SUBSET_SCAN, stdout);
                      }
               else if ( is_abbrev(us, "COLORMGT",3) ) {
                  // returns void
                  show_vcp_values_by_display_ref(dref, SUBSET_COLORMGT, stdout);
               }
               else if ( is_abbrev(us, "PROFILE",3) ) {
                  // printf("(%s) calling setGlobalMsgLevel(%d), new value: %s   \n", __func__, TERSE, msgLevelName(TERSE) );
#ifdef OLD
                  if (parsed_cmd->programmatic_output)
                     set_output_format(OUTPUT_PROG_VCP);
#endif
                  if (dref->ddc_io_mode == DDC_IO_DEVI2C)
                     report_i2c_bus(dref->busno);
                  // returns void
                  show_vcp_values_by_display_ref(dref, SUBSET_PROFILE, stdout);
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
      report_stats(parsed_cmd->cmd_id);
      // report_timestamp_history();  // debugging function
   }

   return main_rc;;
}
