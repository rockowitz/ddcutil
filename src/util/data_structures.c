/** @file data_structures.c
 *  General purpose data structures
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>     // for MIN, MAX
/** \endcond */

// #include "debug_util.h"
#include "string_util.h"

#include "data_structures.h"


// bva - Byte Value Array
//
// An opaque structure containing an array of bytes that
// can grow dynamically.  Note that the same byte can
// appear multiple times.

/** Creates a new **Byte_Value_Array** instance.
 * @return newly allocated **Byte_Value_Array**.
 */
Byte_Value_Array bva_create() {
   GByteArray * ga = g_byte_array_new();
   return (Byte_Value_Array) ga;
}


/** Creates a new **Byte_Value_Array** instance,
 *  containing the values from an existing instance
 *  that satisfy the filter function.
 *
 *  \param  bva **Byte_Value_Array** instance
 *  \param  filter_func  function that takes a byte value as an argument,
 *                       returning true if the value should be included
 *                       in the output **Byte_Value_Array**
 *  \return new **Byte_Value_Array**
 */
Byte_Value_Array bva_filter(Byte_Value_Array  bva, IFilter filter_func) {
   GByteArray * src = (GByteArray*) bva;
   GByteArray * result = g_byte_array_new();
   for (int ndx=0; ndx < src->len;  ndx++) {
      guint8 v = src->data[ndx];
      // Byte v1 = v;
      if (filter_func(v))
         bva_append(result,v);
   }
   return (Byte_Value_Array) result;
}


/** Returns the number of entries in a **Byte_Value_Array**.
 *
 * @param bva **Byte_Value_Array** instance
 * @return number of entries
 */
int bva_length(Byte_Value_Array bva) {
   GByteArray* ga = (GByteArray*) bva;
   return ga->len;
}


/** Adds a value to a **Byte_Value_Array**.
 *
 *  @param  bva   **Byte_Value_Array** instance
 *  @param  item  value to add
 */
void bva_append(Byte_Value_Array bva, Byte item) {
   GByteArray* ga = (GByteArray*) bva;
   GByteArray * ga2 = g_byte_array_append(ga, &item, 1);
   assert(ga2 == ga);
}


/** Gets a value by its index from a **Byte_Value_Array**.
 *
 *  @param  bva **Byte_Value_Array** instance
 *  @param  ndx  index of entry
 *  @return value
 *
 *  **ndx** must be a valid value.
 *  The only check is by assert().
 */
Byte bva_get(Byte_Value_Array bva, int ndx) {
   GByteArray* ga = (GByteArray*) bva;
   assert(0 <= ndx && ndx < ga->len);

   guint8 v = ga->data[ndx];
   Byte v1 = v;
   return v1;
}


/** Checks if a **Byte_Value_Array** contains a value.
 *
 *  @param  bva **Byte_Value_Array** instance
 *  @param  item value to check for
 *  @return true/false
 */
bool bva_contains(Byte_Value_Array bva, Byte item) {
   GByteArray* ga = (GByteArray*) bva;
   int ndx;
   bool result = false;
   // printf("(%s) item=0x%02x, ga->len=%d\n", __func__, item, ga->len);
   for (ndx=0; ndx < ga->len; ndx++) {
      guint8 v = ga->data[ndx];
      // Byte v1 = v;
      if (v == item) {
         result = true;
         break;
      }
   }
   // printf("(%s) returning %d\n", __func__, result);
   return result;
}


// Comparison function used by gba_sort()
static int bva_comp_func(const void * val1, const void * val2) {
   const guint8 * v1 = val1;
   const guint8 * v2 = val2;
   int result = 0;
   if (*v1 < *v2)
      result = -1;
   else if (*v1 > *v2)
      result = 1;
   // printf("(%s) *v1=%u, *v2=%u, returning: %d\n", __func__, *v1, *v2, result);
   return result;
}


/** Sorts a **Byte_Value_Array** in ascending order
 *
 *  @param  bva **Byte_Value_Array** instance
 */
void  bva_sort(Byte_Value_Array bva) {
   // printf("(%s) Starting", __func__);
   GByteArray* ga = (GByteArray*) bva;
   qsort(ga->data, ga->len, 1, bva_comp_func);
   // printf("(%s) Done", __func__);
}


/** Compare 2 sorted #Byte_Value_Array instances for equality.
 *  If the same value occurs multiple times in one array, it
 *  must occur the same number of times in the other.
 *
 *  \param  bva1  pointer to first instance
 *  \param  bva2  pointer to second instance
 *  \retval true  arrays are identical
 *  \retval false arrays not identical
 *
 *  \remark
 *  If bva1 or bva2 is null, it is considered to contain 0 values.
 */
bool bva_sorted_eq(Byte_Value_Array bva1, Byte_Value_Array bva2) {
   int len1 = (bva1) ? bva_length(bva1) : 0;
   int len2 = (bva2) ? bva_length(bva2) : 0;

   bool result = true;
   if (len1  != len2) {
      result = false;
   }
   else if ( (len1+len2) > 0 ) {
      for (int ndx = 0; ndx < bva_length(bva1); ndx++) {
         if (bva_get(bva1,ndx) != bva_get(bva2,ndx))
            result = false;
      }
   }
   return result;
}


/** Returns the bytes from a **Byte_Value_Array**.
 *
 * @param bva **Byte_Value_Array** instance
 * @return pointer to bytes within
 *
 * @remark
 * The length of the bytes returned must be obtained from **bva_length()**.
 * Alternatively, consider returning a **Buffer**.
 * @remark
 * Caller should not free the returned pointer.
 */
Byte * bva_bytes(Byte_Value_Array bva) {
   GByteArray* ga = (GByteArray*) bva;
   // Byte * result = calloc(ga->len, sizeof(guint8));
   // memcpy(result, ga->data, ga->len);
   Byte * result = ga->data;
   return result;
}

/** Returns a string representation of the data in a **Byte_Value_Array.
 *
 *  @param bva  **Byte_Value_Array** instance
 *  @param as_hex if true, use 2 character hex representation,
 *                if false, use 1-3 character integer representation
 *  @param sep  separator string between values, if NULL then none
 *  @return string representation of data, caller must free
 */
char * bva_as_string(Byte_Value_Array bva, bool as_hex, char * sep) {
   assert(bva);
   GByteArray* ga = (GByteArray*) bva;
   if (!sep)
      sep = "";
   int len = ga->len;
   Byte * bytes = ga->data;
   int sepsz = strlen(sep);
   int alloc_sz = len * (3+sepsz) + 1;  // slightly large, but simpler to compute
   char * buf = calloc(1, alloc_sz);
   for (int ndx = 0; ndx < len; ndx++) {
      char * cursep = (ndx > 0) ? sep : "";
      if (as_hex)
         snprintf(buf + strlen(buf), alloc_sz-strlen(buf), "%s%02x", cursep, bytes[ndx]);
      else
         snprintf(buf + strlen(buf), alloc_sz-strlen(buf), "%s%d", cursep, bytes[ndx]);
   }
   return buf;
}


