/* ddcutil_c_api_private.h
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

#ifndef DDCUTIL_C_API_PRIVATE_H_
#define DDCUTIL_C_API_PRIVATE_H_

#include "public/ddcutil_types.h"
#include "private/ddcutil_types_private.h"

// Declarations of API functions that haven't yet been published or that have
// been removed from ddcutil_c_api.h

#ifdef OLD
/**
 *  Frees the contents a #DDCA_Feature_Metadata instance.
 *
 *  Note that #DDCA_Feature_Metadata instances are typically
 *  allocated on the stack,  This function frees any data
 *  pointed to by the metadata instance, not the instance itself.
 *
 *  @param[in] info  metadata instance, passed by value
 *  @return    status code
 *
 *  @since 0.9.0
 */
__attribute__ ((deprecated ("use ddca_free_feature_metadata()")))
DDCA_Status
ddca_free_feature_metadata_contents(DDCA_Feature_Metadata info);

#endif

#endif /* DDCUTIL_C_API_PRIVATE_H_ */
