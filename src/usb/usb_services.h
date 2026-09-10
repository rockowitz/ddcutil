/** @file usb_services.h */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef USB_SERVICES_H_
#define USB_SERVICES_H_

// Nneeded because unit test test_usb_services, created by claude, fails on
// OpenSUSE build service when built for older distributions because true and
// false are undefined.
#include <stdbool.h>

void init_usb_services();
void terminate_usb_services();

#endif /* USB_SERVICES_H_ */
