/** @file dw_udev.h
 *  Watch for monitor addition and removal using UDEV
 */

// Copyright (C) 2019-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_UDEV_H_
#define DW_UDEV_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "public/ddcutil_types.h"
/** \endcond */

extern bool use_sysfs_connector_id;
extern bool report_udev_events;

gpointer    dw_watch_displays_udev(gpointer data);
void        init_dw_udev();

#endif /* DW_UDEV_H_ */
