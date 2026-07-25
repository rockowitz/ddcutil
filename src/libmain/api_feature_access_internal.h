/** @file api_feature_access_internal.h
 *
 *  Contains declarations of functions used only by other api_... files,
 *  and of otherwise unpublished and archived functions.
 */

// Copyright (C) 2015-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef API_FEATURE_ACCESS_INTERNAL_H_
#define API_FEATURE_ACCESS_INTERNAL_H_

#include "public/ddcutil_types.h"

// here because there's no api_feature_access.h
void init_api_feature_access();

// not published
void dbgrpt_any_vcp_value(DDCA_Any_Vcp_Value * valrec, int depth);

#endif /* API_FEATURE_ACCESS_INTERNAL_H_ */
