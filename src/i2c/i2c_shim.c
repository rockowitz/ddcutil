/*
 * i2c_shim.c
 *
 *  Created on: Nov 17, 2015
 *      Author: rock
 */

#include <base/ddc_base_defs.h>
#include <i2c/i2c_base_io.h>
#include <i2c/i2c_io.h>


Global_Status_Code shim_i2c_writer(
      int    fh,
      int    bytect,
      Byte * bytes_to_write,
      int    sleep_millisec)
{
   return call_i2c_writer(
         i2c_io_strategy->i2c_writer,
         i2c_io_strategy->i2c_writer_name,   // temporarily for shimming
         fh,
         bytect,
         bytes_to_write,
         sleep_millisec);
}


Global_Status_Code shim_i2c_reader(
       int        fh,
       int        bytect,
       Byte *     readbuf,
       int        sleep_millisec)
{
   return call_i2c_reader(
         i2c_io_strategy->i2c_reader,
         i2c_io_strategy->i2c_reader_name,   // temporarily for shimming
         fh,
         bytect,
         readbuf,
         sleep_millisec);
}

