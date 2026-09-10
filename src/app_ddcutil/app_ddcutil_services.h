/** @file app_ddcutil_services.h
 *  Master initializer for app_ddcutil directory
 */

// Copyright (C) 2022-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_DDCUTIL_SERVICES_H_
#define APP_DDCUTIL_SERVICES_H_

// Nneeded because unit test test_dw_status_events, created by claude, fails on
// OpenSUSE build service when built for older distributions because true and
// false are undefined.
#include <stdbool.h>

void init_app_ddcutil_services();

#endif /* APP_DDCUTIL_SERVICES_H_ */
