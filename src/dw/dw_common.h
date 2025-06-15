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

#ifdef USE_X11
#include "dw_xevent.h"
#endif

extern uint16_t   initial_stabilization_millisec;
extern uint16_t   stabilization_poll_millisec;
extern uint16_t   udev_watch_loop_millisec;
extern uint16_t   poll_watch_loop_millisec;
extern uint16_t   xevent_watch_loop_millisec;
extern bool       terminate_watch_thread;
extern bool       terminate_using_x11_event;

uint32_t  dw_calc_watch_loop_millisec(DDC_Watch_Mode watch_mode);
uint32_t  dw_split_sleep(int watch_loop_millisec);
void      dw_terminate_if_invalid_thread_or_process(pid_t cur_pid, pid_t cur_tid);

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
#ifdef USE_X11
   XEvent_Data *            evdata;
#endif
  } Watch_Displays_Data;

void dw_free_watch_displays_data(Watch_Displays_Data * wdd);


#define RECHECK_DISPLAYS_DATA_MARKER "RDDM"
typedef struct {
   char                     marker[4];
   pid_t                    main_process_id;  //?
   pid_t                    main_thread_id;   //?
  } Recheck_Displays_Data;

void dw_free_recheck_displays_data(Recheck_Displays_Data * rdd);


#define CALLBACK_DISPLAYS_DATA_MARKER "CDDM"
typedef struct {
   char                     marker[4];
   pid_t                    main_process_id;  //?
// pid_t                    main_thread_id;   //?
  } Callback_Displays_Data;

Callback_Displays_Data * dw_new_callback_displays_data();
void dw_free_callback_displays_data(Callback_Displays_Data * rdd);


#ifdef UNUSED
typedef struct {
   Bit_Set_256 all_displays;
   Bit_Set_256 displays_w_edid;
}  Bit_Set_256_Pair;
bool bs256_pair_eq(Bit_Set_256_Pair pair1, Bit_Set_256_Pair pair2);
#endif

Bit_Set_256
dw_stabilized_buses_bs(Bit_Set_256 bs_prior, bool some_displays_disconnected);

#ifdef WATCH_ASLEEP
Bit_Set_256 ddc_i2c_check_bus_asleep(
      Bit_Set_256  bs_active_buses,
      Bit_Set_256  bs_sleepy_buses,
      GArray*      events_queue);
#endif

void dw_emit_deferred_events(GArray * deferred_events);

bool dw_hotplug_change_handler(
      Bit_Set_256    bs_buses_w_edid_removed,
      Bit_Set_256    bs_buses_w_edid_added,
      GArray * events_queue,
      GPtrArray * drefs_to_recheck);

void record_active_callback_thread(GThread* thread);
void remove_active_callback_thread(GThread* thread);
int  active_callback_thread_ct();

void init_dw_common();

#endif /* DW_COMMON_H_ */
