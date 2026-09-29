/** @file i2c_edid.c   Read and parse EDID
 *  Implements multiple methods to read an EDID, attempting to work around
 *  various quirks.
 */

// Copyright (C) 2018-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <asm-generic/errno.h>
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <base/ddcutil_types_internal.h>
#include <base/trace_control.h>
#include <errno.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <util/coredefs_base.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
/** \endcond */

#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_types.h"

#include "util/coredefs.h"
#include "util/edid.h"
#include "util/file_util.h"
#include "util/i2c_util.h"
#include "util/report_util.h"

#include "util/string_util.h"
#include "util/utilrpt.h"
#include "base/parms.h"
#include "base/core.h"
#include "base/execution_stats.h"
#include "base/rtti.h"
#include "base/sleep.h"

#ifdef TARGET_BSD
#include "bsd/i2c-dev.h"
#else
#include "i2c/wrap_i2c-dev.h"
#endif
#include "i2c/i2c_execute.h"
#include "i2c/i2c_strategy_dispatcher.h"

#include "i2c/i2c_edid.h"

 
// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

//
// I2C Bus Inspection - EDID Retrieval
//

// Globals:
bool EDID_Read_Uses_I2C_Layer        = DEFAULT_EDID_READ_USES_I2C_LAYER;
int  EDID_Read_Size                  = DEFAULT_EDID_READ_SIZE;

// No Longer global:



static Status_Errno_DDC
i2c_get_edid_bytes_directly_using_ioctl(
   int     fd,
   Buffer* rawedid,
   int     edid_read_size)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
            "Getting EDID. File descriptor = %d, filename=%s, edid_read_size=%d",
            fd, filename_for_fd_t(fd), edid_read_size);
   assert(rawedid && rawedid->buffer_size >= EDID_BUFFER_SIZE);

   int rc = 0;

   if (i2c_use_x30)
       i2c_ioctl_write_x30(fd);

   Byte byte_to_write = 0x00;

   struct i2c_msg              messages[1];
   struct i2c_rdwr_ioctl_data  msgset;

   // The memset() calls are logically unnecessary, and code works fine without them.
   // However, without the memset() calls, valgrind complains about uninitialized bytes
   // on the ioctl() call.
   // See:  https://stackoverflow.com/questions/17859320/valgrind-error-in-ioctl-call-while-sending-an-i2c-message
   // Also: https://github.com/the-tcpdump-group/libpcap/issues/1083
   memset(messages,0, sizeof(messages));
   memset(&msgset,0,sizeof(msgset));

   messages[0].addr  = 0x50;
   messages[0].flags = 0;
   messages[0].len   = 1;
   messages[0].buf   = &byte_to_write;

   msgset.msgs  = messages;
   msgset.nmsgs = 1;

   RECORD_IO_EVENT(
         fd,
         IE_IOCTL_WRITE,
         ( rc = ioctl(fd, I2C_RDWR, &msgset) )
         );
   int errsv = errno;
   if (rc < 0) {
      if (debug) {
         REPORT_IOCTL_ERROR("I2C_RDWR", errno);
      }
   }
   // DBGMSG("ioctl(..I2C_RDWR..) returned %d", rc);

   if (rc >= 0) {
      if (rc != 1)      // expected success value
         DBGMSG("Unexpected: ioctl() write returned %d", rc);
      rc = 0;
   }
   else if (rc < 0) {
      rc = -errsv;
   }

   if (rc == 0) {
      // messages needs to be allocated, cannot be on stack:
         struct i2c_msg * messages = calloc(1, sizeof(struct i2c_msg));
      struct i2c_rdwr_ioctl_data  msgset;
      memset(&msgset,0,sizeof(msgset));  // see comment in i2c_ioctl_writer()

      messages[0].addr  = 0x50;
      messages[0].flags = I2C_M_RD;
      messages[0].len   = edid_read_size;
      messages[0].buf   = rawedid->bytes;

      msgset.msgs  = messages;
      msgset.nmsgs = 1;

      RECORD_IO_EVENT(
         fd,
         IE_IOCTL_READ,
         ( rc = ioctl(fd, I2C_RDWR, &msgset))
        );
      int errsv = errno;
      if (rc < 0) {
         if (debug) {
            REPORT_IOCTL_ERROR("I2C_RDWR", errno);
         }
      }
      // DBGMSG("ioctl(..I2C_RDWR..) returned %d", rc);
      if (rc >= 0) {
         // always see rc == 1
         if (rc != 1) {
            DBGMSG("Unexpected ioctl rc = %d, bytect =%d", rc, edid_read_size);
         }
         buffer_set_length(rawedid, edid_read_size);
         rc = 0;
      }
      else if (rc < 0)
         rc = -errsv;

      free(messages);
   }

