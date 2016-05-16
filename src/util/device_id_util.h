/* device_id_util.h
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef DEVICE_ID_UTIL_H_
#define DEVICE_ID_UTIL_H_

#include <glib.h>
#include <stdbool.h>
#include <stdlib.h>


typedef enum {
   ID_TYPE_PCI,
   ID_TYPE_USB
} Device_Id_Type;

char * devid_find_file(Device_Id_Type id_type);

#ifdef OLD
typedef struct {
   ushort subvendor_id;
   ushort subdevice_id;
   char * subsystem_name;
} Pci_Id_Subsys;

typedef struct {
   ushort      device_id;
   char *      device_name;
   GPtrArray * device_subsystems;  // Pci_Id_Subsys
} Pci_Id_Device;

typedef struct {
   ushort      vendor_id;
   char *      vendor_name;
   GPtrArray * vendor_devices;    // Pci_Id_Device
} Pci_Id_Vendor;
#endif

typedef struct {
   char * vendor_name;
   char * device_name;
   char * subsys_or_interface_name;
} Pci_Usb_Id_Names;




// *** HID Descriptor Item Types ***


char * devid_hid_descriptor_item_type(ushort id);

char * devid_hid_descriptor_type(ushort id);


// *** HUT table ***

typedef struct {
   ushort  usage_page;        // usage page
   char *  usage_page_name;
   GPtrArray * usage_ids;
} Devid_Usage_Page;

typedef struct {
   ushort  usage_page;
   ushort  simple_usage_code;         // id within the page
   char *  usage_code_name;
} Devid_Usage_Code;

Devid_Usage_Page * devid_find_usage_page(ushort usage_page);
Devid_Usage_Code * devid_find_usage_id(ulong fq_usage_code);
Devid_Usage_Code * devid_find_usage_by_page_and_id(ushort usage_page, ushort simple_usage_code);



bool devid_ensure_initialized();
#ifdef OLD
Pci_Id_Vendor * pci_id_find_vendor(ushort vendor_id);
Pci_Id_Device * pci_id_find_device(Pci_Id_Vendor * cur_vendor, ushort device_id);
Pci_Id_Subsys * pci_id_find_subsys(Pci_Id_Device * cur_device, ushort subvendor_id, ushort subdevice_id);

Pci_Id_Vendor * usb_id_find_vendor(ushort vendor_id);
Pci_Id_Device * usb_id_find_device(Pci_Id_Vendor * cur_vendor, ushort device_id);
Pci_Id_Subsys * usb_id_find_interface(Pci_Id_Device * cur_device, ushort interface_id);
#endif

Pci_Usb_Id_Names devid_get_pci_names(
                ushort vendor_id,
                ushort device_id,
                ushort subvendor_id,
                ushort subdevice_id,
                int argct);

Pci_Usb_Id_Names devid_get_usb_names(
                ushort vendor_id,
                ushort device_id,
                ushort interface_id,
                int argct);


char * devid_usage_code_page_name(ushort usage_page_code);

char * devid_usage_code_id_name(ushort usage_page_code, ushort usage_simple_id);

#endif /* DEVICE_ID_UTIL_H_ */
