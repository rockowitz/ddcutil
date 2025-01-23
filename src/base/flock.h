/** @file flock.h */

// Copyright (C) 2018-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FLOCK_H_
#define FLOCK_H_

#include <stdbool.h>

#include "public/ddcutil_status_codes.h"

extern int  flock_poll_millisec;
extern int  flock_max_wait_millisec;
extern bool debug_flock;

void         i2c_enable_cross_instance_locks(bool yesno);
Status_Errno flock_lock_by_fd(int fd, const char * filename, bool wait);
Status_Errno flock_unlock_by_fd(int fd);
void         init_flock();
#endif /* FLOCK_H_ */
