/** @file dw_base.c */

// Copyright (C) 2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <glib-2.0/glib.h>
#include <string.h>

#include "public/ddcutil_types.h"

#include "util/glib_util.h"
#include "util/timestamp.h"

#include "displays.h"

#include "dw_base.h"


const char * dw_display_event_class_name(DDCA_Display_Event_Class class) {
   char * result = NULL;
   switch(class) {
   case DDCA_EVENT_CLASS_NONE:               result = "DDCA_EVENT_CLASS_NONE";               break;
   case DDCA_EVENT_CLASS_DPMS:               result = "DDCA_EVENT_CLASS_DPMS";               break;
   case DDCA_EVENT_CLASS_DISPLAY_CONNECTION: result = "DDCA_EVENT_CLASS_DISPLAY_CONNECTION"; break;
   case DDCA_EVENT_CLASS_UNUSED1:            result = "DDCA_EVENT_CLASS_UNUSED1";            break;
   }
   return result;
}


void dw_event_classes_repr(char * buf, int bufsz, DDCA_Display_Event_Class classes) {
   assert(buf && bufsz >= 100);
   g_snprintf(buf,100, "%s%s%s",
         (classes&DDCA_EVENT_CLASS_DPMS)               ? "DDCA_EVENT_CLASS_DPMS," : "",
         (classes&DDCA_EVENT_CLASS_DISPLAY_CONNECTION) ? "DDCA_EVENT_CLASS_DISPLAY_CONNECTION," : "",
         (classes&DDCA_EVENT_CLASS_UNUSED1)            ? "DDCA_EVENT_CLASS_UNUSED1," : "");
   if (strlen(buf) == 0)
      strcpy(buf,"NONE,");
   assert(strlen(buf) > 0);   // avoid coverity warning
   buf[strlen(buf)-1] = '\0';
   return;
}


char * dw_event_classes_repr_t(DDCA_Display_Event_Class classes) {
   static GPrivate  display_status_repr_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&display_status_repr_key, 100);
   dw_event_classes_repr(buf, 100, classes);
   return buf;
}


const char * dw_display_event_type_name(DDCA_Display_Event_Type event_type) {
   char * result = NULL;
   switch(event_type) {
   case DDCA_EVENT_DISPLAY_CONNECTED:    result = "DDCA_EVENT_DISPLAY_CONNECTED";    break;
   case DDCA_EVENT_DISPLAY_DISCONNECTED: result = "DDCA_EVENT_DISPLAY_DISCONNECTED"; break;
   case DDCA_EVENT_DPMS_AWAKE:           result = "DDCA_EVENT_DPMS_AWAKE";           break;
   case DDCA_EVENT_DPMS_ASLEEP:          result = "DDCA_EVENT_DPMS_ASLEEP";          break;
   case DDCA_EVENT_DDC_ENABLED:          result = "DDCA_EVENT_DDC_ENABLED";          break;
   case DDCA_EVENT_UNUSED2:              result = "DDCA_EVENT_UNUSED2";              break;
   }
   return result;
}


char * display_status_event_repr(DDCA_Display_Status_Event evt) {
   char * s = g_strdup_printf(
      "DDCA_Display_Status_Event[%s:  %s, %s, dref: %s, io_path:/dev/i2c-%d, ddc working: %s]",
      formatted_time_t(evt.timestamp_nanos),   // will this clobber a wrapping DBGTRC?
      dw_display_event_type_name(evt.event_type),
                                  evt.connector_name,
                                  ddci_dref_repr_t(evt.dref),
                                  evt.io_path.path.i2c_busno,
                                  sbool(evt.flags&DDCA_DISPLAY_EVENT_DDC_WORKING));
   return s;
}


char * display_status_event_repr_t(DDCA_Display_Status_Event evt) {
   static GPrivate  display_status_repr_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&display_status_repr_key, 200);

   char * repr = display_status_event_repr(evt);
   g_snprintf(buf, 200, "%s", repr);
   free(repr);
   return buf;
}
