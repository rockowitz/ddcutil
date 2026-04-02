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

uint64_t millisec_since_resumed_from_sleep();

#endif /* DBUS_UTIL_H_ */
