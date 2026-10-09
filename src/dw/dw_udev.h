/** @file dw_udev.h
 *  Watch for monitor addition and removal using UDEV
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_UDEV_H_
#define DW_UDEV_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

#include "dw_common.h"   // for EVENT_BUS_NUMBER

extern bool report_udev_events;
extern int  udev_watch_stats_interval_sec;

void dw_udev_setup();
void dw_udev_teardown();
#ifdef EVENT_BUS_NUMBER
bool dw_udev_watch(int watch_loop_millisec, int * event_busno_loc);
#else
bool dw_udev_watch(int watch_loop_millisec);
#endif
int  dw_udev_drain();

void init_dw_udev();
#endif /* DW_UDEV_H_ */
