/* ddc_errno.h
 *
 * Error codes internal to the application, which are
 * primarily ddcutil related.
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
 * Error codes internal to **ddcutil**.
 */


#ifndef DDC_ERRNO_H_
#define DDC_ERRNO_H_

#include "public/ddcutil_status_codes.h"

#include "base/status_code_mgt.h"



Status_Code_Info * ddcrc_find_status_code_info(int rc);

bool ddc_error_name_to_number(const char * errno_name, Status_DDC * perrno);
// bool ddc_error_name_to_modulated_number(const char * errno_name, Global_Status_Code * p_error_number);

// Returns status code description:
char * ddcrc_desc(int rc);   // must be freed after use

bool ddcrc_is_derived_status_code(Public_Status_Code gsc);

bool ddcrc_is_not_error(Public_Status_Code gsc);

#endif /* APP_ERRNO_H_ */
