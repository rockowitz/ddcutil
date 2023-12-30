/** @file data_structures.c  General purpose data structures */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>        // C standard library
#include <strings.h>      
#include <sys/param.h>     // for MIN, MAX
/** \endcond */

#include "debug_util.h"
#include "report_util.h"
#include "string_util.h"

#include "data_structures.h"


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

   // ASSERT_WITH_BACKTRACE(buffe#ifdef TEMPr);
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
      printf("(%s) Storing %d bytes at offset %d (buf->bytes+offset=%p), buffer_size=%d, bytes=%p, bytes+bytect=%p\n",
             __func__, bytect, offset, buf->bytes+offset, buf->buffer_size, bytes, bytes+bytect);
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


/** Displays all fields of the Buffer using rpt_* functions.
 *  This is a debugging function.
 *
 *  @param buffer   pointer to Buffer instance
 *  @param depth    logical indentation depth
 *
 *  @remark
 *  Output is written to stdout.
 */
void buffer_rpt(Buffer * buffer, int depth) {
   rpt_vstring(depth, "Buffer at %p, bytes addr=%p, len=%d, max_size=%d",
         (void*)buffer, buffer->bytes, buffer->len, buffer->buffer_size);
   if (buffer->bytes)
      rpt_hex_dump(buffer->bytes, buffer->len, depth);
}



//
// Circular_String_Buffer
//

/** Allocates a new #Circular_String_Buffer
 *
 *  @param  size  buffer size (number of entries)
 *  @return pointer to newly allocated #Circular_String_Buffer
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
 *  @param   csb   pointer to #Circular_String_Buffer
 *  @param   line  string to append
 *  @param   copy  if true, a copy of the string is appended to the buffer
 *                 if false, the string itself is appended
 */
void
csb_add(Circular_String_Buffer * csb, char * line, bool copy) {
    int nextpos = csb->ct % csb->size;
    // printf("(%s) Adding at ct %d, pos %d, line |%s|\n", __func__, csb->ct, nextpos, line);
    if (csb->lines[nextpos] && copy)
       free(csb->lines[nextpos]);
    csb->lines[nextpos] = (copy) ? g_strdup(line) : line;
    csb->ct++;
}


/** Frees a #Circular_String_Buffer
 *
 *  @param  csb pointer to #Circular_String_Buffer
 *  @param  free_strings  free the strings pointed to by the #Cirular_String_Buffer
 */

void
csb_free(Circular_String_Buffer * csb, bool free_strings) {
   if (free_strings) {
      int first = 0;
      if (csb->ct > csb->size)
         first = csb->ct % csb->size;
      // printf("(%s) first=%d\n", __func__, first);

      for (int ndx = 0; ndx < csb->ct; ndx++) {
         int pos = (first + ndx) % csb->size;
         char * s = csb->lines[pos];
         // printf("(%s) line %d, |%s|\n", __func__, ndx, s);
         free(s);
      }
      csb->ct = 0;
   }
   free(csb->lines);
   free(csb);
}


/** All the strings in a #Circular_String_Buffer are moved to  a newly
 *  allocated GPtrArray. The count of lines in the now empty #Circular_String_Buffer
 *  is set to 0.
 *
 *   @param csb pointer to #Circular_String_Buffer to convert
 *   @return    newly allocated #GPtrArray
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


#ifdef OLD
//
// Circular Integer Buffer
//

typedef struct {
   int *    values;
   int      size;
   int      ct;
} Circular_Integer_Buffer;


/** Allocates a new #Circular_Integer_Buffer
 *
 *  @param  size  buffer size (number of entries)
 *  @return newly allocated #Circular_Integer_Buffer
 */
Circular_Integer_Buffer *
cib_new(int size) {
   Circular_Integer_Buffer * cib = calloc(1, sizeof(Circular_Integer_Buffer));
   cib->values = calloc(size, sizeof(int));
   cib->size = size;
   cib->ct = 0;
   return cib;
}


void cib_free(Circular_Integer_Buffer * cib) {
   free(cib->values);
   free(cib);
}


/** Appends an integer to a #Circular_Integer_Buffer.
 *
 *  @param   cib   #Circular_Integer_Buffer
 *  @param   value value to append
 */
void
cib_add(Circular_Integer_Buffer * cib, int value) {
    int nextpos = cib->ct % cib->size;
    // printf("(%s) Adding at ct %d, pos %d, value %d\n", __func__, cib->ct, nextpos, value);
       cib->values[nextpos] = value;
    cib->ct++;
}


