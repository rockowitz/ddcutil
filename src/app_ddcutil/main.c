/** @file main.c
 *
 *  ddcutil standalone application mainline
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <config.h>

#include <assert.h>
#include <ctype.h>
#include <dynvcp/dyn_dynamic_features.h>
#include <dynvcp/dyn_parsed_capabilities.h>
#include <errno.h>
#include <i2c/i2c_strategy_dispatcher.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/data_structures.h"
#include "util/error_info.h"
#include "util/failsim.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"
/** \endcond */

#include "public/ddcutil_types.h"

#include "base/adl_errors.h"
#include "base/base_init.h"
#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/dynamic_sleep.h"
#include "base/linux_errno.h"
#include "base/parms.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"
#include "base/tuned_sleep.h"

#include "vcp/parse_capabilities.h"
#include "vcp/vcp_feature_codes.h"

#include "dynvcp/dyn_dynamic_features.h"

#include "i2c/i2c_bus_core.h"
#include "adl/adl_shim.h"

#include "usb/usb_displays.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_services.h"
#include "ddc/ddc_try_stats.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"

#include "cmdline/cmd_parser_aux.h"    // for parse_feature_id_or_subset(), should it be elsewhere?
#include "cmdline/cmd_parser.h"
#include "cmdline/parsed_cmd.h"

#include "app_ddcutil/app_dynamic_features.h"
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


// Default race class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_TOP;


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
   FILE * fout = stdout;
   FILE * ferr = stderr;
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
         f0printf(ferr, "Unsupported request\n");
         break;
      case DDCRC_RETRIES:
         f0printf(ferr, "Unable to get capabilities for monitor on %s.  Maximum DDC retries exceeded.\n",
                 dh_repr(dh));
         break;
      default:
         f0printf(ferr, "(%s) !!! Unable to get capabilities for monitor on %s\n",
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
         f0printf(fout,
                  "%s capabilities string: %s\n",
                  (dh->dref->io_path.io_mode == DDCA_IO_USB) ? "Synthesized unparsed" : "Unparsed",
                  capabilities_string);
      }
      else {
         if (dh->dref->io_path.io_mode == DDCA_IO_USB)
            pcap->raw_value_synthesized = true;
         // report_parsed_capabilities(pcap, dh->dref->io_path.io_mode);    // io_mode no longer needed
         dyn_report_parsed_capabilities(
               pcap,
               dh,
               NULL,
               0);
         // free_parsed_capabilities(pcap);
      }
   }
   DBGMSF(debug, "Returning: %p", pcap);
   return pcap;
}


