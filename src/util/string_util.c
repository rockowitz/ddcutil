/* string_util.c
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** @file string_util.c
 *  String utility functions
 */

/** \cond */
#include <assert.h>
#include <ctype.h>
#include <glib-2.0/glib.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
/** \endcond */

#include "glib_util.h"

#include "string_util.h"


// Direct writes to stdout/stderr:
//    debug messages
//    stderr: hhs_to_byte() before terminating execution because of bad value


//
// General
//

/** Returns a character string representation of an integer as a boolean value.
 *
 * @param value value to represent
 * @return "true" or "false"
 */
char * bool_repr(int value) {
   char * answer = (value) ? "true" : "false";
   return answer;
}


//
// String functions (other than hex)
//

/** Compares 2 strings for equality, handling nulls
 *
 *  @param s1  first string
 *  @param s2  second string
 *  @return true if the strings match, false if not
 */
bool streq(const char * s1, const char * s2) {
   bool result = false;
   if ( (s1 == NULL && s2 == NULL) ||
        (s1 != NULL && s2 != NULL && (strcmp(s1, s2) == 0) )
      )
      result = true;
   return result;
}


/** Tests if one string is a valid abbreviation of another.
 *
 * @param  value     is this string an abbreviation?
 * @param  longname  unabbreviated value
 * @param  minchars  minimum number of characters that must match
 * @return true/false
 */
bool is_abbrev(const char * value, const char * longname, int minchars) {
   bool result = false;
   int vlen = strlen(value);
   if ( vlen >= minchars &&
        vlen <= strlen(longname) &&
        memcmp(value, longname, vlen) == 0
      )
      result = true;

   // printf("(%s) value=|%s|, longname=|%s| returning %d\n", __func__, value, longname, result);
   return result;
}


/** Tests if string starts with a string.
 *
 * @param  value_to_test  value to examine
 * @param  prefix         prefix to check for
 * @return true/flase
 *
 * @remark Consider using lib function g_str_prefix() ?? instead
 */
bool str_starts_with(const char * value_to_test, const char * prefix) {
   return is_abbrev(prefix, value_to_test, strlen(prefix));
}

/** Tests if string ends with a string.
 *
 * @param  value_to_test  value to examine
 * @param  suffix         substring to check for
 * @return true/flase
 *
 * @remark Consider using lib function g_str_suffix() ?? instead
 */
bool str_ends_with(const char * value_to_test, const char * suffix) {
   bool debug = false;
   if (debug)
      printf("(%s) value_to_test=|%s|, end_part=|%s|\n", __func__, value_to_test, suffix);
   int value_len = strlen(value_to_test);
   int end_part_len = strlen(suffix);
   bool result = false;
   if (end_part_len <=value_len) {
      int startpos = value_len-end_part_len;
      result = streq(value_to_test+startpos, suffix);
   }
   if (debug)
      printf("(%s) returning: %d\n", __func__, result);
   return result;
}


/** Are all characters in the string printable?
 *
 * @param s   string to test
 * @return    true/false  (true if s==NULL)
 */
bool str_all_printable(const char * s) {
   bool result = true;
   if (s) {
      for (int ndx = 0; ndx < strlen(s); ndx++) {
         if (!isprint(s[ndx])) {
            result = false;
            break;
         }
      }
   }
   return result;
}


/** Compares a string to a null-terminated array of strings, using a specified
 *  comparison function.
 *
 *  @param s          string to test
 *  @param match_list null terminated array of strings to test against
 *  @param comp_func  comparison function
 *
 *  @retval >= 0 index of first entry in list for which the comparison function succeeds
 *  @retval -1   no match
 */
int matches_by_func(const char * s, char ** match_list, String_Comp_Func comp_func) {
   int result = -1;
   int ndx = 0;
   for (ndx=0; match_list[ndx] != NULL; ndx++) {
      if ( (*comp_func)(s, match_list[ndx])) {
         result = ndx;
         break;
      }
   }
   return result;
}


/** Tests if a string exactly matches any string in a null-terminated
 *  array of strings.  (Null_Terminated_String_Array).
 *
 *  @param  s           string to test for
 *  @param  match_list  null terminated array of pointers to strings
 *
 *  @retval >= 0  index of matching array entry
 *  @retval -1    no match
 */
int exactly_matches_any(const char * s, char ** match_list) {
   return matches_by_func(s, match_list, streq);
}


/** Finds the first entry in a null terminated array of strings
 *  that is the initial portion of a specified string.
 *
 *  @param  s     string to test against
 *  @param  match_list  array of prefix strings (null-terminated)
 *
 *  @retval >= 0 index of matching prefix
 *  @retval -1   not found
 */
int starts_with_any(const char * s, char ** match_list) {
   return matches_by_func(s, match_list, str_starts_with);
}


/** Trims leading and trailing whitespace from a string and
 * returns the result in a buffer provided by the caller.
 * If the buffer is insufficiently large, the result string
 * is truncated.
 *
 * The result is always null terminated.
 *
 * @param  s      string to trim (not modified)
 * @param  buffer where to return result
 * @param  bufsz  buffer size
 *
 * @return pointer to truncated string (i.e. buffer)
 */
char * strtrim_r(const char * s, char * buffer, int bufsz) {
   bool debug = false;
   if (debug)
      printf("(%s) s=|%s|\n", __func__, s);
   int slen = strlen(s);
   int startpos = 0;
   int lastpos  = slen-1;   // n. -1 for 1 length string
   while ( startpos < slen && isspace(s[startpos]) )
      startpos++;
   if (startpos < slen) {
      while ( lastpos >= startpos && isspace(s[lastpos]))
         lastpos--;
   }
   int tlen = 1 + lastpos - startpos;
   if (debug)
      printf("(%s) startpos=%d, lastpos=%d, tlen=%d\n", __func__, startpos, lastpos, tlen);
   if (tlen > (bufsz-1))
      tlen = bufsz-1;
   memcpy(buffer, s+startpos, tlen);
   buffer[tlen] = '\0';
   if (debug)
      printf("(%s) returning |%s|\n", __func__, buffer);
   return buffer;
}


