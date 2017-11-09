/* query_sysenv_dmidecode.c
 *
 * Created on: Nov 9, 2017
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../app_sysenv/query_sysenv_base.h"
#include "util/data_structures.h"
#include "util/report_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"



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


#ifdef UNUSED_UGLY
void report_dmidecode_string(char * s, int depth) {
   char cmd[100];
   strcpy(cmd, "dmidecode -s ");
   strcat(cmd, s);
   rpt_vstring(depth, "%s:", s);
   execute_shell_cmd_rpt(cmd, depth+1);
}


void report_dmicode_group(char * header, int depth) {
   char cmd[100];
   snprintf(cmd, 100, "dmidecode | grep '%s' -A2", header);
   // DBGMSG("cmd: |%s|", cmd);
   GPtrArray * lines = execute_shell_cmd_collect(cmd);
   if (lines) {
      for (int ndx = 0; ndx < lines->len; ndx++) {
         char * s = g_ptr_array_index(lines, ndx);
         rpt_title(s, depth);
      }
      g_ptr_array_free(lines,true);
   }
   else
      rpt_vstring(depth, "Command failed: %s", cmd);
}
#endif


/** Reports DMI information for the system.
 */
void query_dmidecode() {

#ifdef NO
   // leave in for testing
   rpt_nl();
   if (test_command_executability("dmidecode") == 0) {
      rpt_vstring(0, "System information from dmidecode:");

#ifdef NO_UGLY
      report_dmidecode_string("baseboard-manufacturer", 1);
      report_dmidecode_string("baseboard-product-name", 1);
      report_dmidecode_string("system-manufacturer", 1);
      report_dmidecode_string("system-product-name", 1);
      report_dmidecode_string("chassis-manufacturer", 1);
      report_dmidecode_string("chassis-type", 1);
#endif

      report_dmicode_group("Base Board Info", 1);
      report_dmicode_group("System Info", 1);
      report_dmicode_group("Chassis Info", 1);
   }
   else
      rpt_vstring(0, "dmidecode command unavailable");
#endif

   char * sysdir = "/sys/class/dmi/id";
   // better way, doesn't require privileged dmidecode
   // rpt_nl();
   rpt_title("DMI Information from /sys/class/dmi/id:", 0);

   char * dv = "(Unavailable)";
   char buf[100];
   int bufsz = 100;

   //    verbpse
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
   }
   rpt_vstring(1, "%-25s %s", "Chassis type:", chassis_desc);

   // Note: The alternative of calling execute_shell_cmd_collect() with the following
   // command fails if executing from a non-privileged account, which lacks permissions
   // for /dev/mem or /sys/firmware/dmi/tables/smbios_entry_point
   // char * cmd =    "dmidecode | grep \"['Base Board Info'|'Chassis Info'|'System Info']\" -A2";
   // GPtrArray * lines = execute_shell_cmd_collect(cmd);

}
