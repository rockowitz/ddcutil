/** \file i2c_execute.c
 *
 * Basic functions for writing to and reading from the I2C bus using
 * alternative mechanisms.
 */
// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
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
 * @param  fd             Linux file descriptor
 * @param  slave_address  I2C slave address being written to (unused)
 * @param  bytect         number of bytes to write
 * @param  pbytes         pointer to bytes to write
 *
 * @retval 0                 success
 * @retval DDCRC_DDC_DATA   incorrect number of bytes read
 * @retval DDCRC_BAD_BYTECT incorrect number of bytes read (deprecated)
 * @retval -errno            negative Linux error number
 *
 * @remark
 * Parameter **slave_address** is present to satisfy the signature of typedef I2C_Writer.
 * The address has already been by #set_slave_address().
 */
Status_Errno_DDC
i2c_fileio_writer(int fd, Byte slave_address, int bytect, Byte * pbytes) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. fh=%d, filename=%s, slave_address=0x%02x, bytect=%d, pbytes=%p -> %s",
                 fd, filename_for_fd_t(fd), slave_address, bytect, pbytes, hexstring_t(pbytes, bytect));
   int rc = 0;

   // #ifdef USE_POLL
   if (write_with_timeout) {
      struct pollfd pfds[1];
      pfds[0].fd = fd;
      pfds[0].events = POLLOUT;

      int pollrc;
      int timeout_msec = 100;
      RECORD_IO_EVENTX(
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

   rc = write(fd, pbytes, bytect);
   // per write() man page:
   // if >= 0, number of bytes actually written, must be <= bytect
   // if -1,   error occurred, errno is set
   if (rc >= 0) {
      if (rc == bytect)
         rc = 0;
      else
         rc = DDCRC_DDC_DATA;    // was  DDCRC_BAD_BYTECT
   }
   else  {       // rc < 0
      int errsv = errno;
      DBGMSF(debug, "write() returned %d, errno=%s", rc, linux_errno_desc(errsv));
      rc = -errsv;
   }

bye:
   DBGTRC(debug, TRACE_GROUP, "Done. Returning: %s", psc_desc(rc));
   return rc;
}


/** Reads from I2C bus using read()
 *
 * @param  fd            Linux file descriptor
 * @param  slave_address I2C slave address being read from (unused)
 * @param  read_bytewise if true, use single byte reads
 * @param  bytect        number of bytes to read
 * @param  readbuf       read bytes into this buffer
 *
 * @retval 0                success
 * @retval DDCRC_DDC_DATA   incorrect number of bytes read
 * @retval -errno           negative Linux errno value from read()
 *
 * @remark
 * Parameter **slave_address** is present to satisfy the signature of typedef I2C_Writer.
 * The address has already been by #set_slave_address().
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
   DBGTRC(debug, TRACE_GROUP, "Starting. fd=%d, fn=%s, bytect=%d, slave_address=0x%02x, single_byte_reads=%s",
                 fd, filename_for_fd_t(fd),
                 bytect, slave_address, sbool(single_byte_reads));

   int rc = 0;
   if (single_byte_reads) {
      // for Acer and P2411h, reads bytes 1,3,5,7 ..
      for (int ndx=0; ndx < bytect && rc == 0; ndx++) {
         // DBGMSF(debug, "Calling read() for 1 byte, ndx=%d", ndx);
         RECORD_IO_EVENTX(
            fd,
            IE_READ,
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
         RECORD_IO_EVENTX(
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

      RECORD_IO_EVENTX(
         fd,
         IE_READ,
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
   DBGTRC(debug, TRACE_GROUP, "Returning: %s, readbuf: %s", psc_desc(rc), hexstring_t(readbuf, bytect));
   return rc;
}


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


/** Writes to I2C bus using ioctl(I2C_RDWR)
 *
 * @param  fd             Linux file descriptor
 * @param  slave_address
 * @param  bytect         number of bytes to write
 * @param  pbytes         pointer to bytes to write
 *
 * @retval 0       success
 * @retval <0      negative Linux errno value
 */
Status_Errno_DDC
i2c_ioctl_writer(
      int    fd,
      Byte   slave_address,
      int    bytect,
      Byte * pbytes)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. fh=%d, filename=%s, slave_address=0x%02x, bytect=%d, pbytes=%p -> %s",
                 fd, filename_for_fd_t(fd), slave_address, bytect, pbytes, hexstring_t(pbytes, bytect));

#ifdef EXPLORING
   int rc2 = ioctl(fd, I2C_SLAVE, 0x38);
   if (rc2 < 0) {
      int errsv = errno;
      DBGMSG("ioctl(I2C_SLAVE) returned errno %s",  linux_errno_desc(errsv) );
   }
#endif

   struct i2c_msg              messages[1];
   struct i2c_rdwr_ioctl_data  msgset;

   messages[0].addr  = slave_address;    // was 0x37;
   messages[0].flags = 0;
   messages[0].len   = bytect;
   // On Ubuntu and SuSE?, i2c_msg is defined in i2c-dev.h, with char *buf
   // On Fedora, i2c_msg is defined in i2c.h, and it's __u8 * buf
// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wpointer-sign"
   messages[0].buf   =  pbytes;
