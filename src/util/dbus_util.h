/** \file dbus_util.h
 *  Base functions for using dbus
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DBUS_UTIL_H_
#define DBUS_UTIL_H_

#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <stdbool.h>

uint64_t ldbus_elapsed_since_resume_from_sleep_ns();
int      ldbus_pause_if_recent_return_from_sleep(int minimum_ms);
bool     ldbus_start_sleep_watch_thread();
void     ldbus_stop_sleep_watch_thread();

typedef  void (*PREPARE_FOR_SLEEP_CALLBACK)(bool);
void     ldbus_register_prepare_for_sleep_callback(PREPARE_FOR_SLEEP_CALLBACK callback);
void     ldbus_unregister_prepare_for_sleep_callback(PREPARE_FOR_SLEEP_CALLBACK callback);

#endif /* DBUS_UTIL_H_ */
