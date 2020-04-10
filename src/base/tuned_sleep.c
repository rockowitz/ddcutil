/** @file tuned_sleep.c
 *
 *  Perform sleep. The sleep time is determined by io mode, sleep event time,
 *  and applicable multipliers.
 */

// Copyright (C) 2019-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <sys/types.h>

// for syscall
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#include "util/debug_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/parms.h"
#include "base/dynamic_sleep.h"
#include "base/execution_stats.h"
#include "base/sleep.h"
#include "base/thread_sleep_data.h"

// Experimental suppression of sleeps after reads
static bool sleep_suppression_enabled = false;

void enable_sleep_suppression(bool enable) {
   sleep_suppression_enabled = enable;
}

/* Two multipliers are applied to the sleep time determined from the
 * io mode and event type.
 *
 * sleep_multiplier_factor: set globally, e.g. from arg passed on
 * command line.  Consider making thread specific.
 *
 * sleep_multiplier_ct: Per thread adjustment,initiated by io retries.
 */


//
// Perform sleep
//

/** Sleep for the period of time required by the DDC protocol, as indicated
 *  by the io mode and sleep event type.
 *
 *  The time is further adjusted by the sleep factor and sleep multiplier
 *  currently in effect.
 *
 *  \todo
 *  Take into account the time since the last monitor return in the
 *  current thread.
 *  \tod
 *  Take into account per-display error statistics.  Would require
 *  error statistics be maintained on a per-display basis, either
 *  in the display reference or display handle.
 *
 * \param io_mode     communication mechanism
 * \param event_type  reason for sleep
 * \param func        name of function that invoked sleep
 * \param lineno      line number in file where sleep was invoked
 * \param filename    name of file from which sleep was invoked
 * \param msg         text to append to trace message
 */
