/** @file test_dw_base.c
 *
 *  Standalone unit tests for src/base/dw_base.c: the display-event class and
 *  type name functions and the event-class bitmask repr.  The status-event repr
 *  functions require a live Display_Ref and are not exercised.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libbase unit test: it links the internal libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "public/ddcutil_types.h"
#include "base/dw_base.h"

static int total = 0;
static int failed = 0;

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)

static void test_class_names(void) {
   CK_STR(dw_display_event_class_name(DDCA_EVENT_CLASS_NONE), "DDCA_EVENT_CLASS_NONE");
   CK_STR(dw_display_event_class_name(DDCA_EVENT_CLASS_DPMS), "DDCA_EVENT_CLASS_DPMS");
   CK_STR(dw_display_event_class_name(DDCA_EVENT_CLASS_DISPLAY_CONNECTION),
          "DDCA_EVENT_CLASS_DISPLAY_CONNECTION");
}

static void test_type_names(void) {
   CK_STR(dw_display_event_type_name(DDCA_EVENT_DISPLAY_CONNECTED), "DDCA_EVENT_DISPLAY_CONNECTED");
   CK_STR(dw_display_event_type_name(DDCA_EVENT_DISPLAY_DISCONNECTED), "DDCA_EVENT_DISPLAY_DISCONNECTED");
   CK_STR(dw_display_event_type_name(DDCA_EVENT_DPMS_AWAKE), "DDCA_EVENT_DPMS_AWAKE");
   CK_STR(dw_display_event_type_name(DDCA_EVENT_DPMS_ASLEEP), "DDCA_EVENT_DPMS_ASLEEP");
   CK_STR(dw_display_event_type_name(DDCA_EVENT_DDC_ENABLED), "DDCA_EVENT_DDC_ENABLED");
}

static void test_classes_repr(void) {
   // no bits set -> "NONE"
   CK_STR(dw_event_classes_repr_t(DDCA_EVENT_CLASS_NONE), "NONE");
   // single class
   CK_STR(dw_event_classes_repr_t(DDCA_EVENT_CLASS_DPMS), "DDCA_EVENT_CLASS_DPMS");
   // multiple classes, comma separated, trailing comma stripped
   CK_STR(dw_event_classes_repr_t(DDCA_EVENT_CLASS_DPMS | DDCA_EVENT_CLASS_DISPLAY_CONNECTION),
          "DDCA_EVENT_CLASS_DPMS,DDCA_EVENT_CLASS_DISPLAY_CONNECTION");

   // buffer-writing variant produces the same text
   char buf[128];
   dw_event_classes_repr(buf, sizeof(buf), DDCA_EVENT_CLASS_DISPLAY_CONNECTION);
   CK_STR(buf, "DDCA_EVENT_CLASS_DISPLAY_CONNECTION");
}

int main(int argc, char ** argv) {
   test_class_names();
   test_type_names();
   test_classes_repr();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
