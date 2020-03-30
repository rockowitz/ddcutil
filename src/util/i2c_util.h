/** \file i2c_util.h
 *
 * I2C utility functions
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_UTIL_H_
#define I2C_UTIL_H_

int i2c_name_to_busno(char * name);

char * i2c_interpret_functionality_flags(unsigned long functionality);
void   i2c_report_functionality_flags(long functionality, int maxline, int depth);

#endif /* I2C_UTIL_H_ */