void tuned_sleep_with_tracex(
      // DDCA_IO_Mode     io_mode,
      Display_Handle * dh,
      Sleep_Event_Type event_type,
      int              special_sleep_time_millis,
      const char *     func,
      int              lineno,
      const char *     filename,
      const char *     msg)
{
   bool debug = true;
   DBGMSF(debug, "Starting. Sleep event type = %s, dh=%s", sleep_event_name(event_type), dh_repr_t(dh));
   assert( (event_type != SE_SPECIAL && special_sleep_time_millis == 0) ||
           (event_type == SE_SPECIAL && special_sleep_time_millis >  0) );

   DDCA_IO_Mode io_mode = dh->dref->io_path.io_mode;

   int spec_sleep_time_millis = 0;    // should be a default
   bool deferrable_sleep = false;

   if (event_type == SE_SPECIAL)
      spec_sleep_time_millis = special_sleep_time_millis;
   else {

      switch(io_mode) {

      case DDCA_IO_I2C:
         switch(event_type) {
         case (SE_WRITE_TO_READ):
               spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_BETWEEN_GETVCP_WRITE_READ;
               break;
         case (SE_POST_WRITE):  // post SET VCP FEATURE write, between SET TABLE write fragments, after final?
               spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_POST_NORMAL_COMMAND;
               deferrable_sleep = true;
               break;
         case (SE_POST_OPEN):   // needed?
               spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
               deferrable_sleep = true;   // ??
               break;
         case (SE_POST_READ):
               deferrable_sleep = true;
               spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_POST_NORMAL_COMMAND;
               if (sleep_suppression_enabled) {
                  DBGMSF(debug, "Suppressing sleep, sleep event type = %s", sleep_event_name(event_type));
                  return;  // TEMP
               }
               break;
         case (SE_POST_SAVE_SETTINGS):
               deferrable_sleep = true;
               spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_POST_SAVE_SETTINGS;   // per DDC spec
               break;
         case SE_DDC_NULL:
              spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_NULL_RESPONSE_INCREMENT;
              break;
         case SE_PRE_EDID:
              spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
              if (sleep_suppression_enabled) {
              DBGMSF(debug, "Suppressing sleep, sleep event type = %s",
                            sleep_event_name(event_type));
              return;   // TEMP
              }
              break;
         case SE_OTHER:
              spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
              // if (sleep_suppression_enabled) {
              // DBGMSF(debug, "Suppressing sleep, sleep event type = %s",
              //               sleep_event_name(event_type));
              // return;
              // }
              break;
         case SE_PRE_MULTI_PART_READ:        // before reading capabilitis
            spec_sleep_time_millis = 200;
            break;
         case SE_MULTI_PART_READ_TO_WRITE:   // unused
            spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
            break;
         case SE_BETWEEN_CAP_TABLE_SEGMENTS:
            spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_BETWEEN_CAP_TABLE_FRAGMENTS;
            break;
         case SE_POST_CAP_TABLE_COMMAND:
            deferrable_sleep = true;
            spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_POST_CAP_TABLE_COMMAND;
            break;
         default:
              spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
         }  // switch within DDC_IO_DEVI2C
         break;

      case DDCA_IO_ADL:
         switch(event_type) {
         case (SE_WRITE_TO_READ):
               spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
               break;
         case (SE_POST_WRITE):
               spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
               break;
         case (SE_POST_OPEN):
               spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
               break;
         case (SE_POST_SAVE_SETTINGS):
               spec_sleep_time_millis = 200;   // per DDC spec
               break;
         default:
            spec_sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
         }
         break;

      case DDCA_IO_USB:
         PROGRAM_LOGIC_ERROR("call_tuned_sleep() called for USB_IO\n");
         break;
      }
   }


   DBGMSF(debug, "deferrable_sleep=%s", sbool(deferrable_sleep));   // avoid unused-but-set-variable warning

   // TODO:
   //   get error rate (total calls, total errors), current adjustment value
   //   adjust by time since last i2c event

   double dynamic_sleep_adjustment_factor = dsa_get_sleep_adjustment();

   // DBGMSG("Calling tsd_get_sleep_multiplier_factor()");
   double sleep_multiplier_factor = tsd_get_sleep_multiplier_factor();
   // DBGMSG("sleep_multiplier_factor = %5.2f", sleep_multiplier_factor);
   // crude, should be sensitive to event type?
   int sleep_multiplier_ct = tsd_get_sleep_multiplier_ct();  // per thread
   double adjusted_sleep_time_millis = sleep_multiplier_ct * sleep_multiplier_factor *
                                       spec_sleep_time_millis * dynamic_sleep_adjustment_factor;
   if (debug) {
      DBGMSG("Before sleep. event type: %s, sleep_multiplier_ct = %d,"
             " sleep_multiplier_factor = %9.1f, dynamic_sleep_adjustment_factor = %9.1f,"
             " sleep_time_millis = %d -> modified_sleep_time_millis=%5.2f",
             sleep_event_name(event_type), sleep_multiplier_ct,
             sleep_multiplier_factor, dynamic_sleep_adjustment_factor,
             spec_sleep_time_millis, adjusted_sleep_time_millis);
   }

   record_sleep_event(event_type);

   char msg_buf[100];
   const char * evname = sleep_event_name(event_type);
   if (msg)
      g_snprintf(msg_buf, 100, "Event type: %s, %s", evname, msg);
   else
      g_snprintf(msg_buf, 100, "Event_type: %s", evname);

   if (deferrable_sleep) {
      uint64_t new_deferred_time = cur_realtime_nanosec() + (1000 *1000) * (int) adjusted_sleep_time_millis;
      if (new_deferred_time > dh->dref->next_i2c_io_after)
         dh->dref->next_i2c_io_after = new_deferred_time;
   }
   // else {    // FUTURE
   sleep_millis_with_tracex(adjusted_sleep_time_millis, func, lineno, filename, msg_buf);
   // }

   DBGMSF(debug, "Done");
}


void deferred_sleep(Display_Handle * dh, const char * func, int lineno, const char * filename) {
   bool debug = true;
   uint64_t curtime = cur_realtime_nanosec();
   DBGMSF(debug, "curtime=%"PRIu64", next_i2c_io_after=%"PRIu64,
                 curtime, dh->dref->next_i2c_io_after);
   if (dh->dref->next_i2c_io_after > curtime) {
      int sleep_time = (dh->dref->next_i2c_io_after - curtime)/ (1000*1000);
      DBGMSF(debug, "sleep_time=%d", sleep_time);
      sleep_millis_with_tracex(sleep_time, func, lineno, filename, "deferred");
   }
}



