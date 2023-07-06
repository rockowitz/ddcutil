/* @file ddc_display_lock.h
 */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_DISPLAY_LOCK_H_
#define DDC_DISPLAY_LOCK_H_

#include <stdbool.h>

#include "ddcutil_types.h"

#include "util/error_info.h"
#include "base/core.h"
#include "base/displays.h"

typedef enum {
   DDISP_NONE  = 0x00,     ///< No flags set
   DDISP_WAIT  = 0x01      ///< If true, #lock_distinct_display() should wait
} Display_Lock_Flags;

typedef void * Distinct_Display_Ref;

void                 init_ddc_display_lock(void);
void                 terminate_ddc_display_lock();
Distinct_Display_Ref get_distinct_display_ref(Display_Ref * dref);
Error_Info *         lock_display(Distinct_Display_Ref id, Display_Lock_Flags flags);
Error_Info *         unlock_display(Distinct_Display_Ref id);
void                 dbgrpt_display_locks(int depth);

#endif /* DDC_DISPLAY_LOCK_H_ */
