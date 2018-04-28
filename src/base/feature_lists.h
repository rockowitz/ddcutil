/* feature_lists.h
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

#ifndef FEATURE_LISTS_H_
#define FEATURE_LISTS_H_

#include <stdbool.h>

#include "ddcutil_types.h"

void
feature_list_clear(
      DDCA_Feature_List* vcplist);

void
feature_list_add(
      DDCA_Feature_List * vcplist,
      uint8_t vcp_code);

bool
feature_list_contains(
      DDCA_Feature_List * vcplist,
      uint8_t vcp_code);

DDCA_Feature_List
feature_list_or(
      DDCA_Feature_List* vcplist1,
      DDCA_Feature_List* vcplist2);

DDCA_Feature_List
feature_list_and(
      DDCA_Feature_List* vcplist1,
      DDCA_Feature_List* vcplist2);


DDCA_Feature_List
feature_list_and_not(
      DDCA_Feature_List* vcplist1,
      DDCA_Feature_List* vcplist2);


int
feature_list_count(
      DDCA_Feature_List * feature_list);

char *
feature_list_string(
      DDCA_Feature_List * feature_list,
      char *              value_prefix,
      char *              sepstr);


#endif /* FEATURE_LISTS_H_ */
