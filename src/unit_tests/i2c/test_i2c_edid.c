/** @file test_i2c_edid.c
 *
 *  Standalone unit tests for src/i2c/i2c_edid.c, restricted to the
 *  defensive behavior on an invalid file descriptor: every read path
 *  (ioctl-based and fileio-based) must fail promptly with a negative
 *  errno and must not crash, since a real EDID read requires an actual
 *  /dev/i2c device with a monitor attached.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libi2c unit test: it links the internal libi2c/libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/edid.h"

#include "base/parms.h"

#include "i2c/i2c_edid.h"
#include "i2c/i2c_strategy_dispatcher.h"
#include "i2c/i2c_execute.h"   // i2c_use_x30
#include "i2c/i2c_bus_open_close.h"
#include "base/i2c_bus_aux.h"            // i2c_device_exists
#include "base/display_lock.h"           // init_i2c_display_lock
#include "base/execution_stats.h"        // init_execution_stats
#include "util/error_info.h"
#include "util/timestamp.h"
#include "sysfs/sysfs_simple.h"          // sysfs_is_ignorable_i2c_device

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


// Runs stmt with stdout and stderr redirected to a temporary file.  The calls
// wrapped below deliberately fail -- a bad file descriptor, a nonexistent bus,
// a forced failure hook -- and the code under test reports those failures to
// the terminal, as it should.  Without this the log of a passing test run
// fills with alarming messages, and a real error has nothing to stand out
// against.  Modeled on the QUIETLY macro in the usb_util tests, extended to
// cover stderr, which is where most of these particular messages go.
// If the redirection cannot be set up, the statement still runs, noisily.
#define QUIETLY(stmt) do { \
   fflush(stdout); fflush(stderr); \
   int _saved_out = dup(fileno(stdout)); \
   int _saved_err = dup(fileno(stderr)); \
   FILE * _tmp = tmpfile(); \
   bool _redirected = (_tmp && _saved_out >= 0 && _saved_err >= 0); \
   if (_redirected) { \
      dup2(fileno(_tmp), fileno(stdout)); \
      dup2(fileno(_tmp), fileno(stderr)); \
   } \
   stmt; \
   fflush(stdout); fflush(stderr); \
   if (_redirected) { \
      dup2(_saved_out, fileno(stdout)); \
      dup2(_saved_err, fileno(stderr)); \
   } \
   if (_saved_out >= 0) close(_saved_out); \
   if (_saved_err >= 0) close(_saved_err); \
   if (_tmp) fclose(_tmp); \
} while(0)


static void test_get_raw_edid_by_fd_bad_fd(void) {
   Buffer * buf = buffer_new(EDID_BUFFER_SIZE, NULL);
   int rc;
   QUIETLY( rc = i2c_get_raw_edid_by_fd(-1, buf) );
   CK(rc < 0);
   CK_INT(buf->len, 0);
   buffer_free(buf, NULL);
}


static void test_get_parsed_edid_by_fd_bad_fd(void) {
   Parsed_Edid * edid = NULL;
   int rc;
   QUIETLY( rc = i2c_get_parsed_edid_by_fd(-1, &edid) );
   CK(rc < 0);
   CK(edid == NULL);
}


/* Finds an I2C bus that ddcutil itself would probe and that has a monitor on
 * it, and leaves it open.  Returns the bus number, or -1 if none was found, in
 * which case the hardware tests are skipped rather than failed -- these tests
 * must pass on a machine with no monitor, and on one where /dev/i2c is not
 * readable by the user running them.
 *
 * Bus selection uses sysfs_is_ignorable_i2c_device(), the same guard ddcutil
 * uses, so the SMBus and AMDGPU SMU buses are never opened.  Probing those is
 * what hangs some cards.
 */
static int find_bus_with_monitor(int * fd_loc, Byte * edid_out) {
   *fd_loc = -1;
   for (int busno = 0; busno < 32; busno++) {
      if (!i2c_device_exists(busno))
         continue;
      if (sysfs_is_ignorable_i2c_device(busno))
         continue;
      int fd = -1;
      Error_Info * err = NULL;
      QUIETLY( err = i2c_open_bus(busno, CALLOPT_WAIT, &fd) );
      if (err) {
         errinfo_free(err);
         continue;
      }
      Buffer * buf = buffer_new(EDID_BUFFER_SIZE, NULL);
      Status_Errno_DDC rc;
      QUIETLY( rc = i2c_get_raw_edid_by_fd(fd, buf) );
      if (rc == 0 && buf->len >= 128) {
         memcpy(edid_out, buf->bytes, 128);
         buffer_free(buf, NULL);
         *fd_loc = fd;
         return busno;
      }
      buffer_free(buf, NULL);
      QUIETLY( i2c_close_bus(busno, fd, CALLOPT_NONE) );
   }
   return -1;
}

