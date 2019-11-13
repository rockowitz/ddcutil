/** \file i2c_do_io.c
 *
 * Allows for alternative mechanisms to read and write to the IC2 bus.
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <stdio.h>
/** \endcond */

#include "util/string_util.h"

#include "base/core.h"
#include "base/parms.h"
#include "base/status_code_mgt.h"
#include "base/last_io_event.h"

// #include "i2c/i2c_base_io.h"

#include "i2c/i2c_do_io.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;


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
}


/** Writes to the I2C bus, using the function specified in the
 * currently active strategy.
 *
 * @param   fd              Linux file descriptor for open /dev/i2c bus
 * @param   bytect          number of bytes to write
 * @param   bytes_to_write  pointer to bytes to be written
 * @return  status code
 */
Status_Errno_DDC invoke_i2c_writer(
      int    fd,
      int    bytect,
      Byte * bytes_to_write)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "writer=%s, bytes_to_write=%s",
                 i2c_io_strategy->i2c_writer_name, hexstring_t(bytes_to_write, bytect));

   Status_Errno_DDC rc;
   RECORD_IO_EVENT(
      IE_WRITE,
      ( rc = i2c_io_strategy->i2c_writer(fd, bytect, bytes_to_write ) )
     );
   // DBGMSF(debug, "writer() function returned %d", rc);
   assert (rc <= 0);
   RECORD_IO_FINISH_NOW(fd, IE_WRITE);

   DBGTRC(debug, TRACE_GROUP, "Returning rc=%s", psc_desc(rc));
   return rc;
}


/** Reads from the I2C bus, using the function specified in the
 * currently active strategy.
 *
 * @param   fd              Linux file descriptor for open /dev/i2c bus
 * @param   bytect          number of bytes to read
 * @param   readbuf         location where bytes will be read to
 * @return  status code
 */
Status_Errno_DDC invoke_i2c_reader(
       int        fd,
       int        bytect,
       Byte *     readbuf)
{
     bool debug = false;
     DBGTRC(debug, TRACE_GROUP, "reader=%s, bytect=%d", i2c_io_strategy->i2c_reader_name, bytect);

     Status_Errno_DDC rc;
     RECORD_IO_EVENTX(
        fd,
        IE_READ,
        ( rc = i2c_io_strategy->i2c_reader(fd, bytect, readbuf) )
       );
     assert (rc <= 0);

     if (rc == 0) {
        DBGTRC(debug, TRACE_GROUP, "Bytes read: %s", hexstring_t(readbuf, bytect) );
     }
     DBGTRC(debug, TRACE_GROUP, "Returning rc=%s", psc_desc(rc));
     return rc;
}


#ifdef TEST_THAT_DIDNT_WORK
// fails
Status_Errno_DDC invoke_single_byte_i2c_reader(
      int        fd,
      int        bytect,
      Byte *     readbuf)
{
   bool debug = false;
   DBGMSF(debug, "bytect=%d", bytect);
   Status_Errno_DDC psc = 0;
   int ndx = 0;
   for (;ndx < bytect; ndx++) {
      psc = invoke_i2c_reader(fd, 1, readbuf+ndx);
      if (psc != 0)
         break;
      // call_tuned_sleep_i2c(SE_POST_READ);
   }
   DBGMSF(debug, "Returning psc=%s", psc_desc(psc));
   return psc;
}
#endif

