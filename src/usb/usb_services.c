/** @file usb_services.c */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "usb_base.h"
#include "usb_displays.h"
#include "usb_edid.h"

#include "usb_services.h"

 void init_usb_services() {
    init_usb_base();
    init_usb_displays();
    init_usb_edid();
 }

 void terminate_usb_services() {
    // terminate_usb_displays();  // already called from termindate_ddc_services
    terminate_usb_base();
 }
