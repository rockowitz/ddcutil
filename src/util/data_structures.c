/*  data_structures.c
 *
 *  Created on: Jun 17, 2014
 *      Author: rock
 *
 *  General purpose data structures..
 */

#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/string_util.h"

#include "util/data_structures.h"


Byte_Value_Array bva_create() {
   GByteArray * ga = g_byte_array_new();
   return (Byte_Value_Array) ga;
}

int bva_length(Byte_Value_Array bva) {
   GByteArray* ga = (GByteArray*) bva;
   return ga->len;
}

void bva_append(Byte_Value_Array bva, Byte item) {
   GByteArray* ga = (GByteArray*) bva;
   GByteArray * ga2 = g_byte_array_append(ga, &item, 1);
   assert(ga2 == ga);
}

Byte bva_get(Byte_Value_Array bva, int ndx) {
   GByteArray* ga = (GByteArray*) bva;
   assert(0 <= ndx && ndx < ga->len);

   guint8 v = ga->data[ndx];
   Byte v1 = v;
   return v1;
}

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

void bva_free(Byte_Value_Array bva) {
   GByteArray* ga = (GByteArray*) bva;
   g_byte_array_free(ga,TRUE);
}

void bva_report(Byte_Value_Array ids, char * title) {
   if (title)
      printf("%s\n", title);
   int ct = bva_length(ids);
   int ndx = 0;
   for (; ndx < ct; ndx++) {
      Byte hval = bva_get(ids, ndx);
      printf("  %02X\n", hval);
   }
}


// Tests and sample code

int egmain(int argc, char** argv) {
   GList* list1 = NULL;
   list1 = g_list_append(list1, "Hello world!");
   // generates warning:
   // printf("The first item is '%s'\n", g_list_first(list1)->data);

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
// 256 bit flags
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


Byte_Bit_Flags bbf_create() {
   _ByteBitFlags* flags = calloc(1, sizeof(_ByteBitFlags));
   memcpy(flags->marker, BYTE_BIT_MARKER, 4);
   return flags;
}


void bbf_free(Byte_Bit_Flags bbflags) {
   // _ByteBitFlags* flags = (_ByteBitFlags*) bbflags;
   BYTE_BIT_UNOPAQUE(flags, bbflags);
   if (flags) {
      assert( memcmp(flags->marker, "BBFG",4) == 0);
      free(flags);
   }
}


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
   //        __func__, hexstring(flags->byte,32), val, result);
   return result;
}