/** Trims trailing whitespace from a string.
 *
 * @param s string to trim
 * @return s
 *
 * @remark
 * Particularly useful for stripping trailing newlines.
 */
char * rtrim_in_place(char * s) {
   int len = strlen(s);
   while(len > 0 && isspace(s[len-1])) {
      len--;
      s[len] = '\0';
   }
   return s;
}


/** Trims leading and trailing whitespace from a string and
 * returns the result in newly allocated memory.
 * It is the caller's responsibility to free this memory.
 * The result string is null terminated.
 *
 * @param  s      string to trim (not modified)
 * @return truncated string in newly allocated memory
 */
char * strtrim(const char * s) {
   int bufsz = strlen(s)+1;
   char * buffer = calloc(1,bufsz);
   strtrim_r(s, buffer, bufsz);
   return buffer;
}


/** Extracts a substring from a string
 *
 * @param s         string to process
 * @param startpos  starting position (0 based)
 * @param ct        number of characters; if ct + startpos is greater than
 *                  the string length, ct is reduced accordingly
 * @return extracted substring, in newly allocated memory
 */
char * substr(const char * s, int startpos, int ct) {
   assert(startpos >= 0);
   assert(ct>=0);
   if (startpos + ct > strlen(s))
      ct = strlen(s) - startpos;
   char * result = calloc(ct+1, sizeof(char));
   strncpy(result, s+startpos, ct);
   result[ct] = '\0';
   return result;
}

/** Returns the initial portion of a string
 *
 * @param s         string to process
 * @param ct        number of characters; if ct is greater than
 *                  the string length, ct is reduced accordingly
 * @return extracted substring, in newly allocated memory
 */
char * lsub(const char * s, int ct) {
   return substr(s, 0, ct);
}


/** Joins an array of strings into a single string, using a separator string.
 *
 * @param  pieces   array of strings
 * @param  ct0      number of strings, if < 0 the array is null terminated
 * @param  sepstr   separator string, if NULL then no separator string
 *
 * @return joined string (null terminated)
 *
 * The returned string has been malloc'd.  It is the responsibility of
 * the caller to free it.
 */
char * strjoin( const char ** pieces, const int ct0, const char * sepstr) {
   // printf("(%s) ct0=%d, sepstr=|%s|\n", __func__, ct0, sepstr);
   int total_length = 0;
   int ndx;
   int seplen = (sepstr) ? strlen(sepstr) : 0;  // sepstr may be null

   int max_ct = (ct0 < 0) ? 9999 : ct0;
   for (ndx=0; ndx < max_ct && pieces[ndx]; ndx++) {
      total_length += strlen(pieces[ndx]);
      if (ndx > 0)
         total_length += seplen;
   }
   total_length += 1;   // for terminating null
   int ct = ndx;

   // printf("(%s) ct=%d, total_length=%d\n", __func__, ct, total_length);
   char * result = malloc(total_length);
   char * end = result;
   for (ndx=0; ndx<ct; ndx++) {
      if (ndx > 0 && seplen > 0) {
         strcpy(end, sepstr);
         end += strlen(sepstr);
      }
      strcpy(end, pieces[ndx]);
      end += strlen(pieces[ndx]);
   }
   // printf("(%s) result=%p, end=%p\n", __func__, result, end);
   assert(end == result + total_length -1);
   return result;
}

#ifdef FUTURE
// YAGNI: String_Array

typedef struct {
   int  max_ct;
   int  cur_ct;
   char** s;
} String_Array;


String_Array* new_string_array(int size) {
   String_Array * result = calloc(1, sizeof(String_Array));
   result->max_ct = size;
   result->cur_ct = 0;
   result->s = calloc(sizeof(char*), size);
   return result;
}
#endif


/** Splits a string based on a list of delimiter characters.
 *
 *  @param  str_to_split     string to be split
 *  @param  delims           string of delimiter characters
 *  @return null terminated array of pieces
 *
 * Note: Each character in delims is used as an individual test.
 * The full string is NOT a delimiter string.
 */
Null_Terminated_String_Array strsplit(const char * str_to_split, const char * delims) {
   bool debug = false;
   int max_pieces = (strlen(str_to_split)+1);
   if (debug)
      printf("(%s) str_to_split=|%s|, delims=|%s|, max_pieces=%d\n", __func__, str_to_split, delims, max_pieces);

   char** workstruct = calloc(sizeof(char *), max_pieces+1);
   int piecect = 0;

   char * str_to_split_dup = strdup(str_to_split);
   char * rest = str_to_split_dup;
   char * token;
   // originally token assignment was in while() clause, but valgrind
   // complaining about uninitialized variable, trying to figure out why
   token = strsep(&rest, delims);      // n. overwrites character found
   while (token) {
      // printf("(%s) token: |%s|\n", __func__, token);
      if (strlen(token) > 0)
         workstruct[piecect++] = strdup(token);
      token = strsep(&rest, delims);
   }
   if (debug)
      printf("(%s) piecect=%d\n", __func__, piecect);
   char ** result = calloc(sizeof(char *), piecect+1);
   // n. workstruct[piecect] == NULL because we used calloc()
   memcpy(result, workstruct, (piecect+1)*sizeof(char*) );
   if (debug) {
      int ndx = 0;
      char * curpiece = result[ndx];
      while (curpiece != NULL) {
         printf("(%s) curpiece=%p |%s|\n", __func__, curpiece, curpiece);
         ndx++;
         curpiece = result[ndx];

      }
   }
   free(workstruct);
   free(str_to_split_dup);
   return result;
}


