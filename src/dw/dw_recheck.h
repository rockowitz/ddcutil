/** @file dw_recheck.h
 *
 *  Process the queue of display references added in
 *  the main loop for whcih DDC communication was
 *  not immediately detected as  enabled.
 */

// Copyright (C) 2025-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_RECHECK_H_
#define DW_RECHECK_H_

#include <glib-2.0/glib.h>

#include "base/displays.h"

void     dw_put_recheck_queue(Display_Ref* dref);
gpointer dw_recheck_displays_func(gpointer data);

void     init_dw_recheck();

#endif /* DW_RECHECK_H_ */