void probe_display_by_dh(Display_Handle * dh)
{
   FILE * fout = stdout;
   bool debug = false;
   DBGMSF(debug, "Starting. dh=%s", dh_repr(dh));
   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;

   Parsed_Edid * pedid = dh->dref->pedid;
   f0printf(fout, "\nEDID version: %d.%d", pedid->edid_version_major, pedid->edid_version_minor);
   f0printf(fout, "\nMfg id: %s, model: %s, sn: %s\n",
                  pedid->mfg_id, pedid->model_name, pedid->serial_ascii);
   f0printf(fout,   "Product code: %u, binary serial number %"PRIu32" (0x%08x)\n",
                  pedid->product_code, pedid->serial_binary, pedid->serial_binary);

   Dref_Flags flags = dh->dref->flags;
   char interpreted[200];
#define FLAG_NAME(_flag) (flags & _flag) ? #_flag : ""
   g_snprintf(interpreted, 200, "%s%s%s%s",
         FLAG_NAME(DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED),
         FLAG_NAME(DREF_DDC_USES_MH_ML_SH_SL_ZERO_FOR_UNSUPPORTED),
         FLAG_NAME(DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED),
         FLAG_NAME(DREF_DDC_DOES_NOT_INDICATE_UNSUPPORTED) );
         f0printf(fout, "\nUnsupported feature indicator: %s\n", interpreted);
#undef FLAG_NAME

   f0printf(fout, "\nCapabilities for display on %s\n", dref_short_name_t(dh->dref));

   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   // not needed, message causes confusing messages if get_vcp_version fails but get_capabilities succeeds
   // if (vspec.major < 2) {
   //    printf("VCP (aka MCCS) version for display is less than 2.0. Output may not be accurate.\n");
   // }

   // reports capabilities, and if successful returns Parsed_Capabilities
   DDCA_Output_Level saved_ol = get_output_level();
   set_output_level(DDCA_OL_VERBOSE);
   Parsed_Capabilities * pcaps = perform_get_capabilities_by_display_handle(dh);
   set_output_level(saved_ol);

   // how to pass this information down into app_show_vcp_subset_values_by_display_handle()?
   bool table_reads_possible = parsed_capabilities_may_support_table_commands(pcaps);
   f0printf(fout, "\nMay support table reads:   %s\n", sbool(table_reads_possible));

   // *** VCP Feature Scan ***
   // printf("\n\nScanning all VCP feature codes for display %d\n", dispno);
   f0printf(fout, "\nScanning all VCP feature codes for display %s\n", dh_repr(dh) );
   Byte_Bit_Flags features_seen = bbf_create();
   app_show_vcp_subset_values_by_display_handle(
         dh, VCP_SUBSET_SCAN, FSF_SHOW_UNSUPPORTED, features_seen);

   if (pcaps) {
      f0printf(fout, "\n\nComparing declared capabilities to observed features...\n");
      Byte_Bit_Flags features_declared =
            parsed_capabilities_feature_ids(pcaps, /*readable_only=*/true);
      char * s0 = bbf_to_string(features_declared, NULL, 0);
      f0printf(fout, "\nReadable features declared in capabilities string: %s\n", s0);
      free(s0);

      Byte_Bit_Flags caps_not_seen = bbf_subtract(features_declared, features_seen);
      Byte_Bit_Flags seen_not_caps = bbf_subtract(features_seen, features_declared);

      f0printf(fout, "\nMCCS (VCP) version reported by capabilities: %s\n",
               format_vspec(pcaps->parsed_mccs_version));
      f0printf(fout, "MCCS (VCP) version reported by feature 0xDf: %s\n",
               format_vspec(vspec));
      if (!vcp_version_eq(pcaps->parsed_mccs_version, vspec))
         f0printf(fout, "Versions do not match!!!\n");

      if (bbf_count_set(caps_not_seen) > 0) {
         f0printf(fout, "\nFeatures declared as readable capabilities but not found by scanning:\n");
         for (int code = 0; code < 256; code++) {
            if (bbf_is_set(caps_not_seen, code)) {
               VCP_Feature_Table_Entry * vfte = vcp_find_feature_by_hexid_w_default(code);
               Display_Feature_Metadata * dfm =
                     dyn_get_feature_metadata_by_dh_dfm(
                        code,
                         dh,
                         true);   //  with_default
               char * feature_name = get_version_sensitive_feature_name(vfte, pcaps->parsed_mccs_version);
               if (!streq(feature_name, dfm->feature_name)) {
                  rpt_vstring(1, "VCP_Feature_Table_Entry feature name: %s", feature_name);
                  rpt_vstring(1, "Display_Feature_Metadata feature name: %s",
                                 dfm->feature_name);
               }
               // assert( streq(feature_name, ifm->external_metadata->feature_name));
               f0printf(fout, "   Feature x%02x - %s\n", code, feature_name);
               if (vfte->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY) {
                  free_synthetic_vcp_entry(vfte);
               }
               // need to free ifm?
            }
         }
      }
      else
         f0printf(fout, "\nAll readable features declared in capabilities were found by scanning.\n");

      if (bbf_count_set(seen_not_caps) > 0) {
         f0printf(fout, "\nFeatures found by scanning but not declared as capabilities:\n");
         for (int code = 0; code < 256; code++) {
            if (bbf_is_set(seen_not_caps, code)) {
               VCP_Feature_Table_Entry * vfte = vcp_find_feature_by_hexid_w_default(code);

               Display_Feature_Metadata * dfm =
                     dyn_get_feature_metadata_by_dh_dfm(
                        code,
                         dh,
                         true);   //  with_default
               char * feature_name = get_version_sensitive_feature_name(vfte, vspec);
               f0printf(fout, "   Feature x%02x - %s\n", code, feature_name);
               if (!streq(feature_name, dfm->feature_name)) {
                  rpt_vstring(1, "VCP_Feature_Table_Entry feature name: %s", feature_name);
                  rpt_vstring(1, "Internal_Feature_Metadata feature name: %s",
                                 dfm->feature_name);
               }
               // assert( streq(feature_name, ifm->external_metadata->feature_name));
               if (vfte->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY) {
                  free_synthetic_vcp_entry(vfte);
               }
               // free ifm
            }
         }
      }
      else
         f0printf(fout, "\nAll features found by scanning were declared in capabilities.\n");

      bbf_free(features_declared);
      bbf_free(caps_not_seen);
      bbf_free(seen_not_caps);
      free_parsed_capabilities(pcaps);
   }
   else {
      f0printf(fout, "\n\nUnable to read or parse capabilities.\n");
      f0printf(fout, "Skipping comparison of declared capabilities to observed features\n");
   }
   bbf_free(features_seen);


   puts("");
   // get VCP 0B
   DDCA_Any_Vcp_Value * valrec;
   int color_temp_increment = 0;
   int color_temp_units = 0;
   ddc_excp = ddc_get_vcp_value(
                 dh,
                 0x0b,              // color temperature increment,
                 DDCA_NON_TABLE_VCP_VALUE,
                 &valrec);
   psc = ERRINFO_STATUS(ddc_excp);
   if (psc == 0) {
      if (debug)
         f0printf(fout, "Value returned for feature x0b: %s\n", summarize_single_vcp_value(valrec) );
      color_temp_increment = valrec->val.c_nc.sl;
      free_single_vcp_value(valrec);

      ddc_excp = ddc_get_vcp_value(
                    dh,
                    0x0c,              // color temperature request
                    DDCA_NON_TABLE_VCP_VALUE,
                    &valrec);
      psc = ERRINFO_STATUS(ddc_excp);
      if (psc == 0) {
         if (debug)
            f0printf(fout, "Value returned for feature x0c: %s\n", summarize_single_vcp_value(valrec) );
         color_temp_units = valrec->val.c_nc.sl;
         int color_temp = 3000 + color_temp_units * color_temp_increment;
         f0printf(fout, "Color temperature increment (x0b) = %d degrees Kelvin\n", color_temp_increment);
         f0printf(fout, "Color temperature request   (x0c) = %d\n", color_temp_units);
         f0printf(fout, "Requested color temperature = (3000 deg Kelvin) + %d * (%d degrees Kelvin)"
               " = %d degrees Kelvin\n",
               color_temp_units,
               color_temp_increment,
               color_temp);
      }
   }
   if (psc != 0) {
      f0printf(fout, "Unable to calculate color temperature from VCP features x0B and x0C\n");
      // errinfo_free(ddc_excp);
      ERRINFO_FREE_WITH_REPORT(ddc_excp, debug || report_freed_exceptions);
   }
   // get VCP 14
   // report color preset

   DBGMSF(debug, "Done.");
}


