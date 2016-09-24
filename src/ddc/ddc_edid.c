/* ddc_edid.c
 *
 * Functions to obtain EDID information for a display.
 *
 * These functions are in a separate source file to avoid circular
 * #include dependencies within the ddc source directory.
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

#include "base/core.h"
#include "util/edid.h"
#include "i2c/i2c_bus_core.h"
#include "adl/adl_shim.h"
#include "ddc/ddc_edid.h"
#ifdef USE_USB
#include "usb/usb_displays.h"
#endif


// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_DDC;


Parsed_Edid*
ddc_get_parsed_edid_by_display_handle(Display_Handle * dh) {
   Parsed_Edid* pEdid = NULL;

   switch (dh->io_mode) {
   case DDC_IO_DEVI2C:
      pEdid = i2c_get_parsed_edid_by_busno(dh->busno);
      break;
   case DDC_IO_ADL:
      pEdid = adlshim_get_parsed_edid_by_display_handle(dh);
      break;

   case USB_IO:
#ifdef USE_USB
      pEdid = usb_get_parsed_edid_by_display_handle(dh);
      break;
#else
      PROGRAM_LOGIC_ERROR("ddcutil not build with USB support");
#endif
   }
   TRCMSG("Returning %p", __func__, pEdid);
   return pEdid;
}


Parsed_Edid*
ddc_get_parsed_edid_by_display_ref(Display_Ref * dref) {
   Parsed_Edid* pEdid = NULL;

   switch(dref->io_mode) {
   case DDC_IO_DEVI2C:
      pEdid = i2c_get_parsed_edid_by_busno(dref->busno);
      break;
   case DDC_IO_ADL:
      pEdid = adlshim_get_parsed_edid_by_display_ref(dref);
      break;
   case USB_IO:
#ifdef USE_USB
      pEdid = usb_get_parsed_edid_by_display_ref(dref);
      break;
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
   }

   TRCMSG("Returning %p", pEdid);
   return pEdid;
}
