/*  debug_util.h
 *
 *  Created on: Nov 5, 2015
 *      Author: rock
 *
 *  Generic debugging utilities
 */

#ifndef UTIL_DEBUG_UTIL_H_
#define UTIL_DEBUG_UTIL_H_

#include <stdlib.h>

//
// Traceable memory management
//

void * call_malloc(size_t size, char * loc);
void * call_calloc(size_t nelem, size_t elsize, char * loc);
void   call_free(void * ptr, char * loc);

#endif /* UTIL_DEBUG_UTIL_H_ */
