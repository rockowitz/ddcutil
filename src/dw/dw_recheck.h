/** @file dw_recheck.h */

// Copyright (C) 2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_RECHECK_H_
#define DW_RECHECK_H_

#include <glib-2.0/glib.h>

void     dw_put_recheck_queue(Display_Ref* dref);
gpointer dw_recheck_displays_func(gpointer data);

void     init_dw_recheck();

#endif /* DW_RECHECK_H_ */