#ifdef RECOVER_CURRENT_ADDRESS_READ
   // A CEA 861 extension block where the base block belongs means the read
   // began at word offset 0x80 rather than 0, i.e. the word offset write did not
   // take effect and this was a read at the current address.  See VESA
   // E-DDC 1.2 sections 6.1 (Read at the Current Address) and 6.3 (DDC
   // Sequential Read Operation).  The address space at 0x50 is 256 bytes, so a
   // 256 byte read starting at 0x80 wraps and returns the extension block
   // followed by the base block.
   //
   // Three parked recoveries for this quirk exist, each under its own macro so
   // that one can be enabled and evaluated without the others.  They are not
   // variations on a single switch: two of them intercept in the read function
   // itself, the third is a catch-all in the caller, and they use different
   // means.  There is no reproducer for any of them.
   //
   //   RECOVER_CURRENT_ADDRESS_READ           this block.  Self-recursion,
   //                                          repair inline.  It was the
   //                                          in-function recovery for
   //                                          i2c_get_edid_bytes_using_single_ioctl()
   //                                          until that reader was parked behind
   //                                          EDID_READ_SINGLE_IOCTL, which left
   //                                          this macro unusable on its own; it
   //                                          now serves this reader, which had
   //                                          none.
   //   RECOVER_CURRENT_ADDRESS_READ_I2C_LAYER same, in
   //                                          i2c_get_edid_bytes_using_i2c_layer().
   //   RECOVER_CURRENT_ADDRESS_READ_BY_FD     catch-all in
   //                                          i2c_get_raw_edid_by_fd(), after
   //                                          whichever read path ran.  Calls a
   //                                          helper and leans on that
   //                                          function's existing 256 byte
   //                                          repair rather than repairing
   //                                          here.  Covers the fileio path,
   //                                          which has no in-function recovery.
   //
   // Recovering here rather than leaving it to the catch-all saves a try: the
   // catch-all in i2c_get_raw_edid_by_fd() is the latest interception point, so by
   // the time control reaches it the ladder has already spent one.  The argument
   // was stronger for the former host, which ran first on every read and always
   // asked for 128 bytes; here it applies only when the caller asked for less
   // than 256, which the guard below tests anyway.
   //
   // The re-read does not depend on the word offset write working the second
   // time.  It relies only on the wrap: the address space at 0x50 is 256 bytes,
   // so a 256 byte read starting at 0x80 returns the extension block followed
   // by the base block, whether it started there by accident or on purpose.
   if (rc == 0 && edid_read_size < 256 &&
         is_valid_raw_cea861_extension_block(rawedid->bytes, rawedid->len))
   {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
            "Read returned a CEA 861 extension block, indicating a read at the"
            " current address.  Re-reading 256 bytes.");
      // Bounded to one level of recursion: the recursive call passes 256, which
      // fails the edid_read_size < 256 test above.
      rc = i2c_get_edid_bytes_directly_using_ioctl(fd, rawedid, 256);
      if (rc == 0 && is_valid_raw_edid(rawedid->bytes+128, rawedid->len-128)) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
               "Base block found at offset 128.  Copying it down.");
         memcpy(rawedid->bytes, rawedid->bytes+128, 128);
         buffer_set_length(rawedid, 128);
      }
   }
#endif

   // rc = -EINVAL;    // ***TESTING***
   if ( (debug || IS_TRACING()) && rc == 0) {
      DBGMSG("Returning buffer:");
      rpt_hex_dump(rawedid->bytes, rawedid->len, 2);
   }
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");
   return rc;
}


