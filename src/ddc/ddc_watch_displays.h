/** @file ddc_watch_displays.h  Watch for monitor addition and removal  */

// Copyright (C) 2019-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_WATCH_DISPLAYS_H_
#define DDC_WATCH_DISPLAYS_H_

/** \cond */
#include <glib-2.0/glib.h>

#include "public/ddcutil_types.h"
/** \endcond */

extern int            extra_stabilization_millisec;
extern int            stabilization_poll_millisec;
extern bool           use_sysfs_connector_id;
extern bool           report_udev_events;
extern int            secondary_udev_receive_millisec;
extern int            udev_poll_loop_millisec;

// bool         is_watch_thread_executing();
void         init_ddc_watch_displays();






gpointer ddc_watch_displays_udev_i2c(gpointer data);

#endif /* DDC_WATCH_DISPLAYS_H_ */
