/** @file query_sysenv_i2c.c
 *
 * Check I2C devices using directly coded I2C calls
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "util/edid.h"
#include "util/i2c_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_i2c_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/linux_errno.h"
#include "base/status_code_mgt.h"
/** \endcond */

#include "i2c/i2c_bus_core.h"

#include "query_sysenv_base.h"
#include "query_sysenv_sysfs.h"
#include "query_sysenv_xref.h"

#include "query_sysenv_i2c.h"


// Auxiliary function for raw_scan_i2c_devices()
static bool is_i2c_device_rw(int busno) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno);

   bool result = true;

   char fnbuf[PATH_MAX];
   snprintf(fnbuf, sizeof(fnbuf), "/dev/i2c-%d", busno);

   int rc;
   int errsv;
   DBGMSF(debug, "Calling access() for %s", fnbuf);
   rc = access(fnbuf, R_OK|W_OK);
   if (rc < 0) {
      errsv = errno;
      rpt_vstring(0,"Device %s is not readable and writable.  Error = %s",
             fnbuf, linux_errno_desc(errsv) );
      result = false;
   }

   DBGMSF(debug, "Returning: %s", sbool(result));
   return result;
}


// Auxiliary function for raw_scan_i2c_devices()
// adapted from ddc_vcp_tests

