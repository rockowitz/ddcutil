/** \file coredefs_base.h
 *  Portion of coredef.h shared with ddcui
 */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef COREDEFS_BASE_H_
#define COREDEFS_BASE_H_


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

#endif /* COREDEFS_BASE_H_ */
