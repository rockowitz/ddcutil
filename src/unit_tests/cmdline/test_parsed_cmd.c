/** @file test_parsed_cmd.c
 *
 *  Standalone unit tests for src/cmdline/parsed_cmd.c: the Cmd_Id_Type/
 *  Setvcp_Value_Type name lookups, the new_parsed_cmd()/free_parsed_cmd()
 *  lifecycle and its documented default field values, the setvcp_values
 *  array's element clear function (freeing each entry's feature_value
 *  string), and a smoke test of dbgrpt_parsed_cmd() across a freshly
 *  allocated (mostly NULL/zero) instance, which exercises the NULL
 *  handling of every name-lookup and sub-report call it makes.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcmdline unit test: it links the internal
 *  libcmdline/libbase/libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cmdline/parsed_cmd.h"

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

/* Runs statement `stmt` with stdout (fd 1) redirected to a temporary file,
 * discarding the captured output; used only to keep report-dump smoke
 * tests quiet. */
#define QUIETLY(stmt) do { \
   fflush(stdout); \
   int _saved = dup(fileno(stdout)); \
   FILE * _tmp = tmpfile(); \
   dup2(fileno(_tmp), fileno(stdout)); \
   stmt; \
   fflush(stdout); \
   dup2(_saved, fileno(stdout)); \
   close(_saved); \
   fclose(_tmp); \
} while(0)


static void test_cmdid_name(void) {
   // cmdid_name() wraps vnt_name(), which returns the stringified enum
   // constant name (the VNT macro's #v), not the descriptive title text
   // ("detect") supplied as the table's second argument.
   CK_STR(cmdid_name(CMDID_DETECT), "CMDID_DETECT");
   CK_STR(cmdid_name(CMDID_GETVCP), "CMDID_GETVCP");
   CK_STR(cmdid_name(CMDID_SETVCP), "CMDID_SETVCP");
   CK_STR(cmdid_name(CMDID_NONE),   "CMDID_NONE");
}


static void test_setvcp_value_type_name(void) {
   CK_STR(setvcp_value_type_name(VALUE_TYPE_ABSOLUTE),      "VALUE_TYPE_ABSOLUTE");
   CK_STR(setvcp_value_type_name(VALUE_TYPE_RELATIVE_PLUS), "VALUE_TYPE_RELATIVE_PLUS");
   CK_STR(setvcp_value_type_name(VALUE_TYPE_RELATIVE_MINUS),"VALUE_TYPE_RELATIVE_MINUS");
}


static void test_new_parsed_cmd_defaults(void) {
   Parsed_Cmd * pc = new_parsed_cmd();
   CK(pc != NULL);
   if (!pc)
      return;

   CK(memcmp(pc->marker, PARSED_CMD_MARKER, 4) == 0);
   CK_INT(pc->output_level, DDCA_OL_NORMAL);
   CK_INT(pc->edid_read_size, -1);
   CK(pc->sleep_multiplier == 1.0f);
   CK(pc->min_dynamic_multiplier == -1.0f);
   CK_INT(pc->i2c_bus_check_async_min, -1);
   CK_INT(pc->ddc_check_async_min, -1);
   CK_INT(pc->i1, -1);
   CK(pc->setvcp_values != NULL);
   CK_INT(pc->setvcp_values->len, 0);
   CK(pc->cmd_id == CMDID_NONE);
   CK(pc->fref == NULL);
   CK(pc->dsel == NULL);

   free_parsed_cmd(pc);
}


static void test_setvcp_values_array_clear_func(void) {
   Parsed_Cmd * pc = new_parsed_cmd();

   Parsed_Setvcp_Args entry;
   entry.feature_code = 0x10;
   entry.feature_value_type = VALUE_TYPE_ABSOLUTE;
   entry.feature_value = g_strdup("50");
   g_array_append_val(pc->setvcp_values, entry);
   CK_INT(pc->setvcp_values->len, 1);

   Parsed_Setvcp_Args * stored = &g_array_index(pc->setvcp_values, Parsed_Setvcp_Args, 0);
   CK_INT(stored->feature_code, 0x10);
   CK_STR(stored->feature_value, "50");

   // g_array_free() below invokes destroy_parsed_setvcp_value() on the one
   // entry, freeing feature_value; must not crash or leak.
   free_parsed_cmd(pc);
   CK(true);
}


static void test_free_parsed_cmd_null_safe(void) {
   free_parsed_cmd(NULL);   // must not crash
   CK(true);
}


static void test_dbgrpt_parsed_cmd_smoke(void) {
   Parsed_Cmd * pc = new_parsed_cmd();
   QUIETLY( dbgrpt_parsed_cmd(pc, 0) );
   QUIETLY( dbgrpt_parsed_cmd(NULL, 0) );
   free_parsed_cmd(pc);
   CK(true);   // reaching here without crashing is the test
}


int main(int argc, char ** argv) {
   test_cmdid_name();
   test_setvcp_value_type_name();
   test_new_parsed_cmd_defaults();
   test_setvcp_values_array_clear_func();
   test_free_parsed_cmd_null_safe();
   test_dbgrpt_parsed_cmd_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
