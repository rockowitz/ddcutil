/* i2c_do_io.c
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \file
 * Allows for alternative mechanisms to read and write to the IC2 bus.
 */

/** \cond */
#include <assert.h>
#include <stdio.h>
/** \endcond */

#include "util/string_util.h"

#include "base/core.h"
#include "base/parms.h"

#include "i2c/i2c_base_io.h"

#include "i2c/i2c_do_io.h"


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


static I2C_IO_Strategy * i2c_io_strategy = &i2c_file_io_strategy;  // default strategy

/** Sets an alternative I2C IO strategy.
 *
 * @param strategy_id  I2C IO strategy id
 */
void i2c_set_io_strategy(I2C_IO_Strategy_Id strategy_id) {
   switch (strategy_id) {
   case (I2C_IO_STRATEGY_FILEIO):
         i2c_io_strategy = &i2c_file_io_strategy;
         break;
   case (I2C_IO_STRATEGY_IOCTL):
         i2c_io_strategy= &i2c_ioctl_io_strategy;
         break;
   }
};


/** Writes to the I2C bus, using the function specified in the
 * currently active strategy.
 *
 * @param   fh              file handle for open /dev/i2c bus
 * @param   bytect          number of bytes to write
 * @param   bytes_to_write  pointer to bytes to be written
 */
Base_Status_Errno_DDC invoke_i2c_writer(
      int    fh,
      int    bytect,
      Byte * bytes_to_write)
{
   bool debug = false;
   if (debug) {
      char * hs = hexstring(bytes_to_write, bytect);
      DBGMSG("writer=|%s|, bytes_to_write=%s", i2c_io_strategy->i2c_writer_name, hs);
      free(hs);
   }

   Base_Status_Errno_DDC rc;
   RECORD_IO_EVENT(
      IE_WRITE,
      ( rc = i2c_io_strategy->i2c_writer(fh, bytect, bytes_to_write ) )
     );
   if (debug)
      DBGMSG("writer() function returned %d", rc);
   assert (rc <= 0);

   if (debug)
      DBGMSG("Returning rc=%d", rc);
   return rc;
}


/** Reads from the I2C bus, using the function specified in the
 * currently active strategy.
 *
 * @param   fh              file handle for open /dev/i2c bus
 * @param   bytect          number of bytes to read
 * @param   readbuf         location where bytes will be read to
 */
Base_Status_Errno_DDC invoke_i2c_reader(
       int        fh,
       int        bytect,
       Byte *     readbuf)
{
     bool debug = false;
     DBGMSF(debug, "reader=%s, bytect=%d", i2c_io_strategy->i2c_reader_name, bytect);

     Base_Status_Errno_DDC rc;
     RECORD_IO_EVENT(
        IE_READ,
        ( rc = i2c_io_strategy->i2c_reader(fh, bytect, readbuf) )
       );
     DBGMSF(debug, "reader() function returned %d", rc);
     assert (rc <= 0);

     DBGMSF(debug, "Returning rc=%d", rc);
     return rc;
}

#ifdef TEST_THAT_DIDNT_WORK
// fails
Base_Status_Errno_DDC invoke_single_byte_i2c_reader(
      int        fh,
      int        bytect,
      Byte *     readbuf)
{
   bool debug = true;
   DBGMSF(debug, "bytect=%d", bytect);
   Base_Status_Errno_DDC psc = 0;
   int ndx = 0;
   for (;ndx < bytect; ndx++) {
      psc = invoke_i2c_reader(fh, 1, readbuf+ndx);
      if (psc != 0)
         break;
      // call_tuned_sleep_i2c(SE_POST_READ);
   }
   DBGMSF(debug, "Returning psc=%s", psc_desc(psc));
   return psc;
}
#endif

