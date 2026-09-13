/** @file test_ddc_open_close.c
 *
 *  Standalone unit tests for src/ddc/ddc_open_close.c: the per-thread open
 *  display bookkeeping, handle validation, and the report function.
 *
 *  ddc_open_display() and ddc_close_display() themselves are not tested.
 *  They open an I2C or USB device, and their result is a property of the
 *  hardware present, not of this code.  What is tested is everything around
 *  them that maintains and interrogates the two tables they write:
 *  open_displays, which holds every handle open in any thread, and
 *  open_displays_for_thread, the per-thread array.
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

#include "util/report_util.h"

#include "base/displays.h"
#include "base/status_code_mgt.h"

#include "ddc/ddc_open_close.h"

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

#define RUN(f) do { printf("-- %s()\n", #f); f(); } while(0)

// Bus 9999 does not exist.  Nothing here opens it: the handle is a vehicle
// for exercising the bookkeeping, never a device to talk to.
#define NONEXISTENT_BUSNO 9999


/** add/remove report whether they changed the per-thread array, and adding
 *  a handle already present is not an error.
 */
static void test_per_thread_bookkeeping(void) {
   Display_Ref *    dref = create_bus_display_ref(NONEXISTENT_BUSNO);
   Display_Handle * dh   = create_base_display_handle(-1, dref);

   // removing from an array that does not exist yet
   CK(remove_open_display_for_current_thread(dh) == false);

   CK(add_open_display_for_current_thread(dh) == true);    // newly added
   CK(add_open_display_for_current_thread(dh) == false);   // already present

   CK(remove_open_display_for_current_thread(dh) == true);
   // the array is freed when it empties, so a second remove takes the
   // no-array path again rather than finding an empty one
   CK(remove_open_display_for_current_thread(dh) == false);

   free(dh);
   free_display_ref(dref);
}


/** Two handles coexist, and removing one leaves the other. */
static void test_per_thread_two_handles(void) {
   Display_Ref *    dref1 = create_bus_display_ref(NONEXISTENT_BUSNO);
   Display_Handle * dh1   = create_base_display_handle(-1, dref1);
   Display_Ref *    dref2 = create_bus_display_ref(NONEXISTENT_BUSNO+1);
   Display_Handle * dh2   = create_base_display_handle(-1, dref2);

   CK(add_open_display_for_current_thread(dh1) == true);
   CK(add_open_display_for_current_thread(dh2) == true);
   CK(remove_open_display_for_current_thread(dh1) == true);
   CK(remove_open_display_for_current_thread(dh1) == false);  // gone
   CK(remove_open_display_for_current_thread(dh2) == true);   // dh2 untouched

   free(dh1); free_display_ref(dref1);
   free(dh2); free_display_ref(dref2);
}


/** A handle that was never opened is not in open_displays, and a
 *  disconnected display is reported as such before the table is consulted.
 */
static void test_validate_display_handle2(void) {
   Display_Ref *    dref = create_bus_display_ref(NONEXISTENT_BUSNO);
   Display_Handle * dh   = create_base_display_handle(-1, dref);

   CK_INT(ddc_validate_display_handle2(dh), DDCRC_ARG);

   // disconnected is tested first, so it wins over absence from the table
   dref->disconnected = true;
   CK_INT(ddc_validate_display_handle2(dh), DDCRC_DISCONNECTED);
   dref->disconnected = false;
   CK_INT(ddc_validate_display_handle2(dh), DDCRC_ARG);

   free(dh);
   free_display_ref(dref);
}


/** The report names the table and says so when it is empty. */
static void test_dbgrpt_valid_display_handles(void) {
   char * cap = NULL;
   size_t sz = 0;
   FILE * ms = open_memstream(&cap, &sz);
   rpt_push_output_dest(ms);
   ddc_dbgrpt_valid_display_handles(0);
   fflush(ms);
   rpt_pop_output_dest();
   fclose(ms);

   CK(cap != NULL);
   CK(cap && strstr(cap, "open_displays") != NULL);
   CK(cap && strstr(cap, "None") != NULL);   // nothing has been opened
   free(cap);
}


/** Closing when nothing is open is a no-op, by either route. */
static void test_close_all_when_none_open(void) {
   ddc_close_all_displays_for_current_thread(false);
   ddc_close_all_displays();
   CK(true);   // reaching here without an assertion failure is the test
}


int main(int argc, char ** argv) {
   setvbuf(stdout, NULL, _IONBF, 0);   // so output survives a crash

   // creates the open_displays hash table, which the functions below assert on
   init_ddc_open_close();

   RUN(test_per_thread_bookkeeping);
   RUN(test_per_thread_two_handles);
   RUN(test_validate_display_handle2);
   RUN(test_dbgrpt_valid_display_handles);
   RUN(test_close_all_when_none_open);

   terminate_ddc_open_close();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
