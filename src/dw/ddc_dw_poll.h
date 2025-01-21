/** @file  ddc_dw_poll.h */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_DW_POLL_H_
#define DDC_DW_POLL_H_

#include <glib-2.0/glib.h>

extern int  nonudev_poll_loop_millisec;
extern bool stabilize_added_buses_w_edid;

gpointer ddc_watch_displays_without_udev(gpointer data);
void init_ddc_watch_displays_poll();

#endif /* DDC_DW_POLL_H_ */
