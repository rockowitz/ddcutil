/** \file i2c_execute.c
 *
 * Basic functions for writing to and reading from the I2C bus using
 * alternative mechanisms.
 */
// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include "ddcutil_types.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
/** \endcond */

#include "util/coredefs.h"
#include "util/file_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/execution_stats.h"
#include "base/last_io_event.h"
#include "base/linux_errno.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/tuned_sleep.h"

#ifdef TARGET_BSD
#include "bsd/i2c.h"
#include "bsd/i2c-dev.h"
#else
#include "i2c/wrap_i2c-dev.h"
#endif

#include "i2c_execute.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;


/** Writes to I2C bus using ioctl(I2C_RDWR)
 *
 * @param  fd             Linux file descriptor
 * @param  slave_address  slave address to write to
 * @param  bytect         number of bytes to write
 * @param  pbytes         pointer to bytes to write
 *
 * @retval 0    success
 * @retval <0   negative Linux errno value
 */
Status_Errno_DDC
i2c_ioctl_writer(
      int    fd,
      Byte   slave_address,
      int    bytect,
      Byte * pbytes)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fh=%d, filename=%s, slave_address=0x%02x, bytect=%d, pbytes=%p -> %s",
                 fd, filename_for_fd_t(fd), slave_address, bytect, pbytes, hexstring_t(pbytes, bytect));

   int rc = 0;

   struct i2c_msg              messages[1];
   struct i2c_rdwr_ioctl_data  msgset;

   // The memset() calls are logically unnecessary, and code works fine without them.
   // However, without the memset() calls, valgrind complains about uninitialized bytes
   // on the ioctl() call.
   // See:  https://stackoverflow.com/questions/17859320/valgrind-error-in-ioctl-call-while-sending-an-i2c-message
   // Also: https://github.com/the-tcpdump-group/libpcap/issues/1083
   memset(messages,0, sizeof(messages));
   memset(&msgset,0,sizeof(msgset));

   messages[0].addr  = slave_address;
   messages[0].flags = 0;
   messages[0].len   = bytect;
   messages[0].buf   = pbytes;

   msgset.msgs  = messages;
   msgset.nmsgs = 1;

   // per ioctl() man page:
   // if success:
   //    normally:  0
   //    occasionally >0 is output parm
   // if error:
   //    -1, errno is set
   // 11/15: as seen: always returns 1 for success
   RECORD_IO_EVENTX(
         fd,
         IE_WRITE,
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

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");
   return rc;
}


/** Reads from I2C bus using ioctl(I2C_RDWR)
 *
 * @param  fd         Linux file descriptor
 * @param  slave_addr slave address to read from
 * @param  bytect     number of bytes to read
 * @param  readbuf    read bytes into this buffer
 *
 * @retval 0    success
 * @retval <0   negative Linux errno value
 */
// static  // disable to allow name in back trace
Status_Errno_DDC
i2c_ioctl_reader1(
      int    fd,
      Byte   slave_addr,
      int    bytect,
      Byte * readbuf) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fd=%d, fn=%s, slave_addr=0x%02x, bytect=%d, readbuf=%p",
                 fd, filename_for_fd_t(fd), slave_addr, bytect, readbuf);

   int rc = 0;
   // messages needs to be allocated, cannot be on stack:
   struct i2c_msg * messages = calloc(1, sizeof(struct i2c_msg));
   struct i2c_rdwr_ioctl_data  msgset;
   memset(&msgset,0,sizeof(msgset));  // see comment in is2_ioctl_writer()

   messages[0].addr  = slave_addr;
   messages[0].flags = I2C_M_RD;
   messages[0].len   = bytect;
   messages[0].buf   = readbuf;

   msgset.msgs  = messages;
   msgset.nmsgs = 1;

   RECORD_IO_EVENTX(
      fd,
      IE_READ,
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
         DBGMSG("Unexpected ioctl rc = %d, bytect =%d", rc, bytect);
      }
      rc = 0;
   }
   else if (rc < 0)
      rc = -errsv;

   free(messages);
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "readbuf: %s", hexstring_t(readbuf, bytect));
   return rc;
}


/** Reads from I2C bus using ioctl(I2C_RDWR)
 *
 * @param  fd            Linux file descriptor
 * @param  slave_addr    slave address to read from
 * @param  read_bytewise if true, read single byte at a time
 * @param  bytect        number of bytes to read
 * @param  readbuf       read bytes into this buffer
 *
 * @retval 0    success
 * @retval <0   negative Linux errno value
 */
Status_Errno_DDC
i2c_ioctl_reader(
      int    fd,
      Byte   slave_addr,
      bool   read_bytewise,
      int    bytect,
      Byte * readbuf)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fd=%d, fn=%s, slave_addr=0x%02x, bytect=%d, readbuf=%p",
                 fd, filename_for_fd_t(fd), slave_addr, bytect, readbuf);
   int rc = 0;

   if (read_bytewise) {
      int ndx = 0;
      for (; ndx < bytect && rc == 0; ndx++) {
         rc = i2c_ioctl_reader1(fd, slave_addr, 1, readbuf+ndx);
      }
   }
   else {
      rc = i2c_ioctl_reader1(fd, slave_addr, bytect, readbuf);
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "readbuf: %s", hexstring_t(readbuf, bytect));
   return rc;
}


void init_i2c_execute_func_name_table() {
   RTTI_ADD_FUNC( i2c_ioctl_reader);
   RTTI_ADD_FUNC( i2c_ioctl_reader1);
   RTTI_ADD_FUNC( i2c_ioctl_writer);
}
