/*  file_util.h
 *
 *  Created on: Dec 6, 2015
 *      Author: rock
 */

#ifndef FILE_UTIL_H_
#define FILE_UTIL_H_

#include <glib.h>
#include <stdbool.h>


int file_getlines(const char * fn, GPtrArray* line_array);

char * read_one_line_file(char * fn, bool verbose);

#endif /* FILE_UTIL_H_ */
