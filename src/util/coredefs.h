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

#define STRLCPY(_str, _dst, _size) \
   do \
{ size_t ct = g_strlcpy(_str, _dst, _size); assert(ct < _size);} \
   while(0)

#define STRLCPY2(_str, _dst, _size)   (void) g_strlcpy(_str, _dst, _size);

#define STRLCPY3(_str, _dst, _size)  /* coverity[OVERRUN] */  (void) g_strlcpy(_str, _dst, _size);

#define STRLCPY4(_str, _dst, _size)  /* coverity[overrun-buffer-val] */  (void) g_strlcpy(_str, _dst, _size);
#define STRLCPY5(_str, _dst, _size)  /* coverity[access_debuf_const] */  (void) g_strlcpy(_str, _dst, _size);

#ifdef REF
 *    /* coverity[OVERRUN] */ (void) g_strlcpy(result.mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);
   /* coverity[overrun-buffer-val] */  (void) g_strlcpy(result.mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);
   /* coverity[access_debuf_const] */ (void) g_strlcpy(result.mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);
 *
#endif


#define STRLCAT(_str, _dst, _size) \
   do \
{ size_t ct = g_strlcat(_str, _dst, _size); assert(ct < _size);} \
   while(0)



/*
#define STRLCPY(_dst, _src, _size) \
do { \
   strncpy(_dst, _src, _size); _dst[strlen(_dst)] = '\0'; \
} while(0)
*/

#ifdef TARGET_BSD
#define I2C "iic"
#else
#define I2C "i2c"
#endif

#endif /* COREDEFS_H_ */
