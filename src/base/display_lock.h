/* @file display_lock.h
 */

// Copyright (C) 2018-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DISPLAY_LOCK_H_
#define DISPLAY_LOCK_H_

#include <stdbool.h>
#include <stdint.h>

#include "ddcutil_types.h"

#include "util/error_info.h"
#include "base/core.h"
#include "base/displays.h"


typedef enum {
   DDISP_NONE  = 0x00,     ///< No flags set
   DDISP_WAIT  = 0x01      ///< If set, #lock_display() should wait
} Display_Lock_Flags;

#define DISPLAY_LOCK_MARKER "DDSC"
typedef struct {
   char         marker[4];
   DDCA_IO_Path io_path;
   GMutex       display_mutex;
   GThread *    display_mutex_thread;     // thread owning mutex
   intmax_t     linux_thread_id;
} Display_Lock_Record;


Display_Lock_Record * create_display_lock_record(DDCA_IO_Path io_path);
void                  terminate_i2c_display_lock();
Error_Info *          lock_display(Display_Lock_Record * id, Display_Lock_Flags flags);
Error_Info *          lock_display2(Display_Lock_Record * ddesc, Display_Lock_Flags flags);
Error_Info *          lock_display_by_dpath(DDCA_IO_Path dpath, Display_Lock_Flags flags);
Error_Info *          unlock_display(Display_Lock_Record * id);
Error_Info *          unlock_display2(Display_Lock_Record * ddesc);
Error_Info *          unlock_display_by_dpath(DDCA_IO_Path dpath);
void                  dbgrpt_display_locks(int depth);
char *                interpret_display_lock_flags_t(Display_Lock_Flags lock_flags);
void                  init_i2c_display_lock(void);

#endif /* DISPLAY_LOCK_H_ */