/** Splits a string into segments, each of which is no longer
 *  that a specified number of characters.  If delimiters are
 *  specified, then they are used to split the string into segments.
 *  Otherwise all segments, except possibly the last, are
 *  #max_piece_length in length.
 *
 *  @param  str_to_split     string to be split
 *  @param  max_piece_length maximum length of each segment
 *  @param  delims           string of delimiter characters
 *  @return null terminated array of pieces
 *
 * @remark
 * Each character in #delims is used as an individual test.
 * The full string is NOT a delimiter string.
 */
Null_Terminated_String_Array
strsplit_maxlength(
      const char *  str_to_split,
      uint16_t      max_piece_length,
      const char *  delims)
{
   bool debug = false;
   if (debug)
      printf("(%s) max_piece_length=%u, delims=|%s|, str_to_split=|%s|\n",
             __func__, max_piece_length, delims, str_to_split);

   GPtrArray * pieces = g_ptr_array_sized_new(20);
   char * str_to_split2 = strdup(str_to_split);   // work around constness
   char * start = str_to_split2;
   char * str_to_split2_end = str_to_split2 + strlen(str_to_split);
   if (debug)
      printf("(%s)x start=%p, str_to_split2_end=%p\n", __func__, start, str_to_split2_end);
   while (start < str_to_split2_end) {
      if (debug)
         printf("(%s) start=%p, str_to_split2_end=%p\n", __func__, start, str_to_split2_end);
      char * end = start + max_piece_length;
      if (end > str_to_split2_end)
         end = str_to_split2_end;
      // int cursize = end-start;
      // printf("(%s) end=%p, start=%p, cursize=%d, max_piece_length=%d\n",
      //        __func__, end, start, cursize, max_piece_length);
      if ( end < str_to_split2_end) {
         // printf("(%s) Need to split. \n", __func__);
         if (delims) {
            char * last = end-1;
            while(last >= start) {
               // printf("(%s) last = %p\n", __func__, last);
               if (strchr(delims, *last)) {
                  end = last+1;
                  break;
               }
               last--;
            }
         }
      }
      char * piece = strndup(start, end-start);
      g_ptr_array_add(pieces, piece);
      start = start + strlen(piece);
   }

   Null_Terminated_String_Array result =
          g_ptr_array_to_ntsa(pieces);
   g_ptr_array_free(pieces, false);
   free(str_to_split2);
   if (debug)
      ntsa_show(result);
   return result;
}


/** Frees a null terminated array of strings.
 *
 *  @param string_array null terminated array of pointers to strings
 *  @param free_strings if try, each string in the array is freed as well
 */
void ntsa_free(Null_Terminated_String_Array string_array, bool free_strings) {
   if (string_array) {
      if (free_strings) {
      int ndx = 0;
      while (string_array[ndx] != NULL)
         free(string_array[ndx++]);
      }
      free(string_array);
   }
}


/** Returns the number of strings in a null terminated array of strings.
 *
 * @param  string_array null terminated array of pointers to strings
 * @return number of strings in the array
 */
int ntsa_length(Null_Terminated_String_Array string_array) {
   assert(string_array);
   int ndx = 0;
   while (string_array[ndx] != NULL) {
      ndx++;
   }
   return ndx;
}

Null_Terminated_String_Array ntsa_join(
      Null_Terminated_String_Array a1,
      Null_Terminated_String_Array a2,
      bool dup)
{
   int ct = ntsa_length(a1) + ntsa_length(a2);
   Null_Terminated_String_Array result = calloc((ct+1), sizeof(char *));
   char ** to = result;
   char ** from = a1;
   while (*from) {
      if (dup)
         *to = strdup(*from);
      else
         *to = *from;
      to++;
      from++;
   }
   from = a2;
   while (*from) {
      if (dup)
         *to = strdup(*from);
      else
         *to = *from;
      to++;
      from++;
   }
   return result;
}





int ntsa_findx(
      Null_Terminated_String_Array string_array,
      char *                       value,
      String_Comp_Func             func)
{
   assert(string_array);
   int result = -1;
   int ndx = 0;
   char * s = NULL;
   while ( (s=string_array[ndx]) ) {
      // printf("(%s) checking ndx=%d |%s|\n", __func__, ndx, s);
      if (func(s,value)) {
         result = ndx;
         break;
      }
      ndx++;
   }
   // printf("(%s) Returning: %d\n", __func__, result);
   return result;

}

int  ntsa_find(  Null_Terminated_String_Array string_array, char * value) {
   return ntsa_findx(string_array, value, streq);
}


/* Reports the contents of a Null_Terminated_String_Array.
 *
 * @param string_array null-terminated string array
 *
 * @remark This is not a **report** function as that would make string_util
 * depend on report_util, creating a circular dependency within util
 */
void ntsa_show(Null_Terminated_String_Array string_array) {
   assert(string_array);
   printf("Null_Terminated_String_Array at %p:\n", string_array);
   int ndx = 0;
   while (string_array[ndx]) {
      printf("  %p: |%s|\n", string_array[ndx], string_array[ndx]);
      ndx++;
   }
   printf("Total entries: %d\n", ndx);
}


/** Converts a Null_Terminated_String_Array to a GPtrArray of pointers to strings.
 * The underlying strings are referenced, not duplicated.
 *
 * @param  ntsa  null-terminated array of strings
 * @return newly allocate GPtrArray
 */
