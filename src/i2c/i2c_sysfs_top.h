/** @file i2c_sysfs_top.h */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_SYSFS_TOP_H_
#define I2C_SYSFS_TOP_H_

void consolidated_i2c_sysfs_report(int depth);
bool is_sysfs_unreliable(int busno);

#endif /* I2C_SYSFS_TOP_H_ */
