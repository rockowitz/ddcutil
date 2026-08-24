/** @file dbus_util.c
 *
 *  dbus related utilities
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <dbus-1.0/dbus/dbus.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
/** \endcond */

#include "data_structures.h"
#include "debug_util.h"
#include "msg_util.h"
#include "report_util.h"
#include "string_util.h"
#include "timestamp.h"

#include "dbus_util.h"

static GPtrArray * prepare_for_sleep_callbacks = NULL;

void ldbus_register_prepare_for_sleep_callback(PREPARE_FOR_SLEEP_CALLBACK func) {
   generic_register_callback(&prepare_for_sleep_callbacks, func);
}


void ldbus_unregister_prepare_for_sleep_callback(PREPARE_FOR_SLEEP_CALLBACK func) {
   generic_unregister_callback(prepare_for_sleep_callbacks, func);
}


void invoke_prepare_for_sleep_callbacks(bool preparing) {
   if (prepare_for_sleep_callbacks) {
      for (int ndx = 0; ndx < prepare_for_sleep_callbacks->len; ndx++) {
         PREPARE_FOR_SLEEP_CALLBACK func =
               g_ptr_array_index(prepare_for_sleep_callbacks, ndx);
         func(preparing);
      }
   }
}


void dbgrpt_DBusMessage(DBusMessage* msg, int depth) {
   int d0 = depth;
   int d1 = depth+1;
   rpt_vstring(d0,"Message type: %d", dbus_message_get_type(msg));   // SIGNAL, METHOD_CALL, etc
   rpt_vstring(d1,"sender:    %s", dbus_message_get_sender(msg));    // org.freedesktop.login1
   rpt_vstring(d1,"interface: %s", dbus_message_get_interface(msg)); // org.freedesktop.login1.Manager
   rpt_vstring(d1,"member:    %s", dbus_message_get_member(msg));    // PrepareForSleep
   rpt_vstring(d1,"path:      %s", dbus_message_get_path(msg));      // /org/freedesktop/login1
}


void dbus_error_emit(
      DBusError    error,
      const char * funcname,
      int          lineno,
      const char * filename)
{
   char * msg = g_strdup_printf("dbus error %s:%s in %s near line %d in file %s",
         error.name, error.message, funcname, lineno, filename);
   fprintf(stderr, "%s", msg);
   syslog(LOG_ERR, "%s", msg);
   free(msg);
}

#define DBUS_ERROR_EMIT(_error) \
   dbus_error_emit(_error, __func__, __LINE__, __FILE__)


static volatile sig_atomic_t quit_sleep_watch_thread = 0;
// static void sigint_handler(int sig) {
//     quit_sleep_watch_thread = 1;
// }
static GThread * sleep_watch_thread = NULL;

_Atomic uint64_t last_prepare_for_sleep_ns = 0;
_Atomic uint64_t last_resume_from_sleep_ns = 0;

/** PrepareForSleep(true) timestamp of a sleep cycle that has been retired,
 *  i.e. one whose matching PrepareForSleep(false) is no longer expected.
 *  Written only by the sleep watch thread.  See ldbus_in_open_sleep_cycle().
 */
_Atomic uint64_t retired_prepare_for_sleep_ns = 0;

/** Poll interval of the sleep watch thread's dispatch loop.
 */
#define SLEEP_WATCH_LOOP_TIMEOUT_MS 500

/** A gap between loop iterations longer than this is time the process was not
 *  running -- frozen for suspend -- rather than time it spent in the loop.
 *  Several loop iterations, so that ordinary scheduling delay on a loaded
 *  system is not read as a freeze.
 */
#define SLEEP_WATCH_FREEZE_GAP_MS (4 * SLEEP_WATCH_LOOP_TIMEOUT_MS)

