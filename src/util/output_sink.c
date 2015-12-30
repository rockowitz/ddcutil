/* output_sink.c
 *
 * Created on: Dec 28, 2015
 *     Author: rock
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

#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/output_sink.h"



Output_Sink * create_terminal_sink() {
    Output_Sink * sink = calloc(1, sizeof(Output_Sink));
    memcpy(sink->marker, OUTPUT_SINK_MARKER, 4);
    sink->sink_type = SINK_STDOUT;
    sink->fp = stdout;            // ??
    return sink;
}

Output_Sink * create_file_sink(FILE * fp) {
   Output_Sink * sink = calloc(1, sizeof(Output_Sink));
   memcpy(sink->marker, OUTPUT_SINK_MARKER, 4);
   sink->sink_type = SINK_FILE;
   sink->fp        = fp;
   return sink;
}

Output_Sink * create_memory_sink(int initial_line_ct, int max_line_size) {
   Output_Sink * sink = calloc(1, sizeof(Output_Sink));
   memcpy(sink->marker, OUTPUT_SINK_MARKER, 4);
   sink->sink_type = SINK_MEMORY;
   sink->line_array = g_ptr_array_sized_new(initial_line_ct);
   sink->max_line_size = max_line_size;
   sink->workbuf = malloc(max_line_size+1);
   return sink;
}

int write_sink(Output_Sink * sink, const char * format, ...) {
   assert(sink && memcmp(sink->marker, OUTPUT_SINK_MARKER, 4) == 0);
   int rc = 0;
   va_list(args);
   va_start(args, format);
   switch(sink->sink_type) {
   case (SINK_STDOUT):
         // rc = vprintf(format, args);
         // break;
   case (SINK_FILE):
         rc = vfprintf(sink->fp, format, args);
         if (rc < 0)
            rc = -errno;
         break;
   case (SINK_MEMORY):
      {
         rc = vsnprintf(sink->workbuf, sink->max_line_size, format, args);
         if (rc < 0)
            rc = -errno;
         else
            g_ptr_array_add(sink->line_array, strdup(sink->workbuf));
         break;
      }
   }
   return rc;
}

GPtrArray *  read_sink(Output_Sink * sink) {
   assert(sink && memcmp(sink->marker, OUTPUT_SINK_MARKER, 4) == 0);
   assert(sink->sink_type == SINK_MEMORY);
   return sink->line_array;
}

int close_sink(Output_Sink * sink) {
   assert(sink && memcmp(sink->marker, OUTPUT_SINK_MARKER, 4) == 0);
   int rc = 0;

   switch(sink->sink_type) {
   case (SINK_STDOUT):
         break;
   case (SINK_FILE):
         rc = fclose(sink->fp);
         if (rc < 0)
            rc = -errno;
         break;
   case (SINK_MEMORY):
      {
         // destroy sink->line_array
         break;
      }
   }
   free(sink);
   return rc;
}



#ifdef ORIGINAL IDEA
// What I really need are curried functions.

typedef int (*VCP_Emitter)(const char * format, ...);

static FILE * vcp_file_emitter_fp = NULL;

int vcp_file_emitter(const char * format, ...) {
   assert(vcp_file_emitter_fp);
   va_list(args);
   va_start(args, format);
   int rc = vfprintf(vcp_file_emitter_fp, format, args);
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