static Status_Errno_DDC
i2c_get_edid_bytes_directly_using_fileio(
   int     fd,
   Buffer* rawedid,
   int     edid_read_size)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
         "Getting EDID. File descriptor = %d, filename=%s, edid_read_size=%d",
                 fd, filename_for_fd_t(fd), edid_read_size);
   assert(rawedid && rawedid->buffer_size >= EDID_BUFFER_SIZE);


   int rc = i2c_set_addr(fd, 0x50);
   if (rc < 0) {
      goto bye;
   }

   Byte byte_to_write = 0x00;
   RECORD_IO_EVENT(
       fd,
       IE_FILEIO_WRITE,
       ( rc = write(fd, &byte_to_write, 1) )
      );
   if (rc < 0) {
      rc = -errno;
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "write() failed.  rc = %s", psc_name_code(rc));
   }
   else {
      rc = 0;
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "write() succeeded");
   }

   if (rc == 0) {
      RECORD_IO_EVENT(
          fd,
          IE_FILEIO_READ,
          ( rc = read(fd, rawedid->bytes, edid_read_size) )
         );
      if (rc >= 0) {
         DBGMSF(debug, "read() returned %d", rc);
         rawedid->len = rc;
         // assert(rc == 128 || rc == 256);
         rc = 0;
      }
      else {
         rc = -errno;
      }
      DBGMSF(debug, "read() returned %s", psc_desc(rc) );
   }

bye:
   if ( (debug || IS_TRACING()) && rc == 0) {
      DBGMSG("Returning buffer:");
      rpt_hex_dump(rawedid->bytes, rawedid->len, 2);
   }
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");
   return rc;
}


static Status_Errno_DDC
i2c_get_edid_bytes_using_i2c_layer(
      int     fd,
      Buffer* rawedid,
      int     edid_read_size)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fd=%d, filename=%s, rawedid=%p, edid_read_size=%d",
                 fd, filename_for_fd_t(fd), (void*)rawedid, edid_read_size);
   assert(rawedid && rawedid->buffer_size >= EDID_BUFFER_SIZE);

   if (i2c_use_x30)
      i2c_ioctl_write_x30(fd);

   int rc = 0;
   Byte byte_to_write = 0x00;
   rc = invoke_i2c_writer(fd, 0x50, 1, &byte_to_write);
   DBGMSF(debug, "invoke_i2c_writer returned %s", psc_desc(rc));
   if (rc == 0) {   // write succeeded
      rc = invoke_i2c_reader(fd, 0x50, /*read_bytewise*/ false, edid_read_size, rawedid->bytes);
      DBGMSF(debug, "invoke_i2c_reader returned %s", psc_desc(rc));
      if (rc == 0) {
         rawedid->len = edid_read_size;
      }
   }  // write succeeded

#ifdef RECOVER_CURRENT_ADDRESS_READ_I2C_LAYER
   // The recovery performed under RECOVER_CURRENT_ADDRESS_READ, now in
   // i2c_get_edid_bytes_directly_using_ioctl(), applied to this transport.
   // Deliberately a separate macro so the two can be enabled independently: the
   // two functions are susceptible for different reasons and there is no
   // reproducer for either, so evidence gathered with one enabled should not be
   // muddied by the other.
   //
   // This function is the more exposed of the two.  It issues the word offset
   // write and the read as two separate I2C transactions rather than one
   // combined transfer with a repeated START, so anything that intervenes
   // between them can leave the read starting from the display's current word
   // offset.  The write is now unconditional, EDID_Write_Before_Read having been
   // eliminated, so a read at the current address here means the write was issued
   // and did not take effect, rather than that it was skipped.
   if (rc == 0 && edid_read_size < 256 &&
         is_valid_raw_cea861_extension_block(rawedid->bytes, rawedid->len))
   {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
            "Read returned a CEA 861 extension block, indicating a read at the"
            " current address.  Re-reading 256 bytes.");
      // Bounded to one level of recursion: the recursive call passes 256, which
      // fails the edid_read_size < 256 test above.
      rc = i2c_get_edid_bytes_using_i2c_layer(fd, rawedid, 256);
      if (rc == 0 && is_valid_raw_edid(rawedid->bytes+128, rawedid->len-128)) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
               "Base block found at offset 128.  Copying it down.");
         memcpy(rawedid->bytes, rawedid->bytes+128, 128);
         buffer_set_length(rawedid, 128);
      }
   }
#endif

   if ( (debug || IS_TRACING()) && rc == 0) {
      DBGMSG("Returning buffer:");
      rpt_hex_dump(rawedid->bytes, rawedid->len, 2);
   }
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");
   return rc;
}


