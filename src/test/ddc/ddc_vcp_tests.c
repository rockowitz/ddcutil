/* ddc_vcp_tests.h
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <assert.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>        // usleep
// On Fedora, i2c-dev.h is miniminal.  i2c.h is required for struct i2c_msg and
// other stuff.  On Ubuntu and SuSE, including both causes redefiition errors.
// I2C_FUNC_I2C is none definition present in the full version of i2c-dev.h but not
// in the abbreviated version
#include <linux/i2c-dev.h>
#ifndef I2C_FUNC_I2C
#include <linux/i2c.h>
#endif
#include <fcntl.h>

#include <base/util.h>
#include <base/ddc_packets.h>
#include <base/common.h>
#include <base/parms.h>
#include <base/linux_errno.h>

#include <test/i2c/i2c_io_old.h>
#include <i2c/i2c_bus_core.h>

#include <adl/adl_shim.h>

#include <ddc/ddc_packet_io.h>
#include <ddc/vcp_feature_codes.h>
#include <ddc/ddc_vcp.h>

#include <test/ddc/ddc_vcp_tests.h>

// #define TIMEOUT 50000



char * hexstring0(unsigned char * bytes, int len) {
   int alloc_size = 3*len + 1;
   char* str_buf = malloc(alloc_size);

   int i;
   for (i = 0; i < len; i++) {
      snprintf(str_buf+3*i, alloc_size-3*i, "%02x ", bytes[i]);
   }
   str_buf[3*len-1] = 0x00;
   return str_buf;
}


int single_getvcp_call(int busno, unsigned char vcp_feature_code) {
   printf("\n(%s) Starting. vcp_feature_code=0x%02x\n", __func__, vcp_feature_code );

   int ndx;
   unsigned char checksum;
   int rc;
   char * devname = malloc(12);
   snprintf(devname, 11, "/dev/i2c-%d", busno);

   int fh = open(devname, O_RDWR);
   ioctl(fh, I2C_SLAVE, 0x37);

#ifdef NO
   // write seems to be necessary to reset monitor state
   unsigned char zeroByte = 0x00;  // 0x00;
   rc = write(fh, &zeroByte, 1);
   if (rc < 0) {
      printf("(%s) Bus reset failed. rc=%d, errno=%d. \n", __func__, rc, errno );
      return -1;
   }
#endif
   // without this or 0 byte write, read() sometimes returns all 0 on P2411H
   usleep(50000);

   unsigned char ddc_cmd_bytes[] = {
      0x6e,              // address 0x37, shifted left 1 bit
      0x51,              // source address
      0x02 | 0x80,       // number of DDC data bytes, with high bit set
      0x01,              // DDC Get Feature Command
      vcp_feature_code,  //
      0x00,              // checksum, to be set
   };
   // unsigned char checksum0 = xor_bytes(ddc_cmd_bytes,5);
   checksum = ddc_checksum(ddc_cmd_bytes, 5, false);    // calculate DDC checksum on all bytes
   // assert(checksum==checksum0);
   ddc_cmd_bytes[5] = ddc_cmd_bytes[0];
   for (ndx=1; ndx < 5; ndx++) ddc_cmd_bytes[5] ^= ddc_cmd_bytes[ndx];    // calculate checksum
   // printf("(%s) ddc_cmd_bytes = %s   \n", __func__ , hexstring(ddc_cmd_bytes,6) );
   // printf("(%s) checksum=0x%02x, ddc_cmd_bytes[5]=0x%02x   \n", __func__, checksum, ddc_cmd_bytes[5] );
   // assert(ddc_cmd_bytes[5] == 0xac);
   assert(checksum == ddc_cmd_bytes[5]);

   int writect = sizeof(ddc_cmd_bytes)-1;
   rc = write(fh, ddc_cmd_bytes+1, writect);
   if (rc < 0) {
      printf("(%s) write() returned %d, errno=%d. \n", __func__, rc, errno);
      return -1;
   }
   else if (rc != writect) {
      printf("(%s) write() returned %d, expected %d   \n", __func__, rc, writect );
      return -1;
   }
   usleep(50000);

   unsigned char ddc_response_bytes[12];
   int readct = sizeof(ddc_response_bytes)-1;

   rc = read(fh, ddc_response_bytes+1, readct);
   if (rc < 0) {
      printf("(%s) read() returned %d, errno=%d.\n", __func__, rc, errno );
      return -1;
   }
   else if (rc != readct) {
      printf("(%s) read() returned %d, should be %d  \n", __func__, rc, readct );
      return -1;
   }

   // printf("(%s) read() returned %s\n", __func__, hexstring(ddc_response_bytes+1, readct) );
   printf("(%s) read() returned %s\n", __func__, hexstring0(ddc_response_bytes+1, readct) );
   // hex_dump(ddc_response_bytes,1+rc);


   int ddc_data_length = ddc_response_bytes[2] & 0x7f;
   // some monitors return a DDC null response to indicate an invalid request:
   if (ddc_response_bytes[1] == 0x6e && ddc_data_length == 0 && ddc_response_bytes[3] == 0xbe) {    // 0xbe == checksum
      printf("(%s) Received DDC null response\n", __func__ );
      return -1;
   }

   bool response_ok = true;

   if (ddc_response_bytes[1] != 0x6e) {
      // assert(ddc_response_bytes[1] == 0x6e);
      printf("(%s) Invalid address byte in response, expected 06e, actual 0x%02x\n", __func__, ddc_response_bytes[1] );
      response_ok = false;
   }

   if (ddc_data_length != 8) {
      printf("(%s) Invalid query VCP response length: %d\n", __func__, ddc_data_length );
      response_ok = false;
   }

   if (ddc_response_bytes[3] != 0x02) {       // get feature response
      printf("(%s) Expected 0x02 in feature response field, actual value 0x%02x\n", __func__, ddc_response_bytes[3] );
      response_ok = false;
   }

   ddc_response_bytes[0] = 0x50;   // for calculating DDC checksum
   // checksum0 = xor_bytes(ddc_response_bytes, sizeof(ddc_response_bytes)-1);
   unsigned char calculated_checksum = ddc_response_bytes[0];
   for (ndx=1; ndx < 11; ndx++) calculated_checksum ^= ddc_response_bytes[ndx];
   // printf("(%s) checksum0=0x%02x, calculated_checksum=0x%02x\n", __func__, checksum0, calculated_checksum );
   if (ddc_response_bytes[11] != calculated_checksum) {
      printf("(%s) Unexpected checksum.  actual=0x%02x, calculated=0x%02x  \n", __func__,
             ddc_response_bytes[11], calculated_checksum );
      response_ok = false;
   }

   if (response_ok) {
      if (ddc_response_bytes[4] == 0x00) {         // valid VCP code
         // The interpretation for most VCP codes:
         int max_val = (ddc_response_bytes[7] << 8) + ddc_response_bytes[8];
         int cur_val = (ddc_response_bytes[9] << 8) + ddc_response_bytes[10];
         printf("(%s) cur_val = %d, max_val = %d   \n", __func__, cur_val, max_val );
      }
      else if (ddc_response_bytes[4] == 0x01) {    // unsupported VCP code
         printf("(%s) Unspported VCP code: 0x%02x\n", __func__ , vcp_feature_code);
      }
      else {
         printf("(%s) Unexpected value in supported VCP code field: 0x%02x  \n", __func__, ddc_response_bytes[4] );
         response_ok = false;
      }
   }

   rc = 0;
   if (!response_ok) {
      // printf("(%s) Unexpected Get VCP response: %s   \n", __func__, hexstring(ddc_response_bytes+1, sizeof(ddc_response_bytes)-1) );;
      rc = -1;
   }
   close(fh);

   return rc;
}



void demo_p2411_problem(int busno) {
   int tryct = 10;
   unsigned char vcp_codes[] = {0x10,         // Luminosity
                                0x12,         // Contrast
                                0x15};        // invalid
   int try_ndx, code_ndx = 0;
   for (code_ndx=0; code_ndx < sizeof(vcp_codes); code_ndx++){
      for (try_ndx=0;try_ndx < tryct; try_ndx++) {
         single_getvcp_call(busno, vcp_codes[code_ndx]);
      }
   }
}



void probe_get_luminosity(int busno, char * write_mode, char * read_mode) {
   printf("\nReading luminosity for bus %d, write_mode=%s, read_mode=%s\n",
                     busno, write_mode, read_mode);
   int  rc;
   // int  request_packet_size;
   int  file;
   Byte luminosity_op_code = 0x10;

   if (!i2c_verify_functions_supported(busno, write_mode, read_mode))
      return;

   DDC_Packet * request_packet_ptr = NULL;
   DDC_Packet * response_packet_ptr = NULL;
   request_packet_ptr = create_ddc_getvcp_request_packet(luminosity_op_code, "probe_get_luminosity");
   // printf("(%s) create_ddc_getvcp_request_packet returned rc=%d, packet_ptr=%p\n", __func__, rc, request_packet_ptr);
   // dump_packet(request_packet_ptr);

   file = i2c_open_bus(busno, EXIT_IF_FAILURE);
   i2c_set_addr(file, 0x37);
   // usleep(DEFAULT_TIMEOUT);
   sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, NULL);

   printf("(%s) calling perform_write()\n", __func__);
   set_i2c_write_mode(write_mode);
   rc = perform_i2c_write2(
         file,
         get_packet_len(request_packet_ptr)-1,
         get_packet_start(request_packet_ptr)+1,
         DDC_TIMEOUT_USE_DEFAULT);
   // rc = perform_i2c_write(file, write_mode, get_packet_len(request_packet_ptr)-1, get_packet_start(request_packet_ptr)+1);
   // TODO free the request packet
   if (rc >= 0) {
      Byte * readbuf = (Byte *)calloc(sizeof(unsigned char),256);
      // Byte cmd_byte = 0x6e;
      set_i2c_read_mode(read_mode);
      rc = perform_i2c_read2(file, 20, readbuf, DDC_TIMEOUT_USE_DEFAULT);
      // rc = perform_i2c_read(file, read_mode, 20, readbuf);
      if (rc >= 0) {
         hex_dump(readbuf, rc);
         printf("(%s) wolf 5\n", __func__);
         int rc2 = create_ddc_getvcp_response_packet(
                      readbuf, 20, luminosity_op_code, "probe_get_luminosity result", &response_packet_ptr);
         printf("(%s) create_ddc_getvcp_response_packet() returned %d\n", __func__, rc2);
         if (rc2 == 0) {
            Interpreted_Nontable_Vcp_Response * interpretation_ptr = NULL;
            rc2 = get_interpreted_vcp_code(response_packet_ptr, false, &interpretation_ptr);
            if (rc2 == 0)
               report_interpreted_nontable_vcp_response(interpretation_ptr);
         }
      } // read_ok
   } // write_ok
   close(file);
}



void get_luminosity_sample_code(int busno) {
   printf("(%s) Starting   \n", __func__ );
   char * writefunc = "write";
   //     writefunc = "i2c_smbus_write_i2c_block_data";
   char * readfunc  = "read";
   //     readfunc  = "i2c_smbus_read_i2c_block_data";
   DDC_Packet * response_packet_ptr = NULL;
   // Byte luminosity_op_code = 0x10;
   int rc;
   char * devname = malloc(12);
   snprintf(devname, 11, "/dev/i2c-%d", busno);

   int fh = open(devname,   O_NONBLOCK|O_RDWR);
   ioctl(fh, I2C_SLAVE, 0x37);

   // try a read:
   unsigned char * readbuf = calloc(sizeof(unsigned char), 256);
   rc = read(fh, readbuf+1, 11);
   if (rc < 0) {
      printf("(%s) Initial read() returned %d, errno=%s. Terminating execution\n",
             __func__, rc, linux_errno_desc(errno) );
      exit(1);
   }
   printf("(%s) Initial read succeeded\n", __func__);

   unsigned char zeroBytes[4] = {0};  // 0x00;
   rc = write(fh, &zeroBytes[0], 1);     // succeeds if <= 2 bytes, fails if >= 3
   if (rc < 0) {
      printf("(%s) Bus reset failed. rc=%d, errno=%s. Terminating execution.\n",
             __func__, rc, linux_errno_desc(errno) );
      exit(1);
   }
   printf("(%s) Initial write succeeded\n", __func__);

   unsigned char ddc_cmd_bytes[] = {
      0x6e,              // address 0x37, shifted left 1 bit
      0x51,              // source address
      0x02 | 0x80,       // number of DDC data bytes, with high bit set
      0x01,              // DDC Get Feature Command
      0x10,              // Luminosity feature code
      0x00,              // checksum, to be set
   };
   ddc_cmd_bytes[5] = ddc_checksum(ddc_cmd_bytes, 5, false);    // calculate DDC checksum on all bytes
   assert(ddc_cmd_bytes[5] == 0xac);

   //   rc = 0;

   if (streq(writefunc,"write"))
      rc = write(fh, ddc_cmd_bytes+1, sizeof(ddc_cmd_bytes)-1);
   else
#ifdef WONT_COMPILE_ON_FEDORA
      rc = i2c_smbus_write_i2c_block_data(fh, ddc_cmd_bytes[1], sizeof(ddc_cmd_bytes)-2, ddc_cmd_bytes+2);
#endif
      rc = -1;

   if (rc < 0) {
      printf("(%s) Error %s(), returned %d, errno=%s. Terminating execution.\n",
            __func__, writefunc, rc, linux_errno_desc(errno));
      exit(1);
   }
   printf("(%s) %s() returned %d   \n", __func__,  writefunc, rc );
   usleep(500000);

#ifdef WORKS
   if (rc >= 0) {
        Byte * readbuf = (Byte *)calloc(sizeof(unsigned char),256);
        // Byte cmd_byte = 0x6e;
        printf("(%s) callling call_read   \n", __func__ );
        rc = call_read(fh, readbuf, 32, true);
        if (rc < 0) {
           printf("(%s) call_read returned %d, errno=%d. Terminating execution  \n", __func__, rc, errno );
           exit(1);
        }
        printf("(%s) call_read() returned %d   \n", __func__, rc );
        if (rc >= 0) {
           hex_dump(readbuf, rc);

           int rc2 = create_ddc_getvcp_response_packet(
                           readbuf, 32, luminosity_op_code, "get_vcp:response packet", &response_packet_ptr);
           printf("(%s) create_ddc_getvcp_response_packet() returned %d\n", __func__, rc2);
           if (rc2 == 0) {
              Interpreted_Nontable_Vcp_Response * interpretation_ptr = NULL;
              rc2 = get_interpreted_vcp_code(response_packet_ptr, false, &interpretation_ptr);
              if (rc2 == 0) {
                 printf("(%s) interpretation_ptr=%p\n", __func__, interpretation_ptr);
                 report_interpreted_nontable_vcp_response(interpretation_ptr);
              }
              // read_ok = true;
           }
        } // read_ok
     } // write_ok
#endif

     if (rc >= 0) {

        if (streq(readfunc, "read"))
           rc = read(fh, readbuf+1, 11);
        else {
#ifdef OLD
           unsigned char cmd_byte = 0x00;   // apparently ignored, can be anything
           rc = i2c_smbus_read_i2c_block_data(fh, cmd_byte, 11, readbuf+1);
#endif
           rc = -1;
        }

        if (rc < 0) {
           printf("(%s) %s() returned %d, errno=%s. Terminating execution\n",
                  __func__, readfunc, rc, linux_errno_desc(errno) );
           exit(1);
        }
        printf("(%s) %s() returned %d\n", __func__, readfunc, rc);

        hex_dump(readbuf,1+rc);
        assert(readbuf[1] == 0x6e);
        int ddc_data_length = readbuf[2] & 0x7f;
        assert(ddc_data_length == 8);
        assert(readbuf[3] == 0x02);       // get feature response


        readbuf[0] = 0x50;   // for calculating DDC checksum
        unsigned char calculated_checksum = ddc_checksum(readbuf, 11, false);
        if (readbuf[11] != calculated_checksum) {
           printf("(%s) Unexpected checksum.  actual=0x%02x, calculated=0x%02x  \n",
                  __func__, readbuf[11], calculated_checksum );
        }

        int max_val = (readbuf[7] << 8) + readbuf[8];
        int cur_val = (readbuf[9] << 8) + readbuf[10];

        printf("(%s) cur_val = %d, max_val = %d   \n", __func__, cur_val, max_val );
     }
     close(fh);

     if (response_packet_ptr)
        free_ddc_packet(response_packet_ptr);
}



void get_luminosity_using_single_ioctl(int busno) {
   printf("(%s) Starting   \n", __func__ );
   bool debug = true;

   // Byte luminosity_op_code = 0x10;
   int rc;
   int errsv;
   char * devname = malloc(12);
   snprintf(devname, 11, "/dev/i2c-%d", busno);

   int fh = open(devname, O_RDWR);
   ioctl(fh, I2C_SLAVE, 0x37);


   unsigned char readbuf[256];

   unsigned char zeroByte = 0x00;

   unsigned char ddc_cmd_bytes[] = {
      0x6e,              // address 0x37, shifted left 1 bit
      0x51,              // source address
      0x02 | 0x80,       // number of DDC data bytes, with high bit set
      0x01,              // DDC Get Feature Command
      0x10,              // Luminosity feature code
      0x00,              // checksum, to be set
   };
   ddc_cmd_bytes[5] = ddc_checksum(ddc_cmd_bytes, 5, false);    // calculate DDC checksum on all bytes
   assert(ddc_cmd_bytes[5] == 0xac);

 #ifdef FOR_REFERENCE
    /*
     * I2C Message - used for pure i2c transaction, also from /dev interface
     */
    struct i2c_msg {
       __u16 addr; /* slave address        */
       unsigned short flags;
    #define I2C_M_TEN 0x10  /* we have a ten bit chip address   */
    #define I2C_M_RD  0x01
    #define I2C_M_NOSTART   0x4000
    #define I2C_M_REV_DIR_ADDR 0x2000
    #define I2C_M_IGNORE_NAK   0x1000
    #define I2C_M_NO_RD_ACK    0x0800
       short len;     /* msg length           */
       char *buf;     /* pointer to msg data        */
    };


 #endif

   //   rc = 0;

   // NB no usleeps() between write and read - should not work

   struct i2c_msg              messages[3];
   struct i2c_rdwr_ioctl_data  msgset;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-sign"

   messages[0].addr  = 0x37;
   messages[0].flags = 0;
   messages[0].len   = 1;
   messages[0].buf   = (char *) &zeroByte;

   messages[1].addr  = 0x37;
   messages[1].flags = 0;
   messages[1].len   = sizeof(ddc_cmd_bytes)-1;
   messages[1].buf   = (char *) ddc_cmd_bytes+1;

   messages[2].addr  = 0x37;
   messages[2].flags = I2C_M_RD;
   messages[2].len   = 12;
   messages[2].buf   = (char *) readbuf+1;
