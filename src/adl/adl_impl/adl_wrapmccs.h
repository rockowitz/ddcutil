/* adl_wrapmccs.h
 *
 * Created on: Oct 18, 2015
 *     Author: rock
 *
 *  File mccs.h from the ADL SDK lacks ifdef tests, which
 *  can cause double inclusion resulting in errors.  This
 *  file should be included wherever mccs.h is needed.
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

#ifndef ADL_WRAPMCCS_H_
#define ADL_WRAPMCCS_H_

typedef void * HMODULE;    // needed by mccs.h
#include <mccs.h>

#endif /* ADL_WRAPMCCS_H_ */
