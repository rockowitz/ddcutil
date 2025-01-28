/** @file  dw_poll.h */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_POLL_H_
#define DW_POLL_H_

#include <glib-2.0/glib.h>

extern int  nonudev_poll_loop_millisec;
extern int  retry_thread_sleep_factor_millisec;
extern bool stabilize_added_buses_w_edid;

gpointer dw_watch_display_connections(gpointer data);
void init_dw_poll();

#endif /* DW_POLL_H_ */
