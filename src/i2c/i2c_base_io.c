/*  i2c_base_io.c
 *
 *  Created on: Nov 17, 2015
 *      Author: rock
 *
 *  Basic functions for writing to and reading from the I2C bus,
 *  using alternative mechanisms.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <i2c-dev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <util/string_util.h>

#include <base/ddc_base_defs.h>
#include <base/ddc_errno.h>
#include <base/execution_stats.h>
#include <base/common.h>
#include <base/status_code_mgt.h>
#include <base/linux_errno.h>

#include <i2c/i2c_base_io.h>


//
// Basic functions for reading and writing to I2C bus.
//

/* Write to i2c bus using write()
 *
 * Arguments:
 *   fh      file handle
 *   bytect  number of bytes to write
 *   pbytes  pointer to bytes to write
 *
 * Returns:
 *   0 if success
 *   if error:
 *      -errno
 *      DDCRC_BAD_BYTECT
 */
Base_Status_Errno_DDC  write_writer(int fh, int bytect, Byte * pbytes) {
   bool debug = false;
   int rc = write(fh, pbytes, bytect);
   // per write() man page:
   // if >= 0, number of bytes actually written, must be <= bytect
   // if -1,   error occurred, errno is set
   if (rc >= 0) {
      if (rc == bytect)
         rc = 0;
      else
         rc = DDCRC_BAD_BYTECT;
   }
   else  {       // rc < 0
      int errsv = errno;
      if (debug)
         printf("(%s) write() returned %d, errno=%s\n",
                __func__, rc, linux_errno_desc(errsv));
      // rc = modulate_rc(-errsv, RR_ERRNO);
      rc = -errsv;
   }
   return rc;
}


/* Read from I2C bus using read()
 *
 * Arguments:
 *   fh        file handle
 *   bytect
 *   readbuf
 *
 * Returns:
 *   0 if success
 *   if error:
 *      -errno
 *      DDCRC_BAD_BYTECT
 */
Base_Status_Errno_DDC read_reader(int fh, int bytect, Byte * readbuf) {
   bool debug = false;
   int rc = read(fh, readbuf, bytect);
   // per read() man page:
   // if >= 0, number of bytes actually read
   // if -1,   error occurred, errno is set
   if (rc >= 0) {
      if (rc == bytect)
         rc = 0;
      else
         rc = DDCRC_BAD_BYTECT;
   }
   else {    // rc < 0
      int errsv = errno;
      if (debug)
         printf("(%s) read() returned %d, errno=%s\n",
                __func__, rc, linux_errno_desc(errsv));
      // rc = modulate_rc(-errsv, RR_ERRNO);
      rc = -errsv;
   }
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


/* Write to I2C bus using ioctl I2C_RDWR
 *
 * Arguments:
 *   fh      file handle
 *   bytect  number of bytes to write
 *   pbytes  pointer to bytes to write
 *
 * Returns:
 *   0 if success
 *   if error:
 *      -errno
 */
Base_Status_Errno_DDC ioctl_writer(int fh, int bytect, Byte * pbytes) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. fh=%d, bytect=%d, pbytes=%p\n", __func__, fh, bytect, pbytes);
   struct i2c_msg              messages[1];
   struct i2c_rdwr_ioctl_data  msgset;

   messages[0].addr  = 0x37;
   messages[0].flags = 0;
   messages[0].len   = bytect;
   messages[0].buf   = (char *) pbytes;

   msgset.msgs  = messages;
   msgset.nmsgs = 1;

   // ioctl works, but valgrind complains about uninitialized parm
   // printf("(%s) messages=%p, messages[0]=%p, messages[0].buf=%p\n",
   //        __func__, messages, &messages[0], messages[0].buf);
   // printf("(%s) msgset=%p, msgset.msgs=%p, msgset.msgs[0]=%p, msgset.msgs[0].buf=%p\n",
   //        __func__, &msgset, msgset.msgs, &msgset.msgs[0], msgset.msgs[0].buf);

   // per ioctl() man page:
   // if success:
   //    normally:  0
   //    occasionally >0 is output parm
   // if error:
   //    -1, errno is set
   // 11/15: as seen: always returns 1 for success
   int rc = ioctl(fh, I2C_RDWR, &msgset);
   if (rc < 0) {
      if (debug) {
#ifdef USE_LIBEXPLAIN
         report_ioctl_error2(errno, fh, I2C_RDWR, &msgset, __func__, __LINE__-4, __FILE__, false /* fatal */ );
#endif
#ifndef USE_LIBEXPLAIN
         report_ioctl_error(errno, __func__, __LINE__-7, __FILE__, false /* fatal */ );
#endif
         // fprintf(stderr, "%s\n", explain_ioctl(fh, I2C_RDWR, &msgset));
      }
   }
   // printf("(%s) ioctl(..I2C_RDWR..) returned %d\n", __func__, rc);

   if (rc > 0) {
      // what should a positive value be equal to?  not bytect
      // if (debug)
      if (rc != 1)
         printf("(%s) ioctl() write returned %d\n", __func__, rc);
      rc = 0;
   }
   else if (rc < 0) {
      // rc = modulate_rc(-errno, RR_ERRNO);
      rc = -errno;
   }
   // if (debug)
   //    printf("(%s) Returning %d\n", __func__, rc);
   return rc;
}