#ifdef RECOVER_CURRENT_ADDRESS_READ_BY_FD
/** Re-reads the EDID after a read returned an extension block where the base
 *  block was expected.
 *
 *  A read issued without a preceding word offset write begins at the display's
 *  current internal word offset, which auto increments across reads.  See VESA
 *  E-DDC standard version 1.2, sections 6.1 (Read at the Current Address) and
 *  6.3 (DDC Sequential Read Operation).  A prior 128 byte read of block 0
 *  leaves that offset at 0x80, so a read whose word offset write does not take
 *  effect starts there rather than at 0.  The address space at 0x50 is 256
 *  bytes, so the read wraps 0xff -> 0x00 and a 256 byte read returns the first
 *  extension block followed by the base block.
 *
 *  Re-reading 256 bytes makes that wrap visible.  The caller's existing check
 *  for a valid base EDID at offset 128 then copies it down.
 *
 *  Which reader performs the re-read does not matter, for the reason given in the
 *  remark below: the recovery relies on the wrap, not on the word offset write
 *  taking effect.  It formerly used i2c_get_edid_bytes_using_single_ioctl(), which
 *  is now parked behind EDID_READ_SINGLE_IOCTL and would make this recovery
 *  unbuildable on its own, so it reads through the i2c layer instead.
 *
 *  @param  fd             file descriptor for open /dev/i2c-n
 *  @param  rawedid        buffer in which to return bytes of the EDID
 *  @return status code
 *
 *  @remark
 *  Untested.  Written from the reported symptom -- an unrequested extension
 *  block at offset 0 with the base block at offset 128 -- not from a
 *  reproducer.  Note the circularity: the adapters that provoke this, most
 *  likely DP AUX to I2C emulation and MST hubs, are the ones least likely to
 *  honor the word offset on the re-read either.  What makes the re-read work
 *  anyway is that it does not depend on the offset write taking effect; it
 *  relies only on the wrap, which puts the base block at offset 128 whether
 *  the read starts at 0x80 by accident or on purpose.
 */
STATIC Status_Errno_DDC
i2c_reread_edid_after_current_address_read(int fd, Buffer * rawedid)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fd=%d, filename=%s",
                   fd, filename_for_fd_t(fd));
   assert(rawedid && rawedid->buffer_size >= EDID_BUFFER_SIZE);

   // Requesting 256 also declines the recovery inside the reader: the
   // RECOVER_CURRENT_ADDRESS_READ_I2C_LAYER block is guarded by
   // edid_read_size < 256, so there is no recursion even with both enabled.
   Status_Errno_DDC rc = i2c_get_edid_bytes_using_i2c_layer(fd, rawedid, 256);
   if (rc == 0 && rawedid->len != 256)
      rc = DDCRC_INVALID_EDID;

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");
   return rc;
}
#endif


/** Gets EDID bytes of a monitor on an open I2C device.
 *
 * @param  fd        file descriptor for open /dev/i2c-n
 * @param  rawedid   buffer in which to return bytes of the EDID
 *
 * @retval  0        success
 * @retval  <0       error
 */
Status_Errno_DDC
i2c_get_raw_edid_by_fd(int fd, Buffer * rawedid)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Getting EDID. File descriptor = %d, filename=%s",
                              fd, filename_for_fd_t(fd));
   assert(rawedid && rawedid->buffer_size >= EDID_BUFFER_SIZE);

   int max_tries = (EDID_Read_Size == 0) ?  4 : 2;
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "EDID_Read_Size=%d, max_tries=%d", EDID_Read_Size, max_tries);
   // n. prior to gcc 11, declaration cannot immediately follow label
   I2C_IO_Strategy_Id cur_strategy_id = I2C_IO_STRATEGY_NOT_SET;
   cur_strategy_id = i2c_get_io_strategy_id();
   assert(cur_strategy_id != I2C_IO_STRATEGY_NOT_SET);
