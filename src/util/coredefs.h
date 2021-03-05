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

// i give up
#define STRLCPY(_dst, _src, _size)  \
   /* coverity[access_dbuf_in_call] */ (void) g_strlcpy( (_dst), (_src), (_size) )



// Workaround for coverity complaints re g_strlcpy(), g_strlcat()
// there's a problem here, string was truncated
#define STRLCPY0(_dst, _src, _size) \
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
