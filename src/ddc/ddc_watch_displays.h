/** \file ddc_watch_displays.h - Watch for monitor addition and removal
 */

// Copyright (C) 2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_WATCH_DISPLAYS_H_
#define DDC_WATCH_DISPLAYS_H_

/** \cond */
#include "util/error_info.h"
/** \endcond */

typedef enum {Changed_None = 0,
              Changed_Added = 1,
              Changed_Removed = 2,
              Changed_Both = 3
} Displays_Change_Type;

typedef void (*Display_Change_Handler)(Displays_Change_Type change_type);

#define WATCH_DISPLAYS_DATA_MARKER "WDDM"
typedef struct {
   char                   marker[4];
   Display_Change_Handler display_change_handler;
} Watch_Displays_Data;


Error_Info * ddc_start_watch_displays();

void dummy_display_change_handler(Displays_Change_Type change_type);

#endif /* DDC_WATCH_DISPLAYS_H_ */
