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
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/i2c_util.h"
#include "util/sysfs_i2c_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/execution_stats.h"
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


/** Global variable.  Controls whether function #i2c_set_addr() attempts retry
 *  after EBUSY error by changing ioctl op I2C_SLAVE to I2C_SLAVE_FORCE.
 */
bool i2c_forceable_slave_addr_flag = false;


Status_Errno
i2c_set_addr0(int fd, uint16_t op, int addr) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
                 "fd=%d, addr=0x%02x, filename=%s, op=%s",
                 fd, addr,
                 filename_for_fd_t(fd),
                 (op == I2C_SLAVE) ? "I2C_SLAVE" : "I2C_SLAVE_FORCE");

   // FAILSIM;
   bool force_pseudo_failure = false;

   Status_Errno result = 0;
   int ioctl_rc = 0;
   int errsv = 0;

   if (force_pseudo_failure && op == I2C_SLAVE) {
      DBGTRC_NOPREFIX(true, TRACE_GROUP, "Forcing pseudo failure");
      ioctl_rc = -1;
      errno=EBUSY;
   }
   else {
      RECORD_IO_EVENT(-1, IE_OTHER, ( ioctl_rc = ioctl(fd, op, addr) ) );
   }

   if (ioctl_rc < 0) {
      errsv = errno;
      if (errsv == EBUSY) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "ioctl(%s, I2C_SLAVE, 0x%02x) returned EBUSY",
                   filename_for_fd_t(fd), addr);
      }
      else {
         REPORT_IOCTL_ERROR( (op == I2C_SLAVE) ? "I2C_SLAVE" : "I2C_SLAVE_FORCE", errsv);
      }
      result = -errsv;
   }

   assert(result <= 0);
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, result, "");
   return result;
}


/** Sets the slave address to be used in subsequent i2c-dev write() and read()
 *  operations.
 *
 *  @param fd   file descriptor
 *  @param addr slave address
 *  @return     status code
 */
Status_Errno
i2c_set_addr(int fd, int addr) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
                 "fd=%d, addr=0x%02x, filename=%s, i2c_forceable_slave_addr_flag=%s",
                 fd, addr,
                 filename_for_fd_t(fd),
                 sbool(i2c_forceable_slave_addr_flag) );

   Status_Errno result = 0;
   uint16_t op = I2C_SLAVE;
   bool done = false;

   while (!done) {
      done = true;
      result = i2c_set_addr0(fd, op, addr);
      if (result < 0) {
         if (result == -EBUSY) {
            if (op == I2C_SLAVE && i2c_forceable_slave_addr_flag) {
               DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                      "Retrying using IOCTL op I2C_SLAVE_FORCE for %s, slave address 0x%02x",
                      filename_for_fd_t(fd), addr );
               // normally errors counted at higher level, but in this case it would be
               // lost because of retry
               COUNT_STATUS_CODE(result);
               op = I2C_SLAVE_FORCE;
               done = false;
            }
         }
      }
   }

   if (result == -EBUSY) {
      char msgbuf[80];
      g_snprintf(msgbuf, 60, "set_addr(%s,%s,0x%02x) failed, error = EBUSY",
                             filename_for_fd_t(fd),
                             (op == I2C_SLAVE) ? "I2C_SLAVE" : "I2C_SLAVE_FORCE",
                             addr);
      DBGTRC_NOPREFIX(debug || get_output_level() >= DDCA_OL_VERBOSE, TRACE_GROUP, "%s", msgbuf);
      SYSLOG2(DDCA_SYSLOG_ERROR, "%s", msgbuf);

   }
   else if (result == 0 && op == I2C_SLAVE_FORCE) {
      char msgbuf[80];
      g_snprintf(msgbuf, 80, "set_addr(%s,I2C_SLAVE_FORCE,0x%02x) succeeded on retry after EBUSY error",
            filename_for_fd_t(fd),
            addr);
      DBGTRC_NOPREFIX(debug || get_output_level() >= DDCA_OL_VERBOSE, TRACE_GROUP, "%s", msgbuf);
      SYSLOG2(DDCA_SYSLOG_ERROR, "%s", msgbuf);
   }

   assert(result <= 0);
   // if (addr == 0x37)  result = -EBUSY;    // for testing
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, result, "");
   return result;
}


