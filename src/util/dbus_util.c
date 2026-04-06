/** @file dbus_util.c
 *
 *  Miscellaneous Linux utilities
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <dbus-1.0/dbus/dbus.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
/** \endcond */

#include <unistd.h>
#include <signal.h>

#include "debug_util.h"
#include "report_util.h"
#include "string_util.h"
#include "timestamp.h"

#ifdef SDBUS_VARIANT
// sd-bus

#include <systemd/sd-bus.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#include "dbus_util.h"


#ifdef SDBUS_VARIANT
static int on_prepare_for_sleep(sd_bus_message *m, void *userdata, sd_bus_error *error) {
    int preparing;
    int r = sd_bus_message_read(m, "b", &preparing);
    if (r < 0) {
        fprintf(stderr, "Failed to parse message: %s\n", strerror(-r));
        return 0;  // Continue
    }
    printf("PrepareForSleep: %s\n", preparing ? "true (prepare)" : "false (resume)");
    return 0;
}

int  watch_for_sleep_using_sdbus() {
   bool debug = true;

    sd_bus *bus = NULL;
    sd_bus_slot *slot = NULL;
    int r;

    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        return EXIT_FAILURE;
    }

    r = sd_bus_add_match(bus, NULL, "type='signal',"
                         "sender='org.freedesktop.login1',"
                         "interface='org.freedesktop.login1.Manager',"
                         "member='PrepareForSleep'", on_prepare_for_sleep, NULL);
    if (r < 0) {
        fprintf(stderr, "Failed to add match: %s\n", strerror(-r));
        goto finish;
    }

    DBGF(debug,"Listening for PrepareForSleep... (Ctrl+C to exit)");
    r = sd_bus_process(bus, NULL);  // Simple loop; use sd_bus_wait() for production
    while (r > 0) {
        r = sd_bus_process(bus, NULL);
    }

finish:
    sd_bus_slot_unref(slot);
    sd_bus_unref(bus);
    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
#endif





// libsdbus

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


typedef struct {
   DBusConnection * conn;
} Dbus_Connection_Data;


static volatile sig_atomic_t quit = 0;
// static void sigint_handler(int sig) {
//     quit = 1;
// }

void dbgrpt_DBusMessage(DBusMessage* msg, int depth) {
   int d0 = depth;
   int d1 = depth+1;
   rpt_vstring(d0,"Message type: %d", dbus_message_get_type(msg));           // SIGNAL, METHOD_CALL, etc
   rpt_vstring(d1,"sender:    %s", dbus_message_get_sender(msg));         // org.freedesktop.login1
   rpt_vstring(d1,"interface: %s", dbus_message_get_interface(msg));      // org.freedesktop.login1.Manager
   rpt_vstring(d1,"menber:    %s", dbus_message_get_member(msg));         // PrepareForSleep
   rpt_vstring(d1,"path:      %s", dbus_message_get_path(msg));           // /org/freedesktop/login1
}


uint64_t last_prepare_for_sleep_ns = 0;
uint64_t last_resume_from_sleep_ns = 0;

uint64_t elapsed_since_resume_from_sleep_ns() {
   uint64_t elapsed_ns = 0;
   if (last_resume_from_sleep_ns > 0) {
      uint64_t cur_ns = cur_realtime_nanosec();
      elapsed_ns = cur_ns - last_resume_from_sleep_ns;
   }
   return elapsed_ns;
}


static DBusHandlerResult
ldbus_handle_message(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
   bool debug = true;
   DBGF(debug,"Starting.");
   DBusError err;
   dbus_error_init(&err);
   dbgrpt_DBusMessage(msg, 1);
   if (dbus_message_is_signal(msg, "org.freedesktop.login1.Manager", "PrepareForSleep")) {
        dbus_bool_t preparing;
        if (dbus_message_get_args(
              msg,
              &err,
              DBUS_TYPE_BOOLEAN, &preparing,
              DBUS_TYPE_INVALID)) {
            DBGF(debug, "PrepareForSleep: %s", preparing ? "true (prepare)" : "false (resume)");
            uint64_t mono_ns = cur_realtime_nanosec();
            if (preparing)
               last_prepare_for_sleep_ns = mono_ns;
            else
               last_resume_from_sleep_ns = mono_ns;
            DBGF(debug,"Set %s = %"PRIu64, (preparing) ? "true (prepare)" : "false (resume)",
                  mono_ns);
        }
        else {
          //  printf("dbus_message_get_args failed\n");
          DBUS_ERROR_EMIT(err);
        }
        fflush(stdout);
    }
    else
       DBGF(debug,"Not for us");

    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    DBGF(debug,"Done.  returning %d", result);
    return result;
}


gpointer ldbus_watch_sleep_events_thread(gpointer data) {
   bool debug = true;

   Dbus_Connection_Data * dcd = (Dbus_Connection_Data*) data;
   DBGF(debug,"Listening for PrepareForSleep...");

   while (!quit) {
       dbus_connection_read_write_dispatch(dcd->conn, -1);
       // printf(".\n");
       // sleep(1);
   }

   dbus_connection_remove_filter(dcd->conn, ldbus_handle_message, NULL);

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
      // fprintf(stderr, "Connection Error (%s)\n", err.message);
      dbus_error_free(&err);
      ok = false;
      goto bye;
   }

   dbus_connection_set_exit_on_disconnect(conn, FALSE);
