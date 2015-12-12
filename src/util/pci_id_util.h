/* pci_id_util.h
 *
 * Created on: Dec 9, 2015
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

#ifndef PCI_ID_UTIL_H_
#define PCI_ID_UTIL_H_

#include <glib.h>
#include <stdbool.h>
#include <stdlib.h>


typedef struct {
   ushort subvendor_id;
   ushort subdevice_id;
   char * subsystem_name;
} Pci_Id_Subsys;


typedef struct {
   ushort      device_id;
   char *      device_name;
   GPtrArray * device_subsystems;  // Pci_Id_Subsystem
} Pci_Id_Device;


typedef struct {
   ushort      vendor_id;
   char *      vendor_name;
   GPtrArray * vendor_devices;    // Pci_Id_Sub_Device
} Pci_Id_Vendor;


typedef struct {
   char * vendor_name;
   char * device_name;
   char * subsys_name;
} Pci_Id_Names;


bool init_pci_ids();
Pci_Id_Vendor * pci_id_find_vendor(ushort vendor_id);
Pci_Id_Device * pci_id_find_device(Pci_Id_Vendor * cur_vendor, ushort device_id);
Pci_Id_Subsys * pci_id_find_subsys(Pci_Id_Device * cur_device, ushort subvendor_id, ushort subdevice_id);
Pci_Id_Names pci_id_get_names(
                ushort vendor_id,
                ushort device_id,
                ushort subvendor_id,
                ushort subdevice_id,
                int argct);

#endif /* PCI_ID_UTIL_H_ */
