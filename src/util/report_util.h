/* report_util.h
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

/** @file report_util.h
 * Report utility package
 */

#ifndef REPORT_UTIL_H_
#define REPORT_UTIL_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdio.h>
#include <stdbool.h>
/** \endcond */

#include "coredefs.h"

void rpt_push_output_dest(FILE* new_dest);
void rpt_pop_output_dest();
FILE * rpt_cur_output_dest();
void rpt_reset_output_dest_stack();
void rpt_change_output_dest(FILE* new_dest);
void rpt_debug_output_dest();

int rpt_get_indent(int depth);

void rpt_flush();

void rpt_nl();
void rpt_title(char * title, int depth);
void rpt_label(int depth, char * text);
void rpt_multiline(int depth, ...);
void rpt_g_ptr_array(int depth, GPtrArray * strings);

void rpt_vstring(int depth, char * format, ...) ;
void rpt_2col(char * s1,  char * s2,  int col2offset, bool offset_absolute, int depth);
void rpt_structure_loc(const char * name, const void * ptr, int depth);
void rpt_hex_dump(const Byte * data, int size, int depth);
int rpt_file_contents(const char * fn, bool verbose, int depth);

// Remaining rpt_ functions share common formatting
void rpt_str(const char * name, char * info, const char * val, int depth);
void rpt_int(char * name, char * info, int val, int depth);
void rpt_bool(char * name, char * info, bool val, int depth);

/** Interpretation function used by rpt_mapped_int() */
typedef char * (*Value_To_Name_Function)(int val);
void rpt_mapped_int(char * name, char * info, int val, Value_To_Name_Function func, int depth);

void rpt_int_as_hex(char * name, char * info, int val, int depth);
void rpt_uint8_as_hex(char * name, char * info, unsigned char val, int depth) ;
void rpt_bytes_as_hex(const char * name, char * info, Byte * bytes, int ct, bool hex_prefix_flag, int depth);

typedef
struct {
   char * flag_name;
   char * flag_info;
   int    flag_val;
} Flag_Info;

typedef
struct {
   int         flag_info_ct;
   Flag_Info * flag_info_recs;
} Flag_Dictionary;

typedef
struct {
   int     flag_name_ct;
   char ** flag_names;
} Flag_Name_Set;

void rpt_ifval2(char * name,
                char * info,
                int    val,
                Flag_Name_Set *   pflagNameSet,
                Flag_Dictionary * pDict,
                int    depth);

#endif /* REPORT_UTIL_H_ */
