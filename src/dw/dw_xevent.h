/** @file dw_xevent.h */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_XEVENT_H_
#define DW_XEVENT_H_

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
bool          dw_next_X11_event_of_interest(XEvent_Data * evdata);
void          ddc_send_x11_termination_message(XEvent_Data * evdata);

void          init_dw_xevent();

#endif /* DW_XEVENT_H_ */
