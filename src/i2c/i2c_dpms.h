/** @file i2c_dpms.h
 *  DPMS related functions
 */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_DPMS_H_
#define I2C_DPMS_H_

#include <stdbool.h>

#include "base/i2c_bus_base.h"

// DPMS Detection
#define DPMS_STATE_X11_CHECKED 0x01
#define DPMS_STATE_X11_ASLEEP  0x02
#define DPMS_SOME_DRM_ASLEEP   0x04
#define DPMS_ALL_DRM_ASLEEP    0x08
typedef Byte Dpms_State;

extern Dpms_State dpms_state;

char *           interpret_dpms_state_t(Dpms_State state);
void             dpms_check_x11_asleep();
bool             dpms_check_drm_asleep(I2C_Bus_Info * businfo);

#endif /* I2C_DPMS_H_ */
