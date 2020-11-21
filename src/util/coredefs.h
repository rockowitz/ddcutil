/** @file coredefs.h
 *  Basic definitions.that re not application specific
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef COREDEFS_H_
#define COREDEFS_H_

#include "config.h"

/** Raw byte
 */
typedef unsigned char Byte;

#ifndef ARRAY_SIZE
/** Number of entries in array
 */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifdef TARGET_BSD
#define I2C "iic"
#else
#define I2C "i2c"
#endif

#endif /* COREDEFS_H_ */
