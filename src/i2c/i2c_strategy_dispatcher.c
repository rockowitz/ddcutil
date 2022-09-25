/** \file i2c_strategy_dispatcher.c
 *
 *  Allows for alternative mechanisms to read and write to the IC2 bus.
 */
 
// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <stdio.h>
#include <syslog.h>
/** \endcond */

#include "util/file_util.h"
#include "util/i2c_util.h"
#include "util/string_util.h"
#include "util/sysfs_i2c_util.h"

#include "base/core.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/status_code_mgt.h"

#include "i2c_strategy_dispatcher.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;



I2C_IO_Strategy  i2c_file_io_strategy = {
      I2C_IO_STRATEGY_FILEIO,
      "I2C_IO_STRATEGY_FILEIO",
      i2c_fileio_writer,
      i2c_fileio_reader,
      "fileio_writer",
      "fileio_reader"
};


I2C_IO_Strategy i2c_ioctl_io_strategy = {
      I2C_IO_STRATEGY_IOCTL,
      "I2C_IO_STRATEGY_IOCTL",
      i2c_ioctl_writer,
      i2c_ioctl_reader,
      "ioctl_writer",
      "ioctl_reader"
};

char * strategy_names[] = {"I2C_IO_STRATEGY_NOT_SET", "I2C_IO_STRATEGY_FILEIO", "I2C_IO_STRATEGY_IOCTL"};


char * i2c_io_strategy_name_from_id(I2C_IO_Strategy_Id id) {
   char * result = strategy_names[id];
   // printf("(%s) id=%d, returning %s\n", __func__, id, result);
   return result;
}


// the default strategy is now the one set by the user
// if not set it is determined by the driver
static I2C_IO_Strategy * i2c_io_strategy = NULL;    //&i2c_file_io_strategy;


bool nvidia_einval_bug_encountered = false;

bool check_nvidia_einval_bug_encountered(I2C_IO_Strategy_Id strategy_id,  int busno, int rc) {
   bool encountered = false;
   char * driver_name = get_i2c_device_sysfs_driver(busno);
   if (streq(driver_name, "nvidia") && rc == -EINVAL && strategy_id == I2C_IO_STRATEGY_IOCTL) {
   // if ( rc == -EINVAL && strategy_id == I2C_IO_STRATEGY_IOCTL) {  // ***TESTING***
      nvidia_einval_bug_encountered = true;
      i2c_set_io_strategy(I2C_IO_STRATEGY_FILEIO);   // the new normal
      free(driver_name);
      char * msg = "nvida/i2c-dev bug encountered. Forcing future io I2C_IO_STRATEGY_FILEIO. Retrying";
      DBGTRC(true, TRACE_GROUP, msg);
      syslog(LOG_WARNING,  msg);
      encountered = true;
   }
   return encountered;
}



/** Sets an alternative I2C IO strategy.
 *
 * @param strategy_id  I2C IO strategy id
 */
void
i2c_set_io_strategy(I2C_IO_Strategy_Id strategy_id) {
   bool debug = false;
   assert(strategy_id != I2C_IO_STRATEGY_NOT_SET);

   switch (strategy_id) {
   case (I2C_IO_STRATEGY_NOT_SET):
         PROGRAM_LOGIC_ERROR("Impossible case");
         i2c_io_strategy = NULL;
         break;
   case (I2C_IO_STRATEGY_FILEIO):
         i2c_io_strategy = &i2c_file_io_strategy;
         break;
   case (I2C_IO_STRATEGY_IOCTL):
         i2c_io_strategy= &i2c_ioctl_io_strategy;
         break;
   }
   DBGMSF(debug, "Set strategy: %s", i2c_io_strategy->strategy_name);
}

I2C_IO_Strategy_Id i2c_get_io_strategy_id() {
   I2C_IO_Strategy_Id result = I2C_IO_STRATEGY_NOT_SET;
   if (i2c_io_strategy)
      result = i2c_io_strategy->strategy_id;
   return result;
}


// caller responsible for freeing returned value
char * driver_from_dev_name(char * device_name) {
   bool debug = false;
   DBGMSF(debug, "Starting. device_name = %s", device_name);
   char * driver_name = NULL;
   int busno = extract_number_after_hyphen(device_name);
   if (busno >= 0) {
      driver_name = get_i2c_device_sysfs_driver(busno);
   }
   DBGMSF(debug, "Returning: %s", driver_name);
   return driver_name;
}


// caller responsible for freeing returned value
char * driver_from_fd(int fd) {
   char * driver_name = NULL;
   int busno = extract_number_after_hyphen(filename_for_fd_t(fd));
   if (busno >= 0) {
      driver_name = get_i2c_device_sysfs_driver(busno);
   }
   printf("($s) Returning %s\n", driver_name);
   return driver_name;
}


int busno_from_fd(int fd) {
   int busno = extract_number_after_hyphen(filename_for_fd_t(fd));
   return busno;
}