static Public_Status_Code
try_single_getvcp_call(
      int           fh,
      unsigned char vcp_feature_code,
      bool          use_smbus,
      int           depth)
{
   bool debug = false;
   rpt_nl();
   DBGMSF(true, "Starting. vcp_feature_code=0x%02x. use_smbus=%s", vcp_feature_code, sbool(use_smbus) );
   // rpt_vstring(depth, "Starting. vcp_feature_code=0x%02x, use_smbus=%s",
   //                     vcp_feature_code, sbool(use_smbus) );

   int ndx;
   Status_Errno rc = 0;

   // extra sleep time does not help P2411
#ifdef ALT
   // resets monitor state?
   unsigned char zeroByte = 0x00;  // 0x00;
   rc = write(fh, &zeroByte, 1);
   if (rc < 0) {
      rpt_vstring(0,"(%s) Bus reset failed. rc=%d, errno=%d. ", __func__, rc, errno );
      return -1;
   }
#endif
   // without usleep() or 0 byte write, read() sometimes returns all 0 on P2411H
   usleep(50000);

   unsigned char ddc_cmd_bytes[] = {
      0x6e,              // address 0x37, shifted left 1 bit
      0x51,              // source address
      0x02 | 0x80,       // number of DDC data bytes, with high bit set
      0x01,              // DDC Get Feature Command
      vcp_feature_code,  //
      0x00,              // checksum, to be set
   };

   // calculate checksum by XORing bytes 0..4
   ddc_cmd_bytes[5] = ddc_cmd_bytes[0];
   for (ndx=1; ndx < 5; ndx++)
      ddc_cmd_bytes[5] ^= ddc_cmd_bytes[ndx];    // calculate checksum
   int writect = sizeof(ddc_cmd_bytes)-1;
   rpt_vstring(depth, "Sending Get VCP Feature Command request packet: %s", hexstring_t(ddc_cmd_bytes+1, writect));
   rc = write(fh, ddc_cmd_bytes+1, writect);
   if (rc < 0) {
      int errsv = errno;
      DBGMSF(debug, "write() failed, errno=%s", linux_errno_desc(errsv));
      rc = -errsv;
      goto bye;
   }
   if (rc != writect) {
      DBGMSF(debug, "write() returned %d, expected %d", rc, writect );
      rc = DDCRC_DDC_DATA;    // was DDCRC_BAD_BYTECT
      goto bye;
   }
   usleep(50000);

   unsigned char ddc_response_bytes[12];
   int readct = sizeof(ddc_response_bytes)-1;

   rpt_vstring(depth, "Reading Get Feature Reply response packet");

   if (use_smbus) {   // FAILS, reads 6e 6e 6e ...
      unsigned long functionality = i2c_get_functionality_flags_by_fd(fh);
       if (!(functionality & I2C_FUNC_SMBUS_READ_BYTE)) {
          rpt_vstring(depth, "%s does not support I2C_FUNC_SMBUS_READ_BYTE", fh);
          rc = DDCRC_UNIMPLEMENTED;
       }
       else {
          int actual_ct = 0;
          rc = 0;
          int ndx = 0;
          __s32 smbus_result = 0;
          for (; ndx < readct && rc == 0; ndx++) {
             smbus_result = i2c_smbus_read_byte_data(fh, ndx);
             DBGMSF(debug, "ndx=%d, smbus_result = 0x%08x, %d", ndx, smbus_result, smbus_result);
             if (smbus_result < 0)
                rc = -errno;
             else {
                ddc_response_bytes[ndx+1] = smbus_result;
                actual_ct = ndx+1;
             }
          }
          if (rc < 0) {
             rpt_vstring(depth,"i2c_smbus_read_byte_data() failed. errno = %s",
                               linux_errno_desc(errno));
             goto bye;
          }
          rpt_vstring(depth+1, "%d bytes were read", actual_ct);
          rpt_vstring(depth, "ddc_response_bytes+1-> %s", hexstring_t(ddc_response_bytes+1,actual_ct) );
       }
   }
   else {
      rc = read(fh, ddc_response_bytes+1, readct);
      if (rc < 0) {
         rc = -errno;
         DBGMSF(debug, "read() failed, errno=%s", linux_errno_desc(-rc));
         goto bye;
      }
      rpt_vstring(depth, "read() returned %s", hexstring_t(ddc_response_bytes+1,rc) );

      if (rc != readct) {
         DBGMSF(debug, "read() returned %d, should be %d", rc, readct );
         rc = DDCRC_DDC_DATA;    // was DDCRC_BAD_BYTECT
         goto bye;
      }
   }
   if ( all_bytes_zero( ddc_response_bytes+1, readct) ) {
      DBGMSF(debug, "All bytes zero");
      rc = DDCRC_READ_ALL_ZERO;
      goto bye;
   }

   int ddc_data_length = ddc_response_bytes[2] & 0x7f;
   // some monitors return a DDC null response to indicate an invalid request:
   if (ddc_response_bytes[1] == 0x6e &&
       ddc_data_length == 0          &&
       ddc_response_bytes[3] == 0xbe)     // 0xbe == checksum
   {
      DBGMSF(debug, "Received DDC null response");
      rc = DDCRC_NULL_RESPONSE;
      goto bye;
   }

   if (ddc_response_bytes[1] != 0x6e) {
      // assert(ddc_response_bytes[1] == 0x6e);
      DBGMSF(debug, "Invalid address byte in response, expected 06e, actual 0x%02x",
                    ddc_response_bytes[1] );
      rc = DDCRC_DDC_DATA;    // was DDCRC_INVALID_DATA;
      goto bye;
   }

   if (ddc_data_length != 8) {
      DBGMSF(debug, "Invalid query VCP response length: %d", ddc_data_length );
      rc = DDCRC_DDC_DATA;    //  was DDCRC_BAD_BYTECT
      goto bye;
   }

   if (ddc_response_bytes[3] != 0x02) {       // get feature response
      DBGMSF(debug, "Expected 0x02 in feature response field, actual value 0x%02x",
                    ddc_response_bytes[3] );
      rc = DDCRC_DDC_DATA;    // was  DDCRC_INVALID_DATA;
      goto bye;
   }

   ddc_response_bytes[0] = 0x50;   // for calculating DDC checksum
   // checksum0 = xor_bytes(ddc_response_bytes, sizeof(ddc_response_bytes)-1);
   unsigned char calculated_checksum = ddc_response_bytes[0];
   for (ndx=1; ndx < 11; ndx++)
      calculated_checksum ^= ddc_response_bytes[ndx];
   // printf("(%s) checksum0=0x%02x, calculated_checksum=0x%02x\n", __func__, checksum0, calculated_checksum );
   if (ddc_response_bytes[11] != calculated_checksum) {
      DBGMSF(debug, "Unexpected checksum.  actual=0x%02x, calculated=0x%02x",
             ddc_response_bytes[11], calculated_checksum );
      rc = DDCRC_DDC_DATA;     // was DDCRC_CHECKSUM;
      goto bye;
   }

   if (ddc_response_bytes[4] == 0x00) {         // valid VCP code
      // The interpretation for most VCP codes:
      int max_val = (ddc_response_bytes[7] << 8) + ddc_response_bytes[8];
      int cur_val = (ddc_response_bytes[9] << 8) + ddc_response_bytes[10];
      DBGMSF(debug, "cur_val = %d, max_val = %d", cur_val, max_val );
      rc = 0;
   }
   else if (ddc_response_bytes[4] == 0x01) {    // unsupported VCP code
      DBGMSF(debug, "Unsupported VCP code: 0x%02x", vcp_feature_code);
      rc = DDCRC_REPORTED_UNSUPPORTED;
   }
   else {
      DBGMSF(debug, "Unexpected value in supported VCP code field: 0x%02x  ",
                    ddc_response_bytes[4] );
      rc = DDCRC_DDC_DATA;    // was DDCRC_INVALID_DATA;
   }

bye:
   DBGMSF(debug, "Returning: %s",  psc_desc(rc));
   return rc;
}


