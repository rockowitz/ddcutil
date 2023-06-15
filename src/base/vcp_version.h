/** @file vcp_version.h
 *
 *  VCP (aka MCCS) version specification
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef VCP_VERSION_H_
#define VCP_VERSION_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

#include "public/ddcutil_types.h"

#include "util/coredefs.h"


extern const char * valid_vcp_versions;                    ///< for error msgs

// Both DDCA_MCCS_Version_Spec and DDCA_MCCS_Version_Id exist for historical reasons.
// DDCA_MCCS_Version_Spec is the form in which the version number is returned from a
// GETVCP of feature xDF.  This form is used throughout much of ddcutil.
// DDCA_MCCS_Version_Id reflects the fact that there are a small number of versions
// and simplifies program logic that varies among versions. As of 3/2022 it is only
// used internally in app_vcpinfo.c.

/** @name version_id
 *  Ids for MCCS/VCP versions, reflecting the fact that
 *  there is a small set of valid version values.
 */
///@{

/** MCCS (VCP) Feature Version IDs */
typedef enum {
   MCCS_SPEC_VNONE =   0,     /**< As response, version unknown */
   MCCS_SPEC_V10   =   1,     /**< MCCS v1.0 */
   MCCS_SPEC_V20   =   2,     /**< MCCS v2.0 */
   MCCS_SPEC_V21   =   4,     /**< MCCS v2.1 */
   MCCS_SPEC_V30   =   8,     /**< MCCS v3.0 */
   MCCS_SPEC_V22   =  16,     /**< MCCS v2.2 */
   MCCS_SPEC_VANY  = 255      /**< On queries, match any VCP version */
} MCCS_SPEC_Version_Id;

#define MCCS_SPEC_VUNK MCCS_SPEC_VNONE    /**< For use on responses, indicates version unknown   */
///@}

bool vcp_version_le(DDCA_MCCS_Version_Spec val, DDCA_MCCS_Version_Spec max);
bool vcp_version_lt(DDCA_MCCS_Version_Spec val, DDCA_MCCS_Version_Spec max);
bool vcp_version_gt(DDCA_MCCS_Version_Spec val, DDCA_MCCS_Version_Spec min);
bool vcp_version_eq(DDCA_MCCS_Version_Spec v1,  DDCA_MCCS_Version_Spec v2);

bool vcp_version_is_valid(DDCA_MCCS_Version_Spec vspec, bool allow_unknown);

char * format_vspec(DDCA_MCCS_Version_Spec vspec);
char * format_vspec_verbose(DDCA_MCCS_Version_Spec vspec);
DDCA_MCCS_Version_Spec parse_vspec(char * s);

#ifdef MCCS_VERSION_ID
char * format_vcp_version_id(MCCS_SPEC_Version_Id version_id);
char * vcp_version_id_name(MCCS_SPEC_Version_Id version_id);

DDCA_MCCS_Version_Spec mccs_version_id_to_spec(MCCS_SPEC_Version_Id id);
MCCS_SPEC_Version_Id mccs_version_spec_to_id(DDCA_MCCS_Version_Spec vspec);
#endif

#endif /* VCP_VERSION_H_ */
