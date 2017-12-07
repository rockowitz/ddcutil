/* ddc_read_capabilities.h
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \file */

#ifndef DDC_READ_CAPABILITIES_H_
#define DDC_READ_CAPABILITIES_H_

#include "base/ddc_error.h"
#include "base/displays.h"
#include "base/status_code_mgt.h"

// Get capability string for monitor.

Ddc_Error *
get_capabilities_string(
      Display_Handle * dh,
      char**           pcaps);

#endif /* DDC_READ_CAPABILITIES_H_ */
