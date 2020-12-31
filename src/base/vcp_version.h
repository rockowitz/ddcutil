/** @file vcp_version.h
 *
 *  VCP (aka MCCS) version specification
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef VCP_VERSION_H_
#define VCP_VERSION_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

#include "public/ddcutil_types.h"

#include "util/coredefs.h"


// Both DDCA_MCCS_Version_Spec and DDCA_MCCS_Version_Id exist for historical reasons.
// DDCA_MCCS_Version_Spec is the form in which the version number is returned from a
// GETVCP of feature xDF.  This form is used throughout much of ddcutil.
// DDCA_MCCS_Version_Id reflects the fact that there are a small number of versions
// and simplifies program logic that varies among versions.
/** @name version_id
 *  Ids for MCCS/VCP versions, reflecting the fact that
 *  there is a small set of valid version values.
 */
///@{

// in sync w constants MCCS_V.. in vcp_feature_codes.c
/** MCCS (VCP) Feature Version IDs */
typedef enum {
   DDCA_MCCS_VNONE =   0,     /**< As response, version unknown */
   DDCA_MCCS_V10   =   1,     /**< MCCS v1.0 */
   DDCA_MCCS_V20   =   2,     /**< MCCS v2.0 */
   DDCA_MCCS_V21   =   4,     /**< MCCS v2.1 */
   DDCA_MCCS_V30   =   8,     /**< MCCS v3.0 */
   DDCA_MCCS_V22   =  16,     /**< MCCS v2.2 */
   DDCA_MCCS_VANY  = 255      /**< On queries, match any VCP version */
} DDCA_MCCS_Version_Id;

#define DDCA_MCCS_VUNK  DDCA_MCCS_VNONE    /**< For use on responses, indicates version unknown   */

///@}

bool vcp_version_le(DDCA_MCCS_Version_Spec val, DDCA_MCCS_Version_Spec max);
bool vcp_version_lt(DDCA_MCCS_Version_Spec val, DDCA_MCCS_Version_Spec max);
bool vcp_version_gt(DDCA_MCCS_Version_Spec val, DDCA_MCCS_Version_Spec min);
bool vcp_version_eq(DDCA_MCCS_Version_Spec v1,  DDCA_MCCS_Version_Spec v2);

bool vcp_version_is_valid(DDCA_MCCS_Version_Spec vspec, bool allow_unknown);

char * format_vspec(DDCA_MCCS_Version_Spec vspec);
char * format_vspec_verbose(DDCA_MCCS_Version_Spec vspec);
DDCA_MCCS_Version_Spec parse_vspec(char * s);

char * format_vcp_version_id(DDCA_MCCS_Version_Id version_id);
char * vcp_version_id_name(DDCA_MCCS_Version_Id version_id);

DDCA_MCCS_Version_Spec mccs_version_id_to_spec(DDCA_MCCS_Version_Id id);
DDCA_MCCS_Version_Id mccs_version_spec_to_id(DDCA_MCCS_Version_Spec vspec);

#endif /* VCP_VERSION_H_ */