void cib_get_latest(Circular_Integer_Buffer * cib, int ct, int latest_values[]) {
   assert(ct <= cib->ct);
   int ctr = 0;

   while(ctr < ct) {int_min
      int ndx = (ctr > 0) ? (ctr-1) % cib->size : cib->size - 1;
      latest_values[ctr] = cib->values[ ndx ];
   }
}

#endif


//
// Identifier id to name and description lookup
//

/** Returns the name of an entry in a Value_Name_Title table.
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


/** Returns the title (description field) of an entry in a Value_Name_Title table.
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
int32_t vnt_find_id(
           Value_Name_Title_Table table,
           const char * s,
           bool use_title,       // if false, search by symbolic name, if true, search by title
           bool ignore_case,
           int32_t default_id)
{
   assert(s);
   int32_t result = default_id;
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
 * @param  flags_val      value to interpret
 * @param  bitname_table  pointer to Value_Name table
 * @param  use_title      if **true**, use the **title** field of the table,\n
 *                        if **false**, use the **name** field of the table
 * @param  sepstr         if non-NULL, separator string to insert between values
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
     char * result = g_strdup(sbuf->str);
     g_string_free(sbuf, true);

     if (debug)
        printf("(%s) Done. Returning: |%s|\n", __func__, result);
     return result;
}


/** Interprets a flags value as a printable string. The returned string is
 *  valid until the next call of this function in the current thread.
 *
 *  @param flags_val      value to interpret
 *  @param bitname_table  pointer to Value_Name_Title table
 *  @param use_title      if **true**, use the **title** field of the table,\n
 *                        if **false**, use the **name** field of the table
 *  @param sepstr         if non-NULL, separator string to insert between values
 *
 *  @return interpreted value
 */
