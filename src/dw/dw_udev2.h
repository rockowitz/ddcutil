/** @file dw_udev2.h
 *  Watch for monitor addition and removal using UDEV
 */

// Copyright (C) 2019-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_UDEV2_H_
#define DW_UDEV2_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "public/ddcutil_types.h"
/** \endcond */

extern bool report_udev_events;

void dw2_setup();
void dw2_teardown();
bool dw2_watch();

void init_dw2_udev();
#endif /* DW_UDEV_H_ */