I2C_IO_Strategy *
calc_i2c_io_strategy(char * device_name) {
   bool debug = false;
   DBGMSF(debug, "Starting. device_name = %s", device_name);
   I2C_IO_Strategy * result = i2c_io_strategy;      // explicit strategy if one has been set
   if (!result) {
      result = &i2c_ioctl_io_strategy;   // the default if no explicit strategy
      char * driver_name = driver_from_dev_name(device_name);
      if (driver_name) {
         // special handling for nvida:
         if (streq(driver_name, "nvidia") && nvidia_einval_bug_encountered) {
         // if ( nvidia_einval_bug_encountered)  {  // ***TESTING***
            i2c_io_strategy = &i2c_file_io_strategy;
            result = &i2c_file_io_strategy;
         }
         free(driver_name);
      }
   }
   DBGMSF(debug, "Returning strategy %s", result->strategy_name);
   return result;
}


I2C_IO_Strategy *
calc_i2c_io_strategy_for_fd(int fd) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fd=%d", fd);
   char * device_name = filename_for_fd_t(fd);
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "device_name = %s", device_name);
   I2C_IO_Strategy * strategy = calc_i2c_io_strategy(device_name);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %s", strategy->strategy_name);
   return strategy;
}


I2C_IO_Strategy_Id i2c_get_calculated_io_strategy_id(char * device_name) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "device_name = %s", device_name);
   I2C_IO_Strategy_Id id =  calc_i2c_io_strategy(device_name)->strategy_id;
   DBGTRC_DONE(debug, TRACE_GROUP, "Done.  Returning %s", i2c_io_strategy_name_from_id(id));
   return id;
}


/** Writes to the I2C bus, using the function specified in the
 * currently active strategy.
 *
 * @param   fd              Linux file descriptor for open /dev/i2c bus
 * @param   slave_address   slave address to write to
 * @param   bytect          number of bytes to write
 * @param   bytes_to_write  pointer to bytes to be written
 * @return  status code
 */
Status_Errno_DDC invoke_i2c_writer(
      int    fd,
      Byte   slave_address,
      int    bytect,
      Byte * bytes_to_write)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
                 "fd=%d, filename=%s, slave_address=0x%02x, bytect=%d, bytes_to_write=%p -> %s",
                 fd,
                 filename_for_fd_t(fd),
                 slave_address,
                 bytect,
                 bytes_to_write,
                 hexstring_t(bytes_to_write, bytect));
   I2C_IO_Strategy * strategy = calc_i2c_io_strategy_for_fd(fd);
retry:
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "strategy = %s", strategy->strategy_name);
   Status_Errno_DDC rc = strategy->i2c_writer(fd, slave_address, bytect, bytes_to_write);

   if (rc == -EINVAL) {
      int busno = extract_number_after_hyphen(filename_for_fd_t(fd));
      if (busno >= 0) {    // guard against pathological case
         bool encountered =  check_nvidia_einval_bug_encountered(strategy->strategy_id, busno, rc);
         if (encountered) {
            strategy = i2c_io_strategy;  // set the current io strategy
            goto retry;
         }
      }
   }
   assert (rc <= 0);
   
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");
   return rc;
}


/** Reads from the I2C bus, using the function specified in the
 *  currently active strategy.
 *
 *  @param   fd              Linux file descriptor for open /dev/i2c bus
 *  @param   slave_address   I2C slave address to read from
 *  @param   read_bytewise   if true, read one byte at a time
 *  @param   bytect          number of bytes to read
 *  @param   readbuf         location where bytes will be read to
 *  @return  status code
 */
Status_Errno_DDC invoke_i2c_reader(
       int        fd,
       Byte       slave_address,
       bool       read_bytewise,
       int        bytect,
       Byte *     readbuf)
{
     bool debug = false;
     DBGTRC_STARTING(debug, TRACE_GROUP,
                   "fd=%d, filename=%s, slave_address=0x%02x, bytect=%d, read_bytewise=%s, readbuf=%p",
                   fd,
                   filename_for_fd_t(fd),
                   slave_address,
                   bytect,
                   sbool(read_bytewise),
                   readbuf);

     I2C_IO_Strategy * strategy = calc_i2c_io_strategy(filename_for_fd_t(fd));
     DBGTRC_NOPREFIX(debug, TRACE_GROUP, "strategy = %s", strategy->strategy_name);
     Status_Errno_DDC rc;
     rc = strategy->i2c_reader(fd, slave_address, read_bytewise, bytect, readbuf);
     assert (rc <= 0);

     if (rc == 0) {
        DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Bytes read: %s", hexstring_t(readbuf, bytect) );
     }
     DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");
     return rc;
}


void init_i2c_strategy_func_name_table() {
   RTTI_ADD_FUNC(invoke_i2c_reader);
   RTTI_ADD_FUNC(invoke_i2c_writer);
}

