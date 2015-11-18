/*
 *  i2c_io.c
 *
 * A framework for exercising the various calls that read and
 * write to the i2c bus, designed for use in test code.
 *
 * In normal code, set_i2c_write_mode() and set_i2c_read_mode()
 * can be called once to specify the write and read modes to
 * be used, and then perform_i2c_write2() and perform_i2c_read2()
 * are called without specifying the write or read mode each time.
 *
 * Since this is a framework for exploratory programming, the mode
 * identifiers are simply strings.
 *
 * */

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

#include <base/ddc_errno.h>
#include <base/call_stats.h>
#include <base/common.h>
#include <base/ddc_packets.h>
#include <base/msg_control.h>
#include <base/parms.h>
#include <base/util.h>
#include <base/status_code_mgt.h>
#include <base/linux_errno.h>

#include <i2c/i2c_io.h>


// TraceControl i2c_write_trace_level = NEVER;
// TraceControl i2c_read_trace_level  = NEVER;

static char * write_mode = DEFAULT_I2C_WRITE_MODE;
static char * read_mode  = DEFAULT_I2C_READ_MODE;

void set_i2c_write_mode(char* mode) {
   write_mode = mode;
}

void set_i2c_read_mode(char* mode) {
   read_mode = mode;
}


// Dummy value for timing_stats in case init_adl_call_stats() is never called.
// Without it, macro RECORD_TIMING_STATS would have to test that
// both timing_stats and pTimingStat->p<stat> are not null.
static I2C_Call_Stats dummystats = {
        .pread_write_stats = NULL,
        .popen_stats  = NULL,
        .pclose_stats = NULL,
        .stats_active = false
 };

static I2C_Call_Stats*  timing_stats = &dummystats;
static bool gather_timing_stats = false;



void init_i2c_io_stats(I2C_Call_Stats * pstats) {
   // printf("(%s) Starting. pstats = %p\n", __func__, timing_stats);
   assert(pstats);
   timing_stats = pstats;
   gather_timing_stats = true;

//   pstats->stat_name = "I2C IO calls";
}


//
// Write to I2C bus
//


typedef int (*I2c_Writer)(int fh, int bytect, Byte * bytes_to_write);
typedef int (*I2c_Reader)(int fh, int bytect, Byte * readbuf);