/* Returns a 64 character long hex string representing the data structure.
 *
 * Arguments:
 *    buffer  buffer in which to return string
 *    buflen  buffer length
 *
 * Returns:
 *    character string representation
 *
 * If buffer is NULL then memory is malloc'd.  It is the responsibility
 * of the caller to free the returned string.
 *
 * If buflen is insufficiently large an assertion fails.
 *
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


/* Returns number of bits set
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


/* Returns a string of space separated 2 character hex values
 * representing the bits set in the Byte_Bit_Flag,
 * e.g. "03 7F" if bits 0x03 and 0x7F are set
 *
 * Arguments:
 *   bbflags  ByteBitFlags instance
 *   buffer   pointer to buffer in which to return character string,
 *            if NULL malloc a new buffer
 *   buflen   buffer length
 *
 * Returns:
 *   pointer to character string
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


//
// Cross functions
//

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


void bva_appender(void * data_struct, Byte val) {
   Byte_Value_Array bva = (Byte_Value_Array) data_struct;
   bva_append(bva, val);
}


void bbf_appender(void * data_struct, Byte val) {
   Byte_Bit_Flags bbf = (Byte_Bit_Flags) data_struct;
   assert(bbf);
   bbf_set(bbf, val);
}


bool store_bytehex_list(char * start, int len, void * data_struct, Byte_Appender appender){
   bool ok = true;

   char * buf = malloc(len+1);
   memcpy(buf, start, len);
   buf[len] = '\0';

   char * curpos = buf;
   char * nexttok;
   Byte   byteVal;
   while ( (nexttok = strtok(curpos, " ")) != NULL) {
      if (curpos)
         curpos = NULL;     // for all calls after first
      int ln = strlen(nexttok);
      if (ln == 2)                // normal case
         byteVal = hhc_to_byte(nexttok);
      else if (ln == 1) {
         // on old ultrasharp connected to blackrock (pre v2), values in capabilities
         // string are single digits.  Not clear whether to regard them as decimal or hex,
         // since all values are < 9.  But in that case decimal and single digit hex
         // give the same value.
         char buf[2];
         buf[0] = '0';
         buf[1] = *nexttok;
         byteVal = hhc_to_byte(buf);
      }
      else {
         printf("(%s) Invalid hex value in list: %s\n", __func__, nexttok);
         ok = false;
      }
      // printf("(%s) byteVal=0x%02x  \n", __func__, byteVal );
      appender(data_struct, byteVal);
   }

   // printf("(%s) Returning %s\n", __func__, bool_repr(ok));
   return ok;
}


bool bva_store_bytehex_list(Byte_Value_Array bva, char * start, int len) {
   return store_bytehex_list(start, len, bva, bva_appender);
}


bool bbf_store_bytehex_list(Byte_Bit_Flags bbf, char * start, int len) {
   return store_bytehex_list(start, len, bbf, bbf_appender);
}



//
// Buffer with length management
//

bool trace_buffer_malloc_free = false;
bool trace_buffer = false;    // controls buffer tracing


// Allocates a Buffer instance
//
// Arguments:
//   size   maximum number of bytes that buffer can hold
//
// Returns:
//   pointer to newly allocated Buffer instance

Buffer * buffer_new(int size, char * trace_msg) {
   int hacked_size = size+16;     // try allocating extra space see if free failures go away - overruns?
   // printf("(%s) sizeof(Buffer)=%ld\n", __func__, sizeof(Buffer));    // is 16
   Buffer * buffer = (Buffer *) malloc(sizeof(Buffer));
   memcpy(buffer->marker, BUFFER_MARKER, 4);
   buffer->bytes = (Byte *) calloc(1, hacked_size);    // hack
   buffer->buffer_size = size;
   buffer->len = 0;
   if (trace_buffer_malloc_free)
      printf("(%s) Allocated buffer.  buffer=%p, buffer->bytes=%p, &buffer->bytes=%p, %s\n",
             __func__, buffer, buffer->bytes, &(buffer->bytes), trace_msg);
   return buffer;
}


// Frees a Buffer instance.  All memory associated with the Buffer is released.
//S
// Arguments:
//   buffer    pointer to Buffer instance, must be valid
//
// Returns:
//   nothing

void buffer_free(Buffer * buffer, char * trace_msg) {
   if (trace_buffer_malloc_free)
      printf("(%s) Starting. buffer = %p\n", __func__, buffer);
   assert(buffer);
   assert(memcmp(buffer->marker, BUFFER_MARKER, 4) == 0);

      if (buffer->bytes) {
        if (trace_buffer_malloc_free)
            printf("(%s) Freeing buffer->bytes = %p, &buffer->bytes=%p\n",
                   __func__, buffer->bytes, &(buffer->bytes));
        free(buffer->bytes);
      }
      if (trace_buffer_malloc_free)
         printf("(%s) Freeing buffer = %p, %s\n", __func__, buffer, trace_msg);
      buffer->marker[3] = 'x';
      free(buffer);
      if (trace_buffer_malloc_free)
         printf("(%s) Done\n", __func__);

}


#ifdef UNUSED
// No real benefit over simple assert
//#define BUFFER_POSITION_OUT_OF_RANGE  66   // *** TEMP ***
//
// Macro for standardized parameter verification
#define BUFFER_VALIDATE(EXPR) if (!(EXPR))                                 \
     { printf("(%s) Failed check (" #EXPR ") at line %d in file %s",__func__, __LINE__, __FILE__); \
       exit(BUFFER_POSITION_OUT_OF_RANGE); }
#endif


int buffer_length(Buffer * buffer) {
   return buffer->len;
}

// Adjusts the number of bytes in the buffer.
//
// Arguments:
//   bytect   new length of buffer contents
//

void buffer_set_length(Buffer * buffer, int bytect) {
   if (trace_buffer)
      printf("(%s) bytect=%d, buffer_size=%d\n", __func__, bytect, buffer->buffer_size);
   assert (bytect <= buffer->buffer_size);
   // BUFFER_VALIDATE(bytect <= buffer->buffer_size);

   buffer->len = bytect;
}


// Sets the value stored in the buffer to a range of bytes.
// The buffer length is updated.
//
// Arguments:
//   buffer     pointer to Buffer instance
//   bytes      pointer to bytes to store in buffer
//   bytect     number of bytes to store

void buffer_put(Buffer * buffer, Byte * bytes, int bytect) {
   if (trace_buffer) {
      printf("(%s) buffer->bytes = %p, bytes=%p, bytect=%d\n",
             __func__, buffer->bytes, bytes, bytect);
      printf("(%s) cur len = %d, storing |%.*s|, bytect=%d\n",
             __func__, buffer->len, bytect, bytes, bytect);
   }
   //  buffer->len + 2 + bytect  .. why the  + 2?
   // if (bytect > buffer->buffer_size) {
   //    printf("(%s) Buffer overflow\n", __func__);
   //    exit(1);
   // }
   assert (bytect <= buffer->buffer_size);

   memcpy(buffer->bytes, bytes, bytect);
   buffer->len = buffer->len + bytect;
   // printf("(%s) Returning.  cur len = %d\n", __func__, buffer->len);
}


// Stores a single byte at a specified offset in the buffer.
// The buffer length is not updated.
//
// Arguments:
//   buf      pointer to Buffer instance
//   offset   offset in buffer at which to store byte
//   byte     byte value to be stored

void buffer_set_byte(Buffer * buf, int offset, Byte byte) {
   if (trace_buffer)
      printf("(%s) Storing 0x%02x at offset %d\n", __func__, byte, offset);
   assert(offset >= 0 && offset < buf->buffer_size);
   buf->bytes[offset] = byte;
}


// Sets a range of bytes in the buffer.
// The logical length of the buffer is not updated.
void buffer_set_bytes(Buffer * buf, int offset, Byte * bytes, int bytect) {
   if (trace_buffer)
      printf("(%s) Storing %d bytes at offset %d, buffer_size=%d\n",
             __func__, bytect, offset, buf->buffer_size);
   assert(offset >= 0 && (offset + bytect) <= buf->buffer_size);

   memcpy(buf->bytes+offset, bytes, bytect);
}


// Appends a string of bytes to the current value in the buffer.
// The buffer length is updated.
//
// Arguments:
//   buffer    pointer to the Buffer object
//   bytes     pointer to the bytes to be appended to the current value in the buffer
//   bytect    number of bytes to append

void buffer_append(Buffer * buffer, Byte * bytes, int bytect) {
   // printf("(%s) Starting. buffer=%p\n", __func__, buffer);
   assert( memcmp(buffer->marker, BUFFER_MARKER, 4) == 0);
   if (trace_buffer) {
      printf("(%s) cur len = %d, appending |%.*s|, bytect=%d\n", __func__, buffer->len, bytect, bytes, bytect);
      printf("(%s) buffer->bytes + buffer->len = %p, bytes=%p, bytect=%d\n",
             __func__, buffer->bytes+buffer->len, bytes, bytect);
   }
   //  buffer->len + 2 + bytect  .. why the  + 2?
   assert(buffer->len + 2 + bytect <= buffer->buffer_size);

   memcpy(buffer->bytes + buffer->len, bytes, bytect);
   buffer->len = buffer->len + bytect;

   // printf("(%s) Returning.  cur len = %d\n", __func__, buffer->len);
}


// Debugging method.  Displays all fields of the Buffer.
//
// Arguments:
//   buffer   point to Buffer instance

void buffer_dump(Buffer * buffer) {
   printf("Buffer at %p,  bytes addr=%p, len=%d, max_size=%d\n",
          buffer, buffer->bytes, buffer->len, buffer->buffer_size);
   // printf("  bytes end addr=%p\n", buffer->bytes+buffer->buffer_size);
   if (buffer->bytes)
      hex_dump(buffer->bytes, buffer->len);
}

