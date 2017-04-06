/* ddc_try_stats.h
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

#ifndef TRY_STATS_H_
#define TRY_STATS_H_

#define MAX_STAT_NAME_LENGTH  31

// Returns an opaque pointer to a Try_Data data structure
void * try_data_create(char * stat_name, int max_tries);

void try_data_reset(void * stats_rec);

void try_data_record_tries(void * stats_rec, int rc, int tryct);

int  try_data_get_total_attempts(void * stats_rec);

void try_data_report(void * stats_rec, int depth);

int  try_data_get_max_tries(void * stats_rec);

void try_data_set_max_tries(void* stats_rec,int new_max_tries);

#endif /* TRY_STATS_H_ */
