/** @file usb_edid.h
 *
 * Functions to get EDID for USB connected monitors
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef USB_EDID_H_
#define USB_EDID_H_

#include <linux/hiddev.h>

#include "util/edid.h"

Parsed_Edid * get_hiddev_edid_with_fallback(int fd, struct hiddev_devinfo * dev_info);

void init_usb_edid();

#endif /* USB_EDID_H_ */
