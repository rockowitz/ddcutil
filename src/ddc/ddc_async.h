/* ddc_async.h
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

#ifndef DDC_ASYNC_H_
#define DDC_ASYNC_H_

#include "public/ddcutil_types.h"

#include "util/coredefs.h"
#include "util/error_info.h"

#include "base/displays.h"

Error_Info *
start_get_vcp_value(
       Display_Handle *          dh,
       Byte                      feature_code,
       DDCA_Vcp_Value_Type       call_type,
       DDCA_Notification_Func     callback_func);

#endif /* DDC_ASYNC_H_ */
