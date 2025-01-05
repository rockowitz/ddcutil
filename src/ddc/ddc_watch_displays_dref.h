// ddc_watch_displays_dref.h

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>

#include "base/displays.h"

#ifndef DDC_WATCH_DISPLAYS_DREF_H_
#define DDC_WATCH_DISPLAYS_DREF_H_

void          ddc_add_display_ref(Display_Ref * dref);
void          ddc_mark_display_ref_removed(Display_Ref* dref);
bool          ddc_recheck_dref(Display_Ref * dref);
Display_Ref * ddc_get_dref_by_busno_or_connector(
                  int          busno,
                  const char * connector,
                  bool         ignore_invalid);
void          init_ddc_watch_displays_dref();
#endif /* DDC_WATCH_DISPLAYS_DREF_H_ */