/** Destroy a **Byte_Value_Array**.
 *
 * @param bva **Byte_Value_Array** instance
 */
void bva_free(Byte_Value_Array bva) {
   GByteArray* ga = (GByteArray*) bva;
   g_byte_array_free(ga,TRUE);
}


/** Debugging function to report the contents of a **Byte_Value_Array**.
 *
 * @param bva **Byte_Value_Array** instance
 * @param title if non-null, line to print at start of report
 */
void bva_report(Byte_Value_Array bva, char * title) {
   if (title)
      printf("%s\n", title);
   int ct = bva_length(bva);
   int ndx = 0;
   for (; ndx < ct; ndx++) {
      Byte hval = bva_get(bva, ndx);
      printf("  %02X\n", hval);
   }
}


// Tests and sample code

int egmain(int argc, char** argv) {
   GList* list1 = NULL;
   list1 = g_list_append(list1, "Hello world!");
   // generates warning:
   // printf("The first item is '%s'\n", g_list_first(list1)->data);
   g_list_free(list1);

   GSList* list = NULL;
   printf("The list is now %d items long\n", g_slist_length(list));
   list = g_slist_append(list, "first");
   list = g_slist_append(list, "second");
   printf("The list is now %d items long\n", g_slist_length(list));
   g_slist_free(list);

   return 0;
}


void test_value_array() {
   Byte_Value_Array bva = bva_create();

   bva_append(bva, 0x01);
   bva_append(bva, 0x02);
   int ndx = 0;
   int ct = bva_length(bva);
   for (;ndx < ct; ndx++) {
      Byte val = bva_get(bva, ndx);
      printf("Value[%d] = 0x%02x\n", ndx, val);
   }
   bva_free(bva);
}


//
// bbf - ByteBitFlags -
//
// An opaque data structure containing 256 flags
//

#define BYTE_BIT_MARKER  "BBFG"

#define BYTE_BIT_BYTE_CT 32    // number of bytes in data structure: 256/8
#define BYTE_BIT_UNOPAQUE(unopaque_var, opaque_var) _ByteBitFlags* unopaque_var = (_ByteBitFlags*) opaque_var
#define BYTE_BIT_VALIDATE(flags)    assert(flags && ( memcmp(flags->marker, BYTE_BIT_MARKER, 4) == 0))

typedef struct {
   char marker[4];    // always BBFG
   char byte[BYTE_BIT_BYTE_CT];
} _ByteBitFlags;
// typedef _ByteBitFlags* PByteBitFlags;


static _ByteBitFlags * bbf_create_internal() {
   _ByteBitFlags* flags = calloc(1, sizeof(_ByteBitFlags));
   memcpy(flags->marker, BYTE_BIT_MARKER, 4);
   return flags;
}


/** Creates a new **Byte_Bit_Flags** instance.
 *
 * @return opaque handle to new instance
 */
Byte_Bit_Flags bbf_create() {
   return bbf_create_internal();
}


/** Destroys a **Byte_Bit_Flags** instance.
 *
 * @param bbflags instance handle
 */
void bbf_free(Byte_Bit_Flags bbflags) {
   // _ByteBitFlags* flags = (_ByteBitFlags*) bbflags;
   BYTE_BIT_UNOPAQUE(flags, bbflags);
   if (flags) {
      assert( memcmp(flags->marker, "BBFG",4) == 0);
      free(flags);
   }
}


/** Sets a flag in a **Byte_Bit_Flags** instance.
 *
 * @param bbflags instance handle
 * @param val     number of bit to set
 */
void bbf_set(Byte_Bit_Flags bbflags, Byte val) {
   BYTE_BIT_UNOPAQUE(flags, bbflags);
   BYTE_BIT_VALIDATE(flags);
   int flagndx   = val >> 3;
   int shiftct   = val & 0x07;
   Byte flagbit  = 0x01 << shiftct;
   // printf("(%s) val=0x%02x, flagndx=%d, shiftct=%d, flagbit=0x%02x\n",
   //        __func__, val, flagndx, shiftct, flagbit);
   flags->byte[flagndx] |= flagbit;
}


/** Tests if a flag is set in a **Byte_Bit_Flags** instance.
 *
 * @param bbflags instance handle
 * @param val     number of bit to test
 * @return        true/false
 */
bool bbf_is_set(Byte_Bit_Flags bbflags, Byte val) {
   BYTE_BIT_UNOPAQUE(flags, bbflags);
   BYTE_BIT_VALIDATE(flags);
   int flagndx   = val >> 3;
   int shiftct   = val & 0x07;
   Byte flagbit  = 0x01 << shiftct;
   // printf("(%s) val=0x%02x, flagndx=%d, shiftct=%d, flagbit=0x%02x\n",
   //        __func__, val, flagndx, shiftct, flagbit);
   bool result = flags->byte[flagndx] & flagbit;
   // printf("(%s) bbflags=0x%s, val=0x%02x, returning: %d\n",
   //        __func__, hexstring( (unsigned char *)flags->byte,32), val, result);
   // printf("(%s) val = 0x%02x, returning %s\n",  __func__, val, sbool(result));
   return result;
}


/** Subtracts one **Byte_Bit_Flags** instance from another.
 *  A flag is set in the result if it is set in the first instance
 *  but not in the second instance.
 *
 * @param bbflags1 handle to first instance
 * @param bbflags2 handle to second instance
 * @return newly created instance with the result
 */
Byte_Bit_Flags bbf_subtract(Byte_Bit_Flags bbflags1, Byte_Bit_Flags bbflags2) {
   BYTE_BIT_UNOPAQUE(flags1, bbflags1);
   BYTE_BIT_VALIDATE(flags1);
   BYTE_BIT_UNOPAQUE(flags2, bbflags2);
   BYTE_BIT_VALIDATE(flags2);
   _ByteBitFlags *  result = bbf_create();
   for (int ndx = 0; ndx < BYTE_BIT_BYTE_CT; ndx++) {
      result->byte[ndx] = flags1->byte[ndx] & ~flags2->byte[ndx];
   }
   return result;
}



/** Returns a 64 character long hex string representing the data structure.
 *
 *  @param   bbflags instance handle
 *  @param   buffer  buffer in which to return string
 *  @param   buflen  buffer length
 *
 * @return  character string representation of flags that are set
 *
 * If buffer is NULL then memory is malloc'd.  It is the responsibility
 * of the caller to free the returned string.
 *
 * If buflen is insufficiently large an assertion fails.
 *
 * @remark
 * Future enhancement:  Insert a separator character every n characters?
 */
