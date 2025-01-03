/** @file ddc_common_init.c
 *  Initialization that must be performed very early by both ddcutil and libddcutil
 */

// Copyright (C) 2021-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// Contains initialization functions extracted from main.c so they can
// be shared with libmain/api.base.c

#include <assert.h>
#include <base/drm_connector_state.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "config.h"

#include "util/debug_util.h"
#ifdef ENABLE_FAILSIM
#include "util/failsim.h"
#endif
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_i2c_util.h"
#include "util/timestamp.h"
#ifdef USE_LIBDRM
#include "util/drm_common.h"
#include "util/libdrm_util.h"
#endif

#include "base/core.h"
#include "base/display_retry_data.h"
#include "base/dsa2.h"
#include "base/flock.h"
#include "base/i2c_bus_base.h"
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
#include "i2c/i2c_sysfs_base.h"
#include "i2c/i2c_sys_drm_connector.h"

#include "ddc_displays.h"
#include "ddc_multi_part_io.h"
#include "ddc_serialize.h"
#include "ddc_services.h"
#include "ddc_try_data.h"
#include "ddc_vcp.h"
#include "ddc/ddc_watch_displays_udev.h"
#include "ddc/ddc_watch_displays_poll.h"
#include "ddc_watch_displays_main.h"
#include "ddc_watch_displays_common.h"

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
   GPtrArray* errinfo_accumulator = g_ptr_array_new_with_free_func((GDestroyNotify) errinfo_free);
   DBGF(debug, "Starting.");
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

   report_freed_exceptions = parsed_cmd->flags & CMD_FLAG_REPORT_FREED_EXCP;   // extern in core.h
   add_trace_groups(parsed_cmd->traced_groups);
   // if (parsed_cmd->s1)
   //    set_trace_destination(parsed_cmd->s1, parser_mode_name(parsed_cmd->parser_mode));

   if (parsed_cmd->traced_functions) {
       for (int ndx = 0; ndx < ntsa_length(parsed_cmd->traced_functions); ndx++) {
          DBGF(debug, "Adding traced function: %s", parsed_cmd->traced_functions[ndx]);
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
          DBGF(debug, "Adding traced api_call: %s", curfunc);
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
         DBGF(debug, "Adding traced call stack function: %s", curfunc);
         bool found = add_traced_callstack_call(curfunc);
         if (!found) {
            emit_init_tracing_error(errinfo_accumulator, __func__, -EINVAL,
                                     "Traced call stack function not found: %s", curfunc);
         }
      }
   }

   if (parsed_cmd->traced_files) {
       for (int ndx = 0; ndx < ntsa_length(parsed_cmd->traced_files); ndx++) {
          DBGF(debug, "Adding traced file: %s", parsed_cmd->traced_files[ndx]);
          add_traced_file(parsed_cmd->traced_files[ndx]);
       }
   }

   ptd_api_profiling_enabled = parsed_cmd->flags & CMD_FLAG_PROFILE_API;

   // dbgrpt_traced_function_table(2);
   if (errinfo_accumulator->len > 0)
      result = errinfo_new_with_causes_gptr(
            DDCRC_CONFIG_ERROR, errinfo_accumulator, __func__, "Invalid trace option(s):");
   g_ptr_array_set_free_func(errinfo_accumulator, (GDestroyNotify) errinfo_free);
   g_ptr_array_free(errinfo_accumulator, true);

   tracing_initialized = true;
   return result;
}


STATIC Error_Info * init_disabled_displays(Parsed_Cmd * parsed_cmd) {
   bool debug = false;
   Error_Info * errinfo = NULL;
   GPtrArray* errinfo_accumulator = g_ptr_array_new_with_free_func((GDestroyNotify) errinfo_free);
   if (parsed_cmd->ddc_disabled) {
    for (int ndx = 0; ndx < ntsa_length(parsed_cmd->ddc_disabled); ndx++) {
          // DBGF(debug, "Adding disabled_mmid: %s", parsed_cmd->ddc_disabled[ndx]);
          char * cur_mmid = parsed_cmd->ddc_disabled[ndx];
          bool found = add_disabled_mmk_by_string(cur_mmid);
          if (!found) {
             Error_Info * err = errinfo_new(DDCRC_CONFIG_ERROR, "Invalid mmid: %s", cur_mmid);
             g_ptr_array_add(errinfo_accumulator, err);
          }
       }
    }

   if (debug)
      dbgrpt_ddc_disabled_table(2);

   if (errinfo_accumulator->len > 0)
      errinfo = errinfo_new_with_causes_gptr(
            DDCRC_CONFIG_ERROR, errinfo_accumulator, __func__, "Invalid mmid(s):");
   g_ptr_array_free(errinfo_accumulator, true);
   return errinfo;
}