char * vnt_interpret_flags_t(
      uint32_t                flags_val,
      Value_Name_Title_Table  bitname_table,
      bool                    use_title,
      char *                  sepstr)
{
   static GPrivate  x_key = G_PRIVATE_INIT(g_free);
   static GPrivate  x_len_key = G_PRIVATE_INIT(g_free);

   char * buftemp = vnt_interpret_flags(flags_val, bitname_table, use_title, sepstr);
   char * buf = get_thread_dynamic_buffer(&x_key, &x_len_key, strlen(buftemp)+1);
   strcpy(buf, buftemp);
   free(buftemp);
   return buf;
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


// bva - Byte Value Array
//
// An opaque structure containing an array of bytes that
// can grow dynamically.  Note that the same byte can
// appear multiple times.

/** Creates a new **Byte_Value_Array** instance.
 *  @return newly allocated **Byte_Value_Array**.
 */
Byte_Value_Array bva_create() {
   GByteArray * ga = g_byte_array_new();
   return (Byte_Value_Array) ga;
}


/** Creates a new **Byte_Value_Array** instance,
 *  containing the values from an existing instance
 *  that satisfy the filter function.
 *
 *  @param  bva **Byte_Value_Array** instance
 *  @param  filter_func  function that takes a byte value as an argument,
 *                       returning true if the value should be included
 *                       in the output **Byte_Value_Array**
 *  @return new **Byte_Value_Array**
 */
Byte_Value_Array bva_filter(Byte_Value_Array  bva, IFilter filter_func) {
   GByteArray * src = (GByteArray*) bva;
   GByteArray * result = g_byte_array_new();
   for (guint ndx=0; ndx < src->len;  ndx++) {
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
#ifdef NDEBUG
   g_byte_array_append(ga, &item, 1);
#else
   GByteArray * ga2 = g_byte_array_append(ga, &item, 1);
   assert(ga2 == ga);
#endif
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
Byte bva_get(Byte_Value_Array bva, guint ndx) {
   GByteArray* ga = (GByteArray*) bva;
   assert(ndx < ga->len);

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
   guint ndx;
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
 *  @param  bva1  pointer to first instance
 *  @param  bva2  pointer to second instance
 *  \retval true  arrays are identical
 *  \retval false arrays not identical
 *
 *  @remark
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


#ifdef TESTS
// Tests and sample code

int egmain(int argc, char** argv) {
   GList* list1 = NULL;
   list1 = g_list_append(list1, "Hello world!");
   // generates warning:
   // printf("The first item is '%s'\n", g_list_first(list1)->data);
   g_list_free(list1);bbf_to_string

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
#endif


//
// Bit_Set_256 - A data structure containing 256 flags
//

const Bit_Set_256 EMPTY_BIT_SET_256 = {{0}};

/** Sets a flag in a #Bit_Set_256
 *
 *  @param  flags   existing #Bit_Set_256 value
 *  @param  flagno  flag number to set (0 based)
 *  @return updated set
 */
Bit_Set_256 bs256_insert(
    Bit_Set_256 bitset,
    Byte        bitno)
{
    bool debug = false;

    Bit_Set_256 result = bitset;
    int bytendx   = bitno >> 3;
    int shiftct   = bitno & 0x07;
    Byte flagbit  = 0x01 << shiftct;
    if (debug) {
       printf("(%s) bitno=0x%02x, bytendx=%d, shiftct=%d, flagbit=0x%02x\n",
              __func__, bitno, bytendx, shiftct, flagbit);
    }
    result.bytes[bytendx] |= flagbit;

    if (debug) {
       char * bs1 = g_strdup(bs256_to_string_t(bitset,  "",""));
       char * bs2 = g_strdup(bs256_to_string_t(result, "",""));
       printf("(%s) old bitstring=%s, value %d, returning: %s\n",
              __func__, bs1, bitno, bs2);
       free( bs1);
       free(bs2);
    }

    return result;
}


/** Tests if a bit is set in a #Bit_Set_256.
 *
 *  @param bitset  #Bit_Set_256 to check
 *  @param bitno   bit number to test (0 based)
 *  @return true/false
 */
bool bs256_contains(
    Bit_Set_256 bitset,
    Byte        bitno)
{
    bool debug = false;

    int flagndx   = bitno >> 3;
    int shiftct   = bitno  & 0x07;
    Byte flagbit  = 0x01 << shiftct;
    // printf("(%s) bitno=0x%02x, flagndx=%d, shiftct=%d, flagbit=0x%02x\n",
    //        __func__, bitno, flagndx, shiftct, flagbit);
    bool result = bitset.bytes[flagndx] & flagbit;
    if (debug) {
       printf("(%s) bitset:\n   ",__func__);
       for (int ndx = 0; ndx < 32; ndx++) {
          printf("%02x", bitset.bytes[ndx]);
       }
       printf("\n");
       printf("(%s)  bit %d, returning: %d\n",  __func__, bitno, result);
    }
    return result;
}

/** Returns the bit number of the first bit set.
 *  @param  bitset #Bit_Set_256 to check
 *  @return number of first bit that is set (0 based),
 *          -1 if no bits set
 */
int bs256_first_bit_set(
      Bit_Set_256 bitset)
{
   int result = -1;
   for (int ndx = 0; ndx < 256; ndx++) {
      if (bs256_contains(bitset, ndx)) {
         result = ndx;
         break;
      }
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


#define BB256_REPR_BUF_SZ (3*32+1)
/** Represents a #Bit_Set_256 value as a sequence of 32 hex values.
 *
 *  @param buf   buffer in which to return value
 *  @param bufsz buffer size, must be at least #BB256_REPR_BUF_SZ
 *  @param bbset value to represent
 */
void bb256_repr(char * buf, int bufsz, Bit_Set_256 bbset) {
   assert(bufsz >= BB256_REPR_BUF_SZ);
   g_snprintf(buf, bufsz,
              "%02x %02x %02x %02x %02x %02x %02x %02x "
              "%02x %02x %02x %02x %02x %02x %02x %02x "
              "%02x %02x %02x %02x %02x %02x %02x %02x "
              "%02x %02x %02x %02x %02x %02x %02x %02x",
              bbset.bytes[ 0], bbset.bytes[ 1], bbset.bytes[ 2], bbset.bytes[ 3],
              bbset.bytes[ 4], bbset.bytes[ 5], bbset.bytes[ 6], bbset.bytes[ 7],
              bbset.bytes[ 8], bbset.bytes[ 9], bbset.bytes[10], bbset.bytes[11],
              bbset.bytes[12], bbset.bytes[13], bbset.bytes[14], bbset.bytes[15],
              bbset.bytes[16], bbset.bytes[17], bbset.bytes[18], bbset.bytes[19],
              bbset.bytes[20], bbset.bytes[21], bbset.bytes[22], bbset.bytes[23],
              bbset.bytes[24], bbset.bytes[25], bbset.bytes[26], bbset.bytes[27],
              bbset.bytes[28], bbset.bytes[29], bbset.bytes[30], bbset.bytes[31] );
}


/** Returns the number of bits set in a #Bit_Set_256 instance.
 *
 *  @param  bbset  value to examine
 *  @return number of bits set
 */
int bs256_count(
   Bit_Set_256 bbset)
{
   bool debug = false;

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
   if (debug) {
      char buf[BB256_REPR_BUF_SZ];
      bb256_repr(buf, sizeof(buf), bbset);
      printf("(%s) Returning %d. bbset: %s\n", __func__, result, buf);
   }
   return result;
}

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

/** Returns a string representation of a #Bit_Set_256 as a list of hex numbers.
 *
 *  The value returned is valid until the next call to this function in the
 *  current thread.
 *
 *  @param  bitset value to represent
 *  @param  decimal_format  if true,  represent values as decimal numbers
 *                          if false, represent values as hex numbers
 *  @param  value_prefix  prefix for each hex number, typically "0x" or ""
 *  @param  sepstr        string to insert between each value, typically "", ",", or " "
 *  @return string representation, caller should not free
 */
static char *
bs256_to_string_general_t(
      Bit_Set_256  bitset,
      bool         decimal_format,
      const char * value_prefix,
      const char * sepstr)
{
   bool debug = false;
   if (debug) {
      printf("(%s) value_prefix=|%s|, sepstr=|%s| bitset: ",
             __func__, value_prefix, sepstr);
      for (int ndx = 0; ndx < 32; ndx++) {
         printf("%02x", bitset.bytes[ndx]);
      }
      printf("\n");
      // rpt_hex_dump((Byte*)feature_list, 32, 2);
   }

   static GPrivate  key =     G_PRIVATE_INIT(g_free);
   static GPrivate  len_key = G_PRIVATE_INIT(g_free);

   if (!value_prefix)
      value_prefix = "";
   if (!sepstr)
      sepstr = "";
   int vsize = strlen(value_prefix) + 2 + strlen(sepstr);   // for hex
   if (decimal_format)
       vsize = strlen(value_prefix) + 3 + strlen(sepstr);   // for decimal
   int bit_ct = bs256_count(bitset);
   int reqd_size = (bit_ct*vsize)+1;   // +1 for trailing null

   char * buf = get_thread_dynamic_buffer(&key, &len_key, reqd_size);
   // char * buf = calloc(1, reqd_size);

   buf[0] = '\0';
   // printf("(%s) feature_ct=%d, vsize=%d, buf size = %d",
   //          __func__, feature_ct, vsize, vsize*feature_ct);

   for (int ndx = 0; ndx < 256; ndx++) {
      if ( bs256_contains(bitset, ndx) ) {
         if (decimal_format)
            sprintf(buf + strlen(buf), "%s%d%s", value_prefix, ndx, sepstr);
         else
            sprintf(buf + strlen(buf), "%s%02x%s", value_prefix, ndx, sepstr);
      }
   }

   if (bit_ct > 0)
      buf[ strlen(buf)-strlen(sepstr)] = '\0';

   // printf("(%s) wolf 4\n", __func__);
   // DBGMSG("Returned string length: %d", strlen(buf));
   // DBGMSG("Returning %p - %s", buf, buf);
   if (debug)
      printf("(%s) Returning: string of length %d: |%s|\n", __func__, (int) strlen(buf), buf);

   return buf;
}


/** Returns a string representation of a #Bit_Set_256 as a list of hex numbers.
 *
 *  The value returned is valid until the next call to this function or to
 *  #bs256_to_string_decimal() in the current thread.
 *
 *  @param  bitset value to represent
 *  @param  value_prefix  prefix for each hex number, typically "0x" or ""
 *  @param  sepstr        string to insert between each value, typically "", ",", or " "
 *  @return string representation, caller should not free
 */
char *
bs256_to_string_t(
      Bit_Set_256  bitset,
      const char * value_prefix,
      const char * sepstr)
{
   return bs256_to_string_general_t(bitset, false, value_prefix, sepstr);
}

/** Returns a string representation of a #Bit_Set_256 as a list of decimal numbers.
 *
 *  The value returned is valid until the next call to this function or to
 *  #bs256_to_string() in the current thread.
 *
 *  @param  bitset value to represent
 *  @param  value_prefix  prefix for each decimal number
 *  @param  sepstr        string to insert between each value, typically "", ",", or " "
 *  @return string representation, caller should not free
 */
char *
bs256_to_string_decimal_t(
      Bit_Set_256  bitset,
      const char * value_prefix,
      const char * sepstr)
{
   return bs256_to_string_general_t(bitset, true, value_prefix, sepstr);
}


int bs256_to_bytes(Bit_Set_256 flags, Byte * buffer, int buflen) {
   // printf("(%s) Starting\n", __func__);
#ifndef NDEBUG
   int bit_set_ct = bs256_count(flags);
   assert(buflen >= bit_set_ct);
#endif
   unsigned int bufpos = 0;
   unsigned int flagno = 0;
   // printf("(%s) bs256lags->byte=0x%s\n", __func__, hexstring(flags->byte,32));
   for (flagno = 0; flagno < 256; flagno++) {
      Byte flg = (Byte) flagno;
      // printf("(%s) flagno=%d, flg=0x%02x\n", __func__, flagno, flg);
      if (bs256_contains(flags, flg)) {
         // printf("(%s) Flag is set: %d, 0x%02x\n", __func__, flagno, flg);
         buffer[bufpos++] = flg;
      }
   }
   // printf("(%s) Done.  Returning: %d\n", __func__, bupos);
   return bufpos;
}


/** Converts a **Bit_Set_256** instance to a sequence of bytes whose values
 *  correspond to the bits that are set.
 *  The byte sequence is returned in a newly allocated **Buffer**.
 *
 * @param  bs256lags  instance handle
 * @return pointer to newly allocated **Buffer**
 */
Buffer * bs256_to_buffer(Bit_Set_256 flags) {
   int bit_set_ct = bs256_count(flags);
   Buffer * buf = buffer_new(bit_set_ct, __func__);
   for (unsigned int flagno = 0; flagno < 256; flagno++) {
      Byte flg = (Byte) flagno;
      // printf("(%s) flagno=%d, flg=0x%02x\n", __func__, flagno, flg);
      if (bs256_contains(flags, flg)) {
         buffer_add(buf, flg);
      }
   }
   // printf("(%s) Done.  Returning: %s\n", __func__, buffer);
   return buf;
}


#define BS256_ITER_MARKER "BSIM"
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
   memcpy(result->marker, BS256_ITER_MARKER, 4);
   result->bbflags = bbflags;   // TODO: save pointer to unopaque _BitByteFlags
   result->lastpos = -1;
   return result;
}


/** Free a #Bit_Set_256_Iterator.
 *
 * @param bs256_iter handle to iterator (may be NULL)
 */
void
bs256_iter_free(
      Bit_Set_256_Iterator bs256_iter)
{
   _Bit_Set_256_Iterator * iter = (_Bit_Set_256_Iterator *) bs256_iter;

   if (bs256_iter) {
      assert(memcmp(iter->marker, BS256_ITER_MARKER, 4) == 0);
      iter->marker[3] = 'x';
      free(iter);
   }
}

/** Reinitializes an iterator.  Sets the current position before the first
 *  value.
 *
 * @param bs256_iter handle to iterator
 */
void
bs256_iter_reset(
      Bit_Set_256_Iterator bs256_iter)
{
   _Bit_Set_256_Iterator * iter = (_Bit_Set_256_Iterator *) bs256_iter;
   assert(iter && memcmp(iter->marker, BS256_ITER_MARKER, 4) == 0);

   iter->lastpos = -1;
}


/** Returns the number of the next bit that is set.
 *
 * @param bs256_iter handle to iterator
 * @return number of next bit that is set, -1 if no more
 */
int
bs256_iter_next(
      Bit_Set_256_Iterator
      bs256_iter)
{
   _Bit_Set_256_Iterator * iter = (_Bit_Set_256_Iterator *) bs256_iter;
   assert( iter && memcmp(iter->marker, BS256_ITER_MARKER, 4) == 0);
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


// TODO:
// Extracted from feature_list.cpp in ddcui. parse_custom_feature_list()
// should be rewritten to call this function.

/** Parse a string containing a list of hex values.
 *
 *  @param unparsed_string
 *  @param error_msgs_loc  if non-null, return null terminated string array of error messages here,
 *                   caller is responsible for freeing
 *  @return #Bit_Set_256, will be EMPTY_BIT_SET_256 if errors
 *
 *  @remark
 *  If error_msgs_loc is non-null on entry, on return the value it points to
 *  is non-null iff there are error messages, i.e. a 0 length array is never returned
 */
Bit_Set_256 bs256_from_string(
      char *                         unparsed_string,
      Null_Terminated_String_Array * error_msgs_loc)
{
    assert(unparsed_string);
    assert(error_msgs_loc);
    bool debug = false;
    if (debug)
       printf("(bs256_from_string) unparsed_string = |%s|\n", unparsed_string );

    Bit_Set_256 result = EMPTY_BIT_SET_256;
    *error_msgs_loc = NULL;
    GPtrArray * errors = g_ptr_array_new();

    // convert all commas to blanks
    char * x = unparsed_string;
    while (*x) {
       if (*x == ',')
          *x = ' ';
       x++;
    }

    // gchar ** pieces = g_strsplit(features_work, " ", -1); // doesn't handle multiple blanks
    Null_Terminated_String_Array pieces = strsplit(unparsed_string, " ");
    int ntsal = ntsa_length(pieces);
    if (debug)
       printf("(bs256_from_string) ntsal=%d\n", ntsal );
    if (ntsal == 0) {
       if (debug)
          printf("(bs256_from_string) Empty string\n");
    }
    else {
       bool ok = true;
       int ndx = 0;
       for (; pieces[ndx] != NULL; ndx++) {
           // char * token = strtrim_r(pieces[ndx], trimmed_piece, 10);
           char * token = g_strstrip(pieces[ndx]);
           if (debug)
              printf("(parse_features_list) token= |%s|\n", token);
           Byte hex_value = 0;
           if ( any_one_byte_hex_string_to_byte_in_buf(token, &hex_value) ) {
              result = bs256_insert(result, hex_value);
           }
           else {
              if (debug)
                 printf("(bs256_from_string) Invalid hex value: %s\n", token);
              char * s = g_strdup_printf("Invalid hex value: %s", token);
              g_ptr_array_add(errors, s);
              ok = false;
           }
       }
       assert(ndx == ntsal);


       ASSERT_IFF(ok, errors->len == 0);

       if (ok) {
          g_ptr_array_free(errors,true);
          *error_msgs_loc = NULL;
       }
       else {
          result = EMPTY_BIT_SET_256;
          *error_msgs_loc = g_ptr_array_to_ntsa(errors, false);
          g_ptr_array_free(errors, false);
       }
    }
    ntsa_free(pieces, /* free_strings */ true);

    if (debug) {
       const char * s = bs256_to_string_t(result, /*prefix*/ "x", /*sepstr*/ ",");
       printf("Returning bit set: %s\n", s);
       if (*error_msgs_loc) {
          printf("(bs256_from_string) Returning error messages:\n");
          ntsa_show(*error_msgs_loc);
       }
    }
    return result;
}


Bit_Set_256 bs256_from_bva(Byte_Value_Array bva) {
   Bit_Set_256 result = EMPTY_BIT_SET_256;
   int ct = bva_length(bva);
   for (int ndx = 0; ndx < ct; ndx++) {
      Byte bitno = bva_get(bva, ndx);
      result = bs256_insert(result, bitno);
   }
   return result;
}


//
// Bit_Set_32 - A set of 32 flags
//

const Bit_Set_32 EMPTY_BIT_SET_32 = 0;
const int BIT_SET_32_MAX = 32;


#define BB32_REPR_BUF_SZ 10
/** Represents a #Bit_Set_32 value as a sequence of 4 hex values.
 *
 *  @param buf   buffer in which to return value
 *  @param bufsz buffer size, must be at least #BB256_REPR_BUF_SZ
 *  @param bbset value to represent
 */
void bb32_repr(char * buf, int bufsz, Bit_Set_32 bbset) {
   assert(bufsz >= BB256_REPR_BUF_SZ);
   g_snprintf(buf, bufsz, "0x%08x", bbset);
}


/** Returns the number of bits set in a #Bit_Set_32 instance.
 *
 *  @param  bbset  value to examine
 *  @return number of bits set
 */
int bs32_count(
   Bit_Set_32 bbset)
{
   bool debug = false;

   int result = 0;
   for (int bitndx = 0; bitndx < 32; bitndx++) {
      unsigned char flagbit = (0x80 >> bitndx);
      if (bbset & flagbit)
         result += 1;
   }
   if (debug) {
      char buf[BB32_REPR_BUF_SZ];
      bb32_repr(buf, sizeof(buf), bbset);
      printf("(%s) Returning %d. bbset: %s\n", __func__, result, buf);
   }
   return result;
}


bool bs32_contains(Bit_Set_32 flags, uint8_t val) {
   assert(val < BIT_SET_32_MAX);
   bool result = flags & (1 << val);
   return result;
}


Bit_Set_32 bs32_insert(Bit_Set_32 flags, uint8_t val) {
   Bit_Set_32 result = flags | (1 << val);
   return result;
}


/** Returns a string representation of a #Bit_Set_32 as a list of hex numbers.
 *
 *  The value returned is valid until the next call to this function in the
 *  current thread.
 *
 *  @param  bitset value to represent
 *  @param  decimal_format  if true,  represent values as decimal numbers
 *                          if false, represent values as hex numbers
 *  @param  value_prefix  prefix for each hex number, typically "0x" or ""
 *  @param  sepstr        string to insert between each value, typically "", ",", or " "
 *  @return string representation, caller should not free
 */
static char *
bs32_to_string_general(
      Bit_Set_32   bitset,
      bool         decimal_format,
      const char * value_prefix,
      const char * sepstr)
{
   bool debug = false;
   if (debug) {
      printf("(%s) value_prefix=|%s|, sepstr=|%s| bitset: 0x%08x",
             __func__, value_prefix, sepstr, bitset);
      // rpt_hex_dump((Byte*)feature_list, 32, 2);
   }

   static GPrivate  key =     G_PRIVATE_INIT(g_free);
   static GPrivate  len_key = G_PRIVATE_INIT(g_free);

   if (!value_prefix)
      value_prefix = "";
   if (!sepstr)
      sepstr = "";
   int vsize = strlen(value_prefix) + 2 + strlen(sepstr);   // for hex
   if (decimal_format)
       vsize = strlen(value_prefix) + 3 + strlen(sepstr);   // for decimal
   int bit_ct = bs32_count(bitset);
   int reqd_size = (bit_ct*vsize)+1;   // +1 for trailing null

   char * buf = get_thread_dynamic_buffer(&key, &len_key, reqd_size);
   // char * buf = calloc(1, reqd_size);

   buf[0] = '\0';
   // printf("(%s) feature_ct=%d, vsize=%d, buf size = %d",
   //          __func__, feature_ct, vsize, vsize*feature_ct);

   for (int ndx = 0; ndx < 32; ndx++) {
      if ( bs32_contains(bitset, ndx) ) {
         if (decimal_format)
            sprintf(buf + strlen(buf), "%s%d%s", value_prefix, ndx, sepstr);
         else
            sprintf(buf + strlen(buf), "%s%02x%s", value_prefix, ndx, sepstr);
      }
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


char* bs32_to_bitstring(Bit_Set_32 val, char * buf, int bufsz) {
   assert(bufsz >= BIT_SET_32_MAX+1);

   // char result[BIT_SET_32_MAX+1];
   for (int ndx = 0; ndx < BIT_SET_32_MAX; ndx++) {
      buf[(BIT_SET_32_MAX-1)-ndx] = (val & 0x01) ? '1' : '0';
      val = val >> 1;
   }
   buf[BIT_SET_32_MAX] = '\0';
   return buf;
}


/** Returns a string representation of a #Bit_Set_32 as a list of hex numbers.
 *
 *  The value returned is valid until the next call to this function or to
 *  #bs32_to_string_decimal() in the current thread.
 *
 *  @param  bitset value to represent
 *  @param  value_prefix  prefix for each hex number, typically "0x" or ""
 *  @param  sepstr        string to insert between each value, typically "", ",", or " "
 *  @return string representation, caller should not free
 */
char *
bs32_to_string(
      Bit_Set_32   bitset,
      const char * value_prefix,
      const char * sepstr)
{
   return bs32_to_string_general(bitset, false, value_prefix, sepstr);
}

/** Returns a string representation of a #Bit_Set_32 as a list of decimal numbers.
 *
 *  The value returned is valid until the next call to this function or to
 *  #bs32_to_string() in the current thread.
 *
 *  @param  bitset value to represent
 *  @param  value_prefix  prefix for each decimal number
 *  @param  sepstr        string to insert between each value, typically "", ",", or " "
 *  @return string representation, caller should not free
 */
char *
bs32_to_string_decimal(
      Bit_Set_32   bitset,
      const char * value_prefix,
      const char * sepstr)
{
   return bs32_to_string_general(bitset, true, value_prefix, sepstr);
}


//
// Cross data structure functions bba <-> bs256
//

/** Tests if the bit number of every byte in a #Byte_Value_Array is set
 *  in a #Bit_Set_256, and conversely that for every bit set in the
 *  #Bit_Set_256 there is a corresponding byte in the #Byte_Value_Array.
 *
 *  Note it is possible that the same byte appears more than once in the
 *  #Byte_Value_Array.
 *
 *  @param bva     #Byte_Value_Array to test
 *  @param bs256   #Bit_Set_256 to test
 *  @return        true/false
 */
bool bva_bs256_same_values( Byte_Value_Array bva , Bit_Set_256 bbflags) {
   bool result = true;
   int item;
   for (item = 0; item < 256; item++) {
      // printf("item=%d\n", item);
      bool r1 = bva_contains(bva, item);
      bool r2 = bs256_contains(bbflags, item);
      if (r1 != r2) {
         result = false;
         break;
      }
   }
   return result;
}








/** Convert a #Byte_Value_Array to a #Bit_Set_256
 *
 *  @param  bva  Byte_Value_Array
 *  @return Bit_Set_256
 */
Bit_Set_256 bva_to_bs256(Byte_Value_Array bva) {
   Bit_Set_256 bitset = EMPTY_BIT_SET_256;

   for (int ndx = 0; ndx < bva_length(bva); ndx++) {
      Byte b = bva_get(bva, ndx);
      bitset = bs256_insert(bitset, b);
   }
   return bitset;
}


/** Function matching signature #Byte_Appender that adds a byte
 * to a #Byte_Value_Array.
 *
 * @param data_struct  pointer to #Byte_Value_Array to modify
 * @param val          byte to append
 */
void bva_appender(void * data_struct, Byte val) {
   Byte_Value_Array bva = (Byte_Value_Array) data_struct;
   bva_append(bva, val);
}


/** Function matching signature #Byte_Appender that sets a bit
 *  in a #Bit_Set_256.
 *
 * @param data_struct  pointer to #Bit_Set_256 to modify
 * @param val          number of bit to set
 */
void bs256_appender(void * data_struct, Byte val) {
   assert(data_struct);
   Bit_Set_256 * bitset = (Bit_Set_256*) data_struct;
   *bitset = bs256_insert(*bitset, val);
}


/** Stores a list of bytehex values in either a **Byte_Value_Array**, or a **Bit_Set_256**.
 *
 * @param start starting address of hex values
 * @param len   length of hex values
 * @param data_struct opaque handle to either a **Byte_Value_Array** or a **Bit_Set_256**
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


/** Parses a list of bytehex values and stores the result in a **Bit_Set_256**.
 *
 * @param pbitset   where to return result **Bit_Set_256** instance
 * @param start     starting address of hex values
 * @param len       length of hex values
 *
 * @return false if any input data cannot be parsed, true otherwise
 */
bool bs256_store_bytehex_list(Bit_Set_256 * pbitset, char * start, int len) {
   return store_bytehex_list(start, len, pbitset, bs256_appender);
}


//
// Generic code to register and deregister callback functions.
//

/** Adds function to a set of registered callbacks
 *
 * @param  array of registered callbacks
 * @param  function to add
 * @retval true  success
 *
 * @remark
 * It is not an error if the function is already registered.
 */
bool generic_register_callback(GPtrArray** registered_callbacks_loc, void * func) {
   bool debug = false;
   DBGF(debug, "Starting. registered_callbacks=%p, func=%p", *registered_callbacks_loc, func);

   if (!*registered_callbacks_loc) {
      *registered_callbacks_loc = g_ptr_array_new();
   }

   bool new_registration = true;
   for (int ndx = 0; ndx < (*registered_callbacks_loc)->len; ndx++) {
      if (func == g_ptr_array_index(*registered_callbacks_loc, ndx)) {
         new_registration = false;
         break;
      }
   }
   if (new_registration) {
      g_ptr_array_add(*registered_callbacks_loc, func);
   }

   bool result = true;
   DBGF(debug, "Done.     Returning %s", SBOOL(result));
   return result;
}


/** Unregisters a callback function
 *
 *  @param func function to remove
 *  @retval true  function deregistered
 *  @retval false function not found
 *   */
bool generic_unregister_callback(GPtrArray* registered_callbacks, void *func) {
     bool debug = false;
     DBGF(debug, "Starting. registered_callbacks=%p, func=%p", registered_callbacks, func);
     bool found = false;
     if (registered_callbacks) {
        for (int ndx = 0; ndx < registered_callbacks->len; ndx++) {
           if ( func == g_ptr_array_index(registered_callbacks, ndx)) {
              g_ptr_array_remove_index(registered_callbacks,ndx);
              found = true;
           }
        }
     }
     DBGF(debug, "Done.     Returning: %s", SBOOL(found));
     return found;
}