/* To make the various methods of reading and writing the I2C bus
 * interchangeable, these calls are encapsulated in functions whose
 * signatures are compatible with I2c_Writer and I2c_Reader.  The
 * encapsulating functions have names of the form xxx_writer and xxx_reader.
 *
 * The functions are:
 *
 * I2C_Writer:
 *    write_writer
 *    ioctl_writer
 *    i2c_smbus_write_i2c_block_data_writer
 *
 * I2C_Reader:
 *    read_reader
 *    ioctl_reader
 *    i2c_smbus_read_i2c_block_data_reader
 *
 * The I2C_Writer (resp I2C_Reader) functions can then be invoked by
 * calling call_i2c_writer (resp call_i2c_reader) passing a function
 * pointer as a parameter. I2C_Writer and I2C_Reader perform common
 * services including tracing, timing, and sleeping after writes.
 *
 * The do_xxx variants call the corresponding base functions, but do
 * so indirectly through call_i2c_writer() and call_i2c_reader() in
 * order to gain the common services.  For example, do_i2c_ioctl_write()
 * wraps ioctl_writer().
 *
 * perform_i2c_write()/perform_i2c_read() also allow for invoking any of
 * the base functions.  Whereas call_i2c_writer()/call_i2c_reader() take
 * function pointers as parameters, perform_i2c_xxx() are passed a string
 * name indicating the function to be chosen.  perform_i2c_xxx() look up
 * the function pointer from the string name and invoke call_i2c_writer()
 * or call_i2c_reader().  The makes it easy for test frameworks to
 * dynamically choose which base read/write mechanism to choose.
 *
 * perform_i2c_write2()/perform_i2c_read2() are similar to perform_is2_write()/
 * perform_i2c_read(), but instead determine the base function to be used
 * from global settings set by set_i2c_write_mode()/set_i2c_read_mode().
 *
 */


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
Global_Status_Code write_writer(int fh, int bytect, Byte * pbytes) {
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
Global_Status_Code read_reader(int fh, int bytect, Byte * readbuf) {
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
      rc = modulate_rc(-errsv, RR_ERRNO);
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
Global_Status_Code ioctl_writer(int fh, int bytect, Byte * pbytes) {
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
      rc = modulate_rc(-errno, RR_ERRNO);
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
Global_Status_Code ioctl_reader(int fh, int bytect, Byte * readbuf) {
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
      rc = modulate_rc(-errno, RR_ERRNO);
   return rc;
}


// Write to I2C bus using i2c_smbus_write_i2c_block_data()

Global_Status_Code i2c_smbus_write_i2c_block_data_writer(int fh, int bytect, Byte * bytes_to_write) {
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
      rc = modulate_rc(-errsv, RR_ERRNO);
   }
   return rc;
}


// Read from I2C bus using i2c_smbus_read_i2c_block_data()

Global_Status_Code i2c_smbus_read_i2c_block_data_reader(int fh, int bytect, Byte * readbuf) {
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
      rc = modulate_rc(-errsv, RR_ERRNO);
   }

   return rc;
}



Global_Status_Code call_i2c_writer(
      I2c_Writer writer,
      char * writer_name,
      int    fh,
      int    bytect,
      Byte * bytes_to_write,
      int    sleep_millisec)
{
   // bool debug = i2c_write_trace_level;
   bool debug = false;
   if (debug) {
      char * hs = hexstring(bytes_to_write, bytect);
      printf("(%s) writer=|%s|, bytes_to_write=%s\n", __func__, writer_name, hs);
      free(hs);
   }

   Global_Status_Code rc;

   RECORD_TIMING_STATS_NOERRNO(
      timing_stats->pread_write_stats,
      ( rc = writer(fh, bytect, bytes_to_write ) )
     );
   if (debug)
      printf("(%s) writer() function returned %d\n",
             __func__, rc);

   assert (rc <= 0);

   if (rc == 0) {
      if (sleep_millisec == DDC_TIMEOUT_USE_DEFAULT)
         sleep_millisec = DDC_TIMEOUT_MILLIS_DEFAULT;
      if (sleep_millisec != DDC_TIMEOUT_NONE)
         sleep_millis_with_trace(sleep_millisec, __func__, "after write");
   }

   if (debug)  printf("(%s) Returning rc=%d\n", __func__, rc);
   return rc;
}


Global_Status_Code call_i2c_reader(
       I2c_Reader reader,
       char *     reader_name,
       int        fh,
       int        bytect,
       Byte *     readbuf,
       int        sleep_millisec)
{
   // bool debug = i2c_write_trace_level;
   bool debug = false;
   if (debug)
      printf("(%s) reader=%s, bytect=%d\n", __func__, reader_name, bytect);

   Global_Status_Code rc;

   RECORD_TIMING_STATS_NOERRNO(
      timing_stats->pread_write_stats,
      ( rc = reader(fh, bytect, readbuf) )
     );
   if (debug)
      printf("(%s) reader() function returned %d\n",
             __func__, rc);

   assert (rc <= 0);

   if (rc == 0) {
      if (sleep_millisec == DDC_TIMEOUT_USE_DEFAULT)
         sleep_millisec = DDC_TIMEOUT_MILLIS_DEFAULT;
      if (sleep_millisec != DDC_TIMEOUT_NONE)
         sleep_millis_with_trace(sleep_millisec, __func__, "after read");
   }

   if (debug ) printf("(%s) Returning rc=%d\n",__func__, rc);
   return rc;
}



Global_Status_Code do_i2c_file_write(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec) {
   return call_i2c_writer(write_writer, "write_writer", fh, bytect, bytes_to_write, sleep_millisec);
}


Global_Status_Code do_i2c_smbus_write_i2c_block_data(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec) {
   return call_i2c_writer(
              i2c_smbus_write_i2c_block_data_writer,
              "i2c_smbus_write_i2c_block_data_writer",
              fh,
              bytect,
              bytes_to_write,
              sleep_millisec);
}


Global_Status_Code do_i2c_ioctl_write(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec) {
   return call_i2c_writer(ioctl_writer, "ioctl_writer", fh, bytect, bytes_to_write, sleep_millisec);
}


Global_Status_Code do_i2c_file_read(int fh, int bytect, Byte * readbuf, int sleep_millisec) {
   return call_i2c_reader(read_reader, "read_reader", fh, bytect, readbuf, sleep_millisec);
}


Global_Status_Code do_i2c_smbus_read_i2c_block_data(int fh, int bytect, Byte * readbuf, int sleep_millisec) {
   return call_i2c_reader(
             i2c_smbus_read_i2c_block_data_reader,
             "i2c_smbus_read_i2c_block_data_reader",
             fh,
             bytect,
             readbuf,
             sleep_millisec);
}


Global_Status_Code do_i2c_ioctl_read(int fh, int bytect, Byte * readbuf, int sleep_millisec) {
   return call_i2c_reader(ioctl_reader, "ioctl_reader", fh, bytect, readbuf, sleep_millisec);
}




Global_Status_Code perform_i2c_write(int fh, char * write_mode, int bytect, Byte * bytes_to_write, int sleep_millisec) {
   // bool debug = i2c_write_trace_level;
   bool debug = false;
   if (debug) printf("(%s) Starting. write_mode=%s\n", __func__, write_mode);

   int rc = 0;
   I2c_Writer writer = NULL;

   if      ( streq(write_mode, "write") )                         writer = write_writer;
   else if ( streq(write_mode, "i2c_smbus_write_i2c_block_data")) writer = i2c_smbus_write_i2c_block_data_writer;
   else if ( streq(write_mode, "ioctl_write"))                    writer = ioctl_writer;

   if (writer) {
      rc = call_i2c_writer(writer, write_mode, fh, bytect, bytes_to_write, sleep_millisec);
   }
   else {
      printf("(%s) Unsupported write mode: %s\n", __func__, write_mode);
      rc = -DDCRC_INVALID_MODE;
   }

   if (debug) printf("(%s) Returning %d\n", __func__, rc);
   return rc;
}


Global_Status_Code perform_i2c_write2(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec) {
   return perform_i2c_write(fh, write_mode, bytect, bytes_to_write, sleep_millisec);
}


// Returns:  -errno, DDCRC_BAD_BYTECT, DDCRC_INVALID_MODE (if invalid read mode)

Global_Status_Code perform_i2c_read(int    fh, char * read_mode, int bytect, Byte * readbuf, int sleep_millisec) {
   // bool debug = i2c_read_trace_level;
   bool debug = false;
   if (debug) printf("(%s) Starting. read_mode=%s\n", __func__, read_mode);

   int rc;
   I2c_Reader reader = NULL;

   if      ( streq(read_mode, "read") )                         reader = read_reader;
   else if ( streq(read_mode, "i2c_smbus_read_i2c_block_data")) reader = i2c_smbus_read_i2c_block_data_reader;
   else if ( streq(read_mode, "ioctl_read"))                    reader = ioctl_reader;
   // assert(reader);

   if (reader) {
      rc = call_i2c_reader(reader, read_mode, fh, bytect, readbuf, sleep_millisec);
   }
   else {
      printf("(%s) Unsupported read mode: %s\n", __func__, read_mode);
      rc = DDCRC_INVALID_MODE;
   }

   if (debug ) printf("(%s) Returning %d\n", __func__, rc);
   return rc;
}


/* Performs I2C read using the default read function.
 *
 * Arguments:
 *   fh                  file handle for open I2C device
 *   bytect              number of bytes to read
 *   readbuf             address at which to store bytes
 *   sleep_milliseconds  milliseconds to sleep after read
 *                       may be DDC_TIMEOUT_USE_DEFAULT or DDC_TIMEOUT_NONE
 *
 *   Returns:
 *     9 if success
 *     modulated error number if error
 */
Global_Status_Code perform_i2c_read2(int    fh, int bytect, Byte * readbuf, int sleep_millisec) {
   return perform_i2c_read(fh, read_mode, bytect, readbuf, sleep_millisec);
}
