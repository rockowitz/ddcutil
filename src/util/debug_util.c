/*  debug_util.c
 *
 *  Created on: Nov 5, 2015
 *      Author: rock
 *
 *  Generic debugging utilities
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <util/debug_util.h>


//
// Traceable memory allocation functions
//

// Is this really worth keeping around given what valgrind can do?

// Controls trace messages for call_malloc(), call_calloc(), and call_free()
bool debug_memory_allocation = false;

void * call_malloc(size_t size, char * loc) {
   void * result = malloc(size);
   if (debug_memory_allocation) {
      printf("(%s) loc=%s, size=%d, returning %p\n", __func__, loc, (int) size, result );
      // hex_dump(result-16,32);
      // hex_dump(result+size-16,32);
   }
   return result;
}

void * call_calloc(size_t nelem, size_t elsize, char * loc) {
   void * result = calloc(nelem, elsize);
   if (debug_memory_allocation) {
      printf("(%s) loc=%s, nelem=%d, size=%d, returning %p\n", __func__, loc, (int) nelem, (int) elsize, result );
      // hex_dump(result-16,32);
   }
   return result;

}

void call_free(void * ptr, char * loc) {
   bool force = false;
   if (debug_memory_allocation || force) {
      printf("(%s) loc=%s, ptr=%p\n", __func__, loc, ptr);
      // hex_dump(ptr-16,32);
   }
   free(ptr);
}

