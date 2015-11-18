/*
 * i2c_base_io.c
 *
 *  Created on: Nov 17, 2015
 *      Author: rock
 */

#include <assert.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <i2c-dev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>        // usleep

#include <util/string_util.h>

#include <base/ddc_base_defs.h>
#include <base/ddc_errno.h>
#include <base/call_stats.h>
#include <base/common.h>
#include <base/ddc_packets.h>
#include <base/msg_control.h>
#include <base/parms.h>
#include <base/util.h>
#include <base/status_code_mgt.h>
#include <base/linux_errno.h>

#include <i2c/i2c_base_io.h>


// Dummy value for timing_stats in case init_adl_call_stats() is never called.
// Without it, macro RECORD_TIMING_STATS would have to test that
// both timing_stats and pTimingStat->p<stat> are not null.
static I2C_Call_Stats dummystats = {
        .pread_write_stats = NULL,
        .popen_stats  = NULL,
        .pclose_stats = NULL,
        .stats_active = false
 };

I2C_Call_Stats*  timing_stats = &dummystats;
static bool gather_timing_stats = false;



void init_i2c_io_stats(I2C_Call_Stats * pstats) {
   // printf("(%s) Starting. pstats = %p\n", __func__, timing_stats);
   assert(pstats);
   timing_stats = pstats;
   gather_timing_stats = true;

//   pstats->stat_name = "I2C IO calls";
}



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
 *      modulated(-errno)
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
      rc = modulate_rc(-errsv, RR_ERRNO);
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
 *      modulated(-errno)
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
 *      -errno (modulated)
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





#define NAME(id) #id

I2C_IO_Strategy  i2c_file_io_strategy = {
      write_writer,
      read_reader,
      NAME(read_writer),
      NAME(read_reader)
};

I2C_IO_Strategy i2c_ioctl_io_strategy = {
      ioctl_writer,
      ioctl_reader,
      NAME(ioctl_writer),
      NAME(ioctl_reader)
};

#undef NAME

I2C_IO_Strategy * i2c_io_strategy = &i2c_file_io_strategy;

void init_i2c_io() {

}

void set_i2c_io_strategy(I2C_IO_Strategy_Id strategy_id) {
   switch (strategy_id) {
   case (I2C_IO_STRATEGY_FILEIO):
         i2c_io_strategy = &i2c_file_io_strategy;
         break;
   case (I2C_IO_STRATEGY_IOCTL):
         i2c_io_strategy= &i2c_ioctl_io_strategy;
         break;
   }
};