retry:
   DBGMSF(debug, "Using strategy  %s", i2c_io_strategy_id_name(cur_strategy_id) );
   int rc = -1;
   int tryctr = 0;
   int consecutive_eio_ct = 0;

   while (tryctr < max_tries && rc != 0) {
      int edid_read_size = EDID_Read_Size;
      if (EDID_Read_Size == 0)
         edid_read_size = (tryctr < 2) ? 128 : 256;
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                    "Trying EDID read. tryctr=%d, max_tries=%d,"
                    " edid_read_size=%d, using %s",
                    tryctr, max_tries, edid_read_size,
                    (EDID_Read_Uses_I2C_Layer) ? "I2C layer" : "local io");


     char * called_func_name = NULL;
      if (EDID_Read_Uses_I2C_Layer) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
               "Calling i2c_get_edid_bytes_using_i2c_layer, cur_strategy_id = %s...",
                i2c_io_strategy_id_name(cur_strategy_id));
         rc = i2c_get_edid_bytes_using_i2c_layer(fd, rawedid, edid_read_size);
         called_func_name = "i2c_get_edid_bytes_using_i2c_layer";
      }
      else {   // use local functions
         if (cur_strategy_id == I2C_IO_STRATEGY_IOCTL) {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                  "Calling i2c_get_edid_bytes_directly_using_ioctl()...");
            called_func_name = "i2c_get_edid_bytes_directly_using_ioctl";
            rc = i2c_get_edid_bytes_directly_using_ioctl(
               fd,
               rawedid,
               edid_read_size);
            if (rc == -EINVAL) {
               int busno = extract_number_after_hyphen(filename_for_fd_t(fd));
               assert(busno >= 0);
               if ( is_nvidia_einval_bug(I2C_IO_STRATEGY_IOCTL, busno, rc)) {
                  cur_strategy_id = I2C_IO_STRATEGY_FILEIO;
                  goto retry;
               }
            }
            // Reports a read that needed the retry ladder.  The message formerly
            // named i2c_get_edid_bytes_using_single_ioctl() as what had failed,
            // that read having been attempted first; with it gone this is the
            // first read of the try, so there is nothing to report unless an
            // earlier try failed.  tryctr is the current try's index, reset at
            // the retry label, so it counts only failures within this strategy.
            if (rc == 0 && tryctr > 0) {
               DUAL_MSGXV(debug, DDCA_SYSLOG_WARNING, TRACE_GROUP,
                  "%s() succeeded after %d failed attempt(s).",
                  called_func_name, tryctr);
            }
         }
         else {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                  "Calling i2c_get_edid_bytes_directly_using_fileio()...");
            called_func_name = "i2c_get_edid_bytes_directly_using_fileio";
            rc = i2c_get_edid_bytes_directly_using_fileio(fd, rawedid, edid_read_size);
         }
      }  // use local functions
      tryctr++;
      // -EIO is deliberately absent from the break list below, having been
      // removed 3/4/2021: unlike ENXIO, which means no device acknowledged, a
      // single EIO can be transient bus trouble and is worth retrying.  Two in
      // a row on the same bus is another matter.  Measured on a hybrid laptop
      // after resume, each empty nvidia bus returned four consecutive EIOs at
      // roughly 41 millisec per iteration -- a single ioctl attempt plus an
      // i2c layer fallback -- while the one bus that had a display succeeded
      // on its first single ioctl read and never reached a fallback.  Across
      // that whole scan no fallback ever succeeded after a first failure: the
      // ladder exists for monitors needing 128 vs 256 bytes or another
      // transport, and none of that helps when nothing answers at 0x50.
      //
      // So retry once and stop, rather than restoring -EIO to the break list.
      // That keeps the 2021 behavior for the transient case it was removed
      // for, and halves the cost of a bus with nothing on it.
      if (rc == -EIO) {
         if (++consecutive_eio_ct >= 2) {
            DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                  "Second consecutive EIO, nothing responding at 0x50.  Not retrying.");
            break;
         }
      }
      else
         consecutive_eio_ct = 0;
      if (rc == -ENXIO || rc == -EOPNOTSUPP || rc == -ETIMEDOUT ) {    // removed -EIO 3/4/2021, moved -EBUSY 7/24/2026
         // DBGMSG("breaking");
         break;
      }
      if (rc == -EBUSY) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "EDID read failed with -EBUSY.  Will retry.");
      }
      assert(rc <= 0);
      if (rc == 0) {
         // rawedid->len = 128;
         if (IS_DBGTRC(debug, DDCA_TRC_NONE) ) {  // only show if explicitly tracing this function
            DBGMSG("%s returned:", called_func_name);
            dbgrpt_buffer(rawedid, 1);
            DBGMSG("edid checksum = %d", edid_checksum(rawedid->bytes) );
         }
         if (!is_valid_raw_edid(rawedid->bytes, rawedid->len)) {
            DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Invalid EDID");
            rc = DDCRC_INVALID_EDID;
            if (is_valid_raw_cea861_extension_block(rawedid->bytes, rawedid->len)) {
               DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                               "EDID appears to start with a CEA 861 extension block");
#ifdef RECOVER_CURRENT_ADDRESS_READ_BY_FD
               // A valid extension block where the base block belongs means the
               // read began at word offset 0x80, i.e. it was a read at the
               // current address.  Re-read 256 bytes so the wrap puts the base
               // block at offset 128, where the check just below recovers it.
               // Only worth doing if less than 256 bytes were read; at 256 the
               // base block is already present and that check handles it.
               //
               // This is the catch-all of the three parked recoveries, listed in
               // the RECOVER_CURRENT_ADDRESS_READ block in
               // i2c_get_edid_bytes_directly_using_ioctl().  It sits after
               // whichever read path ran, so unlike the two in-function
               // recoveries it also covers
               // i2c_get_edid_bytes_directly_using_fileio(), which has none.  It
               // is the latest interception point and therefore the most
               // expensive: by the time control reaches here the ladder has
               // already spent a try.
               //
               // Does not collide with the recovery inside the reader it calls:
               // the helper requests 256, so
               // RECOVER_CURRENT_ADDRESS_READ_I2C_LAYER's edid_read_size < 256
               // guard declines and the 256 bytes arrive here intact.
               if (rawedid->len < 256) {
                  Status_Errno_DDC reread_rc =
                        i2c_reread_edid_after_current_address_read(fd, rawedid);
                  DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                        "i2c_reread_edid_after_current_address_read() returned %s",
                        psc_desc(reread_rc));
               }
#endif
            }
         }
         if (rawedid->len == 256) {
            if (is_valid_raw_cea861_extension_block(rawedid->bytes+128, rawedid->len-128)) {
               DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                               "Second physical EDID block appears to be a CEA 861 extension block");
            }
            else if (is_valid_raw_edid(rawedid->bytes+128, rawedid->len-128)) {
               DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                               "Second physical EDID block read is actually the initial EDID block");
               memcpy(rawedid->bytes, rawedid->bytes+128, 128);
               buffer_set_length(rawedid, 128);
               rc = 0;
            }
         }
      }  // get bytes succeeded
      if (rc == -EBUSY) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "EDID read failed with -EBUSY.  Will retry after 100 ms.");
         LOGGABLE_SLEEP(100, SLEEP_OPT_TRACEABLE, DDCA_SYSLOG_WARNING,
               "EDID read failed with -EBUSY.  Will retry after 100 ms.");
      }
   }

   if (rc < 0)
      rawedid->len = 0;

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "tries=%d", tryctr);
   return rc;
}


