/** @file feature_lists.h
 */

// Copyright (C) 2018=2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


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
