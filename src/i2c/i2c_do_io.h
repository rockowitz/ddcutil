/* i2c_do_io.h
 *
 * Created on: Nov 17, 2015
 *     Author: rock
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

#ifndef I2C_DO_IO_H_
#define I2C_DO_IO_H_

#include "util/coredefs.h"

#include "base/execution_stats.h"
#include "base/status_code_mgt.h"

#include "i2c/i2c_base_io.h"


typedef struct {
   I2C_Writer i2c_writer;
   I2C_Reader i2c_reader;
   char *     i2c_writer_name;
   char *     i2c_reader_name;
} I2C_IO_Strategy;

void i2c_set_io_strategy(I2C_IO_Strategy_Id strategy_id);

Global_Status_Code invoke_i2c_writer(
      int    fh,
      int    bytect,
      Byte * bytes_to_write);

Global_Status_Code invoke_i2c_reader(
       int        fh,
       int        bytect,
       Byte *     readbuf);

#endif /* I2C_DO_IO_H_ */