/** The same sweep as below, against a real bus with a monitor on it.
 *
 *  With a working file descriptor the interesting property is not the status but
 *  agreement: no combination of switches should change what is read.  Every
 *  combination that succeeds must return the same 128 bytes, and the 256 byte and
 *  dynamic read sizes must agree with the 128 byte one on the first block rather
 *  than returning an extension block -- which is the failure
 *  is_valid_raw_cea861_extension_block() exists to recover from.
 *
 *  Combinations that fail are counted and printed rather than asserted against a
 *  fixed number, which would be specific to this monitor and driver.  The test
 *  requires only that at least one combination succeeded, so that agreement is
 *  being checked against something.
 */
static void test_get_raw_edid_all_switches_real_bus(void) {
   Byte reference[128];
   int fd = -1;
   int busno = find_bus_with_monitor(&fd, reference);
   if (busno < 0) {
      printf("   no usable bus with a monitor found; hardware sweep skipped\n");
      return;
   }
   printf("   using /dev/i2c-%d\n", busno);

   I2C_IO_Strategy_Id saved_strategy   = i2c_get_io_strategy_id();
   bool saved_uses_i2c_layer           = EDID_Read_Uses_I2C_Layer;
   bool saved_use_x30                  = i2c_use_x30;
   int  saved_read_size                = EDID_Read_Size;

   I2C_IO_Strategy_Id strategies[] = {I2C_IO_STRATEGY_IOCTL, I2C_IO_STRATEGY_FILEIO};
   bool bools[] = {false, true};
   int  read_sizes[] = {0, 128, 256};

   int succeeded = 0, failed_ct = 0, disagreed = 0;
   uint64_t min_us = UINT64_MAX, max_us = 0, total_us = 0;
   char slowest[160] = "";
   for (int si = 0; si < 2; si++)
    for (int ul = 0; ul < 2; ul++)
     for (int x3 = 0; x3 < 2; x3++)
      for (int rs = 0; rs < 3; rs++) {
            i2c_set_io_strategy_by_id(strategies[si]);
            EDID_Read_Uses_I2C_Layer     = bools[ul];
            i2c_use_x30                  = bools[x3];
            EDID_Read_Size               = read_sizes[rs];

            Buffer * buf = buffer_new(EDID_BUFFER_SIZE, NULL);
            Status_Errno_DDC rc;
            uint64_t t0 = cur_realtime_nanosec();
            QUIETLY( rc = i2c_get_raw_edid_by_fd(fd, buf) );
            uint64_t elapsed_us = NANOS2MICROS(cur_realtime_nanosec() - t0);

            char settings[128];
            snprintf(settings, sizeof(settings),
                     "%-22s layer=%d x30=%d size=%-3d",
                     i2c_io_strategy_id_name(strategies[si]), bools[ul],
                     bools[x3], read_sizes[rs]);

            if (elapsed_us < min_us) min_us = elapsed_us;
            if (elapsed_us > max_us) { max_us = elapsed_us; snprintf(slowest, sizeof(slowest), "%s", settings); }
            total_us += elapsed_us;

            bool ok = (rc == 0 && buf->len >= 128);
            bool agrees = ok && (memcmp(buf->bytes, reference, 128) == 0);
            if (ok) {
               succeeded++;
               if (!agrees) disagreed++;
            }
            else
               failed_ct++;
            printf("   %s  %7lu us  %s%s\n", settings, (unsigned long) elapsed_us,
                   ok ? "ok" : "FAILED", (ok && !agrees) ? "  DISAGREES WITH REFERENCE" : "");
            if (!ok)
               printf("        rc = %d\n", rc);
            buffer_free(buf, NULL);
         }

   int n = succeeded + failed_ct;
   printf("   %d of %d combinations read the EDID, %d disagreed\n", succeeded, n, disagreed);
   printf("   elapsed per combination: min %lu us, mean %lu us, max %lu us\n",
          (unsigned long) min_us, (unsigned long) (n ? total_us/n : 0), (unsigned long) max_us);
   printf("   slowest: %s\n", slowest);
   CK(succeeded > 0);          // agreement is being checked against something
   CK_INT(disagreed, 0);       // no switch setting changes what is read

   EDID_Read_Uses_I2C_Layer     = saved_uses_i2c_layer;
   i2c_use_x30                  = saved_use_x30;
   EDID_Read_Size               = saved_read_size;
   i2c_set_io_strategy_by_id((saved_strategy == I2C_IO_STRATEGY_NOT_SET)
                                ? DEFAULT_I2C_IO_STRATEGY : saved_strategy);
   QUIETLY( i2c_close_bus(busno, fd, CALLOPT_NONE) );
}


