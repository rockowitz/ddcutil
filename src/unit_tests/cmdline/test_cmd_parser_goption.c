/** @file test_cmd_parser_goption.c
 *
 *  Standalone unit tests for src/cmdline/cmd_parser_goption.c:
 *  parse_syslog_level(), and end-to-end tests of parse_command() driving
 *  it with representative argv arrays.  parse_command() is pure argv
 *  parsing -- it does not read the ddcutil config file or touch any
 *  hardware -- so it is fully testable without mocking.  As with any real
 *  argv[], element 0 must be a program name; GOption's parser expects and
 *  skips it.
 *
 *  This directory has roughly 150 command-line options; this file does
 *  not attempt to exercise all of them.  It covers: command name
 *  recognition and abbreviation, the argument-count and error-reporting
 *  paths (unrecognized command, no command, missing/extra arguments),
 *  getvcp's feature-code-or-subset argument, setvcp's feature/value
 *  (including the "+"/"-" relative-value forms), the --display/--bus
 *  display-selection options, and a small sample of plain boolean options
 *  (--verify/--noverify, mutual exclusivity) and a callback-based option
 *  (--stats).
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

#include "cmdline/cmd_parser.h"

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


// argv must be NULL-terminated with element 0 a (dummy) program name, e.g.
// { "ddcutil", "detect", NULL }.  Frees the returned Parsed_Cmd and error
// array itself; returns the Parsed_Cmd only when the caller needs to
// inspect its fields further (in which case the caller must free it).
static Parsed_Cmd * parse(char ** argv, int argc, GPtrArray ** errmsgs_out) {
   GPtrArray * errmsgs = g_ptr_array_new_with_free_func(g_free);
   Parsed_Cmd * pc = parse_command(argc, argv, MODE_DDCUTIL, errmsgs);
   if (errmsgs_out)
      *errmsgs_out = errmsgs;
   else
      g_ptr_array_free(errmsgs, true);
   return pc;
}

static void test_command_recognition(void) {
   char * argv1[] = {"ddcutil", "detect", NULL};
   Parsed_Cmd * pc = parse(argv1, 2, NULL);
   CK(pc != NULL);
   if (pc) { CK(pc->cmd_id == CMDID_DETECT); free_parsed_cmd(pc); }

   char * argv2[] = {"ddcutil", "cap", NULL};   // abbreviation, minchars=3
   pc = parse(argv2, 2, NULL);
   CK(pc != NULL);
   if (pc) { CK(pc->cmd_id == CMDID_CAPABILITIES); free_parsed_cmd(pc); }

   char * argv3[] = {"ddcutil", "getvcp", "10", NULL};
   pc = parse(argv3, 3, NULL);
   CK(pc != NULL);
   if (pc) { CK(pc->cmd_id == CMDID_GETVCP); free_parsed_cmd(pc); }
}


static void test_unrecognized_command(void) {
   char * argv[] = {"ddcutil", "definitely-not-a-command", NULL};
   GPtrArray * errmsgs;
   Parsed_Cmd * pc = parse(argv, 2, &errmsgs);
   CK(pc == NULL);
   CK(errmsgs->len > 0);
   g_ptr_array_free(errmsgs, true);
}


static void test_no_command(void) {
   char * argv[] = {"ddcutil", NULL};
   GPtrArray * errmsgs;
   Parsed_Cmd * pc = parse(argv, 1, &errmsgs);
   CK(pc == NULL);
   CK(errmsgs->len > 0);
   g_ptr_array_free(errmsgs, true);
}


static void test_missing_required_argument(void) {
   // getvcp requires at least 1 argument
   char * argv[] = {"ddcutil", "getvcp", NULL};
   GPtrArray * errmsgs;
   Parsed_Cmd * pc = parse(argv, 2, &errmsgs);
   CK(pc == NULL);
   CK(errmsgs->len > 0);
   g_ptr_array_free(errmsgs, true);
}


static void test_too_many_arguments(void) {
   // detect accepts 0 arguments
   char * argv[] = {"ddcutil", "detect", "unexpected-arg", NULL};
   GPtrArray * errmsgs;
   Parsed_Cmd * pc = parse(argv, 3, &errmsgs);
   CK(pc == NULL);
   CK(errmsgs->len > 0);
   g_ptr_array_free(errmsgs, true);
}


static void test_getvcp_feature_arg(void) {
   char * argv[] = {"ddcutil", "getvcp", "color", NULL};
   Parsed_Cmd * pc = parse(argv, 3, NULL);
   CK(pc != NULL);
   if (pc) {
      CK(pc->fref != NULL);
      if (pc->fref)
         CK(pc->fref->subset == VCP_SUBSET_COLOR);
      free_parsed_cmd(pc);
   }

   // invalid feature code/subset -> whole parse fails
   char * bad_argv[] = {"ddcutil", "getvcp", "not-a-feature", NULL};
   GPtrArray * errmsgs;
   pc = parse(bad_argv, 3, &errmsgs);
   CK(pc == NULL);
   CK(errmsgs->len > 0);
   g_ptr_array_free(errmsgs, true);
}


static void test_setvcp_args(void) {
   char * argv[] = {"ddcutil", "setvcp", "10", "50", NULL};
   Parsed_Cmd * pc = parse(argv, 4, NULL);
   CK(pc != NULL);
   if (pc) {
      CK_INT(pc->setvcp_values->len, 1);
      if (pc->setvcp_values->len == 1) {
         Parsed_Setvcp_Args * a = &g_array_index(pc->setvcp_values, Parsed_Setvcp_Args, 0);
         CK_INT(a->feature_code, 0x10);
         CK(a->feature_value_type == VALUE_TYPE_ABSOLUTE);
         CK_STR(a->feature_value, "50");
      }
      free_parsed_cmd(pc);
   }

   // relative value form: setvcp <code> + <value>
   char * argv_plus[] = {"ddcutil", "setvcp", "10", "+", "5", NULL};
   pc = parse(argv_plus, 5, NULL);
   CK(pc != NULL);
   if (pc) {
      CK_INT(pc->setvcp_values->len, 1);
      if (pc->setvcp_values->len == 1) {
         Parsed_Setvcp_Args * a = &g_array_index(pc->setvcp_values, Parsed_Setvcp_Args, 0);
         CK(a->feature_value_type == VALUE_TYPE_RELATIVE_PLUS);
         CK_STR(a->feature_value, "5");
      }
      free_parsed_cmd(pc);
   }

   // missing value after feature code
   char * bad_argv[] = {"ddcutil", "setvcp", "10", NULL};
   GPtrArray * errmsgs;
   pc = parse(bad_argv, 3, &errmsgs);
   CK(pc == NULL);
   CK(errmsgs->len > 0);
   g_ptr_array_free(errmsgs, true);
}


static void test_display_selection(void) {
   // detect has Option_None: --display is not supported for it, and
   // parse_command() rejects the combination.  Use capabilities, which
   // has Option_Explicit_Display.
   char * argv[] = {"ddcutil", "--display", "2", "capabilities", NULL};
   Parsed_Cmd * pc = parse(argv, 4, NULL);
   CK(pc != NULL);
   if (pc) {
      CK(pc->dsel != NULL);
      if (pc->dsel)
         CK_INT(pc->dsel->dispno, 2);
      free_parsed_cmd(pc);
   }

   // confirm the converse: --display rejected for a command that does not
   // support it
   char * argv2[] = {"ddcutil", "--display", "1", "detect", NULL};
   GPtrArray * errmsgs;
   pc = parse(argv2, 4, &errmsgs);
   CK(pc == NULL);
   CK(errmsgs->len > 0);
   g_ptr_array_free(errmsgs, true);
}


static void test_verify_noverify_flags(void) {
   char * argv[] = {"ddcutil", "--verify", "detect", NULL};
   Parsed_Cmd * pc = parse(argv, 3, NULL);
   CK(pc != NULL);
   if (pc) { CK(pc->flags & CMD_FLAG_VERIFY); free_parsed_cmd(pc); }

   char * argv2[] = {"ddcutil", "--noverify", "detect", NULL};
   pc = parse(argv2, 3, NULL);
   CK(pc != NULL);
   if (pc) { CK(!(pc->flags & CMD_FLAG_VERIFY)); free_parsed_cmd(pc); }

   // mutually exclusive
   char * argv3[] = {"ddcutil", "--verify", "--noverify", "detect", NULL};
   GPtrArray * errmsgs;
   pc = parse(argv3, 4, &errmsgs);
   CK(pc == NULL);
   CK(errmsgs->len > 0);
   g_ptr_array_free(errmsgs, true);
}


static void test_stats_option(void) {
   char * argv[] = {"ddcutil", "--stats", "calls", "detect", NULL};
   Parsed_Cmd * pc = parse(argv, 4, NULL);
   CK(pc != NULL);
   if (pc) {
      CK(pc->stats_types & DDCA_STATS_CALLS);
      free_parsed_cmd(pc);
   }
}


static void test_parse_syslog_level(void) {
   DDCA_Syslog_Level level;
   GPtrArray * errmsgs = g_ptr_array_new_with_free_func(g_free);

   CK(parse_syslog_level("INFO", &level, errmsgs));
   CK(level == DDCA_SYSLOG_INFO);
   CK_INT(errmsgs->len, 0);

   CK(parse_syslog_level("DEBUG", &level, errmsgs));
   CK(level == DDCA_SYSLOG_DEBUG);

   bool ok = parse_syslog_level("NOT-A-LEVEL", &level, errmsgs);
   CK(!ok);
   CK(errmsgs->len > 0);   // at least one error message emitted

   g_ptr_array_free(errmsgs, true);
}


int main(int argc, char ** argv) {
   test_command_recognition();
   test_unrecognized_command();
   test_no_command();
   test_missing_required_argument();
   test_too_many_arguments();
   test_getvcp_feature_arg();
   test_setvcp_args();
   test_display_selection();
   test_verify_noverify_flags();
   test_stats_option();
   test_parse_syslog_level();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