/** Returns a parsed EDID record for the monitor on an I2C bus.
 *
 * @param fd      file descriptor for open /dev/i2c-n
 * @param edid_ptr_loc where to return pointer to newly allocated #Parsed_Edid,
 *                     or NULL if error
 * @return status code
 */
Status_Errno_DDC
i2c_get_parsed_edid_by_fd(int fd, Parsed_Edid ** edid_ptr_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fd=%d, filename=%s", fd, filename_for_fd_t(fd));
   Parsed_Edid * edid = NULL;
   Buffer * rawedidbuf = buffer_new(EDID_BUFFER_SIZE, NULL);

   Status_Errno_DDC rc = i2c_get_raw_edid_by_fd(fd, rawedidbuf);
   if (rc == 0) {
      edid = create_parsed_edid2(rawedidbuf->bytes, "I2C");
      if (debug) {
         if (edid)
            report_parsed_edid(edid, false /* verbose */, 0);
         else
            DBGMSG("create_parsed_edid() returned NULL");
      }
      if (!edid)
         rc = DDCRC_INVALID_EDID;
   }

   buffer_free(rawedidbuf, NULL);

   *edid_ptr_loc = edid;
   if (edid)
      DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "*edid_ptr_loc = %p -> ...%s",
                                 edid, hexstring3_t(edid->bytes+124, 4, "", 1, false));
   else
      DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");

   return rc;
}


void init_i2c_edid() {
   RTTI_ADD_FUNC(i2c_get_edid_bytes_using_i2c_layer);
   RTTI_ADD_FUNC(i2c_get_edid_bytes_directly_using_fileio);
   RTTI_ADD_FUNC(i2c_get_edid_bytes_directly_using_ioctl);
   RTTI_ADD_FUNC(i2c_get_raw_edid_by_fd);
   RTTI_ADD_FUNC(i2c_get_parsed_edid_by_fd);
}


