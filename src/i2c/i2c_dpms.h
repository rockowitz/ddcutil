/** @file i2c_dpms.h
 *  DPMS related functions
 */

// Copyright (C) 2023-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_DPMS_H_
#define I2C_DPMS_H_

#include <stdbool.h>

#include "config.h"

#include "base/displays.h"
#include "base/i2c_bus_base.h"

// DPMS Detection
#ifdef USE_X11
#define DPMS_STATE_X11_CHECKED 0x01
#define DPMS_STATE_X11_ASLEEP  0x02
#endif
#define DPMS_SOME_DRM_ASLEEP   0x04
#define DPMS_ALL_DRM_ASLEEP    0x08
typedef Byte Dpms_State;

extern Dpms_State dpms_state;

char *           interpret_dpms_state_t(Dpms_State state);
bool             dpms_check_drm_asleep_by_connector(const char * drm_connector_name);
bool             dpms_check_drm_asleep_by_businfo(I2C_Bus_Info * businfo);
bool             dpms_check_drm_asleep_by_dref(Display_Ref * dref);
void             init_i2c_dpms();

#endif /* I2C_DPMS_H_ */
