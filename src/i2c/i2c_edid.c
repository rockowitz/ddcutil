/** @file i2c_edid.c   Read and parse EDID
 *  Implements multiple methods to read an EDID, attempting to work around
 *  various quirks.
 */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#ifdef TEST_EDID_SMBUG
#include <i2c/smbus.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
/** \endcond */

#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_types.h"

#include "util/coredefs.h"
#include "util/file_util.h"
#include "util/i2c_util.h"
#include "util/report_util.h"
#include "util/edid.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/utilrpt.h"

#include "base/parms.h"
#include "base/core.h"
#include "base/execution_stats.h"
#include "base/rtti.h"

#ifdef TARGET_BSD
#include "bsd/i2c-dev.h"
#else
#include "i2c/wrap_i2c-dev.h"
#endif
#include "i2c/i2c_strategy_dispatcher.h"

#include "i2c/i2c_edid.h"

 
// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

//
// I2C Bus Inspection - EDID Retrieval
//

// Globals:
bool EDID_Read_Uses_I2C_Layer        = DEFAULT_EDID_READ_USES_I2C_LAYER;
bool EDID_Read_Bytewise              = DEFAULT_EDID_READ_BYTEWISE;
int  EDID_Read_Size                  = DEFAULT_EDID_READ_SIZE;
bool EDID_Write_Before_Read          = DEFAULT_EDID_WRITE_BEFORE_READ;
#ifdef TEST_EDID_SMBUS
bool EDID_Read_Uses_Smbus            = false;
#endif

static Status_Errno_DDC
i2c_get_edid_bytes_directly_using_ioctl(
   int     fd,
   Buffer* rawedid,
   int     edid_read_size,
   bool    read_bytewise)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Getting EDID. File descriptor = %d, filename=%s, edid_read_size=%d, read_bytewise=%s",
                 fd, filename_for_fd_t(fd), edid_read_size, sbool(read_bytewise));
   assert(rawedid && rawedid->buffer_size >= EDID_BUFFER_SIZE);

   bool write_before_read = EDID_Write_Before_Read;
   // write_before_read = false;
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "write_before_read = %s", sbool(write_before_read));
   int rc = 0;

   if (write_before_read) {
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
   }

   if (rc == 0) {
      read_bytewise = false;
      if (read_bytewise) { // unimplemented
         PROGRAM_LOGIC_ERROR("oops");
#ifdef FOR_REF
         int ndx = 0;
         for (; ndx < edid_read_size && rc == 0; ndx++) {
            RECORD_IO_EVENT(
                fd,
                IE_FILEIO_READ,
                ( rc = read(fd, &rawedid->bytes[ndx], 1) )
               );
            if (rc < 0) {
               rc = -errno;
               break;
            }
            assert(rc == 1);
            rc = 0;
          }
          rawedid->len = ndx;
          DBGMSF(debug, "Final single byte read returned %d, ndx=%d", rc, ndx);
#endif
      }
      else {
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
   }

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
   int     edid_read_size,
   bool    read_bytewise)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Getting EDID. File descriptor = %d, filename=%s, edid_read_size=%d, read_bytewise=%s",
                 fd, filename_for_fd_t(fd), edid_read_size, sbool(read_bytewise));
   assert(rawedid && rawedid->buffer_size >= EDID_BUFFER_SIZE);

   bool write_before_read = EDID_Write_Before_Read;
   // write_before_read = false;
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "write_before_read = %s", sbool(write_before_read));

   int rc = i2c_set_addr(fd, 0x50);
   if (rc < 0) {
      goto bye;
   }

   if (write_before_read) {
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
   }

   if (rc == 0) {
      if (read_bytewise) {
         int ndx = 0;
         for (; ndx < edid_read_size && rc == 0; ndx++) {
#ifdef TEST_EDID_SMBUS
            if (EDID_Read_Uses_Smbus) {
               // ndx 0 reads byte 1, why?
               __s32 smbdata = i2c_smbus_read_byte_data(fd, ndx);
               if (smbdata < 0) {
                  rc = -errno;
                  break;
               }
               assert((smbdata & 0xff) == smbdata);
               rawedid->bytes[ndx] = smbdata;
            }
            else {
#endif
               RECORD_IO_EVENT(
                   fd,
                   IE_FILEIO_READ,
                   ( rc = read(fd, &rawedid->bytes[ndx], 1) )
                  );
#ifdef TEST_EDID_SMBUS
            }
#endif
            RECORD_IO_EVENT(
                fd,
                IE_FILEIO_READ,
                ( rc = read(fd, &rawedid->bytes[ndx], 1) )
               );
            if (rc < 0) {
               rc = -errno;
               break;
            }
            assert(rc == 1);
            rc = 0;
          }
          rawedid->len = ndx;
          DBGMSF(debug, "Final single byte read returned %d, ndx=%d", rc, ndx);
      }
      else {
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
      int     edid_read_size,
      bool    read_bytewise)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fd=%d, filename=%s, rawedid=%p, edid_read_size=%d, read_bytewise=%s",
                 fd, filename_for_fd_t(fd), (void*)rawedid, edid_read_size, sbool(read_bytewise));
   assert(rawedid && rawedid->buffer_size >= EDID_BUFFER_SIZE);

   int rc = 0;
   bool write_before_read = EDID_Write_Before_Read;
   rc = 0;
   if (write_before_read) {
      Byte byte_to_write = 0x00;
      rc = invoke_i2c_writer(fd, 0x50, 1, &byte_to_write);
      DBGMSF(debug, "invoke_i2c_writer returned %s", psc_desc(rc));
   }
   if (rc == 0) {   // write succeeded or no write
      if (read_bytewise) {
         int ndx = 0;
         for (; ndx < edid_read_size && rc == 0; ndx++) {
            // DBGMSG("Before invoke_i2c_reader() call");
            rc = invoke_i2c_reader(fd, 0x50, false, 1, &rawedid->bytes[ndx] );
         }
         DBGMSF(debug, "Final single byte read returned %d, ndx=%d", rc, ndx);
      } // read_bytewise == true
      else {
         rc = invoke_i2c_reader(fd, 0x50, read_bytewise, edid_read_size, rawedid->bytes);
         DBGMSF(debug, "invoke_i2c_reader returned %s", psc_desc(rc));

      }
      if (rc == 0) {
         rawedid->len = edid_read_size;
      }
   }  // write succeeded

   if ( (debug || IS_TRACING()) && rc == 0) {
      DBGMSG("Returning buffer:");
      rpt_hex_dump(rawedid->bytes, rawedid->len, 2);
   }
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");
   return rc;
}


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
retry:
   cur_strategy_id = i2c_get_io_strategy_id();
   assert(cur_strategy_id != I2C_IO_STRATEGY_NOT_SET);
   DBGMSF(debug, "Using strategy  %s", i2c_io_strategy_id_name(cur_strategy_id) );
   int rc = -1;
   bool read_bytewise = EDID_Read_Bytewise;
   // DBGMSF(debug, "EDID read performed using %s,read_bytewise=%s",
   //               (EDID_Read_Uses_I2C_Layer) ? "I2C layer" : "local io", sbool(read_bytewise));
   int tryctr = 0;
