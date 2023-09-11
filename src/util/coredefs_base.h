/** \file coredefs_base.h
 *  Portion of coredefs.h shared with ddcui
 */

// Copyright (C) 2021-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef COREDEFS_BASE_H_
#define COREDEFS_BASE_H_

#include "config.h"          // for TARGET_BSD, TARGET_LINUX

#include <errno.h>           // to check if EBUSY defined

#ifndef EBUSY
#define EBUSY 16             // not defined in at least EPEL 8, RHEL 8 (9/11/2023)
#endif

/** Raw byte
 */
typedef unsigned char Byte;

#ifndef ARRAY_SIZE
/** Number of entries in array
 */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef ASSERT_IFF
#define ASSERT_IFF(_cond1, _cond2) \
   assert( ( (_cond1) && (_cond2) ) || ( !(_cond1) && !(_cond2) ) )
#endif

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


#define SETCLR_BIT(_flag_var, _bit, _onoff) \
   do { \
      if ( (_onoff) ) \
        _flag_var |= _bit; \
      else \
        _flag_var &= ~_bit; \
   } while(0)


#define FREE(_ptr) \
   do { \
      if (_ptr) { \
         free(_ptr); \
         _ptr = NULL; \
      } \
   } while (0)


#ifdef TARGET_BSD
#define I2C "iic"
#else
#define I2C "i2c"
#endif

#endif /* COREDEFS_BASE_H_ */
