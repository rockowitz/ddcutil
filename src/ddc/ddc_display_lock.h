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

#define DISPLAY_LOCK_MARKER "DDSC"
typedef struct {
   char         marker[4];
   DDCA_IO_Path io_path;
   GMutex       display_mutex;
   GThread *    display_mutex_thread;     // thread owning mutex
} Display_Lock_Record;

void                  init_ddc_display_lock(void);
void                  terminate_ddc_display_lock();
Display_Lock_Record * get_display_lock_record(Display_Ref * dref);
Display_Lock_Record * get_display_lock_record_by_dpath(DDCA_IO_Path dpath);
Error_Info *          lock_display(Display_Lock_Record * id, Display_Lock_Flags flags);
Error_Info *          lock_display_by_dref(Display_Ref * dref, Display_Lock_Flags flags);
Error_Info *          lock_display_by_dpath(DDCA_IO_Path dpath, Display_Lock_Flags flags);
Error_Info *          unlock_display(Display_Lock_Record * id);
Error_Info *          unlock_display_by_dref(Display_Ref * dref);
Error_Info *          unlock_display_by_dpath(DDCA_IO_Path dpath);
void                  dbgrpt_display_locks(int depth);

#endif /* DDC_DISPLAY_LOCK_H_ */
