/*  i2c_do_io.c
 *
 *  Created on: Nov 17, 2015
 *      Author: rock
 */

#include <assert.h>
#include <stdio.h>

#include <util/string_util.h>
#include <base/execution_stats.h>
#include <base/ddc_base_defs.h>
#include <base/parms.h>
#include <i2c/i2c_base_io.h>
#include <i2c/i2c_do_io.h>


I2C_IO_Strategy  i2c_file_io_strategy = {
      write_writer,
      read_reader,
      "read_writer",
      "read_reader"
};

I2C_IO_Strategy i2c_ioctl_io_strategy = {
      ioctl_writer,
      ioctl_reader,
      "ioctl_writer",
      "ioctl_reader"
};

static I2C_IO_Strategy * i2c_io_strategy = &i2c_file_io_strategy;

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


/* Write to the I2C bus, using the function specified in the
 * currently active strategy.
 *
 * Arguments:
 *    fh              file handle for open /dev/i2c bus
 *    bytect          number of bytes to write
 *    bytes_to_write  pointer to bytes to be written
 *    sleep_millisec  delay after writing to bus
 */
Global_Status_Code invoke_i2c_writer(
      int    fh,
      int    bytect,
      Byte * bytes_to_write,
      int    sleep_millisec)
{
   bool debug = false;
   if (debug) {
      char * hs = hexstring(bytes_to_write, bytect);
      printf("(%s) writer=|%s|, bytes_to_write=%s\n",
             __func__, i2c_io_strategy->i2c_writer_name, hs);
      free(hs);
   }

   Base_Status_Errno_DDC rc;


   RECORD_IO_EVENT(
      IE_WRITE,
      ( rc = i2c_io_strategy->i2c_writer(fh, bytect, bytes_to_write ) )
     );


   // rc = i2c_io_strategy->i2c_writer(fh, bytect, bytes_to_write );
   if (debug)
      printf("(%s) writer() function returned %d\n", __func__, rc);

   assert (rc <= 0);
   if (rc == 0) {
#ifdef OLD
      if (sleep_millisec == DDC_TIMEOUT_USE_DEFAULT)
         sleep_millisec = DDC_TIMEOUT_MILLIS_DEFAULT;
      if (sleep_millisec != DDC_TIMEOUT_NONE)
         sleep_millis_with_trace(sleep_millisec, __func__, "after write");
#endif
   }

   Global_Status_Code gsc = modulate_base_errno_ddc_to_global(rc);
   if (debug)
      printf("(%s) Returning gsc=%s\n", __func__, global_status_code_description(gsc));
   return gsc;
}


/* Read from the I2C bus, using the function specified in the
 * currently active strategy.
 *
 * Arguments:
 *    fh              file handle for open /dev/i2c bus
 *    bytect          number of bytes to read
 *    bytes_to_write  location where bytes will be read to
 *    sleep_millisec  delay after reading from bus
 */
Global_Status_Code invoke_i2c_reader(
       int        fh,
       int        bytect,
       Byte *     readbuf,
       int        sleep_millisec)
{
     bool debug = false;
     if (debug)
        printf("(%s) reader=%s, bytect=%d\n",
               __func__, i2c_io_strategy->i2c_reader_name, bytect);

     Base_Status_Errno_DDC rc;


     RECORD_IO_EVENT(
        IE_READ,
        ( rc = i2c_io_strategy->i2c_reader(fh, bytect, readbuf) )
       );
     // rc = i2c_io_strategy->i2c_reader(fh, bytect, readbuf);
     if (debug)
        printf("(%s) reader() function returned %d\n", __func__, rc);

     assert (rc <= 0);
#ifdef OLD
     if (rc == 0) {
        if (sleep_millisec == DDC_TIMEOUT_USE_DEFAULT)
           sleep_millisec = DDC_TIMEOUT_MILLIS_DEFAULT;
        if (sleep_millisec != DDC_TIMEOUT_NONE)
           sleep_millis_with_trace(sleep_millisec, __func__, "after read");
     }
#endif
     Global_Status_Code gsc = modulate_base_errno_ddc_to_global(rc);
     if (debug )
        printf("(%s) Returning gsc=%s\n", __func__, global_status_code_description(gsc));
     return gsc;
}



