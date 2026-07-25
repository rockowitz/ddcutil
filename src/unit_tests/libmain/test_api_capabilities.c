/** @file test_api_capabilities.c
 *
 *  Standalone unit tests for src/libmain/api_capabilities.c:
 *  ddca_free_parsed_capabilities() and ddca_feature_list_from_capabilities().
 *
 *  A DDCA_Capabilities struct is fabricated by hand here, matching exactly
 *  what ddca_parse_capabilities_string() would produce, rather than calling
 *  ddca_parse_capabilities_string() itself: that function is wrapped in
 *  API_PROLOGX, which performs full implicit library initialization
 *  (including real I2C bus/display detection) the first time it -- or any
 *  other API_PROLOG(X)-wrapped function -- is called if the library isn't
 *  already initialized. That is exactly the kind of real-hardware side
 *  effect these unit tests are designed to avoid.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon+libsharedlib unit test: it links the internal
 *  libmain/libsharedlib.la convenience library (the intermediate library
 *  that becomes libddcutil.so) together with the top-level libcommon
 *  convenience library it depends on.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "public/ddcutil_c_api.h"
#include "public/ddcutil_types.h"

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


// Builds a DDCA_Capabilities matching what ddca_parse_capabilities_string()
// would produce for a capabilities string with 2 vcp() features (one with
// declared values, one without) and no messages.
static DDCA_Capabilities * make_capabilities(void) {
   DDCA_Capabilities * caps = calloc(1, sizeof(DDCA_Capabilities));
   memcpy(caps->marker, DDCA_CAPABILITIES_MARKER, 4);
   caps->unparsed_string = strdup("(vcp(10 60(01 02)))");
   caps->version_spec.major = 2;
   caps->version_spec.minor = 1;

   caps->cmd_ct = 2;
   caps->cmd_codes = malloc(2);
   caps->cmd_codes[0] = 0x01;
   caps->cmd_codes[1] = 0x02;

   caps->vcp_code_ct = 2;
   caps->vcp_codes = calloc(2, sizeof(DDCA_Cap_Vcp));

   memcpy(caps->vcp_codes[0].marker, DDCA_CAP_VCP_MARKER, 4);
   caps->vcp_codes[0].feature_code = 0x10;
   caps->vcp_codes[0].value_ct = 0;
   caps->vcp_codes[0].values = NULL;

   memcpy(caps->vcp_codes[1].marker, DDCA_CAP_VCP_MARKER, 4);
   caps->vcp_codes[1].feature_code = 0x60;
   caps->vcp_codes[1].value_ct = 2;
   caps->vcp_codes[1].values = malloc(2);
   caps->vcp_codes[1].values[0] = 0x01;
   caps->vcp_codes[1].values[1] = 0x02;

   caps->msg_ct = 0;
   caps->messages = NULL;

   return caps;
}


static void test_feature_list_from_capabilities(void) {
   DDCA_Capabilities * caps = make_capabilities();

   DDCA_Feature_List list = ddca_feature_list_from_capabilities(caps);
   CK(ddca_feature_list_contains(list, 0x10));
   CK(ddca_feature_list_contains(list, 0x60));
   CK(!ddca_feature_list_contains(list, 0x12));
   CK_INT(ddca_feature_list_count(list), 2);

   ddca_free_parsed_capabilities(caps);
}


static void test_feature_list_from_capabilities_null(void) {
   DDCA_Feature_List list = ddca_feature_list_from_capabilities(NULL);
   CK_INT(ddca_feature_list_count(list), 0);
}


static void test_free_parsed_capabilities_null_safe(void) {
   ddca_free_parsed_capabilities(NULL);   // must not crash
   CK(true);
}


static void test_free_parsed_capabilities_with_messages(void) {
   // exercises the messages != NULL path through ntsa_free(), distinct
   // from the msg_ct == 0 case exercised by test_feature_list_from_capabilities()
   DDCA_Capabilities * caps = make_capabilities();
   caps->msg_ct = 2;
   caps->messages = calloc(3, sizeof(char *));   // NULL-terminated array
   caps->messages[0] = strdup("first parse warning");
   caps->messages[1] = strdup("second parse warning");
   caps->messages[2] = NULL;

   ddca_free_parsed_capabilities(caps);   // must not crash
   CK(true);
}


int main(int argc, char ** argv) {
   test_feature_list_from_capabilities();
   test_feature_list_from_capabilities_null();
   test_free_parsed_capabilities_null_safe();
   test_free_parsed_capabilities_with_messages();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
