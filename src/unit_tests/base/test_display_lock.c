/** @file test_display_lock.c
 *
 *  Standalone unit tests for src/base/display_lock.c: the lock-flag interpreter,
 *  lock-record creation, the lock/unlock round trip by device path (including
 *  the same-thread re-lock rejection), unlock_all_displays_for_current_thread,
 *  and reset_display_locks_table.
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

#include "public/ddcutil_types.h"
#include "public/ddcutil_status_codes.h"
#include "util/error_info.h"
#include "base/execution_stats.h"    // init_execution_stats
#include "base/display_lock.h"

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

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)

// stdout+stderr muting, so the backtrace on the re-lock error path stays out
// of the log.
static int saved_out = -1, saved_err = -1;
static void mute(void) {
   fflush(stdout); fflush(stderr);
   saved_out = dup(STDOUT_FILENO);
   saved_err = dup(STDERR_FILENO);
   int devnull = open("/dev/null", O_WRONLY);
   dup2(devnull, STDOUT_FILENO);
   dup2(devnull, STDERR_FILENO);
   close(devnull);
}
static void unmute(void) {
   fflush(stdout); fflush(stderr);
   dup2(saved_out, STDOUT_FILENO);
   dup2(saved_err, STDERR_FILENO);
   close(saved_out); close(saved_err);
   saved_out = saved_err = -1;
}

static DDCA_IO_Path i2c_path(int busno) {
   DDCA_IO_Path p;
   memset(&p, 0, sizeof(p));
   p.io_mode = DDCA_IO_I2C;
   p.path.i2c_busno = busno;
   return p;
}

static void test_flags(void) {
   CK_STR(interpret_display_lock_flags_t(DDISP_WAIT), "DDISP_WAIT");
   CK_STR(interpret_display_lock_flags_t(DDISP_NONE), "DDISP_NONE");
}

static void test_create_record(void) {
   Display_Lock_Record * rec = create_display_lock_record(i2c_path(5));
   CK(rec != NULL);
   CK(memcmp(rec->marker, "DDSC", 4) == 0);
   CK_INT(rec->io_path.io_mode, DDCA_IO_I2C);
   CK_INT(rec->io_path.path.i2c_busno, 5);
   g_mutex_clear(&rec->display_mutex);
   free(rec);
}

static void test_lock_unlock(void) {
   DDCA_IO_Path p = i2c_path(5);

   // first lock succeeds
   CK(lock_display_by_dpath(p, DDISP_WAIT) == NULL);

   // a second lock from the same thread is rejected
   mute();
   Error_Info * err = lock_display_by_dpath(p, DDISP_WAIT);
   unmute();
   CK(err != NULL);
   CK_INT(ERRINFO_STATUS(err), DDCRC_ALREADY_OPEN);
   errinfo_free(err);

   // unlock, then the same path can be locked again
   CK(unlock_display_by_dpath(p) == NULL);
   CK(lock_display_by_dpath(p, DDISP_WAIT) == NULL);
   CK(unlock_display_by_dpath(p) == NULL);
}

static void test_unlock_all(void) {
   CK(lock_display_by_dpath(i2c_path(5), DDISP_WAIT) == NULL);
   CK(lock_display_by_dpath(i2c_path(6), DDISP_WAIT) == NULL);
   CK_INT(unlock_all_displays_for_current_thread(), 2);   // both released
   CK_INT(unlock_all_displays_for_current_thread(), 0);   // nothing left owned
}

static void test_reset(void) {
   // reset deletes the unlocked records; a subsequent lock still works
   reset_display_locks_table();
   CK(lock_display_by_dpath(i2c_path(7), DDISP_WAIT) == NULL);
   CK(unlock_display_by_dpath(i2c_path(7)) == NULL);
}

int main(int argc, char ** argv) {
   init_execution_stats();
   init_i2c_display_lock();

   test_flags();
   test_create_record();
   test_lock_unlock();
   test_unlock_all();
   test_reset();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
