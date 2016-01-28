/* data_structures.h
 *
 * Created on: Jun 18, 2014
 *     Author: rock
 *
 * General purpose data structures
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

#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <stdbool.h>

#include "util/coredefs.h"   // for Byte

// An opaque structure containing an array of bytes that
// can grow dynamically.  Note that the same byte can
// appear multiple times.
typedef void * Byte_Value_Array;

Byte_Value_Array bva_create();
int  bva_length(Byte_Value_Array bva);
void bva_append(Byte_Value_Array bva, Byte item);
Byte bva_get(Byte_Value_Array bva, int ndx);
bool bva_contains(Byte_Value_Array bva, Byte item);
void bva_free(Byte_Value_Array bva);
void bva_report(Byte_Value_Array ids, char * title);
bool bva_store_bytehex_list(Byte_Value_Array bva, char * start, int len);


// An opaque data structure containing 256 flags
typedef void * Byte_Bit_Flags;

Byte_Bit_Flags bbf_create();
void   bbf_free(Byte_Bit_Flags flags);
void   bbf_set(Byte_Bit_Flags flags, Byte val);
bool   bbf_is_set(Byte_Bit_Flags flags, Byte val);
char * bbf_repr(Byte_Bit_Flags flags, char * buffer, int buflen);
int    bbf_count_set(Byte_Bit_Flags flags);  // number of bits set
char * bbf_to_string(Byte_Bit_Flags flags, char * buffer, int buflen);
bool   bbf_store_bytehex_list(Byte_Bit_Flags flags, char * start, int len);


//
// Byte_Value_Array Byte_Bit_Flags cross-compatibility functions
//

bool bva_bbf_same_values( Byte_Value_Array bva , Byte_Bit_Flags bbf);

typedef void (*Byte_Appender) (void * data_struct, Byte val);
void bva_appender(void * data_struct, Byte val);
void bbf_appender(void * data_struct, Byte val);
// Store a value in either a Byte_Value_Array or a Byte_Bit_Flag
bool store_bytehex_list(char * start, int len, void * data_struct, Byte_Appender appender);


// test case

void test_value_array();



//
// Buffer with length management
//

#define BUFFER_MARKER "BUFR"
typedef
struct {
   char       marker[4];      // "BUFR"
   Byte *     bytes;
   int        buffer_size;
   int        len;
} Buffer;

Buffer * buffer_new(int size, const char * trace_msg);
Buffer * buffer_dup(Buffer * srcbuf, const char * trace_msg);
int      buffer_length(Buffer * buffer);
void     buffer_set_length(Buffer * buffer, int bytect);
void     buffer_free(Buffer * buffer, const char * trace_msg);
void     buffer_put(Buffer * buffer, Byte * bytes, int bytect);
void     buffer_set_byte(Buffer * buffer, int offset, Byte byte);
void     buffer_set_bytes(Buffer * buffer, int offset, Byte * bytes, int bytect);
void     buffer_append(Buffer * buffer, Byte * bytes, int bytect);
void     buffer_dump(Buffer * buffer);
bool     buffer_eq(Buffer* buf1, Buffer* buf2);


#endif /* DATA_STRUCTURES_H */
