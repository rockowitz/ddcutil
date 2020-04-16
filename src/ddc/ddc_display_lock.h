/* @file ddc_display_lock.h
 */

// Copyright (C) 2018-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_DISPLAY_LOCK_H_
#define DDC_DISPLAY_LOCK_H_

#include <stdbool.h>

#include "ddcutil_types.h"

#include "base/core.h"
#include "base/displays.h"

typedef enum {
   DDISP_NONE  = 0x00,     ///< No flags set
   DDISP_WAIT  = 0x01      ///< If true, #lock_distinct_display() should wait
} Distinct_Display_Flags;

typedef void * Distinct_Display_Ref;

void init_ddc_display_lock(void);

Distinct_Display_Ref get_distinct_display_ref(Display_Ref * dref);

DDCA_Status lock_distinct_display(Distinct_Display_Ref id, Distinct_Display_Flags flags);

DDCA_Status unlock_distinct_display(Distinct_Display_Ref id);

void dbgrpt_distinct_display_descriptors(int depth);

#endif /* DDC_DISPLAY_LOCK_H_ */
