/*  stringutil.h
 *
 *  Created on: Nov 4, 2015
 *      Author: rock
 *
 *  String and hex conversion functions
 */

#ifndef STRINGUTIL_H_
#define STRINGUTIL_H_

#include <stdbool.h>

#include "util/coredefs.h"


//
// General
//


// Returns "true" or "false":
char * bool_repr(int value);


//
// String functions (other than hex)
//

bool   streq(const char * s1, const char * s2);
bool   is_abbrev(char * value, const char * longname, int minchars);
char * strupper(char * s);
char * strjoin(const char ** pieces, const int ct, const char * sepstr);
char * chars_to_string(char * start, int len);

typedef char** Null_Terminated_String_Array;

Null_Terminated_String_Array strsplit(char * str_to_split, char sepchar);
void null_terminated_string_array_free(Null_Terminated_String_Array string_array);
int null_terminated_string_array_length(Null_Terminated_String_Array string_array);


//
// Hex value conversion.
//

bool hhs_to_byte_in_buf(char * s,  Byte * result);    // converts null terminated string into buffer
bool hhc_to_byte_in_buf(char * hh, Byte * result);    // converts 2 characters at hh into buffer
Byte hhs_to_byte(char * s);                           // converts null terminated string
Byte hhc_to_byte(char * hh);                          // converts 2 characters at hh
void test_hhs_to_byte() ;
int  hhs_to_byte_array(char * hhs, Byte** pBa);

char * hexstring(Byte * bytes, int size);  // buffer returned must be freed
char * hexstring2(
          const unsigned char * bytes,      // bytes to convert
          int                   len,        // number of bytes
          const char *          sep,        // separator character (used how?)
          bool                  uppercase,  // use upper case hex characters?
          char *                buffer,     // buffer in which to return hex string
          int                   bufsz);     // buffer size

void hex_dump(Byte * bytes, int size);


#endif /* STRINGUTIL_H_ */
