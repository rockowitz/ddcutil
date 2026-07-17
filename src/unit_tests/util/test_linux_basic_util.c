/** @file test_linux_basic_util.c
 *
 *  Standalone unit tests for src/util/linux_basic_util.c: the thread/process id
 *  accessors and validity check, uid/gid name lookups, and the file owner/group
 *  id accessor.  The group-i2c helpers depend on the host's group configuration
 *  and are not exercised.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "util/linux_basic_util.h"

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

// stdout+stderr muting, so the diagnostics on the error paths stay out of the log.
static int saved_out = -1;
static int saved_err = -1;
static void mute_stderr(void) {
   fflush(stdout); fflush(stderr);
   saved_out = dup(STDOUT_FILENO);
   saved_err = dup(STDERR_FILENO);
   int devnull = open("/dev/null", O_WRONLY);
   dup2(devnull, STDOUT_FILENO);
   dup2(devnull, STDERR_FILENO);
   close(devnull);
}
static void unmute_stderr(void) {
   fflush(stdout); fflush(stderr);
   dup2(saved_out, STDOUT_FILENO);
   dup2(saved_err, STDERR_FILENO);
   close(saved_out);
   close(saved_err);
   saved_out = saved_err = -1;
}

int main(int argc, char ** argv) {
   // process / thread ids
   CK_INT(get_process_id(), getpid());
   CK(get_thread_id() > 0);

   // validity check against /proc
   CK(is_valid_thread_or_process(getpid()) == true);
   mute_stderr();
   bool absurd = is_valid_thread_or_process(0x3fffffff);
   unmute_stderr();
   CK(absurd == false);

   // uid / gid name lookups (root is uid/gid 0 on every Linux system)
   CK_STR(uid_name(0), "root");
   CK_STR(gid_name(0), "root");
   CK_STR(uid_name(999999998), "unknown");   // no such user

   // file owner/group ids match the current user for a file we create
   char path[] = "/tmp/test_lbu_XXXXXX";
   int fd = mkstemp(path);
   if (fd >= 0) close(fd);
   uid_t uid = -1;
   gid_t gid = -1;
   CK(get_file_owner_group_ids(path, &uid, &gid) == true);
   CK_INT(uid, getuid());
   CK_INT(gid, getgid());
   unlink(path);

   // nonexistent file -> false
   mute_stderr();
   bool ok = get_file_owner_group_ids("/no/such/file", &uid, &gid);
   unmute_stderr();
   CK(ok == false);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
