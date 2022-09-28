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

#define I2C_STRATEGY_BUSCT_MAX 32


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

static char * strategy_names[] = {
      "I2C_IO_STRATEGY_NOT_SET",
      "I2C_IO_STRATEGY_FILEIO",
      "I2C_IO_STRATEGY_IOCTL"};


char * i2c_io_strategy_id_name(I2C_IO_Strategy_Id id) {
   char * result = strategy_names[id];
   // printf("(%s) id=%d, returning %s\n", __func__, id, result);
   return result;
}


static I2C_IO_Strategy * i2c_io_strategy[I2C_STRATEGY_BUSCT_MAX];    // per bus strategy


#ifdef FUTURE
typedef struct {
   int    busno;
   char * driver_name;
} Per_Driver_Info;


// void (*GDestroyNotify) (gpointer data)

void per_driver_info_destroy_key(gpointer data) {
}

void per_driver_info_destroy_data(gpointer data) {
   Per_Driver_Info * info = data;
   free(data->driver_name);
}


GHashTable * per_driver_hash =NULL;

Per_Driver_Info * get_per_driver_info(int busno) {
   if (!per_driver_hash) {
      per_driver_hash = g_hash_table_new(g_direct_hash, g_int_equal, NULL, per_driver_info_destroy_data);
   }
   Per_Driver_Info * info = g_hash_table_get(GINT_TO_POINTER(busno));
   if (!info) {
      info = calloc(1,sizeof(Per_Driver_Info));
      g_hash_table_insert(per_driver_hash, GINT_TO_POINTER(busno), info);
   }
   return info;
}

Per_Driver_Info * retrieve_driver_for_bus(int busno) {
}

void save_driver_for_bus

#endif


static bool nvidia_einval_bug_encountered = false;

/** Checks a status code to see if it indicates the nvida/i2c-dev driver bug.
 *
 *  It is if the following 3 tests are met:
 *  - the status code is -EINVAL
 *  - the driver name is "nvidia"
 *  - the current io strategy is I2C_IO_STRATEGY_IOCTL
 *
 *  @param strategy_id  current io strategy
 *  @param busno        /dev/i2c-N bus number
 *  @param rc           status code to check
 *  @return             true if all tests are met, false otherwise
 *
 *  If the function returns true:
 *  - global variable nvidia_einval_bug_encountered is set true
 *  - the current IO strategy is set to I2C_IO_STRATEGY_FILEIO
 */
bool
is_nvidia_einval_bug(
      I2C_IO_Strategy_Id  strategy_id,
      int                 busno,
      int                 rc)
{
   bool result = false;
   if ( rc == -EINVAL && strategy_id == I2C_IO_STRATEGY_IOCTL) {
      char * driver_name = get_i2c_sysfs_driver_by_busno(busno);
      if (streq(driver_name, "nvidia")) {
         nvidia_einval_bug_encountered = true;
         i2c_set_initial_io_strategy_by_id(I2C_IO_STRATEGY_FILEIO);   // the new normal
         char * msg = "nvida/i2c-dev bug encountered. Forcing future io I2C_IO_STRATEGY_FILEIO. Retrying";
         DBGTRC(true, TRACE_GROUP, msg);
         syslog(LOG_WARNING, msg);
         result = true;
      }
      free(driver_name);
   }
   return result;
}

static I2C_IO_Strategy * initial_i2c_io_strategy = &i2c_ioctl_io_strategy;

/** Sets an alternative I2C IO strategy.
 *
 * @param strategy_id  I2C IO strategy id
 */
void
i2c_set_initial_io_strategy_by_id(I2C_IO_Strategy_Id strategy_id) {
   bool debug = true;
   assert(strategy_id != I2C_IO_STRATEGY_NOT_SET);
   DBGMSF(debug, "Starting. id=%d", strategy_id);

   switch (strategy_id) {
   case (I2C_IO_STRATEGY_NOT_SET):
         PROGRAM_LOGIC_ERROR("Impossible case");
         initial_i2c_io_strategy = NULL;
         break;
   case (I2C_IO_STRATEGY_FILEIO):
         initial_i2c_io_strategy = &i2c_file_io_strategy;
         break;
   case (I2C_IO_STRATEGY_IOCTL):
         initial_i2c_io_strategy= &i2c_ioctl_io_strategy;
         break;
   }

   DBGMSF(debug, "Done. Set strategy: %s", initial_i2c_io_strategy->strategy_name);
}


void i2c_set_io_strategy_for_device(
      int busno,
      I2C_IO_Strategy_Id strategy_id)
{
   bool debug = true;
   assert(strategy_id != I2C_IO_STRATEGY_NOT_SET);
   DBGMSF(debug, "Starting. id=%d", strategy_id);

   I2C_IO_Strategy * strategy = NULL;

   switch (strategy_id) {
   case (I2C_IO_STRATEGY_NOT_SET):
         PROGRAM_LOGIC_ERROR("Impossible case");
         strategy = NULL;
         break;
   case (I2C_IO_STRATEGY_FILEIO):
         strategy = &i2c_file_io_strategy;
         break;
   case (I2C_IO_STRATEGY_IOCTL):
         strategy= &i2c_ioctl_io_strategy;
         break;
   }
   i2c_io_strategy[busno] = strategy;

   DBGMSF(debug, "Done. Set strategy: for busno %d %s", busno, initial_i2c_io_strategy->strategy_name);
}