GPtrArray * ntsa_to_g_ptr_array(Null_Terminated_String_Array ntsa) {
   int len = ntsa_length(ntsa);
   GPtrArray * garray = g_ptr_array_sized_new(len);
   int ndx;
   for (ndx=0; ndx<len; ndx++) {
      g_ptr_array_add(garray, ntsa[ndx]);
   }
   return garray;
}


/** Converts a GPtrArray if pointers to strings to a Null_Terminated_String_Array.
 * The underlying strings are referenced, not duplicated.
 *
 * @param gparray pointer to GPtrArray
 * @return null-terminated array of string pointers
 */
Null_Terminated_String_Array g_ptr_array_to_ntsa(GPtrArray * gparray) {
   assert(gparray);
   Null_Terminated_String_Array ntsa = calloc(gparray->len+1, sizeof(char *));
   for (int ndx=0; ndx < gparray->len; ndx++) {
      ntsa[ndx] = g_ptr_array_index(gparray,ndx);
   }
   return ntsa;
}


/** Converts an ASCII string to upper case.  The original string is converted in place.
 *
 * @param  s string to force to upper case
 *
 * @return converted string
 */
char * strupper(char * s) {
   if (s) {     // check s not a null pointer
      char * p = s;
      while(*p) {
         *p = toupper(*p);
         p++;
      }
   }
   return s;
}


/** Creates an upper case copy of an ASCII string
 *
 * Arguments:
 * @param  s  string to copy
 * @return newly allocated string, NULL if s is NULL
 */
char * strdup_uc(const char* s) {
   if (!s)
      return NULL;
   char * us = strdup( s );
   char * p = us;
   while (*p) {*p=toupper(*p); p++; }
   return us;
}


/* Replaces all instances of a character in a string with a different character.
 * The original string is converted in place.
 *
 * Arguments:
 *   s   string to force to upper case
 *
 * Returns:
 *   s   converted string
 */
char * str_replace_char(char * s, char old_char, char new_char) {
   if (s) {
      char * p = s;
      while (*p) {
         if (*p == old_char)
            *p = new_char;
         p++;
      }
   }
   return s;
}

/** Concatenates 2 strings into a newly allocated buffer.
 *
 * @param s1 first string
 * @param s2 second string
 * @return newly allocated string
 */
char * strcat_new(char * s1, char * s2) {
    assert(s1);
    assert(s2);
    char * result = malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcpy(result+strlen(s1), s2);
    return result;
}


/** Converts a sequence of characters into a (null-terminated) string.
 *
 *  @param start   pointer to first character
 *  @param len     number of characters
 *
 *  @return newly allocated string,
 *          NULL if start was NULL (is this the most useful behavior?)
 */
char * chars_to_string(const char * start, int len) {
   assert(len >= 0);
   char * strbuf = NULL;
   if (start) {
      strbuf = malloc(len+1);
      memcpy(strbuf, start, len);
      *(strbuf + len) = '\0';
   }
   return strbuf;
}


/** Appends a value to a string in a buffer.
 *
 * @param buf     pointer to character buffer
 * @param bufsz   buffer size
 * @param sepstr  if non-null, separator string to insert
 * @param nextval value to append
 *
 * @retval true   string was truncated
 * @retval false  normal append
 *
 * @remark
 * Consider allowing the truncation maker, currently "..." to be
 * specified as a parameter.
 */
bool sbuf_append(char * buf, int bufsz, char * sepstr, char * nextval) {
   assert(buf && (bufsz > 4) );   //avoid handling pathological case
   bool truncated = false;
   int seplen = (sepstr) ? strlen(sepstr) : 0;
   int maxchars = bufsz-1;
   int newlen = ( strlen(buf) == 0 )
                     ? strlen(nextval)
                     : ( strlen(buf) + seplen + strlen(nextval));
   if (newlen <= maxchars) {
      if (strlen(buf) > 0 && sepstr)
         strcat(buf, sepstr);
      strcat(buf, nextval);
   }
   else {
      if ( strlen(buf) < (maxchars-3) )
         strcat(buf, "...");
      else
         strcpy(buf+(maxchars-3), "...");
      truncated = true;
   }
   return truncated;
}


//
// Integer conversion
//

/** Converts a string representing an integer to an integer value.
 *
 * @param sval   string representing an integer
 * @param p_ival address at which to store integer value
 * @return true if conversion succeeded, false if it failed
 *
 * \remark
 * If conversion fails, the value pointed to by **p_ival** is unchanged.
 * @remark
 * This function wraps system function strtol(), hiding the ugly details.
 */
bool str_to_int(const char * sval, int * p_ival) {
   bool debug = false;
   if (debug)
      printf("(%s) sval->|%s|\n", __func__, sval);

   char * endptr;
   bool ok = false;
   if ( *sval != '\0') {
      long result = strtol(sval, &endptr, 10);
      // printf("(%s) sval=%p, endptr=%p, *endptr=|%c| (0x%02x), result=%ld\n",
      //        __func__, sval, endptr, *endptr, *endptr, result);
      if (*endptr == '\0') {
         *p_ival = result;
         ok = true;
      }
   }

   if (debug) {
      if (ok)
        printf("(%s) sval=%s, Returning: %s, *ival = %d\n", __func__, sval, bool_repr(ok), *p_ival);
      else
        printf("(%s) sval=%s, Returning: %s\n", __func__, sval, bool_repr(ok));
   }
   return ok;
}


//
// Hex value conversion.
//


