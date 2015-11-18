/*
 * i2c_shim.c
 *
 *  Created on: Nov 17, 2015
 *      Author: rock
 */

#include <assert.h>
#include <stdio.h>

#include <util/string_util.h>

#include <base/ddc_base_defs.h>
#include <base/parms.h>
#include <i2c/i2c_base_io.h>
// #include <i2c/i2c_io.h>
#include <i2c/i2c_shim.h>


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

// void init_i2c_io() { }

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

// TODO: merge call__shim functions (which are currently copies of call_... in i2c_io.c,
// into shim_...


Global_Status_Code call_i2c_writer_shim(
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

   rc = modulate_base_errno_ddc_to_global(rc);
   if (debug)  printf("(%s) Returning rc=%d\n", __func__, rc);
   return rc;
}


Global_Status_Code call_i2c_reader_shim(
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

   rc = modulate_base_errno_ddc_to_global(rc);
   if (debug ) printf("(%s) Returning rc=%d\n",__func__, rc);
   return rc;
}









Global_Status_Code shim_i2c_writer(
      int    fh,
      int    bytect,
      Byte * bytes_to_write,
      int    sleep_millisec)
{
   return call_i2c_writer_shim(
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
   return call_i2c_reader_shim(
         i2c_io_strategy->i2c_reader,
         i2c_io_strategy->i2c_reader_name,   // temporarily for shimming
         fh,
         bytect,
         readbuf,
         sleep_millisec);
}



