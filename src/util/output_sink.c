/* output_sink.c
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

/** @file output_sink.c
 *  Alternative mechanism for output redirecton.
 *  Not currently used (3/2017)
 */

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "output_sink.h"


#define OUTPUT_SINK_MARKER "SINK"
struct Output_Sink{
   char              marker[4];
   Output_Sink_Type  sink_type;
   FILE *            fp;
   GPtrArray*        line_array;
   int               cur_max_chars;
   char *            workbuf;
};


/** Creates an output sink representing stdout.
 *
 * @return output sink handle
 */
Output_Sink create_terminal_sink() {
    struct Output_Sink * psink = calloc(1, sizeof(struct Output_Sink));
    memcpy(psink->marker, OUTPUT_SINK_MARKER, 4);
    psink->sink_type = SINK_STDOUT;
    psink->fp = stdout;            // ??
    return psink;
}


/** Creates an output sink representing a file.
 *
 * @param fp pointer to open file stream
 * @return output sink handle
 */
Output_Sink create_file_sink(FILE * fp) {
   struct Output_Sink * psink = calloc(1, sizeof(struct Output_Sink));
   memcpy(psink->marker, OUTPUT_SINK_MARKER, 4);
   psink->sink_type = SINK_FILE;
   psink->fp        = fp;
   return psink;
}


/** Creates an in-memory output sink.
 *
 * @param initial_line_ct      initially allocate space for this many lines
 * @param estimated_max_chars  estimated maximum line length
 * @return output sink handle
 */
Output_Sink create_memory_sink(int initial_line_ct, int estimated_max_chars) {
   struct Output_Sink * psink = calloc(1, sizeof(struct Output_Sink));
   memcpy(psink->marker, OUTPUT_SINK_MARKER, 4);
   psink->sink_type = SINK_MEMORY;
   psink->line_array = g_ptr_array_sized_new(initial_line_ct);
   g_ptr_array_set_free_func(psink->line_array, free);
   psink->cur_max_chars = estimated_max_chars;
   psink->workbuf = calloc(estimated_max_chars+1, sizeof(char));
   return psink;
}


/** Writes to an output sink in a printf() like manner.
 *
 * @param sink   Output_Sink handle
 * @param format printf() format string
 * @param ...    printf() arguments
 */
int printf_sink(Output_Sink sink, const char * format, ...) {
   struct Output_Sink * psink = (struct Output_Sink*) sink;
   assert(psink && memcmp(psink->marker, OUTPUT_SINK_MARKER, 4) == 0);
   int rc = 0;
   va_list(args);
   va_start(args, format);
   switch(psink->sink_type) {
   case (SINK_STDOUT):
         // rc = vprintf(format, args);
         // break;
   case (SINK_FILE):
         rc = vfprintf(psink->fp, format, args);
         if (rc < 0)
            rc = -errno;
         break;
   case (SINK_MEMORY):
      {
         bool done = false;
         while (!done) {
            rc = vsnprintf(psink->workbuf, psink->cur_max_chars, format, args);
            if (rc > psink->cur_max_chars) {
               // if work buffer was too small, reallocate and retry
               free(psink->workbuf);
               psink->cur_max_chars = rc + 1;
               psink->workbuf = calloc( (psink->cur_max_chars)+1, sizeof(char));
            }
            else
               done = true;
         };
         if (rc < 0)
            rc = -errno;
         else
            g_ptr_array_add(psink->line_array, strdup(psink->workbuf));
         break;
      }
   }
   va_end(args);
   return rc;
}


/** Reads the current contents of an in-memory Output_Sink.
 *
 * @param sink Output_Sink handle
 * @return pointer to GPtrArray of strings
 *
 * @remark
 * Note thsi returns a pointer into the Output_Sink data structure.
 */
GPtrArray *  read_sink(Output_Sink sink) {
   struct Output_Sink * psink = (struct Output_Sink *) sink;
   assert(psink && memcmp(psink->marker, OUTPUT_SINK_MARKER, 4) == 0);
   assert(psink->sink_type == SINK_MEMORY);
   return psink->line_array;
}


/** Closes an Output_Sink.
 *
 * If a file output sink, the underlying file is closed.
 *
 * If an in-memory output sink, all memory associated with the
 * sink is freed.  (UNIMPLEMENTED)
 *
 * @param sink handle to Output_Sink
 *
 * @remark
 * - For symmetry, if this function closes a file sink,
 *   create_sink should probably open the file.
 */
int close_sink(Output_Sink sink) {
   struct Output_Sink * psink = (struct Output_Sink *) sink;
   assert(psink && memcmp(psink->marker, OUTPUT_SINK_MARKER, 4) == 0);
   int rc = 0;

   switch(psink->sink_type) {
   case (SINK_STDOUT):
         break;
   case (SINK_FILE):
         rc = fclose(psink->fp);
         if (rc < 0)
            rc = -errno;
         break;
   case (SINK_MEMORY):
         g_ptr_array_free(psink->line_array, true);
         psink->line_array = NULL;
         break;
   }
   psink->marker[3] = 'x';
   free(psink);
   return rc;
}



#ifdef ORIGINAL_IDEA
// What I really need are curried functions.

typedef int (*VCP_Emitter)(const char * format, ...);

static FILE * vcp_file_emitter_fp = NULL;

int vcp_file_emitter(const char * format, ...) {
   assert(vcp_file_emitter_fp);
   va_list(args);
   va_start(args, format);
   int rc = vfprintf(vcp_file_emitter_fp, format, args);
   va_end(args);
   return rc;
}

static GPtrArray* vcp_garray_emitter_array = NULL;

int vcp_garray_emitter(const char * format, ...) {
   assert(vcp_garray_emitter_array);
   va_list(args);
   va_start(args, format);
   char buf[400];
   vsnprintf(buf, 400, format, args);
   g_ptr_array_add(vcp_garray_emitter_array, strdup(buf));
}
#endif
