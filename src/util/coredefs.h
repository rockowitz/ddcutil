/** @file coredefs.h
 *  Basic definitions.that are not application specific
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef COREDEFS_H_
#define COREDEFS_H_

#include <glib-2.0/glib.h>   // for MIN()
#include <string.h>          // for memcpy(), strlen()

#include "config.h"          // for TARGET_BSD, TARGET_LINUX
#include "coredefs_base.h"   // shared with ddcui

/* gcc 8 introduced stringop- warnings: stringop-truncation, stringop-overflow, (more?)
 * These cause warnings for otherwise valid strncpy(), strncat(), g_strlcpy()  code that
 * appears only when using gcc 8, not 7 or earlier.  Writing code that compiles without
 * warnings everywhere, and in addition does not raise a Coverity defect
 * (e.g. access_dbuf_in_call) is difficult. See:
 * https://stackoverflow.com/questions/50198319/gcc-8-wstringop-truncation-what-is-the-good-practice
 *
 * Using:
 *    #pragma GCC diagnostic ignored "-Wstringop-truncation"
 * suppresses the warning on GCC 8, but causes an unrecognized pragma error on gcc  7.
 *
 * Surrounding the strndup() expression in parentheses, as suggested in the stackoverflow
 * thread, works for gcc 7 and 8, but still triggers the Coverity Scan diagnostics, which
 * requires a coverity annotation, e.g. "converity[access_dbuf_in_call]"/
 *
 * Using memcpy() avoids all the warnings, but has the drawback that STRLCPY(), unlike
 * strndup() etc., does not have a return value, so is not a drop-in replacement.
 * However, in practice, that return value is not used by callers.
 */

#define STRLCPY(_dst, _src, _size) \
do { \
  memcpy(_dst, _src, MIN((_size)-1,strlen(_src))); \
  _dst[ MIN((_size)-1,strlen(_src)) ] = '\0'; \
} while(0)


#define STRLCAT(_dst, _src, _size) \
do { \
   strncat(_dst, _src, (_size)-1); \
  _dst[(_size)-1] = '\0'; \
} while(0)


#ifdef TARGET_BSD
#define I2C "iic"
#else
#define I2C "i2c"
#endif

#endif /* COREDEFS_H_ */
