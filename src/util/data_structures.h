/** @file data_structures.h
 *  General purpose data structures
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdint.h>
/** \endcond */

#ifdef __cplusplus
extern "C" {
#endif

// #ifndef Byte
// #define Byte unsigned char
// #endif
#include "coredefs_base.h"   // for Byte


typedef bool *IFilter(int i);

/** An opaque structure containing an array of bytes that
 *  can grow dynamically.  Note that the same byte value can
 * appear multiple times.
 */
typedef void * Byte_Value_Array;

Byte_Value_Array bva_create();
int              bva_length(Byte_Value_Array bva);
void             bva_append(Byte_Value_Array bva, Byte item);
Byte             bva_get(Byte_Value_Array bva, guint ndx);
bool             bva_contains(Byte_Value_Array bva, Byte item);
bool             bva_sorted_eq(Byte_Value_Array bva1, Byte_Value_Array bva2);
Byte *           bva_bytes(Byte_Value_Array bva);
char *           bva_as_string(Byte_Value_Array bva, bool as_hex, char * sep);
void             bva_free(Byte_Value_Array bva);
void             bva_report(Byte_Value_Array ids, char * title);
bool             bva_store_bytehex_list(Byte_Value_Array bva, char * start, int len);
Byte_Value_Array bva_filter(Byte_Value_Array bva, IFilter filter_func);
void             bva_sort(Byte_Value_Array bva);


/** An opaque data structure containing 256 flags */
typedef void * Byte_Bit_Flags;

Byte_Bit_Flags bbf_create();
void           bbf_free(Byte_Bit_Flags flags);
void           bbf_set(Byte_Bit_Flags flags, Byte val);
bool           bbf_is_set(Byte_Bit_Flags flags, Byte val);
Byte_Bit_Flags bbf_subtract(Byte_Bit_Flags bbflags1, Byte_Bit_Flags bbflags2);
char *         bbf_repr(Byte_Bit_Flags flags, char * buffer, int buflen);
int            bbf_count_set(Byte_Bit_Flags flags);  // number of bits set
int            bbf_to_bytes(Byte_Bit_Flags  flags, Byte * buffer, int buflen);
char *         bbf_to_string(Byte_Bit_Flags flags, char * buffer, int buflen);
bool           bbf_store_bytehex_list(Byte_Bit_Flags flags, char * start, int len);

/** Opaque iterator for #Byte_Bit_Flags */
typedef void * Byte_Bit_Flags_Iterator;

Byte_Bit_Flags_Iterator
               bbf_iter_new(Byte_Bit_Flags bbflags);
void           bbf_iter_free(Byte_Bit_Flags_Iterator bbf_iter);
void           bbf_iter_reset(Byte_Bit_Flags_Iterator bbf_iter);
int            bbf_iter_next(Byte_Bit_Flags_Iterator bbf_iter);


//
// Byte_Value_Array Byte_Bit_Flags cross-compatibility functions
//

bool bva_bbf_same_values( Byte_Value_Array bva , Byte_Bit_Flags bbf);

/** Function signature for passing function that appends a value to
 * either a #Byte_Bit_Flags or a #Byte_Value_Array
 */
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
#define VN(v) {v,#v,NULL}
/** \def VN_END
 * Terminating entry for a Value_Name table. */
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

/** A Value_Name_Title table is used to map byte values to their
 * symbolic names and description (title).
 * Each entry is a value/name/description triple..
 *
 * The table is terminated by an entry whose name and description fields are NULL.
 */
typedef struct {
   uint32_t value;       ///< value
   char *   name;        ///< symbolic name
   char *   title;       ///< value description
} Value_Name_Title;

typedef Value_Name_Title Value_Name_Title_Table[];

typedef Value_Name_Title       Value_Name;
typedef Value_Name_Title_Table Value_Name_Table;


char * vnt_name( Value_Name_Title* table, uint32_t val);
#ifdef TRANSITIONAL
#define vn_name vnt_name
#endif
char * vnt_title(Value_Name_Title* table, uint32_t val);

uint32_t vnt_find_id(
           Value_Name_Title_Table table,
           const char * s,
           bool use_title,       // if false, search by symbolic name, if true, search by title
           bool ignore_case,
           uint32_t default_id);

#define INTERPRET_VNT_FLAGS_BY_NAME false
#define INTERPRET VNT_FLAGS_BY_TITLE true
char * vnt_interpret_flags(
      uint32_t                flags_val,
      Value_Name_Title_Table  bitname_table,
      bool                    use_title,
      char *                  sepstr);


//
// Circular String Buffer
//

typedef struct {
   char **  lines;
   int      size;
   int      ct;
} Circular_String_Buffer;


/** Allocates a new #Circular_String_Buffer
 *
 *  @param  size  buffer size (number of entries)
 *  @return newly allocated #Circular_String_Buffer
 */
Circular_String_Buffer * csb_new(int size);
void csb_add(Circular_String_Buffer * csb, char * line, bool copy);
GPtrArray * csb_to_g_ptr_array(Circular_String_Buffer * csb);


typedef struct {
   uint8_t bytes[32];
} Bit_Set_256;

extern const Bit_Set_256 EMPTY_BIT_SET_256;

Bit_Set_256    bs256_add(Bit_Set_256 flags, uint8_t val);
bool           bs256_contains(Bit_Set_256 flags, uint8_t val);
bool           bs256_eq(Bit_Set_256 set1, Bit_Set_256 set2);
Bit_Set_256    bs256_or(Bit_Set_256 set1, Bit_Set_256 set2);         // union
Bit_Set_256    bs256_and(Bit_Set_256 set1, Bit_Set_256 set2);        // intersection
Bit_Set_256    bs256_and_not(Bit_Set_256 set1, Bit_Set_256 set2);    // subtract
int            bs256_count(Bit_Set_256 set);
char *         bs256_to_string(Bit_Set_256 set, const char * value_prefix, const char * septr);


/** Opaque iterator for Bit_Set_256 */
typedef void * Bit_Set_256_Iterator;

Bit_Set_256_Iterator
               bs256_iter_new(Bit_Set_256 bs256lags);
void           bs256_iter_free(Bit_Set_256_Iterator iter);
void           bs256_iter_reset(Bit_Set_256_Iterator iter);
int            bs256_iter_next(Bit_Set_256_Iterator  iter);

#ifdef __cplusplus
}    // extern "C"
#endif



#endif /* DATA_STRUCTURES_H */