//
// Collected comments re code variants eliminated
//


#ifdef OUT
// if EDID_Read_Bytewise tests show significantly lower performance,
// cases where it fails
// To be eliminated.
// moved from parms.h
#define DEFAULT_EDID_READ_BYTEWISE        false
// Strategy    Bytewise    read edid uses local i2c call                      read edid uses i2c layer
// FILEIO      false       ok                                                 ok
// FILEIO      true        on P2411h and Acer, reads byes 0. 2, 4 of response EDID ok, getvcp fails
// IOCTL       false       ok                                                 All ok
// IOCTL       true        on P2411h and Acer, returns corrupt data           EDID ok, getvcp fails

#endif


#ifdef EDID_READ_SINGLE_IOCTL
/* Retained for comparison testing only.  Must not be enabled in normal use.
 *
 * Why not: on some drivers the single ioctl read is very slow when there is no
 * EDID to read -- which is most of what display detection looks at, and the bus
 * flock is held throughout, so other ddcutil and libddcutil instances wait too.
 * Users reported this; it was confirmed by measurement on amdgpu (AMD Baffin,
 * kernel 7.0.0) on 2026-09-28.  On an AMDGPU DM i2c hw bus with nothing at 0x50,
 * elapsed time per i2c_get_raw_edid_by_fd() call was 22.9 ms for the default path
 * and 1337.9 ms with this switch on, a factor of 58.
 *
 * The mechanism is the amdgpu per payload timeout, which scales with payload
 * length: get_transaction_timeout_hw() in dce_i2c_hw.c.  The missing piece, and
 * the reason an earlier reading of the driver wrongly exonerated this function,
 * is that the timeout is reached at all.  amdgpu_dm_i2c_xfer() maps one payload
 * per i2c_msg and dce_i2c_submit_command_hw() breaks out of the payload loop on
 * the first failure, so the one byte word offset write submitted here as payload
 * 0 ought to fail on a silent bus and stop the transaction before the long read.
 * It does not: the measurement shows payload 1 being submitted and running its
 * timeout out, about 650 ms for a 128 byte read.  The engine evidently reports
 * the NACKed write as successful.  (650 ms is twice what the formula gives for
 * 128 bytes at speed 100, so the speed or the (len+1)<<3 term in that arithmetic
 * is off by two.  The scaling with payload length is the point, not the constant.)
 *
 * The two ioctl readers are immune for a reason that has nothing to do with the
 * driver: ddcutil itself gates the read on the write's ioctl returning success,
 * so on a silent bus the read is never submitted.  The corollary is that
 * suppressing the write exposed the two ioctl readers to the same cost for the
 * opposite reason -- no write precedes the read, so the read goes to the engine
 * alone.  Measured 1301.9 ms on the same bus, using EDID_Write_Before_Read=false
 * via --f37, both since eliminated: the write is now unconditional, which is what
 * keeps a silent bus cheap on this driver.
 *
 * One further fact from that measurement: this function's caller falls through to
 * the normal i2c layer attempt on EIO, so enabling it prepends a timeout rather
 * than replacing one.  The two costs added, to 2617 ms, when the write was also
 * suppressed.
 *
 * Not explained: of the three DM i2c hw buses on that host only hw bus 0 reaches
 * the timeout.  Hw bus 1 and the aux bus, equally silent at 0x50, fail in under a
 * millisecond in every combination.
 *
 * The other half of the comparison is settled and favorable: where a monitor is
 * present the single ioctl read costs nothing, 12.13 ms against 12.25 ms for the
 * default path on amdgpu and 11.78 against 11.81 on i915, and it is atomic
 * against the adapter lock, so nothing can intervene between the write and the
 * read the way it can in the two transaction readers.  That is why it is kept
 * rather than deleted.
 *
 * Nothing assigns to read_edid_using_single_ioctl any more.  The
 * --enable-single-ioctl-edid-read and --disable-single-ioctl-edid-read options
 * have been removed along with the OPTION_SINGLE_IOCTL plumbing that carried
 * them.  Enabling it now means editing the source.
 */
bool read_edid_using_single_ioctl    = DEFAULT_SINGLE_IOCTL_EDID_READ;
#endif

