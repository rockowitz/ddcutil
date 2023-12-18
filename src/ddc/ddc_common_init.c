/** @file ddc_common_init.c
 *  Initialization that must be performed very early by both ddcutil and libddcutil
 */

// Copyright (C) 2021-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// Contains initialization functions extracted from main.c so they can
// be shared with libmain/api.base.c

#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "config.h"

#ifdef ENABLE_FAILSIM
#include "util/failsim.h"
#endif
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_i2c_util.h"

#include "base/core.h"
#include "base/display_retry_data.h"
#include "base/dsa2.h"
#include "base/parms.h"
#include "base/per_display_data.h"
#include "base/per_thread_data.h"
#include "base/rtti.h"
#include "base/stats.h"
#include "base/tuned_sleep.h"

#include "vcp/persistent_capabilities.h"

#include "dynvcp/dyn_feature_files.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_edid.h"
#include "i2c/i2c_execute.h"
#include "i2c/i2c_strategy_dispatcher.h"

#include "ddc_displays.h"
#include "ddc_multi_part_io.h"
#include "ddc_serialize.h"
#include "ddc_services.h"
#include "ddc/ddc_try_data.h"
#include "ddc_vcp.h"

#include "ddc_common_init.h"


/** Assembles a #Error_Info struct and appends it to an array.
 *
 *  @param errinfo_accumulator  array of #Error_Info
 *  @param func                 function generating the error
 *  @param errcode              status code
 *  @param format               msg template
 *  @param ...                  substitution arguments
 */
STATIC void
emit_init_tracing_error(
      GPtrArray*   errinfo_accumulator,
      const char * func,
      DDCA_Status  errcode,
      const char * format, ...)
{
   assert(errinfo_accumulator);
   va_list(args);
   va_start(args, format);
   char buffer[200];
   vsnprintf(buffer, 200, format, args);
   va_end(args);
   va_end(args);
   g_ptr_array_add(errinfo_accumulator, errinfo_new(errcode, func, buffer));
}


void
i2c_discard_caches(Cache_Types caches) {
   bool debug = false;
   if (caches & CAPABILITIES_CACHE) {
      DBGMSF(debug, "Erasing capabilities cache");
      delete_capabilities_file();
   }
   if (caches & DISPLAYS_CACHE) {
      DBGMSF(debug, "Erasing displays cache");
      ddc_erase_displays_cache();
   }
   if (caches & DSA2_CACHE) {
      DBGMSF(debug, "Erasing dynamic sleep cache");
      dsa2_erase_persistent_stats();
   }
}