void probe_display_by_dref(Display_Ref * dref) {
   FILE * fout = stdout;
   Display_Handle * dh = NULL;
   Public_Status_Code psc = ddc_open_display(dref, CALLOPT_ERR_MSG, &dh);
   if (psc != 0) {
      f0printf(fout, "Unable to open display %s, status code %s",
                     dref_short_name_t(dref), psc_desc(psc) );
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
   FILE * fout = stdout;
   bool main_debug = false;
   int main_rc = EXIT_FAILURE;

   // set_trace_levels(TRC_ADL);   // uncomment to enable tracing during initialization
   init_base_services();  // so tracing related modules are initialized
   Parsed_Cmd * parsed_cmd = parse_command(argc, argv);
   if (!parsed_cmd) {
      goto bye;      // main_rc == EXIT_FAILURE
   }

   // configure tracing
   if (parsed_cmd->flags & CMD_FLAG_TIMESTAMP_TRACE)     // timestamps on debug and trace messages?
      dbgtrc_show_time = true;                           // extern in core.h
   if (parsed_cmd->flags & CMD_FLAG_THREAD_ID_TRACE)     // timestamps on debug and trace messages?
      dbgtrc_show_thread_id = true;                      // extern in core.h
   report_freed_exceptions = parsed_cmd->flags & CMD_FLAG_REPORT_FREED_EXCP;   // extern in core.h
   set_trace_levels(parsed_cmd->traced_groups);
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
         goto bye;      // main_rc == EXIT_FAILURE
      }
      fsim_report_error_table(0);
   }
#endif

   // global variable in dyn_dynamic_features:
   enable_dynamic_features = parsed_cmd->flags & CMD_FLAG_ENABLE_UDF;
   enable_sleep_suppression(parsed_cmd->flags & CMD_FLAG_F1);

   init_ddc_services();  // n. initializes start timestamp
   // overrides setting in init_ddc_services():
   i2c_set_io_strategy(DEFAULT_I2C_IO_STRATEGY);

   ddc_set_verify_setvcp(parsed_cmd->flags & CMD_FLAG_VERIFY);

