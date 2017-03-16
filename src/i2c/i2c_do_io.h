/* i2c_do_io.h
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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
 *
 */

#ifndef I2C_DO_IO_H_
#define I2C_DO_IO_H_

#include "util/coredefs.h"

#include "base/execution_stats.h"
#include "base/status_code_mgt.h"

#include "i2c/i2c_base_io.h"

/** Describes one I2C IO strategy */
typedef struct {
   I2C_Writer i2c_writer;          ///< writer function
   I2C_Reader i2c_reader;          ///< read function
   char *     i2c_writer_name;     ///< write function name
   char *     i2c_reader_name;     ///< read function name
} I2C_IO_Strategy;

// may need to move this definition to base
/** I2C IO strategy ids */
typedef enum {
   I2C_IO_STRATEGY_FILEIO,    ///< use file write() and read()
   I2C_IO_STRATEGY_IOCTL}     ///< use ioctl(I2C_RDWR)
I2C_IO_Strategy_Id;

void i2c_set_io_strategy(I2C_IO_Strategy_Id strategy_id);

Status_Errno_DDC invoke_i2c_writer(
      int    fh,
      int    bytect,
      Byte * bytes_to_write);

Status_Errno_DDC invoke_i2c_reader(
       int        fh,
       int        bytect,
       Byte *     readbuf);

#ifdef TEST_THAT_DIDNT_WORK
Status_Errno_DDC invoke_single_byte_i2c_reader(
       int        fh,
       int        bytect,
       Byte *     readbuf);
#endif

#endif /* I2C_DO_IO_H_ */
