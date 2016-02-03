/* ddc_edid.h
 *
 * Created on: Dec 31, 2015
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef SRC_DDC_DDC_EDID_H_
#define SRC_DDC_DDC_EDID_H_

#include "base/displays.h"

Parsed_Edid* ddc_get_parsed_edid_by_display_handle(Display_Handle * dh);
Parsed_Edid* ddc_get_parsed_edid_by_display_ref(Display_Ref * dref);

#endif /* SRC_DDC_DDC_EDID_H_ */
