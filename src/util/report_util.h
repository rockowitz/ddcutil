/*
 * report_util.h
 *
 *  Created on: Jul 20, 2014
 *      Author: rock
 */

#ifndef REPORT_UTIL_H_
#define REPORT_UTIL_H_

#include <stdbool.h>
#include <util/string_util.h>


void rpt_push_output_dest(FILE* new_dest);
void rpt_pop_output_dest();
void rpt_reset_output_dest_stack();


void rpt_title(char * title, int depth);

void rpt_vstring(int depth, char * format, ...) ;

void rpt_structure_loc(char * name, void * ptr, int depth);

void rpt_str(char * name, char * info, char * val, int depth);

void rpt_int(char * name, char * info, int val, int depth);


typedef char * (*Value_To_Name_Function)(int val);


void rpt_mapped_int(char * name, char * info, int val, Value_To_Name_Function func, int depth);

void rpt_int_as_hex(char * name, char * info, int val, int depth);

void rpt_bytes_as_hex(char * name, char * info, Byte * bytes, int ct, bool hex_prefix_flag, int depth);

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


void rpt_bool(char * name, char * info, bool val, int depth);

int rpt_indent(int depth);

#endif /* REPORT_UTIL_H_ */
