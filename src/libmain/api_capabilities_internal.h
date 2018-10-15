/** @file api_capabilities_internal.h
 */

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 

#ifndef API_CAPABILITIES_INTERNAL_H_
#define API_CAPABILITIES_INTERNAL_H_

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


#endif /* API_CAPABILITIES_INTERNAL_H_ */
