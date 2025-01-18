/** @file i2c_sysfs_conflicting_drivers.h */

// Copyright (C) 2020-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_SYSFS_CONFLICTING_DRIVERS_H_
#define I2C_SYSFS_CONFLICTING_DRIVERS_H_

#include <glib-2.0/glib.h>

#include "util/coredefs.h"

typedef struct {
   int     i2c_busno;
   char *  n_nnnn;
   char *  name;            // n_nnnn/name
   char *  driver_module;   // basename(realpath(n_nnnn/driver/module))
   char *  modalias;        // n_nnnn/modalias
   Byte *  eeprom_edid_bytes;
   gsize   eeprom_edid_size;
} Sys_Conflicting_Driver;

GPtrArray * collect_conflicting_drivers(int busno, int depth);
GPtrArray * collect_conflicting_drivers_for_any_bus(int depth);
void        report_conflicting_drivers(GPtrArray * conflicts, int depth);   // for a single busno
void        free_conflicting_drivers(GPtrArray* conflicts);
GPtrArray * conflicting_driver_names(GPtrArray * conflicts);
char *      conflicting_driver_names_string_t(GPtrArray * conflicts);
void        init_i2c_sysfs_conflicting_drivers();

#endif /* I2C_SYSFS_CONFLICTING_DRIVERS_H_ */
