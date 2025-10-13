/** @file ddc_display_selection.h */

// Copyright (C) 2022-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_DISPLAY_SELECTION_H_
#define DDC_DISPLAY_SELECTION_H_

#include "base/core.h"
#include "base/displays.h"

#ifndef DISPSEL_ONLY
Display_Ref*
get_display_ref_for_display_identifier(
   Display_Identifier* pdid,
   Call_Options        callopts);
#endif

// new way:
Display_Ref *
ddc_find_display_ref_by_selector(Display_Selector * dsel);


void
init_ddc_display_selection();
#endif /* DDC_DISPLAY_SELECTION_H_ */
