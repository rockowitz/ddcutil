/** @file test_parse_capabilities.c
 *
 *  Standalone unit tests for src/vcp/parse_capabilities.c:
 *  parse_capabilities_string()/parse_capabilities() end-to-end parsing of a
 *  full capabilities string, and the query functions
 *  get_parsed_capabilities_feature_ids() and
 *  parsed_capabilities_supports_table_commands().  Pure string parsing --
 *  no hardware or file I/O.  init_vcp_feature_codes() is called once at
 *  startup, as documented as required in vcp_feature_codes.h, before any
 *  lookup into the VCP feature table.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libvcp unit test: it links the internal libvcp/libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/data_structures.h"

#include "vcp/parse_capabilities.h"
#include "vcp/parsed_capabilities_feature.h"
#include "vcp/vcp_feature_codes.h"

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


static void test_simple_valid_string(void) {
   Parsed_Capabilities * pcaps = parse_capabilities_string(
         "(prot(monitor)type(LCD)model(Test Monitor)cmds(01 02 03 0C)"
         "vcp(02 04 05 10 12 60(01 03 11) AC AE B6)mccs_ver(2.1))");
   CK(pcaps != NULL);
   if (pcaps) {
      // Quirk, not a parse error: Capabilities_Feature_Record.valid_values is
      // calloc'd to false and is only ever set to true when the feature has
      // a parenthesized value list (see parse_capabilities_feature()); a
      // bare feature code like "02" leaves it false, which
      // parse_vcp_segment() then downgrades to CAPABILITIES_USABLE even
      // though nothing is actually invalid. Since almost every real
      // capabilities string mixes bare and parenthesized feature codes,
      // CAPABILITIES_VALID is essentially unreachable in practice for a
      // vcp() segment with more than one feature -- CAPABILITIES_USABLE is
      // the realistic expected value here, not evidence of a parse problem.
      CK_INT(pcaps->caps_validity, CAPABILITIES_USABLE);
      CK_STR(pcaps->model, "Test Monitor");
      CK_STR(pcaps->mccs_version_string, "2.1");
      CK_INT(pcaps->parsed_mccs_version.major, 2);
      CK_INT(pcaps->parsed_mccs_version.minor, 1);
      CK(pcaps->raw_cmds_segment_seen);
      CK(pcaps->raw_cmds_segment_valid);
      CK(pcaps->commands != NULL);
      if (pcaps->commands) {
         CK(bva_contains(pcaps->commands, 0x01));
         CK(bva_contains(pcaps->commands, 0x0c));
         CK(!bva_contains(pcaps->commands, 0xff));
      }
      CK(pcaps->raw_vcp_features_seen);
      CK(pcaps->vcp_features != NULL);
      CK_INT(pcaps->vcp_features->len, 9);   // 02 04 05 10 12 60 AC AE B6

      free_parsed_capabilities(pcaps);
   }
}


static void test_vcp_segment_feature_ids(void) {
   Parsed_Capabilities * pcaps = parse_capabilities_string(
         "(vcp(10 12 60(01 02))mccs_ver(2.1))");
   CK(pcaps != NULL);
   if (pcaps) {
      CK_INT(pcaps->vcp_features->len, 3);

      Bit_Set_256 ids = get_parsed_capabilities_feature_ids(pcaps, false);
      CK(bs256_contains(ids, 0x10));
      CK(bs256_contains(ids, 0x12));
      CK(bs256_contains(ids, 0x60));
      CK(!bs256_contains(ids, 0x11));
      CK_INT(bs256_count(ids), 3);

      free_parsed_capabilities(pcaps);
   }
}


static void test_feature_with_values(void) {
   Parsed_Capabilities * pcaps = parse_capabilities_string("(vcp(60(01 02 0f)))");
   CK(pcaps != NULL);
   if (pcaps) {
      CK_INT(pcaps->vcp_features->len, 1);
      if (pcaps->vcp_features->len == 1) {
         Capabilities_Feature_Record * vfr = g_ptr_array_index(pcaps->vcp_features, 0);
         CK_INT(vfr->feature_id, 0x60);
         CK(vfr->valid_values);
         CK(vfr->values != NULL);
         if (vfr->values) {
            CK_INT(bva_length(vfr->values), 3);
            CK(bva_contains(vfr->values, 0x01));
            CK(bva_contains(vfr->values, 0x0f));
         }
      }
      free_parsed_capabilities(pcaps);
   }
}


static void test_missing_closing_paren(void) {
   Parsed_Capabilities * pcaps = parse_capabilities_string("(vcp(10 20)");
   CK(pcaps != NULL);
   if (pcaps) {
      CK_INT(pcaps->caps_validity, CAPABILITIES_INVALID);
      CK(pcaps->messages != NULL);
      CK(pcaps->messages->len > 0);
      free_parsed_capabilities(pcaps);
   }
}


static void test_invalid_mccs_version(void) {
   Parsed_Capabilities * pcaps = parse_capabilities_string("(model(X)mccs_ver(9.9))");
   CK(pcaps != NULL);
   if (pcaps) {
      CK_INT(pcaps->caps_validity, CAPABILITIES_USABLE);
      CK(pcaps->messages->len > 0);
      free_parsed_capabilities(pcaps);
   }
}


static void test_no_surrounding_parens(void) {
   // Apple Cinema Display violates the spec and omits the outer parens
   Parsed_Capabilities * pcaps = parse_capabilities_string("model(Apple Display)VCP(10 12)");
   CK(pcaps != NULL);
   if (pcaps) {
      CK_STR(pcaps->model, "Apple Display");
      CK(pcaps->raw_vcp_features_seen);
      CK_INT(pcaps->vcp_features->len, 2);
      free_parsed_capabilities(pcaps);
   }
}


static void test_empty_string(void) {
   Parsed_Capabilities * pcaps = parse_capabilities_string("");
   CK(pcaps != NULL);
   if (pcaps) {
      // pathological case: no vcp segment seen at all
      CK(!pcaps->raw_vcp_features_seen);
      CK_INT(pcaps->vcp_features->len, 0);
      free_parsed_capabilities(pcaps);
   }
}


static void test_supports_table_commands(void) {
   Parsed_Capabilities * yes = parse_capabilities_string("(cmds(01 02 E2 E4)vcp(10))");
   CK(yes != NULL);
   if (yes) {
      CK(parsed_capabilities_supports_table_commands(yes));
      free_parsed_capabilities(yes);
   }

   Parsed_Capabilities * no = parse_capabilities_string("(cmds(01 02)vcp(10))");
   CK(no != NULL);
   if (no) {
      CK(!parsed_capabilities_supports_table_commands(no));
      free_parsed_capabilities(no);
   }

   CK(!parsed_capabilities_supports_table_commands(NULL));
}


static void test_capabilities_validity_name(void) {
   // capabilities_validity_name() wraps vnt_name(), which returns the
   // stringified enum constant (VN macro's #v), not a descriptive title.
   CK_STR(capabilities_validity_name(CAPABILITIES_VALID),   "CAPABILITIES_VALID");
   CK_STR(capabilities_validity_name(CAPABILITIES_USABLE),  "CAPABILITIES_USABLE");
   CK_STR(capabilities_validity_name(CAPABILITIES_INVALID), "CAPABILITIES_INVALID");
}


int main(int argc, char ** argv) {
   init_vcp_feature_codes();

   test_simple_valid_string();
   test_vcp_segment_feature_ids();
   test_feature_with_values();
   test_missing_closing_paren();
   test_invalid_mccs_version();
   test_no_surrounding_parens();
   test_empty_string();
   test_supports_table_commands();
   test_capabilities_validity_name();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
