/** @file test_app_vcpinfo.c
 *
 *  Standalone unit tests for src/app_ddcutil/app_vcpinfo.c: app_listvcp()
 *  (writes the static VCP feature code table to a FILE*),
 *  report_vcp_feature_table_entry() (report function driven by a static
 *  VCP_Feature_Table_Entry, no hardware), and app_vcpinfo() (the VCPINFO
 *  command mainline, which operates purely on the static feature table and
 *  a caller-supplied Feature_Set_Ref -- no display handle is involved).
 *
 *  All other functions in this file are static helpers exercised
 *  indirectly through the above.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libappddcutil/
 *  libcommon convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "base/core.h"
#include "base/feature_set_ref.h"
#include "util/data_structures.h"
#include "vcp/vcp_feature_codes.h"

#include "cmdline/parsed_cmd.h"

#include "app_ddcutil/app_vcpinfo.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

/* Runs statement `stmt` with stdout (fd 1) redirected to a temporary file
 * whose contents are then read into caller-supplied buffer `outbuf`
 * (size outbufsz), NUL terminated. */
#define CAPTURE(stmt, outbuf, outbufsz) do { \
   fflush(stdout); \
   int _saved = dup(fileno(stdout)); \
   FILE * _tmp = tmpfile(); \
   dup2(fileno(_tmp), fileno(stdout)); \
   stmt; \
   fflush(stdout); \
   dup2(_saved, fileno(stdout)); \
   close(_saved); \
   rewind(_tmp); \
   size_t _n = fread((outbuf), 1, (outbufsz)-1, _tmp); \
   (outbuf)[_n] = '\0'; \
   fclose(_tmp); \
} while(0)

static void test_app_listvcp(void) {
   char buf[8192];
   FILE * tmp = tmpfile();
   app_listvcp(tmp);
   rewind(tmp);
   size_t n = fread(buf, 1, sizeof(buf)-1, tmp);
   buf[n] = '\0';
   fclose(tmp);

   CK(strstr(buf, "Recognized VCP feature codes:") != NULL);
   CK(strstr(buf, "10 -") != NULL);   // Brightness
}


static void test_report_vcp_feature_table_entry_smoke(void) {
   VCP_Feature_Table_Entry * entry = vcp_find_feature_by_hexid(0x10);
   CK(entry != NULL);
   if (entry) {
      char buf[4000];
      CAPTURE( report_vcp_feature_table_entry(entry, 0), buf, sizeof(buf) );
      CK(strstr(buf, "VCP code 10") != NULL);
   }
}


static void test_app_vcpinfo_single_feature(void) {
   DDCA_Output_Level saved_ol = get_output_level();
   set_output_level(DDCA_OL_NORMAL);

   Feature_Set_Ref fsref;
   fsref.subset = VCP_SUBSET_SINGLE_FEATURE;
   fsref.features = EMPTY_BIT_SET_256;
   fsref.features = bs256_insert(fsref.features, 0x10);

   Parsed_Cmd parsed_cmd = {0};
   parsed_cmd.fref = &fsref;
   parsed_cmd.mccs_vspec = DDCA_VSPEC_V22;
   parsed_cmd.flags = 0;

   bool ok;
   char buf[4000];
   CAPTURE( ok = app_vcpinfo(&parsed_cmd), buf, sizeof(buf) );
   CK(ok);
   CK(strstr(buf, "VCP code 10") != NULL);

   set_output_level(saved_ol);
}


static void test_app_vcpinfo_unrecognized_feature(void) {
   DDCA_Output_Level saved_ol = get_output_level();
   set_output_level(DDCA_OL_NORMAL);

   Feature_Set_Ref fsref;
   fsref.subset = VCP_SUBSET_SINGLE_FEATURE;
   fsref.features = EMPTY_BIT_SET_256;
   fsref.features = bs256_insert(fsref.features, 0xe1);   // manufacturer-reserved

   Parsed_Cmd parsed_cmd = {0};
   parsed_cmd.fref = &fsref;
   parsed_cmd.mccs_vspec = DDCA_VSPEC_V22;

   bool ok;
   char buf[4000];
   CAPTURE( ok = app_vcpinfo(&parsed_cmd), buf, sizeof(buf) );
   CK(ok);
   CK(strstr(buf, "Reserved for manufacturer use") != NULL);

   set_output_level(saved_ol);
}


int main(int argc, char ** argv) {
   test_app_listvcp();
   test_report_vcp_feature_table_entry_smoke();
   test_app_vcpinfo_single_feature();
   test_app_vcpinfo_unrecognized_feature();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
