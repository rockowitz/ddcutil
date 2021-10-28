/** @file api_capabilities_internal.h
 */

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef API_CAPABILITIES_INTERNAL_H_
#define API_CAPABILITIES_INTERNAL_H_

#include "public/ddcutil_types.h"

// UNPUBLISHED
/** Parses a capabilities string, and reports the parsed string
 *  using the code of command "ddcutil capabilities".
 *
 *  The report is written to the current FOUT location.
 *
 *  The detail level written is sensitive to the current output level.
 *
 *  @param[in]  capabilities_string  capabilities string
 *  @param[in]  dref                 display reference
 *  @param[in]  depth  logical       indentation depth
 *
 *  @remark
 *  This function exists as a development aide.  Internally, ddcutil uses
 *  a different data structure than DDCA_Parsed_Capabilities.  That
 *  data structure uses internal collections that are not exposed at the
 *  API level.
 *  @remark
 *  Signature changed in 0.9.3
 *  @since 0.9.0
 */
void ddca_parse_and_report_capabilities(
      char *                    capabilities_string,
      DDCA_Display_Ref          dref,
      int                       depth);


// DEPRECATED


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

