/* i2c_io_old.h
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

#ifndef PROBEI2C_I2C_IO
#define PROBEI2C_I2C_IO

#include <stdbool.h>

#include "base/core.h"
#include "base/execution_stats.h"
#include "base/status_code_mgt.h"

#include "i2c/i2c_base_io.h"

// was in common.h
#define MAX_I2C_MESSAGE_SIZE   131    // 127 + 4;

void set_i2c_write_mode(char* mode);

void set_i2c_read_mode(char* mode);


Status_Errno_DDC call_i2c_writer(
      I2C_Writer writer,
      char * writer_name,
      int    fh,
      int    bytect,
      Byte * bytes_to_write,
      int    sleep_millisec)
;


Status_Errno_DDC call_i2c_reader(
       I2C_Reader reader,
       char *     reader_name,
       int        fh,
       int        bytect,
       Byte *     readbuf,
       int        sleep_millisec);


//
// Write to I2C bus
//

Status_Errno_DDC do_i2c_file_write(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec);

Status_Errno_DDC do_i2c_smbus_write_i2c_block_data(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec);

Status_Errno_DDC do_i2c_ioctl_write(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec);

Status_Errno_DDC perform_i2c_write(int fh, char * write_mode, int bytect, Byte * bytes_to_write, int sleep_millisec);

Status_Errno_DDC perform_i2c_write2(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec);

//
// Read from I2C bus
//

Status_Errno_DDC do_i2c_file_read(int fh, int bytect, Byte * readbuf, int sleep_millisec);

Status_Errno_DDC do_i2c_smbus_read_i2c_block_data(int fh, int bytect, Byte * readbuf, int sleep_millisec);

Status_Errno_DDC do_i2c_ioctl_read(int fh, int bytect, Byte * readbuf, int sleep_millisec);

Status_Errno_DDC perform_i2c_read(
                 int    fh,
                 char * read_mode,
                 int    bytect,
                 Byte * readbuf,
                 int    sleep_millisec
                );

Status_Errno_DDC perform_i2c_read2(int fh, int bytect, Byte * readbuf, int sleep_millisec);



#endif
