/** @file test_edid.c
 *
 *  Standalone unit tests for src/util/edid.c.
 *
 *  The tests build a synthetic but structurally valid 128-byte EDID (fixed
 *  header, packed "DEL" manufacturer id, known product/serial/date fields, a
 *  digital video-input byte, and monitor-name and serial descriptor blocks) and
 *  check the checksum/header validators, the manufacturer-id unpacking, and the
 *  fields extracted by create_parsed_edid().  A second, descriptor-less EDID
 *  exercises the laptop heuristic.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/edid.h"

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

// Sets byte 127 so the 128-byte block sums to 0 mod 256.
static void fix_checksum(Byte * e) {
   Byte sum = 0;
   for (int i = 0; i < 127; i++) sum += e[i];
   e[127] = (Byte)(0 - sum);
}

// Writes a monitor descriptor block (tags 0xfc name / 0xff serial / 0xfe other).
static void set_descriptor(Byte * e, int offset, Byte tag, const char * text) {
   e[offset+0] = 0; e[offset+1] = 0; e[offset+2] = 0;
   e[offset+3] = tag; e[offset+4] = 0;
   int n = strlen(text);
   if (n > 13) n = 13;
   memcpy(e + offset + 5, text, n);
   if (n < 13) e[offset + 5 + n] = 0x0a;   // line-feed terminator
}

// Builds a valid EDID; if with_descriptors, adds monitor-name and serial blocks.
static void build_edid(Byte * e, bool with_descriptors) {
   memset(e, 0, 128);
   e[0] = 0x00;
   memset(e + 1, 0xff, 6);
   e[7] = 0x00;                         // header 00 ff ff ff ff ff ff 00
   e[8] = 0x10; e[9] = 0xac;            // manufacturer id "DEL"
   e[0x0a] = 0x34; e[0x0b] = 0x12;      // product code 0x1234
   e[0x0c] = 0x04; e[0x0d] = 0x03;      // serial binary 0x01020304
   e[0x0e] = 0x02; e[0x0f] = 0x01;
   e[16] = 5;                           // manufacture week 5
   e[17] = 30;                          // year 1990 + 30 = 2020
   e[18] = 1; e[19] = 4;                // EDID version 1.4
   e[0x14] = 0x80;                      // video input definition: digital
   if (with_descriptors) {
      set_descriptor(e, 54, 0xfc, "Test Monitor");
      set_descriptor(e, 72, 0xff, "SN12345");
   }
   fix_checksum(e);
}

static void test_validators(void) {
   Byte e[128];
   build_edid(e, true);

   CK(is_valid_edid_header(e) == true);
   CK(is_valid_edid_checksum(e) == true);
   CK(is_valid_raw_edid(e, 128) == true);
   CK_INT(edid_checksum(e), 0);

   // too-short buffer is not a valid raw EDID
   CK(is_valid_raw_edid(e, 100) == false);

   // corrupt the header
   Byte bad[128];
   memcpy(bad, e, 128);
   bad[0] = 0x01;
   CK(is_valid_edid_header(bad) == false);
   CK(is_valid_raw_edid(bad, 128) == false);

   // corrupt a payload byte without fixing the checksum
   memcpy(bad, e, 128);
   bad[20] ^= 0xff;
   CK(is_valid_edid_checksum(bad) == false);

   // CEA-861 extension block: first byte 0x02 and a valid checksum
   Byte ext[128];
   memset(ext, 0, 128);
   ext[0] = 0x02;
   fix_checksum(ext);
   CK(is_valid_raw_cea861_extension_block(ext, 128) == true);
   CK(is_valid_raw_cea861_extension_block(ext, 64) == false);   // too short
   ext[0] = 0x00;
   CK(is_valid_raw_cea861_extension_block(ext, 128) == false);  // wrong tag
}

static void test_mfg_id(void) {
   char buf[8];
   Byte del[] = {0x10, 0xac};
   parse_mfg_id_in_buffer(del, buf, sizeof(buf));
   CK_STR(buf, "DEL");

   Byte abc[] = {0x04, 0x43};
   parse_mfg_id_in_buffer(abc, buf, sizeof(buf));
   CK_STR(buf, "ABC");

   Byte e[128];
   build_edid(e, true);
   get_edid_mfg_id_in_buffer(e, buf, sizeof(buf));
   CK_STR(buf, "DEL");
}

static void test_parse(void) {
   Byte e[128];
   build_edid(e, true);

   Parsed_Edid * pe = create_parsed_edid(e);
   CK(pe != NULL);
   CK(memcmp(pe->marker, EDID_MARKER_NAME, 4) == 0);
   CK_STR(pe->mfg_id, "DEL");
   CK_INT(pe->product_code, 0x1234);
   CK_INT(pe->serial_binary, 0x01020304);
   CK_STR(pe->model_name, "Test Monitor");
   CK_STR(pe->serial_ascii, "SN12345");
   CK_INT(pe->year, 2020);
   CK(pe->is_model_year == false);
   CK_INT(pe->manufacture_week, 5);
   CK_INT(pe->edid_version_major, 1);
   CK_INT(pe->edid_version_minor, 4);
   CK(is_input_digital(pe) == true);
   CK(is_laptop_parsed_edid(pe) == false);      // has model name and serial

   // deep copy
   Parsed_Edid * cp = copy_parsed_edid(pe);
   CK(cp != pe);
   CK_STR(cp->model_name, "Test Monitor");
   CK_INT(cp->product_code, 0x1234);
   free_parsed_edid(cp);
   free_parsed_edid(pe);

   // invalid header -> NULL
   Byte bad[128];
   build_edid(bad, true);
   bad[0] = 0x99;
   CK(create_parsed_edid(bad) == NULL);
}

static void test_laptop(void) {
   // no descriptor blocks: model name and serial remain empty -> laptop heuristic
   Byte e[128];
   build_edid(e, false);
   Parsed_Edid * pe = create_parsed_edid(e);
   CK(pe != NULL);
   CK_STR(pe->model_name, "");
   CK_STR(pe->serial_ascii, "");
   CK(is_laptop_parsed_edid(pe) == true);
   free_parsed_edid(pe);
}

int main(int argc, char ** argv) {
   test_validators();
   test_mfg_id();
   test_parse();
   test_laptop();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