static bool read_with_timeout  = false;
static bool write_with_timeout = false;

void set_i2c_fileio_use_timeout(bool yesno) {
   // DBGMSG("Setting  %s", sbool(yesno));
   read_with_timeout  = yesno;
   write_with_timeout = yesno;
}

bool get_i2c_fileio_use_timeout() {
   return read_with_timeout;
}


/** Writes to i2c bus using write()
 *
 * @param  fd              Linux file descriptor
 * @param  slave_address   I2C slave address being written to (unused)
 * @param  bytect          number of bytes to write
 * @param  pbytes          pointer to bytes to write
 *
 * @retval 0               success
 * @retval DDCRC_DDC_DATA  incorrect number of bytes read
 * @retval -errno          negative Linux error number
 */
Status_Errno_DDC
i2c_fileio_writer(
      int    fd,
      Byte   slave_address,
      int    bytect,
      Byte * pbytes) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
                   "fd=%d, filename=%s, slave_address=0x%02x, bytect=%d, pbytes=%p -> %s",
                   fd, filename_for_fd_t(fd), slave_address,
                   bytect, pbytes, hexstring_t(pbytes, bytect));
   int rc = 0;

   rc = i2c_set_addr(fd, slave_address);
   if (rc < 0)
      goto bye;

   // #ifdef USE_POLL
     if (write_with_timeout) {
        struct pollfd pfds[1];
        pfds[0].fd = fd;
        pfds[0].events = POLLOUT;

        int pollrc;
        int timeout_msec = 100;
        RECORD_IO_EVENT(
              fd,
              IE_OTHER,
              ( pollrc = poll(pfds, 1, timeout_msec) )
        );

        int errsv = errno;
        if (pollrc < 0)  { //  i.e. -1
           DBGMSG("poll() returned %d, errno=%d", pollrc, errsv);
           rc = -errsv;
           goto bye;
        }
        else if (pollrc == 0) {
           DBGMSG("poll() timed out after %d milliseconds", timeout_msec);
           rc = -ETIMEDOUT;
           goto bye;
        }
        else {
           if ( !( pfds[0].revents & POLLOUT) ) {
              DBGMSG("pfds[0].revents: 0x%04x", pfds[0].revents);
              // just continue, write() will fail and we'll return that status code
           }
        }
     }
     // #endif

     RECORD_IO_EVENT(
           fd,
           IE_FILEIO_WRITE,
           ( rc = write(fd, pbytes, bytect) )
           );
     // per write() man page:
     // if >= 0, number of bytes actually written, must be <= bytect
     // if -1,   error occurred, errno is set
     if (rc >= 0) {
        if (rc == bytect)
           rc = 0;
        else
           rc = DDCRC_DDC_DATA;    // was  DDCRC_BAD_BYTECT
     }
     else  {       // ioctl_rc < 0
        int errsv = errno;
        DBGMSF(debug, "write() returned %d, errno=%s", rc, linux_errno_desc(errsv));
        rc = -errsv;
     }

  bye:
     DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");
     return rc;
  }


/** Reads from I2C bus using read()
 *
 * @param  fd            Linux file descriptor
 * @param  slave_address I2C slave address being read from
 * @param  read_bytewise if true, use single byte reads
 * @param  bytect        number of bytes to read
 * @param  readbuf       read bytes into this buffer
 *
 * @retval 0                success
 * @retval DDCRC_DDC_DATA   incorrect number of bytes read
 * @retval -errno           negative Linux errno value from read()
 *
 * @remark
 * read_bytewise == true fails on some monitors, should generally be false
 */
