/** @file dw_dref.h
 *  Functions that modify persistent Display_Ref related data structures when
 *  display connection and disconnection are detected.
 */

// Copyright (C) 2024-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>

#include "util/error_info.h"

#include "base/displays.h"
#include "base/i2c_bus_base.h"

#ifndef DW_DREF_H_
#define DW_DREF_H_

void         dw_add_display_ref(Display_Ref * dref);
Display_Ref* dw_add_display_by_businfo(I2C_Bus_Info * businfo);
Display_Ref* dw_remove_display_by_businfo(I2C_Bus_Info * businfo);
Error_Info*  dw_recheck_dref(Display_Ref * dref);

void         init_dw_dref();

#endif /* DW_DREF_H_ */