/** i2c_get_raw_edid_by_fd() over every combination of the switches that select
 *  its path, with a file descriptor that cannot work.
 *
 *  The switches, and what each selects:
 *
 *    io strategy                    ioctl(I2C_RDWR) or read()/write()
 *    EDID_Read_Uses_I2C_Layer       i2c_get_edid_bytes_using_i2c_layer() or
 *                                   i2c_get_edid_bytes_directly_using_ioctl()
 *    i2c_use_x30                    a write to x30 selects EDID block 0 first
 *    EDID_Read_Size                 128, 256, or 0 for dynamic, which also
 *                                   changes max_tries from 2 to 4
 *
 *  That is 2*2*2*3 = 24 combinations.  What must hold for all of them is
 *  narrow but worth having: a negative status, no crash, and rawedid->len left
 *  at 0 rather than describing bytes that were never read.  A caller that got 0
 *  here would go on to parse an uninitialized buffer.
 *
 *  This is a reachability sweep, not a protocol test -- with no device there is
 *  nothing to read, so what it exercises is that every branch combination
 *  handles failure consistently.  Combinations differ in how many attempts they
 *  make and in which of the three readers they reach, and those are exactly the
 *  paths where an early return or an uninitialized value would hide.
 */
static void test_get_raw_edid_all_switch_combinations(void) {
   I2C_IO_Strategy_Id saved_strategy   = i2c_get_io_strategy_id();
   bool saved_uses_i2c_layer           = EDID_Read_Uses_I2C_Layer;
   bool saved_use_x30                  = i2c_use_x30;
   int  saved_read_size                = EDID_Read_Size;

   I2C_IO_Strategy_Id strategies[] = {I2C_IO_STRATEGY_IOCTL, I2C_IO_STRATEGY_FILEIO};
   bool bools[] = {false, true};
   int  read_sizes[] = {0, 128, 256};

   int combinations = 0;
   int nonneg = 0;
   int nonzero_len = 0;
   for (int si = 0; si < 2; si++) {
    for (int ul = 0; ul < 2; ul++) {
     for (int x3 = 0; x3 < 2; x3++) {
      for (int rs = 0; rs < 3; rs++) {
            i2c_set_io_strategy_by_id(strategies[si]);
            EDID_Read_Uses_I2C_Layer     = bools[ul];
            i2c_use_x30                  = bools[x3];
            EDID_Read_Size               = read_sizes[rs];

            Buffer * buf = buffer_new(EDID_BUFFER_SIZE, NULL);
            Status_Errno_DDC rc;
            QUIETLY( rc = i2c_get_raw_edid_by_fd(-1, buf) );
            if (rc >= 0) {
               nonneg++;
               printf("FAIL  strategy=%s i2c_layer=%d use_x30=%d read_size=%d -> %d\n",
                      i2c_io_strategy_id_name(strategies[si]), bools[ul],
                      bools[x3], read_sizes[rs], rc);
            }
            if (buf->len != 0)
               nonzero_len++;
            buffer_free(buf, NULL);
            combinations++;
      }}}}

   CK_INT(combinations, 2*2*2*3);
   CK_INT(nonneg, 0);         // every combination reported failure
   CK_INT(nonzero_len, 0);    // and left the buffer empty

   EDID_Read_Uses_I2C_Layer     = saved_uses_i2c_layer;
   i2c_use_x30                  = saved_use_x30;
   EDID_Read_Size               = saved_read_size;
   i2c_set_io_strategy_by_id((saved_strategy == I2C_IO_STRATEGY_NOT_SET)
                                ? DEFAULT_I2C_IO_STRATEGY : saved_strategy);
}




