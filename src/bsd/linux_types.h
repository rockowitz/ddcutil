/** \f linux_types.h
*
*  Integer type definitions needed by private copies of
*  i2c.h, i2c-dev.h.  Used if TARGET_BSD
*/

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LINUX_TYPES_H_
#define LINUX_TYPES_H_

#include <inttypes.h>

#include <sys/types.h>  // n.b. BSD version

typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;

#endif /* LINUX_TYPES_H_ */