char * bbf_repr(Byte_Bit_Flags bbflags, char * buffer, int buflen) {
   BYTE_BIT_UNOPAQUE(flags, bbflags);
   BYTE_BIT_VALIDATE(flags);
   int reqd_size = (2*BYTE_BIT_BYTE_CT) /* 2 hex chars for each byte*/ + 1 /* trailing null*/ ;
   if (buffer)
      assert(buflen >= reqd_size);
   else
      buffer = malloc(reqd_size);
   *buffer = '\0';
   int flagndx = 0;
   for (; flagndx < 8; flagndx++)
     sprintf(buffer + strlen(buffer), "%02x", flags->byte[flagndx]);
   return buffer;
}


/** Returns the number of bits set in a **Byte_Bit_Flags**
 *
 * @param bbflags  instance handle
 * @return number of bits set (0..256)
 */
int bbf_count_set(Byte_Bit_Flags bbflags) {
   BYTE_BIT_UNOPAQUE(flags, bbflags);
   BYTE_BIT_VALIDATE(flags);
   int result = 0;
   int flagndx;
   int bitndx;
   for (flagndx=0; flagndx < BYTE_BIT_BYTE_CT; flagndx++) {
      for (bitndx = 0; bitndx < 8; bitndx++) {
         unsigned char flagbit = (0x80 >> bitndx);
         if (flags->byte[flagndx] & flagbit)
            result += 1;
      }
   }
   // printf("(%s) returning: %d\n", __func__, result);
   return result;
}


/** Returns a string of space separated 2 character hex values
 * representing the bits set in the Byte_Bit_Flag,
 * e.g. "03 7F" if bits 0x03 and 0x7F are set
 *
 * @param  bbflags  instance handle
 * @param  buffer   pointer to buffer in which to return character string,
 *                  if NULL malloc a new buffer
 * @param  buflen   buffer length
 *
 * @return pointer to character string
 *
 * If a new buffer is allocated, it is the responsibility of the caller to
 * free the string returned.
 *
 * For complete safety in case every bit is set, buflen should be >= 768.
 * (2 chars for every bit (512), 255 separator characters, 1 terminating null)
 * If buflen in insufficiently large to contain the result, an assertion fails.
 */
char * bbf_to_string(Byte_Bit_Flags bbflags, char * buffer, int buflen) {
   // printf("(%s) Starting\n", __func__);
   BYTE_BIT_UNOPAQUE(flags, bbflags);
   BYTE_BIT_VALIDATE(flags);
   int bit_set_ct = bbf_count_set(flags);
   int reqd_size = bit_set_ct * 2     +     // char rep of bytes
                   (bit_set_ct-1) * 1 +     // separating spaces
                   1;                       // trailing null
   if (buffer)
      assert(buflen >= reqd_size);
   else
      buffer = malloc(reqd_size);
   char * pos = buffer;
   unsigned int flagno = 0;
   // printf("(%s) bbflags->byte=0x%s\n", __func__, hexstring(flags->byte,32));
   for (flagno = 0; flagno < 256; flagno++) {
      Byte flg = (Byte) flagno;
      // printf("(%s) flagno=%d, flg=0x%02x\n", __func__, flagno, flg);
      if (bbf_is_set(flags, flg)) {
         // printf("(%s) Flag is set: %d, 0x%02x\n", __func__, flagno, flg);
         if (pos > buffer) {
            *pos  = ' ';
            pos++;
         }
         // printf("(%s) flg=%02x\n", __func__, flg);
         sprintf(pos, "%02x", flg);
         pos += 2;
         // printf("(%s) pos=%p\n", __func__, pos);
      }
   }
   // printf("(%s) Done.  Returning: %s\n", __func__, buffer);
   return buffer;
}


int bbf_to_bytes(Byte_Bit_Flags bbflags, Byte * buffer, int buflen) {
   // printf("(%s) Starting\n", __func__);
   BYTE_BIT_UNOPAQUE(flags, bbflags);
   BYTE_BIT_VALIDATE(flags);

   int bit_set_ct = bbf_count_set(flags);
   assert(buflen >= bit_set_ct);

   unsigned int bufpos = 0;
   unsigned int flagno = 0;
   // printf("(%s) bbflags->byte=0x%s\n", __func__, hexstring(flags->byte,32));
   for (flagno = 0; flagno < 256; flagno++) {
      Byte flg = (Byte) flagno;
      // printf("(%s) flagno=%d, flg=0x%02x\n", __func__, flagno, flg);
      if (bbf_is_set(flags, flg)) {
         // printf("(%s) Flag is set: %d, 0x%02x\n", __func__, flagno, flg);
         buffer[bufpos++] = flg;
      }
   }
   // printf("(%s) Done.  Returning: %d\n", __func__, bupos);
   return bufpos;
}



/** Converts a **Byte_Bit_Flags** instance to a sequence of bytes whose values
 *  correspond to the bits that are set.
 *  The byte sequence is returned in a newly allocated **Buffer**.
 *
 * @param  bbflags  instance handle
 * @return pointer to newly allocated **Buffer**
 */
Buffer * bbf_to_buffer(Byte_Bit_Flags bbflags) {
   BYTE_BIT_UNOPAQUE(flags, bbflags);
   BYTE_BIT_VALIDATE(flags);
   int bit_set_ct = bbf_count_set(flags);
   Buffer * buf = buffer_new(bit_set_ct, __func__);
   for (unsigned int flagno = 0; flagno < 256; flagno++) {
      Byte flg = (Byte) flagno;
      // printf("(%s) flagno=%d, flg=0x%02x\n", __func__, flagno, flg);
      if (bbf_is_set(flags, flg)) {
         buffer_add(buf, flg);
      }
   }
   // printf("(%s) Done.  Returning: %s\n", __func__, buffer);
   return buf;
}


#define BBF_ITER_MARKER "BBFI"
typedef struct {
   char  marker[4];
   Byte_Bit_Flags bbflags;
   int   lastpos;
} _Byte_Bit_Flags_Iterator;


/** Creates an iterator for a #Byte_Bit_Flags instance.
 *  The iterator is an opaque object.
 *
 * \param bbflags handle to #Byte_Bit_Flags instance
 * \return iterator
 */
Byte_Bit_Flags_Iterator bbf_iter_new(Byte_Bit_Flags bbflags) {
   _Byte_Bit_Flags_Iterator * result = malloc(sizeof(_Byte_Bit_Flags_Iterator));
   memcpy(result->marker, BBF_ITER_MARKER, 4);
   result->bbflags = bbflags;   // TODO: save pointer to unopaque _BitByteFlags
   result->lastpos = -1;
   return result;
}


/** Free a #Byte_Bit_Flags_Iterator.
 *
 * \param bbf_iter handle to iterator (may be NULL)
 */
