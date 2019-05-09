/** @file usb_hid_common.h
 *
 *  Functions that are common to the wrappers for multiple USB HID
 *  packages such as libusb, hiddev
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef USB_HID_COMMON_H_
#define USB_HID_COMMON_H_

/** \cond */
#include <stdbool.h>
#include <stdint.h>
/** \endcond */

const char * collection_type_name(uint8_t collection_type);

bool force_hid_monitor_by_vid_pid(int16_t vid, int16_t pid);
bool deny_hid_monitor_by_vid_pid(int16_t vid, int16_t pid);

#endif /* USB_HID_COMMON_H_ */