Status_Errno_DDC
i2c_fileio_reader(
      int    fd,
      Byte   slave_address,
      bool   single_byte_reads,
      int    bytect,
      Byte * readbuf)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
                   "fd=%d, fn=%s, bytect=%d, slave_address=0x%02x, single_byte_reads=%s",
                   fd, filename_for_fd_t(fd),
                   bytect, slave_address, sbool(single_byte_reads));

   int rc = i2c_set_addr(fd, slave_address);
   if (rc < 0)
      goto bye;

   if (single_byte_reads) {
      // for Acer and P2411h, reads bytes 1,3,5,7 ..
      for (int ndx=0; ndx < bytect && rc == 0; ndx++) {
         // DBGMSF(debug, "Calling read() for 1 byte, ndx=%d", ndx);
         RECORD_IO_EVENT(
            fd,
            IE_FILEIO_READ,
            ( rc = read(fd, readbuf+ndx, 1) )
           );
         // DBGMSF(debug, "Byte read: readbuf[%d] = 0x%02x", ndx, readbuf[ndx]);
         // rc = read(fd, readbuf+ndx, 1);

         if (rc >= 0) {
            if (rc == 1) {
               rc = 0;
               // does not solve problem of every other byte read on some monitors
               // TUNED_SLEEP_WITH_TRACE(DDCA_IO_I2C, SE_POST_READ, "After 1 byte read");
            }
            else
               rc = DDCRC_DDC_DATA;
         }
      }
   }
   else {

// #ifdef USE_POLL
      if (read_with_timeout) {
         struct pollfd pfds[1];
         pfds[0].fd = fd;
         pfds[0].events = POLLIN;

         int pollrc;
         int timeout_msec = 100;
         RECORD_IO_EVENT(
               fd,
               IE_OTHER,
               ( pollrc = poll(pfds, 1, timeout_msec) )
         );

         int errsv = errno;
         if (pollrc < 0)  { //  i.e. -1
            DBGMSG("poll() returned %d, errno=%d", pollrc, errsv);
            rc = -errsv;
            goto bye;
         }
         else if (pollrc == 0) {
            DBGMSG("poll() timed out after %d milliseconds", timeout_msec);
            rc = -ETIMEDOUT;
            goto bye;
         }
         else {
            if ( !(pfds[0].revents & POLLIN) ) {
               DBGMSG("pfds[0].revents: 0x%04x", pfds[0].revents);
               // just continue, read() will fail and we'll return that status code
            }
         }
      }
// #endif

      RECORD_IO_EVENT(
         fd,
         IE_FILEIO_READ,
         ( rc = read(fd, readbuf, bytect) )
      );
      // per read() man page:
      // if rc >= 0, number of bytes actually read
      // if rc ==-1, error occurred, errno is set
      if (rc >= 0) {
         if (rc == bytect)
           rc = 0;
         else
            rc = DDCRC_DDC_DATA;    // was DDCRC_BAD_BYTECT
      }
   }
   if (rc < 0) {
      int errsv = errno;
      DBGMSF(debug, "read() returned %d, errno=%s", rc, linux_errno_desc(errsv));
      rc = -errsv;
   }

bye:
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "readbuf: %s", hexstring_t(readbuf, bytect));
   return rc;
}


