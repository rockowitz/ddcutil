/* query_sysenv_sysfs.h
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \f
 *  Query environment using /sys file system
 */

#ifndef QUERY_SYSENV_SYSFS_H_
#define QUERY_SYSENV_SYSFS_H_

#include "query_sysenv_base.h"

typedef struct {
   ushort   vendor_id;
   ushort   device_id;
   ushort   subdevice_id;    // subsystem device id
   ushort   subvendor_id;    // subsystem vendor id
} Device_Ids;

void query_card_and_driver_using_sysfs(Env_Accumulator * accum);
void query_loaded_modules_using_sysfs();
void query_sys_bus_i2c(Env_Accumulator * accum);
void query_drm_using_sysfs();

#endif /* QUERY_SYSENV_SYSFS_H_ */
