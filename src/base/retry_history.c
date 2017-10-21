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

#include "retry_history.h"


void retry_history_free(Retry_History* history) {
   free(history);
}

void retry_history_clear(Retry_History * history) {
   assert(history);
   memset(history, sizeof(Retry_History), 0);
}

int retry_history_add(Retry_History * history, Public_Status_Code psc) {
   assert(history->ct < MAX_MAX_TRIES);
   history->psc[history->ct++] = psc;
   return history->ct;
   }

