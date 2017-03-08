/* string_util.h
 *
 * String and hex conversion functions
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef STRINGUTIL_H_
#define STRINGUTIL_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "util/coredefs.h"
#include "util/glib_util.h"


//
// General
//

// Returns "true" or "false":
char * bool_repr(int value);

#ifdef DEPRECATED
// use library function g_strlcpy() instead
#define SAFE_STRNCPY(dest, src, buflen) \
   do { \
      strncpy(dest, src, (buflen) ); \
      if (buflen > 0) \
         dest[buflen-1] = '\0'; \
   } while(0)
#endif

//
// String functions (other than hex)
//

bool   streq(const char * s1, const char * s2);
bool   is_abbrev(const char * value, const char * longname, int minchars);
bool   str_starts_with(const char * value_to_test, const char * start_part);
bool   str_ends_with(const char * value_to_test, const char * end_part);
bool   str_all_printable(char * s);
char * strupper(char * s);
char * strdup_uc(char* s);
char * strjoin( const char ** pieces, const int ct, const char * sepstr);
char * chars_to_string(char * start, int len);
char * strtrim(const char * s);
char * strtrim_r(const char * s, char * buffer, int bufsz);
char * rtrim_in_place(char * s);
char * substr(char * s, int startpos, int ct);
char * lsub(char * s, int ct);
char * str_replace_char(char * s, char old_char, char new_char);
char * strcat_new(char * s1, char * s2);

typedef bool (*String_Comp_Func)(const char * a, const char * b);
int matches_by_func(    char * word, char ** null_terminated_string_array, String_Comp_Func  comp_func);
int exactly_matches_any(char * word, char ** null_terminated_string_array);
int starts_with_any(    char * word, char ** null_terminated_string_array);

typedef char** Null_Terminated_String_Array;
void ntsa_free(  Null_Terminated_String_Array string_array);
int  ntsa_length(Null_Terminated_String_Array string_array);
void ntsa_show(  Null_Terminated_String_Array string_array);

Null_Terminated_String_Array strsplit(const char * str_to_split, const char* delims);
Null_Terminated_String_Array strsplit_maxlength(
                                      const char * str_to_split,
                                      uint16_t     max_piece_length,
                                      const char * delims);

GPtrArray * ntsa_to_g_ptr_array(Null_Terminated_String_Array ntsa);
Null_Terminated_String_Array g_ptr_array_to_ntsa(GPtrArray * garray);


//
// Integer conversion
//

bool str_to_int(const char * nptr, int * ival);


//
// Hex value conversion.
//

bool hhs_to_byte_in_buf(char * s,  Byte * result);    // converts null terminated string into buffer
bool any_one_byte_hex_string_to_byte_in_buf(char * s, Byte * result);
bool hhc_to_byte_in_buf(char * hh, Byte * result);    // converts 2 characters at hh into buffer
Byte hhs_to_byte(char * s);                           // converts null terminated string
Byte hhc_to_byte(char * hh);                          // converts 2 characters at hh
void test_hhs_to_byte() ;
int  hhs_to_byte_array(char * hhs, Byte** pBa);

char * hexstring(const Byte * bytes, int size);  // buffer returned must be freed
char * hexstring2(
          const unsigned char * bytes,      // bytes to convert
          int                   len,        // number of bytes
          const char *          sepstr,     // separator string between hex digits
          bool                  uppercase,  // use upper case hex characters?
          char *                buffer,     // buffer in which to return hex string
          int                   bufsz);     // buffer size

void fhex_dump_indented(FILE * fh, const Byte* data, int size, int indents);
void fhex_dump(FILE * fh, const Byte* bytes, int size);
void hex_dump(const Byte * bytes, int size);


//
// Standard function variants that handle stream == NULL
//

int f0putc(int c, FILE * stream);
int f0puts(const char * s, FILE * stream);
int f0printf(FILE * stream, const char * format, ...);
int vf0printf(FILE * stream, const char * format, va_list ap);

#endif /* STRINGUTIL_H_ */
