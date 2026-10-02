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

/* The edid attribute of card2-DP-4 on ritter, byte for byte.  The Nvidia driver
 * publishes this for a connected DisplayPort connector instead of the EDID of the
 * monitor attached to it.  Structurally impeccable -- correct header, and byte 127
 * makes block 0 sum to zero -- but it names nothing: manufacturer id "NVD" is the
 * driver's own, product code and serial are zero, and bytes 54-125 hold no
 * detailed timing descriptor at all.
 */
static const Byte ritter_dp4_placeholder[128] = {
   0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00, 0x3a,0xc4,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x01,0x04,0x95,0x00,0x00,0x78, 0xee,0x91,0xa3,0x54,0x4c,0x99,0x26,0x0f,
   0x50,0x54,0x00,0x20,0x00,0x00,0x01,0x01, 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x92,
};

static void test_placeholder(void) {
   char buf[8];

   // The captured stub.  That it passes the structural validators is the point:
   // nothing else rejects it, which is why is_placeholder_edid() exists.
   CK(is_valid_edid_header(ritter_dp4_placeholder)   == true);
   CK(is_valid_edid_checksum(ritter_dp4_placeholder) == true);
   CK(is_valid_raw_edid(ritter_dp4_placeholder, 128) == true);
   get_edid_mfg_id_in_buffer(ritter_dp4_placeholder, buf, sizeof(buf));
   CK_STR(buf, "NVD");
   CK(is_placeholder_edid(ritter_dp4_placeholder) == true);

   // A fully populated EDID is not a placeholder.
   Byte e[128];
   build_edid(e, true);
   CK(is_placeholder_edid(e) == false);

   // Nor is one without descriptor blocks, so long as it identifies a product.
   // This is the false positive the three-part test exists to prevent: the same
   // EDID the laptop heuristic accepts must not be called a placeholder.
   build_edid(e, false);
   CK(is_placeholder_edid(e) == false);

   // Each condition alone is insufficient.  Start from the stub and restore one
   // piece of identity at a time; every one of these must read as not-placeholder.
   Byte v[128];

   memcpy(v, ritter_dp4_placeholder, 128);   // product code restored
   v[0x0a] = 0x34; v[0x0b] = 0x12;
   fix_checksum(v);
   CK(is_placeholder_edid(v) == false);

   memcpy(v, ritter_dp4_placeholder, 128);   // serial number restored
   v[0x0c] = 0x04; v[0x0d] = 0x03; v[0x0e] = 0x02; v[0x0f] = 0x01;
   fix_checksum(v);
   CK(is_placeholder_edid(v) == false);

   memcpy(v, ritter_dp4_placeholder, 128);   // a single descriptor restored
   set_descriptor(v, 54, 0xfc, "Real Monitor");
   fix_checksum(v);
   CK(is_placeholder_edid(v) == false);

   // A descriptor anywhere counts, not just the first slot.
   memcpy(v, ritter_dp4_placeholder, 128);
   set_descriptor(v, 108, 0xff, "SN999");
   fix_checksum(v);
   CK(is_placeholder_edid(v) == false);

   // One non-zero byte at either end of the descriptor range is enough.
   memcpy(v, ritter_dp4_placeholder, 128);
   v[54] = 0x01;  fix_checksum(v);
   CK(is_placeholder_edid(v) == false);
   memcpy(v, ritter_dp4_placeholder, 128);
   v[125] = 0x01; fix_checksum(v);
   CK(is_placeholder_edid(v) == false);

   // Byte 126 is the extension block count and 127 the checksum; neither is part
   // of the descriptor range, so neither makes an otherwise empty EDID identify
   // a display.
   memcpy(v, ritter_dp4_placeholder, 128);
   v[126] = 0x01; fix_checksum(v);
   CK(is_placeholder_edid(v) == true);

   // A wholly zero block past the header is a placeholder too.
   memset(v, 0, 128);
   v[0] = 0x00; memset(v+1, 0xff, 6); v[7] = 0x00;
   fix_checksum(v);
   CK(is_placeholder_edid(v) == true);
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
   setvbuf(stdout, NULL, _IONBF, 0);   // so output survives a crash

   test_validators();
   test_mfg_id();
   test_parse();
   test_laptop();
   test_placeholder();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
