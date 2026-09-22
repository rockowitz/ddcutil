/** @file test_ddc_displays.c
 *
 *  Standalone unit tests for the subset of src/ddc/ddc_displays.c that
 *  operates on the master display list rather than on hardware: the
 *  accessors and filters over #all_display_refs, the report functions
 *  they feed, #Display_Ref validation, and the USB detection switch.
 *
 *  Display detection itself is not tested.  ddc_detect_all_displays(),
 *  ddc_ensure_displays_detected(), ddc_redetect_displays() and
 *  ddc_discard_detected_displays() probe the I2C buses and open displays;
 *  what they return is a property of the hardware present, not of this
 *  code.  ddc_redetect_displays() is covered here only to the extent that
 *  it is reachable and registered in the RTTI function name table -- the
 *  check that the extraction of the non-watch portion of
 *  dw_redetect_displays() into src/ddc left a properly registered
 *  function behind.  Its behavior is exercised by the stress_watch sample
 *  client, which drives it through ddca_redetect_displays().
 *
 *  The tests instead populate all_display_refs directly with fabricated
 *  Display_Refs (created by create_bus_display_ref() for a bus number no
 *  system has, never opened, never published) and check what the functions
 *  under test make of them.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: ddc source files cross-reference each other
 *  and the rest of the ddcutil core extensively, so it links the full
 *  top-level libcommon convenience library (the same aggregate the
 *  ddcutil executable itself links) rather than a minimal per-directory
 *  library set.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_types.h"

#include "util/report_util.h"
#include "util/string_util.h"

#include "base/displays.h"
#include "base/rtti.h"

#include "ddc/ddc_displays.h"

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

#define RUN(f) do { printf("-- %s()\n", #f); f(); } while(0)

/* Runs statement `stmt` with the rpt_* output destination redirected to a
 * memory stream.  On return *(bufloc) holds the captured text; the caller
 * frees it. */
#define CAPTURE(bufloc, stmt) do { \
   size_t _sz = 0; \
   *(bufloc) = NULL; \
   FILE * _ms = open_memstream((bufloc), &_sz); \
   rpt_push_output_dest(_ms); \
   stmt; \
   fflush(_ms); \
   rpt_pop_output_dest(); \
   fclose(_ms); \
} while(0)

// No system has this bus.  Nothing here opens it: the Display_Refs built on
// it are vehicles for exercising the list handling, never devices to talk to.
#define NONEXISTENT_BUSNO 9990


static Display_Ref * make_dref(int busno, int dispno, bool disconnected) {
   Display_Ref * dref = create_bus_display_ref(busno);
   dref->dispno       = dispno;
   dref->disconnected = disconnected;
   return dref;
}


static void discard_dref(Display_Ref * dref) {
   dref->flags |= DREF_TRANSIENT;   // free_display_ref() frees only transient refs
   free_display_ref(dref);
}


static int count_lines(const char * s) {
   int ct = 0;
   for (const char * p = s; *p; p++) {
      if (*p == '\n')
         ct++;
   }
   return ct;
}


static bool array_contains(GPtrArray * arr, Display_Ref * dref) {
   for (int ndx = 0; ndx < arr->len; ndx++) {
      if (g_ptr_array_index(arr, ndx) == dref)
         return true;
   }
   return false;
}


/** The function extracted from dw_redetect_displays() is registered in the
 *  RTTI function name table, in both directions.
 */
static void test_redetect_rtti_registration(void) {
   // a name that is not in the table, so a hit below means something
   CK(rtti_get_func_addr_by_name("ddc_no_such_function") == NULL);

   CK(rtti_get_func_addr_by_name("ddc_redetect_displays") == (void*) ddc_redetect_displays);

   char * name = rtti_get_func_name_by_addr((void*) ddc_redetect_displays);
   CK(name != NULL);
   if (name)
      CK(streq(name, "ddc_redetect_displays"));

   // the neighbors it was factored out alongside
   CK(rtti_get_func_addr_by_name("ddc_discard_detected_displays") ==
                                  (void*) ddc_discard_detected_displays);
   CK(rtti_get_func_addr_by_name("ddc_detect_all_displays") ==
                                  (void*) ddc_detect_all_displays);
}


