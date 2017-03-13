/* status_code_mgt.c
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

/** \file
 * Status Code Management
 */

/** \cond */
#include <assert.h>
#include <base/adl_errors.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
/** \endcond */

#include "util/string_util.h"

#include "base/ddc_errno.h"
#include "base/linux_errno.h"

#include "base/status_code_mgt.h"


/*

Notes on status code management.

Status codes in the ddcutil have multiple sources:

1) Linux system calls.  
- In general, functions return 0 or a positive value to indicate success.
- Values greater than 0 indicate something about a successful call, such as the number of bytes read.
- Negative values indicate that an error occurred. In that case the special variable errno is
  typically the "error code", though some packages set the return code to -errno.
- Errno values, listed in errno.h, are positive numbers ranging from 1 to apparently
  less than 200.

2) ADL
- ADL functions return status codes, listed in file adl_defines.h
- The value of these codes ranges from 4 to -21.  0 indicates normal success.
- Positive values appear to be "ok, but".  Not clear when these values occur or what to do about them.
- Negative values indicate errors, some of which may reflect programming errors.

3) ddcutil  specific status codes.


Problem: Linux and ADL error numbers conflict.
ddcutil error numbers can be assigned to a range out of conflict.

Solution.

Mulitplexing.

 */




typedef struct {
   Retcode_Range_Id            id;
   int                         base;
   int                         max;
   Retcode_Description_Finder  desc_finder;
   bool                        finder_arg_is_modulated;
   Retcode_Number_Finder       number_finder;
   Retcode_Number_Finder       base_number_finder;
} Retcode_Range_Table_Entry;


// order must be kept consistent with Retcode_Range_Id
// For explainers in files that are included by this file, the explainer
// can be filled in statically.  For other files, register_retcode_desc_finder()
// is called by the initializer function in those files.
Retcode_Range_Table_Entry retcode_range_table[] = {
      {RR_BASE,
       RCRANGE_BASE_START,   RCRANGE_BASE_MAX,
       NULL,                        false,
       NULL,
       NULL
      },     // should this be entry in table?
      {RR_ERRNO,
       RCRANGE_ERRNO_START,  RCRANGE_ERRNO_MAX,
       NULL,                        false,
       errno_name_to_modulated_number,
       errno_name_to_number
      },
      {RR_ADL,
       RCRANGE_ADL_START,    RCRANGE_ADL_MAX,
       NULL,                        false,
// #ifdef HAVE_ADL
       adl_errno_name_to_modulated_number,   // get mock implementation if not HAVE_ADL
       adl_error_name_to_number
// #else
//        NULL,
//        NULL
// #endif
      },
      {RR_DDC,
       RCRANGE_DDC_START,
       RCRANGE_DDC_MAX,
       ddcrc_find_status_code_info, true,
       ddc_error_name_to_modulated_number,
       ddc_error_name_to_number
      },
};
int retcode_range_ct = sizeof(retcode_range_table)/sizeof(Retcode_Range_Table_Entry);


static
void validate_retcode_range_table() {
   int ndx = 0;
   for (;ndx < retcode_range_ct; ndx++) {
      // printf("ndx=%d, id=%d, base=%d\n", ndx, retcode_range_table[ndx].id, retcode_range_table[ndx].base);
      assert( retcode_range_table[ndx].id == ndx);
   }
}


// n. this is called from source file initialization functions, which are called
// from main before the command line is parsed, so trace control not yet configured
void register_retcode_desc_finder(
        Retcode_Range_Id           id,
        Retcode_Description_Finder finder_func,
        bool                       finder_arg_is_modulated)
{
   bool debug = false;
   if (debug)
      printf("(%s) registering callback description finder for range id %d, finder_func=%p, finder_arg_is_modulated=%s\n",
            __func__, id, finder_func, bool_repr(finder_arg_is_modulated));
   retcode_range_table[id].desc_finder = finder_func;
   retcode_range_table[id].finder_arg_is_modulated = finder_arg_is_modulated;
}


/* Shifts a status code in the RR_BASE range to a specified range.
 *
 * Arguments:
 *   rc        base status code to modulate
 *   range_id  range to which status code should be modulated
 *
 * Returns:
 *   modulated status code
 */
int modulate_rc(int rc, Retcode_Range_Id range_id){
   bool debug = false;
   if (debug)
      printf("(%s) rc=%d, range_id=%d\n", __func__, rc, range_id);
   assert( abs(rc) <= RCRANGE_BASE_MAX );
   int base = retcode_range_table[range_id].base;
   if (rc != 0) {
      if (rc < 0)
         rc -= base;
      else
         rc += base;
   }
   if (debug)
      printf("(%s) Returning: %d\n", __func__, rc);
   return rc;
}


/* Shifts a status code from the specified modulation range to the base range
 *
 * Arguments:
 *    rc        status code to demodulate
 *    range_id  a modulation range
 *
 * Returns:
 *   demodulated status code
 */
int demodulate_rc(int rc, Retcode_Range_Id range_id) {
   // TODO: check that rc is in the specified modulation range
   assert( abs(rc) > RCRANGE_BASE_MAX );
   int base = retcode_range_table[range_id].base;
   if (rc != 0) {
      if (rc < 0)
         rc = rc + base;    // rc =   -((-rc)-base);
      else
         rc = rc-base;
   }
   return rc;
}

/* Determines the modulation range for a status code.
 * Can be either the base range or a modulation range
 *
 * Arguments:
 *   rc     status code to check
 *
 * Returns:
 *   range identifier
 */
