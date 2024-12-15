/** @file ddc_watch_displays_xevent.h */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_WATCH_DISPLAYS_XEVENT_H_
#define DDC_WATCH_DISPLAYS_XEVENT_H_

#include <stdbool.h>
#include <X11/Xlib.h>

typedef struct {
   Display* dpy;
   int      screen;
   Window   w;
   int      rr_event_base;
   int      rr_error_base;
   int      screen_change_eventno;
} XEvent_Data;

void          dbgrpt_xevent_data(XEvent_Data* evdata, int depth);
void          ddc_free_xevent_data(XEvent_Data * evdata);
XEvent_Data * ddc_init_xevent_screen_change_notification();
bool          ddc_detect_xevent_screen_change(XEvent_Data * evdata,  int poll_interval);

void          init_ddc_watch_displays_xevent();

#endif /* DDC_WATCH_DISPLAYS_XEVENT_H_ */