/** Before detection there is no list, and the accessors that tolerate its
 *  absence say so rather than fabricating an empty answer.
 */
static void test_before_detection(void) {
   CK(all_display_refs == NULL);          // the state every other test restores

   CK(ddc_displays_already_detected() == false);
   CK_INT(ddc_get_display_count(false), -1);
   CK_INT(ddc_get_display_count(true),  -1);
   CK(ddc_get_bus_open_errors() == NULL);
}


/** USB display detection may be switched only before detection has run. */
static void test_enable_usb_display_detection(void) {
   CK(all_display_refs == NULL);
   CK_INT(ddc_enable_usb_display_detection(true),  DDCRC_OK);
   CK_INT(ddc_enable_usb_display_detection(false), DDCRC_OK);

   all_display_refs = g_ptr_array_new();
   CK_INT(ddc_enable_usb_display_detection(true),  DDCRC_INVALID_OPERATION);
   CK_INT(ddc_enable_usb_display_detection(false), DDCRC_INVALID_OPERATION);

   g_ptr_array_free(all_display_refs, true);
   all_display_refs = NULL;
}


/** An empty list is still a list: detection has occurred, the count is 0
 *  rather than -1, and the array itself is handed back unchanged.
 */
static void test_empty_list(void) {
   all_display_refs = g_ptr_array_new();

   CK(ddc_displays_already_detected() == true);
   CK(ddc_get_all_display_refs() == all_display_refs);
   CK_INT(ddc_get_display_count(false), 0);
   CK_INT(ddc_get_display_count(true),  0);

   GPtrArray * filtered = ddc_get_filtered_display_refs(true, true);
   CK(filtered != NULL);
   CK_INT(filtered->len, 0);
   g_ptr_array_free(filtered, true);

   g_ptr_array_free(all_display_refs, true);
   all_display_refs = NULL;
}


/** The two independent filters of ddc_get_filtered_display_refs():
 *  include_invalid_displays admits refs never assigned a display number,
 *  include_removed_drefs admits refs marked disconnected.
 */
static void test_filtered_display_refs(void) {
   Display_Ref * valid        = make_dref(NONEXISTENT_BUSNO,   1, false);
   Display_Ref * invalid      = make_dref(NONEXISTENT_BUSNO+1, -1, false);
   Display_Ref * removed      = make_dref(NONEXISTENT_BUSNO+2,  2, true);
   Display_Ref * both         = make_dref(NONEXISTENT_BUSNO+3, -1, true);

   all_display_refs = g_ptr_array_new();
   g_ptr_array_add(all_display_refs, valid);
   g_ptr_array_add(all_display_refs, invalid);
   g_ptr_array_add(all_display_refs, removed);
   g_ptr_array_add(all_display_refs, both);

   GPtrArray * f = ddc_get_filtered_display_refs(false, false);   // neither
   CK_INT(f->len, 1);
   CK(array_contains(f, valid));
   g_ptr_array_free(f, true);

   f = ddc_get_filtered_display_refs(true, false);                // invalid only
   CK_INT(f->len, 2);
   CK(array_contains(f, valid));
   CK(array_contains(f, invalid));
   g_ptr_array_free(f, true);

   f = ddc_get_filtered_display_refs(false, true);                // removed only
   CK_INT(f->len, 2);
   CK(array_contains(f, valid));
   CK(array_contains(f, removed));
   g_ptr_array_free(f, true);

   f = ddc_get_filtered_display_refs(true, true);                 // both
   CK_INT(f->len, 4);
   g_ptr_array_free(f, true);

   // ddc_get_display_count() filters on dispno only; disconnected refs count
   CK_INT(ddc_get_display_count(false), 2);   // valid, removed
   CK_INT(ddc_get_display_count(true),  4);

   g_ptr_array_free(all_display_refs, true);
   all_display_refs = NULL;
   discard_dref(valid);
   discard_dref(invalid);
   discard_dref(removed);
   discard_dref(both);
}


