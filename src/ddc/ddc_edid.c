/* ddc_edid.c
 *
 * Created on: Dec 31, 2015
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

// functions in separate file to eliminate circular dependencies

#include "base/edid.h"
#include "base/msg_control.h"

#include "i2c/i2c_bus_core.h"

#include "adl/adl_shim.h"

#include "ddc/ddc_edid.h"


// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_DDC;


Parsed_Edid* ddc_get_parsed_edid_by_display_handle(Display_Handle * dh) {
   Parsed_Edid* pEdid = NULL;

   if (dh->ddc_io_mode == DDC_IO_DEVI2C)
      pEdid = i2c_get_parsed_edid_by_busno(dh->busno);
   else {
      pEdid = adlshim_get_parsed_edid_by_display_handle(dh);
   }
   // DBGMSG("Returning %p", pEdid);
   TRCMSG("Returning %p", __func__, pEdid);
   return pEdid;
}



Parsed_Edid* ddc_get_parsed_edid_by_display_ref(Display_Ref * dref) {
   Parsed_Edid* pEdid = NULL;

   if (dref->ddc_io_mode == DDC_IO_DEVI2C)
      pEdid = i2c_get_parsed_edid_by_busno(dref->busno);
   else {
      pEdid = adlshim_get_parsed_edid_by_display_ref(dref);
   }
   // DBGMSG("Returning %p", pEdid);
   TRCMSG("Returning %p", pEdid);
   return pEdid;
}
