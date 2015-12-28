/* ddc_displays.h
 *
 * Created on: Dec 28, 2015
 *     Author: rock
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

#ifndef SRC_DDC_DDC_DISPLAYS_H_
#define SRC_DDC_DDC_DISPLAYS_H_

#include <glib.h>
#include <stdio.h>
#include <time.h>

#include "base/common.h"
#include "base/ddc_base_defs.h"     // for Version_Spec
#include "base/displays.h"
#include "base/status_code_mgt.h"

#include "ddc/vcp_feature_codes.h"


Display_Ref* get_display_ref_for_display_identifier(Display_Identifier* pdid, bool emit_error_msg);


Display_Info_List * ddc_get_valid_displays();
int ddc_show_active_displays(int depth);

Display_Ref* ddc_find_display_by_dispno(int dispno);


#endif /* SRC_DDC_DDC_DISPLAYS_H_ */
