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

/** Poll interval of the sleep watch thread's dispatch loop, and so the
 *  interval at which it records its heartbeat.
 */
#define SLEEP_WATCH_LOOP_TIMEOUT_MS 500

/** CLOCK_BOOTTIME at the top of the sleep watch thread's most recent dispatch
 *  loop iteration, 0 before the thread first runs.
 *
 *  The thread completes an iteration at least every
 *  SLEEP_WATCH_LOOP_TIMEOUT_MS while the process is running, and completes
 *  none at all while the process is frozen, so the age of this timestamp
 *  distinguishes the two.  That is the only way to tell that a suspend
 *  stopped this process, given that a suspend need not leave any trace in the
 *  clocks.  See ldbus_in_open_sleep_cycle().
 */
_Atomic uint64_t sleep_watch_heartbeat_ns = 0;

/** A heartbeat older than this means the process was not running until a
 *  moment ago.  Several loop iterations, so that ordinary scheduling delay on
 *  a loaded system is not read as a freeze.
 */
#define SLEEP_WATCH_FREEZE_EVIDENCE_MS (4 * SLEEP_WATCH_LOOP_TIMEOUT_MS)

/** An open sleep cycle is abandoned once the sleep watch thread has been seen
 *  running this long since the PrepareForSleep(true) signal without the
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
 *  at program start.  See ldbus_elapsed_since_pending_prepare_for_sleep_ns().
 *  The heartbeat is seeded here as well: this function is called by the sleep
 *  watch thread as it begins, which is exactly what the heartbeat records.
 *
 *  @remark
 *  This is the reason the dbus method cannot simply be replaced by the
 *  clocktime resume detector, which reports nothing at program start
 *  because no sleep has accumulated.  See recently_resumed_from_sleep().
 */
