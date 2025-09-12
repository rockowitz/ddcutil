/** @file dw_base.h */

// Copyright (C) 2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_BASE_H_
#define DW_BASE_H_

#include "public/ddcutil_types.h"

const char * dw_display_event_class_name(DDCA_Display_Event_Class class);
void         dw_event_classes_repr(char * buf, int bufsz, DDCA_Display_Event_Class classes);
char *       dw_event_classes_repr_t(DDCA_Display_Event_Class classes);
const char*  dw_display_event_type_name(DDCA_Display_Event_Type event_type);
char *       display_status_event_repr(DDCA_Display_Status_Event evt);
char *       display_status_event_repr_t(DDCA_Display_Status_Event evt);

#endif /* DW_BASE_H_ */
