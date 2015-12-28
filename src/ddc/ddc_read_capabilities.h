/* ddc_read_capabilities.h
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

#ifndef SRC_DDC_DDC_READ_CAPABILITIES_H_
#define SRC_DDC_DDC_READ_CAPABILITIES_H_

#include <glib.h>
#include <stdio.h>
#include <time.h>

#include "base/common.h"
#include "base/ddc_base_defs.h"     // for Version_Spec
#include "base/displays.h"
#include "base/status_code_mgt.h"

#include "ddc/vcp_feature_codes.h"


// Get capability string for monitor.
Global_Status_Code get_capabilities_buffer_by_display_handle(Display_Handle * dh, Buffer** ppCapabilitiesBuffer);
Global_Status_Code get_capabilities_string_by_display_handle(Display_Handle * dh, char** pcaps);
Global_Status_Code get_capabilities_buffer_by_display_ref(Display_Ref * pdisp, Buffer** ppCapabilitiesBuffer);
Global_Status_Code get_capabilities_string_by_display_ref(Display_Ref * pdisp, char** pcaps);

#endif /* SRC_DDC_DDC_READ_CAPABILITIES_H_ */
