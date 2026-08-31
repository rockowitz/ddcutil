/** @file test_dw_common.c
 *
 *  Standalone unit tests for src/dw/dw_common.c: dw_calc_watch_loop_millisec(),
 *  the termination eventfd lifecycle (dw_create_terminate_eventfd()/
 *  dw_signal_terminate_eventfd()/dw_close_terminate_eventfd()) and its use by
 *  dw_split_sleep(), dw_terminate_if_invalid_thread_or_process() (valid-thread
 *  path only -- the invalid-thread path calls g_thread_exit() and would end
 *  the calling thread, so it is not exercised here), dw_free_watch_displays_data(),
 *  and the active-callback-thread bookkeeping (record/remove/count), which is
 *  a plain GHashTable keyed by pointer identity -- the GThread* values are
 *  never dereferenced, so arbitrary non-NULL pointers stand in for them.
 *
 *  dw_hotplug_change_handler() and dw_stabilized_buses_bs() are not tested:
 *  both perform real I2C bus probing (i2c_check_bus(), a live ioctl-level
 *  hardware scan) with no way to substitute fabricated state.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: dw source files cross-reference each other
 *  and the rest of the ddcutil core extensively, so it links the full
 *  top-level libcommon convenience library (the same aggregate the
 *  ddcutil executable itself links) rather than a minimal per-directory
 *  library set.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/displays.h"

#include "dw/dw_common.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define CK_INT(expr, expected) do { \
   total++; \
   long _a = (long)(expr); long _e = (long)(expected); \
   if (_a != _e) { failed++; \
      printf("FAIL  line %-4d  %s -> %ld, expected %ld\n", __LINE__, #expr, _a, _e); } \
} while(0)


static void test_dw_calc_watch_loop_millisec(void) {
   udev_watch_loop_millisec   = 111;
   xevent_watch_loop_millisec = 222;
   poll_watch_loop_millisec   = 333;

   CK_INT(dw_calc_watch_loop_millisec(Watch_Mode_Udev),   111);
   CK_INT(dw_calc_watch_loop_millisec(Watch_Mode_Xevent), 222);
   CK_INT(dw_calc_watch_loop_millisec(Watch_Mode_Poll),   333);
}


static void test_terminate_eventfd_lifecycle(void) {
   CK_INT(terminate_watch_thread_fd, -1);   // nothing created yet

   // no-op unless use_eventfd or split_sleep_eventfd is set
   use_eventfd = false;
   split_sleep_eventfd = false;
   dw_create_terminate_eventfd();
   CK_INT(terminate_watch_thread_fd, -1);

   use_eventfd = true;
   dw_create_terminate_eventfd();
   CK(terminate_watch_thread_fd >= 0);

   dw_signal_terminate_eventfd();   // must not crash; writes to a real fd
   CK(true);

   dw_close_terminate_eventfd();
   CK_INT(terminate_watch_thread_fd, -1);

   // signaling/closing with no fd created must be safe no-ops
   dw_signal_terminate_eventfd();
   dw_close_terminate_eventfd();
   CK(true);

   use_eventfd = false;
}


static void test_dw_split_sleep_terminate_signaled(void) {
   split_sleep_eventfd = true;
   dw_create_terminate_eventfd();
   CK(terminate_watch_thread_fd >= 0);

   dw_signal_terminate_eventfd();   // pending data makes poll() return immediately

   gint64 start_us = g_get_monotonic_time();
   uint32_t slept_ms = dw_split_sleep(2000);   // would otherwise take ~2 real seconds
   gint64 elapsed_ms = (g_get_monotonic_time() - start_us) / 1000;

   CK(elapsed_ms < 500);      // returned promptly, not after the full 2000ms
   CK(slept_ms <= 2000);

   dw_close_terminate_eventfd();
   split_sleep_eventfd = false;
}


static void test_dw_split_sleep_short_timed(void) {
   // non-eventfd path: plain segmented usleep(), capped at 200ms per segment
   split_sleep_eventfd = false;
   terminate_watch_thread = false;

   gint64 start_us = g_get_monotonic_time();
   uint32_t slept_ms = dw_split_sleep(5);   // shorter than one 200ms segment
   gint64 elapsed_ms = (g_get_monotonic_time() - start_us) / 1000;

   CK(slept_ms <= 5);
   CK(elapsed_ms < 200);   // did not round up to a full 200ms segment
}


// dw_sleep_spent_by_suspend() times its sleep on CLOCK_BOOTTIME and reports
// true when the elapsed time exceeds the requested interval by more than
// PAUSE_SUSPENDED_SLACK_MS -- the signature of a sleep the system froze rather
// than served.  A SIGSTOP reproduces that signature: the process makes no
// progress while BOOTTIME keeps advancing, exactly as during a suspend.  A
// child does the stopping and the continuing, since a stopped process cannot
// resume itself.
static void test_dw_sleep_spent_by_suspend(void) {
   split_sleep_eventfd = false;
   terminate_watch_thread = false;

   // undisturbed: the sleep is served, so not spent by a suspend
   CK(dw_sleep_spent_by_suspend(200) == false);

   pid_t parent = getpid();
   pid_t child = fork();
   if (child == 0) {
      usleep(100 * 1000);            // let the parent enter its sleep
      kill(parent, SIGSTOP);
      usleep(1600 * 1000);           // longer than PAUSE_SUSPENDED_SLACK_MS (1000)
      kill(parent, SIGCONT);
      _exit(0);
   }
   CK(child > 0);
   if (child > 0) {
      // Stopped ~100ms into a 200ms sleep and resumed ~1.7s later, so BOOTTIME
      // elapsed is ~1.8s against a 200ms request: well past the slack.
      CK(dw_sleep_spent_by_suspend(200) == true);
      int status;
      waitpid(child, &status, 0);
   }
}


static void test_dw_terminate_if_invalid_thread_or_process_valid(void) {
   pid_t cur_pid = getpid();
   pid_t cur_tid = (pid_t) syscall(SYS_gettid);
   // both refer to the calling process/thread, so is_valid_thread_or_process()
   // must find them and this must return normally rather than calling
   // g_thread_exit().
   dw_terminate_if_invalid_thread_or_process(cur_pid, cur_tid);
   CK(true);
}


static void test_dw_free_watch_displays_data(void) {
   Watch_Displays_Data * wdd = calloc(1, sizeof(Watch_Displays_Data));
   memcpy(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4);
   dw_free_watch_displays_data(wdd);   // must not crash; evdata is NULL

   dw_free_watch_displays_data(NULL);   // must not crash
   CK(true);
}


static void test_active_callback_threads(void) {
   CK_INT(active_callback_thread_ct(), 0);

   // GHashTable keyed by pointer identity: the values are never
   // dereferenced as GThread*, so any distinct non-NULL pointers work.
   GThread * fake1 = (GThread *)(intptr_t) 0x1001;
   GThread * fake2 = (GThread *)(intptr_t) 0x1002;

   record_active_callback_thread(fake1);
   CK_INT(active_callback_thread_ct(), 1);

   record_active_callback_thread(fake2);
   CK_INT(active_callback_thread_ct(), 2);

   record_active_callback_thread(fake1);   // already present, not an error
   CK_INT(active_callback_thread_ct(), 2);

   remove_active_callback_thread(fake1);
   CK_INT(active_callback_thread_ct(), 1);

   remove_active_callback_thread(fake1);   // already removed, must not crash
   CK_INT(active_callback_thread_ct(), 1);

   remove_active_callback_thread(fake2);
   CK_INT(active_callback_thread_ct(), 0);
}


int main(int argc, char ** argv) {
   test_dw_calc_watch_loop_millisec();
   test_terminate_eventfd_lifecycle();
   test_dw_split_sleep_terminate_signaled();
   test_dw_split_sleep_short_timed();
   test_dw_sleep_spent_by_suspend();
   test_dw_terminate_if_invalid_thread_or_process_valid();
   test_dw_free_watch_displays_data();
   test_active_callback_threads();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
