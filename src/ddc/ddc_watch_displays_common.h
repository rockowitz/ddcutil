// ddc_watch_displays_common.h

// Copyright (C) 2018-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_WATCH_DISPLAYS_COMMON_H_
#define DDC_WATCH_DISPLAYS_COMMON_H_

#include <glib-2.0/glib.h>

#include "util/data_structures.h"

extern bool      terminate_watch_thread;
extern bool           ddc_slow_watch;

typedef void (*Display_Change_Handler)(
                 GPtrArray *          buses_removed,
                 GPtrArray *          buses_added,
                 GPtrArray *          connectors_removed,
                 GPtrArray *          connectors_added);

#define WATCH_DISPLAYS_DATA_MARKER "WDDM"
typedef struct {
   char                     marker[4];
   pid_t                    main_process_id;
   pid_t                    main_thread_id;
   DDCA_Display_Event_Class event_classes;
// #ifdef OLD_HOTPLUG_VERSION
   Display_Change_Handler display_change_handler;
   Bit_Set_32             drm_card_numbers;
// #endif
} Watch_Displays_Data;

void free_watch_displays_data(Watch_Displays_Data * wdd);


void ddc_i2c_emit_deferred_events(GArray * deferred_events);

bool ddc_i2c_hotplug_change_handler(
      Bit_Set_256    bs_buses_w_edid_removed,
      Bit_Set_256    bs_buses_w_edid_added,
      GArray * events_queue);

void init_ddc_watch_displays_common();

#endif /* DDC_WATCH_DISPLAYS_COMMON_H_ */