STATIC Error_Info *
init_failsim(Parsed_Cmd * parsed_cmd) {
   Error_Info * result = NULL;

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
         result = ERRINFO_NEW(DDCRC_CONFIG_ERROR,
                              "Error loading failure simulation control file %s",
                              parsed_cmd->failsim_control_fn);
      }
   }
#endif

   return result;
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
                         "deferred sleeps: %s, sleep_multiplier: %5.2f",
                         SBOOL(parsed_cmd->flags & CMD_FLAG_DEFER_SLEEPS),
                         parsed_cmd->sleep_multiplier);
   enable_deferred_sleep( parsed_cmd->flags & CMD_FLAG_DEFER_SLEEPS);

#ifdef OLD
   int threshold = DISPLAY_CHECK_ASYNC_NEVER;
   if (parsed_cmd->flags & CMD_FLAG_ASYNC) {
      threshold = DEFAULT_DDC_CHECK_ASYNC_MIN;
      ddc_set_async_threshold(threshold);
   }
   if (parsed_cmd->flags & CMD_FLAG_I3_SET) {
      ddc_set_async_threshold(parsed_cmd->i3);
   }

   if (parsed_cmd->flags & CMD_FLAG_ASYNC_I2C_CHECK)
      i2c_businfo_async_threshold = I2C_BUS_CHECK_ASYNC_MIN;
   else
      i2c_businfo_async_threshold = 999;
#endif

   int threshold = DEFAULT_BUS_CHECK_ASYNC_THRESHOLD;
   if (parsed_cmd->i2c_bus_check_async_min >= 0) {
      threshold = parsed_cmd->i2c_bus_check_async_min;
   }
   i2c_businfo_async_threshold = threshold;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "set i2c_businfo_async_threshold = %d", threshold);

   threshold = DEFAULT_DDC_CHECK_ASYNC_THRESHOLD;
   if (parsed_cmd->ddc_check_async_min >= 0) {
      threshold= parsed_cmd->ddc_check_async_min;
   }
   ddc_set_async_threshold(threshold);
   // DBGMSG("called ddc_set_async_threshold(%d)", threshold);


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
init_display_watch_options(Parsed_Cmd* parsed_cmd) {
   ddc_watch_mode = parsed_cmd->watch_mode;
   enable_watch_displays = parsed_cmd->flags & CMD_FLAG_WATCH_DISPLAY_EVENTS;
   try_get_edid_from_sysfs_first = parsed_cmd->flags & CMD_FLAG_TRY_GET_EDID_FROM_SYSFS;

   if (parsed_cmd->flags2 & CMD_FLAG2_F17)
       use_sysfs_connector_id = false;
    if (parsed_cmd->flags2 & CMD_FLAG2_F18)
       report_udev_events = true;
    force_sysfs_unreliable = parsed_cmd->flags2 & CMD_FLAG2_F21;
    force_sysfs_reliable   = parsed_cmd->flags2 & CMD_FLAG2_F22;

    use_x37_detection_table = !(parsed_cmd->flags2 & CMD_FLAG2_F20);

    if (parsed_cmd->flags2 & CMD_FLAG2_I1_SET)
       extra_stabilization_millisec = parsed_cmd->i1;
    if (parsed_cmd->i7 >= 0 && (parsed_cmd->flags2 & CMD_FLAG2_I7_SET))
       stabilization_poll_millisec = parsed_cmd->i7;
    if (parsed_cmd->i8 >= 0 && (parsed_cmd->flags2 & CMD_FLAG2_I8_SET))
       udev_watch_loop_millisec = parsed_cmd->i8;
    if (parsed_cmd->i9 >= 0 && (parsed_cmd->flags2 & CMD_FLAG2_I9_SET))
       poll_watch_loop_millisec = parsed_cmd->i9;
    if (parsed_cmd->i10 >= 0 && (parsed_cmd->flags2 & CMD_FLAG2_I10_SET))
       xevent_watch_loop_millisec = parsed_cmd->i10;
}


