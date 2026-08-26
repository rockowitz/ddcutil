/** @file test_linux_util.c
 *
 *  Standalone unit tests for host-independent functions in src/util/linux_util.c.
 *
 *  is_readable_file() is checked against temporary files (it returns true only
 *  when at least one byte can actually be read).  The kernel-config, module,
 *  and lsof helpers depend on the host and are not exercised.
 *
 *  The suspend and resume detection functions that formerly lived here moved
 *  to src/util/suspend_resume_util.c; their checks are in
 *  test_suspend_resume_util.c.
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/linux_util.h"

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

int main(int argc, char ** argv) {
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

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