/** Provokes a read at word offset 0x80 and reports whether the EEPROM rolls over,
 *  which is the premise the three parked RECOVER_CURRENT_ADDRESS_READ* blocks in
 *  i2c_edid.c rest on.
 *
 *  Those recoveries respond to a read that began at the display's current word
 *  offset, 0x80 after a prior 128 byte read, by re-reading 256 bytes.  They then
 *  expect the address counter to wrap 0xff -> 0x00 so that the base block lands at
 *  buffer offset 128, where the caller's existing check copies it down.  Nothing has
 *  ever confirmed the wrap: there is no reproducer for the quirk, so the recovery
 *  path has never run on real hardware.
 *
 *  This does not need the quirk.  Writing 0x80 rather than 0x00 as the word offset
 *  produces the same starting point deliberately, so the premise can be tested
 *  directly.  Two questions, in order:
 *
 *    1. is the word offset write honored at all?  Set 0x80, read 128 bytes, and see
 *       whether what comes back differs from the base block.  If it does not differ,
 *       the write was ignored and question 2 cannot be answered on this display.
 *    2. does the address counter wrap?  Set 0x80, read 256 bytes, and see whether the
 *       base block appears at offset 128.  That is exactly what the recoveries do.
 *
 *  Reports rather than asserts on question 2: a display that does not wrap is not
 *  broken, it just cannot be repaired by those blocks.  The offset is restored to 0
 *  afterwards, with a 128 byte read to consume the block, so nothing later in this
 *  process or the next sees a shifted pointer.
 */
static void test_current_address_read_wrap(void) {
   Byte reference[128];
   int fd = -1;
   int busno = find_bus_with_monitor(&fd, reference);
   if (busno < 0) {
      printf("   no usable bus with a monitor found; wrap test skipped\n");
      return;
   }
   printf("   using /dev/i2c-%d\n", busno);

   Byte offset_80 = 0x80;
   Byte offset_00 = 0x00;
   Byte buf[256];
   Status_Errno_DDC rc;

   // 1. is the word offset write honored?
   memset(buf, 0, sizeof(buf));
   QUIETLY( rc = invoke_i2c_writer(fd, 0x50, 1, &offset_80) );
   CK_INT(rc, 0);
   bool offset_honored = false;
   if (rc == 0) {
      QUIETLY( rc = invoke_i2c_reader(fd, 0x50, false, 128, buf) );
      CK_INT(rc, 0);
      if (rc == 0) {
         offset_honored = (memcmp(buf, reference, 128) != 0);
         printf("   offset 0x80, 128 byte read: %s (first 8 bytes %02x %02x %02x %02x %02x %02x %02x %02x)\n",
                (offset_honored) ? "differs from base block, write honored"
                                 : "SAME as base block, write appears ignored",
                buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
      }
   }

   // 2. does the address counter wrap 0xff -> 0x00?
   memset(buf, 0, sizeof(buf));
   QUIETLY( rc = invoke_i2c_writer(fd, 0x50, 1, &offset_80) );
   if (rc == 0) {
      QUIETLY( rc = invoke_i2c_reader(fd, 0x50, false, 256, buf) );
      printf("   offset 0x80, 256 byte read: %s\n", psc_desc(rc));
      if (rc == 0) {
         bool wrapped      = (memcmp(buf+128, reference, 128) == 0);
         bool valid_at_128 = is_valid_raw_edid(buf+128, 128);
         printf("   ROLL-OVER %s: base block %sfound at offset 128"
                " (is_valid_raw_edid: %s)\n",
                (wrapped) ? "CONFIRMED" : "NOT observed",
                (wrapped) ? "" : "NOT ",
                (valid_at_128) ? "true" : "false");
         printf("   bytes 128..135: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                buf[128], buf[129], buf[130], buf[131],
                buf[132], buf[133], buf[134], buf[135]);
         if (wrapped != valid_at_128)
            printf("   NOTE: the two tests disagree, which the recovery's copy-down"
                   " would act on\n");
      }
   }

   // restore: leave the word offset at 0 and consume the block
   QUIETLY( rc = invoke_i2c_writer(fd, 0x50, 1, &offset_00) );
   if (rc == 0)
      QUIETLY( rc = invoke_i2c_reader(fd, 0x50, false, 128, buf) );
   if (rc == 0)
      CK(memcmp(buf, reference, 128) == 0);     // the display is left readable

   QUIETLY( i2c_close_bus(busno, fd, CALLOPT_NONE) );
}


int main(int argc, char ** argv) {
   setvbuf(stdout, NULL, _IONBF, 0);   // so output survives a crash

   i2c_set_io_strategy_by_id(DEFAULT_I2C_IO_STRATEGY);   // required: asserted non-NOT_SET

   test_get_raw_edid_by_fd_bad_fd();
   test_get_parsed_edid_by_fd_bad_fd();
   test_get_raw_edid_all_switch_combinations();
   // i2c_open_bus()/i2c_close_bus() use the display lock table
   init_execution_stats();
   init_i2c_display_lock();
   test_get_raw_edid_all_switches_real_bus();
   test_current_address_read_wrap();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
