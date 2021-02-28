/** @file query_sysenv_dmidecode.c
 *
 *  dmidecode report for the environment command
 */

// Copyright (C) 2016-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/report_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"
/** \endcond */

#include "query_sysenv_base.h"


//
// dmidecode related functions
//

// from dmidecode.c
static const char *dmi_chassis_type(Byte code)
{
   /* 7.4.1 */
   static const char *type[] = {
      "Other", /* 0x01 */
      "Unknown",
      "Desktop",
      "Low Profile Desktop",
      "Pizza Box",
      "Mini Tower",
      "Tower",
      "Portable",
      "Laptop",
      "Notebook",
      "Hand Held",
      "Docking Station",
      "All In One",
      "Sub Notebook",
      "Space-saving",
      "Lunch Box",
      "Main Server Chassis", /* CIM_Chassis.ChassisPackageType says "Main System Chassis" */
      "Expansion Chassis",
      "Sub Chassis",
      "Bus Expansion Chassis",
      "Peripheral Chassis",
      "RAID Chassis",
      "Rack Mount Chassis",
      "Sealed-case PC",
      "Multi-system",
      "CompactPCI",
      "AdvancedTCA",
      "Blade",
      "Blade Enclosing",
      "Tablet",
      "Convertible",
      "Detachable",
      "IoT Gateway",
      "Embedded PC",
      "Mini PC",
      "Stick PC" /* 0x24 */
   };

   code &= 0x7F; /* bits 6:0 are chassis type, 7th bit is the lock bit */

   if (code >= 0x01 && code <= 0x24)
      return type[code - 0x01];
   return NULL;
}


/** Reports DMI information for the system.
 */
void query_dmidecode() {

   // Note: The alternative of calling execute_shell_cmd_collect() with the following
   // command fails if executing from a non-privileged account, which lacks permissions
   // for /dev/mem or /sys/firmware/dmi/tables/smbios_entry_point
   // char * cmd =    "dmidecode | grep \"['Base Board Info'|'Chassis Info'|'System Info']\" -A2";
   // GPtrArray * lines = execute_shell_cmd_collect(cmd);

   char * sysdir = "/sys/class/dmi/id";
   rpt_title("DMI Information from /sys/class/dmi/id:", 0);

   char * dv = "(Unavailable)";
   char buf[100];
   int bufsz = 100;

   //    verbose
   rpt_vstring(1, "%-25s %s","Motherboard vendor:",       read_sysfs_attr_w_default_r(sysdir, "board_vendor",  dv, buf, bufsz, false));
   rpt_vstring(1, "%-25s %s","Motherboard product name:", read_sysfs_attr_w_default_r(sysdir, "board_name",    dv, buf, bufsz, false));
   rpt_vstring(1, "%-25s %s","System vendor:",            read_sysfs_attr_w_default_r(sysdir, "sys_vendor",    dv, buf, bufsz, false));
   rpt_vstring(1, "%-25s %s","System product name:",      read_sysfs_attr_w_default_r(sysdir, "product_name",  dv, buf, bufsz, false));
   rpt_vstring(1, "%-25s %s","Chassis vendor:",           read_sysfs_attr_w_default_r(sysdir, "chassis_vendor",dv, buf, bufsz, false));

   char * chassis_type_s = read_sysfs_attr(sysdir, "chassis_type", /*verbose=*/ true);
   char * chassis_desc = dv;
   char workbuf[100];
   if (chassis_type_s) {
      int chassis_type_i = atoi(chassis_type_s);   // TODO: use something safer?
      const char * chassis_type_name = dmi_chassis_type(chassis_type_i);
      if (chassis_type_name)
         snprintf(workbuf, 100, "%s - %s", chassis_type_s, chassis_type_name);
      else
         snprintf(workbuf, 100, "%s - Unrecognized value", chassis_type_s);
      chassis_desc = workbuf;
      free(chassis_type_s);
   }
   rpt_vstring(1, "%-25s %s", "Chassis type:", chassis_desc);

}
