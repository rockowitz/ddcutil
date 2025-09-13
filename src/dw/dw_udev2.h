/** @file dw_udev2.h
 *  Watch for monitor addition and removal using UDEV
 */

// Copyright (C) 2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_UDEV2_H_
#define DW_UDEV2_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "public/ddcutil_types.h"
/** \endcond */

extern bool report_udev_events;

void dw_udev_setup();
void dw_udev_teardown();
bool dw_udev_watch();

void init_dw_udev2();
#endif /* DW_UDEV_H_ */
