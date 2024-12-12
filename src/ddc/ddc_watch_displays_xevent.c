/** @file ddc_watch_displays_xevent.c */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "base/core.h"
#include "base/displays.h"     // for terminate_watch_thread
#include "base/rtti.h"
#include "base/sleep.h"

#include "ddc_watch_displays_xevent.h"


void ddc_free_xevent_data(XEvent_Data * evdata) {
   if (evdata->dpy)
      XCloseDisplay(evdata->dpy);
   free(evdata);
}


XEvent_Data * ddc_init_xevent_screen_change_notification() {
   bool debug = true;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");

   bool ok = false;
   // check for extension
   XEvent_Data * evdata = calloc(1, sizeof(XEvent_Data));
   evdata->dpy = XOpenDisplay(NULL);
   if (!evdata->dpy)
      goto bye;
   evdata->screen = DefaultScreen(evdata->dpy);
   evdata->w = RootWindow(evdata->dpy, evdata->screen);

   bool have_rr = XRRQueryExtension(evdata->dpy, &evdata->rr_event_base, &evdata->rr_error_base);
   if (!have_rr) {
      DBGTRC(true, DDCA_TRC_NONE, "XRR Extension unavailable");
      goto bye;
   }
   // TODO: additional checks

   evdata->screen_change_eventno = evdata->rr_event_base + RRScreenChangeNotify;
   XRRSelectInput(evdata->dpy, evdata->w, RRScreenChangeNotifyMask);
   ok = true;

bye:
   if (!ok) {
      ddc_free_xevent_data(evdata);
      evdata = NULL;
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning %p", evdata);
   return evdata;
}


_Bool ddc_detect_xevent_screen_change(XEvent_Data *evdata, int poll_interval) {
   bool debug = true;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "evdata=%p, poll_interval=%d", evdata, poll_interval);
   bool found = false;
   XEvent event;
   while (true) {
      if (terminate_watch_thread)
         break;
      found = XCheckTypedEvent(evdata->dpy, evdata->screen_change_eventno, &event);
      if (found) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Received event type %d", event.type);
         XAnyEvent *e = (XAnyEvent*) &event;
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
               "windows change event  serial %ld, synthetic %s, window 0x%lx,", e->serial,
               sbool(e->send_event), e->window);

         bool more = true;
         int flushct = 0;
         while (more) {
            more = XCheckTypedEvent(evdata->dpy, evdata->screen_change_eventno, &event);
            flushct++;
         }
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Flushed %d events", flushct);
         break;
      } else {
         // DBGF(debug, "Not found");
         // DBGF(debug, "sleeping");
         // sleep(2);
         // split_sleep();
         sleep_millis(poll_interval);
      }
   }
   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, found, "");
   return found;
}

void init_ddc_watch_displays_xevent() {
   RTTI_ADD_FUNC(ddc_detect_xevent_screen_change);
   RTTI_ADD_FUNC(ddc_init_xevent_screen_change_notification);
}
