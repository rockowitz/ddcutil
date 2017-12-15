/* output_sink.h
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

/** @file output_sink.h
 *  Alternative mechanism for output redirecton.
 *  Not currently used (3/2017)
 */

#ifndef UTIL_OUTPUT_SINK_H_
#define UTIL_OUTPUT_SINK_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdio.h>
/** \endcond */

/** Type of output sink */
typedef enum {SINK_STDOUT, SINK_FILE, SINK_MEMORY} Output_Sink_Type;

/** Opaque handle to output sink instance */
typedef void * Output_Sink;

Output_Sink create_terminal_sink();

Output_Sink create_file_sink(FILE * fp);

Output_Sink create_memory_sink(int initial_line_ct, int estimated_max_chars);

int printf_sink(Output_Sink sink, const char * format, ...);

GPtrArray *  read_sink(Output_Sink sink);

int close_sink(Output_Sink sink);

#endif /* UTIL_OUTPUT_SINK_H_ */
