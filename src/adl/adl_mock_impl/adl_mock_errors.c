/* adl_mock_errors.c
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
 * Mock ADL Error Number Services
 */

/** \cond */
#include <stdbool.h>
#include <stddef.h>
/** \endcond */

#include "base/adl_errors.h"

void init_adl_errors() {}

/** Mock implementation if **get_adl_status_description()**.
 *
 *  @param  errnum   unmodulated ADL status code
 *  @retval NULL
 */
Status_Code_Info *
get_adl_status_description(
      Base_Status_ADL errnum)
{
   return NULL;
}

/** Mock implementation of **adl_error_name_to_number()**.
 *
 *  @param  adl_error_name     symbolic name, e.g. ADL_ERR_NOT_SUPPORTED
 *  @param  p_adl_error_number where to return error number
 *  @retval false
 */
bool
adl_error_name_to_number(
      const char *      adl_error_name,
      Base_Status_ADL * p_adl_error_number)
{
   *p_adl_error_number = 0;
   return false;
}


/** Mock implementation of **adl_error_name_to_modulated_number()**.
 *
 *  @param  adl_error_name    symbolic name, e.g. ADL_ERR_NOT_SUPPORTED
 *  @param  p_adl_error_number where to return error number
 *  @retval false
 */
bool
adl_error_name_to_modulated_number(
        const char *           adl_error_name,
        Modulated_Status_ADL * p_adl_error_number)
{
   *p_adl_error_number = 0;
   return false;
}