#ifdef UNUSED
/** Gets the current io strategy.
 *  Assigns the default strategy if not already set
 */
I2C_IO_Strategy_Id
i2c_get_current_io_strategy_id() {
   bool debug = true;
   if (i2c_io_strategy == I2C_IO_STRATEGY_NOT_SET )
   {
      i2c_set_initial_io_strategy_by_id(I2C_IO_STRATEGY_IOCTL);
      DBGMSF(debug, "Applied default strategy I2C_IO_STRATEGY_IOCTL...");
   }
   I2C_IO_Strategy_Id result = i2c_io_strategy->strategy_id;
   return result;
}
#endif


#ifdef UNUSED
// unused, this has be a duplicate of some other code
int busno_from_fd(int fd) {
   int busno = extract_number_after_hyphen(filename_for_fd_t(fd));
   return busno;
}
#endif


/** Gets the strategy to be used on the next read or write
 *
 *  @param device-name   e.g. /dev/i2c-1
 *  @return              pointer to strategy record
 */
I2C_IO_Strategy *
i2c_get_io_strategy_by_device_name(char * device_name) {
   bool debug = true;
   DBGMSF(debug, "Starting. device_name = %s", device_name);

   int busno = extract_number_after_hyphen(device_name);
   assert(busno >= 0 && busno < I2C_STRATEGY_BUSCT_MAX);

   if (!i2c_io_strategy[busno]) {
      i2c_set_io_strategy_for_device(busno, initial_i2c_io_strategy->strategy_id);
      DBGMSF(debug, "Applied default strategy: %s", initial_i2c_io_strategy->strategy_name);
   }

   DBGMSF(debug, "Returning strategy %s", i2c_io_strategy[busno]->strategy_name);
   return i2c_io_strategy[busno];
}


/** Gets the strategy to be used on the next read or write
 *
 *  @param device-name   e.g. /dev/i2c-1
 *  @return              strategy id
 */
I2C_IO_Strategy_Id
i2c_get_io_strategy_id_by_device_name(char * device_name) {
   bool debug = true;
   DBGMSF(debug, "device_name=|%s|", device_name);
   I2C_IO_Strategy_Id id = i2c_get_io_strategy_by_device_name(device_name)->strategy_id;
   DBGMSF(debug, "Returning %d", id);
   return id;
}


#ifdef UNUSED
I2C_IO_Strategy_Id
i2c_get_calculated_io_strategy_id(char * device_name) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "device_name = %s", device_name);
   I2C_IO_Strategy_Id id =  get_i2c_io_strategy_by_device_name(device_name)->strategy_id;
   DBGTRC_DONE(debug, TRACE_GROUP, "Done.  Returning %s", i2c_io_strategy_id_name(id));
   return id;
}
#endif


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
   bool debug = true;
   DBGTRC_STARTING(debug, TRACE_GROUP,
                 "fd=%d, filename=%s, slave_address=0x%02x, bytect=%d, bytes_to_write=%p -> %s",
                 fd,
                 filename_for_fd_t(fd),
                 slave_address,
                 bytect,
                 bytes_to_write,
                 hexstring_t(bytes_to_write, bytect));

retry:
   I2C_IO_Strategy * strategy = i2c_get_io_strategy_by_device_name(filename_for_fd_t(fd));
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "strategy = %s", strategy->strategy_name);
   Status_Errno_DDC rc = strategy->i2c_writer(fd, slave_address, bytect, bytes_to_write);
   if (rc == -EINVAL) {
      int busno = extract_number_after_hyphen(filename_for_fd_t(fd));
      if (busno >= 0) {    // guard against pathological case
         if (is_nvidia_einval_bug(strategy->strategy_id, busno, rc)) {
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

retry:
     I2C_IO_Strategy * strategy = i2c_get_io_strategy_by_device_name(filename_for_fd_t(fd));
     DBGTRC_NOPREFIX(debug, TRACE_GROUP, "strategy = %s", strategy->strategy_name);
     Status_Errno_DDC rc = strategy->i2c_reader(fd, slave_address, read_bytewise, bytect, readbuf);
     assert (rc <= 0);

     if (rc == -EINVAL) {
        int busno = extract_number_after_hyphen(filename_for_fd_t(fd));
        if (busno >= 0) {    // guard against pathological case
           if (is_nvidia_einval_bug(strategy->strategy_id, busno, rc)) {
              goto retry;
           }
        }
     }
     if (rc == 0) {
        DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Bytes read: %s", hexstring_t(readbuf, bytect) );
     }
     DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");
     return rc;
}


void init_i2c_strategy_func_name_table() {
   RTTI_ADD_FUNC(invoke_i2c_reader);
   RTTI_ADD_FUNC(invoke_i2c_writer);

   // quick and dirty
   for (int ndx = 0; ndx < I2C_STRATEGY_BUSCT_MAX; ndx++) {
      i2c_io_strategy[ndx] = NULL;
   }
}

