/** @file ddc_watch_displays.h  Watch for monitor addition and removal  */

// Copyright (C) 2019-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_WATCH_DISPLAYS_UDEV_H_
#define DDC_WATCH_DISPLAYS_UDEV_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "public/ddcutil_types.h"
/** \endcond */

extern bool           use_sysfs_connector_id;
extern bool           report_udev_events;

gpointer    ddc_watch_displays_udev(gpointer data);

void        init_ddc_watch_displays_udev();

#endif /* DDC_WATCH_DISPLAYS_UDEV_H_ */
