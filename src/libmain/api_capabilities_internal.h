/** @file api_capabilities_internal.h
 */

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef API_CAPABILITIES_INTERNAL_H_
#define API_CAPABILITIES_INTERNAL_H_

#include "public/ddcutil_types.h"


#ifdef UNUSED

//
// MCCS Version Id
//

/** \deprecated
 *  Returns the symbolic name of a #DDCA_MCCS_Version_Id,
 *  e.g. "DDCA_MCCS_V20."
 *
 *  @param[in]  version_id  version id value
 *  @return symbolic name (do not free)
 */
__attribute__ ((deprecated))
char *
ddca_mccs_version_id_name(
      DDCA_MCCS_Version_Id  version_id);

/** \deprecated
 *  Returns the descriptive name of a #DDCA_MCCS_Version_Id,
 *  e.g. "2.0".
 *
 *  @param[in]  version_id  version id value
 *  @return descriptive name (do not free)
 *
 *  @remark added to replace ddca_mccs_version_id_desc() during 0.9
 *  development, but then use of DDCA_MCCS_Version_Id deprecated
 */
__attribute__ ((deprecated))
char *
ddca_mccs_version_id_desc(
      DDCA_MCCS_Version_Id  version_id) ;

#endif

void init_api_capabilities();

#endif /* API_CAPABILITIES_INTERNAL_H_ */

