/** @file ddc_dw_xevent.c */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

#include "util/report_util.h"

#include "base/core.h"
#include "base/displays.h"       // for terminate_watch_thread
#include "base/i2c_bus_base.h"   // for DW_SLEEP()
#include "base/rtti.h"
#include "base/sleep.h"

#include "ddc/ddc_dw_common.h"

#include "ddc/ddc_dw_xevent.h"

static const DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;

static Display* dpy;
static int  screen_change_eventno = 0;


void  dbgrpt_xevent_data(XEvent_Data* evdata, int depth) {
   rpt_structure_loc("XEvent_Data", evdata, depth);
   int d1 = depth+1;
   rpt_vstring(d1, "dpy:                   %p",  evdata->dpy);
   rpt_vstring(d1, "screen:                %d",  evdata->screen);
   rpt_vstring(d1, "w:                     %jd", (uintmax_t) evdata->w);
   rpt_vstring(d1, "rr_error_base:         %d",  evdata->rr_error_base);
   rpt_vstring(d1, "rr_event_base:         %d",  evdata->rr_event_base);
   rpt_vstring(d1, "screen_change_eventno: %d",  evdata->screen_change_eventno);
}


void ddc_free_xevent_data(XEvent_Data * evdata) {
   if (evdata->dpy)
      XCloseDisplay(evdata->dpy);
   free(evdata);
}


/** Initialization for using X11 to detect screen changes.
 *
 *  @return pointer to newly allocated XEvent_Data struct,
 *          NULL if initialization fails
 */
XEvent_Data * ddc_init_xevent_screen_change_notification() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   bool ok = false;
   // check for extension
   XEvent_Data * evdata = calloc(1, sizeof(XEvent_Data));
   Display * display = XOpenDisplay(NULL);
   evdata->dpy = display;
   if (!evdata->dpy)
      goto bye;
   evdata->screen = DefaultScreen(evdata->dpy);
   evdata->w = RootWindow(evdata->dpy, evdata->screen);

   bool have_rr = XRRQueryExtension(evdata->dpy, &evdata->rr_event_base, &evdata->rr_error_base);
   if (have_rr) {
      int maj = 0;
      int min = 0;
      XRRQueryVersion(evdata->dpy, &maj, &min);
      int version = (maj << 8) | min;
      if (version < 0x0102)     // is this the right version check?
         have_rr = false;
   }
   if (!have_rr) {
      DBGTRC(true, DDCA_TRC_NONE, "XRR Extension unavailable");
      goto bye;
   }

   screen_change_eventno = evdata->rr_event_base + RRScreenChangeNotify;
   evdata->screen_change_eventno = evdata->rr_event_base + RRScreenChangeNotify;
   if (!terminate_using_x11_event) {
      XRRSelectInput(evdata->dpy, evdata->w, RRScreenChangeNotifyMask);
      // XSelectInput(evdata->dpy, evdata->w, RRScreenChangeNotifyMask);
   }
   else {
      XRRSelectInput(evdata->dpy, evdata->w, 0xffffffff);
   }
   ok = true;

bye:
   if (!ok) {
      ddc_free_xevent_data(evdata);
      evdata = NULL;
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %p", evdata);
   return evdata;
}


//
// Used for non XIfEvent() mode, i.e. terminate_using_x11_event == false
//

/** Waits for an X11 screen change event. Repeatedly calls XCheckTypedEvent()
 *  in a polling loop until a screen change XEvent is received or the polling
 *  loop is terminated by #global terminate_watch_thread being set.
 *
 *  This function is used when waiting is NOT terminated by a user created
 *  X11 event.
 *
 *  @param evdata pointer to XEvent_Data struct
 *  @param poll_interval  XCheckTypedEvent() polling interval
 *  @retval true  screen changed event was received
 *  @retval false
 */
bool ddc_detect_xevent_screen_change(XEvent_Data *evdata, int poll_interval) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "evdata=%p, poll_interval=%d", evdata, poll_interval);

   bool found = false;
   int flushct = 0;
   XEvent event;
   while (true) {
      if (terminate_watch_thread)
         break;
      found = XCheckTypedEvent(evdata->dpy, evdata->screen_change_eventno, &event);
      if (found) {
         if (debug)
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Received event type %d", event.type);
         XAnyEvent *e = (XAnyEvent*) &event;
         if (debug)
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                  "windows change event  serial %ld, synthetic %s, window %ju,",
                  e->serial, sbool(e->send_event), (intmax_t)e->window);
         bool more = true;
         while (more) {
            more = XCheckTypedEvent(evdata->dpy, evdata->screen_change_eventno, &event);
            flushct++;
         }
         if (debug)
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Flushed %d events", flushct);
         break;
      } else {
         if (debug)
            printf(".");
         sleep_millis(poll_interval);
      }
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, found, "Flushed %d events", flushct);
   return found;
}