/** Converts a (null terminated) string of 2 hex characters to
 * its byte value.
 *
 * @param  s       pointer to hex string
 * @param  result  pointer to byte in which converted value will be returned
 *
 * @return true  if successful conversion,
 *         false if string does not consist of hex characters,
 *               or is not 2 characters in length.
 */
bool hhs_to_byte_in_buf(const char * s, Byte * result) {
   // printf("(%s) Starting s=%s, strlen(s)=%zd\n", __func__, s, strlen(s) );
   // consider changing to fail if len != 2, or perhaps len != 1,2
   //assert(strlen(s) == 2);

   bool ok = true;
   if (strlen(s) != 2)
      ok = false;
   else {
   char * endptr = NULL;
   errno = 0;
   long longtemp = strtol(s, &endptr, 16 );
   int errsv = errno;
   // printf("(%s) After strtol, longtemp=%ld  \n", __func__, longtemp );
   // printf("errno=%d, s=|%s|, s=0x%02x &s=%p, longtemp = %ld, endptr=%p, *endptr=0x%02x\n",
   //        errsv, s, s, &s, longtemp, endptr,*endptr);
   // if (*endptr != '\0' || errsv != 0) {
   if (endptr != s+2 || errsv != 0) {
      ok = false;
   }
   else
      *result = (Byte) longtemp;
   }

   // printf("(%s) Returning ok=%d\n", __func__, ok);
   return ok;
}


/** Converts a hex string representing a single byte into its byte value.
 *  This is a more lenient version of hhs_to_byte_in_buf(), allowing
 *  the value to begin with "0x" or "x", or end with "h".  The allowed
 *  prefix or suffix is case-insensitive.
 *
 *  @param  s       pointer to hex string
 *  @param  result  pointer to byte in which result will be returned
 *
 *  @return **true**  if successful conversion,
 *          **false** if not
 */
bool any_one_byte_hex_string_to_byte_in_buf(const char * s, Byte * result) {
   // printf("(%s) s = |%s|\n", __func__, s);
   char * suc = strdup_uc(s);
   char * suc0 = suc;
   if (str_starts_with(suc, "0X"))
         suc = suc + 2;
   else if (*suc == 'X')
         suc = suc + 1;
   else if (str_ends_with(suc, "H"))
         *(suc+strlen(suc)-1) = '\0';
   bool ok = hhs_to_byte_in_buf(suc, result);
   free(suc0);
   // printf("(%s) returning %d, *result=0x%02x\n", __func__, ok, *result);
   return ok;
}


/** Converts 2 hex characters to their corresponding byte value.
 *  The characters need not be null terminated.
 *
 *  @param p_hh      pointer to hex characters.
 *  @param converted pointer go byte in which converted value will be returned
 *
 *  @return true if successful conversion, false if s does not point
 *          to hex characters
 */
bool hhc_to_byte_in_buf(const char * p_hh, Byte * converted) {
   // printf("(%s) Starting p_hh=%.2s   \n", __func__, hh );
   char hhs[3];
   hhs[0] = p_hh[0];
   // hhs[1] = cc[1];   // why does compiler complain?
   hhs[1] = *(p_hh+1);
   hhs[2] = '\0';
   return  hhs_to_byte_in_buf(hhs, converted);
}


#ifdef DEPRECATED
/* Converts a (null terminated) string of 2 hex characters to
 * its byte value.
 *
 * Arguments:
 *   s   pointer to hex string
 *
 * Returns:
 *   byte value
 *
 * Execution terminates if invalid hex value.
 */
Byte hhs_to_byte(char * s) {
   // printf("(%s) Starting s=%s, strlen(s)=%d   \n", __func__, s, strlen(s) );
   Byte converted;
   if (!hhs_to_byte_in_buf(s, &converted)) {
      // no way to properly signal failure, so terminate execution
      // don't call a function such as program_logic_error() since this
      // file should have no dependencies on any other program files.
      fprintf(stderr, "Invalid hex value: %s", s);
      // exit(1);   //  warnings in OBS re exit() in library
      converted = 0x00;    // stick something in there, ugh
   }
   return converted;
}
#endif

#ifdef DEPRECATED
/* Converts 2 hex characters to a single byte.
 *
 * Arguments:
 *   hh    address of 2 hex characters, need not be null terminated
 *
 * Returns:
 *   byte value
 *
 * Execution terminates if invalid hex value.
 */
Byte hhc_to_byte(char * hh) {
   // printf("(%s) Starting hh=%.2s   \n", __func__, hh );
   char hhs[3];
   hhs[0] = hh[0];
   // hhs[1] = cc[1];   // why does compiler complain?
   hhs[1] = *(hh+1);
   hhs[2] = '\0';
   return hhs_to_byte(hhs);
}
#endif


/** Converts a string of hex characters (null terminated) to an array of bytes.
 *
 *  @param   hhs     string of hex characters
 *  @param   pBa     address at which to return pointer to byte array
 *
 *  @retval  >= 0 number of bytes in array,
 *  @retval  -1   string could not be converted
 *
 * If successful, the byte array whose address is returned in pBa has
 * been malloc'd.  It is the responsibility of the caller to free it.
 */
int hhs_to_byte_array(const char * hhs, Byte** pBa) {
   if ( strlen(hhs) % 2)     // if odd number of characters
      return -1;
   char xlate[] = "0123456789ABCDEF";
   int bytect = strlen(hhs)/2;
   Byte * ba = malloc(bytect);
   bool ok = true;

   const char * h = hhs;
   Byte * b = ba;
   for (;  *h && ok; b++) {
      char ch0 = toupper(*h++);
      char ch1 = toupper(*h++);
      char * pos0 = strchr(xlate, ch0);
      char * pos1 = strchr(xlate, ch1);
      if (pos0 && pos1) {
         *b = (pos0-xlate) * 16 + (pos1-xlate);
      }
      else {
         ok = false;
      }
   }

   if (!ok) {
      free(ba);
      bytect = -1;
   }
   else {
      *pBa = ba;
   }
   return bytect;
}

