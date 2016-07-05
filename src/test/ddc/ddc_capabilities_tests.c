/* ddc_capabilities_tests.c
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#include <unistd.h>

#include "util/string_util.h"

#include "base/ddc_packets.h"
#include "base/parms.h"
#include "base/sleep.h"

#include "test/i2c/i2c_io_old.h"
#include "i2c/i2c_bus_core.h"

#include "test/ddc/ddc_capabilities_tests.h"


//
// Test driver for exploratory programming
//

void probe_get_capabilities(int busno, char* write_mode, char* read_mode, Byte addr) {
   printf("\n(probe_get_capabilities) busno=%d, write_mode=%s, read_mode=%s, addr=0x%02x\n",
                        busno, write_mode, read_mode, addr);
   int file;
   int rc;
   unsigned char * readbuf;

   if (!i2c_verify_functions_supported(busno, write_mode, read_mode))
      return;

   // For testing, just read first 32 bytes of capabilities
   unsigned char packet_bytes[] = {0x6e, 0x51, 0x83, 0xf3, 0x00, 0x00, 0x00};
   packet_bytes[6] = ddc_checksum(packet_bytes, 6, false);

   int len_packet_bytes = sizeof(packet_bytes);

   file = i2c_open_bus(busno, CALLOPT_ERR_ABORT);
   printf("Setting addr to %02x\n", addr);
   i2c_set_addr(file, addr);
   // usleep(TIMEOUT);
   sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, NULL);

   // rc = perform_i2c_write(file, write_mode, len_packet_bytes-1, packet_bytes+1);
   set_i2c_write_mode(write_mode);
   rc = perform_i2c_write2(file, len_packet_bytes-1, packet_bytes+1, DDC_TIMEOUT_USE_DEFAULT);

   if (rc >= 0) {
      readbuf = (unsigned char *)calloc(sizeof(unsigned char),256);
      // Byte cmd_byte = 0x6e;
      set_i2c_read_mode(read_mode);
      rc = perform_i2c_read2(file, 200, readbuf, DDC_TIMEOUT_USE_DEFAULT);
      // rc = perform_i2c_read(file, read_mode, 200, readbuf);

      if (rc >= 0)
         hex_dump(readbuf, rc);
   }
   close(file);
}



void test_get_capabilities_for_bus(int busno) {
   printf("\n========== Probing get capabilities =============\n");
   //               busno, write_mode,                       read_mode, addr,
   probe_get_capabilities(busno, "write",                          "read",    0x37);  // busno=3, write succeeds
// probe_get_capabilities(busno, "write",                          "read",    0x6e);  // busno=3, write fails, ENXIO
   probe_get_capabilities(busno, "i2c_smbus_write_byte",           "read", 0x37);
   probe_get_capabilities(busno, "i2c_smbus_write_byte_data",      "read", 0x37);
// probe_get_capabilities(busno, "i2c_smbus_write_block_data",     "read", 0x37);  // i2c_smbus_write_block_data always wrong
   probe_get_capabilities(busno, "i2c_smbus_write_i2c_block_data", "read", 0x37);
// probe_get_capabilities(busno, "i2c_smbus_write_i2c_block_data", "i2c_smbus_read_block_data", 0x37);
   probe_get_capabilities(busno, "i2c_smbus_write_i2c_block_data", "i2c_smbus_read_i2c_block_data", 0x37);
   probe_get_capabilities(busno, "write",                          "i2c_smbus_read_byte",    0x37);
   probe_get_capabilities(busno, "write",                          "i2c_smbus_read_byte_data",    0x37);
   probe_get_capabilities(busno, "write",                          "i2c_smbus_read_i2c_block_data",    0x37);
}