/** The terse report emits one line per ref that survives the filter, and
 *  a disconnected ref is labeled as such.  Removed refs are always included:
 *  ddc_dbgrpt_display_refs_terse() passes include_removed_drefs=true, so its
 *  one parameter selects on dispno alone.
 */
static void test_dbgrpt_display_refs_terse(void) {
   Display_Ref * valid   = make_dref(NONEXISTENT_BUSNO,   1, false);
   Display_Ref * invalid = make_dref(NONEXISTENT_BUSNO+1, -1, false);
   Display_Ref * removed = make_dref(NONEXISTENT_BUSNO+2,  2, true);

   all_display_refs = g_ptr_array_new();
   g_ptr_array_add(all_display_refs, valid);
   g_ptr_array_add(all_display_refs, invalid);
   g_ptr_array_add(all_display_refs, removed);

   char * cap = NULL;
   CAPTURE(&cap, ddc_dbgrpt_display_refs_terse(true, 0));
   CK(cap != NULL);
   CK_STR_CONTAINS(cap, "Display_Ref[");
   CK_STR_CONTAINS(cap, "Disconnected");
   CK_INT(count_lines(cap), 3);
   free(cap);

   // the ref never assigned a display number drops out, the removed one does not
   CAPTURE(&cap, ddc_dbgrpt_display_refs_terse(false, 0));
   CK(cap != NULL);
   CK_INT(count_lines(cap), 2);
   CK_STR_CONTAINS(cap, "Disconnected");
   free(cap);

   g_ptr_array_free(all_display_refs, true);
   all_display_refs = NULL;
   discard_dref(valid);
   discard_dref(invalid);
   discard_dref(removed);
}


/** The summary report names each field it prints, and honors the
 *  include_invalid_displays filter.
 */
static void test_dbgrpt_display_refs_summary(void) {
   Display_Ref * valid   = make_dref(NONEXISTENT_BUSNO,   1, false);
   Display_Ref * invalid = make_dref(NONEXISTENT_BUSNO+1, -1, false);

   all_display_refs = g_ptr_array_new();
   g_ptr_array_add(all_display_refs, valid);
   g_ptr_array_add(all_display_refs, invalid);

   char * cap = NULL;
   CAPTURE(&cap, ddc_dbgrpt_display_refs_summary(true, /*report_businfo*/ false, 0));
   CK(cap != NULL);
   CK_STR_CONTAINS(cap, "dref_id");
   CK_STR_CONTAINS(cap, "dispno:");
   CK_STR_CONTAINS(cap, "pedid:");
   CK_STR_CONTAINS(cap, "drm_connector:");
   free(cap);

   // invalid refs excluded: one dispno line rather than two
   CAPTURE(&cap, ddc_dbgrpt_display_refs_summary(false, false, 0));
   CK(cap != NULL);
   int dispno_lines = 0;
   for (char * p = cap; (p = strstr(p, "dispno:")); p++)
      dispno_lines++;
   CK_INT(dispno_lines, 1);
   free(cap);

   g_ptr_array_free(all_display_refs, true);
   all_display_refs = NULL;
   discard_dref(valid);
   discard_dref(invalid);
}


/** dbgrpt_bus_open_errors() reports the absence of errors explicitly, and
 *  distinguishes I2C from hiddev devices.
 */
static void test_dbgrpt_bus_open_errors(void) {
   char * cap = NULL;

   CAPTURE(&cap, dbgrpt_bus_open_errors(NULL, 0));
   CK_STR_CONTAINS(cap, "None");
   free(cap);

   GPtrArray * errors = g_ptr_array_new();
   CAPTURE(&cap, dbgrpt_bus_open_errors(errors, 0));      // empty, not NULL
   CK_STR_CONTAINS(cap, "None");
   free(cap);

   Bus_Open_Error boe_i2c = {DDCA_IO_I2C,    7, -13, "Permission denied"};
   Bus_Open_Error boe_usb = {DDCA_IO_USB,    3,  -2, "No such file"};
   g_ptr_array_add(errors, &boe_i2c);
   g_ptr_array_add(errors, &boe_usb);

   CAPTURE(&cap, dbgrpt_bus_open_errors(errors, 0));
   CK(cap && strstr(cap, "None") == NULL);
   CK_STR_CONTAINS(cap, "I2C bus:  7");
   CK_STR_CONTAINS(cap, "Permission denied");
   CK_STR_CONTAINS(cap, "hiddev bus:  3");
   CK_STR_CONTAINS(cap, "No such file");
   free(cap);

   g_ptr_array_free(errors, true);
}


