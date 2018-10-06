// ddc_dynamic_features.h

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \file
 *  Maintain dynamic feature records
 */

#ifndef DDC_DYNAMIC_FEATURES_H_

/** \cond */
#include "util/error_info.h"
/** \endcond */
#include "base/displays.h"


bool enable_dynamic_features;

Error_Info * dfr_check_by_dref(Display_Ref * dref);

#endif /* DDC_DYNAMIC_FEATURES_H_ */
