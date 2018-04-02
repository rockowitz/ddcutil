/* i2c_util.c
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

/** \f
 *   I2C Utility Functions
 */

#include "string_util.h"

#include "i2c_util.h"


/** Converts a string of the form "i2c-N" to a number.
 *
 *  \param  name  string to convert
 *  \return extracted number, -1 if conversion fails
 */
int i2c_name_to_busno(char * name) {
   int result = -1;
   if (name && str_starts_with(name, "i2c-")) {
      int ival;
      bool ok = str_to_int2(name+4, &ival, 10);
      if (ok)
         result = ival;
   }
   return result;
}
