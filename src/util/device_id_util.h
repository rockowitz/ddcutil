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


// *** Initialization ***
bool devid_ensure_initialized();


// *** Device ID lookup ***

typedef struct {
   char * vendor_name;
   char * device_name;
   char * subsys_or_interface_name;
} Pci_Usb_Id_Names;

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


// *** HID Descriptor Item Types ***
char * devid_hid_descriptor_item_type(ushort id);
char * devid_hid_descriptor_type(ushort id);


// *** HUT table ***
char * devid_usage_code_page_name(ushort usage_page_code);
char * devid_usage_code_id_name(ushort usage_page_code, ushort usage_simple_id);

#endif /* DEVICE_ID_UTIL_H_ */