#ifdef DEPRECATED
void test_one_hhs2Byte(char * hhs) {
   printf("(%s) Starting.  hhs=%s  \n", __func__, hhs );
   Byte b1 = hhs_to_byte(hhs);
   printf("(%s) %s -> 0x%02x  \n", __func__, hhs, b1 );
}


void test_hhs_to_byte() {
   printf("(%s) Startomg  \n", __func__ );
   test_one_hhs2Byte("01");
   test_one_hhs2Byte("ZZ");
   // test_one_hhs2Byte("123");
}
#endif


/** Converts a sequence of bytes to its representation as a string of hex characters.
 *
 *  @param  bytes     pointer to bytes
 *  @param  len       number of bytes
 *
 *  @return pointer to newly allocated hex string
 *
 * The value returned by this function has been malloc'd.   It is the
 * responsibility of the caller to free the memory.
 */
char * hexstring(const unsigned char * bytes, int len) {
   int alloc_size = 3*len + 1;
   char* str_buf = malloc(alloc_size);

   int i;
   for (i = 0; i < len; i++) {
      // printf("(%s) 3*i = %d, alloc_size-3*i = %d\n", __func__, 3*i, alloc_size-3*i);
      snprintf(str_buf+3*i, alloc_size-3*i, "%02x ", bytes[i]);
   }
   // printf ("(%s) Final null offset: %d\n", __func__, 3*len-1);
   str_buf[3*len-1] = 0x00;
   // printf("(%s) Returning: |%s|\n", __func__, str_buf);
   return str_buf;
}


/** Converts a sequence of bytes to its representation as a string of hex characters.
 *
 * @param   bytes    pointer to bytes
 * @param   len      number of bytes
 * @param   sepstr   string to separate each 2 hex character pairs representing a byte,
 *                  if NULL then no separators will be inserted
 * @param   uppercase if true, use uppercase hex characters,
 *                    if false, use lower case hex characters
 * @param   buffer    pointer to buffer in which hex string will be returned,
 *                    if NULL, then a buffer will be allocated
 * @param   bufsz     size of buffer
 *                    if 0, then a buffer will be allocated
 *
 * @return  pointer to hex string
 *
 * If this function allocates a buffer, it is the responsibility of the caller
 * to free the memory.
 */
char * hexstring2(
          const unsigned char * bytes,
          int                   len,
          const char *          sepstr,
          bool                  uppercase,
          char *                buffer,
          int                   bufsz)
{
   // if (len > 1)
   // printf("(%s) bytes=%p, len=%d, sepstr=|%s|, uppercase=%s, buffer=%p, bufsz=%d\n", __func__,
   //       bytes, len, sepstr, bool_repr(uppercase), buffer, bufsz);
   int sepsize = 0;
   if (sepstr) {
      sepsize = strlen(sepstr);
   }
   int required_size =   2*len             // hex rep of bytes
                       + (len-1)*sepsize   // for separators
                       + 1;                // terminating null
   // if (len > 1)
   // printf("(%s) required_size=%d\n", __func__, required_size);
   // special case:
   if (len == 0)
      required_size = 1;

   if (!buffer)
      bufsz = 0;
   assert (bufsz == 0 || bufsz >= required_size);
   if (bufsz == 0) {
      buffer = malloc(required_size);
      // printf("(%s) allocate buffer at %p, length=%d\n", __func__, buffer, required_size);
   }

   char * pattern = (uppercase) ? "%02X" : "%02x";

   int incr1 = 2 + sepsize;
   int i;
   if (len == 0)
      *buffer = '\0';
   for (i=0; i < len; i++) {
      // printf("(%s) i=%d, buffer+i*incr1=%p\n", __func__, i, buffer+i*incr1);
      sprintf(buffer+i*incr1, pattern, bytes[i]);
      if (i < (len-1) && sepstr)
         strcat(buffer, sepstr);
   }
   // printf("(%s) strlen(buffer) = %ld, required_size=%d   \n", __func__, strlen(buffer), required_size );
   // printf("(%s)  buffer=|%s|\n", __func__, buffer );
   assert(strlen(buffer) == required_size-1);

   return buffer;
}


#ifdef DEPRECATED
// TODO: replace implementation of hexstring2_t() with call to hexstring3_t()

/** Thread safe version of #hexstring2().
 *
 *  This function allocates a thread specific buffer in which the
 *  hexstring is built.  The buffer is valid until the next call
 *  of this function in the same thread.
 *
 * @param   bytes    pointer to bytes
 * @param   len      number of bytes
 * @param   sepstr   string to separate each 2 hex character pairs representing a byte,
 *                   if NULL then no separators will be inserted
 * @param   uppercase if true, use uppercase hex characters,
 *                    if false, use lower case hex characters
 *
 * @return  pointer to hex string
 *
 * Note that if the returned pointer is referenced after another call to
 * this function, the results are unpredictable.
 *
 * This function is intended to simplify formatting of diagnostic messages, since
 * the caller needn't be concerned with buffer size and allocation.
 */
