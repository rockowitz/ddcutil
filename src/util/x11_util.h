/** @file x11_util.h  Utilities for X11
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef X11_UTIL_H_
#define X11_UTIL_H_

/** \cond */
#include <glib-2.0/glib.h>
/** \endcond */

#include "coredefs.h"

/** Represents one EDID known to X11 */
typedef struct {
   char * output_name;   ///< RandR output name
   Byte * edidbytes;     ///< pointer to 128 byte EDID
} X11_Edid_Rec;

GPtrArray * get_x11_edids();   // returns array of X11_Edid_Rec
void        free_x11_edids(GPtrArray * edidrecs);
bool        get_x11_dpms_info(unsigned short * power_level, unsigned char * state);
const char* dpms_power_level_name(unsigned short power_level);

#endif /* X11_UTIL_H_ */
