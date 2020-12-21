/* @file rtti.h
 * Primitive runtime type information facilities
 */

// Copyright (C) 2018-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef RTTI_H_
#define RTTI_H_

#define RTTI_ADD_FUNC(_NAME) rtti_func_name_table_add(_NAME, #_NAME);

void   rtti_func_name_table_add(void * func_addr, const char * func_name);
char * rtti_get_func_name_by_addr(void * ptr);
void   dbgrpt_rtti_func_name_table(int depth);

#endif /* RTTI_H_ */
