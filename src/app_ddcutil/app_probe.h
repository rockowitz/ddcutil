/** \file app_probe.h
  * Implement PROBE command
  */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_PROBE_H_
#define APP_PROBE_H_

#include "base/displays.h"

void app_probe_display_by_dref(Display_Ref * dref);
void app_probe_display_by_dh(Display_Handle * dh);

#endif /* APP_PROBE_H_ */
