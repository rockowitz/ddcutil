/** \file dyn_feature_files.h
 *
 *  Maintain dynamic feature files
 */
// Copyright (C) 2018-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DYN_FEATURE_FILES_H_
#define DYN_FEATURE_FILES_H_

/** \cond */
#include "ddcutil_types.h"

#include "util/error_info.h"
/** \endcond */

#include "base/displays.h"

extern bool enable_dynamic_features;

Error_Info *
dfr_load_by_mmk(
      DDCA_Monitor_Model_Key  mmk,
      Dynamic_Features_Rec ** dfr_loc);

Error_Info * dfr_check_by_dref(Display_Ref * dref);
#ifdef UNUSED
Error_Info * dfr_check_by_mmk(DDCA_Monitor_Model_Key mmk);
#endif

#endif /* DYN_FEATURE_FILES_H_ */
