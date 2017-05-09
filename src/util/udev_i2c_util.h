/* udev_i2c_util.h
 *
 * <copyright>
 * Copyright (C) 2016-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \file *
 *  I2C specific udev utilities
 */

#ifndef UDEV_I2C_UTIL_H_
#define UDEV_I2C_UTIL_H_

/** \cond */
#include <glib.h>
#include <stdbool.h>
/** \endcond */

#include "data_structures.h"
#include "udev_util.h"

GPtrArray *                     // array of Udev_Device_Summary
get_i2c_devices_using_udev();
int udev_i2c_device_summary_busno(Udev_Device_Summary * summary);

bool
is_smbus_device_summary(
      GPtrArray * summaries,    // array of Udev_Device_Summary
      char *      sbusno) ;

void
report_i2c_udev_device_summaries(
      GPtrArray * summaries,    // array of Udev_Device_Summary
      char *      title,
      int         depth) ;


Byte_Value_Array                // one byte for each I2C bus number
get_i2c_device_numbers_using_udev(bool include_smbus);

#endif /* UDEV_I2C_UTIL_H_ */
