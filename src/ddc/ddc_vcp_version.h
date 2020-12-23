/** @file ddc_vcp_version.h
 *
 * Functions to obtain the VCP (MCCS) version for a display.
 * These functions are in a separate source file to simplify
 * the acyclic graph of #includes within the ddc source directory.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_VCP_VERSION_H_
#define DDC_VCP_VERSION_H_

#include "base/displays.h"
#include "base/vcp_version.h"

DDCA_MCCS_Version_Spec set_vcp_version_xdf_by_dh(Display_Handle * dh);
DDCA_MCCS_Version_Spec get_vcp_version_by_dh(    Display_Handle * dh);
DDCA_MCCS_Version_Spec get_vcp_version_by_dref(  Display_Ref *    dref);

#endif /* DDC_VCP_VERSION_H_ */
