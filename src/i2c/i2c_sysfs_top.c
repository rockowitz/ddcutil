/** i2c_sysfs_top.c */

// Copyright (C) 2020-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later
 
#include <glib-2.0/glib.h>

#include "util/data_structures.h"
#include "util/report_util.h"

#include "i2c_sysfs_conflicting_drivers.h"
#include "i2c_sysfs_i2c_info.h"
#include "i2c/i2c_sys_drm_connector.h"

#include "i2c_sysfs_top.h"


void consolidated_i2c_sysfs_report(int depth) {
   int d0 = depth;
   int d1 = depth+1;

   rpt_label(d0, "*** Sys_Drm_Connector report: Detailed /sys/class/drm report: ***");
   report_sys_drm_connectors(true, d1);
   rpt_nl();

   // not currently used, and leaks memory
   // rpt_label(d0, "*** Sys_Drm_Connector_FixedInfo report: Simplified /sys/class/drm report: ***");
   // report_sys_drm_connectors_fixedinfo(d1);
   // rpt_nl();

   rpt_label(d0, "*** Sysfs_I2C_Info report ***");
   GPtrArray * reports = get_all_sysfs_i2c_info(true, -1);
   dbgrpt_all_sysfs_i2c_info(reports, d1);
   rpt_nl();

   rpt_label(d0, "*** Sysfs I2C devices possibly associated with displays ***");
   Bit_Set_256 buses = get_possible_ddc_ci_bus_numbers_using_sysfs_i2c_info();
   rpt_vstring(d0, "I2C buses to check: %s", bs256_to_string_t(buses, "x", " "));
   rpt_nl();

   rpt_label(d0, "*** Sys_Conflicting_Driver report: Check for Conflicting Device Drivers ***");
   GPtrArray * conflicts = collect_conflicting_drivers_for_any_bus(-1);
   if (conflicts && conflicts->len > 0) {
      report_conflicting_drivers(conflicts, d1);
      rpt_vstring(d1, "Likely conflicting drivers found: %s\n", conflicting_driver_names_string_t(conflicts));
   }
   else
      rpt_label(d1, "No conflicting drivers found");
   free_conflicting_drivers(conflicts);
   rpt_nl();

   rpt_label(0, "*** Sysfs Reports Done ***");
   rpt_nl();
}

