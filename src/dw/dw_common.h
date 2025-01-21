/** @file dw_common.h */

// Copyright (C) 2018-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_COMMON_H_
#define DW_COMMON_H_

#include <glib-2.0/glib.h>
#include <sys/types.h>

#include "util/data_structures.h"
#include "util/linux_util.h"

#include "base/displays.h"

#include "dw_xevent.h"

extern int       initial_stabilization_millisec;
extern int       stabilization_poll_millisec;
extern int       udev_watch_loop_millisec;
extern int       poll_watch_loop_millisec;
extern int       xevent_watch_loop_millisec;
extern int       calculated_watch_loop_millisec;
extern bool      terminate_using_x11_event;

int  dw_calc_watch_loop_millisec(DDC_Watch_Mode watch_mode);
int  dw_split_sleep(int watch_loop_millisec);
void dw_terminate_if_invalid_thread_or_process(pid_t cur_pid, pid_t cur_tid);

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
   DDC_Watch_Mode           watch_mode;
   int                      watch_loop_millisec;
   XEvent_Data *            evdata;
  } Watch_Displays_Data;

void dw_free_watch_displays_data(Watch_Displays_Data * wdd);

#ifdef UNUSED
typedef struct {
   Bit_Set_256 all_displays;
   Bit_Set_256 displays_w_edid;
}  Bit_Set_256_Pair;
bool bs256_pair_eq(Bit_Set_256_Pair pair1, Bit_Set_256_Pair pair2);
#endif

Bit_Set_256
dw_i2c_stabilized_buses_bs(Bit_Set_256 bs_prior, bool some_displays_disconnected);

#ifdef WATCH_ASLEEP
Bit_Set_256 ddc_i2c_check_bus_asleep(
      Bit_Set_256  bs_active_buses,
      Bit_Set_256  bs_sleepy_buses,
      GArray*      events_queue);
#endif

void dw_i2c_emit_deferred_events(GArray * deferred_events);

bool dw_i2c_hotplug_change_handler(
      Bit_Set_256    bs_buses_w_edid_removed,
      Bit_Set_256    bs_buses_w_edid_added,
      GArray * events_queue,
      GPtrArray * drefs_to_recheck);

void init_dw_common();

#endif /* DW_COMMON_H_ */