#ifdef OUT
   const char * rule1 =
         "type='signal',"
         "sender='org.freedesktop.login1',"
         "interface='org.freedesktop.login1.Manager',"
         "member='PrepareForSleep'";
   DBGF(debug,"Suck uo rule %s", rule1);

   const char * rule2 =   "type='signal'";
   DBGF(debug,"Suck uo rule %s", rule2);
#endif
   const char * rule3 =
         "type='signal',"
         "interface='org.freedesktop.login1.Manager',"
         "member='PrepareForSleep'";

   dbus_bus_add_match(conn, rule3, &err);
   if (dbus_error_is_set(&err)) {
      DBUS_ERROR_EMIT(err);
      // fprintf(stderr, "Match Error (%s)\n", err.message);
      ok = false;
      goto bye;
   }

   dbus_connection_add_filter(conn, ldbus_handle_message, NULL, NULL);

   Dbus_Connection_Data * dcd = calloc(1,sizeof(Dbus_Connection_Data));
   dbus_connection_ref(conn);   // thread holds its own reference
   dcd->conn = conn;
   GThreadFunc watch_thread_func = ldbus_watch_sleep_events_thread;
   GThread * sleep_watch_thread = g_thread_new("slwwp_watch_thread",             // optional thread name
                                 watch_thread_func,
                                 dcd);
   DBGF(true, "Started sleep watch thread at %p", sleep_watch_thread);

bye:
   if (conn)
      dbus_connection_unref(conn);
   DBGF(debug, "Done.  Returning %s", ok);
   return ok;
}


#ifdef INCORRECT
uint64_t last_sleep_change_timestamp() {
   bool debug = false;
   DBGF(debug, "Starting.");

   DBusError error;
   dbus_error_init(&error);
   uint64_t last_sleep_change_timestamp = 0;

   DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
   if (dbus_error_is_set(&error)) {
         // fprintf(stderr, "Connection Error (%s)\n", error.message);
         // syslog(LOG_ERR, "dbus connection error (%s)\n", error.message);
      DBUS_ERROR_EMIT(error);
      dbus_error_free(&error);
      goto bye;
   }

   DBusMessage *dbus_msg = dbus_message_new_method_call(
                              "org.freedesktop.login1",          // destination
                              "/org/freedesktop/login1",         // path
                              "org.freedesktop.DBus.Properties", // interface
                              "Get");                            // method
   if (!dbus_msg) {
      // fprintf(stderr, "dbus_message_new_method_call failed\n");
      // syslog(LOG_ERR, "dbus_message_new_method_call failed");
      DBUS_ERROR_EMIT(error);
      goto bye;
   }

   char * arg1 = "org.freedesktop.login1.Manager";
   char * arg2 =   "IdleSinceHintMonotonic"; // "SleepOperationTimestampMonotonic"; //    "IdleSinceHintMonotonic";
   dbus_message_append_args(dbus_msg,
                              DBUS_TYPE_STRING, &arg1,
                              DBUS_TYPE_STRING, &arg2,
                              DBUS_TYPE_INVALID);

   DBusMessage *reply = dbus_connection_send_with_reply_and_block(
           conn, dbus_msg, /* timeout_millis */ 3000, &error);
   dbus_message_unref(dbus_msg);
   if (dbus_error_is_set(&error)) {
      DBUS_ERROR_EMIT(error);
      // fprintf(stderr, "Error (%s)\n", error.message);
      // syslog(LOG_ERR,  "dbus error (%s)\n", error.message);
      dbus_error_free(&error);
      goto bye;
   }

   if (reply) {
      DBusMessageIter outer, inner;
      dbus_message_iter_init(reply, &outer);
      dbus_message_iter_recurse(&outer, &inner);

      // is this microseconds or milliseconds?
      dbus_message_iter_get_basic(&inner, &last_sleep_change_timestamp);
      DBGF(debug, "Last sleep change (monotonic usec): %" PRIu64 , last_sleep_change_timestamp);
   }

   dbus_message_unref(reply);
   dbus_connection_unref(conn);

 bye:
   DBGF(debug, "Done.    Returning %"PRIu64, last_sleep_change_timestamp);
   return last_sleep_change_timestamp;
}


uint64_t millisec_since_resumed_from_sleep() {
   bool debug = false;
   DBGF(debug, "Starting.");

   uint64_t last_sleep_change_timestamp_usec = last_sleep_change_timestamp();

   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   uint64_t now_usec =  now.tv_sec * 1000000ULL + now.tv_nsec / 1000;

   uint64_t usec_since_resumed = now_usec - last_sleep_change_timestamp_usec;
   uint64_t millisec_since_resumed = usec_since_resumed/1000;

   DBGF(debug, "Done.    Returning %"PRIu64, millisec_since_resumed);
   return millisec_since_resumed;
}
#endif