void bbf_iter_free(Byte_Bit_Flags_Iterator bbf_iter) {
   _Byte_Bit_Flags_Iterator * iter = (_Byte_Bit_Flags_Iterator *) bbf_iter;

   if (bbf_iter) {
      assert(memcmp(iter->marker, BBF_ITER_MARKER, 4) == 0);
      iter->marker[3] = 'x';
      free(iter);
   }
}

/** Reinitializes an iterator.  Sets the current position before the first
 *  value.
 *
 * \param bbf_iter handle to iterator
 */
void bbf_iter_reset(Byte_Bit_Flags_Iterator bbf_iter) {
   _Byte_Bit_Flags_Iterator * iter = (_Byte_Bit_Flags_Iterator *) bbf_iter;
   assert(iter && memcmp(iter->marker, BBF_ITER_MARKER, 4) == 0);

   iter->lastpos = -1;
}


/** Returns the number of the next bit that is set.
 *
 * \param bbf_iter handle to iterator
 * \return number of next bit that is set
 */
int bbf_iter_next(Byte_Bit_Flags_Iterator bbf_iter) {
   _Byte_Bit_Flags_Iterator * iter = (_Byte_Bit_Flags_Iterator *) bbf_iter;
   assert( iter && memcmp(iter->marker, BBF_ITER_MARKER, 4) == 0);
   // printf("(%s) Starting. lastpos = %d\n", __func__, iter->lastpos);

   int result = -1;
   for (int ndx = iter->lastpos + 1; ndx < 256; ndx++) {
      if (bbf_is_set(iter->bbflags, ndx)) {
         result = ndx;
         iter->lastpos = ndx;
         break;
      }
   }
   // printf("(%s) Returning: %d\n", __func__, result);
   return result;
}

//
// Cross functions bba <-> bbf
//


/** Tests if the bit number of every byte in a #Byte_Value_Array is set
 *  in a #Byte_Bit_Flags, and conversely that for every bit set in the
 *  #Byte_Bit_Flags there is a corresponding byte in the #Byte_Value_Array.
 *
 *  Note it is possible that the same byte appears more than once in the
 *  #Byte_Value_Array.
 *
 *  \param bva     #Byte_Value_Array to test
 *  \param bbflags #Byte_Bit_Flags to test
 *  \return        true/false
 */
bool bva_bbf_same_values( Byte_Value_Array bva , Byte_Bit_Flags bbflags) {
   bool result = true;
   int item;
   for (item = 0; item < 256; item++) {
      // printf("item=%d\n", item);
      bool r1 = bva_contains(bva, item);
      bool r2 = bbf_is_set(bbflags, item);
      if (r1 != r2)
         result = false;
   }
   return result;
}

/** Function matching signature #Byte_Appender that adds a byte
 * to a #Byte_Value_Array.
 *
 * \param data_struct pointer to #Byte_Value_Array
 * \param val  byte to append
 */
void bva_appender(void * data_struct, Byte val) {
   Byte_Value_Array bva = (Byte_Value_Array) data_struct;
   bva_append(bva, val);
}


/** Function matching signature #Byte_Appender that sets a bit in
 *  a #Byte_Bit_Flags
 *
 * \param data_struct pointer to #Byte_Bit_Flags
 * \param val  bit number to set
 */
void bbf_appender(void * data_struct, Byte val) {
   Byte_Bit_Flags bbf = (Byte_Bit_Flags) data_struct;
   assert(bbf);
   bbf_set(bbf, val);
}


/** Stores a list of bytehex values in either a **Byte_Value_Array** or a **Byte_Bit_Flags**.
 *
 * @param start starting address of hex values
 * @param len   length of hex values
 * @param data_struct opague handle to either a **Byte_Value_Array** or a **Byte_Bit_Flags**
 * @param appender function to add a value to **data_struct**
 *
 * @return false if any input data cannot be parsed, true otherwise
 */
bool store_bytehex_list(char * start, int len, void * data_struct, Byte_Appender appender){
   bool ok = true;

   char * buf = malloc(len+1);
   memcpy(buf, start, len);
   buf[len] = '\0';

   char * curpos = buf;
   char * nexttok;
   Byte   byteVal = 0x00;    // initialization logically unnecessary, but makes compiler happy
   while ( (nexttok = strtok(curpos, " ")) != NULL) {
      if (curpos)
         curpos = NULL;     // for all calls after first
      int ln = strlen(nexttok);
      bool hexok = false;
      if (ln == 2) {                // normal case
         // byteVal = hhc_to_byte(nexttok);
         hexok = hhc_to_byte_in_buf(nexttok, &byteVal);
      }
      else if (ln == 1) {
         // on old ultrasharp connected to blackrock (pre v2), values in capabilities
         // string are single digits.  Not clear whether to regard them as decimal or hex,
         // since all values are < 9.  But in that case decimal and single digit hex
         // give the same value.
         char buf[2];
         buf[0] = '0';
         buf[1] = *nexttok;
         // byteVal = hhc_to_byte(buf);
         hexok = hhc_to_byte_in_buf(buf, &byteVal);
      }
      if (!hexok) {
         // printf("(%s) Invalid hex value in list: %s\n", __func__, nexttok);
         ok = false;
      }
      // printf("(%s) byteVal=0x%02x  \n", __func__, byteVal );
      if (hexok)
         appender(data_struct, byteVal);
   }

   free(buf);
   // printf("(%s) Returning %s\n", __func__, sbool(ok));
   return ok;
}


/** Parses a list of bytehex values and stores the result in a **Byte_Value_Array**.
 *
 * @param bva   handle of **Byte_Value_Array** instance
 * @param start starting address of hex values
 * @param len   length of hex values
 *
 * @return false if any input data cannot be parsed, true otherwise
 */
bool bva_store_bytehex_list(Byte_Value_Array bva, char * start, int len) {
   return store_bytehex_list(start, len, bva, bva_appender);
}


/** Parses a list of bytehex values and stores the result in a **Byte_Bit_Flags**.
 *
 * @param bbf   handle of **Byte_Bit_Flags** instance
 * @param start starting address of hex values
 * @param len   length of hex values
 *
 * @return false if any input data cannot be parsed, true otherwise
 */
bool bbf_store_bytehex_list(Byte_Bit_Flags bbf, char * start, int len) {
   return store_bytehex_list(start, len, bbf, bbf_appender);
}


//
// Buffer with length management
//

bool trace_buffer_malloc_free = false;
bool trace_buffer = false;    // controls buffer tracing
bool trace_buffer_resize = false;


/** Allocates a **Buffer** instance
 *
 *  @param  size       maximum number of bytes that buffer can hold
 *  @param  trace_msg  optional trace message
 *
 *  @return pointer to newly allocated instance
 */
