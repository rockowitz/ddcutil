/** @file dw_dref.h
 *  Functions that modify persistent Display_Ref related data structures when
 *  display connection and disconnection are detected.
 */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>

#include "util/error_info.h"

#include "base/displays.h"
#include "base/i2c_bus_base.h"

#ifndef DW_DREF_H_
#define DW_DREF_H_

void         ddc_add_display_ref(Display_Ref * dref);
void         ddc_mark_display_ref_removed(Display_Ref* dref);

Display_Ref* ddc_add_display_by_businfo(I2C_Bus_Info * businfo);
Display_Ref* ddc_remove_display_by_businfo(I2C_Bus_Info * businfo);

Display_Ref* ddc_get_dref_by_busno_or_connector(int busno, const char * connector, bool ignore_invalid);
#define      DDC_GET_DREF_BY_BUSNO(_busno, _ignore) \
             ddc_get_dref_by_busno_or_connector(_busno,NULL, (_ignore))
#define      DDC_GET_DREF_BY_CONNECTOR(_connector_name, _ignore_invalid) \
             ddc_get_dref_by_busno_or_connector(-1, _connector_name, _ignore_invalid)

Error_Info*  ddc_recheck_dref(Display_Ref * dref);

void         init_dw_dref();

#endif /* DW_DREF_H_ */