STATIC void
init_experimental_options(Parsed_Cmd* parsed_cmd) {
   suppress_se_post_read = parsed_cmd->flags2 & CMD_FLAG2_F1;
   ddc_never_uses_null_response_for_unsupported = parsed_cmd->flags2 & CMD_FLAG2_F3;
   // ddc_always_uses_null_response_for_unsupported = parsed_cmd->flags2 & CMD_FLAG2_F8;

   if (parsed_cmd->flags2 & CMD_FLAG2_F5)
      EDID_Read_Uses_I2C_Layer = !EDID_Read_Uses_I2C_Layer;
   if (parsed_cmd->flags2 & CMD_FLAG2_F6)
      use_drm_connector_states = true;
   if (parsed_cmd->flags2 & CMD_FLAG2_F7)
      detect_phantom_displays = false;
   if (parsed_cmd->flags2 & CMD_FLAG2_F9)
      msg_to_syslog_only = true;
   if (parsed_cmd->flags2 & CMD_FLAG2_F16)
      msg_to_syslog_only = prefix_report_output = true;

   ddc_enable_displays_cache(parsed_cmd->flags & (CMD_FLAG_ENABLE_CACHED_DISPLAYS)); // was CMD_FLAG_ENABLE_CACHED_DISPLAYS
   if (parsed_cmd->flags2 & CMD_FLAG2_F10)
      null_msg_adjustment_enabled = true;
   if (parsed_cmd->flags2 & CMD_FLAG2_F11)
      monitor_state_tests = true;
   if (parsed_cmd->flags2 & CMD_FLAG2_F14)
      debug_flock = true;

#ifdef TEST_EDID_SMBUS
   if (parsed_cmd->flags & CMD_FLAG_F13)
      EDID_Read_Uses_Smbus = true;
#endif
#ifdef GET_EDID_USING_SYSFS
   if (parsed_cmd->flags2 & CMD_FLAG2_F15)
      verify_sysfs_edid = true;
#endif

   if (parsed_cmd->flags2 & CMD_FLAG2_F19)
      stabilize_added_buses_w_edid = true;
   if (parsed_cmd->flags2 & CMD_FLAG2_F23)
      primitive_sysfs = true;

   if (parsed_cmd->flags2 & CMD_FLAG2_F24)
      enable_write_detect_to_status = true;

   if (parsed_cmd->flags2 & CMD_FLAG2_I2_SET)
        multi_part_null_adjustment_millis = parsed_cmd->i2;
   if (parsed_cmd->flags2 & CMD_FLAG2_I3_SET)
        flock_poll_millisec = parsed_cmd->i3;
   if (parsed_cmd->flags2 & CMD_FLAG2_I4_SET)
        flock_max_wait_millisec = parsed_cmd->i4;
   // if (parsed_cmd->flags & CMD_FLAG_FL1_SET)
   //     dsa2_step_floor = dsa2_multiplier_to_step(parsed_cmd->fl1);
   if (parsed_cmd->flags2 & CMD_FLAG2_I5_SET) {
      if (parsed_cmd->i5 >= 1)
         max_setvcp_verify_tries = parsed_cmd->i5;
      else
         rpt_label(0, "--i5 value must be greater than 1");
   }
}


/** Initialization code common to the standalone program ddcutil and
 *  the shared library libddcutil. Called from both main.c and api.base.c.
 *
 *  @param  parsed_cmd   parsed command
 *  @return ok if initialization succeeded, false if not
 */
Error_Info *
submaster_initializer(Parsed_Cmd * parsed_cmd) {
   assert(tracing_initialized);  // Full tracing services now available
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_DDC, "parsed_cmd = %p", parsed_cmd);
   Error_Info * final_result = NULL;

   redirect_reports_to_syslog = parsed_cmd->flags2 & CMD_FLAG2_F8;

   final_result = init_failsim(parsed_cmd);
   if (final_result)
      goto bye;      // main_rc == EXIT_FAILURE

   final_result = init_disabled_displays(parsed_cmd);

   if (parsed_cmd->flags & CMD_FLAG_NULL_MSG_INDICATES_UNSUPPORTED_FEATURE) {
      DBGMSF(debug, "setting simulate_null_msg_means_unspported = true");
      simulate_null_msg_means_unsupported = true;
   }

   // global variable in dyn_dynamic_features:
   enable_dynamic_features = parsed_cmd->flags & CMD_FLAG_ENABLE_UDF;
   if (parsed_cmd->edid_read_size >= 0)
      EDID_Read_Size = parsed_cmd->edid_read_size;
   if (parsed_cmd->flags & CMD_FLAG_I2C_IO_FILEIO)
      i2c_set_io_strategy_by_id(I2C_IO_STRATEGY_FILEIO);
   if (parsed_cmd->flags & CMD_FLAG_I2C_IO_IOCTL)
      i2c_set_io_strategy_by_id(I2C_IO_STRATEGY_IOCTL);
   i2c_enable_cross_instance_locks(parsed_cmd->flags & CMD_FLAG_FLOCK);

   setvcp_verify_default = parsed_cmd->flags & CMD_FLAG_VERIFY;  // for new threads
   ddc_set_verify_setvcp(setvcp_verify_default);                 // set current thread
   set_output_level(parsed_cmd->output_level);  // current thread
   set_default_thread_output_level(parsed_cmd->output_level); // for future threads
   enable_report_ddc_errors( parsed_cmd->flags & CMD_FLAG_DDCDATA );
   // TMI:
   // if (show_recoverable_errors)
   //    parsed_cmd->stats = true;

   // -------
   char * expected_architectures[] = {"x86_64", "i386", "i686", "armv7l", "aarch64", "ppc64",  NULL};
   char * architecture   = execute_shell_cmd_one_line_result("uname -m");
   // char * distributor_id = execute_shell_cmd_one_line_result("lsb_release -s -i");  // e.g. Ubuntu, Raspbian

      if ( ntsa_find(expected_architectures, architecture) >= 0) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_DDC, "Found a known architecture: %s", architecture);
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_DDC, "Unexpected architecture %s.  Please report.", architecture);
         SYSLOG2(DDCA_SYSLOG_ERROR, "Unexpected architecture %s.", architecture);
      }

     // bool is_raspbian = distributor_id && streq(distributor_id, "Raspbian");
      bool is_arm      = architecture   &&
                           ( str_starts_with(architecture, "arm") ||
                             str_starts_with(architecture, "aarch")
                           );
      free(architecture);
      // free(distributor_id);

      if (is_arm)
         primitive_sysfs = true;

   // ---------

