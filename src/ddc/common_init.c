/** \file common_init.c */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// Contains initialization functions extracted from main.c so they can
// be shared with libmain/api.base.c

#include <assert.h>
#include <stdbool.h>

#include "config.h"

#include "util/string_util.h"

#include "base/core.h"
#include "base/parms.h"
#include "base/thread_retry_data.h"
#include "base/thread_sleep_data.h"
#include "base/tuned_sleep.h"

#include "vcp/persistent_capabilities.h"

#include "dynvcp/dyn_feature_files.h"

#include "i2c/i2c_execute.h"
#include "i2c/i2c_strategy_dispatcher.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_services.h"
#include "ddc/ddc_try_stats.h"
#include "ddc/ddc_vcp.h"

#include "ddc/common_init.h"


void
init_tracing(Parsed_Cmd * parsed_cmd)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting.\n",__func__);
      
   if (parsed_cmd->flags & CMD_FLAG_TIMESTAMP_TRACE)     // timestamps on debug and trace messages?
       dbgtrc_show_time = true;                           // extern in core.h
    if (parsed_cmd->flags & CMD_FLAG_THREAD_ID_TRACE)     // timestamps on debug and trace messages?
       dbgtrc_show_thread_id = true;                      // extern in core.h
    report_freed_exceptions = parsed_cmd->flags & CMD_FLAG_REPORT_FREED_EXCP;   // extern in core.h
    set_trace_levels(parsed_cmd->traced_groups);
    // if (parsed_cmd->s1)
    //    set_trace_destination(parsed_cmd->s1, parser_mode_name(parsed_cmd->parser_mode));
    if (parsed_cmd->traced_functions) {
       for (int ndx = 0; ndx < ntsa_length(parsed_cmd->traced_functions); ndx++) {
          if (debug)
                printf("(%s) Adding traced function: %s\n",
                       __func__, parsed_cmd->traced_functions[ndx]);
          add_traced_function(parsed_cmd->traced_functions[ndx]);
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
    
    if (debug)
       printf("(%s) Done.\n", __func__);
}


bool init_failsim(Parsed_Cmd * parsed_cmd) {
#ifdef ENABLE_FAILSIM
   fsim_set_name_to_number_funcs(
         status_name_to_modulated_number,
         status_name_to_unmodulated_number);
   if (parsed_cmd->failsim_control_fn) {
      bool ok = fsim_load_control_file(parsed_cmd->failsim_control_fn);
      if (!ok) {
         fprintf(stderr, "Error loading failure simulation control file %s.\n",
                         parsed_cmd->failsim_control_fn);
         return false;

      }
      fsim_report_error_table(0);
   }
#endif
   return true;
}


void init_max_tries(Parsed_Cmd * parsed_cmd)
{
   // n. MAX_MAX_TRIES checked during command line parsing
   if (parsed_cmd->max_tries[0] > 0) {
      // ddc_set_max_write_only_exchange_tries(parsed_cmd->max_tries[0]);  // sets in Try_Data
      // try_data_set_maxtries2(WRITE_ONLY_TRIES_OP, parsed_cmd->max_tries[0]);
      try_data_init_retry_type(WRITE_ONLY_TRIES_OP, parsed_cmd->max_tries[0]);  // resets highest, lowest

      // redundant
      trd_set_default_max_tries(0, parsed_cmd->max_tries[0]);
      trd_set_initial_thread_max_tries(0, parsed_cmd->max_tries[0]);
   }

   if (parsed_cmd->max_tries[1] > 0) {
      // ddc_set_max_write_read_exchange_tries(parsed_cmd->max_tries[1]);   // sets in Try_Data
      // try_data_set_maxtries2(WRITE_READ_TRIES_OP, parsed_cmd->max_tries[1]);
      try_data_init_retry_type(WRITE_READ_TRIES_OP, parsed_cmd->max_tries[1]);

      trd_set_default_max_tries(1, parsed_cmd->max_tries[1]);
      trd_set_initial_thread_max_tries(1, parsed_cmd->max_tries[1]);
   }

   if (parsed_cmd->max_tries[2] > 0) {
      // ddc_set_max_multi_part_read_tries(parsed_cmd->max_tries[2]);       // sets in Try_Data
      // ddc_set_max_multi_part_write_tries(parsed_cmd->max_tries[2]);
      // try_data_set_maxtries2(MULTI_PART_READ_OP,  parsed_cmd->max_tries[2]);
      // try_data_set_maxtries2(MULTI_PART_WRITE_OP, parsed_cmd->max_tries[2]);
      try_data_init_retry_type(MULTI_PART_READ_OP,  parsed_cmd->max_tries[2]);
      try_data_init_retry_type(MULTI_PART_WRITE_OP, parsed_cmd->max_tries[2]);

      trd_set_default_max_tries(2, parsed_cmd->max_tries[2]);
      trd_set_initial_thread_max_tries(2, parsed_cmd->max_tries[2]);
      // impedance match
      trd_set_default_max_tries(3, parsed_cmd->max_tries[2]);
      trd_set_initial_thread_max_tries(3, parsed_cmd->max_tries[2]);
   }
}


void init_performance_options(Parsed_Cmd * parsed_cmd)
{
   enable_sleep_suppression( parsed_cmd->flags & CMD_FLAG_REDUCE_SLEEPS );
   enable_deferred_sleep( parsed_cmd->flags & CMD_FLAG_DEFER_SLEEPS);

   int threshold = DISPLAY_CHECK_ASYNC_NEVER;
   if (parsed_cmd->flags & CMD_FLAG_ASYNC) {
      threshold = DISPLAY_CHECK_ASYNC_THRESHOLD_STANDARD;
      ddc_set_async_threshold(threshold);
   }

   if (parsed_cmd->sleep_multiplier != 0 && parsed_cmd->sleep_multiplier != 1) {
      tsd_set_sleep_multiplier_factor(parsed_cmd->sleep_multiplier);         // for current thread
      tsd_set_default_sleep_multiplier_factor(parsed_cmd->sleep_multiplier); // for new threads
      if (parsed_cmd->sleep_multiplier > 1.0f && (parsed_cmd->flags & CMD_FLAG_DSA) )
      {
         tsd_dsa_enable_globally(true);
      }
   }

   // experimental timeout of i2c read()
   if (parsed_cmd->flags & CMD_FLAG_TIMEOUT_I2C_IO) {
      set_i2c_fileio_use_timeout(true);
   }
}


bool
submaster_initializer(Parsed_Cmd * parsed_cmd) {
   bool debug = false;
   bool ok = true;
   DBGMSF(debug, "Starting. parsed_cmd = %p", parsed_cmd);

   if (!init_failsim(parsed_cmd))
      goto bye;      // main_rc == EXIT_FAILURE

   // global variable in dyn_dynamic_features:
   enable_dynamic_features = parsed_cmd->flags & CMD_FLAG_ENABLE_UDF;

   if (parsed_cmd->edid_read_size >= 0)
      EDID_Read_Size = parsed_cmd->edid_read_size;

    init_ddc_services();   // n. initializes start timestamp
    // overrides setting in init_ddc_services():
    i2c_set_io_strategy(DEFAULT_I2C_IO_STRATEGY);
    ddc_set_verify_setvcp(parsed_cmd->flags & CMD_FLAG_VERIFY);

    set_output_level(parsed_cmd->output_level);
    enable_report_ddc_errors( parsed_cmd->flags & CMD_FLAG_DDCDATA );
    // TMI:
    // if (show_recoverable_errors)
    //    parsed_cmd->stats = true;

    init_max_tries(parsed_cmd);

 #ifdef USE_USB
       DDCA_Status rc = ddc_enable_usb_display_detection( parsed_cmd->flags & CMD_FLAG_ENABLE_USB );
       assert (rc == DDCRC_OK);
 #endif

    init_performance_options(parsed_cmd);

    enable_capabilities_cache(parsed_cmd->flags & CMD_FLAG_ENABLE_CACHED_CAPABILITIES);

   ok = true;

bye:
   return ok;
}

