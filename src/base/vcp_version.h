/* vcp_version_spec.h
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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
 * VCP Version Specification
 */


#ifndef VCP_VERSION_H_
#define VCP_VERSION_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

#include "public/ddcutil_types.h"

#include "util/coredefs.h"

#ifdef MOVED_TO_API
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_V10;
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_V20;
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_V21;
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_V30;
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_V22;
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_ANY;
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_UNKNOWN;
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_UNQUERIED;
#endif

bool vcp_version_le(DDCA_MCCS_Version_Spec val, DDCA_MCCS_Version_Spec max);
bool vcp_version_gt(DDCA_MCCS_Version_Spec val, DDCA_MCCS_Version_Spec min);
bool vcp_version_eq(DDCA_MCCS_Version_Spec v1,  DDCA_MCCS_Version_Spec v2);

bool is_known_vcp_spec(DDCA_MCCS_Version_Spec vspec);
bool vcp_version_is_unqueried(DDCA_MCCS_Version_Spec vspec);

char * format_vspec(DDCA_MCCS_Version_Spec vspec);
DDCA_MCCS_Version_Spec parse_vspec(char * s);

char * format_vcp_version_id(DDCA_MCCS_Version_Id version_id);
char * vcp_version_id_name(DDCA_MCCS_Version_Id version_id);

DDCA_MCCS_Version_Spec mccs_version_id_to_spec(DDCA_MCCS_Version_Id id);
DDCA_MCCS_Version_Id mccs_version_spec_to_id(DDCA_MCCS_Version_Spec vspec);

#endif /* VCP_VERSION_H_ */
