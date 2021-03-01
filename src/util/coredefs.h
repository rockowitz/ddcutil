/** @file coredefs.h
 *  Basic definitions.that re not application specific
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
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

#define ASSERT_IFF(_cond1, _cond2) \
   assert( ( (_cond1) && (_cond2) ) || ( !(_cond1) && !(_cond2) ) )


// Workaround for coverity complaints re g_strlcpy(), g_strlcat()

#define STRLCPY0(_dst, _src, _size) \
   do \
{ size_t ct = g_strlcpy(_dst, _src, _size); assert(ct < _size);} \
   while(0)

#define STRLCPY9(_dst, _src, _size) \
   do { \
         size_t maxcpy = ( sizeof(_dst) < _size ) ? sizeof(_dst) : _size; \
         size_t ndx = 0;                                                   \
         for ( ; ndx < maxcpy-1 && _src[ndx]; ndx++) {                     \
            _dst[ndx] =  _src[ndx];                                        \
         };                                                                \
         _dst[ndx] = '\0';                                                  \
   } while(0)


#define STRLCPY11(_dst, _src, _size)  \
   do {                             \
     assert(sizeof(_dst) >= _size); \
     strncpy(_dst, _src, _size);    \
     _dst[_size-1] = '\0';          \
   } while (0)


#define STRLCPY12(_dst, _src, _size) \
   /* coverity[OVERRUN, index_parm, overrun-call, access_debuf_const,CHECKED_RETURN] */ \
   (void) g_strlcpy(_dst, _src, _size)

// works
#define STRLCPY2(_dst, _src, _size)   (void) g_strlcpy(_dst, _src, _size)
// works
#define STRLCPY3(_dst, _src, _size)  /* coverity[OVERRUN] */  (void) g_strlcpy(_dst, _src, _size)
//works
#define STRLCPY4(_dst, _src, _size)  /* coverity[overrun-buffer-val] */  (void) g_strlcpy(_dst, _src, _size)
// works
#define STRLCPY5(_dst, _src, _size)  /* coverity[access_debuf_const] */  (void) g_strlcpy(_dst, _src, _size)
// fails:
#define STRLCPY6(_dst, _src, _size)  /* coverity[OVERRUN] */ /* coverity[CHECKED_RETURN] */ g_strlcpy(_dst, _src, _size)
// seems to work:
#define STRLCPY7(_dst, _src, _size)  /* coverity[OVERRUN, CHECKED_RETURN] */  g_strlcpy(_dst, _src, _size)


#ifdef REF
 *    /* coverity[OVERRUN] */ (void) g_strlcpy(result.mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);
   /* coverity[overrun-buffer-val] */  (void) g_strlcpy(result.mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);
   /* coverity[access_debuf_const] */ (void) g_strlcpy(result.mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);
 *
#endif

#ifdef NO
#define STRLCAT(_dst, _src, _size) \
   do \
{ size_t ct = g_strlcat(_dst, _src, _size); assert(ct < _size);} \
   while(0)
#endif
#define STRLCAT(_dst, _src, _size) \
   /* coverity[index_parm] */ g_strlcat(_dst, _src, _size)



#define STRLCPY(_dst, _src, _size) \
do { \
   assert(sizeof(_dst) >= 1); \
   strncpy(_dst, _src, _size); _dst[sizeof(_dst)-1] = '\0'; \
} while(0)

#ifdef TARGET_BSD
#define I2C "iic"
#else
#define I2C "i2c"
#endif

#endif /* COREDEFS_H_ */
