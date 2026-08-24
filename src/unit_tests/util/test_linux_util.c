/** @file test_linux_util.c
 *
 *  Standalone unit tests for host-independent functions in src/util/linux_util.c.
 *
 *  is_readable_file() is checked against temporary files (it returns true only
 *  when at least one byte can actually be read).  millisec_since_resume_detected_
 *  by_clocktime() returns UINT64_MAX until a resume reset occurs, which is the
 *  state at program start.  recently_resumed_from_sleep() is driven through the
 *  logind sleep timestamps that dbus_util.c records, which needs no bus.  The
 *  kernel-config, module, and lsof helpers depend on the host and are not
 *  exercised.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/linux_util.h"
#ifdef USE_DBUS
#include <stdatomic.h>
#include "util/dbus_util.h"
#include "util/timestamp.h"

// Internal symbols of dbus_util.c, non-static but not declared in its header.
extern _Atomic uint64_t last_resume_from_sleep_ns;
extern _Atomic uint64_t last_prepare_for_sleep_ns;
extern _Atomic uint64_t retired_prepare_for_sleep_ns;
#endif

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

// Writes content to a fresh temp file, returning its path in path_out.
static void write_tmpfile(char * path_out, const char * content) {
   strcpy(path_out, "/tmp/test_lu_XXXXXX");
   int fd = mkstemp(path_out);
   if (fd < 0) { perror("mkstemp"); exit(2); }
   if (content && *content) { ssize_t n = write(fd, content, strlen(content)); (void) n; }
   close(fd);
}

#ifdef USE_DBUS
// recently_resumed_from_sleep() reports a resume for the duration of an open
// sleep cycle, i.e. a PrepareForSleep(true) whose PrepareForSleep(false) has
// not arrived.  Driving it through the two timestamps needs no bus, and the
// clocktime detector reports nothing here: no sleep accumulates during a test
// run, so what the checks below see is the sleep-cycle rule alone.
static void test_open_sleep_cycle(void) {
   uint64_t ms = 0;

   // no cycle has been retired.  Which cycles the sleep watch thread retires
   // is checked in test_dbus_util.
   retired_prepare_for_sleep_ns = 0;

   // no cycle open, last resume long past: not a recent resume
   last_resume_from_sleep_ns = 1;                  // ~boot, long ago
   last_prepare_for_sleep_ns = 0;
   CK(recently_resumed_from_sleep(500, &ms) == false);
   CK(ms == UINT64_MAX);

   // an open cycle is reported as a resume that just occurred, whatever the
   // stale resume timestamp says
   last_prepare_for_sleep_ns = cur_boot_time_nanosec();
   CK(recently_resumed_from_sleep(500, &ms) == true);
   CK(ms == 0);

   // within_ms 0 asks whether a resume occurred within no time at all
   CK(recently_resumed_from_sleep(0, &ms) == false);

   // closing the cycle hands the answer back to the resume timestamp, which
   // is now current
   last_resume_from_sleep_ns = cur_boot_time_nanosec();
   CK(recently_resumed_from_sleep(500, &ms) == true);
   CK(ms < 500);

   // and once that timestamp is old, no resume is reported
   last_resume_from_sleep_ns = 1;
   last_prepare_for_sleep_ns = 0;
   CK(recently_resumed_from_sleep(500, &ms) == false);

   // a retired cycle is not a resume either: the prepare signal is still
   // unmatched, but the thread has concluded its counterpart is not coming
   last_prepare_for_sleep_ns    = cur_boot_time_nanosec();
   retired_prepare_for_sleep_ns = last_prepare_for_sleep_ns;
   CK(recently_resumed_from_sleep(500, &ms) == false);
}
#endif

int main(int argc, char ** argv) {
   // no resume reset has occurred at program start
   CK(millisec_since_resume_detected_by_clocktime() == UINT64_MAX);

   // a file with content is readable
   char path[64];
   write_tmpfile(path, "hello");
   CK(is_readable_file(path) == true);
   unlink(path);

   // an empty file yields no readable byte -> false
   write_tmpfile(path, "");
   CK(is_readable_file(path) == false);
   unlink(path);

   // /dev/null is immediately at EOF -> no byte read -> false
   CK(is_readable_file("/dev/null") == false);

   // a nonexistent file cannot be opened -> false
   CK(is_readable_file("/no/such/file/anywhere") == false);

#ifdef USE_DBUS
   test_open_sleep_cycle();
#endif

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
