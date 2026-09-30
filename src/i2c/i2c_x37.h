/** \file i2c_x37.h
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_X37_H_
#define I2C_X37_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

#include "util/coredefs_base.h"

#include "base/core.h"
#include "base/status_code_mgt.h"

// Set from --skip-ddc-checks: assume x37 responsive without probing.
extern bool skip_x37_detection;

// Detection of slave address x37.  i2c_check_x37_old() is the earlier variant,
// retained for comparison; its only live reference is its RTTI registration.
Status_Errno_DDC i2c_detect_x37(   int fd, char * driver);
Status_Errno_DDC i2c_check_x37_old(int fd, char * driver);

// Probes one address one way.  Used by the exploration functions below.
int  probe_i2c_quick(int fd, Byte addr, bool write, int size);  // I2C_SMBUS_QUICK or I2C_SMBUS_BYTE

// Report how a bus responds to each way of probing an address.
void explore_smbus_quick( int fd);
void explore_smbus_probes(int fd, char * driver);

void init_i2c_x37();

#endif /* I2C_X37_H_ */
