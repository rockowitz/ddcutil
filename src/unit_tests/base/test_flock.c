/** @file test_flock.c
 *
 *  Standalone unit test for src/base/flock.c: taking and releasing an advisory
 *  lock on a file descriptor via flock_lock_by_fd / flock_unlock_by_fd against a
 *  temporary file.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libbase unit test: it links the internal libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "base/status_code_mgt.h"    // init_status_code_mgt (psc_desc on error paths)
#include "base/flock.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

int main(int argc, char ** argv) {
   init_status_code_mgt();
   init_flock();
   i2c_enable_cross_instance_locks(true);

   char path[] = "/tmp/test_flock_XXXXXX";
   int fd = mkstemp(path);
   CK(fd >= 0);

   // an uncontended lock succeeds, and can be released and re-taken
   CK(flock_lock_by_fd(fd, path, true) == 0);
   CK(flock_unlock_by_fd(fd) == 0);
   CK(flock_lock_by_fd(fd, path, true) == 0);
   CK(flock_unlock_by_fd(fd) == 0);

   close(fd);
   unlink(path);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