/** An open sleep cycle is retired once the sleep watch thread has been
 *  *running* this long since the PrepareForSleep(true) signal without the
 *  matching PrepareForSleep(false) having arrived.  Well beyond any plausible
 *  interval between the signal and the freeze, which logind bounds by
 *  InhibitDelayMaxSec, 5 seconds by default.
 */
#define OPEN_SLEEP_CYCLE_MAX_RUNNING_MS 60000


/** Seeds the resume timestamps with the current time at sleep-watch thread
 *  startup, deliberately treating program start like a resume from sleep.
 *  The window just after boot/login has the same transient EACCES race on
 *  /dev/i2c-N opens as the window just after resume (udev rules apply device
 *  permissions shortly after the nodes appear), so the first opens get the
 *  same settling pause from recently_resumed_from_sleep().
 *
 *  Both timestamps are set to the same value, so that no sleep cycle is open
 *  at program start.  See ldbus_in_open_sleep_cycle().
 *
 *  @remark
 *  This is the reason the dbus method cannot simply be replaced by the
 *  clocktime resume detector, which reports nothing at program start
 *  because no sleep has accumulated.  See recently_resumed_from_sleep().
 */
void ldbus_elapsed_since_resume_from_sleep_mark_start() {
   bool debug = false;

   last_resume_from_sleep_ns = last_prepare_for_sleep_ns = cur_boot_time_nanosec();

   DBGF(debug, "Executed.  set last_resume_from_sleep_ns = last_prepare_for_sleep_ns =%"PRIu64,
         last_resume_from_sleep_ns);
}


/** Returns the number of nanoseconds since the most recent return from
 *  sleep, detected via D-Bus.
 *
 *  If no resume has occurred, the reference point is the start of the
 *  sleep-watch thread, per ldbus_elapsed_since_resume_from_sleep_mark_start(),
 *  so program startup is intentionally reported like a recent resume.
 *
 *  @return nanoseconds since last resume (or since sleep-watch thread start)
 */
uint64_t ldbus_elapsed_since_resume_from_sleep_ns() {
   bool debug = false;

   uint64_t elapsed_ns = cur_boot_time_nanosec() - last_resume_from_sleep_ns;

   //  elapsed_ns = MILLIS2NANOS(400);   // *** TEST ***
   DBGF(debug,
         "last_resume_from_sleep_ns=%"PRIu64", Returning %"PRIu64" ns = %"PRIu64" ms",
         last_resume_from_sleep_ns, elapsed_ns, NANOS2MILLIS(elapsed_ns));
   return elapsed_ns;
}


/** Reports whether this process is inside a sleep cycle that is still open,
 *  and how long ago that cycle began.
 *
 *  logind emits **PrepareForSleep(true)** before suspending and
 *  **PrepareForSleep(false)** after the suspend attempt returns, successful or
 *  not.  A prepare signal that the matching resume signal has not closed opens
 *  a cycle.  Program start is not an open cycle:
 *  ldbus_elapsed_since_resume_from_sleep_mark_start() sets both timestamps to
 *  the same value.
 *
 *  A cycle that is never closed, the connection to the bus having been lost
 *  between the two signals, would otherwise stay open for the life of the
 *  process, and every caller of recently_resumed_from_sleep() would pause
 *  indefinitely.  The sleep watch thread retires such a cycle, and the
 *  retirement is permanent: only a new prepare signal opens another.
 *
 *  What the thread measures is its own running time, not elapsed time, and
 *  the distinction is the whole point.  Elapsed time since the prepare signal
 *  spans the suspend, so it cannot tell a cycle whose closing signal was lost
 *  from one whose system simply slept for an hour.  Time in which the thread
 *  did not run is time this process was frozen, and does not count.  See
 *  ldbus_watch_sleep_events_thread().
 *
 *  @param  elapsed_ns_loc  if non-NULL, set to the nanoseconds since an
 *          unmatched PrepareForSleep(true), UINT64_MAX if there is none.  Set
 *          whether or not the cycle is still open, so that one call, and so
 *          one sample of the state, serves both a decision and a report.
 *  @return true if a sleep cycle is open, false if none is or the open one
 *          has been retired
 *
 *  @remark
 *  Known limit: a system holding the suspend inhibited for longer than
 *  OPEN_SLEEP_CYCLE_MAX_RUNNING_MS retires the cycle before the freeze, and it
 *  stays retired afterward.  Detection then falls back to the clock method and
 *  the resume signal, which is where it stood before open cycles were
 *  consulted at all.
 */
