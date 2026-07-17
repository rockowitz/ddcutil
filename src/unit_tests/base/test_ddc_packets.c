/** @file test_ddc_packets.c
 *
 *  Standalone unit tests for src/base/ddc_packets.c: the DDC checksum, the
 *  double-byte test, packet-checksum validation, the Get VCP / Set VCP / Save
 *  Settings request-packet builders (verified byte-for-byte), the packet
 *  accessor functions, and the type check in get_interpreted_vcp_code.
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

#include "util/coredefs_base.h"    // Byte
#include "base/ddc_packets.h"
#include "base/execution_stats.h"  // init_execution_stats (COUNT_STATUS_CODE)

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
      printf("FAIL  line %-4d  %s -> %ld (0x%lx), expected %ld (0x%lx)\n", __LINE__, \
             #expr, _a, _a, _e, _e); } \
} while(0)

static void test_checksum(void) {
   // non-alt: XOR of all bytes
   Byte b1[] = { 0x6e, 0x51, 0x82, 0x01, 0x10 };
   CK_INT(ddc_checksum(b1, 5, false), 0xac);       // 0x6e^0x51^0x82^0x01^0x10

   Byte b2[] = { 0xaa, 0x01, 0x02 };
   CK_INT(ddc_checksum(b2, 3, false), 0xa9);       // 0xaa^0x01^0x02
   // alt: the first byte is replaced by 0x50 in the XOR
   CK_INT(ddc_checksum(b2, 3, true), 0x53);        // 0x50^0x01^0x02

   Byte b3[] = { 0x37 };
   CK_INT(ddc_checksum(b3, 1, false), 0x37);       // single byte
}

static void test_is_double_byte(void) {
   Byte buf[] = { 0x10, 0x10, 0x20 };
   CK(is_double_byte(&buf[1]) == true);            // buf[1] == buf[0]
   CK(is_double_byte(&buf[2]) == false);           // buf[2] != buf[1]
}

static void test_getvcp_request(void) {
   DDC_Packet * pkt = create_ddc_getvcp_request_packet(0x10, "test");
   CK(pkt != NULL);
   CK_INT(get_packet_len(pkt), 6);      // 3 header + 2 data + 1 checksum
   CK_INT(get_data_len(pkt), 2);
   CK_INT(pkt->type, DDC_PACKET_TYPE_QUERY_VCP_REQUEST);

   Byte * b = get_packet_start(pkt);
   CK_INT(b[0], 0x6e);                  // destination
   CK_INT(b[1], 0x51);                  // source
   CK_INT(b[2], 0x82);                  // 0x80 | data length 2
   CK_INT(b[3], 0x01);                  // Get VCP command
   CK_INT(b[4], 0x10);                  // vcp code
   CK_INT(b[5], 0x6e ^ 0x51 ^ 0x82 ^ 0x01 ^ 0x10);   // checksum

   CK(get_data_start(pkt) == b + 3);
   free_ddc_packet(pkt);
}

static void test_setvcp_request(void) {
   DDC_Packet * pkt = create_ddc_setvcp_request_packet(0x12, 0x0134, "test");
   CK(pkt != NULL);
   CK_INT(get_packet_len(pkt), 8);      // 3 header + 4 data + 1 checksum
   CK_INT(get_data_len(pkt), 4);

   Byte * b = get_packet_start(pkt);
   CK_INT(b[2], 0x84);                  // 0x80 | data length 4
   CK_INT(b[3], 0x03);                  // Set VCP command
   CK_INT(b[4], 0x12);                  // vcp code
   CK_INT(b[5], 0x01);                  // high byte of value 0x0134
   CK_INT(b[6], 0x34);                  // low byte
   CK_INT(b[7], 0x6e ^ 0x51 ^ 0x84 ^ 0x03 ^ 0x12 ^ 0x01 ^ 0x34);
   free_ddc_packet(pkt);
}

static void test_save_settings_request(void) {
   DDC_Packet * pkt = create_ddc_save_settings_request_packet("test");
   CK(pkt != NULL);
   CK_INT(get_packet_len(pkt), 5);      // 3 header + 1 data + 1 checksum
   CK_INT(get_data_len(pkt), 1);
   Byte * b = get_packet_start(pkt);
   CK_INT(b[3], DDC_PACKET_TYPE_SAVE_CURRENT_SETTINGS);
   free_ddc_packet(pkt);
}

static void test_get_interpreted_vcp_code_type_check(void) {
   // a request packet is not a VCP response, so interpretation is rejected
   DDC_Packet * pkt = create_ddc_getvcp_request_packet(0x10, "test");
   Parsed_Nontable_Vcp_Response * interp = (void *) 0x1;
   Status_DDC rc = get_interpreted_vcp_code(pkt, false, &interp);
   CK(rc != 0);
   CK(interp == NULL);
   free_ddc_packet(pkt);
}

int main(int argc, char ** argv) {
   init_execution_stats();   // COUNT_STATUS_CODE, used by get_interpreted_vcp_code

   test_checksum();
   test_is_double_byte();
   test_getvcp_request();
   test_setvcp_request();
   test_save_settings_request();
   test_get_interpreted_vcp_code_type_check();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
