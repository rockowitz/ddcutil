// i2c_testutil.h

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>


#ifndef I2C_TESTUTIL_H_
#define I2C_TESTUTIL_H_

bool i2c_verify_functions_supported(int busno, char * write_func_name, char * read_func_name);


#endif /* I2C_TESTUTIL_H_ */