#pragma GCC diagnostic pop

   msgset.msgs  = messages;
   msgset.nmsgs = 3;

   rc = ioctl(fh, I2C_RDWR, &msgset);
   errsv = errno;
   if (debug)
      printf("(%s) ioctl() returned %d, errno=%s\n", __func__, rc, linux_errno_desc(errsv) );
   if (rc >= 0) {
      hex_dump(readbuf,12);
      assert(readbuf[1] == 0x6e);
      int ddc_data_length = readbuf[2] & 0x7f;
      assert(ddc_data_length == 8);
      assert(readbuf[3] == 0x02);       // get feature response

      readbuf[0] = 0x50;   // for calculating DDC checksum
      unsigned char calculated_checksum = ddc_checksum(readbuf, 11, false);
      if (readbuf[11] != calculated_checksum) {
         printf("(%s) Unexpected checksum.  actual=0x%02x, calculated=0x%02x  \n",
                __func__, readbuf[11], calculated_checksum );
      }

      int max_val = (readbuf[7] << 8) + readbuf[8];
      int cur_val = (readbuf[9] << 8) + readbuf[10];

      printf("(%s) cur_val = %d, max_val = %d   \n", __func__, cur_val, max_val );

   }

   close(fh);
}



void demo_nvidia_bug_sample_code(int busno) {
   printf("\n(%s) Starting   \n", __func__ );
   char * writefunc = "write";
   //     writefunc = "i2c_smbus_write_i2c_block_data";
   // char * readfunc  = "read";
   //     readfunc  = "i2c_smbus_read_i2c_block_data";

   int rc;

   char devname[12];
   snprintf(devname, 11, "/dev/i2c-%d", busno);

   int fh = open(devname,   O_NONBLOCK|O_RDWR);
   ioctl(fh, I2C_SLAVE, 0x37);

   // try a read, it succeeds
   unsigned char * readbuf = calloc(sizeof(unsigned char), 256);
   rc = read(fh, readbuf+1, 1);
   if (rc < 0) {
      printf("(%s) read() returned %d, errno=%s. Terminating execution  \n",
             __func__, rc, linux_errno_desc(errno) );
      exit(1);
   }
   printf("(%s) read succeeded.  Address 0x37 active on %s\n", __func__, devname);

   unsigned char zeroBytes[5] = {0};  // 0x00;

   unsigned char ddc_cmd_bytes[] = {
      0x6e,              // address 0x37, shifted left 1 bit
      0x51,              // source address
      0x02 | 0x80,       // number of DDC data bytes, with high bit set
      0x01,              // DDC Get Feature Command
      0x10,              // Luminosity feature code
      0x00,              // checksum, to be set
   };
   ddc_cmd_bytes[5] = ddc_checksum(ddc_cmd_bytes, 5, false);    // calculate DDC checksum on all bytes
   assert(ddc_cmd_bytes[5] == 0xac);

   printf("\n(%s) Try writing fragments of DDC request string...\n", __func__ );
   int bytect;
   for (bytect=sizeof(ddc_cmd_bytes)-1; bytect > 0; bytect--) {
      usleep(500000);
      errno = 0;
      rc = write(fh, ddc_cmd_bytes+1, bytect);
      if (rc == bytect)
         printf("(%s) bytect=%d, %s() returned rc=%d as expected\n", __func__, bytect, writefunc, rc);
      else if (rc < 0)
         printf("(%s) bytect=%d, Error. %s(), returned %d, errno=%s\n",
                __func__, bytect, writefunc, rc, linux_errno_desc(errno));
      else
         printf("(%s) bytect=%d, Truly weird. rc=%d\n", __func__, bytect, rc);
   }

   printf("\n(%s) Try writing null bytes...\n", __func__ );
   for (bytect=sizeof(zeroBytes); bytect > 0; bytect--) {
      usleep(500000);
      errno = 0;
      rc = write(fh, zeroBytes, bytect);
      if (rc == bytect)
         printf("(%s) bytect=%d, %s() returned rc=%d as expected\n",
                __func__, bytect, writefunc, rc);
      else if (rc < 0)
         printf("(%s) bytect=%d, Error. %s(), returned %d, errno=%s\n",
                __func__, bytect, writefunc, rc, linux_errno_desc(errno));
      else
         printf("(%s) bytect=%d, Truly weird. rc=%d\n", __func__, bytect, rc);
   }
   close(fh);

}