Buffer * buffer_new(int size, const char * trace_msg) {
   int hacked_size = size+16;     // try allocating extra space see if free failures go away - overruns?
   // printf("(%s) sizeof(Buffer)=%ld, size=%d\n", __func__, sizeof(Buffer), size);    // sizeof(Buffer) == 16
   Buffer * buffer = (Buffer *) malloc(sizeof(Buffer));
   memcpy(buffer->marker, BUFFER_MARKER, 4);
   buffer->bytes = (Byte *) calloc(1, hacked_size);    // hack
   buffer->buffer_size = size;
   buffer->len = 0;
   buffer->size_increment = 0;
   if (trace_buffer_malloc_free)
      printf("(%s) Allocated buffer.  buffer=%p, buffer->bytes=%p, &buffer->bytes=%p, %s\n",
             __func__, (void*)buffer, buffer->bytes, (void*)&(buffer->bytes), trace_msg);
   return buffer;
}


/** Sets a size increment for the buffer, allowing it to be dynamically
 *  resized if necessary.
 *
 *  @param buf   pointer to Buffer instance
 *  @param size_increment if resizing is necessary, the buffer size will be
 *                         increased by this amount
 */
void buffer_set_size_increment(Buffer * buf, uint16_t size_increment) {
   buf->size_increment = size_increment;
}


/** Allocates a **Buffer** instance and sets an initial value
 *
 *  @param  bytes      pointer to initial value
 *  @param  bytect     length of initial value, the buffer size is
 *                     also set to this value
 *  @param  trace_msg  optional trace message
 *
 *  @return pointer to newly allocated instance
 *
 *  @remark
 *  Setting the buffer size to the initial value does not allow for
 *  expansion, unless buffer_set_size_increment() is called
 */
Buffer * buffer_new_with_value(Byte * bytes, int bytect, const char * trace_msg) {
   Buffer* buf = buffer_new(bytect, trace_msg);
   buffer_put(buf, bytes, bytect);
   return buf;
}


/** Copies a Buffer.
 *
 *  @param srcbuf instance to copy
 *  @param trace_msg optional trace message
 *
 *  @return newly allocated copy
 *
 *  @remark
 *  - The contents of the newly allocated Buffer is identical to
 *    the original, but the maximum length and size increment are
 *    not.   Is this an issue?
 *  - Not currently used. (3/2017)
 */
Buffer * buffer_dup(Buffer * srcbuf, const char * trace_msg) {
   return buffer_new_with_value(srcbuf->bytes, srcbuf->len, trace_msg);
}


/** Frees a Buffer instance.  All memory associated with the Buffer is released.
 *
 *  @param buffer    pointer to Buffer instance, must be valid
 *  @param trace_msg optional trace message
 */
void buffer_free(Buffer * buffer, const char * trace_msg) {
   if (trace_buffer_malloc_free)
      printf("(%s) Starting. buffer = %p\n", __func__, (void*) buffer);

   // ASSERT_WITH_BACKTRACE(buffer);
   // ASSERT_WITH_BACKTRACE(memcmp(buffer->marker, BUFFER_MARKER, 4) == 0);

      if (buffer->bytes) {
        if (trace_buffer_malloc_free)
            printf("(%s) Freeing buffer->bytes = %p, &buffer->bytes=%p\n",
                   __func__, buffer->bytes, (void*)&(buffer->bytes));
        free(buffer->bytes);
      }
      if (trace_buffer_malloc_free)
         printf("(%s) Freeing buffer = %p, %s\n", __func__, (void*)buffer, trace_msg);
      buffer->marker[3] = 'x';
      free(buffer);
      if (trace_buffer_malloc_free)
         printf("(%s) Done\n", __func__);
}


/** Returns the length of the data in the Buffer.
 *
 *  @param  buffer  pointer to Buffer instance
 *  @return number of bytes in Buffer
 */
int buffer_length(Buffer * buffer) {
   return buffer->len;
}


/** Adjusts the number of bytes in a Buffer.
 *
 *  @param  buffer  pointer to buffer instance
 *  @param  bytect  new length of buffer contents, must be less that
 *                  the maximum size of the buffer
 */
void buffer_set_length(Buffer * buffer, int bytect) {
   if (trace_buffer)
      printf("(%s) bytect=%d, buffer_size=%d\n", __func__, bytect, buffer->buffer_size);
   assert (bytect <= buffer->buffer_size);
   buffer->len = bytect;
}


/** Sets the value stored in a Buffer to a range of bytes.
 *  The buffer length is updated.
 *
 *  @param  buffer     pointer to Buffer instance
 *  @param  bytes      pointer to bytes to store in buffer
 *  @param  bytect     number of bytes to store
 */
void buffer_put(Buffer * buffer, Byte * bytes, int bytect) {
   if (trace_buffer) {
      printf("(%s) buffer->bytes = %p, bytes=%p, bytect=%d\n",
             __func__, buffer->bytes, bytes, bytect);
      printf("(%s) cur len = %d, storing |%.*s|, bytect=%d\n",
             __func__, buffer->len, bytect, bytes, bytect);
   }
   assert (bytect <= buffer->buffer_size);
   memcpy(buffer->bytes, bytes, bytect);
   buffer->len = buffer->len + bytect;
   // printf("(%s) Returning.  cur len = %d\n", __func__, buffer->len);
}


/** Stores a single byte at a specified offset in the buffer.
 *  The buffer length is not updated.
 *
 *  @param  buf      pointer to Buffer instance
 *  @param  offset   offset in buffer at which to store byte
 *  @param  byte     byte value to be stored
 *
 *  @remark
 *  A dangerous function.  Use with care.
 */
void buffer_set_byte(Buffer * buf, int offset, Byte byte) {
   if (trace_buffer)
      printf("(%s) Storing 0x%02x at offset %d\n", __func__, byte, offset);
   assert(offset >= 0 && offset < buf->buffer_size);
   buf->bytes[offset] = byte;
}


/** Sets a range of bytes in a Buffer.
 *  The logical length of the buffer is not updated.
 *
 *  @param  buf      pointer to Buffer instance
 *  @param  offset   offset in buffer at which to store byte
 *  @param  bytes    pointer to bytes to store
 *  @param  bytect   number of bytes to store
 */
void buffer_set_bytes(Buffer * buf, int offset, Byte * bytes, int bytect) {
   if (trace_buffer)
      printf("(%s) Storing %d bytes at offset %d, buffer_size=%d\n",
             __func__, bytect, offset, buf->buffer_size);
   assert(offset >= 0 && (offset + bytect) <= buf->buffer_size);

   memcpy(buf->bytes+offset, bytes, bytect);
}


/** Appends a sequence of bytes to the current contents of a Buffer.
 *  The buffer length is updated.
 *
 *  @param  buffer    pointer to the Buffer object
 *  @param  bytes     pointer to the bytes to be appended
 *  @param  bytect    number of bytes to append
 */
