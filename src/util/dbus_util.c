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


void ldbus_elapsed_since_resume_fromm_sleep_mark_start() {
   bool debug = false;

   last_resume_from_sleep_ns = last_prepare_for_sleep_ns = cur_boot_time_nanosec();

   DBGF(debug, "Executed.  set last_resume_from_sleep_ns = last_prepare_for_sleep_ns =%"PRIu64,
         last_resume_from_sleep_ns);
}


/** Returns the number of nanoseconds since the most
 *  recent return from sleep, detected via D-Bus.
 *
 *  @return nanoseconds since last resume, UINT64_MAX if no resume detected
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


/** If the elapsed time since the most recent return from sleep occurred
 *  is less than the specified value, sleep for the time remaining until
 *  the specified time value has elapsed.
 *
 *  @param  minimum_ms
 *  @return number of milliseconds slept
 */
int ldbus_pause_if_recent_return_from_sleep(int minimum_ms) {
   bool debug = false;

   uint64_t elapsed_ns = ldbus_elapsed_since_resume_from_sleep_ns();
   uint64_t elapsed_ms = NANOS2MILLIS(elapsed_ns);

   if (debug) {
      char * msg = g_strdup_printf(
                    "Time since last return from sleep = %"PRIu64" ns = %"PRIu64" ms",
                    elapsed_ns, elapsed_ms);
      DBG("%s", msg);
      char prefix[200];
      get_msg_decoration(prefix, 200, /*dest_syslog*/ true);
      syslog(LOG_WARNING, "%s%s", prefix, msg);
      free(msg);
   }

   uint64_t remaining_ms = 0;
   if (elapsed_ms < minimum_ms) {
      remaining_ms = minimum_ms - elapsed_ms;
      char * msg2 = g_strdup_printf("Pausing for %"PRIu64, remaining_ms);
      syslog(LOG_NOTICE, "%s", msg2);
      DBGF(debug,"%s", msg2);
      usleep(MILLIS2MICROS(remaining_ms));
      free(msg2);
   }

   DBGF(debug, "Done.   Returning: %"PRIu64" millisec", remaining_ms);
   return remaining_ms;
}


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
          uint64_t mono_ns = cur_boot_time_nanosec();
          if (preparing)
             last_prepare_for_sleep_ns = mono_ns;
          else
             last_resume_from_sleep_ns = mono_ns;
          char prefix[200];
          get_msg_decoration(prefix, 200, /*dest_syslog*/ true);
          char * s1 = g_strdup_printf(
                "%s%sReceived dbus signal PrepareForSleep(%s)",
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

   ldbus_elapsed_since_resume_fromm_sleep_mark_start();
   int timeout_ms = 500;    // - = never
   while (!quit_sleep_watch_thread) {
       dbus_connection_read_write_dispatch(dcd->conn, timeout_ms);
   }

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

   dbus_connection_add_filter(conn, ldbus_handle_message, NULL, NULL);

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

