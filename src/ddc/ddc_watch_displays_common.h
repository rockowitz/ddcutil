// ddc_watch_displays_common.h

// Copyright (C) 2018-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_WATCH_DISPLAYS_COMMON_H_
#define DDC_WATCH_DISPLAYS_COMMON_H_

#include <glib-2.0/glib.h>

#include "util/data_structures.h"

extern bool      terminate_watch_thread;

void ddc_i2c_emit_deferred_events(GArray * deferred_events);

bool ddc_i2c_hotplug_change_handler(
      Bit_Set_256    bs_buses_w_edid_removed,
      Bit_Set_256    bs_buses_w_edid_added,
      GArray * events_queue);

void init_ddc_watch_displays_common();

#endif /* DDC_WATCH_DISPLAYS_COMMON_H_ */
