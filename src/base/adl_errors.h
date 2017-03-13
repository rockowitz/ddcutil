/*adl_errors.h
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
 * ADL Error Number Services
 */

#ifndef ADL_ERRORS_H_
#define ADL_ERRORS_H_

#include "base/status_code_mgt.h"

void init_adl_errors() ;

Status_Code_Info * get_adl_status_description(int errnum);

bool adl_error_name_to_number(const char * adl_error_name, int * adl_error_number);
bool adl_errno_name_to_modulated_number(const char * error_name, Global_Status_Code * p_error_number);

#endif /* ADL_ERRORS_H_ */