char * hexstring2_t_old(
          const unsigned char * bytes,
          int                   len,
          const char *          sepstr,
          bool                  uppercase)
{
   static GPrivate  hexstring_key = G_PRIVATE_INIT(g_free);

   char * buf = g_private_get(&hexstring_key);
   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, hexstring_key=%p, buf=%p\n",
   //        __func__, this_thread, &hexstring_key, buf);

   // TODO: Keep track of buffer size, only reallocate if buffer insufficiently large.
   // But note that this function is only used for diagnostic messages, so performance
   // gain is insignificant.

   if (buf)
      g_free(buf);

   // printf("(%s) bytes=%p, len=%d, sepstr=|%s|, uppercase=%s\n", __func__,
   //       bytes, len, sepstr, bool_repr(uppercase));
   int sepsize = 0;
   if (sepstr) {
      sepsize = strlen(sepstr);
   }
   int required_size = 1;    // special case if len == 0
   if (len > 0)
      required_size =   2*len             // hex rep of bytes
                       + (len-1)*sepsize   // for separators
                       + 1;                // terminating null
   // printf("(%s) required_size=%d\n", __func__, required_size);

   buf = g_new(char, required_size);
   // printf("(%s) Calling g_private_set()\n", __func__);
   g_private_set(&hexstring_key, buf);

   // hexstring2(bytes, len, sepstr, uppercase, buf, required_size);

   char * pattern = (uppercase) ? "%02X" : "%02x";

   int incr1 = 2 + sepsize;
   if (len == 0)
      *buf = '\0';
   for (int i=0; i < len; i++) {
      // printf("(%s) i=%d, buffer+i*incr1=%p\n", __func__, i, buffer+i*incr1);
      sprintf(buf+i*incr1, pattern, bytes[i]);
      if (i < (len-1) && sepstr)
         strcat(buf, sepstr);
   }
   // printf("(%s) strlen(buffer) = %ld, required_size=%d   \n", __func__, strlen(buffer), required_size );
   // printf("(%s)  buffer=|%s|\n", __func__, buffer );
   assert(strlen(buf) == required_size-1);

   return buf;
}
#endif

#ifdef DEPRECATED
char * hexstring2_t(
          const unsigned char * bytes,
          int                   len,
          const char *          sepstr,
          bool                  uppercase)
{
   return hexstring3_t(bytes, len, sepstr, 1, uppercase);
}
#endif


/** Thread safe version of #hexstring2().
 *
 * This function allocates a thread specific buffer in which the hex string is built.
 * The buffer is valid until the next call of this function in the same thread.
 *
 * @param   bytes    pointer to bytes
 * @param   len      number of bytes
 * @param   sepstr   string to separate each segment of 2 hex character pairs representing bytes
 *                   if NULL then no separators will be inserted
 * @param   hunk_size separator string frequency
 * @param   uppercase if true, use uppercase hex characters,
 *                    if false, use lower case hex characters
 *
 * @return  pointer to hex string
 *
 * Note that if the returned pointer is referenced after another call to
 * this function, the results are unpredictable.
 *
 * This function is intended to simplify formatting of diagnostic messages, since
 * the caller needn't be concerned with buffer size and allocation.
 */
char * hexstring3_t(
          const unsigned char * bytes,      // bytes to convert
          int                   len,        // number of bytes
          const char *          sepstr,     // separator string between hex digits
          uint8_t               hunk_size,  // separator string frequency
          bool                  uppercase)  // use upper case hex characters
{
   static GPrivate  hexstring3_key = G_PRIVATE_INIT(g_free);
   static GPrivate  hexstring3_len_key = G_PRIVATE_INIT(g_free);

#ifdef OLD
   char * buf = g_private_get(&hexstring3_key);
   int  * bufsz_ptr = g_private_get(&hexstring3_len_key);
   GThread * this_thread = g_thread_self();
   printf("(%s) this_thread=%p, hexstring3_key=%p, buf=%p, hexstring3_len_key=%p, bufsz_ptr=%p\n",
          __func__, this_thread, &hexstring3_key, buf, &hexstring3_len_key, bufsz_ptr);
   if (bufsz_ptr)
      printf("(%s) *bufsz_ptr = %d\n", __func__, *bufsz_ptr);

   // TODO: Keep track of buffer size, only reallocate if buffer insufficiently large.
   // But note that this function is only used for diagnostic messages, so performance
   // gain is insignificant.

   // unnecessary if use g_private_replace() instead of g_private_set()
   // if (buf)
   //    g_free(buf);
#endif
   // printf("(%s) bytes=%p, len=%d, sepstr=|%s|, uppercase=%s\n", __func__,
   //       bytes, len, sepstr, bool_repr(uppercase));
   if (hunk_size == 0)
      sepstr = NULL;
   else if (sepstr == NULL)
      hunk_size = 0;

   int sepsize = 0;
   if (sepstr) {
      sepsize = strlen(sepstr);
   }
   int required_size = 1;    // special case if len == 0
   // excessive if hunk_size > 1, but not worth the effort to be accurate
   if (len > 0)
      required_size =   2*len             // hex rep of bytes
                       + (len-1)*sepsize   // for separators
                       + 1;                // terminating null
   // printf("(%s) sepstr=|%s|, hunk_size=%d, required_size=%d\n", __func__, sepstr, hunk_size, required_size);

#ifdef OLD
   if ( !bufsz_ptr || *bufsz_ptr < required_size) {
      buf = g_new(char, required_size);
      // printf("(%s) Calling g_private_set()\n", __func__);
      g_private_replace(&hexstring3_key, buf);


      if (!bufsz_ptr) {
         bufsz_ptr = g_new(int, 1);
         g_private_set(&hexstring3_len_key, bufsz_ptr);
      }
      *bufsz_ptr = required_size;
   }
#endif
   char * buf = get_thread_dynamic_buffer(&hexstring3_key, &hexstring3_len_key, required_size);
   // char * buf = get_thread_private_buffer(&hexstring3_key, NULL, required_size);

   char * pattern = (uppercase) ? "%02X" : "%02x";

   // int incr1 = 2 + sepsize;
   *buf = '\0';
   for (int i=0; i < len; i++) {
      // printf("(%s) i=%d, strlen(buf)=%ld\n", __func__, i, strlen(buf));
      sprintf(buf+strlen(buf), pattern, bytes[i]);
      bool insert_sepstr = (hunk_size == 0)
                               ? (i < (len-1) && sepstr)
                               : (i < (len-1) && sepstr && (i+1)%hunk_size == 0);
      if (insert_sepstr)
         strcat(buf, sepstr);
   }
   // printf("(%s) strlen(buffer) = %ld, required_size=%d   \n", __func__, strlen(buffer), required_size );
   // printf("(%s)  buffer=|%s|\n", __func__, buffer );
   assert(strlen(buf) <= required_size-1);

   // printf("(%s) Returning: %p\n", __func__, buf);
   return buf;
}