#ifdef EDID_READ_SINGLE_IOCTL
// Comments that claude code put in parms.h, more appropriate here
/** Read the EDID using a single multi-message ioctl instead of
 *  separate write and read calls. */

// Needed to read the EDID on some monitors, e.g. Dell P2725DE, which returns
// a CEA extension block unless offset write and read are a single
// combined transaction.
//
// However: Using one ioctl() carrying both the word offset write and the 128
// byte read, instead of using separate write and read ioctls, is much more
// expensive when it fails, i.e. when there's no EDID at slave address x50.
// On amdgpu the driver's timeout scales with the payload length, so the 128
// byte read spends several hundred millisec before returning EIO where a write
// only transaction fails in single digits.  Measured on an AMD Baffin, kernel
// 7.0.0: 650 ms for the read against 11 ms for the write, and 1337.9 ms against
// 22.9 ms per EDID read attempt on a silent bus, the retry ladder included.
//
// Also, the multi-message ioctl may trigger an amgdpu driver failure.
//
// Not merely a default: the single ioctl read must not be enabled outside
// comparison testing.  Users reported it, and measurement on amdgpu confirmed
// both the cost and the mechanism -- see the comment at
// read_edid_using_single_ioctl in i2c_edid.c.  The same comment records that
// suppressing the word offset write incurred the same cost on that driver, for
// the opposite reason; that is part of why the write is now unconditional.
#define DEFAULT_SINGLE_IOCTL_EDID_READ   false
#endif




#ifdef EDID_READ_SINGLE_IOCTL
/** Reads the EDID in a single I2C_RDWR ioctl.
 *
 *  Some monitors, e.g. Dell P2725DE ignore an offset write sent as a transaction
 *  of its own and return the wrong 128 byte block for the subsequent read.
 *
 * @param   fd             file descriptor for open /dev/i2c-n
 * @param   rawedid        buffer in which to return bytes of the EDID
 * @param   edid_read_size number of bytes to read
 * @return  status code
 *
 * @remark
 * From user przemech, based on function drm_do_probe_ddc_edid() in kernel source
 * file drivers/gpu/drm/drm_edid.c, which uses a single ioctl() to read the EDID.
 */
Status_Errno_DDC
i2c_get_edid_bytes_using_single_ioctl(
   int     fd,
   Buffer* rawedid,
   int     edid_read_size)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Getting EDID. File descriptor = %d, filename=%s, edid_read_size=%d",
                 fd, filename_for_fd_t(fd), edid_read_size);
   assert(rawedid && rawedid->buffer_size >= EDID_BUFFER_SIZE);

   int rc = 0;
   Byte byte_to_write = 0x00;

   // messages needs to be allocated, cannot be on stack:
   struct i2c_msg * messages = calloc(2, sizeof(struct i2c_msg));
   struct i2c_rdwr_ioctl_data  msgset;
   memset(&msgset,0,sizeof(msgset));  // see comment in i2c_get_edid_bytes_directly_using_ioctl()

   messages[0].addr  = 0x50;
   messages[0].flags = 0;
   messages[0].len   = 1;
   messages[0].buf   = &byte_to_write;

   messages[1].addr  = 0x50;
   messages[1].flags = I2C_M_RD;
   messages[1].len   = edid_read_size;
   messages[1].buf   = rawedid->bytes;

   msgset.msgs  = messages;
   msgset.nmsgs = 2;

   if (IS_DBGTRC(debug, DDCA_TRC_NONE))
      dbgrpt_i2c_rdwr_ioctl_data(1, &msgset);

   RECORD_IO_EVENT(
         fd,
         IE_IOCTL_READ,
         ( rc = ioctl(fd, I2C_RDWR, &msgset) )
         );
   int errsv = errno;
   if (rc < 0) {
      if (debug) {
         REPORT_IOCTL_ERROR("I2C_RDWR", errno);
      }
      rc = -errsv;
   }
   else {
      if (rc != 2)      // expected success value: number of messages executed
         DBGMSG("Unexpected: ioctl() returned %d", rc);
      buffer_set_length(rawedid, edid_read_size);
      rc = 0;
   }
   free(messages);


   if ( (debug || IS_TRACING()) && rc == 0) {
      DBGMSG("Returning buffer:");
      rpt_hex_dump(rawedid->bytes, rawedid->len, 2);
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");
   return rc;
}
#endif