Error_Info *
init_tracing(Parsed_Cmd * parsed_cmd)
{
   bool debug = false;
   Error_Info * result = NULL;
   GPtrArray* errinfo_accumulator = g_ptr_array_new_with_free_func(g_free);
   if (debug)
      printf("(%s) Starting.\n",__func__);
   if (parsed_cmd->flags & CMD_FLAG_TIMESTAMP_TRACE)    // timestamps on debug and trace messages?
       dbgtrc_show_time = true;                         // extern in core.h
   if (parsed_cmd->flags & CMD_FLAG_WALLTIME_TRACE)     // wall timestamps on debug and trace messages?
       dbgtrc_show_wall_time = true;                    // extern in core.h
   if (parsed_cmd->flags & CMD_FLAG_THREAD_ID_TRACE)    // thread id on debug and trace messages?
       dbgtrc_show_thread_id = true;                    // extern in core.h
   if (parsed_cmd->flags & CMD_FLAG_PROCESS_ID_TRACE)   // process id on debug and trace messages?
       dbgtrc_show_process_id = true;                   // extern in core.h
   if (parsed_cmd->flags & CMD_FLAG_TRACE_TO_SYSLOG_ONLY)
       dbgtrc_trace_to_syslog_only = true;              // extern in core.h
   if (parsed_cmd->flags & CMD_FLAG_F6)
      watch_watching = true;

   report_freed_exceptions = parsed_cmd->flags & CMD_FLAG_REPORT_FREED_EXCP;   // extern in core.h
   add_trace_groups(parsed_cmd->traced_groups);
   // if (parsed_cmd->s1)
   //    set_trace_destination(parsed_cmd->s1, parser_mode_name(parsed_cmd->parser_mode));

   if (parsed_cmd->traced_functions) {
       for (int ndx = 0; ndx < ntsa_length(parsed_cmd->traced_functions); ndx++) {
          if (debug)
                printf("(%s) Adding traced function: %s\n",
                       __func__, parsed_cmd->traced_functions[ndx]);
          char * curfunc = parsed_cmd->traced_functions[ndx];
          bool found = add_traced_function(curfunc);
          if (!found) {
             emit_init_tracing_error(errinfo_accumulator, __func__, -EINVAL,
                                     "Traced function not found: %s", curfunc);
          }
       }
    }

    if (parsed_cmd->traced_api_calls) {
       for (int ndx = 0; ndx < ntsa_length(parsed_cmd->traced_api_calls); ndx++) {
          char * curfunc = parsed_cmd->traced_api_calls[ndx];
          if (debug)
                printf("(%s) Adding traced api_call: %s\n", __func__, curfunc);

          bool found = add_traced_api_call(curfunc);
          if (!found) {
             emit_init_tracing_error(errinfo_accumulator, __func__, -EINVAL,
                                     "Traced API call not found: %s", curfunc);
          }
       }
   }

   if (parsed_cmd->traced_calls) {
      for (int ndx = 0; ndx < ntsa_length(parsed_cmd->traced_calls); ndx++) {
         char * curfunc = parsed_cmd->traced_calls[ndx];
         if (debug)
               printf("(%s) Adding traced call stack function: %s\n", __func__, curfunc);

         bool found = add_traced_callstack_call(curfunc);
         if (!found) {
            emit_init_tracing_error(errinfo_accumulator, __func__, -EINVAL,
                                     "Traced call stack function not found: %s", curfunc);
         }
      }
   }

   if (parsed_cmd->traced_files) {
       for (int ndx = 0; ndx < ntsa_length(parsed_cmd->traced_files); ndx++) {
          if (debug)
             printf("(%s) Adding traced file: %s\n",
                    __func__, parsed_cmd->traced_files[ndx]);
          add_traced_file(parsed_cmd->traced_files[ndx]);
       }
   }

   ptd_api_profiling_enabled = parsed_cmd->flags & CMD_FLAG_PROFILE_API;

   // if (debug)
   //    printf("(%s) Done. Returning: %s\n", __func__, sbool(ok));

   // dbgrpt_traced_function_table(2);
   if (errinfo_accumulator->len > 0)
      result = errinfo_new_with_causes_gptr(
            -EINVAL, errinfo_accumulator, __func__, "Invalid trace option(s):");
   g_ptr_array_free(errinfo_accumulator, false);
   return result;
}


STATIC bool
init_failsim(Parsed_Cmd * parsed_cmd) {
   bool debug = true;

#ifdef ENABLE_FAILSIM
   fsim_set_name_to_number_funcs(
         status_name_to_modulated_number,
         status_name_to_unmodulated_number);
   if (parsed_cmd->failsim_control_fn) {
      bool loaded = fsim_load_control_file(parsed_cmd->failsim_control_fn);
      if (loaded) {
         printf("Loaded failure simulation control file %s\n", parsed_cmd->failsim_control_fn);
         fsim_report_failure_simulation_table(2);
      }
      else  {
         // fprintf(stderr, "Error loading failure simulation control file %s.\n",
         //                 parsed_cmd->failsim_control_fn);
         return false;
      }
   }
#endif

   if (parsed_cmd->flags & CMD_FLAG_NULL_MSG_INDICATES_UNSUPPORTED_FEATURE) {
      DBGMSF(debug, "setting simulate_null_msg_means_unspported = true");
      simulate_null_msg_means_unsupported = true;
   }
   return true;
}


