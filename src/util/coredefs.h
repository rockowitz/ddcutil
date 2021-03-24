/** @file coredefs.h
 *  Basic definitions.that re not application specific
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef COREDEFS_H_
#define COREDEFS_H_

#include "config.h"
#include "coredefs_base.h"


// defined in glib:
// #define MIN(a,b) ( ((a) < (b) ? (a) : (b) )

#ifdef NO
#define STRLCPY(_dst, _src, _size) \
   do { \
      int ctr = 0;  \
      while  ( ctr < ((_size)-1) && *(_src[ctr]) {  \
         {_dest[ctr] = _src[ctr] } \
         ctr++; \
       } \
      _dst[ctr] = '\0'; \
   } while (0)
#endif

#define STRLCPY_WORKS_USING_G_STRLCPY(_dst, _src, _size)  \
   /* coverity[access_dbuf_in_call] */ (void) g_strlcpy( (_dst), (_src), (_size) )

// n. may  trigger  -Wstringop-truncation, if so caller must use #pragma to disable
#define STRLCPY(_dst, _src, _size) \
do { \
  /* converity[access_dbuf_in_call] */ strncpy(_dst, _src, (_size)-1); \
  _dst[(_size)-1] = '\0'; \
} while(0)




// Workaround for coverity complaints re g_strlcpy(), g_strlcat()
// there's a problem here, string was truncated
#define STRLCPY0(_dst, _src, _size) \
do { \
   assert(sizeof(_dst) >= 1); \
   strncpy(_dst, _src, _size); _dst[sizeof(_dst)-1] = '\0'; \
} while(0)

#define STRLCAT_WORKS_FOR_COVERITY(_dst, _src, _size) \
   /* coverity[index_parm] */ (void) g_strlcat(_dst, _src, _size)

#define STRLCAT(_dst, _src, _size) \
do { \
   strncat(_dst, _src, (_size)-1); \
  _dst[(_size)-1] = '\0'; \
} while(0)



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