bool simple_read_edid(
      int  busno,
      int  read_size,
      bool write_before_read,
      bool use_smbus,
      int  depth)
{
   assert(read_size == 128 || read_size == 256);
   rpt_nl();
   rpt_vstring(depth, "Attempting simple %d byte EDID read of /dev/i2c-%d, %s initial write() using %s",
                  read_size, busno,
                  (write_before_read) ? "WITH" : "WITHOUT",
                  (use_smbus) ?  "i2c_smbus_read_byte_data()" : "read()");
   int rc = 0;
   char i2cdev[20];
   Byte edid_buf[256];
   snprintf(i2cdev, 20, "/dev/i2c-%d", busno);
   bool ok = false;
   int fd = open(i2cdev, O_RDWR );
   if (fd < 0) {
      rpt_vstring(depth, "Open failed for %s, errno=%s", i2cdev, linux_errno_desc(errno));
   }
   else {
      uint16_t op = I2C_SLAVE;
      rc = ioctl(fd, op, 0x50);
      int errsv = 0;
      if (rc < 0) {
         errsv = errno;
         rpt_vstring(depth, "ioctl I2C_SLAVE returned errno=%s", linux_errno_desc(errno));
         if (errsv == EBUSY) {
            rpt_label(depth, "Retrying ioctl I2C_SLAVE_FORCE...");
            errno = 0;
            op = I2C_SLAVE_FORCE;
            rc = ioctl(fd, op, 0x50);
            if (rc < 0) {
               rpt_vstring(depth, "ioctl(I2C_FORCE_SLACE) returned %s", linux_errno_desc(errno));
            }
         }
         if (rc < 0) {
            goto close;
         }
      }
      if (write_before_read) {
         edid_buf[0] = 0x00;
         rc = write(fd, edid_buf, 1);
         if (rc < 0) {
            rpt_vstring(depth, "write() of 1 byte failed, errno = %s", linux_errno_desc(errno));
            rpt_label(depth, "Continuing");
         }
      }

      int actual_ct = 0;
      if (use_smbus) {
         unsigned long functionality = i2c_get_functionality_flags_by_fd(fd);
         if (!(functionality & I2C_FUNC_SMBUS_READ_BYTE)) {
            rpt_vstring(depth, "%s does not support I2C_FUNC_SMBUS_READ_BYTE", i2cdev);
         }
         else {
            rc = 0;
            int ndx = 0;
            __s32 smbus_result = 0;
            for (; ndx < read_size && rc == 0; ndx++) {
               smbus_result = i2c_smbus_read_byte_data(fd, ndx);
               // DBGMSG("smbus_result = 0x%08x, %d", smbus_result, smbus_result);
               if (smbus_result < 0)
                  rc = -errno;
               else {
                  edid_buf[ndx] = smbus_result;
                  actual_ct = ndx+1;
               }
            }
            if (rc < 0) {
               rpt_vstring(depth,"i2c_smbus_read_byte_data() failed. errno = %s",
                                 linux_errno_desc(errno));
               goto close;
            }
            rpt_vstring(depth+1, "%d bytes were read", actual_ct);
            rpt_hex_dump(edid_buf, actual_ct, depth+1);
            ok = true;
         }
      }
      else {
         actual_ct = read(fd, edid_buf, read_size);
         if (actual_ct < 0) {
            rpt_vstring(depth,"read failed. errno = %s", linux_errno_desc(errno));
            goto close;
         }
         rpt_vstring(depth, "read() returned %d bytes", actual_ct);
         rpt_hex_dump(edid_buf, actual_ct, depth+1);
         ok = true;
      }

close:
      close(fd);
   }
   return ok;
}


