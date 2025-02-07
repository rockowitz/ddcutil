/** @file common_inlines.h */

// Copyright (C) 2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef COMMON_INLINES_H_
#define COMMON_INLINES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/syscall.h>
#include <unistd.h>

// TODO: merge tid(), pid() with linux_errno.c/h

static inline pid_t tid() {
   static __thread pid_t thread_id2;
   if (!thread_id2)
      thread_id2 = syscall(SYS_gettid);
   return thread_id2;
}

#define TID() (intmax_t) tid()

static inline pid_t pid() {
   static __thread pid_t process_id;
   if (!process_id)
      process_id = syscall(SYS_gettid);
   return process_id;
}

#define PID() (intmax_t) pid()

#ifdef __cplusplus
}    // extern "C"
#endif

#endif /* COMMON_INLINES_H_ */
