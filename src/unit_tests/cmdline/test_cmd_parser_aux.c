/** @file test_cmd_parser_aux.c
 *
 *  Standalone unit tests for src/cmdline/cmd_parser_aux.c: the command
 *  table lookups (find_command()/get_command(), including abbreviation
 *  matching), the small argument-syntax parsers (all_digits(),
 *  parse_dot_separated_arg(), parse_colon_separated_arg(),
 *  parse_colon_separated_vid_pid(), parse_int_arg()), the feature subset
 *  table lookup (find_subset()) and its wrapper
 *  parse_feature_ids_or_subset(), validate_output_level(), and
 *  init_cmd_parser_base()'s internal self-consistency assertions (which
 *  would abort if the command/subset tables and their corresponding enums
 *  ever drifted out of sync).
 *
 *  Not exercised: show_cmd_desc() and assemble_command_argument_help()
 *  are pure text formatters with no return value/logic to assert on
 *  beyond "does not crash", which is implicitly covered by every other
 *  test calling into this module.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcmdline unit test: it links the internal
 *  libcmdline/libbase/libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmdline/cmd_parser_aux.h"

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


static void test_find_command(void) {
   Cmd_Desc * d = find_command("detect");
   CK(d != NULL);
   if (d) CK_INT(d->cmd_id, CMDID_DETECT);

   d = find_command("det");   // minchars = 3
   CK(d != NULL);
   if (d) CK_INT(d->cmd_id, CMDID_DETECT);

   d = find_command("de");    // below minchars
   CK(d == NULL);

   d = find_command("getvcp");
   CK(d != NULL);
   if (d) CK_INT(d->cmd_id, CMDID_GETVCP);

   d = find_command("definitely-not-a-command");
   CK(d == NULL);
}


static void test_get_command(void) {
   Cmd_Desc * d = get_command(CMDID_SETVCP);
   CK(d != NULL);
   if (d) CK(strcmp(d->cmd_name, "setvcp") == 0);

   d = get_command(0x7fffffff);   // no such cmd_id
   CK(d == NULL);
}


static void test_all_digits(void) {
   CK(all_digits("12345", 5));
   CK(all_digits("007", 3));
   CK(!all_digits("12a45", 5));
   CK(!all_digits("-123", 4));
}


static void test_parse_dot_separated_arg(void) {
   int v1, v2;
   CK(parse_dot_separated_arg("1.2", &v1, &v2));
   CK_INT(v1, 1);
   CK_INT(v2, 2);

   CK(!parse_dot_separated_arg("1", &v1, &v2));
   CK(!parse_dot_separated_arg("abc", &v1, &v2));
}


static void test_parse_colon_separated_arg(void) {
   int v1, v2;
   CK(parse_colon_separated_arg("3:4", &v1, &v2));
   CK_INT(v1, 3);
   CK_INT(v2, 4);

   CK(!parse_colon_separated_arg("3", &v1, &v2));
}


static void test_parse_colon_separated_vid_pid(void) {
   uint16_t v1, v2;
   CK(parse_colon_separated_vid_pid("04e8:1234", &v1, &v2));
   CK_INT(v1, 0x04e8);
   CK_INT(v2, 0x1234);

   CK(!parse_colon_separated_vid_pid("4e8:1234", &v1, &v2));    // not 4 hex digits
   CK(!parse_colon_separated_vid_pid("04e8", &v1, &v2));        // missing second part
   CK(!parse_colon_separated_vid_pid("04e8:1234:5678", &v1, &v2)); // too many parts
}


static void test_parse_int_arg(void) {
   int v;
   CK(parse_int_arg("42", &v));
   CK_INT(v, 42);

   CK(parse_int_arg("-7", &v));
   CK_INT(v, -7);

   CK(!parse_int_arg("abc", &v));
}


static void test_find_subset(void) {
   CK_INT(find_subset("COLOR", CMDID_GETVCP), VCP_SUBSET_COLOR);
   CK_INT(find_subset("col",   CMDID_GETVCP), VCP_SUBSET_COLOR);   // abbreviation, min_chars=3
   CK_INT(find_subset("ALL",  CMDID_GETVCP), VCP_SUBSET_KNOWN);
   CK_INT(find_subset("definitely-not-a-subset", CMDID_GETVCP), VCP_SUBSET_NONE);

   // PRESET is valid only for CMDID_VCPINFO, not CMDID_GETVCP
   CK_INT(find_subset("PRESET", CMDID_VCPINFO), VCP_SUBSET_PRESET);
   CK_INT(find_subset("PRESET", CMDID_GETVCP),  VCP_SUBSET_NONE);
}


static void test_parse_feature_ids_or_subset(void) {
   // single value: a recognized subset name
   char * subset_args[] = {"color"};
   Feature_Set_Ref * fsref = parse_feature_ids_or_subset(CMDID_GETVCP, subset_args, 1);
   CK(fsref != NULL);
   if (fsref) {
      CK_INT(fsref->subset, VCP_SUBSET_COLOR);
      free(fsref);
   }

   // single value: a hex feature code
   char * hex_args[] = {"10"};
   fsref = parse_feature_ids_or_subset(CMDID_GETVCP, hex_args, 1);
   CK(fsref != NULL);
   if (fsref) {
      CK_INT(fsref->subset, VCP_SUBSET_SINGLE_FEATURE);
      free(fsref);
   }

   // no values: defaults to "ALL"
   fsref = parse_feature_ids_or_subset(CMDID_GETVCP, NULL, 0);
   CK(fsref != NULL);
   if (fsref) {
      CK_INT(fsref->subset, VCP_SUBSET_KNOWN);
      free(fsref);
   }

   // multiple values: all must be valid hex feature codes
   char * multi_args[] = {"10", "12", "0x14"};
   fsref = parse_feature_ids_or_subset(CMDID_GETVCP, multi_args, 3);
   CK(fsref != NULL);
   if (fsref) {
      CK_INT(fsref->subset, VCP_SUBSET_MULTI_FEATURES);
      free(fsref);
   }

   // multiple values: one invalid -> whole parse fails
   char * bad_multi_args[] = {"10", "not-hex"};
   fsref = parse_feature_ids_or_subset(CMDID_GETVCP, bad_multi_args, 2);
   CK(fsref == NULL);

   // single value: neither a subset name nor a valid hex code
   char * bad_args[] = {"not-a-thing"};
   fsref = parse_feature_ids_or_subset(CMDID_GETVCP, bad_args, 1);
   CK(fsref == NULL);
}


static void test_validate_output_level(void) {
   Parsed_Cmd * pc = new_parsed_cmd();

   pc->cmd_id = CMDID_DETECT;
   pc->output_level = DDCA_OL_NORMAL;
   CK(validate_output_level(pc));

   pc->output_level = DDCA_OL_TERSE;
   CK(validate_output_level(pc));

   // CMDID_PROBE does not accept DDCA_OL_TERSE
   pc->cmd_id = CMDID_PROBE;
   pc->output_level = DDCA_OL_TERSE;
   CK(!validate_output_level(pc));

   pc->output_level = DDCA_OL_NORMAL;
   CK(validate_output_level(pc));

   free_parsed_cmd(pc);
}


static void test_init_cmd_parser_base_smoke(void) {
   // init_cmd_parser_base() is only defined when NDEBUG is not set (it
   // asserts internally that the command/subset tables and their enums
   // have not drifted out of sync); guard the call the same way
   // cmd_parser_goption.c does at its one call site.
#ifndef NDEBUG
   init_cmd_parser_base();
#endif
   CK(true);
}


int main(int argc, char ** argv) {
   test_find_command();
   test_get_command();
   test_all_digits();
   test_parse_dot_separated_arg();
   test_parse_colon_separated_arg();
   test_parse_colon_separated_vid_pid();
   test_parse_int_arg();
   test_find_subset();
   test_parse_feature_ids_or_subset();
   test_validate_output_level();
   test_init_cmd_parser_base_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