STATIC void
dbgrpt_i2c_msg(int depth, struct i2c_msg message) {
   rpt_vstring(depth, "addr:    0x%04x", message.addr);   //      __u16 addr;
   rpt_vstring(depth, "flags:   0x%04x", message.flags);
   // #define I2C_M_RD     0x0001   /* guaranteed to be 0x0001! */
   // #define I2C_M_TEN          0x0010   /* use only if I2C_FUNC_10BIT_ADDR */
   // #define I2C_M_DMA_SAFE     0x0200   /* use only in kernel space */
   // #define I2C_M_RECV_LEN     0x0400   /* use only if I2C_FUNC_SMBUS_READ_BLOCK_DATA */
   // #define I2C_M_NO_RD_ACK    0x0800   /* use only if I2C_FUNC_PROTOCOL_MANGLING */
   // #define I2C_M_IGNORE_NAK   0x1000   /* use only if I2C_FUNC_PROTOCOL_MANGLING */
   // #define I2C_M_REV_DIR_ADDR 0x2000   /* use only if I2C_FUNC_PROTOCOL_MANGLING */
   // #define I2C_M_NOSTART      0x4000   /* use only if I2C_FUNC_NOSTART */
   // #define I2C_M_STOP         0x8000   /* use only if I2C_FUNC_PROTOCOL_MANGLING */
   rpt_vstring(depth, "len:     0x%04x (%d)", message.len, message.len);  // __u16
   // rpt_vstring(depth, "buf:     %p ->  %s", message.buf, hexstring_t(message.buf, message.len)); // __u8 *buf;
   rpt_vstring(depth, "buf:     %p", message.buf); // __u8 *buf;
}


STATIC void
dbgrpt_i2c_rdwr_ioctl_data(int depth, struct i2c_rdwr_ioctl_data * data) {
   bool debug = false;
   DBGMSF(debug, "data=%p", data);
   rpt_structure_loc("i2c_rdwr_ioctl_data", data, depth);
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_vstring(d1, "nmsgs:    %d", data->nmsgs);
   for (int ndx = 0; ndx < data->nmsgs; ndx++) {
      struct i2c_msg  cur = data->msgs[ndx];
      rpt_vstring(d1, "i2c_msg[%d]", ndx);
      // rpt_structure_loc("i2c_msg", cur, depth);
      dbgrpt_i2c_msg(d2, cur);
   }
}


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
   DBGTRC_STARTING(debug, TRACE_GROUP,
         "fh=%d, filename=%s, slave_address=0x%02x, bytect=%d, pbytes=%p -> %s",
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

   if (IS_TRACING())
      dbgrpt_i2c_rdwr_ioctl_data(1, &msgset);

   // per ioctl() man page:
   // if success:
   //    normally:  0
   //    occasionally >0 is output parm
   // if error:
   //    -1, errno is set
   // 11/15: as seen: always returns 1 for success
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
STATIC Status_Errno_DDC
i2c_ioctl_reader1(
      int    fd,
      Byte   slave_addr,
      int    bytect,
      Byte * readbuf) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fd=%d, fn=%s, slave_addr=0x%02x, bytect=%d, readbuf=%p",
                 fd, filename_for_fd_t(fd), slave_addr, bytect, readbuf);

   // If read fails, readbuf will not be set.
   // Initialize it here, otherwise valgrind complains about uninitialized variable
   // in hexstring_t()
   memset(readbuf, 0x00, bytect);

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

   if (IS_TRACING())
      dbgrpt_i2c_rdwr_ioctl_data(1, &msgset);

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
         DBGMSG("Unexpected ioctl rc = %d, bytect =%d", rc, bytect);
      }
      rc = 0;
   }
   else if (rc < 0)
      rc = -errsv;

   free(messages);
   // DBGMSG("readbuf=%p, bytect=%d", readbuf, bytect);
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
   DBGTRC_STARTING(debug, TRACE_GROUP,
         "fd=%d, fn=%s, slave_addr=0x%02x, read_bytewise=%s, bytect=%d, readbuf=%p",
         fd, filename_for_fd_t(fd), slave_addr, SBOOL(read_bytewise), bytect, readbuf);
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


void init_i2c_execute() {
   RTTI_ADD_FUNC(i2c_set_addr);
   RTTI_ADD_FUNC(i2c_set_addr0);
   RTTI_ADD_FUNC(i2c_ioctl_reader);
   RTTI_ADD_FUNC(i2c_ioctl_reader1);
   RTTI_ADD_FUNC(i2c_ioctl_writer);
   RTTI_ADD_FUNC(i2c_fileio_reader);
   RTTI_ADD_FUNC(i2c_fileio_writer);
}