char * hexstring_t(
          const unsigned char * bytes,
          int                   len)
{
      return hexstring3_t(bytes, len, " ", 1, false);
}


/** Dump a region of memory as hex characters and their ASCII values.
 *  The output is indented by the specified number of spaces.
 *
 *  @param fh       where to write output, if NULL, write nothing
 *  @param data     start of region to show
 *  @param size     length of region
 *  @param indents  number of spaces to indent the output
 */
void fhex_dump_indented(FILE * fh, const Byte* data, int size, int indents)
{
   if (fh) {
      int i; // index in data...
      int j; // index in line...
      char temp[8];
      char buffer[128];
      char *ascii;
      char indentation[100];
      snprintf(indentation, 100, "%*s", indents, "");

      memset(buffer, 0, 128);

      // printf("\n");
      // Printing the ruler...
      fprintf(fh,
              "%s        +0          +4          +8          +c            0   4   8   c   \n",
              indentation);
      ascii = buffer + 58;
      memset(buffer, ' ', 58 + 16);
      buffer[58 + 16] = '\n';
      buffer[58 + 17] = '\0';
      buffer[0] = '+';
      buffer[1] = '0';
      buffer[2] = '0';
      buffer[3] = '0';
      buffer[4] = '0';
      for (i = 0, j = 0; i < size; i++, j++) {
         if (j == 16) {
            fprintf(fh, "%s%s", indentation, buffer);
            memset(buffer, ' ', 58 + 16);
            sprintf(temp, "+%04x", i);
            memcpy(buffer, temp, 5);
            j = 0;
         }

         sprintf(temp, "%02x", 0xff & data[i]);
         memcpy(buffer + 8 + (j * 3), temp, 2);
         if ((data[i] > 31) && (data[i] < 127))
            ascii[j] = data[i];
         else
            ascii[j] = '.';
      }

      if (j != 0)
         fprintf(fh, "%s%s", indentation, buffer);
   }
}


/** Dump a region of memory as hex characters and their ASCII values.
 *  Output is written to the location specified by parameter fh.
 *
 * @param   fh       where to write output
 * @param   data     start of region to show
 * @param   size     length of region
 */
void fhex_dump(FILE * fh, const Byte* data, int size)
{
   fhex_dump_indented(fh, data, size, 0);
}


/** Dump a region of memory as hex characters and their ASCII values.
 *  Output is written to stdout.
 *
 * @param   data     start of region to show
 * @param   size     length of region
 */
void hex_dump(const Byte* data, int size) {
   fhex_dump(stdout, data, size);
}


/** Extension of fputc() that allows a NULL stream argument,
 * in which case no output is written.
 *
 *  @param  c          character to write
 *  @param  stream     if null do nothing
 *
 *  @return result of underlying fputs(), or 0 if stream is NULL
 */
int f0putc(int c, FILE * stream) {
   int rc = 0;
   if (stream)
      rc = fputc(c, stream);
   return rc;
}


/** Extension of fputs() that allows a NULL stream argument,
 * in which case no output is written.
 *
 * @param   msg        text to write
 * @param   stream     if null do nothing
 *
 * @return   result of underlying fputs(), or 0 if stream is NULL
 */
int f0puts(const char * msg, FILE * stream) {
   int rc = 0;
   if (stream)
      rc = fputs(msg, stream);
   return rc;
}


/** Extension of fprintf() that allows a NULL stream argument,
 * in which case no output is written.
 *
 * @param stream     if null do nothing
 * @param  format     format string
 *
 * @return  result of underlying vfprintf(), or 0 if stream is NULL
 */
int f0printf(FILE * stream, const char * format, ...) {
   int rc = 0;
   // printf("(%s) stream=%p\n", __func__, stream);
   if (stream) {
      va_list(args);
      va_start(args, format);
      rc = vfprintf(stream, format, args);
      va_end(args);
   }
   return rc;
}


/** Extension of vfprintf() that allows a NULL stream argument,
 * in which case no output is written.
 *
 * @param stream     if null do nothing
 * @param format     format string
 * @param ap         pointer to variable argument list
 *
 * @return result of underlying vfprintf(), or 0 if stream is NULL
 */
int vf0printf(FILE * stream, const char * format, va_list ap) {
   int rc = 0;
   if (stream)
      rc = vfprintf(stream, format, ap);
   return rc;
}


//
// Miscellaneous
//

/** Tests if a range of bytes is entirely 0
 *
 * @param bytes pointer to first byte
 * @param bytect number of bytes
 * @return **true** if all bytes are zero, **false** if not
 */
bool all_bytes_zero(Byte * bytes, int bytect) {
   Byte sum = 0;
   for (int ndx=0; ndx < bytect; ndx++) {
      sum |= bytes[ndx];
   }
   return !sum;
}
