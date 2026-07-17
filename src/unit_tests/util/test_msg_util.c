/** @file test_msg_util.c
 *
 *  Standalone unit tests for the functions in src/util/msg_util.c.
 *
 *  get_msg_decoration() builds a message prefix whose contents depend on a set
 *  of global flags, the suspend flag, and the traced function stack.  The tests
 *  drive those inputs and check the resulting prefix; the parts that vary at
 *  runtime (elapsed/wall time, thread id) are checked structurally, while the
 *  function-name field, which is fully determined by the inputs, is checked
 *  against exact output.  formatted_wall_time() uses the current time, so only
 *  its fixed "%b %d %T" layout is verified.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
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
#include <sys/types.h>

#include "util/msg_util.h"
#include "util/traced_function_stack.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)

// Resets every global that influences get_msg_decoration() to its quiet state.
static void reset_flags(void) {
   dbgtrc_show_time       = false;
   dbgtrc_show_wall_time  = false;
   dbgtrc_show_thread_id  = false;
   dbgtrc_show_process_id = false;
   msg_decoration_suspended = false;
   traced_function_stack_enabled = false;
   reset_current_traced_function_stack();
}

int main(int argc, char ** argv) {
   char buf[128];

   // suspended: prefix is empty regardless of other settings
   reset_flags();
   msg_decoration_suspended = true;
   dbgtrc_show_thread_id = true;
   CK_STR(get_msg_decoration(buf, sizeof(buf), false), "");

   // nothing enabled: empty prefix, no trailing space appended
   reset_flags();
   CK_STR(get_msg_decoration(buf, sizeof(buf), false), "");

   // return value is the passed-in buffer
   reset_flags();
   CK(get_msg_decoration(buf, sizeof(buf), false) == buf);

   // syslog destination always emits elapsed time (bracketed) and a trailing space
   reset_flags();
   get_msg_decoration(buf, sizeof(buf), true);
   CK(strchr(buf, '[') != NULL);
   CK(strlen(buf) > 0 && buf[strlen(buf) - 1] == ' ');

   // elapsed-time flag produces a bracketed field for the non-syslog case
   reset_flags();
   dbgtrc_show_time = true;
   get_msg_decoration(buf, sizeof(buf), false);
   CK(strchr(buf, '[') != NULL);
   CK(buf[strlen(buf) - 1] == ' ');

   // function-name field: fully determined by the pushed name and field size
   reset_flags();
   traced_function_stack_enabled = true;
   push_traced_function("myfunc");
   set_funcname_field_size(6);                 // exactly the name width
   CK_STR(get_msg_decoration(buf, sizeof(buf), false), "(myfunc) ");

   // wider field left-justifies the name and pads with spaces
   reset_current_traced_function_stack();
   push_traced_function("abc");
   set_funcname_field_size(10);                // name "abc" padded to width 10
   CK_STR(get_msg_decoration(buf, sizeof(buf), false), "(abc       ) ");

   // topmost function is the one shown
   reset_current_traced_function_stack();
   push_traced_function("outer");
   push_traced_function("inner");
   set_funcname_field_size(5);
   CK_STR(get_msg_decoration(buf, sizeof(buf), false), "(inner) ");

   reset_flags();

   // formatted_wall_time(): "%b %d %T" -> "Mon DD HH:MM:SS", 15 chars
   char * wt = formatted_wall_time();
   CK(strlen(wt) == 15);
   CK(wt[3] == ' ' && wt[6] == ' ' && wt[9] == ':' && wt[12] == ':');

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