STATIC void
init_max_tries(Parsed_Cmd * parsed_cmd)
{
   // n. MAX_MAX_TRIES checked during command line parsing
   if (parsed_cmd->max_tries[0] > 0) {
      // resets highest, lowest:
      try_data_init_retry_type(WRITE_ONLY_TRIES_OP, parsed_cmd->max_tries[0]);

      // redundant
      drd_set_default_max_tries(0, parsed_cmd->max_tries[0]);
      // drd_set_initial_display_max_tries(0, parsed_cmd->max_tries[0]);
   }

   if (parsed_cmd->max_tries[1] > 0) {
      try_data_init_retry_type(WRITE_READ_TRIES_OP, parsed_cmd->max_tries[1]);

      drd_set_default_max_tries(1, parsed_cmd->max_tries[1]);
      // drd_set_initial_display_max_tries(1, parsed_cmd->max_tries[1]);
   }

   if (parsed_cmd->max_tries[2] > 0) {
      try_data_init_retry_type(MULTI_PART_READ_OP,  parsed_cmd->max_tries[2]);
      try_data_init_retry_type(MULTI_PART_WRITE_OP, parsed_cmd->max_tries[2]);

      drd_set_default_max_tries(MULTI_PART_READ_OP, parsed_cmd->max_tries[2]);
      // drd_set_initial_display_max_tries(2, parsed_cmd->max_tries[2]);
      // impedance match
      drd_set_default_max_tries(MULTI_PART_WRITE_OP, parsed_cmd->max_tries[2]);
      // drd_set_initial_display_max_tries(3, parsed_cmd->max_tries[2]);
   }
}


STATIC void
init_performance_options(Parsed_Cmd * parsed_cmd)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE,
                         "deferred sleeps: %s", SBOOL(parsed_cmd->flags & CMD_FLAG_DEFER_SLEEPS),
                         "sleep_multiplier: %5.2f", parsed_cmd->sleep_multiplier);
   enable_deferred_sleep( parsed_cmd->flags & CMD_FLAG_DEFER_SLEEPS);

   int threshold = DISPLAY_CHECK_ASYNC_NEVER;
   if (parsed_cmd->flags & CMD_FLAG_ASYNC) {
      threshold = DISPLAY_CHECK_ASYNC_THRESHOLD_STANDARD;
      ddc_set_async_threshold(threshold);
   }
   if (parsed_cmd->flags & CMD_FLAG_I3_SET) {
      ddc_set_async_threshold(parsed_cmd->i3);
   }

   if (parsed_cmd->sleep_multiplier >= 0) {
      User_Multiplier_Source  source =
            (parsed_cmd->flags & CMD_FLAG_EXPLICIT_SLEEP_MULTIPLIER) ? Explicit : Default;
      pdd_set_default_sleep_multiplier_factor(parsed_cmd->sleep_multiplier, source);
   }

   bool dsa2_enabled = parsed_cmd->flags & CMD_FLAG_DSA2;
   dsa2_enable(dsa2_enabled);
   if (dsa2_enabled) {
      if (parsed_cmd->flags & CMD_FLAG_EXPLICIT_SLEEP_MULTIPLIER) {
         dsa2_reset_multiplier(parsed_cmd->sleep_multiplier);
         dsa2_erase_persistent_stats();
      }
      else {
         Error_Info * stats_errs = dsa2_restore_persistent_stats();
         if (stats_errs) {
            // for now, just dump to terminal
            rpt_vstring(0, stats_errs->detail);
            for (int ndx = 0; ndx < stats_errs->cause_ct; ndx++) {
               rpt_vstring(1, stats_errs->causes[ndx]->detail);
            }
            errinfo_free(stats_errs);
         }
      }
      if (parsed_cmd->min_dynamic_multiplier >= 0.0f) {
          dsa2_step_floor = dsa2_multiplier_to_step(parsed_cmd->min_dynamic_multiplier);
          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                "min_dynamic_multiplier = %3.1f, setting dsa2_step_floor = %d",
                parsed_cmd->min_dynamic_multiplier, dsa2_step_floor);
      }
   }
   else {
      // dsa2_erase_persistent_stats();   // do i want to do this ?
   }

   if (display_caching_enabled)
      ddc_restore_displays_cache();
   // else
   //    ddc_erase_displays_cache();

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


