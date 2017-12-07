/* app_dumpload.h
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

/** \f
 *
 */

#ifndef APP_DUMPLOAD_H_
#define APP_DUMPLOAD_H_

#include <base/displays.h>
#include <base/status_code_mgt.h>

bool loadvcp_by_file(const char * fn, Display_Handle * dh);

Public_Status_Code dumpvcp_as_file(Display_Handle * dh, char * optional_filename);

#ifdef OLD
bool dumpvcp_as_file_old(Display_Handle * dh, char * optional_filename);
#endif

#endif /* APP_DUMPLOAD_H_ */