void buffer_append(Buffer * buffer, Byte * bytes, int bytect) {
   // printf("(%s) Starting. buffer=%p\n", __func__, buffer);
   assert( memcmp(buffer->marker, BUFFER_MARKER, 4) == 0);
   if (trace_buffer) {
      printf("(%s) cur len = %d, appending |%.*s|, bytect=%d\n", __func__, buffer->len, bytect, bytes, bytect);
      printf("(%s) buffer->bytes + buffer->len = %p, bytes=%p, bytect=%d\n",
             __func__, buffer->bytes+buffer->len, bytes, bytect);
   }
   //  buffer->len + 2 + bytect  .. why the  + 2?

   int required_size = buffer->len + 2 + bytect;
   if (required_size > buffer->buffer_size && buffer->size_increment > 0) {
      int new_size = MAX(required_size, buffer->buffer_size + buffer->size_increment);
      if (trace_buffer_resize)
         printf("(%s) Resizing. old size = %d, new size = %d\n",
                __func__, buffer->buffer_size, new_size);
      buffer_extend(buffer, new_size - buffer->buffer_size);
   }

   assert(buffer->len + 2 + bytect <= buffer->buffer_size);

   memcpy(buffer->bytes + buffer->len, bytes, bytect);
   buffer->len = buffer->len + bytect;

   // printf("(%s) Returning.  cur len = %d\n", __func__, buffer->len);
}


/** Appends a string to the current string in the buffer.
 *
 *  @param  buffer pointer to Buffer
 *  @param  str    string to append
 *
 *  @remark
 *  If the buffer is not empty, checks by assert that
 *  the last character stored is '\0';
 */
void buffer_strcat(Buffer * buffer, char * str) {
   assert( memcmp(buffer->marker, BUFFER_MARKER, 4) == 0);
   if (buffer->len == 0) {
      buffer_append(buffer, (Byte *) str, strlen(str)+1);
   }
   else {
      assert(buffer->bytes[buffer->len - 1] == '\0');
      buffer_set_length(buffer, buffer->len - 1);     // truncate trailing \0
      buffer_append(buffer, (Byte *) str, strlen(str) + 1);
   }
}


/** Appends a single byte to the current value in the buffer.
 *  The buffer length is updated.
 *
 *  @param buffer   pointer to Buffer instance
 *  @param byte     value to append
 *
 *  @todo Increase buffer size if necessary and size_increment > 0
 */
void     buffer_add(Buffer * buffer, Byte byte) {
   assert( memcmp(buffer->marker, BUFFER_MARKER, 4) == 0);
   assert(buffer->len + 1 <= buffer->buffer_size);
   buffer->bytes[buffer->len++] = byte;
}


/** Tests whether 2 Buffer instances have the same
 *  contents.
 *
 *  @param buf1  pointer to first Buffer
 *  @param buf2  pointer to second Buffer
 *  @return true if contents are identical, false if not
 *
 *  @remark
 *  - If both buf1==NULL and buf2==NULL, the result is true
 */
bool     buffer_eq(Buffer* buf1, Buffer* buf2) {
   bool result = false;
   if (!buf1 && !buf2)
      result = true;
   else if (buf1 && buf2 &&
            buf1->len == buf2->len &&
            memcmp(buf1->bytes, buf2->bytes, buf1->len) == 0
           )
      result = true;
   return result;
}


/** Increases the size of a Buffer
 *
 *  @param buf       pointer to Buffer instance
 *  @param addl_size number of additional bytes
 */
void     buffer_extend(Buffer* buf, int addl_size) {
   int new_size = buf->buffer_size + addl_size;
   buf->bytes = realloc(buf->bytes, new_size);
   buf->buffer_size = new_size;
}


/** Displays all fields of the Buffer.
 *  This is a debugging function.
 *
 *  @param buffer   pointer to Buffer instance
 *
 *  @remark
 *  Output is written to stdout.
 */
void buffer_dump(Buffer * buffer) {
   printf("Buffer at %p,  bytes addr=%p, len=%d, max_size=%d\n",
          (void*)buffer, buffer->bytes, buffer->len, buffer->buffer_size);
   // printf("  bytes end addr=%p\n", buffer->bytes+buffer->buffer_size);
   if (buffer->bytes)
      hex_dump(buffer->bytes, buffer->len);
}


//
// Identifier id to name and description lookup
//

/** Returns the name of an entry in a Value_Nmme_Title table.
 *
 * @param table  pointer to table
 * @param val    value to lookup
 *
 * @return name of value, NULL if not found
 */
char * vnt_name(Value_Name_Title* table, uint32_t val) {
   // printf("(%s) val=%d\n", __func__, val);
   // debug_vnt_table(table);
   char * result = NULL;

   Value_Name_Title * cur = table;
   for (; cur->name; cur++) {
      if (val == cur->value) {
         result = cur->name;
         break;
      }
   }
   return result;
}


/** Returns the title (description field) of an entry in a Value_Nmme_Title table.
 *
 * @param table  pointer to table
 * @param val    value to lookup
 *
 * @return title of value, NULL if not found
 */
char * vnt_title(Value_Name_Title* table, uint32_t val) {
   // printf("(%s) val=%d\n", __func__, val);
   // debug_vnt_table(table);
   char * result = NULL;

   Value_Name_Title * cur = table;
   for (; cur->name; cur++) {
      if (val == cur->value) {
         result = cur->title;
         break;
      }
   }
   return result;
}


/** Searches a Value_Name_Title_Table for a specified name or title,
 *  and returns its id value.
 *
 *  @param table a      Value_Name_Title table
 *  @param s            string to search for
 *  @param use_title    if false, search name  field\n
 *                      if true,  search title field
 *  @param ignore_case  if true, search is case-insensitive
 *  @param default_id   value to return if not found
 *
 *  @result value id
 */
uint32_t vnt_find_id(
           Value_Name_Title_Table table,
           const char * s,
           bool use_title,       // if false, search by symbolic name, if true, search by title
           bool ignore_case,
           uint32_t default_id)
{
   assert(s);
   uint32_t result = default_id;
   Value_Name_Title * cur = table;
   for (; cur->name; cur++) {
      char * comparand = (use_title) ? cur->title : cur->name;
      if (comparand) {
         int comprc = (ignore_case)
                         ? strcasecmp(s, comparand)
                         : strcmp(    s, comparand);
         if (comprc == 0) {
            result = cur->value;
            break;
         }
      }
   }
   return result;
}


/** Interprets an integer whose bits represent named flags.
 *
 * @param flags_val      value to interpret
 * @param bitname_table  pointer to Value_Name table
 * @param use_title      if **true**, use the **title** field of the table,\n
 *                       if **false**, use the **name** field of the table
 * @param sepstr         if non-NULL, separator string to insert between values
 *
 * @return newly allocated character string
 *
 * @remark
 * - It is the responsibility of the caller to free the returned string
 * - If a referenced **title** field is NULL, "missing" is used as the value
 */