/* Read from I2C bus using ioctl I2C_RDWR
 *
 * Arguments:
 *   fh      file handle
 *   bytect  number of bytes to read
 *   readbuf pointer to buffer in which to return bytes read
 *
 * Returns:
 *   0 if success
 *   if error:
 *      -errno (modulated)
 */
Base_Status_Errno_DDC ioctl_reader(int fh, int bytect, Byte * readbuf) {
   bool debug = true;
   // printf("(%s) Starting\n", __func__);
   struct i2c_msg              messages[1];
   struct i2c_rdwr_ioctl_data  msgset;

   messages[0].addr  = 0x37;
   messages[0].flags = I2C_M_RD;
   messages[0].len   = bytect;
   messages[0].buf   = (char *) readbuf;

   msgset.msgs  = messages;
   msgset.nmsgs = 1;

   // per ioctl() man page:
   // if success:
   //    normally:  0
   //    occasionally >0 is output parm
   // if error:
   //    -1, errno is set
   int rc =  ioctl(fh, I2C_RDWR, &msgset);
   if (rc < 0) {
      if (debug) {
#ifdef USE_LIBEXPLAIN
         report_ioctl_error2(errno, fh, I2C_RDWR, &msgset, __func__, __LINE__-4, __FILE__, false /* fatal */ );
#endif
#ifndef USE_LIBEXPLAIN
         report_ioctl_error(errno, __func__, __LINE__-7, __FILE__, false /* fatal */ );
#endif
      }
   }
   // printf("(%s) ioctl(..I2C_RDWR..) returned %d\n", __func__, rc);
   if (rc > 0) {
      // always see rc == 1
      if (rc != 1)
         printf("(%s) ioctl rc = %d, bytect =%d\n", __func__, rc, bytect);
      rc = 0;
   }
   else if (rc < 0)
      // rc = modulate_rc(-errno, RR_ERRNO);
      rc = -errno;
   return rc;
}


// i2c_sumbus_write_i2c_block_data_writer, and _reader are retained only for
// possible further exploration.   They do not work.

// Write to I2C bus using i2c_smbus_write_i2c_block_data()

// 11/2015:    fails:
Base_Status_Errno_DDC i2c_smbus_write_i2c_block_data_writer(int fh, int bytect, Byte * bytes_to_write) {
   bool debug = true;
   int rc = i2c_smbus_write_i2c_block_data(fh,
                                           bytes_to_write[0],   // cmd
                                           bytect-1,            // len of values
                                           bytes_to_write+1);   // values
   if (rc < 0) {
      int errsv = errno;
      if (debug)
         printf("(%s) i2c_smbus_write_i2c_block_data() returned %d, errno=%s\n",
                __func__, rc, linux_errno_desc(errsv));
      // set rc to -errno?
      // rc = modulate_rc(-errsv, RR_ERRNO);
      rc = -errsv;
   }
   return rc;
}


// Read from I2C bus using i2c_smbus_read_i2c_block_data()

// i2c_smbus_read_i2c_block_data can't handle capabilities fragments 32 bytes in size, since with
// "envelope" the packet exceeds the i2c_smbus_read_i2c_block_data 32 byte limit
Base_Status_Errno_DDC i2c_smbus_read_i2c_block_data_reader(int fh, int bytect, Byte * readbuf) {
   bool debug = true;
   const int MAX_BYTECT = 256;
   assert(bytect <= MAX_BYTECT);
   Byte workbuf[MAX_BYTECT+1];
   Byte zeroByte = 0x00;
   // can't handle 32 byte fragments from capabilities reply
   int rc = i2c_smbus_read_i2c_block_data(fh,
                                           zeroByte,   // cmd byte
                                           bytect,
                                           workbuf);
#ifdef WRONG
   if (rc > 0) {
      int errsv = errno;
      // always see rc=32, bytect=39
      // but leading byte of response is not 0
      printf("(%s) i2c_smbus_read_i2c_block_data() returned %d, bytect=%d\n",__func__, rc, bytect);
      hex_dump(workbuf, bytect);
      errno = errsv;
      rc = 0;
      int ndx = 0;
      // n no leading 0 byte
      for (ndx=0; ndx < bytect; ndx++)
         readbuf[ndx] = workbuf[ndx+0];
   }
   else
#endif
   if (rc == 0) {
      assert(workbuf[0] == zeroByte);    // whatever in cmd byte returned as first byte of buffer
      int ndx = 0;
      for (ndx=0; ndx < bytect; ndx++)
         readbuf[ndx] = workbuf[ndx+1];
   }
   else if (rc < 0) {
      int errsv = errno;
      if (debug)
         printf("(%s) i2c_smbus_read_i2c_block_data() returned %d, errno=%s\n",
                __func__, rc, linux_errno_desc(errsv));
      // set rc to -errno?
      // rc = modulate_rc(-errsv, RR_ERRNO);
      rc = -errno;
   }

   return rc;
}


