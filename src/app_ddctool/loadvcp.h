/* loadvcp.h
 *
 * Created on: Aug 16, 2014
 *     Author: rock
 *
 * Load/store VCP settings from/to file.
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

#ifndef LOADVCP_H_
#define LOADVCP_H_

#include <base/status_code_mgt.h>
#include <base/displays.h>

bool loadvcp_from_file(const char * fn);

bool dumpvcp_to_file(Display_Handle * dh, char * optional_filename);
bool dumpvcp_to_file_new(Display_Handle * dh, char * optional_filename);
Global_Status_Code dumpvcp_to_string(Display_Handle * dh, char** result);
Global_Status_Code dumpvcp_to_string_new(Display_Handle * dh, char** result);

Global_Status_Code loadvcp_from_string(char * catenated);

#endif /* LOADVCP_H_ */
