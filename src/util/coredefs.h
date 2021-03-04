/** @file coredefs.h
 *  Basic definitions.that re not application specific
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef COREDEFS_H_
#define COREDEFS_H_

#include "config.h"
#include "coredefs_base.h"

// Workaround for coverity complaints re g_strlcpy(), g_strlcat()

#define STRLCPY(_dst, _src, _size) \
do { \
   assert(sizeof(_dst) >= 1); \
   strncpy(_dst, _src, _size); _dst[sizeof(_dst)-1] = '\0'; \
} while(0)

#define STRLCAT(_dst, _src, _size) \
   /* coverity[index_parm] */ (void) g_strlcat(_dst, _src, _size)


#ifdef ALTERNATIVES
#define STRLCPY11(_dst, _src, _size)  \
   do {                             \
     assert(sizeof(_dst) >= _size); \
     strncpy(_dst, _src, _size);    \
     _dst[_size-1] = '\0';          \
   } while (0)


#define STRLCPY12(_dst, _src, _size) \
   /* coverity[OVERRUN, index_parm, overrun-call, access_debuf_const,CHECKED_RETURN] */ \
   (void) g_strlcpy(_dst, _src, _size)


#endif


#ifdef TARGET_BSD
#define I2C "iic"
#else
#define I2C "i2c"
#endif

#endif /* COREDEFS_H_ */
