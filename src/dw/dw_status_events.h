/** @file dw_status_events.h */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_STATUS_EVENTS_H_
#define DW_STATUS_EVENTS_H_

#include "glib-2.0/glib.h"

#include "public/ddcutil_types.h"

#include "base/displays.h"

typedef struct {
   DDCA_Display_Status_Callback_Func func;
   DDCA_Display_Status_Event         event;
} Callback_Queue_Entry;

gpointer dw_execute_callback_func(gpointer data);


// Display Status Events
DDCA_Status  dw_register_display_status_callback(DDCA_Display_Status_Callback_Func func);
DDCA_Status  dw_unregister_display_status_callback(DDCA_Display_Status_Callback_Func func);
DDCA_Display_Status_Event
             dw_create_display_status_event(DDCA_Display_Event_Type event_type,
                                             const char *            connector_name,
                                             Display_Ref*            dref,
                                             DDCA_IO_Path            io_path);
void         dw_emit_display_status_record(DDCA_Display_Status_Event  evt);
void         dw_emit_or_queue_display_status_event(DDCA_Display_Event_Type event_type,
                                              const char *            connector_name,
                                              Display_Ref*            dref,
                                              DDCA_IO_Path            io_path,
                                              GArray*                 queue);
void init_dw_status_events();

#endif /* DW_STATUS_EVENTS_H_ */
