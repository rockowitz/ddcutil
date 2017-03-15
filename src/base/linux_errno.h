/* linux_errno.h
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

/** \file
 * Linux errno descriptions
 */

#ifndef BASE_LINUX_ERRNO_H_
#define BASE_LINUX_ERRNO_H_

#include "base/status_code_mgt.h"

void init_linux_errno();

// Interpret system error number
// not thread safe, always points to same internal buffer, contents overwritten on each call
char * linux_errno_name(int error_number);
char * linux_errno_desc(int error_number);

Status_Code_Info * get_errno_info(int errnum);
Status_Code_Info * get_negative_errno_info(int errnum);

bool errno_name_to_number(const char * errno_name, int * perrno);
bool errno_name_to_modulated_number(const char * errno_name, Global_Status_Code * p_error_number);

#endif /* BASE_LINUX_ERRNO_H_ */