void ldbus_elapsed_since_resume_from_sleep_mark_start() {
   bool debug = false;

   last_resume_from_sleep_ns = last_prepare_for_sleep_ns =
         sleep_watch_heartbeat_ns = cur_boot_time_nanosec();

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


/** Returns the number of nanoseconds since a **PrepareForSleep(true)** signal
 *  that has not yet been matched by a **PrepareForSleep(false)**, i.e. since
 *  the start of a sleep cycle that is still open.
 *
 *  logind emits PrepareForSleep(true) before suspending and
 *  PrepareForSleep(false) after the suspend attempt returns, successful or
 *  not, so the later of the two timestamps says whether a cycle is open.
 *  Program start is not an open cycle:
 *  ldbus_elapsed_since_resume_from_sleep_mark_start() sets both to the same
 *  value.
 *
 *  Note that the elapsed time is measured from the start of the cycle, not
 *  from a resume.  For a cycle that is still open it spans however long the
 *  system was suspended.  It is a diagnostic; what the resume detector uses
 *  is only whether a cycle is open.  See recently_resumed_from_sleep().
 *
 *  @return nanoseconds since the unmatched PrepareForSleep(true),
 *          UINT64_MAX if no sleep cycle is open
 */
uint64_t ldbus_elapsed_since_pending_prepare_for_sleep_ns() {
   bool debug = false;

   // Read the prepare timestamp first.  Should PrepareForSleep(false) be
   // recorded between the two reads, the resume value read here is the older
   // one and the cycle is reported as still open.  That is the safe
   // direction: the resume so missed occurred this very instant.
   uint64_t prepare_ns = last_prepare_for_sleep_ns;
   uint64_t resume_ns  = last_resume_from_sleep_ns;

   uint64_t elapsed_ns = UINT64_MAX;
   if (prepare_ns > resume_ns)
      // Sampled after prepare_ns was read, and both are CLOCK_BOOTTIME,
      // so the subtraction cannot wrap.
      elapsed_ns = cur_boot_time_nanosec() - prepare_ns;

   DBGF(debug, "last_prepare_for_sleep_ns=%"PRIu64", last_resume_from_sleep_ns=%"PRIu64
               ", Returning %"PRIu64" ns",
               prepare_ns, resume_ns, elapsed_ns);
   return elapsed_ns;
}


/** Reports whether this process is inside a sleep cycle that is still open,
 *  i.e. whether a **PrepareForSleep(true)** signal is outstanding and is still
 *  to be believed.
 *
 *  An open cycle is closed by its matching **PrepareForSleep(false)**.  Were
 *  that signal never to arrive -- the connection to the bus lost between the
 *  two -- the cycle would otherwise stay open for the life of the process, and
 *  every caller of recently_resumed_from_sleep() would pause indefinitely.
 *  The sleep watch thread's heartbeat bounds that: an open cycle is abandoned
 *  once the thread has been observed running OPEN_SLEEP_CYCLE_MAX_RUNNING_MS
 *  since the prepare signal.
 *
 *  Observed running time is the right measure, rather than elapsed time, and
 *  it is what makes the heartbeat necessary.  Elapsed time since the prepare
 *  signal spans the suspend, so it cannot distinguish a cycle that is stuck
 *  from one whose system simply slept for an hour.  The heartbeat does not
 *  advance while the process is frozen, since the thread writing it is frozen
 *  too, so a suspend of any length leaves it at its pre-freeze value and the
 *  cycle is still honored on the far side.
 *
 *  A heartbeat older than SLEEP_WATCH_FREEZE_EVIDENCE_MS overrides the
 *  abandonment: the process was demonstrably not running until a moment ago,
 *  so whatever else is true, it has just been thawed.  This covers a system
 *  configured with an InhibitDelayMaxSec long enough that the interval between
 *  the signal and the freeze exhausts the running-time allowance on its own.
 *
 *  @return true if a sleep cycle is open, false if none is or the open one has
 *          been abandoned
 */
bool ldbus_in_open_sleep_cycle() {
   bool debug = false;
   bool result = false;

   // Read the prepare timestamp first, per
   // ldbus_elapsed_since_pending_prepare_for_sleep_ns(), and the current time
   // last, so that no difference taken below can wrap.
   uint64_t prepare_ns   = last_prepare_for_sleep_ns;
   uint64_t resume_ns    = last_resume_from_sleep_ns;
   uint64_t heartbeat_ns = sleep_watch_heartbeat_ns;
   uint64_t now_ns       = cur_boot_time_nanosec();

   uint64_t running_since_prepare_ns = 0;
   uint64_t heartbeat_age_ns         = 0;
   if (prepare_ns > resume_ns) {
      running_since_prepare_ns =
            (heartbeat_ns > prepare_ns) ? heartbeat_ns - prepare_ns : 0;
      heartbeat_age_ns = (now_ns > heartbeat_ns) ? now_ns - heartbeat_ns : 0;

      result =
         running_since_prepare_ns < MILLIS2NANOS(OPEN_SLEEP_CYCLE_MAX_RUNNING_MS) ||
         heartbeat_age_ns         > MILLIS2NANOS(SLEEP_WATCH_FREEZE_EVIDENCE_MS);
   }

   DBGF(debug, "last_prepare_for_sleep_ns=%"PRIu64", last_resume_from_sleep_ns=%"PRIu64
               ", running_since_prepare=%"PRIu64" ms, heartbeat_age=%"PRIu64" ms,"
               " Returning %s",
               prepare_ns, resume_ns, NANOS2MILLIS(running_since_prepare_ns),
               NANOS2MILLIS(heartbeat_age_ns), sbool(result));
   return result;
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
          // ldbus_elapsed_since_pending_prepare_for_sleep_ns().
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
   while (!quit_sleep_watch_thread) {
       // Recorded before the dispatch rather than after it, so that a freeze
       // during the dispatch -- where this thread spends all of its time --
       // leaves the heartbeat at its pre-freeze value.
       sleep_watch_heartbeat_ns = cur_boot_time_nanosec();
       dbus_connection_read_write_dispatch(dcd->conn, SLEEP_WATCH_LOOP_TIMEOUT_MS);
   }

   // Nothing is left to receive the matching PrepareForSleep(false), so an open
   // cycle could never close.  Close it here, using the prepare timestamp
   // rather than the current time: the cycle is over, but no resume has
   // occurred and none should be reported.
   if (last_prepare_for_sleep_ns > last_resume_from_sleep_ns)
      last_resume_from_sleep_ns = last_prepare_for_sleep_ns;

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

