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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/coredefs_base.h"

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


/** Every value in Parsed_Cmd_Flags and Parsed_Cmd_Flags2 is a distinct single
 *  bit.
 *
 *  Both enums are hand assigned bit masks, so a flag added with a value already
 *  in use, or with two bits set, aliases an existing flag and the aliasing is
 *  silent -- the compiler has no reason to object and every test that sets one
 *  flag and reads the other still passes.  Removing a flag frees its bit, which
 *  is exactly the value a later addition is likely to reach for:
 *  CMD_FLAG_SINGLE_IOCTL_EDID_READ was removed and freed 0x80000000.
 *
 *  Members parked under #ifdef are deliberately not listed.  Several of them do
 *  collide with live flags, which is harmless while they are not compiled.
 */
static void test_cmd_flag_bits_distinct(void) {
   static const struct { const char * name; uint64_t value; } flags[] = {
      { "CMD_FLAG_DDCDATA", CMD_FLAG_DDCDATA },
      { "CMD_FLAG_FORCE_UNRECOGNIZED_VCP_CODE", CMD_FLAG_FORCE_UNRECOGNIZED_VCP_CODE },
      { "CMD_FLAG_FORCE_SLAVE_ADDR", CMD_FLAG_FORCE_SLAVE_ADDR },
      { "CMD_FLAG_TIMESTAMP_TRACE", CMD_FLAG_TIMESTAMP_TRACE },
      { "CMD_FLAG_SHOW_UNSUPPORTED", CMD_FLAG_SHOW_UNSUPPORTED },
      { "CMD_FLAG_ENABLE_FAILSIM", CMD_FLAG_ENABLE_FAILSIM },
      { "CMD_FLAG_VERIFY", CMD_FLAG_VERIFY },
      { "CMD_FLAG_SKIP_DDC_CHECKS", CMD_FLAG_SKIP_DDC_CHECKS },
      { "CMD_FLAG_UNUSED1", CMD_FLAG_UNUSED1 },
      { "CMD_FLAG_REPORT_FREED_EXCP", CMD_FLAG_REPORT_FREED_EXCP },
      { "CMD_FLAG_NOTABLE", CMD_FLAG_NOTABLE },
      { "CMD_FLAG_THREAD_ID_TRACE", CMD_FLAG_THREAD_ID_TRACE },
      { "CMD_FLAG_NULL_MSG_INDICATES_UNSUPPORTED_FEATURE", CMD_FLAG_NULL_MSG_INDICATES_UNSUPPORTED_FEATURE },
      { "CMD_FLAG_HEURISTIC_UNSUPPORTED_FEATURES", CMD_FLAG_HEURISTIC_UNSUPPORTED_FEATURES },
      { "CMD_FLAG_DISCARD_CACHES", CMD_FLAG_DISCARD_CACHES },
      { "CMD_FLAG_PROCESS_ID_TRACE", CMD_FLAG_PROCESS_ID_TRACE },
      { "CMD_FLAG_RW_ONLY", CMD_FLAG_RW_ONLY },
      { "CMD_FLAG_RO_ONLY", CMD_FLAG_RO_ONLY },
      { "CMD_FLAG_WO_ONLY", CMD_FLAG_WO_ONLY },
      { "CMD_FLAG_ASYNC_I2C_CHECK", CMD_FLAG_ASYNC_I2C_CHECK },
      { "CMD_FLAG_ENABLE_UDF", CMD_FLAG_ENABLE_UDF },
      { "CMD_FLAG_ENABLE_USB", CMD_FLAG_ENABLE_USB },
      { "CMD_FLAG_EDP_ALWAYS_LAPTOP", CMD_FLAG_EDP_ALWAYS_LAPTOP },
      { "CMD_FLAG_TRY_GET_EDID_FROM_SYSFS", CMD_FLAG_TRY_GET_EDID_FROM_SYSFS },
      { "CMD_FLAG_FLOCK", CMD_FLAG_FLOCK },
      { "CMD_FLAG_DEFER_SLEEPS", CMD_FLAG_DEFER_SLEEPS },
      { "CMD_FLAG_X52_NO_FIFO", CMD_FLAG_X52_NO_FIFO },
      { "CMD_FLAG_VERBOSE_STATS", CMD_FLAG_VERBOSE_STATS },
      { "CMD_FLAG_SHOW_SETTINGS", CMD_FLAG_SHOW_SETTINGS },
      { "CMD_FLAG_ENABLE_CACHED_CAPABILITIES", CMD_FLAG_ENABLE_CACHED_CAPABILITIES },
      { "CMD_FLAG_WALLTIME_TRACE", CMD_FLAG_WALLTIME_TRACE },
      { "CMD_FLAG_I2C_IO_FILEIO", CMD_FLAG_I2C_IO_FILEIO },
      { "CMD_FLAG_I2C_IO_IOCTL", CMD_FLAG_I2C_IO_IOCTL },
      { "CMD_FLAG_EXPLICIT_SLEEP_MULTIPLIER", CMD_FLAG_EXPLICIT_SLEEP_MULTIPLIER },
      { "CMD_FLAG_DSA2", CMD_FLAG_DSA2 },
      { "CMD_FLAG_QUICK", CMD_FLAG_QUICK },
      { "CMD_FLAG_MOCK", CMD_FLAG_MOCK },
      { "CMD_FLAG_PROFILE_API", CMD_FLAG_PROFILE_API },
      { "CMD_FLAG_ENABLE_CACHED_DISPLAYS", CMD_FLAG_ENABLE_CACHED_DISPLAYS },
      { "CMD_FLAG_TRACE_TO_SYSLOG_ONLY", CMD_FLAG_TRACE_TO_SYSLOG_ONLY },
      { "CMD_FLAG_TRACE_TO_SYSLOG", CMD_FLAG_TRACE_TO_SYSLOG },
      { "CMD_FLAG_STATS_TO_SYSLOG", CMD_FLAG_STATS_TO_SYSLOG },
      { "CMD_FLAG_INTERNAL_STATS", CMD_FLAG_INTERNAL_STATS },
      { "CMD_FLAG_EXPLICIT_I2C_SOURCE_ADDR", CMD_FLAG_EXPLICIT_I2C_SOURCE_ADDR },
      { "CMD_FLAG_ENABLE_EARLY_PERMISSION_CHECKS", CMD_FLAG_ENABLE_EARLY_PERMISSION_CHECKS },
      { "CMD_FLAG_ENABLE_TRACED_FUNCTION_STACK", CMD_FLAG_ENABLE_TRACED_FUNCTION_STACK },
      { "CMD_FLAG_TRACED_FUNCTION_STACK_ERRORS_FATAL", CMD_FLAG_TRACED_FUNCTION_STACK_ERRORS_FATAL },
      { "CMD_FLAG_DISABLE_API", CMD_FLAG_DISABLE_API },
      { "CMD_FLAG_WATCH_DISPLAY_EVENTS", CMD_FLAG_WATCH_DISPLAY_EVENTS },
   };

   static const struct { const char * name; uint64_t value; } flags2[] = {
      { "CMD_FLAG2_F1", CMD_FLAG2_F1 },
      { "CMD_FLAG2_F2", CMD_FLAG2_F2 },
      { "CMD_FLAG2_F3", CMD_FLAG2_F3 },
      { "CMD_FLAG2_F4", CMD_FLAG2_F4 },
      { "CMD_FLAG2_F5", CMD_FLAG2_F5 },
      { "CMD_FLAG2_F6", CMD_FLAG2_F6 },
      { "CMD_FLAG2_F7", CMD_FLAG2_F7 },
      { "CMD_FLAG2_F8", CMD_FLAG2_F8 },
      { "CMD_FLAG2_F9", CMD_FLAG2_F9 },
      { "CMD_FLAG2_F10", CMD_FLAG2_F10 },
      { "CMD_FLAG2_F11", CMD_FLAG2_F11 },
      { "CMD_FLAG2_F12", CMD_FLAG2_F12 },
      { "CMD_FLAG2_F13", CMD_FLAG2_F13 },
      { "CMD_FLAG2_F14", CMD_FLAG2_F14 },
      { "CMD_FLAG2_F15", CMD_FLAG2_F15 },
      { "CMD_FLAG2_F16", CMD_FLAG2_F16 },
      { "CMD_FLAG2_F17", CMD_FLAG2_F17 },
      { "CMD_FLAG2_F18", CMD_FLAG2_F18 },
      { "CMD_FLAG2_F19", CMD_FLAG2_F19 },
      { "CMD_FLAG2_F20", CMD_FLAG2_F20 },
      { "CMD_FLAG2_F21", CMD_FLAG2_F21 },
      { "CMD_FLAG2_F22", CMD_FLAG2_F22 },
      { "CMD_FLAG2_F23", CMD_FLAG2_F23 },
      { "CMD_FLAG2_F24", CMD_FLAG2_F24 },
      { "CMD_FLAG2_F25", CMD_FLAG2_F25 },
      { "CMD_FLAG2_F26", CMD_FLAG2_F26 },
      { "CMD_FLAG2_F27", CMD_FLAG2_F27 },
      { "CMD_FLAG2_F28", CMD_FLAG2_F28 },
      { "CMD_FLAG2_F29", CMD_FLAG2_F29 },
      { "CMD_FLAG2_F30", CMD_FLAG2_F30 },
      { "CMD_FLAG2_F31", CMD_FLAG2_F31 },
      { "CMD_FLAG2_F32", CMD_FLAG2_F32 },
      { "CMD_FLAG2_F33", CMD_FLAG2_F33 },
      { "CMD_FLAG2_F34", CMD_FLAG2_F34 },
      { "CMD_FLAG2_F35", CMD_FLAG2_F35 },
      { "CMD_FLAG2_F36", CMD_FLAG2_F36 },
      { "CMD_FLAG2_F37", CMD_FLAG2_F37 },
      { "CMD_FLAG2_F38", CMD_FLAG2_F38 },
      { "CMD_FLAG2_F39", CMD_FLAG2_F39 },
      { "CMD_FLAG2_F40", CMD_FLAG2_F40 },
      { "CMD_FLAG2_I1_SET", CMD_FLAG2_I1_SET },
      { "CMD_FLAG2_I2_SET", CMD_FLAG2_I2_SET },
      { "CMD_FLAG2_I3_SET", CMD_FLAG2_I3_SET },
      { "CMD_FLAG2_I4_SET", CMD_FLAG2_I4_SET },
      { "CMD_FLAG2_I5_SET", CMD_FLAG2_I5_SET },
      { "CMD_FLAG2_I6_SET", CMD_FLAG2_I6_SET },
      { "CMD_FLAG2_I7_SET", CMD_FLAG2_I7_SET },
      { "CMD_FLAG2_I8_SET", CMD_FLAG2_I8_SET },
      { "CMD_FLAG2_I9_SET", CMD_FLAG2_I9_SET },
      { "CMD_FLAG2_I10_SET", CMD_FLAG2_I10_SET },
      { "CMD_FLAG2_I11_SET", CMD_FLAG2_I11_SET },
      { "CMD_FLAG2_I12_SET", CMD_FLAG2_I12_SET },
      { "CMD_FLAG2_I13_SET", CMD_FLAG2_I13_SET },
      { "CMD_FLAG2_I14_SET", CMD_FLAG2_I14_SET },
      { "CMD_FLAG2_I15_SET", CMD_FLAG2_I15_SET },
      { "CMD_FLAG2_I16_SET", CMD_FLAG2_I16_SET },
      { "CMD_FLAG2_I17_SET", CMD_FLAG2_I17_SET },
      { "CMD_FLAG2_FL1_SET", CMD_FLAG2_FL1_SET },
      { "CMD_FLAG2_FL2_SET", CMD_FLAG2_FL2_SET },
   };

   for (unsigned i = 0; i < ARRAY_SIZE(flags); i++) {
      CK(flags[i].value != 0);
      CK((flags[i].value & (flags[i].value - 1)) == 0);     // exactly one bit
      for (unsigned j = i+1; j < ARRAY_SIZE(flags); j++) {
         total++;
         if (flags[i].value == flags[j].value) {
            failed++;
            printf("FAIL  line %-4d  %s and %s share value 0x%lx\n", __LINE__,
                   flags[i].name, flags[j].name, (unsigned long) flags[i].value);
         }
      }
   }

   for (unsigned i = 0; i < ARRAY_SIZE(flags2); i++) {
      CK(flags2[i].value != 0);
      CK((flags2[i].value & (flags2[i].value - 1)) == 0);
      for (unsigned j = i+1; j < ARRAY_SIZE(flags2); j++) {
         total++;
         if (flags2[i].value == flags2[j].value) {
            failed++;
            printf("FAIL  line %-4d  %s and %s share value 0x%lx\n", __LINE__,
                   flags2[i].name, flags2[j].name, (unsigned long) flags2[i].value);
         }
      }
   }

   // the bit freed by the removal of CMD_FLAG_SINGLE_IOCTL_EDID_READ is unclaimed
   uint64_t union_flags = 0;
   for (unsigned i = 0; i < ARRAY_SIZE(flags); i++)
      union_flags |= flags[i].value;
   CK((union_flags & 0x80000000) == 0);
}


int main(int argc, char ** argv) {
   setvbuf(stdout, NULL, _IONBF, 0);   // so output survives a crash

   test_cmdid_name();
   test_cmd_flag_bits_distinct();
   test_setvcp_value_type_name();
   test_new_parsed_cmd_defaults();
   test_setvcp_values_array_clear_func();
   test_free_parsed_cmd_null_safe();
   test_dbgrpt_parsed_cmd_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