// #pragma GCC diagnostic pop

   msgset.msgs  = messages;
   msgset.nmsgs = 1;

   // ioctl works, but valgrind complains about uninitialized parm
   // DBGMSG("messages=%p, messages[0]=%p, messages[0].buf=%p", messages, &messages[0], messages[0].buf);
   // char * s = hexstring_t((unsigned char*)messages[0].buf, messages[0].len);
   // DBGMSG("messages[0].addr = 0x%04x, messages[0].flags=0x%04x, messages[0].len=%d, messages[0].buf -> %s",
   //         messages[0].addr,          messages[0].flags,        messages[0].len,    s);
   // DBGMSG("msgset=%p, msgset.nmsgs=%d, msgset.msgs[0]=%p",
   //        &msgset, msgset.nmsgs, msgset.msgs[0]);

   // per ioctl() man page:
   // if success:
   //    normally:  0
   //    occasionally >0 is output parm
   // if error:
   //    -1, errno is set
   // 11/15: as seen: always returns 1 for success
   int rc = 0;
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

   if (rc > 0) {
      // what should a positive value be equal to?  not bytect
      if (rc != 1)
         DBGMSG("ioctl() write returned %d", rc);
      rc = 0;
   }
   else if (rc < 0) {
      // rc = modulate_rc(-errno, RR_ERRNO);
      rc = -errsv;
   }

   DBGTRC(debug, TRACE_GROUP, "Returning %d", rc);
   return rc;
}


/** Reads from I2C bus using ioctl(I2C_RDWR)
 *
 * @param  fd        Linux file descriptor
 * @param  bytect    number of bytes to read
 * @param  readbuf   read bytes into this buffer
 *
 * @retval 0         success
 * @retval <0        negative Linux errno value
 */

// FAILING

// static  // disable to allow name in back trace
Status_Errno_DDC
ioctl_reader1(
      int    fd,
      Byte   slave_address,
      int    bytect,
      Byte * readbuf) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. fd=%d, fn=%s, slave_address=0x%02x, bytect=%d, readbuf=%p",
                 fd, filename_for_fd_t(fd), slave_address, bytect, readbuf);

   struct i2c_msg              messages[1];
   struct i2c_rdwr_ioctl_data  msgset;

   messages[0].addr  = slave_address;      // this is the slave address currently set
   messages[0].flags = I2C_M_RD;
   messages[0].len   = bytect;
   // On Ubuntu and SuSE?, i2c_msg is defined in i2c-dev.h, with char *buf
   // On Fedora, i2c_msg is defined in i2c.h, and it's __u8 * buf
// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wpointer-sign"
   messages[0].buf   = readbuf;
// #pragma GCC diagnostic pop

   msgset.msgs  = messages;
   msgset.nmsgs = 1;

   // DBGMSG("B msgset=%p, msgset.nmsgs=%d, msgset.msgs[0]=%p",
   //         &msgset, msgset.nmsgs, msgset.msgs[0]);
   // DBGMSG("C messages=%p, messages[0]=%p, messages[0].buf=%p", messages, &messages[0], messages[0].buf);
   // DBGMSG("D messages[0].addr = 0x%04x, messages[0].flags=0x%04x, messages[0].len=%d, messages[0].buf = %p",
   //         messages[0].addr,          messages[0].flags,          messages[0].len,      messages[0].buf );

   // per ioctl() man page:
   // if success:
   //    normally:  0
   //    occasionally >0 is output parm
   // if error:
   //    -1, errno is set
   int rc = 0; // ioctl(fd, I2C_RDWR, &msgset);
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
   if (rc > 0) {
      // always see rc == 1
      if (rc != 1) {
         DBGMSG("ioctl rc = %d, bytect =%d", rc, bytect);
      }
      rc = 0;
   }
   else if (rc < 0)
      rc = -errsv;
   // DBGMSF("Done. Returning: %s", ddcrc_desc_t(rc));
   DBGTRC(debug, TRACE_GROUP, "Returning: %s, readbuf: %s", psc_desc(rc), hexstring_t(readbuf, bytect));
   return rc;
}


Status_Errno_DDC i2c_ioctl_reader(int fd, Byte slave_address, bool read_bytewise, int bytect, Byte * readbuf) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. fd=%d, fn=%s, slave_address=0x%02x, bytect=%d, readbuf=%p",
                 fd, filename_for_fd_t(fd), slave_address, bytect, readbuf);
   int rc = 0;

   if (read_bytewise) {
      int ndx = 0;
      for (; ndx < bytect && rc == 0; ndx++) {
         rc = ioctl_reader1(fd, slave_address, 1, readbuf+ndx);
      }
   }
   else {
      rc = ioctl_reader1(fd, slave_address, bytect, readbuf);
   }

   DBGTRC(debug, TRACE_GROUP, "Returning: %s, readbuf: %s", psc_desc(rc), hexstring_t(readbuf, bytect));
   return rc;
}


void init_i2c_execute_func_name_table() {
   RTTI_ADD_FUNC( i2c_fileio_writer);
   RTTI_ADD_FUNC( i2c_fileio_reader);
   RTTI_ADD_FUNC( i2c_ioctl_writer);
   RTTI_ADD_FUNC( i2c_ioctl_reader);
}
