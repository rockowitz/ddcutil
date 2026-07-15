/** @file test_debug_util.c
 *
 *  Standalone unit tests for the functions in src/util/debug_util.c.
 *
 *  debug_util's simple_dbgmsg() writes to stdout, so the tests redirect the
 *  stdout file descriptor to a temporary file to capture and check the exact
 *  output.  Prints one line per failing check and a summary; exit status is 0
 *  if all checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/debug_util.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, _a, _e); } \
} while(0)

/* Runs statement `stmt` with stdout (fd 1) redirected to a temporary file,
 * then restores stdout and copies the captured bytes into char array `dst`. */
#define CAPTURE(dst, stmt) do { \
   fflush(stdout); \
   int _saved = dup(fileno(stdout)); \
   FILE * _tmp = tmpfile(); \
   dup2(fileno(_tmp), fileno(stdout)); \
   stmt; \
   fflush(stdout); \
   dup2(_saved, fileno(stdout)); \
   close(_saved); \
   rewind(_tmp); \
   size_t _n = fread((dst), 1, sizeof(dst) - 1, _tmp); \
   (dst)[_n] = '\0'; \
   fclose(_tmp); \
} while(0)

int main(int argc, char ** argv) {
   bool r;
   char out[256];

   // debug_flag == false: no output, returns false
   r = true;
   CAPTURE(out, r = simple_dbgmsg(false, "func", 10, "file.c", "should not appear"));
   CK(r == false);
   CK_STR(out, "");

   // default min funcname size is 30: funcname is left-justified in a 30-wide
   // field, wrapped in "(...) ", followed by the formatted message, no newline
   char expected30[256];
   snprintf(expected30, sizeof(expected30), "(%-*s) %s", 30, "abc", "hello");
   r = false;
   CAPTURE(out, r = simple_dbgmsg(true, "abc", 1, "file.c", "hello"));
   CK(r == true);
   CK_STR(out, expected30);

   // set a small field width and check the exact literal output
   set_simple_dbgmsg_min_funcname_size(5);
   CAPTURE(out, simple_dbgmsg(true, "f", 1, "file.c", "hello"));
   CK_STR(out, "(f    ) hello");     // "f" left-justified in 5, no trailing newline

   // variadic message arguments are substituted
   CAPTURE(out, simple_dbgmsg(true, "f", 1, "file.c", "x=%d y=%s z=%02x", 42, "hi", 10));
   CK_STR(out, "(f    ) x=42 y=hi z=0a");

   // funcname longer than the field width is not truncated (it is a minimum)
   CAPTURE(out, simple_dbgmsg(true, "longfuncname", 1, "file.c", "m"));
   CK_STR(out, "(longfuncname) m");

   // empty format string
   CAPTURE(out, simple_dbgmsg(true, "f", 1, "file.c", "%s", ""));
   CK_STR(out, "(f    ) ");

   // changing the field width takes effect
   set_simple_dbgmsg_min_funcname_size(10);
   CAPTURE(out, simple_dbgmsg(true, "f", 1, "file.c", "q"));
   CK_STR(out, "(f         ) q");   // "f" left-justified in 10

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