uint64_t t0;
uint64_t t1;
all_video_adapters_implement_drm = false;
#ifdef USE_LIBDRM
   // For each video adapter node in sysfs, check that subdirectories drm/cardN/cardN-xxx exist
   t0 = cur_realtime_nanosec();
   all_video_adapters_implement_drm = check_all_video_adapters_implement_drm();  // in i2c_sysfs.c
   t1 = cur_realtime_nanosec();
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
         "check_all_video_adapters_implement_drm() returned %s in %"PRIu64" microsec",
         sbool(all_video_adapters_implement_drm), NANOS2MICROS(t1-t0));

#ifdef OUT
   // Fails if nvidia driver, adapter path not filled in
   // Gets a list of video adapter paths from the Sys_I2C_Info array and checks if each
   // supports DRM by checking that subdirectories drm/cardN/cardNxxx exist.
   bool result3 = all_sysfs_i2c_info_drm(/*rescan=*/false);  // in i2c_sysfs.c
   DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "all_sysfs_i2c_info_drm() returned %s", sbool(result3));
#endif

   if (parsed_cmd->flags2 & CMD_FLAG2_F12)
      all_video_adapters_implement_drm = false;
#endif

   subinit_i2c_bus_core();

   // rpt_nl();
   // get_sys_drm_connectors(false);  // initializes global sys_drm_connectors

   if (use_drm_connector_states)
      redetect_drm_connector_states();
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "use_drm_connector_states=%s, drm_enabled = %s",
         sbool(use_drm_connector_states), sbool(all_video_adapters_implement_drm));

#ifdef NOT_HERE
  // adding or removing MST device can change whether all drm connectors have connector_id
  all_drm_connectors_have_connector_id = all_sys_drm_connectors_have_connector_id(false);
  bool all2 =                            all_sys_drm_connectors_have_connector_id_direct();
  assert(all2 == all_drm_connectors_have_connector_id);
  DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "all_drm_connectors_have_connector_id = %s",
        SBOOL(all_drm_connectors_have_connector_id));
  // all_drm_connectors_have_connector_id = all_drm_connectors_have_connector_id && (parsed_cmd->flags2 & CMD_FLAG2_F17);
#endif

   init_max_tries(parsed_cmd);
   enable_mock_data = parsed_cmd->flags & CMD_FLAG_MOCK;
   (void) ddc_enable_usb_display_detection( parsed_cmd->flags & CMD_FLAG_ENABLE_USB );

   if (parsed_cmd->flags & CMD_FLAG_DISCARD_CACHES) {
      i2c_discard_caches(parsed_cmd->discarded_cache_types);
   }
   // if (parsed_cmd->flags & CMD_FLAG2_F16)
   //    dbgrpt_sysfs_basic_connector_attributes(1);

   init_performance_options(parsed_cmd);
   enable_capabilities_cache(parsed_cmd->flags & CMD_FLAG_ENABLE_CACHED_CAPABILITIES);
   skip_ddc_checks = parsed_cmd->flags & CMD_FLAG_SKIP_DDC_CHECKS;
#ifdef BUILD_SHARED_LIB
   library_disabled = parsed_cmd->flags & CMD_FLAG_DISABLE_API;
#endif
   init_display_watch_options(parsed_cmd);
   init_experimental_options(parsed_cmd);

bye:
   DBGTRC_RET_ERRINFO(debug, DDCA_TRC_DDC, final_result,  "");
   return final_result;
}


void init_ddc_common_init() {
   RTTI_ADD_FUNC(submaster_initializer);
   RTTI_ADD_FUNC(init_performance_options);
}

