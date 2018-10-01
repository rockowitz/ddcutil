/* ddc_parsed_capabilities.h
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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
 *
 */

#ifndef SRC_DDC_DDC_PARSED_CAPABILITIES_H_
#define SRC_DDC_DDC_PARSED_CAPABILITIES_H_

#include "vcp/parsed_capabilities_feature.h"
#include "vcp/parse_capabilities.h"

#include <glib.h>

#include "util/data_structures.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/dynamic_features.h"
#include "base/vcp_version.h"

#ifdef UNNEEDED
void report_capabilities_feature(
      Capabilities_Feature_Record * vfr,
      DDCA_MCCS_Version_Spec        vcp_version,
      int                           depth);
#endif


void                 report_parsed_capabilities(
                         Parsed_Capabilities*    pcaps,
                         DDCA_Monitor_Model_Key* mmid,
                         int                     depth);

#endif /* SRC_DDC_DDC_PARSED_CAPABILITIES_H_ */
