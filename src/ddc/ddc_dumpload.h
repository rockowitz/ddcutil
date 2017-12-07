/* ddc_dumpload.h
 *
 * Load/store VCP settings from/to file.
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

/** \file
 * Load/store VCP settings to/from file.
 */

#ifndef DDC_DUMPLOAD_H_
#define DDC_DUMPLOAD_H_

/** \cond */
#include <stdio.h>
/** \endcond */

#include "base/ddc_error.h"
#include "base/displays.h"
#include "base/status_code_mgt.h"

#include "vcp/vcp_feature_values.h"


/**
Internal form data structure used to
hold data being loaded.

Whatever the external form, a file or a string, it is converted to
**Dumpload_Data** and then written to the monitor.
*/
typedef
struct {
   time_t         timestamp_millis;       ///< creation timestamp
   Byte           edidbytes[128];         ///< 128 byte EDID,
   char           edidstr[257];           ///< 128 byte EDID as hex string (for future use)
   char           mfg_id[4];              ///< 3 character manufacturer id (from EDID)
   char           model[14];              ///< model string (from EDID)
   char           serial_ascii[14];       ///< serial number string (from EDID)
   DDCA_MCCS_Version_Spec   vcp_version;  ///< monitor VCP/MCCS version
   int            vcp_value_ct;           ///< number of VCP values
   Vcp_Value_Set  vcp_values;             ///< VCP values
} Dumpload_Data;

void
report_dumpload_data(Dumpload_Data * data, int depth);

void
free_dumpload_data(Dumpload_Data * pdata);

char *
format_timestamp(time_t time_millis, char * buf, int bufsz);

Ddc_Error *
loadvcp_by_dumpload_data(
      Dumpload_Data*   pdata,
      Display_Handle * dh);

Ddc_Error *
loadvcp_by_string(
      char *           catenated,
      Display_Handle * dh);

Dumpload_Data*
create_dumpload_data_from_g_ptr_array(GPtrArray * garray);

GPtrArray *
convert_dumpload_data_to_string_array(Dumpload_Data * data);

Public_Status_Code
dumpvcp_as_dumpload_data(
      Display_Handle * dh,
      Dumpload_Data**  pdumpload_data);

Public_Status_Code
dumpvcp_as_string(
      Display_Handle * dh,
      char**           result);

#endif /* DDC_DUMPLOAD_H_ */
