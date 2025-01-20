// common_inlines.h

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
extern __thread pid_t process_id;
extern __thread pid_t thread_id;

static inline pid_t tid() {
   if (!thread_id)
      thread_id = syscall(SYS_gettid);
   return thread_id;
}

static inline pid_t pid() {
   if (!process_id)
      process_id = syscall(SYS_gettid);
   return thread_id;
}

#ifdef __cplusplus
}    // extern "C"
#endif

#endif /* COMMON_INLINES_H_ */
