/** @file ddc_save_current_settings.h
 *  Implement DDC command Save Current Settings
 */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_SAVE_CURRENT_SETTINGS_H_
#define DDC_SAVE_CURRENT_SETTINGS_H_


#include "util/error_info.h"

#include "base/displays.h"

Error_Info *
ddc_save_current_settings(
      Display_Handle *          dh);

void
init_ddc_save_current_settings();

#endif /* DDC_SAVE_CURRENT_SETTINGS_H_ */
