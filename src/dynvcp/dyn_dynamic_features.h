/** \file dyn_dynamic_features.h
 *
 *  Maintain dynamic feature records
 */
// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DYN_DYNAMIC_FEATURES_H_
#define DYN_DYNAMIC_FEATURES_H_

/** \cond */
#include "ddcutil_types.h"

#include "util/error_info.h"
/** \endcond */
#include "base/displays.h"


bool enable_dynamic_features;

Error_Info *
dfr_load_by_mmk(
      DDCA_Monitor_Model_Key  mmk,
      Dynamic_Features_Rec ** dfr_loc);

Error_Info * dfr_check_by_dref(Display_Ref * dref);
Error_Info * dfr_check_by_mmk(DDCA_Monitor_Model_Key mmk);

#endif /* DYN_DYNAMIC_FEATURES_H_ */