STATIC void
init_experimental_options(Parsed_Cmd* parsed_cmd) {
   suppress_se_post_read = parsed_cmd->flags & CMD_FLAG_F1;
   ddc_never_uses_null_response_for_unsupported = parsed_cmd->flags & CMD_FLAG_F3;
   // ddc_always_uses_null_response_for_unsupported = parsed_cmd->flags & CMD_FLAG_F8;
   EDID_Read_Uses_I2C_Layer = parsed_cmd->flags & CMD_FLAG_F5;
   if (parsed_cmd->flags & CMD_FLAG_F7)
      detect_phantom_displays = false;
   ddc_enable_displays_cache(parsed_cmd->flags & (CMD_FLAG_ENABLE_CACHED_DISPLAYS)); // was CMD_FLAG_ENABLE_CACHED_DISPLAYS
   if (parsed_cmd->flags & CMD_FLAG_F10)
      null_msg_adjustment_enabled = true;
   if (parsed_cmd->flags & CMD_FLAG_F11)
      monitor_state_tests = true;
#ifdef TEST_EDID_SMBUS
   if (parsed_cmd->flags & CMD_FLAG_F13)
      EDID_Read_Uses_Smbus = true;
#endif
   if (parsed_cmd->flags & CMD_FLAG_F14)
      force_read_edid = true;
   if (parsed_cmd->flags & CMD_FLAG_I1_SET)
      i2c_businfo_async_threshold = parsed_cmd->i1;
   if (parsed_cmd->flags & CMD_FLAG_I2_SET)
        multi_part_null_adjustment_millis = parsed_cmd->i2;
   // if (parsed_cmd->flags & CMD_FLAG_FL1_SET)
   //     dsa2_step_floor = dsa2_multiplier_to_step(parsed_cmd->fl1);
}


/** Initialization code common to the standalone program ddcutil and
 *  the shared library libddcutil. Called from both main.c and api.base.c.
 *
 *  @param  parsed_cmd   parsed command
 *  @return ok if initialization succeeded, false if not
 */
bool
submaster_initializer(Parsed_Cmd * parsed_cmd) {
   bool debug = false;
   bool ok = false;
   DBGMSF(debug, "Starting  parsed_cmd = %p", parsed_cmd);

   if (!init_failsim(parsed_cmd))
      goto bye;      // main_rc == EXIT_FAILURE


   // init_ddc_services();   // n. initializes start timestamp
   // global variable in dyn_dynamic_features:
   enable_dynamic_features = parsed_cmd->flags & CMD_FLAG_ENABLE_UDF;
   DBGMSF(debug, "          Setting enable_dynamic_features = %s", sbool(enable_dynamic_features));
   if (parsed_cmd->edid_read_size >= 0)
      EDID_Read_Size = parsed_cmd->edid_read_size;

   if (parsed_cmd->flags & CMD_FLAG_I2C_IO_FILEIO)
      i2c_set_io_strategy_by_id(I2C_IO_STRATEGY_FILEIO);
   if (parsed_cmd->flags & CMD_FLAG_I2C_IO_IOCTL)
      i2c_set_io_strategy_by_id(I2C_IO_STRATEGY_IOCTL);
   i2c_enable_cross_instance_locks(parsed_cmd->flags & CMD_FLAG_FLOCK);
   ddc_set_verify_setvcp(parsed_cmd->flags & CMD_FLAG_VERIFY);
   set_output_level(parsed_cmd->output_level);  // current thread
   set_default_thread_output_level(parsed_cmd->output_level); // for future threads
   enable_report_ddc_errors( parsed_cmd->flags & CMD_FLAG_DDCDATA );
   // TMI:
   // if (show_recoverable_errors)
   //    parsed_cmd->stats = true;

   all_video_drivers_implement_drm = check_all_video_adapters_implement_drm();
   get_sys_drm_connectors(false);  // initializes global sys_drm_connectors
   subinit_i2c_bus_core();

   init_max_tries(parsed_cmd);
   enable_mock_data = parsed_cmd->flags & CMD_FLAG_MOCK;

#ifdef USE_USB
#ifndef NDEBUG
   DDCA_Status rc =
#endif
   ddc_enable_usb_display_detection( parsed_cmd->flags & CMD_FLAG_ENABLE_USB );
   assert (rc == DDCRC_OK);
 #endif

   if (parsed_cmd->flags & CMD_FLAG_DISCARD_CACHES) {
      i2c_discard_caches(parsed_cmd->discarded_cache_types);
   }

   init_performance_options(parsed_cmd);
   enable_capabilities_cache(parsed_cmd->flags & CMD_FLAG_ENABLE_CACHED_CAPABILITIES);
   skip_ddc_checks = parsed_cmd->flags & CMD_FLAG_SKIP_DDC_CHECKS;
   init_experimental_options(parsed_cmd);
   ok = true;

bye:
   DBGMSF(debug, "Done      Returning %s", sbool(ok));
   return ok;
}

