/* retry_history.h
 *
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

#ifndef RETRY_HISTORY_H_
#define RETRY_HISTORY_H_

#include "parms.h"
#include "status_code_mgt.h"


typedef struct retry_history {
   int ct;
   Public_Status_Code psc[MAX_MAX_TRIES];
} Retry_History; 

Retry_History * retry_history_new(){
   return calloc(1, sizeof(Retry_History));
}

void retry_history_free(Retry_History* history);
void retry_history_clear(Retry_History * history);
int  retry_history_add(Retry_History * history, Public_Status_Code psc);



#endif /* RETRY_HISTORY_H_ */
