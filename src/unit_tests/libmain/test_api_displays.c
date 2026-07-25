/** @file test_api_displays.c
 *
 *  Standalone unit tests for the pure subset of src/libmain/api_displays.c:
 *  the ddca_create_*_display_identifier() constructors, ddca_did_repr(),
 *  and ddca_display_ref_from_handle()/ddca_dbgrpt_display_ref() driven
 *  with a fabricated Display_Ref/Display_Handle (never opened, never
 *  published) rather than a real display.
 *
 *  ddca_free_display_identifier() is deliberately not called here: unlike
 *  the ddca_create_*_display_identifier() constructors, it is wrapped in
 *  API_PROLOGX, which performs full implicit library initialization
 *  (including real I2C bus/display detection) the first time it -- or any
 *  other API_PROLOG(X)-wrapped function -- is called if the library isn't
 *  already initialized. Display_Identifiers are instead released via the
 *  lower-level free_display_identifier() (base/displays.h), which is what
 *  ddca_free_display_identifier() calls internally after its validation.
 *
 *  Nearly everything else in api_displays.c -- display detection,
 *  opening, validation, sleep multiplier control, DDCA_Display_Info
 *  construction -- is wrapped in API_PROLOG/API_PROLOGX for the same
 *  reason, or requires a dref registered in the live published-dref hash
 *  table, and so is out of scope for these unit tests.
 *
 *  test_dbgrpt_display_ref_smoke() below exercises a real bug found while
 *  writing these tests: ddca_dbgrpt_display_ref() cast its DDCA_Display_Ref
 *  parameter directly to Display_Ref*, but under NUMERIC_DDCA_DISPLAY_REF
 *  (base/parms.h) a DDCA_Display_Ref is dref->dref_id, a small integer, not
 *  a real pointer -- every real caller obtains one via dref_to_ddca_dref()
 *  and must resolve it back via dref_from_published_ddca_dref(), exactly as
 *  ddca_dref_repr() (the function immediately preceding it in
 *  api_displays.c) already correctly did. Fixed to match.
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
#include <unistd.h>

#include "public/ddcutil_c_api.h"
#include "public/ddcutil_types.h"

#include "util/coredefs.h"
#include "util/edid.h"

#include "base/displays.h"

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

#define CK_STR_CONTAINS(actual, needle) do { \
   total++; \
   const char * _a = (actual); const char * _n = (needle); \
   if (_a == NULL || strstr(_a, _n) == NULL) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected to contain \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _n); } \
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


static void test_create_dispno_display_identifier(void) {
   DDCA_Display_Identifier did = NULL;
   DDCA_Status rc = ddca_create_dispno_display_identifier(3, &did);
   CK_INT(rc, DDCRC_OK);
   CK(did != NULL);

   Display_Identifier * pdid = (Display_Identifier *) did;
   CK(pdid->id_type == DISP_ID_DISPNO);
   CK_INT(pdid->dispno, 3);

   free_display_identifier(pdid);
}


static void test_create_busno_display_identifier(void) {
   DDCA_Display_Identifier did = NULL;
   DDCA_Status rc = ddca_create_busno_display_identifier(11, &did);
   CK_INT(rc, DDCRC_OK);
   CK(did != NULL);

   Display_Identifier * pdid = (Display_Identifier *) did;
   CK(pdid->id_type == DISP_ID_BUSNO);
   CK_INT(pdid->busno, 11);

   free_display_identifier(pdid);
}


static void test_create_mfg_model_sn_display_identifier(void) {
   DDCA_Display_Identifier did = NULL;
   DDCA_Status rc = ddca_create_mfg_model_sn_display_identifier(
         "ACM", "TestModel", "SN12345", &did);
   CK_INT(rc, DDCRC_OK);
   CK(did != NULL);

   Display_Identifier * pdid = (Display_Identifier *) did;
   CK(pdid->id_type == DISP_ID_MONSER);
   CK(strcmp(pdid->mfg_id, "ACM") == 0);
   CK(strcmp(pdid->model_name, "TestModel") == 0);
   CK(strcmp(pdid->serial_ascii, "SN12345") == 0);

   free_display_identifier(pdid);

   // all 3 fields empty is rejected
   DDCA_Display_Identifier bad = NULL;
   DDCA_Status rc2 = ddca_create_mfg_model_sn_display_identifier("", "", "", &bad);
   CK_INT(rc2, DDCRC_ARG);
   CK(bad == NULL);
}


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


static void test_create_edid_display_identifier(void) {
   Parsed_Edid * edid = make_valid_edid(0x22);
   CK(edid != NULL);

   DDCA_Display_Identifier did = NULL;
   DDCA_Status rc = ddca_create_edid_display_identifier(edid->bytes, &did);
   CK_INT(rc, DDCRC_OK);
   CK(did != NULL);

   Display_Identifier * pdid = (Display_Identifier *) did;
   CK(pdid->id_type == DISP_ID_EDID);
   CK(memcmp(pdid->edidbytes, edid->bytes, 128) == 0);

   free_display_identifier(pdid);
   free_parsed_edid(edid);

   DDCA_Display_Identifier bad = NULL;
   DDCA_Status rc2 = ddca_create_edid_display_identifier(NULL, &bad);
   CK_INT(rc2, DDCRC_ARG);
   CK(bad == NULL);
}


static void test_create_usb_display_identifier(void) {
   DDCA_Display_Identifier did = NULL;
   DDCA_Status rc = ddca_create_usb_display_identifier(3, 7, &did);
   CK_INT(rc, DDCRC_OK);
   CK(did != NULL);

   Display_Identifier * pdid = (Display_Identifier *) did;
   CK(pdid->id_type == DISP_ID_USB);
   CK_INT(pdid->usb_bus, 3);
   CK_INT(pdid->usb_device, 7);

   free_display_identifier(pdid);
}


static void test_create_usb_hiddev_display_identifier(void) {
   DDCA_Display_Identifier did = NULL;
   DDCA_Status rc = ddca_create_usb_hiddev_display_identifier(2, &did);
   CK_INT(rc, DDCRC_OK);
   CK(did != NULL);

   Display_Identifier * pdid = (Display_Identifier *) did;
   CK(pdid->id_type == DISP_ID_HIDDEV);
   CK_INT(pdid->hiddev_devno, 2);

   free_display_identifier(pdid);
}


static void test_did_repr(void) {
   DDCA_Display_Identifier did = NULL;
   ddca_create_busno_display_identifier(9, &did);

   const char * repr = ddca_did_repr(did);
   CK(repr != NULL);
   CK_STR_CONTAINS(repr, "9");

   CK(ddca_did_repr(NULL) == NULL);

   free_display_identifier((Display_Identifier *) did);
}


static void test_display_ref_from_handle(void) {
   Display_Ref * dref = create_bus_display_ref(241);
   Display_Handle * dh = create_base_display_handle(-1, dref);

   DDCA_Display_Ref ddca_dref = ddca_display_ref_from_handle((DDCA_Display_Handle) dh);
   CK(ddca_dref == dref_to_ddca_dref(dref));

   CK(ddca_display_ref_from_handle(NULL) == NULL);

   free(dh);
}


static void test_dbgrpt_display_ref_smoke(void) {
   // ddca_dbgrpt_display_ref() takes an opaque DDCA_Display_Ref handle,
   // which (under NUMERIC_DDCA_DISPLAY_REF) is dref->dref_id, not the raw
   // Display_Ref* -- it must be resolved through the published-dref table
   // (dref_from_published_ddca_dref()) rather than cast directly, exactly
   // like the ddca_dref_repr() function just above it in api_displays.c.
   // init_published_dref_hash() must run before any lookup against that
   // table; it is otherwise NULL, and GLib's g_hash_table_lookup() on a
   // NULL table logs a GLib-CRITICAL warning (though it does still
   // return NULL rather than crashing).
   init_published_dref_hash();

   Display_Ref * dref = create_bus_display_ref(242);
   DDCA_Display_Ref ddca_dref = dref_to_ddca_dref(dref);

   // not yet published: must report "invalid", not crash
   QUIETLY( ddca_dbgrpt_display_ref(ddca_dref, 0) );
   CK(true);

   // published: must find and report it
   add_published_dref_id_by_dref(dref);
   QUIETLY( ddca_dbgrpt_display_ref(ddca_dref, 0) );
   CK(true);

   QUIETLY( ddca_dbgrpt_display_ref(NULL, 0) );
   CK(true);   // reaching here without crashing is the test
}


int main(int argc, char ** argv) {
   test_create_dispno_display_identifier();
   test_create_busno_display_identifier();
   test_create_mfg_model_sn_display_identifier();
   test_create_edid_display_identifier();
   test_create_usb_display_identifier();
   test_create_usb_hiddev_display_identifier();
   test_did_repr();
   test_display_ref_from_handle();
   test_dbgrpt_display_ref_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
