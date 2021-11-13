/** \f ddc_async.c
 *
 *  Experimental async code
 */

// Copyright (C) 2018-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <string.h>

#include "util/error_info.h"
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
      bool debug = false;

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
   DBGTRC_STARTING(debug, TRACE_GROUP, "Reading feature 0x%02x, dh=%s, dh->fd=%d",
            feature_code, dh_repr_t(dh), dh->fd);

   Error_Info * ddc_excp = NULL;

   Async_Getvcp_Data parms;
   memcpy(parms.marker, ASYNC_GETVCP_DATA_MARKER, 4);
   parms.call_type = call_type;
   parms.feature_code = feature_code;
   parms.dh = dh;
   parms.callback_func = callback_func;

   // GThread * th =
   g_thread_new(
         "getvcp",
         threaded_get_vcp_value,
         &parms);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %s", errinfo_summary(ddc_excp));
   return ddc_excp;
}