//
// Used when checking the event queue using XIfEvent()
//

/** Calls XSendEvent to place a termination message into the
 *  event queue.
 *
 *  @param evdata pointer to XEvent_Data struct
 */
void ddc_send_x11_termination_message(XEvent_Data * evdata) {
   bool debug = true;
   DBGTRC_STARTING(debug, TRACE_GROUP, "evdata->dpy=%p", evdata->dpy);

   Display * dpy = evdata->dpy;
   // Display * dpy = XOpenDisplay(NULL);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "dpy = %p", dpy);
   int screen = DefaultScreen(dpy);
   Window win = RootWindow(dpy, screen);
   XEvent evt;

   evt.xclient.type = ClientMessage;
   evt.xclient.serial = 0;
   evt.xclient.send_event = True;
   evt.xclient.display = evdata->dpy;
   evt.xclient.window = win;
   evt.xclient.message_type = XInternAtom(evdata->dpy, "TERMINATION_MSG", false);
   evt.xclient.format = 32;
   evt.xclient.data.l[0] = 0;
   evt.xclient.data.l[1] = 0;
   evt.xclient.data.l[2] = 0;
   evt.xclient.data.l[3] = 0;
   evt.xclient.data.l[4] = 0;

   // Send the client message
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling XSendEvent() ...");
   bool ok = XSendEvent(dpy, win /*DefaultRootWindow(dpy)*/, False,
              NoEventMask,
           /*   SubstructureRedirectMask | SubstructureNotifyMask,*/
              (XEvent *)&evt);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "XSendEvent() returned %s", SBOOL(ok));
   XFlush(dpy);
   DW_SLEEP_MILLIS(2000, "After XSendEvent");  // needed?

   if (ok)
      DBGTRC_DONE(debug, TRACE_GROUP, "XSendEvent() succeeded");
   else
      DBGTRC_DONE(debug, TRACE_GROUP, "XSendEvent() failed!");
}


/** Predicate function used by XIfEvent()
 *
 *  @param  dsp   X11 display
 *  @param  evt   XEvent to test
 *  @param  arg   pointer to Watch_Displays_Data
 */
Bool dw_is_ddc_event(Display * dsp, XEvent * evt, XPointer arg) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dsp=%p, evt=%p, arg=%p", dsp, evt, arg);

   bool result = false;
   // Watch_Displays_Data * wdd = arg;
   XEvent_Data * evdata = (XEvent_Data*) arg;

   if (evt->xclient.type == ClientMessage &&
         evt->xclient.message_type == XInternAtom(dpy, "TERMINATION_MSG", false)
      )
   {
      result = true;
      DBGMSG("detected termination msg");
   }
   else if (evt->xclient.type == evdata->screen_change_eventno)
   {
      result = true;
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "detected screen change");
   }
   else {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Ignoring evnt->xclient.type == %d", evt->xclient.type);
   }

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "");
   return result;
}


/** Blocks until either a XRRScreenChangeEvent or ClientMessageEvent is returned
 *
 * #param   evdata pointer to XEvent_Data
 * @return  true  received ScreenChangeNotify event
 *          false received termination event
 */
bool dw_next_X11_event_of_interest(XEvent_Data * evdata) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,"evdata=%p", evdata);

   bool result = false;

   XEvent event_return;

   // XNextEvent(evdata->dpy, &event_return);  // temp

   XIfEvent(evdata->dpy, &event_return, dw_is_ddc_event, (XPointer) evdata);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "XIfEvent returned");

   if (event_return.xclient.type == ClientMessage &&
       event_return.xclient.message_type ==  XInternAtom(dpy, "TERMINATION_MSG", false) )
   {
      DBGMSG("received termination msg");
      result = false;
   }
   else if (event_return.xclient.type == evdata->screen_change_eventno) {
      DBGMSG("received screen changed event");
      result = true;

      XEvent event;
      bool more = true;
      int flushct = 0;
      while (more) {
         more = XCheckTypedEvent(evdata->dpy, evdata->screen_change_eventno, &event);
         flushct++;
      }
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Flushed %d events", flushct);
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, result, "");
   return result;
}


void init_ddc_watch_displays_xevent() {
   RTTI_ADD_FUNC(ddc_detect_xevent_screen_change);
   RTTI_ADD_FUNC(ddc_init_xevent_screen_change_notification);
   RTTI_ADD_FUNC(dw_next_X11_event_of_interest);
   RTTI_ADD_FUNC(ddc_send_x11_termination_message);
   RTTI_ADD_FUNC(dw_is_ddc_event);
}
