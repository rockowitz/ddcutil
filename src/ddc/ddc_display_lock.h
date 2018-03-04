/* ddc_display_lock.h
 *
 * <copyright>
 * Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#ifndef DDC_DISPLAY_LOCK_H_
#define DDC_DISPLAY_LOCK_H_

#include <stdbool.h>

#include "base/core.h"
#include "base/displays.h"

typedef enum {
   DDISP_WAIT  = 0x01      ///< If true, #lock_distinct_display() should wait
} Distinct_Display_Flags;

typedef void * Distinct_Display_Ref;

void init_ddc_display_lock(void);

Distinct_Display_Ref get_distinct_display_ref(Display_Ref * dref);

bool lock_distinct_display(Distinct_Display_Ref id, Distinct_Display_Flags flags);

void unlock_distinct_display(Distinct_Display_Ref id);

void dbgrpt_distinct_display_descriptors(int depth);

#endif /* DDC_DISPLAY_LOCK_H_ */
