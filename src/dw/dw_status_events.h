/** @file dw_status_events.h */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_STATUS_EVENTS_H_
#define DW_STATUS_EVENTS_H_

#include "glib-2.0/glib.h"

#include "public/ddcutil_types.h"

#include "base/displays.h"

// Display Status Events
DDCA_Status  ddc_register_display_status_callback(DDCA_Display_Status_Callback_Func func);
DDCA_Status  ddc_unregister_display_status_callback(DDCA_Display_Status_Callback_Func func);
const char * ddc_display_event_class_name(DDCA_Display_Event_Class class);
const char*  ddc_display_event_type_name(DDCA_Display_Event_Type event_type);
char *       display_status_event_repr(DDCA_Display_Status_Event evt);
char *       display_status_event_repr_t(DDCA_Display_Status_Event evt);
DDCA_Display_Status_Event
             ddc_create_display_status_event(DDCA_Display_Event_Type event_type,
                                             const char *            connector_name,
                                             Display_Ref*            dref,
                                             DDCA_IO_Path            io_path);
void         ddc_emit_display_status_record(DDCA_Display_Status_Event  evt);
void         ddc_emit_or_queue_display_status_event(DDCA_Display_Event_Type event_type,
                                              const char *            connector_name,
                                              Display_Ref*            dref,
                                              DDCA_IO_Path            io_path,
                                              GArray*                 queue);
void init_dw_status_events();

#endif /* DW_STATUS_EVENTS_H_ */