Retcode_Range_Id get_modulation(int rc) {
   int ndx = 0;
   int abs_rc = abs(rc);
   Retcode_Range_Id range_id;
   for (;ndx < retcode_range_ct; ndx++) {
      if (abs_rc >= retcode_range_table[ndx].base && abs_rc <= retcode_range_table[ndx].max) {
         range_id = retcode_range_table[ndx].id;
         assert (range_id == ndx);
         break;
      }
   }
   assert(ndx < retcode_range_ct);    // fails if not found
   return range_id;
}


Global_Status_Code modulate_base_errno_ddc_to_global(Base_Status_Errno_DDC rc) {
   Global_Status_Code gsc =
         (get_modulation(rc) == RR_BASE)
             ? modulate_rc(rc, RR_ERRNO)
             : rc;
   return gsc;
}

Public_Status_Code global_to_public_status_code(Global_Status_Code gsc){
   Public_Status_Code psc =
         (get_modulation(gsc) == RR_ERRNO)
            ? demodulate_rc(gsc, RR_ERRNO)
            : gsc;
   return psc;
}

Global_Status_Code public_to_global_status_code(Public_Status_Code psc) {
   Global_Status_Code gsc =
         (get_modulation(psc) == RR_BASE)
             ? modulate_rc(psc, RR_ERRNO)
             : psc;
   return gsc;
}




static Status_Code_Info ok_status_code_info = {0, "OK", "success"};


Status_Code_Info * find_global_status_code_info(Global_Status_Code rc) {
   bool debug = false;
   // use don't use DBGMSG to avoid circular includes
   if (debug)
      printf("(%s) Starting.  rc = %d\n", __func__, rc);

   Status_Code_Info * pinfo = NULL;

   if (rc == 0)
      pinfo = &ok_status_code_info;
   else {
      Retcode_Range_Id modulation = get_modulation(rc);
      if (debug)
         printf("(%s) modulation=%d\n", __func__, modulation);

      Retcode_Description_Finder finder_func = retcode_range_table[modulation].desc_finder;
      assert(finder_func != NULL);
      bool finder_arg_is_modulated = retcode_range_table[modulation].finder_arg_is_modulated;
      int rawrc = (finder_arg_is_modulated) ? rc : demodulate_rc(rc, modulation);
      if (debug)
         printf("(%s) rawrc = %d\n", __func__, rawrc);
      pinfo = finder_func(rawrc);
   }
   if (debug) {
      printf("(%s) Done.  Returning %p", __func__, pinfo);
      if (pinfo)
         report_status_code_info(pinfo);
   }

   return pinfo;
}

#define WORKBUF_SIZE 300
static char workbuf[WORKBUF_SIZE];

// Returns status code description:
char * gsc_desc(Global_Status_Code status_code) {
   // printf("(%s) status_code=%d\n", __func__, status_code);
   Status_Code_Info * pdesc = find_global_status_code_info(status_code);
   if (pdesc) {
      snprintf(workbuf, WORKBUF_SIZE, "%s(%d): %s",
               pdesc->name, status_code, pdesc->description);
   }
   else {
      snprintf(workbuf, WORKBUF_SIZE, "%d",
               status_code );
   }
   return workbuf;
}

#undef WORKBUF_SIZE

char * gsc_name(Global_Status_Code status_code) {
   Status_Code_Info * pdesc = find_global_status_code_info(status_code);
   char * result = (pdesc) ? pdesc->name : "";
   return result;
}



bool gsc_name_to_unmodulated_number(const char * status_code_name, int * p_error_number) {
   int  status_code = 0;
   bool found = false;

   for (int ndx = 1; ndx < retcode_range_ct; ndx++) {
      // printf("ndx=%d, id=%d, base=%d\n", ndx, retcode_range_table[ndx].id, retcode_range_table[ndx].base);
      if (retcode_range_table[ndx].base_number_finder) {
         found = retcode_range_table[ndx].base_number_finder(status_code_name, &status_code);
         if (found)
            break;
      }
   }

   *p_error_number = status_code;
   return found;
}


bool gsc_name_to_modulated_number(const char * status_code_name, Global_Status_Code * p_error_number) {
   Global_Status_Code gsc = 0;
   bool found = false;

   for (int ndx = 1; ndx < retcode_range_ct; ndx++) {
      // printf("ndx=%d, id=%d, base=%d\n", ndx, retcode_range_table[ndx].id, retcode_range_table[ndx].base);
      if (retcode_range_table[ndx].number_finder) {
         found = retcode_range_table[ndx].number_finder(status_code_name, &gsc);
         if (found)
            break;
      }
   }

   *p_error_number = gsc;
   return found;
}




//
// Initialization and debugging
//

// N.B called before command line parsed, so command line trace control not in effect
void init_status_code_mgt() {
   // printf("(%s) Starting\n", __func__);
   validate_retcode_range_table();                         // uses asserts to check consistency
   // error_counts_hash = g_hash_table_new(NULL,NULL);

   // initialize_ddcrc_desc();
}


// Debugging function for Status_Code_Info structure
void report_status_code_info(Status_Code_Info * pdesc) {
   printf("Status_Code_Info struct at %p\n", pdesc);
   if (pdesc) {
      printf("code:                 %d\n",     pdesc->code);
      printf("name:                 %p: %s\n", pdesc->name, pdesc->name);
      printf("description:          %p: %s\n", pdesc->description, pdesc->description);
      // printf("memoized_description: %p: %s\n", pdesc->memoized_description, pdesc->memoized_description);
   }
}
