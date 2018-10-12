/** @file query_sysenv_i2c.h
 *
 * Check I2C devices using directly coded I2C calls
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef QUERY_SYSENV_I2C_H_
#define QUERY_SYSENV_I2C_H_

#include "query_sysenv_base.h"


void raw_scan_i2c_devices(Env_Accumulator * accum);
void query_i2c_buses();

#endif /* QUERY_SYSENV_I2C_H_ */