#ifndef HAVE_ADL
   if ( is_module_loaded_using_sysfs("fglrx") ) {
      fprintf(stdout, "WARNING: AMD proprietary video driver fglrx is loaded,");
      fprintf(stdout, "but this copy of ddcutil was built without fglrx support.");
   }
#endif

   Call_Options callopts = CALLOPT_NONE;
   i2c_force_slave_addr_flag = parsed_cmd->flags & CMD_FLAG_FORCE_SLAVE_ADDR;
   if (parsed_cmd->flags & CMD_FLAG_FORCE)
      callopts |= CALLOPT_FORCE;

   set_output_level(parsed_cmd->output_level);
   enable_report_ddc_errors( parsed_cmd->flags & CMD_FLAG_DDCDATA );
   // TMI:
   // if (show_recoverable_errors)
   //    parsed_cmd->stats = true;

   if (parsed_cmd->output_level >= DDCA_OL_VERBOSE) {
      show_reporting();
      f0printf( fout, "%.*s%-*s%s\n",
                0,"",
                28, "Force I2C slave address:",
                sbool(i2c_force_slave_addr_flag));
      f0printf( fout, "%.*s%-*s%s\n",
                0,"",
                28, "User defined features:",
                (enable_dynamic_features) ? "enabled" : "disabled" );  // "Enable user defined features" is too long a title
                // sbool(enable_dynamic_features));
      f0puts("\n", fout);
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

#ifdef USE_USB
   // if ( !(parsed_cmd->flags & CMD_FLAG_ENABLE_USB)) {
      DDCA_Status rc = ddc_enable_usb_display_detection( parsed_cmd->flags & CMD_FLAG_ENABLE_USB );
      assert (rc == DDCRC_OK);
   // }
#endif

   int threshold = DISPLAY_CHECK_ASYNC_NEVER;
   if (parsed_cmd->flags & CMD_FLAG_ASYNC)
      threshold = DISPLAY_CHECK_ASYNC_THRESHOLD;
   ddc_set_async_threshold(threshold);

   if (parsed_cmd->sleep_multiplier != 0 && parsed_cmd->sleep_multiplier != 1) {
      set_sleep_multiplier_factor(parsed_cmd->sleep_multiplier);
      if (parsed_cmd->sleep_multiplier > 1.0f)
         enable_dynamic_sleep_adjustment(parsed_cmd->flags & CMD_FLAG_F2);
   }

   main_rc = EXIT_SUCCESS;     // from now on assume success;
   DBGTRC(main_debug, TRACE_GROUP, "Initialization complete, process commands");

   if (parsed_cmd->cmd_id == CMDID_LISTVCP) {
      vcp_list_feature_codes(stdout);
      main_rc = EXIT_SUCCESS;
   }

   else if (parsed_cmd->cmd_id == CMDID_VCPINFO) {
      bool vcpinfo_ok = true;

      // DDCA_MCCS_Version_Spec vcp_version_any = {0,0};

      Feature_Set_Flags flags = 0;
      if (parsed_cmd->flags & CMD_FLAG_RW_ONLY)
         flags |= FSF_RW_ONLY;
      if (parsed_cmd->flags & CMD_FLAG_RO_ONLY)
         flags |= FSF_RO_ONLY;
      if (parsed_cmd->flags & CMD_FLAG_WO_ONLY)
         flags |= FSF_WO_ONLY;

      VCP_Feature_Set * fset = create_feature_set_from_feature_set_ref(
                                parsed_cmd->fref,
                                parsed_cmd->mccs_vspec,
                                flags);
      if (!fset) {
         vcpinfo_ok = false;
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
         free_vcp_feature_set(fset);
      }

      main_rc = (vcpinfo_ok) ? EXIT_SUCCESS : EXIT_FAILURE;
   }

#ifdef INCLUDE_TESTCASES
   else if (parsed_cmd->cmd_id == CMDID_LISTTESTS) {
      show_test_cases();
      main_rc = EXIT_SUCCESS;
   }
#endif

   // start of commands that actually access monitors

   else if (parsed_cmd->cmd_id == CMDID_DETECT) {
      DBGTRC(main_debug, TRACE_GROUP, "Detecting displays...");
      if (parsed_cmd->i1 <= 0) {     // normal case
         ddc_ensure_displays_detected();
         ddc_report_displays(/*include_invalid_displays=*/ true, 0);
      }
      else {  // temporary
         typedef struct {
            I2C_IO_Strategy_Id  i2c_io_strategy_id;
            bool                i2c_read_bytewise;
            bool                edid_uses_i2c_layer;
            bool                edid_read_bytewise;
         } Choice_Entry;

         Choice_Entry choices[16] =
         { {I2C_IO_STRATEGY_FILEIO,  false,  false, false},
           {I2C_IO_STRATEGY_FILEIO,  false,  false, true},
           {I2C_IO_STRATEGY_FILEIO,  false,  true,  false},
           {I2C_IO_STRATEGY_FILEIO,  false,  true,  true},
           {I2C_IO_STRATEGY_FILEIO,  true,   false, false},
           {I2C_IO_STRATEGY_FILEIO,  true,   false, true},
           {I2C_IO_STRATEGY_FILEIO,  true,   true,  false},
           {I2C_IO_STRATEGY_FILEIO,  true,   true,  true},

           {I2C_IO_STRATEGY_IOCTL,  false,  false, false},
           {I2C_IO_STRATEGY_IOCTL,  false,  false, true},
           {I2C_IO_STRATEGY_IOCTL,  false,  true,  false},
           {I2C_IO_STRATEGY_IOCTL,  false,  true,  true},
           {I2C_IO_STRATEGY_IOCTL,  true,   false, false},
           {I2C_IO_STRATEGY_IOCTL,  true,   false, true},
           {I2C_IO_STRATEGY_IOCTL,  true,   true,  false},
           {I2C_IO_STRATEGY_IOCTL,  true,   true,  true},
         };

         for (int ndx=0; ndx<16; ndx++) {
            Choice_Entry cur = choices[ndx];

            rpt_nl();
            rpt_vstring(0, "===========> IO STRATEGY %d:", ndx+1);
             char * s = (cur.i2c_io_strategy_id == I2C_IO_STRATEGY_FILEIO) ? "FILEIO" : "IOCTL";
             int d = 1;
             rpt_vstring(d, "i2c_io_strategy:          %s", s);
             rpt_vstring(d, "i2c_read_bytewise:        %s", sbool(cur.i2c_read_bytewise));
             rpt_vstring(d, "EDID read uses I2C layer: %s", sbool(cur.edid_uses_i2c_layer));
             rpt_vstring(d, "EDID read bytewise:       %s", sbool(cur.edid_read_bytewise));

             i2c_set_io_strategy      ( cur.i2c_io_strategy_id);
             I2C_Read_Bytewise        = cur.i2c_read_bytewise;
             EDID_Read_Uses_I2C_Layer = cur.edid_uses_i2c_layer;
             EDID_Read_Bytewise       = cur.edid_read_bytewise;

             rpt_nl();
             // discard existing detected monitors
             ddc_discard_detected_displays();
             ddc_ensure_displays_detected();
             // will include any USB or ADL displays, but that's ok
             ddc_report_displays(/*include_invalid_displays=*/ true, 0);
         }
      }
      DBGTRC(main_debug, TRACE_GROUP, "Display detection complete");
      main_rc = EXIT_SUCCESS;
   }

#ifdef INCLUDE_TESTCASES
   else if (parsed_cmd->cmd_id == CMDID_TESTCASE) {
      int testnum;
      bool ok = true;
      int ct = sscanf(parsed_cmd->args[0], "%d", &testnum);
      if (ct != 1) {
         f0printf(fout, "Invalid test number: %s\n", parsed_cmd->args[0]);
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
      bool loadvcp_ok = true;
      if (parsed_cmd->pdid) {
         Display_Ref * dref = get_display_ref_for_display_identifier(
                                 parsed_cmd->pdid, callopts | CALLOPT_ERR_MSG);
         if (!dref)
            loadvcp_ok = false;
         else {
            ddc_open_display(dref, callopts | CALLOPT_ERR_MSG, &dh);  // rc == 0 iff dh, removed CALLOPT_ERR_ABORT
            if (!dh)
               loadvcp_ok = false;
         }
      }
      if (loadvcp_ok) {
         enable_dynamic_sleep_adjustment(parsed_cmd->flags & CMD_FLAG_F2);
         loadvcp_ok = loadvcp_by_file(fn, dh);
      }

      // if we opened the display, we close it
      if (dh)
         ddc_close_display(dh);
      free(fn);
      main_rc = (loadvcp_ok) ? EXIT_SUCCESS : EXIT_FAILURE;
   }

   else if (parsed_cmd->cmd_id == CMDID_ENVIRONMENT) {
      DBGTRC(main_debug, TRACE_GROUP, "Processing command ENVIRONMENT...");
      dup2(1,2);   // redirect stderr to stdout
      ddc_ensure_displays_detected();   // *** NEEDED HERE ??? ***
      DBGTRC(main_debug, TRACE_GROUP, "display detection complete");

      f0printf(fout, "The following tests probe the runtime environment using multiple overlapping methods.\n");
      query_sysenv();
      main_rc = EXIT_SUCCESS;
   }

   else if (parsed_cmd->cmd_id == CMDID_USBENV) {
#ifdef USE_USB
      DBGTRC(main_debug, TRACE_GROUP, "Processing command USBENV...");
      dup2(1,2);   // redirect stderr to stdout
      ddc_ensure_displays_detected();   // *** NEEDED HERE ??? ***
      DBGTRC(main_debug, TRACE_GROUP, "display detection complete");
      f0printf(fout, "The following tests probe for USB connected monitors.\n");
      // DBGMSG("Exploring USB runtime environment...\n");
      query_usbenv();
      main_rc = EXIT_SUCCESS;
#else
      f0printf(fout, "ddcutil was not built with support for USB connected monitors\n");
      main_rc = EXIT_FAILURE;
#endif
   }

   else if (parsed_cmd->cmd_id == CMDID_CHKUSBMON) {
#ifdef USE_USB
      // DBGMSG("Processing command chkusbmon...\n");
      DBGTRC(main_debug, TRACE_GROUP, "Processing command CHKUSBMON...");
      bool is_monitor = check_usb_monitor( parsed_cmd->args[0] );
      main_rc = (is_monitor) ? EXIT_SUCCESS : EXIT_FAILURE;
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
      main_rc = EXIT_FAILURE;
#endif
   }

   else if (parsed_cmd->cmd_id == CMDID_INTERROGATE) {
      DBGTRC(main_debug, TRACE_GROUP, "Processing command INTERROGATE...");
      dup2(1,2);   // redirect stderr to stdout
      // set_ferr(fout);    // ensure that all messages are collected - made unnecessary by dup2()
      f0printf(fout, "Setting output level verbose...\n");
      set_output_level(DDCA_OL_VERBOSE);
      f0printf(fout, "Setting maximum retries...\n");
      f0printf(fout, "Forcing --stats...\n");
      parsed_cmd->stats_types = DDCA_STATS_ALL;
      f0printf(fout, "Forcing --force-slave-address..\n");
      i2c_force_slave_addr_flag = true;
      f0printf(fout, "This command will take a while to run...\n\n");
      ddc_set_max_write_read_exchange_tries(MAX_MAX_TRIES);
      ddc_set_max_multi_part_read_tries(MAX_MAX_TRIES);

      ddc_ensure_displays_detected();    // *** ???
      DBGTRC(main_debug, TRACE_GROUP, "display detection complete");

      query_sysenv();
#ifdef USE_USB
      // 7/2017: disable, USB attached monitors are rare, and this just
      // clutters the output
      f0printf(fout, "\nSkipping USB environment exploration.\n");
      f0printf(fout, "Issue command \"ddcutil usbenvironment --verbose\" if there are any USB attached monitors.\n");
      // query_usbenv();
#endif
      f0printf(fout, "\nStatistics for environment exploration:\n");
      report_stats(DDCA_STATS_ALL);
      reset_stats();

      f0printf(fout, "\n*** Detected Displays ***\n");
      /* int display_ct =  */ ddc_report_displays(
                                 true,   // include_invalid_displays
                                 0);      // logical depth
      // printf("Detected: %d displays\n", display_ct);   // not needed
      f0printf(fout, "\nStatistics for display detection:\n");
      report_stats(DDCA_STATS_ALL);
      reset_stats();

      f0printf(fout, "Setting output level normal  Table features will be skipped...\n");
      set_output_level(DDCA_OL_NORMAL);

      GPtrArray * all_displays = ddc_get_all_displays();
      for (int ndx=0; ndx < all_displays->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
         assert( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
         if (dref->dispno < 0) {
            f0printf(fout, "\nSkipping invalid display on %s\n", dref_short_name_t(dref));
         }
         else {
            f0printf(fout, "\nProbing display %d\n", dref->dispno);
            enable_dynamic_sleep_adjustment(parsed_cmd->flags & CMD_FLAG_F2);   // should this apply to INTERROGATE?
            probe_display_by_dref(dref);
            f0printf(fout, "\nStatistics for probe of display %d:\n", dref->dispno);
            report_stats(DDCA_STATS_ALL);
         }
         reset_stats();
      }
      f0printf(fout, "\nDisplay scanning complete.\n");

      main_rc = EXIT_SUCCESS;
   }

   // *** Commands that require Display Identifier ***
   else {
      DBGTRC(main_debug, TRACE_GROUP, "display identifier supplied");
      if (!parsed_cmd->pdid)
         parsed_cmd->pdid = create_dispno_display_identifier(1);   // default monitor
      // assert(parsed_cmd->pdid);
      // returns NULL if not a valid display:
      Call_Options callopts = CALLOPT_ERR_MSG;        // emit error messages
      if (parsed_cmd->flags & CMD_FLAG_FORCE)
         callopts |= CALLOPT_FORCE;

      // If --nodetect and --bus options were specified,skip scan for all devices.
      // --nodetect option not needed, just do it
      // n. useful even if not much speed up, since avoids cluttering stats
      // with all the failures during detect
      Display_Ref * dref = NULL;
      if (parsed_cmd->pdid->id_type == DISP_ID_BUSNO && (parsed_cmd->flags & CMD_FLAG_NODETECT)) {
         int busno = parsed_cmd->pdid->busno;
         // is this really a monitor?
         I2C_Bus_Info * businfo = i2c_detect_single_bus(busno);
         if ( businfo && (businfo->flags & I2C_BUS_ADDR_0X50) ) {
            dref = create_bus_display_ref(busno);
            dref->dispno = -1;     // should use some other value for unassigned vs invalid
            dref->pedid = businfo->edid;    // needed?
            // dref->pedid = i2c_get_parsed_edid_by_busno(parsed_cmd->pdid->busno);
            dref->detail = businfo;
            dref->flags |= DREF_DDC_IS_MONITOR_CHECKED;
            dref->flags |= DREF_DDC_IS_MONITOR;
            dref->flags |= DREF_TRANSIENT;
            if (!initial_checks_by_dref(dref)) {
               f0printf(fout, "DDC communication failed for monitor on I2C bus /dev/i2c-%d\n", busno);
               free_display_ref(dref);
               dref = NULL;
            }
            // else
               // check_dynamic_features(dref);    // the hook wrong location
            // DBGMSG("Synthetic Display_Ref");
         }
         else {
            f0printf(fout, "No monitor detected on I2C bus /dev/i2c-%d\n", busno);
         }
      }
      else {
         DBGTRC(main_debug, TRACE_GROUP, "Detecting displays...");
         ddc_ensure_displays_detected();
         DBGTRC(main_debug, TRACE_GROUP, "display detection complete");
         dref = get_display_ref_for_display_identifier(parsed_cmd->pdid, callopts);
      }

      if (dref) {
         Display_Handle * dh = NULL;
         callopts |=  CALLOPT_ERR_MSG;    // removed CALLOPT_ERR_ABORT
         ddc_open_display(dref, callopts, &dh);

         if (dh) {
            enable_dynamic_sleep_adjustment(parsed_cmd->flags & CMD_FLAG_F2);   // here or per command?
            if (// parsed_cmd->cmd_id == CMDID_CAPABILITIES ||
                parsed_cmd->cmd_id == CMDID_GETVCP       ||
                parsed_cmd->cmd_id == CMDID_READCHANGES
               )
            {
               DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
               if (vspec.major < 2 && get_output_level() >= DDCA_OL_NORMAL) {
                  f0printf(fout, "VCP (aka MCCS) version for display is undetected or less than 2.0. "
                        "Output may not be accurate.\n");
               }
            }

            switch(parsed_cmd->cmd_id) {

            case CMDID_CAPABILITIES:
               {
                  check_dynamic_features(dref);

                  Parsed_Capabilities * pcaps = perform_get_capabilities_by_display_handle(dh);
                  main_rc = (pcaps) ? EXIT_SUCCESS : EXIT_FAILURE;
                  if (pcaps)
                     free_parsed_capabilities(pcaps);
                  break;
               }

            case CMDID_GETVCP:
               {
                  check_dynamic_features(dref);

                  Feature_Set_Flags flags = 0x00;

                  // DBGMSG("parsed_cmd->flags: 0x%04x", parsed_cmd->flags);
                  if (parsed_cmd->flags & CMD_FLAG_SHOW_UNSUPPORTED)
                     flags |= FSF_SHOW_UNSUPPORTED;
                  if (parsed_cmd->flags & CMD_FLAG_FORCE)
                     flags |= FSF_FORCE;
                  if (parsed_cmd->flags & CMD_FLAG_NOTABLE)
                     flags |= FSF_NOTABLE;
                  if (parsed_cmd->flags & CMD_FLAG_RW_ONLY)
                     flags |= FSF_RW_ONLY;
                  if (parsed_cmd->flags & CMD_FLAG_RO_ONLY)
                     flags |= FSF_RO_ONLY;

                  // this is nonsense, get do getvcp on a WO feature
                  // should be caught by parser
                  if (parsed_cmd->flags & CMD_FLAG_WO_ONLY) {
                     flags |= FSF_WO_ONLY;
                     DBGMSG("Invalid: GETVCP for WO features");
                     assert(false);
                  }
                  // char * s0 = feature_set_flag_names(flags);
                  // DBGMSG("flags: 0x%04x - %s", flags, s0);
                  // free(s0);

                  Public_Status_Code psc = app_show_feature_set_values_by_display_handle(
                        dh,
                        parsed_cmd->fref,
                        flags
                        );
                  main_rc = (psc==0) ? EXIT_SUCCESS : EXIT_FAILURE;
               }
               break;

            case CMDID_SETVCP:
               if (parsed_cmd->argct % 2 != 0) {
                  f0printf(fout, "SETVCP command requires even number of arguments\n");
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
                             parsed_cmd->flags & CMD_FLAG_FORCE);
                     if (ddc_excp) {
                        ERRINFO_FREE_WITH_REPORT(ddc_excp, report_freed_exceptions);
                        main_rc = EXIT_FAILURE;   // ???
                        break;
                     }
                  }
               }
               break;

            case CMDID_SAVE_SETTINGS:
               if (parsed_cmd->argct != 0) {
                  f0printf(fout, "SCS command takes no arguments\n");
                  main_rc = EXIT_FAILURE;
               }
               else if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
                  f0printf(fout, "SCS command not supported for USB devices\n");
                  main_rc = EXIT_FAILURE;
               }
               else {
                  main_rc = EXIT_SUCCESS;
                  Error_Info * ddc_excp = ddc_save_current_settings(dh);
                  if (ddc_excp)  {
                     f0printf(fout, "Save current settings failed. rc=%s\n", psc_desc(ddc_excp->status_code));
                     if (ddc_excp->status_code == DDCRC_RETRIES)
                        f0printf(fout, "    Try errors: %s", errinfo_causes_string(ddc_excp) );
                     errinfo_report(ddc_excp, 0);   // ** ALTERNATIVE **/
                     errinfo_free(ddc_excp);
                     // ERRINFO_FREE_WITH_REPORT(ddc_excp, report_exceptions);
                     main_rc = EXIT_FAILURE;
                  }
               }
               break;

            case CMDID_DUMPVCP:
               {
                  check_dynamic_features(dref);

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
               main_rc = EXIT_SUCCESS;
               break;

            case CMDID_PROBE:
               check_dynamic_features(dref);

               probe_display_by_dh(dh);
               main_rc = EXIT_SUCCESS;
               break;

            default:
               main_rc = EXIT_FAILURE;
               break;
            }

            ddc_close_display(dh);
         }
         if (dref->flags & DREF_TRANSIENT)
            free_display_ref(dref);
      }   // if (dref)
      else {
         main_rc = EXIT_FAILURE;
      }
   }

   if (parsed_cmd->stats_types != DDCA_STATS_NONE && parsed_cmd->cmd_id != CMDID_INTERROGATE) {
      report_stats(parsed_cmd->stats_types);
      // report_timestamp_history();  // debugging function
   }
   free_parsed_cmd(parsed_cmd);

bye:
   DBGTRC(main_debug, TRACE_GROUP, "Done.  main_rc=%d", main_rc);
   return main_rc;
}
