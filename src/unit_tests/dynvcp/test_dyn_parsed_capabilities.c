/** @file test_dyn_parsed_capabilities.c
 *
 *  Standalone smoke tests for src/dynvcp/dyn_parsed_capabilities.c:
 *  dyn_report_parsed_capabilities(), which is almost entirely a report
 *  function (formats a Parsed_Capabilities as human-readable text via
 *  rpt_* functions). These tests capture but do not deeply validate the
 *  output text; the goal is to exercise the code paths (no dref, dref
 *  with dynamic feature lookup, damaged capabilities string, feature x72
 *  gamma special-case formatting) without crashing.
 *
 *  Every capabilities string used here includes a valid mccs_ver()
 *  segment, so pcaps->parsed_mccs_version is never
 *  DDCA_VSPEC_UNKNOWN/UNQUERIED, and dyn_report_parsed_capabilities()
 *  never falls back to get_vcp_version_by_dh()/get_vcp_version_by_dref()
 *  (both of which would otherwise attempt real DDC communication).
 *
 *  main() sandboxes $XDG_DATA_HOME/$XDG_DATA_DIRS so the dynamic feature
 *  file lookup triggered by a non-NULL dref reliably misses regardless of
 *  the real environment.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: dynvcp source files cross-reference vcp/base/ddc
 *  extensively, so it links the top-level libcommon convenience library
 *  (the same aggregate the ddcutil executable itself links).
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/edid.h"

#include "base/displays.h"

#include "vcp/parse_capabilities.h"
#include "vcp/vcp_feature_codes.h"

#include "dynvcp/dyn_parsed_capabilities.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

/* Runs statement `stmt` with stdout (fd 1) redirected to a temporary file,
 * discarding the captured output. */
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


static Parsed_Edid * make_valid_edid(Byte fill_byte) {
   Byte raw[128];
   memset(raw, fill_byte, 128);
   raw[0] = 0x00; raw[1] = raw[2] = raw[3] = raw[4] = raw[5] = raw[6] = 0xff; raw[7] = 0x00;
   int sum = 0;
   for (int i = 0; i < 127; i++)
      sum += raw[i];
   raw[127] = (Byte)((256 - (sum % 256)) % 256);
   return create_parsed_edid2(raw, "TEST");
}


static void test_no_dref_no_dh(void) {
   Parsed_Capabilities * pcaps = parse_capabilities_string(
         "(prot(monitor)type(LCD)model(Test)cmds(01 02 03)vcp(02 10 12 60(01 02))mccs_ver(2.1))");
   CK(pcaps != NULL);

   QUIETLY( dyn_report_parsed_capabilities(pcaps, NULL, NULL, 0) );
   CK(true);   // reaching here without crashing is the test

   free_parsed_capabilities(pcaps);
}


static void test_with_dref(void) {
   Parsed_Capabilities * pcaps = parse_capabilities_string(
         "(prot(monitor)type(LCD)model(Test)cmds(01 02 03)vcp(10 12)mccs_ver(2.2))");
   CK(pcaps != NULL);

   Display_Ref * dref = create_bus_display_ref(231);
   dref->pedid = make_valid_edid(0x44);

   QUIETLY( dyn_report_parsed_capabilities(pcaps, NULL, dref, 0) );
   CK(true);
   // dref->dfr gets populated as a side effect (dummy record, file not found)
   CK(dref->dfr != NULL);
   CK(dref->flags & DREF_DYNAMIC_FEATURES_CHECKED);

   free_parsed_capabilities(pcaps);
}


static void test_damaged_capabilities(void) {
   // missing closing parenthesis -> caps_validity == CAPABILITIES_INVALID,
   // exercises the "not completely parsed" reporting branch
   Parsed_Capabilities * pcaps = parse_capabilities_string("(vcp(10 20)");
   CK(pcaps != NULL);

   QUIETLY( dyn_report_parsed_capabilities(pcaps, NULL, NULL, 0) );
   CK(true);

   free_parsed_capabilities(pcaps);
}


static void test_gamma_feature_x72(void) {
   // full-range absolute gamma descriptor: ideal tolerance (00), native
   // gamma byte 0x78, 0xff = full range / no bypass
   Parsed_Capabilities * pcaps = parse_capabilities_string(
         "(vcp(72(00 78 FF))mccs_ver(2.1))");
   CK(pcaps != NULL);

   QUIETLY( dyn_report_parsed_capabilities(pcaps, NULL, NULL, 0) );
   CK(true);   // reaching here without crashing is the test

   free_parsed_capabilities(pcaps);
}


int main(int argc, char ** argv) {
   char tmpl[] = "/tmp/ddcutil_test_xdg_data_XXXXXX";
   char * tmpdir = mkdtemp(tmpl);
   if (!tmpdir) {
      fprintf(stderr, "mkdtemp() failed: %s\n", strerror(errno));
      return 2;
   }
   setenv("XDG_DATA_HOME", tmpdir, 1);
   setenv("XDG_DATA_DIRS", tmpdir, 1);

   init_vcp_feature_codes();

   test_no_dref_no_dh();
   test_with_dref();
   test_damaged_capabilities();
   test_gamma_feature_x72();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
