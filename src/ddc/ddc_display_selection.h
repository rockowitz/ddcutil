/** ddc_display_selection.h
 *
 */

// Copyright (C) 2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_DISPLAY_SELECTION_H_
#define DDC_DISPLAY_SELECTION_H_

#include "base/core.h"
#include "base/displays.h"

Display_Ref*
get_display_ref_for_display_identifier(
   Display_Identifier* pdid,
   Call_Options        callopts);

#endif /* DDC_DISPLAY_SELECTION_H_ */