void test_get_luminosity_for_bus(int busno) {
   printf("\n========== Probing get luminosity =============\n");
   //                                                                                          // banner     blackrock
   probe_get_luminosity(busno, "write",                           "read");                           // bad data   ok
// probe_get_luminosity(busno, "write",                           "i2c_smbus_read_byte");
// probe_get_luminosity(busno, "write",                           "i2c_smbus_read_byte_data");
// probe_get_luminosity(busno, "write",                           "i2c_smbus_read_block_data");
   probe_get_luminosity(busno, "write",                           "i2c_smbus_read_i2c_block_data");  // EINVAL
// probe_get_luminosity(busno, "i2c_smbus_write_byte",            "read");
// probe_get_luminosity(busno, "i2c_smbus_write_byte",            "i2c_smbus_read_i2c_block_data");
// probe_get_luminosity(busno, "i2c_smbus_write_byte_data",       "read");
// probe_get_luminosity(busno, "i2c_smbus_write_byte_data",       "i2c_smbus_read_i2c_block_data");
   probe_get_luminosity(busno, "i2c_smbus_write_i2c_block_data",  "read");                           // EINVAL     ok
   probe_get_luminosity(busno, "i2c_smbus_write_i2c_block_data",  "i2c_smbus_read_i2c_block_data");  // EINVAL
   probe_get_luminosity(busno, "ioctl_write",                     "read");
   probe_get_luminosity(busno, "ioctl_write",                     "ioctl_read");
   probe_get_luminosity(busno, "write",                           "ioctl_read");
}

