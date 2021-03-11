/* @file rtti.h
 * Runtime trace information
 */

// Copyright (C) 2018-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef RTTI_H_
#define RTTI_H_

#define RTTI_ADD_FUNC(_NAME) rtti_func_name_table_add(_NAME, #_NAME);

void   rtti_func_name_table_add(void * func_addr, const char * func_name);
char * rtti_get_func_name_by_addr(void * ptr);
void * rtti_get_func_addr_by_name(char * name);
void   dbgrpt_rtti_func_name_table(int depth);

#endif /* RTTI_H_ */
