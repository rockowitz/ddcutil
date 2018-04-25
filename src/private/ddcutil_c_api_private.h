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

// Declarations of API functions that haven't yet been published or that have
// been removed from ddcutil_c_api.h

/** Parses a capabilities string, and reports the parsed string
 *  using the code of command "ddcutil capabilities".
 *
 *  The report is written to the current FOUT location.
 *
 *  The detail level written is sensitive to the current output level.
 *
 *  @param[in]  capabilities_string  capabilities string
 *  @param[in]  depth  logical       indentation depth
 *
 *  @remark
 *  This function exists as a development aide.  Internally, ddcutil uses
 *  a different data structure than DDCA_Parsed_Capabilities.  That
 *  data structure uses internal collections that are not exposed at the
 *  API level.
 *  @since 0.9.0
 */
void ddca_parse_and_report_capabilities(
      char *                    capabilities_string,
      DDCA_Monitor_Model_Key *  mmid,
      int                       depth);




#endif /* DDCUTIL_C_API_PRIVATE_H_ */
