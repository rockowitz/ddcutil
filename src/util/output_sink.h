/* output_sink.h
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

#ifndef SRC_UTIL_OUTPUT_SINK_H_
#define SRC_UTIL_OUTPUT_SINK_H_


#include <glib.h>
#include <stdio.h>



typedef enum {SINK_STDOUT, SINK_FILE, SINK_MEMORY} Output_Sink_Type;



#define OUTPUT_SINK_MARKER "SINK"
typedef struct {
   char              marker[4];
   Output_Sink_Type  sink_type;
   FILE *            fp;
   GPtrArray*        line_array;
   int               max_line_size;
   char *            workbuf;

} Output_Sink;


int write_sink(Output_Sink * sink, const char * format, ...);


#endif /* SRC_UTIL_OUTPUT_SINK_H_ */