bool ldbus_in_open_sleep_cycle(uint64_t * elapsed_ns_loc) {
   bool debug = false;

   // Read the resume timestamp first and the prepare timestamp last, so that a
   // signal recorded between the reads resolves toward reporting the cycle
   // open.  That is the safe direction for a detector whose callers pause on a
   // true: a resume so missed occurred this very instant, and a prepare so
   // caught opens the cycle a moment early.  Reading them the other way round
   // would report the cycle closed in the first case and miss it entirely in
   // the second, both of which under-report.
   uint64_t resume_ns   = last_resume_from_sleep_ns;
   uint64_t retired_ns  = retired_prepare_for_sleep_ns;
   uint64_t prepare_ns  = last_prepare_for_sleep_ns;

   bool unmatched = prepare_ns > resume_ns;
   // Timestamps are CLOCK_BOOTTIME nanoseconds, so a retired cycle cannot be
   // confused with the next one to open.
   bool open      = unmatched && prepare_ns != retired_ns;

   if (elapsed_ns_loc)
      // Sampled after prepare_ns was read, and both are CLOCK_BOOTTIME, so the
      // subtraction cannot wrap.
      *elapsed_ns_loc = (unmatched) ? cur_boot_time_nanosec() - prepare_ns
                                    : UINT64_MAX;

   DBGF(debug, "last_prepare_for_sleep_ns=%"PRIu64", last_resume_from_sleep_ns=%"PRIu64
               ", retired_prepare_for_sleep_ns=%"PRIu64", unmatched=%s, Returning %s",
               prepare_ns, resume_ns, retired_ns, sbool(unmatched), sbool(open));
   return open;
}


#ifdef UNUSED
/** If the elapsed time since the most recent return from sleep occurred
 *  is less than the specified value, sleep for the time remaining until
 *  the specified time value has elapsed.
 *
 *  @param  minimum_ms
 *  @return number of milliseconds slept
 *
 *  @remark
 *  Superseded by recently_resumed_from_sleep() in linux_util.c, which
 *  combines this dbus timestamp with the clocktime resume detector and
 *  leaves the pause to the caller.  The dbus contribution is now
 *  ldbus_elapsed_since_resume_from_sleep_ns().
 */
int ldbus_pause_if_recent_return_from_sleep(int minimum_ms) {
   bool debug = false;

   uint64_t elapsed_ns = ldbus_elapsed_since_resume_from_sleep_ns();
   uint64_t elapsed_ms = NANOS2MILLIS(elapsed_ns);

   char prefix[200];
   get_msg_decoration(prefix, 200, /*dest_syslog*/ true);

   if (debug) {
      char * msg = g_strdup_printf(
                    "Time since last return from sleep = %"PRIu64" ns = %"PRIu64" ms",
                    elapsed_ns, elapsed_ms);
      DBG("%s", msg);
      syslog(LOG_WARNING, "%s(%s)%s", prefix, __func__, msg);
      free(msg);
   }

   uint64_t remaining_ms = 0;
   if (elapsed_ms < minimum_ms) {
      remaining_ms = minimum_ms - elapsed_ms;
      char * msg2 = g_strdup_printf("Pausing for %"PRIu64" ms", remaining_ms);
      syslog(LOG_NOTICE, "%s(%s)%s", prefix, __func__, msg2);
      DBGF(debug,"%s", msg2);
      usleep(MILLIS2MICROS(remaining_ms));
      free(msg2);
   }

   DBGF(debug, "Done.   Returning: %"PRIu64" millisec", remaining_ms);
   return remaining_ms;
}
#endif


