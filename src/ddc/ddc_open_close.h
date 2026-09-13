/** @file ddc_open_close.h
 *  Opening and closing a display at the DDC level
 */

// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_OPEN_CLOSE_H_
#define DDC_OPEN_CLOSE_H_

#include <stdbool.h>

#include "util/error_info.h"

#include "base/displays.h"

Error_Info * ddc_open_display(
      Display_Ref *    dref,
      Call_Options     callopts,
      Display_Handle** dh_loc);

Error_Info * ddc_close_display(
      Display_Handle * dh);

void ddc_close_display_wo_return(
      Display_Handle * dh);

void ddc_close_all_displays();
void ddc_close_all_displays_for_current_thread(bool error_if_open);

// Maintain the per-thread record of which display handles this thread has
// open.  Called by ddc_open_display() and ddc_close_display(); declared here
// so they can be tested directly.  Each reports whether it changed the array.
bool add_open_display_for_current_thread(Display_Handle * dh);
bool remove_open_display_for_current_thread(Display_Handle * dh);

DDCA_Status ddc_validate_display_handle2(Display_Handle * dh);

void ddc_dbgrpt_valid_display_handles(int depth);

void
init_ddc_open_close();

void
terminate_ddc_open_close();

#endif /* DDC_OPEN_CLOSE_H_ */
