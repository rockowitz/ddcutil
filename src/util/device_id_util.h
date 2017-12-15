/* device_id_util.h
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** @file device_id_util.h
 * Lookup PCI and USB device ids
 */

#ifndef DEVICE_ID_UTIL_H_
#define DEVICE_ID_UTIL_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
/** \endcond */

// *** Initialization ***
bool devid_ensure_initialized();


// *** Device ID lookup ***

/** Return value for devid_get_pci_names() and devid_usb_names().
 *  Depending on the number of arguments to those functions,
 *  device_name and subsys_or_interface_name may or may not be set.
 */
typedef struct {
   char * vendor_name;               ///< vendor name
   char * device_name;               ///< device name (may be NULL)
   char * subsys_or_interface_name;  ///< subsystem or interface name (may be NULL)
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
// "item type" is the term used in usb.ids
// "item tag"  is the term used in USB HID documentation
char * devid_hid_descriptor_item_type(ushort id);  // R entry in usb.ids, corresponds to names_reporttag()

// *** HID Descriptor Type ***
// declared here but not defined
// char * devid_hid_descriptor_type(ushort id);       // HID   entry in usb.ids


// *** HUT table ***
char * devid_usage_code_page_name(ushort usage_page_code);  // corresponds to names_huts()
char * devid_usage_code_id_name(ushort usage_page_code, ushort usage_simple_id);  // corresponds to names_hutus()
char * devid_usage_code_name_by_extended_id(uint32_t extended_usage);

#endif /* DEVICE_ID_UTIL_H_ */
