/* linux_errno.h
 *
 * Created on: Nov 4, 2015
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

#ifndef LINUX_ERRNO_H_
#define LINUX_ERRNO_H_

#include "base/status_code_mgt.h"

void init_linux_errno();


// Interpret system error number
// not thread safe, always points to same internal buffer, contents overwritten on each call

char * linux_errno_name(int error_number);
char * linux_errno_desc(int error_number);

// char * errno_name(int error_number);

// still used?
char * errno_name_negative(int negative_error_number);

Status_Code_Info * get_errno_info(int errnum);

Status_Code_Info * get_negative_errno_info(int errnum);

#endif /* LINUX_ERRNO_H_ */
