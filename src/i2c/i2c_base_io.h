/* i2c_base_io.h
 *
 *  Low level functions for writing to and reading from the I2C bus,
 *  using various mechanisms.
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
 *  Low level functions for writing to and reading from the I2C bus,
 *  using various mechanisms.
 */

#ifndef I2C_BASE_IO_H_
#define I2C_BASE_IO_H_

#include "util/coredefs.h"

#include "base/status_code_mgt.h"


/** Function template for I2C write function */
typedef Status_Errno_DDC (*I2C_Writer)(int fh, int bytect, Byte * bytes_to_write);
/** Function template for I2C read function */
typedef Status_Errno_DDC (*I2C_Reader)(int fh, int bytect, Byte * readbuf);

Status_Errno_DDC write_writer(int fh, int bytect, Byte * pbytes);
Status_Errno_DDC read_reader (int fh, int bytect, Byte * readbuf);
Status_Errno_DDC ioctl_writer(int fh, int bytect, Byte * pbytes);
Status_Errno_DDC ioctl_reader(int fh, int bytect, Byte * readbuf);

// Don't work:
Status_Errno_DDC i2c_smbus_write_i2c_block_data_writer(int fh, int bytect, Byte * bytes_to_write);
Status_Errno_DDC i2c_smbus_read_i2c_block_data_reader(int fh, int bytect, Byte * readbuf);

#endif /* I2C_BASE_IO_H_ */
