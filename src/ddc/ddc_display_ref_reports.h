/** @file ddc_display_ref_reports.h  */

// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_DISPLAY_REF_REPORTS_H_
#define DDC_DISPLAY_REF_REPORTS_H_

#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "base/displays.h"

// Display_Ref Reports
void ddc_report_display_by_dref(Display_Ref * dref, int depth);
int  ddc_report_displays(bool include_invalid_displays, int depth);
void ddc_dbgrpt_display_ref(Display_Ref * drec, int depth);
void ddc_dbgrpt_drefs(char * msg, GPtrArray* ptrarray, int depth);

// Initialization
void init_ddc_display_ref_reports();

#endif /* DDC_DISPLAY_REF_REPORTS_H_ */
