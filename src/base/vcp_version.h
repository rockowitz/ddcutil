/** @file vcp_version.h
 *
 *  VCP (aka MCCS) version specification
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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

bool vcp_version_is_valid(DDCA_MCCS_Version_Spec vspec, bool allow_unknown);

char * format_vspec(DDCA_MCCS_Version_Spec vspec);
DDCA_MCCS_Version_Spec parse_vspec(char * s);

char * format_vcp_version_id(DDCA_MCCS_Version_Id version_id);
char * vcp_version_id_name(DDCA_MCCS_Version_Id version_id);

DDCA_MCCS_Version_Spec mccs_version_id_to_spec(DDCA_MCCS_Version_Id id);
DDCA_MCCS_Version_Id mccs_version_spec_to_id(DDCA_MCCS_Version_Spec vspec);

#endif /* VCP_VERSION_H_ */
