/** @file sysfs_services.h
 */

// Copyright (C) 2022-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SYSFS_SERVICES_H_
#define SYSFS_SERVICES_H_

// Nneeded because unit test test_dw_status_events, created by claude, fails on
// OpenSUSE build service when built for older distributions because true and
// false are undefined.
#include <stdbool.h>

void init_sysfs_services();
void terminate_sysfs_services();

#endif /* SYSFS_SERVICES_H_ */