#ifdef TEST_EDID_SMBUS
   if (EDID_Read_Uses_Smbus) {
      read_bytewise = true;
      cur_strategy_id = I2C_IO_STRATEGY_FILEIO;
      EDID_Read_Uses_I2C_Layer = false;
   }
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "EDID_Read_Uses_Smbus = %s", sbool(EDID_Read_Uses_Smbus));
   #endif
   while (tryctr < max_tries && rc != 0) {
      int edid_read_size = EDID_Read_Size;
      if (EDID_Read_Size == 0)

         edid_read_size = (tryctr < 2) ? 128 : 256;
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                    "Trying EDID read. tryctr=%d, max_tries=%d,"
                    " edid_read_size=%d, read_bytewise=%s, using %s",
                    tryctr, max_tries, edid_read_size, sbool(read_bytewise),
                    (EDID_Read_Uses_I2C_Layer) ? "I2C layer" : "local io");

      char * called_func_name = NULL;
      if (EDID_Read_Uses_I2C_Layer) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
               "Calling i2c_get_edid_bytes_using_i2c_layer, cur_strategy_id = %s...",
                i2c_io_strategy_id_name(cur_strategy_id));
         rc = i2c_get_edid_bytes_using_i2c_layer(fd, rawedid, edid_read_size, read_bytewise);
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
               edid_read_size,
               read_bytewise);
            if (rc == -EINVAL) {
               int busno = extract_number_after_hyphen(filename_for_fd_t(fd));
               assert(busno >= 0);
               if ( is_nvidia_einval_bug(I2C_IO_STRATEGY_IOCTL, busno, rc))
                   goto retry;
            }
         }
         else {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                  "Calling i2c_get_edid_bytes_directly_using_fileio()...");
            called_func_name = "i2c_get_edid_bytes_directly_using_fileio";
            rc = i2c_get_edid_bytes_directly_using_fileio(fd, rawedid, edid_read_size, read_bytewise);
         }
      }  // use local functions
      tryctr++;
      if (rc == -ENXIO || rc == -EOPNOTSUPP || rc == -ETIMEDOUT || rc == -EBUSY) {    // removed -EIO 3/4/2021
         // DBGMSG("breaking");
         break;
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