/** Checks each I2C device.
 *
 * This function largely uses direct coding to probe the I2C buses.
 * Allows for trying to read x37 even if X50 fails, and provides clearer
 * diagnostic messages than relying entirely on the normal code path.
 *
 * As part of its scan, this function adds an entry to the display cross
 * reference table for each I2C device reporting an EDID.
 * It must be called before any other functions accessing the table, since
 * they will search by I2C bus number.
 *
 * \param accum accumulates sysenv query information
 */
void raw_scan_i2c_devices(Env_Accumulator * accum) {
   bool debug = false;
   DBGMSF(debug, "Starting");

   int depth = 0;
   int d1 = depth+1;
   int d2 = depth+2;
   Parsed_Edid * edid = NULL;

   rpt_nl();
   rpt_title("Performing alternative scans of I2C devices using local sysenv functions...",depth);
   sysenv_rpt_current_time(NULL, d1);

   Buffer * buf0 = buffer_new(1000, __func__);
   int  busct = 0;
   Public_Status_Code psc;
   Status_Errno rc;
   bool saved_i2c_force_slave_addr_flag = i2c_force_slave_addr_flag;

   for (int busno=0; busno < I2C_BUS_MAX; busno++) {
      if (i2c_device_exists(busno)) {
         busct++;
         rpt_nl();
         rpt_vstring(d1, "Examining device /dev/i2c-%d...", busno);

         if (sysfs_is_ignorable_i2c_device(busno)) {
            rpt_vstring(d2, "Device /dev/i2c-%d is a SMBus or other ignorable device.  Skipping.", busno);
            continue;
         }

         if (!is_i2c_device_rw(busno))   // issues message if not RW
            continue;

         rpt_label(d2, "Tests using read()...");
         bool ok = false;
         rpt_label(d2, "Without write() before read()...");
         ok = simple_read_edid(busno, 128, false, false, d2);
         if (!ok)
            simple_read_edid(busno, 128, false, false, d2);
         simple_read_edid(busno, 256, false, false, d2);
         rpt_nl();
         rpt_label(d2, "Retrying with write() before read()...");
         ok = simple_read_edid(busno, 128, true, false, d2);
         if (!ok)
            simple_read_edid(busno, 128, true, false, d2);
         simple_read_edid(busno, 256, true, false, d2);
         rpt_nl();

         rpt_label(d2, "Tests using i2c_smbus_read_byte_data()...");
         rpt_label(d2, "Without write() before read()...");
         ok = simple_read_edid(busno, 128, false, true, d2);
         if (!ok)
            simple_read_edid(busno, 128, false, true, d2);
         simple_read_edid(busno, 256, false, true, d2);
         rpt_nl();
         rpt_label(d2, "Retrying with write() before read()...");
         ok = simple_read_edid(busno, 128, true, true, d2);
         if (!ok)
            simple_read_edid(busno, 128, true, true, d2);
         simple_read_edid(busno, 256, true, true, d2);
         rpt_nl();

         rpt_label(d2, "Obtain and interpret EDID using normal i2c functions...");
         rpt_nl();
         int fd = i2c_open_bus(busno, CALLOPT_ERR_MSG);
         if (fd < 0)
            continue;

         unsigned long functionality = i2c_get_functionality_flags_by_fd(fd);
         // DBGMSG("i2c_get_functionality_flags_by_fd() returned %ul", functionality);
         i2c_report_functionality_flags(functionality, 90, d2);

         int maxtries = 3;
         for (int tryctr = 0; tryctr < maxtries; tryctr++) {
            //  Base_Status_Errno rc = i2c_set_addr(fd, 0x50, CALLOPT_ERR_MSG);
            // TODO save force slave addr setting, set it for duration of call - do it outside loop
            psc = i2c_get_raw_edid_by_fd(fd, buf0);
            if (psc != 0) {
               rpt_vstring(d2, "Unable to read EDID, psc=%s", psc_desc(psc));
               tryctr = 999;   // retries have already happened in i2c_get_raw_edid_by_fd()
            }
            else {
               rpt_label(d2, "Raw EDID:");
               rpt_hex_dump(buf0->bytes, buf0->len, d2);
               edid = create_parsed_edid(buf0->bytes);
               if (edid) {
                  report_parsed_edid_base(
                     edid,
                     true,     // verbose
                     false,    // show_edid
                     d2);
                  rpt_vstring(d2, "Attempt %d to read and parse EDID succeeded", tryctr+1);
                  Byte * edidbytes = buf0->bytes;
                  // Device_Id_Xref * xref = device_xref_get(buf0->bytes);
                  // xref->i2c_busno = busno;
                  device_xref_new_with_busno(busno, edidbytes);
                  tryctr = 999;
               }
               else {
                  rpt_vstring(d2, "Unable to parse EDID");
                  if (tryctr < maxtries)
                     rpt_label(d2, "Retrying read EDID");
               }
            }

#ifdef SYSENV_TEST_IDENTICAL_EDIDS
            if (!first_edid) {
               DBGMSG("Setting first_edid");
               first_edid = calloc(1,128);
               memcpy(first_edid, buf0->bytes, 128);
            }
            else  {
               DBGMSG("Forcing duplicate EDID");
               edidbytes = first_edid;
            }
#endif
         }

         rpt_nl();
         rpt_vstring(d2, "Trying simple VCP read of feature 0x10...");
         rc = i2c_set_addr(fd, 0x37, CALLOPT_ERR_MSG);
         if (rc == 0) {
            int maxtries = 3;
            psc = -1;
            for (int tryctr=0; tryctr<maxtries && psc < 0; tryctr++) {
               psc = try_single_getvcp_call(fd, 0x10, false, d2);
               if (psc == 0 || psc == DDCRC_NULL_RESPONSE || psc == DDCRC_REPORTED_UNSUPPORTED) {
                  switch (psc) {
                  case 0:
                     rpt_vstring(d2, "Attempt %d to read feature succeeded.", tryctr+1);
                     break;
                  case DDCRC_REPORTED_UNSUPPORTED:
                     rpt_vstring(d2, "Attempt %d to read feature returned DDCRC_REPORTED_UNSUPPORTED", tryctr+1);
                     psc = 0;
                     break;
                  case DDCRC_NULL_RESPONSE:
                     rpt_vstring(d2, "Attempt %d to read feature returned DDCRC_NULL_RESPONSE", tryctr+1);
                     break;
                  }
                  break;
               }
               if (get_modulation(psc) == RR_ERRNO) {    // also RR_ADL?
                  rpt_vstring(d2, "Attempt %d to read feature returned hard error: %s", tryctr+1, psc_desc(psc));
                  break;
               }
               rpt_vstring(d2, "Attempt %d to read feature failed. status = %s.  %s",
                             tryctr+1, psc_desc(psc), (tryctr < maxtries-1) ? "Retrying..." : "");
            }
            if (psc == 0)
               rpt_vstring(d2, "DDC communication succeeded");
            else {
               rpt_vstring(d2, "DDC communication failed.");
               // If this message remains it should be split into multiple messages depending
               // on whether this is a laptop display
               // if (edid)
               //    rpt_vstring(d2, "Is DDC/CI enabled in the monitor's on-screen display?");
            }
         }

         if (edid) {
            free_parsed_edid(edid);
            edid = NULL;
         }
         i2c_close_bus(fd, CALLOPT_ERR_MSG);
      }
   }

   if (busct == 0) {
      rpt_vstring(d2, "No /dev/i2c-* devices found");
      rpt_nl();
   }

   i2c_force_slave_addr_flag = saved_i2c_force_slave_addr_flag;
   buffer_free(buf0, __func__);

   // DBGMSG("setting i2c_bus_scan_complete");
   device_xref_set_i2c_bus_scan_complete();
   // device_xref_report(3);
   DBGMSF(debug, "Done" );
}


/** Checks each I2C device, using the normal code path
 */

void query_i2c_buses() {
   rpt_vstring(0,"Examining I2C buses, as detected by I2C layer...");
   sysenv_rpt_current_time(NULL, 1);
   i2c_report_buses(true, 1 /* indentation depth */);    // in i2c_bus_core.c
}

