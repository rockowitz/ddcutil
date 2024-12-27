/** @file ddc_displays.h
 *
 *  Access displays, whether DDC or USB
 */

// Copyright (C) 2014-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_DISPLAYS_H_
#define DDC_DISPLAYS_H_

#include "public/ddcutil_types.h"
#include "public/ddcutil_c_api.h"

#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "base/ddcutil_types_internal.h"
#include "base/displays.h"
#include "base/i2c_bus_base.h"

#ifdef ENABLE_USB
#include "usb/usb_displays.h"
#endif

extern bool  monitor_state_tests;
extern bool  detect_phantom_displays;
extern bool  skip_ddc_checks;

// Initial Checks
void         ddc_set_async_threshold(int threshold);
bool         ddc_initial_checks_by_dref(Display_Ref * dref, bool newly_added);
bool         ddc_recheck_dref(Display_Ref * dref);

// Get Display Information
GPtrArray *  ddc_get_all_display_refs();  // returns GPtrArray of Display_Ref instances, including invalid displays
void         ddc_dbgrpt_display_refs(bool include_invalid_displays, bool report_businfo, int depth);
void         ddc_dbgrpt_display_refs_summary(bool include_invalid_displays, bool report_businfo, int depth);
void         ddc_dbgrpt_display_refs_terse(bool include_invalid_displays, int depth);
GPtrArray *  ddc_get_filtered_display_refs(bool include_invalid_displays, bool include_removed_drefs);
GPtrArray *  ddc_get_bus_open_errors();
int          ddc_get_display_count(bool include_invalid_displays);

// Display Detection
void         ddc_ensure_displays_detected();
void         ddc_discard_detected_displays();
void         ddc_redetect_displays();
bool         ddc_displays_already_detected();
#ifdef UNUSED
Display_Ref* detect_display_by_businfo(I2C_Bus_Info * businfo);
#endif
DDCA_Status  ddc_enable_usb_display_detection(bool onoff);
bool         ddc_is_usb_display_detection_enabled();
void         dbgrpt_bus_open_errors(GPtrArray * open_errors, int depth);

typedef enum {
   DREF_VALIDATE_BASIC_ONLY         = 0,
   DREF_VALIDATE_EDID               = 1,
   DREF_VALIDATE_AWAKE              = 2,
   DREF_VALIDATE_DDC_COMMUNICATION_FAILURE_OK = 3,
} Dref_Validation_Options;
#define DREF_VALIDATE_ALL (DREF_VALIDATE_EDID | DREF_VALIDATE_AWAKE)
DDCA_Status  ddc_validate_display_ref2(Display_Ref * dref, Dref_Validation_Options validation_options);

// Display Status Change
Display_Ref* ddc_add_display_by_businfo(I2C_Bus_Info * businfo);
Display_Ref* ddc_remove_display_by_businfo(I2C_Bus_Info * businfo);    // for poll
// Display_Ref* ddc_remove_display_by_businfo(I2C_Bus_Info * businfo);
Display_Ref* ddc_get_dref_by_busno_or_connector(int busno, const char * connector, bool ignore_invalid);
#define      DDC_GET_DREF_BY_BUSNO(_busno, _ignore) \
             ddc_get_dref_by_busno_or_connector(_busno,NULL, (_ignore))
#define      DDC_GET_DREF_BY_CONNECTOR(_connector_name, _ignore_invalid) \
             ddc_get_dref_by_busno_or_connector(-1, _connector_name, _ignore_invalid)

void         ddc_add_display_ref(Display_Ref * dref);
void         ddc_mark_display_ref_removed(Display_Ref* dref);

#ifdef OLD
// Report Hotplug Event (alternative, simpler)
DDCA_Status  ddc_register_display_hotplug_callback(DDCA_Display_Hotplug_Callback_Func func);
DDCA_Status  ddc_unregister_display_hotplug_callback(DDCA_Display_Hotplug_Callback_Func func);
void         ddc_emit_display_hotplug_event();
#endif

// Initialization and termination
void         init_ddc_displays();
void         terminate_ddc_displays();

#endif /* DDC_DISPLAYS_H_ */