char * vnt_interpret_flags(
      uint32_t                flags_val,
      Value_Name_Title_Table  bitname_table,
      bool                    use_title,
      char *                  sepstr)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. flags_val=0x%08x, bitname_table=%p, use_title=%s, sepstr=|%s|\n",
             __func__, flags_val, (void*)bitname_table, sbool(use_title), sepstr);

   GString * sbuf = g_string_sized_new(200);
   bool first = true;
   Value_Name_Title * cur_entry = bitname_table;
     while (cur_entry->name) {
        if (debug)
           printf("(%s) cur_entry=%p, Comparing flags_val=0x%08x vs cur_entry->value = 0x%08x\n",
                  __func__, (void*)cur_entry, flags_val, cur_entry->value);
        if (!flags_val && cur_entry->value == flags_val) { // special value for no bit set
           char * sval = (use_title) ? cur_entry->title : cur_entry->name;
           if (!sval)
              sval = "missing";
           g_string_append(sbuf, sval);
           break;
        }
        if (flags_val & cur_entry->value) {
           if (first)
              first = false;
           else {
              if (sepstr) {
                 g_string_append(sbuf, sepstr);
              }
           }

           char * sval = (use_title) ? cur_entry->title : cur_entry->name;
           if (!sval) {
              sval = "missing";
           }
           g_string_append(sbuf, sval);
        }
        cur_entry++;
     }
     char * result = strdup(sbuf->str);
     g_string_free(sbuf, true);

     if (debug)
        printf("(%s) Done. Returning: |%s|\n", __func__, result);
     return result;

}


/** Shows the contents of a **Value_Name_Title table.
 *  Output is written to stdout.
 *
 * @param table pointer to table
 */
void vnt_debug_table(Value_Name_Title * table) {
   printf("Value_Name_Title table:\n");
   Value_Name_Title * cur = table;
   for (; cur->name; cur++) {
      printf("   %2d %-30s %s\n",  cur->value, cur->name, cur->title);
   }
}


//
// Circular_String_Buffer
//

/** Allocates a new #Circular_String_Buffer
 *
 *  @param  size  buffer size (number of entries)
 *  @return newly allocated #Circular_String_Buffer
 */
Circular_String_Buffer *
csb_new(int size) {
   Circular_String_Buffer * csb = calloc(1, sizeof(Circular_String_Buffer));
   csb->lines = calloc(size, sizeof(char*));
   csb->size = size;
   csb->ct = 0;
   return csb;
}


/** Appends a string to a #Circular_String_Buffer.
 *
 *  \param   csb   #Circular_String_Buffer
 *  \param   line  string to append
 *  \param   copy  if true, a copy of the string is appended to the buffer
 *                 if false, the string itself is appended
 */
void
csb_add(Circular_String_Buffer * csb, char * line, bool copy) {
    int nextpos = csb->ct % csb->size;
    // printf("(%s) Adding at ct %d, pos %d, line |%s|\n", __func__, csb->ct, nextpos, line);
    if (csb->lines[nextpos])
       free(csb->lines[nextpos]);
    if (copy)
       csb->lines[nextpos] = g_strdup(line);
    else
       csb->lines[nextpos] = line;
    csb->ct++;
}


/** All the strings in a #Circular_String_Buffer are moved to  a newly
 *  allocated GPtrArray. The count of lines in the now empty #Circular_String_Buffer
 *  is set to 0.
 *
 *   \param csb #Circular_String_Buffer to convert
 *   \return    newly allocated #GPtrArray
 */
GPtrArray *
csb_to_g_ptr_array(Circular_String_Buffer * csb) {
   // printf("(%s) csb->size=%d, csb->ct=%d\n", __func__, csb->size, csb->ct);
   GPtrArray * pa = g_ptr_array_sized_new(csb->ct);

   int first = 0;
   if (csb->ct > csb->size)
      first = csb->ct % csb->size;
   // printf("(%s) first=%d\n", __func__, first);

   for (int ndx = 0; ndx < csb->ct; ndx++) {
      int pos = (first + ndx) % csb->size;
      char * s = csb->lines[pos];
      // printf("(%s) line %d, |%s|\n", __func__, ndx, s);

      g_ptr_array_add(pa, s);
   }
   csb->ct = 0;
   return pa;
}


//
// bs256 - Bit_Set_256
//
// A data structure containing 256 flags
//

const Bit_Set_256 EMPTY_BIT_SET_256 = {{0}};

Bit_Set_256 bs256_add(
    Bit_Set_256 flags,
    Byte val)
{
    bool debug = false;

    Bit_Set_256 result = flags;
    int flagndx   = val >> 3;
    int shiftct   = val & 0x07;
    Byte flagbit  = 0x01 << shiftct;
    if (debug) // coverity[DEADCODE]
       printf("(%s) val=0x%02x, flagndx=%d, shiftct=%d, flagbit=0x%02x\n",
              __func__, val, flagndx, shiftct, flagbit);
    result.bytes[flagndx] |= flagbit;

    if (debug) {  //  /* coverity[DEADCODE] */ at start of line doesn't work
       /* coverity[dead_error_begin] */
       char * bs1 = bs256_to_string(flags, "","");
       char * bs2 = bs256_to_string(result, "","");
       printf("(%s) bitstring=%s, value %d, returning: %s\n",
              __func__, bs1, val, bs2);
       // free( bs1);
       // free(bs2);
       // printf("(%s) wolf 3\n", __func__);
    }


    return result;
}


bool bs256_contains(
    Bit_Set_256 flags,
    Byte val)
{
    bool debug = false;

    int flagndx   = val >> 3;
    int shiftct   = val  & 0x07;
    Byte flagbit  = 0x01 << shiftct;
    // printf("(%s) val=0x%02x, flagndx=%d, shiftct=%d, flagbit=0x%02x\n",
    //        __func__, val, flagndx, shiftct, flagbit);
    bool result = flags.bytes[flagndx] & flagbit;
    if (debug) {
       printf("(%s) flags:\n   ",__func__);
       for (int ndx = 0; ndx < 32; ndx++) {
          printf("%02x", flags.bytes[ndx]);
       }
       printf("\n");
       printf("(%s)  bit %d, returning: %d\n",  __func__, val, result);
    }
    return result;
}


bool bs256_eq(
    Bit_Set_256 set1,
    Bit_Set_256 set2)
{
   return memcmp(&set1, &set2, 32) == 0;
}


Bit_Set_256 bs256_or(
   Bit_Set_256 set1,
   Bit_Set_256 set2)
{
   Bit_Set_256 result;
   for (int ndx = 0; ndx < 32; ndx++) {
      result.bytes[ndx] =  set1.bytes[ndx] | set2.bytes[ndx];
   }
   return result;
}


