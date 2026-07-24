/** @file test_ddc_serialize.c
 *
 *  Standalone unit tests for src/ddc/ddc_serialize.c: the JSON
 *  serialize/deserialize round trip for a Display_Ref and its component
 *  fields (io_path, MCCS version, EDID, Monitor_Model_Key), the
 *  deserialized_displays lookup, and the on-disk displays cache
 *  (ddc_store_displays_cache()/ddc_restore_displays_cache()/
 *  ddc_erase_displays_cache()).
 *
 *  The cache file lives under $XDG_CACHE_HOME (via xdg_cache_home_file()),
 *  so main() below points $XDG_CACHE_HOME at a freshly created temporary
 *  directory before running any test, the same sandboxing approach used
 *  for src/vcp/persistent_capabilities.c's unit tests -- the real user's
 *  cache file is never touched.
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

#include <errno.h>
#include <glib-2.0/glib.h>
#include <jansson.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/coredefs.h"
#include "util/edid.h"
#include "util/file_util.h"

#include "base/displays.h"
#include "base/monitor_model_key.h"

#include "ddc/ddc_serialize.h"

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


// Builds a structurally valid 128-byte EDID (correct header + checksum)
// filled otherwise with fill_byte, so create_parsed_edid() (called inside
// deserialize_parsed_edid()) accepts it rather than rejecting it as
// malformed. The decoded mfg_id/model_name/etc are not meaningful; only
// round-trip fidelity of the raw bytes is checked.
static Parsed_Edid * make_valid_edid(Byte fill_byte, const char * source) {
   Byte raw[128];
   memset(raw, fill_byte, 128);
   raw[0] = 0x00; raw[1] = raw[2] = raw[3] = raw[4] = raw[5] = raw[6] = 0xff; raw[7] = 0x00;
   int sum = 0;
   for (int i = 0; i < 127; i++)
      sum += raw[i];
   raw[127] = (Byte)((256 - (sum % 256)) % 256);
   return create_parsed_edid2(raw, source);
}


static void test_serialize_deserialize_dpath(void) {
   DDCA_IO_Path path;
   path.io_mode = DDCA_IO_I2C;
   path.path.i2c_busno = 42;

   json_t * j = serialize_dpath(path);
   DDCA_IO_Path path2 = deserialize_dpath(j);
   CK(path2.io_mode == DDCA_IO_I2C);
   CK_INT(path2.path.i2c_busno, 42);
   json_decref(j);
}


static void test_serialize_deserialize_vspec(void) {
   DDCA_MCCS_Version_Spec vspec = {2, 1};
   json_t * j = serialize_vspec(vspec);
   DDCA_MCCS_Version_Spec vspec2 = deserialize_vspec(j);
   CK_INT(vspec2.major, 2);
   CK_INT(vspec2.minor, 1);
   json_decref(j);
}


static void test_serialize_deserialize_parsed_edid(void) {
   Parsed_Edid * orig = make_valid_edid(0x37, "TEST");
   CK(orig != NULL);
   if (!orig)
      return;

   json_t * j = serialize_parsed_edid(orig);
   Parsed_Edid * back = deserialize_parsed_edid(j);
   CK(back != NULL);
   if (back) {
      CK(memcmp(back->bytes, orig->bytes, 128) == 0);
      CK_STR(back->edid_source, "TEST");
      free_parsed_edid(back);
   }
   json_decref(j);
   free_parsed_edid(orig);
}


static void test_serialize_deserialize_mmk(void) {
   Monitor_Model_Key * mmk = mmk_new("ACM", "TestModel", 0x1234);
   json_t * j = serialize_mmk(mmk);
   Monitor_Model_Key * back = deserialize_mmid(j);
   CK(back != NULL);
   if (back) {
      CK_STR(back->mfg_id, "ACM");
      CK_STR(back->model_name, "TestModel");
      CK_INT(back->product_code, 0x1234);
      mmk_free(back);
   }
   json_decref(j);
   mmk_free(mmk);
}


static void test_serialize_deserialize_one_display(void) {
   Display_Ref * dref = create_bus_display_ref(55);
   dref->pedid = make_valid_edid(0x42, "TEST");
   CK(dref->pedid != NULL);
   dref->mmid = mmk_new("ACM", "RoundTripMdl", 0x0202);
   dref->vcp_version_xdf.major = 2;
   dref->vcp_version_xdf.minor = 1;
   dref->vcp_version_cmdline.major = 3;
   dref->vcp_version_cmdline.minor = 0;
   dref->flags = DREF_DDC_COMMUNICATION_WORKING | DREF_DDC_IS_MONITOR;
   dref->capabilities_string = strdup("(vcp(10 12))");
   dref->dispno = 1;

   json_t * j = serialize_one_display(dref);
   CK(j != NULL);

   Display_Ref * back = deserialize_one_display(j);
   CK(back != NULL);
   if (back) {
      CK(back->io_path.io_mode == DDCA_IO_I2C);
      CK_INT(back->io_path.path.i2c_busno, 55);
      CK_INT(back->vcp_version_xdf.major, 2);
      CK_INT(back->vcp_version_xdf.minor, 1);
      CK_INT(back->vcp_version_cmdline.major, 3);
      CK_INT(back->vcp_version_cmdline.minor, 0);
      CK_INT(back->flags, DREF_DDC_COMMUNICATION_WORKING | DREF_DDC_IS_MONITOR);
      CK_STR(back->capabilities_string, "(vcp(10 12))");
      CK_INT(back->dispno, 1);
      CK(back->pedid != NULL && memcmp(back->pedid->bytes, dref->pedid->bytes, 128) == 0);
      CK(back->mmid != NULL);
      if (back->mmid) {
         CK_STR(back->mmid->mfg_id, "ACM");
         CK_STR(back->mmid->model_name, "RoundTripMdl");
      }
   }
   json_decref(j);
}


static void test_ddc_find_deserialized_display(void) {
   Parsed_Edid * edid1 = make_valid_edid(0x11, "TEST");
   Parsed_Edid * edid2 = make_valid_edid(0x22, "TEST");
   Display_Ref * d1 = create_bus_display_ref(201);
   d1->pedid = edid1;
   Display_Ref * d2 = create_bus_display_ref(202);
   d2->pedid = edid2;

   deserialized_displays = g_ptr_array_new();
   g_ptr_array_add(deserialized_displays, d1);
   g_ptr_array_add(deserialized_displays, d2);

   CK(ddc_find_deserialized_display(201, edid1->bytes) == d1);
   CK(ddc_find_deserialized_display(202, edid2->bytes) == d2);
   // wrong busno for the matching edid
   CK(ddc_find_deserialized_display(999, edid1->bytes) == NULL);
   // right busno, wrong edid
   CK(ddc_find_deserialized_display(201, edid2->bytes) == NULL);

   g_ptr_array_free(deserialized_displays, true);
   deserialized_displays = NULL;
}


static void test_store_restore_erase_cache(void) {
   char * fn = ddc_displays_cache_file_name();
   CK(fn != NULL);

   // no displays detected yet -> nothing to store
   all_display_refs = NULL;
   CK(!ddc_store_displays_cache());

   Display_Ref * dref = create_bus_display_ref(301);
   dref->pedid = make_valid_edid(0x55, "TEST");
   dref->mmid = mmk_new("ACM", "CacheModel", 0x0303);
   dref->flags = DREF_DDC_COMMUNICATION_WORKING;
   dref->dispno = 1;
   all_display_refs = g_ptr_array_new();
   g_ptr_array_add(all_display_refs, dref);

   CK(ddc_store_displays_cache());
   CK(fn != NULL && regular_file_exists(fn));

   ddc_restore_displays_cache();
   CK(deserialized_displays != NULL);
   CK_INT(deserialized_displays->len, 1);
   if (deserialized_displays->len == 1) {
      Display_Ref * restored = g_ptr_array_index(deserialized_displays, 0);
      CK_INT(restored->io_path.path.i2c_busno, 301);
      CK(restored->mmid != NULL);
      if (restored->mmid)
         CK_STR(restored->mmid->model_name, "CacheModel");
   }

   ddc_erase_displays_cache();
   CK(!regular_file_exists(fn));

   free(fn);
   all_display_refs = NULL;
}


int main(int argc, char ** argv) {
   char tmpl[] = "/tmp/ddcutil_test_xdg_cache_XXXXXX";
   char * tmpdir = mkdtemp(tmpl);
   if (!tmpdir) {
      fprintf(stderr, "mkdtemp() failed: %s\n", strerror(errno));
      return 2;
   }
   setenv("XDG_CACHE_HOME", tmpdir, 1);

   test_serialize_deserialize_dpath();
   test_serialize_deserialize_vspec();
   test_serialize_deserialize_parsed_edid();
   test_serialize_deserialize_mmk();
   test_serialize_deserialize_one_display();
   test_ddc_find_deserialized_display();
   test_store_restore_erase_cache();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
