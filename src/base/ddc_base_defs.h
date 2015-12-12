/* ddc_base_defs.h
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

#ifndef DDC_BASE_DEFS_H_
#define DDC_BASE_DEFS_H_

#include "util/coredefs.h"

typedef struct {
    Byte  major;
    Byte  minor;
} Version_Spec;

typedef enum {I2C_IO_STRATEGY_FILEIO, I2C_IO_STRATEGY_IOCTL} I2C_IO_Strategy_Id;

#endif /* DDC_BASE_DEFS_H_ */
