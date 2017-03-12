/* data_structures.h
 *
 * General purpose data structures
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

/** @file data_structures.h
 *  Generic data structures
 */

#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <stdbool.h>
#include <stdint.h>

#include "coredefs.h"   // for Byte


/** An opaque structure containing an array of bytes that
 *  can grow dynamically.  Note that the same byte value can
 * appear multiple times.
 */
typedef void * Byte_Value_Array;

Byte_Value_Array bva_create();
int              bva_length(Byte_Value_Array bva);
void             bva_append(Byte_Value_Array bva, Byte item);
Byte             bva_get(Byte_Value_Array bva, int ndx);
bool             bva_contains(Byte_Value_Array bva, Byte item);
Byte *           bva_bytes(Byte_Value_Array bva);
void             bva_free(Byte_Value_Array bva);
void             bva_report(Byte_Value_Array ids, char * title);
bool             bva_store_bytehex_list(Byte_Value_Array bva, char * start, int len);


/* An opaque data structure containing 256 flags */
typedef void * Byte_Bit_Flags;

Byte_Bit_Flags bbf_create();
void           bbf_free(Byte_Bit_Flags flags);
void           bbf_set(Byte_Bit_Flags flags, Byte val);
bool           bbf_is_set(Byte_Bit_Flags flags, Byte val);
Byte_Bit_Flags bbf_subtract(Byte_Bit_Flags bbflags1, Byte_Bit_Flags bbflags2);
char *         bbf_repr(Byte_Bit_Flags flags, char * buffer, int buflen);
int            bbf_count_set(Byte_Bit_Flags flags);  // number of bits set
char *         bbf_to_string(Byte_Bit_Flags flags, char * buffer, int buflen);
bool           bbf_store_bytehex_list(Byte_Bit_Flags flags, char * start, int len);


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
/** Buffer with length management */
typedef
struct {
   char       marker[4];      ///< always "BUFR"
   Byte *     bytes;          ///< pointer to internal buffer
   int        buffer_size;    ///< size of internal buffer
   int        len;            ///< number of bytes in buffer
   uint16_t   size_increment; ///< if > 0, auto-extend increment
} Buffer;

Buffer * buffer_new(int size, const char * trace_msg);
void     buffer_set_size_increment(Buffer * buffer, uint16_t increment);
Buffer * buffer_dup(Buffer * srcbuf, const char * trace_msg);
Buffer * buffer_new_with_value(Byte * bytes, int bytect, const char * trace_msg);
int      buffer_length(Buffer * buffer);
void     buffer_set_length(Buffer * buffer, int bytect);
void     buffer_free(Buffer * buffer, const char * trace_msg);
void     buffer_put(Buffer * buffer, Byte * bytes, int bytect);
void     buffer_set_byte(Buffer * buffer, int offset, Byte byte);
void     buffer_set_bytes(Buffer * buffer, int offset, Byte * bytes, int bytect);
void     buffer_append(Buffer * buffer, Byte * bytes, int bytect);
void     buffer_strcat(Buffer * buffer, char * str);
void     buffer_add(Buffer * buffer, Byte byte);
void     buffer_dump(Buffer * buffer);
bool     buffer_eq(Buffer* buf1, Buffer* buf2);
void     buffer_extend(Buffer* buf, int addl_bytes);

Buffer * bbf_to_buffer(Byte_Bit_Flags flags);


//
// Identifier id to name and description lookup
//

/** \def VN(v)
 * Creates a Value_Name table entry by specifying its symbolic name.
 *
 *  @param v symbolic name
 */
#ifdef OLD
#define VN(v) {v,#v}
#endif
#define VN(v) {v,#v,NULL}
/** \def VN_END
 * Terminating entry for a Value_Name table. */
#ifdef OLD
#define VN_END {0xff,NULL}
#endif
#define VN_END {0xff,NULL,NULL}

/** \def VNT(v,t)
 *  Creates a Value_Name_Title table entry by specifying its symbolic name
 *  and description
 *
 *  @param v symbolic name
 *  @param t symbol description
 */
#define VNT(v,t) {v,#v,t}
/** Terminating entry for a Value_Name_Title table. */
#define VNT_END {0xff,NULL,NULL}

/** A Value_Name struct struct is a pair containing
 *  value and its symbolic name.
 */
#ifdef OLD
typedef struct {
   Byte   value;   ///< byte value
   char * name;    ///< symbolic name
} Value_Name;
#endif

/** A Value_Name table is an array of Value_Name structs.
 *  It is used to map byte values to their symbolic names.
 *  Each Value_Name struct contains a value/name pair.
 *
 * The table is terminated by an entry whose name field is NULL.
 */
#ifdef OLD
typedef Value_Name Value_Name_Table[];
#endif

/** A Value_Name_Title table is used to map byte values to their
 * symbolic names and description (title).
 * Each entry is a value/name/description triple..
 *
 * The table is terminated by an entry whose name and description fields are NULL.
 */
typedef struct {
   Byte   value;         ///< byte value
   char * name;          ///< symbolic name
   char * title;         ///< value description
} Value_Name_Title;

typedef Value_Name_Title Value_Name_Title_Table[];

typedef Value_Name_Title       Value_Name;
typedef Value_Name_Title_Table Value_Name_Table;


#ifdef OLD
char * vn_name(  Value_Name*       table, uint32_t val);
#endif
char * vnt_name( Value_Name_Title* table, uint32_t val);
#define vn_name vnt_name
char * vnt_title(Value_Name_Title* table, uint32_t val);


uint32_t vnt_id_by_title(Value_Name_Title_Table table,
                         const char * title,
                         bool ignore_case,
                         uint32_t default_id);




//
// Misc
//

bool sbuf_append(char * buf, int bufsz, char * sepstr, char * nextval);

#ifdef OLD
char * interpret_named_flags_old(
      Value_Name * table,
      uint32_t     val,
      char *       buffer,
      int          bufsz,
      char *       sepstr);
#endif

char * interpret_named_flags(
          uint32_t       flags_val,
          Value_Name *   bitname_table,
          char *         sepstr,
          char *         buffer,
          int            bufsize );

char * interpret_vnt_flags_by_title(
          uint32_t       flags_val,
          Value_Name_Title_Table   bitname_table,
          char *         sepstr,
          char *         buffer,
          int            bufsz );

#endif /* DATA_STRUCTURES_H */
