/* retry_history.c
 *
 * <copyright>
 * Copyright (C) 2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "status_code_mgt.h"

#include "retry_history.h"

void retry_history_init(Retry_History* history) {
   assert(history);
   memcpy(history->marker, RETRY_HISTORY_MARKER, 4);
   history->ct = 0;
}

Retry_History * retry_history_new() {
   Retry_History * history = calloc(1, sizeof(Retry_History));
   memcpy(history->marker, RETRY_HISTORY_MARKER, 4);
   return history;
}

void retry_history_free(Retry_History* history) {
   if (history) {
      assert(memcmp(history->marker, RETRY_HISTORY_MARKER, 4) == 0);
      history->marker[3] = 'x';
      free(history);
   }
}

void retry_history_clear(Retry_History * history) {
   if (history) {
      assert(memcmp(history->marker, RETRY_HISTORY_MARKER, 4) == 0);
      history->ct = 0;
   }
}

int retry_history_add(Retry_History * history, Public_Status_Code psc) {
   int result = 0;
   if (history) {
      assert(memcmp(history->marker, RETRY_HISTORY_MARKER, 4) == 0);
      assert(history->ct < MAX_MAX_TRIES);
      history->psc[history->ct++] = psc;
      result = history->ct;
   }
   return result;
}


void retry_history_dump(Retry_History * history) {
   if (history) {
      assert(memcmp(history->marker, RETRY_HISTORY_MARKER, 4) == 0);
      for (int ndx = 0; ndx < history->ct; ndx++) {
         char * desc = psc_desc(history->psc[ndx]);
         DBGMSG("psc[%d]: %s", ndx, desc);
      }
   }
}


char * retry_history_string(Retry_History * history) {
   bool debug = false;
   DBGMSF(debug, "history=%p, history->ct=%d", history, history->ct);

   GString * gs = g_string_new(NULL);

   if (history) {
      assert(memcmp(history->marker, RETRY_HISTORY_MARKER, 4) == 0);

      bool first = true;

      int ndx = 0;
      while (ndx < history->ct) {
         Public_Status_Code this_psc = history->psc[ndx];
         int cur_ct = 1;
         for (int i = ndx+1; i < history->ct; i++) {
            if (history->psc[i] != this_psc)
               break;
            cur_ct++;
         }
         if (first)
            first = false;
         else
            g_string_append(gs, ", ");
         char * cur_name = psc_name(this_psc);
         g_string_append(gs, cur_name);
         if (cur_ct > 1)
            g_string_append_printf(gs, "(x%d)", cur_ct);
         ndx += cur_ct;
      }

   }

   char * result = gs->str;
   g_string_free(gs, false);

   DBGMSF(debug, "Done.  Returning: |%s|", result);
   return result;
}
