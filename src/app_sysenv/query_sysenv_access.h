/* @file query_sysenv_access.h
 *
 *  Checks on the the existence of and access to /dev/i2c devices
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef QUERY_SYSENV_ACCESS_H_
#define QUERY_SYSENV_ACCESS_H_

#include "util/data_structures.h"

#include "app_sysenv/query_sysenv_base.h"

Byte_Value_Array identify_i2c_devices();

void check_i2c_devices(Env_Accumulator * accum);

#endif /* QUERY_SYSENV_ACCESS_H_ */
