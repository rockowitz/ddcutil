/* ddc_async.c
 *
 * <copyright>
 * Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \f
 *  Experimental async code
 */

#include <assert.h>
#include <string.h>

#include "base/core.h"

#include "ddc_vcp.h"

#include "ddc_async.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDC;



#define ASYNC_GETVCP_DATA_MARKER "GVCP"
typedef struct {
   char  marker[4];
   Display_Handle *  dh;
   Byte              feature_code;
   DDCA_Vcp_Value_Type call_type;
   DDCA_Notification_Func callback_func;
} Async_Getvcp_Data;

// function to be run in thread
gpointer threaded_get_vcp_value(gpointer data) {
      bool debug = true;

      Async_Getvcp_Data * parms = data;
      assert(memcmp(parms->marker, ASYNC_GETVCP_DATA_MARKER, 4) == 0 );

      Public_Status_Code psc = 0;
      DDCA_Any_Vcp_Value    * anyval = NULL;
      Error_Info * ddc_excp = ddc_get_vcp_value(
            parms->dh,
            parms->feature_code,
            parms->call_type,
            &anyval);

      if (ddc_excp) {
         psc = ERRINFO_STATUS(ddc_excp);
         ERRINFO_FREE_WITH_REPORT(ddc_excp, debug || IS_TRACING() || report_freed_exceptions);
      }
      else {
         psc = 0;
         // free(valrec);   // ??? what of table bytes
      }

      parms->callback_func(psc, anyval);
      // g_thread_exit(NULL);
      return NULL;
   }

Error_Info *
start_get_vcp_value(
       Display_Handle *          dh,
       Byte                      feature_code,
       DDCA_Vcp_Value_Type       call_type,
       DDCA_Notification_Func    callback_func)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. Reading feature 0x%02x, dh=%s, dh->fh=%d",
            feature_code, dh_repr_t(dh), dh->fh);

   Error_Info * ddc_excp = NULL;

   Async_Getvcp_Data parms;
   parms.call_type = call_type;
   parms.feature_code = feature_code;
   parms.dh = dh;
   parms.callback_func = callback_func;

   // GThread * th =
   g_thread_new(
         "getvcp",
         threaded_get_vcp_value,
         &parms);
   return ddc_excp;
}

