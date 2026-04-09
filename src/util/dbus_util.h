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

extern uint64_t last_prepare_for_sleep_ns;
extern uint64_t last_resume_from_sleep_ns;

uint64_t ldbus_elapsed_since_resume_from_sleep_ns();
bool     ldbus_start_sleep_watch_thread();
void     ldbus_stop_sleep_watch_thread();

#endif /* DBUS_UTIL_H_ */
