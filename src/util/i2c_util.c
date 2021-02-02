/** \file i2c_util.c
 *
 * I2C utility functions
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#ifdef TARGET_BSD
#include "bsd/i2c-dev.h"
#include "bsd/i2c.h"
#else
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#endif

#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "data_structures.h"
#include "report_util.h"
#include "string_util.h"

#include "i2c_util.h"


/** Converts a string of the form "i2c-N" to a number.
 *
 *  \param  name  string to convert
 *  \return extracted number, -1 if conversion fails
 */
int i2c_name_to_busno(const char * name) {
   int result = -1;
   if (name && str_starts_with(name, I2C"-")) {
      int ival;
      if ( str_to_int(name+4, &ival, 10) )
         result = ival;
   }
   return result;
}


/** Compare strings i2c-X by bus number, handling unusual case where "X" is
 *  a string other than a number.  Non-numeric values sort before numeric
 *  values.
 *
 *  This is a qsort type comparison function.  The arguments are pointers
 *  to pointers to strings, not pointers to strings.
 *
 *  Arguments are of type gconstpointer so that the function signature
 *  matches GCompareFunc.
 *
 *  \param   v1   pointer to pointer to first string to compare
 *  \param   v2   pointer to pointer to second string to compare
 *  \return  -1  if v1 sorts before v2,
 *            0  v1 equals v2
 *            1  v1 sorts after v2
 */
gint i2c_compare(gconstpointer v1, gconstpointer v2) {
   bool debug = false;

   int result = 0;
   char * s1 = (v1) ? *(char**)v1 : NULL;
   char * s2 = (v2) ? *(char**)v2 : NULL;
   if (debug)
      printf("(%s) s1=%p->%s, s2=%p->%s\n", __func__, s1, s1, s2, s2);

   // do something "reasonable" for pathological cases
   if (!s1 && s2)
      result = -1;
   else if (!s1 && !s2)
      result = 0;
   else if (s1 && !s2)
      result = 1;

   else {      // normal case
      int i1 = i2c_name_to_busno(s1);
      int i2 = i2c_name_to_busno(s2);
      if (i1 < 0 && i2 < 0)
         result = strcmp(s1, s2);
      else if (i1 < i2)
         result = -1;
      else if (i1 == i2)
         result = 0;
      else
         result = 1;
   }

   if (debug)
      printf("(%s) Returning: %d\n", __func__, result);
   return result;
}


//
// Functionality flags
//

// Table for interpreting functionality flags.

Value_Name_Table functionality_flag_table = {
      VN(I2C_FUNC_I2C                    ),   //0x00000001
      VN(I2C_FUNC_10BIT_ADDR             ),   //0x00000002
      VN(I2C_FUNC_PROTOCOL_MANGLING      ),   //0x00000004 /* I2C_M_IGNORE_NAK etc. */
      VN(I2C_FUNC_SMBUS_PEC              ),   //0x00000008
   // VN(I2C_FUNC_NOSTART                ),   //0x00000010 /* I2C_M_NOSTART */  // i2c-tools 4.0
   // VN(I2C_FUNC_SLAVE                  ),   //0x00000020                      // i2c-tools 4.0
      {0x00000010, "I2C_FUNC_NOSTART", NULL  },
      {0x00000020, "I2C_FUNC_SLAVE",   NULL    },
      VN(I2C_FUNC_SMBUS_BLOCK_PROC_CALL  ),   //0x00008000 /* SMBus 2.0 */
      VN(I2C_FUNC_SMBUS_QUICK            ),   //0x00010000
      VN(I2C_FUNC_SMBUS_READ_BYTE        ),   //0x00020000
      VN(I2C_FUNC_SMBUS_WRITE_BYTE       ),   //0x00040000
      VN(I2C_FUNC_SMBUS_READ_BYTE_DATA   ),   //0x00080000
      VN(I2C_FUNC_SMBUS_WRITE_BYTE_DATA  ),   //0x00100000
      VN(I2C_FUNC_SMBUS_READ_WORD_DATA   ),   //0x00200000
      VN(I2C_FUNC_SMBUS_WRITE_WORD_DATA  ),   //0x00400000
      VN(I2C_FUNC_SMBUS_PROC_CALL        ),   //0x00800000
      VN(I2C_FUNC_SMBUS_READ_BLOCK_DATA  ),   //0x01000000
      VN(I2C_FUNC_SMBUS_WRITE_BLOCK_DATA ),   //0x02000000
      VN(I2C_FUNC_SMBUS_READ_I2C_BLOCK   ),   //0x04000000 /* I2C-like block xfer  */
      VN(I2C_FUNC_SMBUS_WRITE_I2C_BLOCK  ),   //0x08000000 /* w/ 1-byte reg. addr. */
   // VN(I2C_FUNC_SMBUS_HOST_NOTIFY      ),   //0x10000000               // i2c-tools 4.0
      {0x10000000, "I2C_FUNC_SMBUS_HOST_NOTIFY", NULL},
      VN_END
};


/** Gets the I2C functionality flags for an open I2C bus,
 *  specified by its file descriptor.
 *
 *  @param fd  Linux file descriptor
 *  @return functionality flags
 */
unsigned long i2c_get_functionality_flags_by_fd(int fd) {
   unsigned long funcs;
   int rc = ioctl(fd, I2C_FUNCS, &funcs);
   if (rc < 0) {
      // should be impossible
      fprintf(stderr, "(%s) Error in ioctl(I2C_FUNCS), errno=%d\n", __func__, errno);
      funcs = 0;
   }
   // printf("(%s) Functionality for file descriptor %d: %lu, 0x%0lx\n", __func__, fd, funcs, funcs);
   return funcs;
}


/** Returns a string representation of functionality flags.
 *
 * @param functionality  long int of flags
 * @return string representation of flags
 */
char * i2c_interpret_functionality_flags(unsigned long functionality) {
   return vnt_interpret_flags(functionality, functionality_flag_table, false, ", ");
}


/** Reports functionality flags.
 *
 *  The output is multiline.
 *
 *  @param  functionality  flags to report
 *  @param  maxline        maximum length of 1 line
 *  @param  depth          logical indentation depth
 */
void i2c_report_functionality_flags(long functionality, int maxline, int depth) {
   char * buf0 = i2c_interpret_functionality_flags(functionality);

   char * header = "Functionality: ";
   int hdrlen = strlen(header);
   int maxpiece = maxline - ( rpt_get_indent(depth) + hdrlen);

   Null_Terminated_String_Array ntsa = strsplit_maxlength( buf0, maxpiece, " ");
   int ntsa_ndx = 0;
   while (true) {
      char * s = ntsa[ntsa_ndx++];
      if (!s)
         break;
      rpt_vstring(depth, "%-*s%s", hdrlen, header, s);
      if (strlen(header) > 0)
         header = "";
   }
   free(buf0);
   ntsa_free(ntsa, /* free_strings */ true);
}