static DBusHandlerResult
ldbus_handle_message(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
   bool debug = false;
   DBGF(debug,"Starting.");
   DBusError err;
   dbus_error_init(&err);
   if (debug)
      dbgrpt_DBusMessage(msg, 1);
   if (dbus_message_is_signal(msg, "org.freedesktop.login1.Manager", "PrepareForSleep")) {
       dbus_bool_t preparing;
       if (dbus_message_get_args(
              msg,
              &err,
              DBUS_TYPE_BOOLEAN, &preparing,
              DBUS_TYPE_INVALID)) {
          DBGF(debug, "PrepareForSleep: %s", preparing ? "true (prepare)" : "false (resume)");
          // The two timestamps also bracket the sleep cycle: recording the
          // prepare opens it, recording the resume closes it.  See
          // ldbus_in_open_sleep_cycle().
          uint64_t mono_ns = cur_boot_time_nanosec();
          if (preparing)
             last_prepare_for_sleep_ns = mono_ns;
          else
             last_resume_from_sleep_ns = mono_ns;
          char prefix[200];
          get_msg_decoration(prefix, 200, /*dest_syslog*/ true);
          char * s1 = g_strdup_printf(
                "%s(%s)Received dbus signal PrepareForSleep(%s)",
                prefix, __func__, (preparing) ? "true=prepare" : "false=resume" );
          char * s = g_strdup_printf(
                  "Set %s = %"PRIu64" millisec", (preparing) ? "true (prepare)" : "false (resume)",
                  NANOS2MILLIS(mono_ns));
          DBGF(debug, "%s", s);
          syslog(LOG_INFO, "%s", s1);  // violates layering, should really use callback funct
          free(s);
          free(s1);
          invoke_prepare_for_sleep_callbacks(preparing);
       }
       else {
          DBUS_ERROR_EMIT(err);
          dbus_error_free(&err);
       }
       fflush(stdout);
    }
    else
       DBGF(debug,"Not for us");

    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    DBGF(debug,"Done.  returning %d", result);
    return result;
}


typedef struct {
   DBusConnection * conn;
} Dbus_Connection_Data;


gpointer ldbus_watch_sleep_events_thread(gpointer data) {
   bool debug = false;

   Dbus_Connection_Data * dcd = (Dbus_Connection_Data*) data;
   DBGF(debug,"Listening for PrepareForSleep...");

   ldbus_elapsed_since_resume_from_sleep_mark_start();

   // Running time accounting for an open sleep cycle.  This loop is the only
   // part of the library that knows the process is running: it completes an
   // iteration at least every SLEEP_WATCH_LOOP_TIMEOUT_MS while running, and
   // completes none at all while frozen for suspend.  The state is local, the
   // loop being its only reader and writer; what other threads consult is the
   // verdict it publishes in retired_prepare_for_sleep_ns.
   uint64_t prev_iteration_ns = cur_boot_time_nanosec();
   uint64_t running_ns        = 0;                          // since counted_prepare_ns
   uint64_t counted_prepare_ns = last_prepare_for_sleep_ns;

   while (!quit_sleep_watch_thread) {
       uint64_t now_ns = cur_boot_time_nanosec();
       uint64_t gap_ns = now_ns - prev_iteration_ns;
       prev_iteration_ns = now_ns;

       if (gap_ns > MILLIS2NANOS(SLEEP_WATCH_FREEZE_GAP_MS))
          // The loop did not run for far longer than its own timeout, so the
          // process was frozen rather than running.  The gap is not running
          // time, and the allowance starts over on the far side, so that time
          // spent running before the freeze does not eat the window after it.
          running_ns = 0;
       else
          running_ns += gap_ns;

       uint64_t prepare_ns = last_prepare_for_sleep_ns;
       if (prepare_ns != counted_prepare_ns) {   // a cycle opened or closed
          counted_prepare_ns = prepare_ns;
          running_ns = 0;
       }
       else if (running_ns > MILLIS2NANOS(OPEN_SLEEP_CYCLE_MAX_RUNNING_MS) &&
                prepare_ns > last_resume_from_sleep_ns)
       {
          // Open across more running time than any suspend cycle takes to
          // reach the freeze: the closing signal is not coming.  Retiring is
          // permanent for this cycle, since the next one carries a later
          // prepare timestamp.
          retired_prepare_for_sleep_ns = prepare_ns;
       }

       dbus_connection_read_write_dispatch(dcd->conn, SLEEP_WATCH_LOOP_TIMEOUT_MS);
   }

   // Nothing will be left to receive the matching PrepareForSleep(false), so
   // an open cycle could never close.  Retire it rather than move the resume
   // timestamp forward, which would report a resume that never occurred.
   uint64_t final_prepare_ns = last_prepare_for_sleep_ns;
   if (final_prepare_ns > last_resume_from_sleep_ns)
      retired_prepare_for_sleep_ns = final_prepare_ns;

   dbus_connection_remove_filter(dcd->conn, ldbus_handle_message, NULL);
   dbus_connection_unref(dcd->conn);
   DBGF(debug,"Done listening");
   return data;
}


