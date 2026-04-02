/** @file dbus_util.c
 *
 *  Miscellaneous Linux utilities
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <grp.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>

#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

// for dbus
#include <dbus-1.0/dbus/dbus.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include <stdlib.h>




#include "common_inlines.h"
#include "debug_util.h"
#include "file_util.h"
#include "report_util.h"
#include "string_util.h"
#include "subprocess_util.h"

#include "dbus_util.h"


uint64_t last_sleep_change_timestamp() {
   bool debug = false;
    DBGF(debug, "Starting.");

    DBusError error;
    dbus_error_init(&error);
    uint64_t last_sleep_change_timestamp = 0;

    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (dbus_error_is_set(&error)) {
         fprintf(stderr, "Connection Error (%s)\n", error.message);
         syslog(LOG_ERR, "dbus connection error (%s)\n", error.message);
         dbus_error_free(&error);
         goto bye;
    }

    DBusMessage *dbus_msg = dbus_message_new_method_call("org.freedesktop.login1",
                                                     "/org/freedesktop/login1",
                                                     "org.freedesktop.DBus.Properties",
                                                     "Get");
    if (!dbus_msg) {
         fprintf(stderr, "dbus_message_new_method_call failed\n");
         syslog(LOG_ERR, "dbus_message_new_method_call failed");
         goto bye;
    }

    char * arg1 = "org.freedesktop.login1.Manager";
    char * arg2 = "IdleSinceHintMonotonic";
    dbus_message_append_args(dbus_msg,
                              DBUS_TYPE_STRING, &arg1,
                              DBUS_TYPE_STRING, &arg2,
                              DBUS_TYPE_INVALID);

    DBusMessage *reply = dbus_connection_send_with_reply_and_block(
           conn, dbus_msg, /* timeout_millis */ 3000, &error);
    dbus_message_unref(dbus_msg);
    if (dbus_error_is_set(&error)) {
         fprintf(stderr, "Error (%s)\n", error.message);
         syslog(LOG_ERR,  "dbus error (%s)\n", error.message);
         dbus_error_free(&error);
         goto bye;
    }

    if (reply) {
         DBusMessageIter outer, inner;
         dbus_message_iter_init(reply, &outer);
         dbus_message_iter_recurse(&outer, &inner);

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
