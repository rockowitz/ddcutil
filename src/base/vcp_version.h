/* vcp_version_spec.h
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef VCP_VERSION_H_
#define VCP_VERSION_H_

#include <stdbool.h>

#include "util/coredefs.h"


typedef struct {
    Byte  major;
    Byte  minor;
} Version_Spec;

extern const Version_Spec VCP_SPEC_V20;
extern const Version_Spec VCP_SPEC_V21;
extern const Version_Spec VCP_SPEC_V30;
extern const Version_Spec VCP_SPEC_V22;
extern const Version_Spec VCP_SPEC_ANY;
extern const Version_Spec VCP_SPEC_UNKNOWN;
extern const Version_Spec VCP_SPEC_UNQUERIED;

bool vcp_version_le(Version_Spec val, Version_Spec max);
bool vcp_version_gt(Version_Spec val, Version_Spec min);
bool vcp_version_eq(Version_Spec v1,  Version_Spec v2);

bool is_vcp_version_unqueried(Version_Spec vspec);

char * format_vspec(Version_Spec vspec);
Version_Spec parse_vspec(char * s);

#endif /* VCP_VERSION_H_ */
