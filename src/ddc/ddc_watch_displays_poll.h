/** @file  ddc_watch_displays_poll.h */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_WATCH_DISPLAYS_POLL_H_
#define DDC_WATCH_DISPLAYS_POLL_H_

#include <glib-2.0/glib.h>

extern int  nonudev_poll_loop_millisec;

gpointer ddc_watch_displays_using_poll(gpointer data);

void init_ddc_watch_displays_poll();

#endif /* DDC_WATCH_DISPLAYS_POLL_H_ */