/** ddc_get_bus_open_errors() returns whatever detection left in
 *  display_open_errors, without interpreting it.
 */
static void test_get_bus_open_errors(void) {
   all_display_refs = g_ptr_array_new();
   GPtrArray * errors = g_ptr_array_new();
   display_open_errors = errors;

   CK(ddc_get_bus_open_errors() == errors);

   display_open_errors = NULL;
   CK(ddc_get_bus_open_errors() == NULL);

   g_ptr_array_free(errors, true);
   g_ptr_array_free(all_display_refs, true);
   all_display_refs = NULL;
}


/** ddc_validate_display_ref2() rejects anything that is not a live
 *  Display_Ref, and reports a disconnected one distinctly.
 *
 *  The EDID and DPMS checks that the Dref_Validation_Options select are
 *  currently compiled out (DONT_CHECK_EDID), so every option combination
 *  yields the same answer.  The checks below pin the live behavior; they
 *  are written per option so they keep their meaning if that changes.
 */
static void test_validate_display_ref2(void) {
   all_display_refs = g_ptr_array_new();    // the function asserts it exists

   CK_INT(ddc_validate_display_ref2(NULL, DREF_VALIDATE_BASIC_ONLY), DDCRC_ARG);

   Display_Ref * dref = make_dref(NONEXISTENT_BUSNO, 1, false);
   g_ptr_array_add(all_display_refs, dref);

   CK_INT(ddc_validate_display_ref2(dref, DREF_VALIDATE_BASIC_ONLY), DDCRC_OK);
   CK_INT(ddc_validate_display_ref2(dref, DREF_VALIDATE_EDID),       DDCRC_OK);
   CK_INT(ddc_validate_display_ref2(dref, DREF_VALIDATE_AWAKE),      DDCRC_OK);
   CK_INT(ddc_validate_display_ref2(dref, DREF_VALIDATE_ALL),        DDCRC_OK);

   dref->disconnected = true;
   CK_INT(ddc_validate_display_ref2(dref, DREF_VALIDATE_BASIC_ONLY), DDCRC_DISCONNECTED);
   CK_INT(ddc_validate_display_ref2(dref, DREF_VALIDATE_ALL),        DDCRC_DISCONNECTED);
   dref->disconnected = false;

   // a struct that is not a Display_Ref at all
   char saved_marker[4];
   memcpy(saved_marker, dref->marker, 4);
   memcpy(dref->marker, "XXXX", 4);
   CK_INT(ddc_validate_display_ref2(dref, DREF_VALIDATE_BASIC_ONLY), DDCRC_ARG);
   memcpy(dref->marker, saved_marker, 4);

   g_ptr_array_free(all_display_refs, true);
   all_display_refs = NULL;
   discard_dref(dref);
}


int main(int argc, char ** argv) {
   setvbuf(stdout, NULL, _IONBF, 0);   // so output survives a crash

   // init_displays() creates published_dref_hash, which free_display_ref()
   // removes from; init_ddc_displays() populates the RTTI table entries.
   init_displays();
   init_ddc_displays();

   RUN(test_redetect_rtti_registration);
   RUN(test_before_detection);
   RUN(test_enable_usb_display_detection);
   RUN(test_empty_list);
   RUN(test_filtered_display_refs);
   RUN(test_dbgrpt_display_refs_terse);
   RUN(test_dbgrpt_display_refs_summary);
   RUN(test_dbgrpt_bus_open_errors);
   RUN(test_get_bus_open_errors);
   RUN(test_validate_display_ref2);

   terminate_displays();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
