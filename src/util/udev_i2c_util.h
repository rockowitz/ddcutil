/* udev_i2c_util.h
 *
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

#ifndef UDEV_I2C_UTIL_H_
#define UDEV_I2C_UTIL_H_

#include <glib.h>
#include <stdbool.h>

#include "data_structures.h"

GPtrArray * get_i2c_devices_using_udev();
void report_i2c_device_summaries(GPtrArray * summaries, char * title, int depth) ;
bool is_smbus_device_summary(GPtrArray * summaries, char * sbusno) ;

Byte_Value_Array get_i2c_devices_as_bva_using_udev();

#endif /* UDEV_I2C_UTIL_H_ */