bool ldbus_start_sleep_watch_thread() {
   bool debug = false;
   DBGF(debug, "Starting");

   DBusError err;
   // signal(SIGINT, sigint_handler);

   bool ok = true;
   dbus_error_init(&err);

   DBusConnection *conn= dbus_bus_get(DBUS_BUS_SYSTEM, &err);
   if (dbus_error_is_set(&err)) {
      DBUS_ERROR_EMIT(err);
      dbus_error_free(&err);
      ok = false;
      goto bye;
   }

   dbus_connection_set_exit_on_disconnect(conn, FALSE);
#ifdef OUT
   const char * rule1 =
         "type='signal',"
         "sender='org.freedesktop.login1',"     // does not match
         "interface='org.freedesktop.login1.Manager',"
         "member='PrepareForSleep'";
#endif
   const char * rule3 =
         "type='signal',"
         "interface='org.freedesktop.login1.Manager',"
         "member='PrepareForSleep'";

   dbus_bus_add_match(conn, rule3, &err);
   if (dbus_error_is_set(&err)) {
      DBUS_ERROR_EMIT(err);
      dbus_error_free(&err);
      ok = false;
      goto bye;
   }

   if (!dbus_connection_add_filter(conn, ldbus_handle_message, NULL, NULL)) {
      ok = false;
      goto bye;
   }

   Dbus_Connection_Data * dcd = calloc(1,sizeof(Dbus_Connection_Data));
   dbus_connection_ref(conn);   // thread holds its own reference
   dcd->conn = conn;
   GThreadFunc watch_thread_func = ldbus_watch_sleep_events_thread;
   quit_sleep_watch_thread = false;
   sleep_watch_thread = g_thread_new("sleep_watch_thread",   // optional thread name
                                 watch_thread_func,
                                 dcd);
   DBGF(debug, "Started sleep watch thread at %p", sleep_watch_thread);

bye:
   if (conn)
      dbus_connection_unref(conn);
   DBGF(debug, "Done.  Returning %s", SBOOL(ok));
   return ok;
}


void ldbus_stop_sleep_watch_thread() {
   bool debug = false;
   DBGF(debug, "Starting...");

   quit_sleep_watch_thread = true;
   if (sleep_watch_thread) {
      Dbus_Connection_Data * dcd = g_thread_join(sleep_watch_thread);
      free(dcd);
      sleep_watch_thread = NULL;
   }

   DBGF(debug, "Done");
}