Bit_Set_256 bs256_and(
   Bit_Set_256 set1,
   Bit_Set_256 set2)
{
   Bit_Set_256 result;
   for (int ndx = 0; ndx < 32; ndx++) {
      result.bytes[ndx] =  set1.bytes[ndx] & set2.bytes[ndx];
   }
   return result;
}


Bit_Set_256 bs256_and_not(
      Bit_Set_256 set1,
      Bit_Set_256 set2)
{
   // DBGMSG("Starting. vcplist1=%p, vcplist2=%p", vcplist1, vcplist2);
   Bit_Set_256 result;
   for (int ndx = 0; ndx < 32; ndx++) {
      result.bytes[ndx] =  set1.bytes[ndx] & ~set2.bytes[ndx];
   }

   // char * s = ddca_bs256_string(&result, "0x",", ");
   // DBGMSG("Returning: %s", s);
   // free(s);
   return result;
}


// #ifdef OLD
int bs256_count(
   Bit_Set_256 bbset)
{
   int result = 0;
   int flagndx;
   int bitndx;
   for (flagndx=0; flagndx < 32; flagndx++) {
      for (bitndx = 0; bitndx < 8; bitndx++) {
         unsigned char flagbit = (0x80 >> bitndx);
         if (bbset.bytes[flagndx] & flagbit)
            result += 1;
      }
   }
   // printf("(%s) returning: %d\n", __func__, result);
   return result;
}
// #endif


#ifdef COMPILE_ERRORS
int bs256_count(
      Bit_Set_256 bbset)
{
   // regard the array of 32 bytes as an array of 8 4-byte unsigned integers
   uint64_t  list2 = (uint64_t) bbset.bytes;
   unsigned int ct = 0;
   for (int ndx = 0; ndx < 4; ndx++) {
      // clever algorithm for counting number of bits per Brian Kernihgan
      uint64_t v = list2[ndx];
      for (; v; ct++) {
        v &= v - 1; // clear the least significant bit set
      }
      // DBGMSG("feature_list_count() returning: %d", ct);
   }
// #ifdef OLD
   assert(ct == bs256_count0(bbset));
// #endif
   return ct;
}
#endif


char *
bs256_to_string(
      Bit_Set_256  bitset,
      const char * value_prefix,
      const char * sepstr)
{
   bool debug = false;
   if (debug) {
      printf("(%s) value_prefix=|%s|, sepstr=|%s|\n",
             __func__, value_prefix, sepstr);
      for (int ndx = 0; ndx < 32; ndx++) {
         printf("%02x", bitset.bytes[ndx]);
      }
      printf("\n");
      // rpt_hex_dump((Byte*)feature_list, 32, 2);
   }

   static GPrivate  key = G_PRIVATE_INIT(g_free);
   static GPrivate  len_key = G_PRIVATE_INIT(g_free);

   if (!value_prefix)
      value_prefix = "";
   if (!sepstr)
      sepstr = "";
   int vsize = strlen(value_prefix) + 2 + strlen(sepstr);
   int bit_ct = bs256_count(bitset);
   int reqd_size = (bit_ct*vsize)+1;   // +1 for trailing null

   char * buf = get_thread_dynamic_buffer(&key, &len_key, reqd_size);
   // char * buf = calloc(1, reqd_size);

   buf[0] = '\0';
   // printf("(%s) feature_ct=%d, vsize=%d, buf size = %d",
   //          __func__, feature_ct, vsize, vsize*feature_ct);

   for (int ndx = 0; ndx < 256; ndx++) {
      if ( bs256_contains(bitset, ndx) )
         sprintf(buf + strlen(buf), "%s%02x%s", value_prefix, ndx, sepstr);
   }

   if (bit_ct > 0)
      buf[ strlen(buf)-strlen(sepstr)] = '\0';

   // printf("(%s) wolf 4\n", __func__);
   // DBGMSG("Returned string length: %d", strlen(buf));
   // DBGMSG("Returning %p - %s", buf, buf);
   if (debug)
   printf("(%s) Returning: len=%d, %s\n", __func__, (int) strlen(buf), buf);

   return buf;
}


#define BBF_ITER_MARKER "BBFI"
typedef struct {
   char        marker[4];
   Bit_Set_256 bbflags;
   int         lastpos;
} _Bit_Set_256_Iterator;


/** Creates an iterator for a #Bit_Set_256 instance.
 *  The iterator is an opaque object.
 */
Bit_Set_256_Iterator
bs256_iter_new(Bit_Set_256 bbflags) {
   _Bit_Set_256_Iterator * result = malloc(sizeof(_Bit_Set_256_Iterator));
   memcpy(result->marker, BBF_ITER_MARKER, 4);
   result->bbflags = bbflags;   // TODO: save pointer to unopaque _BitByteFlags
   result->lastpos = -1;
   return result;
}


/** Free a #Bit_Set_256_Iterator.
 *
 * \param bs256_iter handle to iterator (may be NULL)
 */
void
bs256_iter_free(
      Bit_Set_256_Iterator bs256_iter)
{
   _Bit_Set_256_Iterator * iter = (_Bit_Set_256_Iterator *) bs256_iter;

   if (bs256_iter) {
      assert(memcmp(iter->marker, BBF_ITER_MARKER, 4) == 0);
      iter->marker[3] = 'x';
      free(iter);
   }
}

/** Reinitializes an iterator.  Sets the current position before the first
 *  value.
 *
 * \param bs256_iter handle to iterator
 */
void
bs256_iter_reset(
      Bit_Set_256_Iterator bs256_iter)
{
   _Bit_Set_256_Iterator * iter = (_Bit_Set_256_Iterator *) bs256_iter;
   assert(iter && memcmp(iter->marker, BBF_ITER_MARKER, 4) == 0);

   iter->lastpos = -1;
}


/** Returns the number of the next bit that is set.
 *
 * \param bs256_iter handle to iterator
 * \return number of next bit that is set, -1 if no more
 */
int
bs256_iter_next(
      Bit_Set_256_Iterator
      bs256_iter)
{
   _Bit_Set_256_Iterator * iter = (_Bit_Set_256_Iterator *) bs256_iter;
   assert( iter && memcmp(iter->marker, BBF_ITER_MARKER, 4) == 0);
   // printf("(%s) Starting. lastpos = %d\n", __func__, iter->lastpos);

   int result = -1;
   for (int ndx = iter->lastpos + 1; ndx < 256; ndx++) {
      if (bs256_contains(iter->bbflags, ndx)) {
         result = ndx;
         iter->lastpos = ndx;
         break;
      }
   }
   // printf("(%s) Returning: %d\n", __func__, result);
   return result;
}




