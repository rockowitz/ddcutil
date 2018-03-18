/* i2c_io.c
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
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#include <assert.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util/string_util.h"

#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/execution_stats.h"
#include "base/linux_errno.h"
#include "base/parms.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"

#include "i2c/i2c_base_io.h"

#include "test/i2c/i2c_io_old.h"



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


//
// Write to I2C bus
//



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



Public_Status_Code call_i2c_writer(
      I2C_Writer writer,
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

   Public_Status_Code rc;

   RECORD_IO_EVENT(IE_WRITE,
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

   // rc = modulate_base_errno_ddc_to_global(rc);
   if (debug)  printf("(%s) Returning rc=%d\n", __func__, rc);
   return rc;
}


Public_Status_Code call_i2c_reader(
       I2C_Reader reader,
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

   Public_Status_Code rc;

   RECORD_IO_EVENT(
      IE_READ,
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

   // rc = modulate_base_errno_ddc_to_global(rc);
   if (debug ) printf("(%s) Returning rc=%d\n",__func__, rc);
   return rc;
}



Public_Status_Code do_i2c_file_write(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec) {
   return call_i2c_writer(write_writer, "write_writer", fh, bytect, bytes_to_write, sleep_millisec);
}

#ifdef WONT_COMPILE_ON_FEDORA
Public_Status_Code do_i2c_smbus_write_i2c_block_data(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec) {
   return call_i2c_writer(
              i2c_smbus_write_i2c_block_data_writer,
              "i2c_smbus_write_i2c_block_data_writer",
              fh,
              bytect,
              bytes_to_write,
              sleep_millisec);
}
#endif


Public_Status_Code do_i2c_ioctl_write(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec) {
   return call_i2c_writer(ioctl_writer, "ioctl_writer", fh, bytect, bytes_to_write, sleep_millisec);
}


Public_Status_Code do_i2c_file_read(int fh, int bytect, Byte * readbuf, int sleep_millisec) {
   return call_i2c_reader(read_reader, "read_reader", fh, bytect, readbuf, sleep_millisec);
}

#ifdef WONT_COMPILE_ON_FEDORA
Public_Status_Code do_i2c_smbus_read_i2c_block_data(int fh, int bytect, Byte * readbuf, int sleep_millisec) {
   return call_i2c_reader(
             i2c_smbus_read_i2c_block_data_reader,
             "i2c_smbus_read_i2c_block_data_reader",
             fh,
             bytect,
             readbuf,
             sleep_millisec);
}
#endif


Public_Status_Code do_i2c_ioctl_read(int fh, int bytect, Byte * readbuf, int sleep_millisec) {
   return call_i2c_reader(ioctl_reader, "ioctl_reader", fh, bytect, readbuf, sleep_millisec);
}




Public_Status_Code perform_i2c_write(int fh, char * write_mode, int bytect, Byte * bytes_to_write, int sleep_millisec) {
   // bool debug = i2c_write_trace_level;
   bool debug = false;
   if (debug) printf("(%s) Starting. write_mode=%s\n", __func__, write_mode);

   int rc = 0;
   I2C_Writer writer = NULL;

   if      ( streq(write_mode, "write") )                         writer = write_writer;
#ifdef WONT_COMPILE_ON_FEDORA
   else if ( streq(write_mode, "i2c_smbus_write_i2c_block_data")) writer = i2c_smbus_write_i2c_block_data_writer;
#endif
   else if ( streq(write_mode, "ioctl_write"))                    writer = ioctl_writer;

   if (writer) {
      rc = call_i2c_writer(writer, write_mode, fh, bytect, bytes_to_write, sleep_millisec);
   }
   else {
      printf("(%s) Unsupported write mode: %s\n", __func__, write_mode);
      rc = DDCRC_ARG;       // was -DDCRC_INVALID_MODE;
   }

   if (debug) printf("(%s) Returning %d\n", __func__, rc);
   return rc;
}


Public_Status_Code perform_i2c_write2(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec) {
   return perform_i2c_write(fh, write_mode, bytect, bytes_to_write, sleep_millisec);
}


// Returns:  -errno, DDCRC_BAD_BYTECT, DDCRC_INVALID_MODE (if invalid read mode)

Public_Status_Code perform_i2c_read(int    fh, char * read_mode, int bytect, Byte * readbuf, int sleep_millisec) {
   // bool debug = i2c_read_trace_level;
   bool debug = false;
   if (debug) printf("(%s) Starting. read_mode=%s\n", __func__, read_mode);

   int rc;
   I2C_Reader reader = NULL;

   if      ( streq(read_mode, "read") )                         reader = read_reader;
#ifdef WONT_COMPILE_ON_FEDORA
   else if ( streq(read_mode, "i2c_smbus_read_i2c_block_data")) reader = i2c_smbus_read_i2c_block_data_reader;
#endif
   else if ( streq(read_mode, "ioctl_read"))                    reader = ioctl_reader;
   // assert(reader);

   if (reader) {
      rc = call_i2c_reader(reader, read_mode, fh, bytect, readbuf, sleep_millisec);
   }
   else {
      printf("(%s) Unsupported read mode: %s\n", __func__, read_mode);
      rc = DDCRC_ARG;     // was DDCRC_INVALID_MODE;
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
Public_Status_Code perform_i2c_read2(int    fh, int bytect, Byte * readbuf, int sleep_millisec) {
   return perform_i2c_read(fh, read_mode, bytect, readbuf, sleep_millisec);
}

