// ddc_watch_displays_dref.h

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>

#include "base/displays.h"

#ifndef DDC_WATCH_DISPLAYS_DREF_H_
#define DDC_WATCH_DISPLAYS_DREF_H_

// Display Status Change
void         ddc_add_display_ref(Display_Ref * dref);
void         ddc_mark_display_ref_removed(Display_Ref* dref);

Display_Ref* ddc_add_display_by_businfo(I2C_Bus_Info * businfo);
Display_Ref* ddc_remove_display_by_businfo2(I2C_Bus_Info * businfo);

Display_Ref* ddc_get_dref_by_busno_or_connector(int busno, const char * connector, bool ignore_invalid);
#define      DDC_GET_DREF_BY_BUSNO(_busno, _ignore) \
             ddc_get_dref_by_busno_or_connector(_busno,NULL, (_ignore))
#define      DDC_GET_DREF_BY_CONNECTOR(_connector_name, _ignore_invalid) \
             ddc_get_dref_by_busno_or_connector(-1, _connector_name, _ignore_invalid)

bool         ddc_recheck_dref(Display_Ref * dref);

void         init_ddc_watch_displays_dref();
#endif /* DDC_WATCH_DISPLAYS_DREF_H_ */
