// app_probe.h

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 

#ifndef APP_PROBE_H_
#define APP_PROBE_H_

#include "base/displays.h"

void probe_display_by_dh(Display_Handle * dh);
void probe_display_by_dref(Display_Ref * dref);

#endif /* APP_PROBE_H_ */
