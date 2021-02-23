/** \file status_code_mgt.c
 *
 *  Status Code Management
 */
// Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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


// Describes a status code range
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
      {.id = RR_ERRNO,
       .base = RCRANGE_ERRNO_START,
       .max = RCRANGE_ERRNO_MAX,
    // .desc_finder        = NULL,                    // will be filled in by call to ...
    // .finder_arg_is_modulated = false,              //    ... register_retcode_desc_finder()
       .desc_finder = get_negative_errno_info,
       .finder_arg_is_modulated = true,                // finder_arg_is_modulated
       // .number_finder      = errno_name_to_modulated_number,
       .number_finder      = errno_name_to_number,
       .base_number_finder = errno_name_to_number
      },
      {.id                 = RR_ADL,
       .base               = RCRANGE_ADL_START,
       .max                = RCRANGE_ADL_MAX,
    // .desc_finder        = NULL,                    // will be filled in by call to ...
    // .finder_arg_is_modulated = false,              //    ... register_retcode_desc_finder()
#ifdef ADL
       .desc_finder = get_adl_status_description,
       .finder_arg_is_modulated = false,                     // finder_arg_is_modulated
       .number_finder      = adl_error_name_to_modulated_number,   // mock implementation if not HAVE_ADL
       .base_number_finder = adl_error_name_to_number              // mock implementation if not HAVE_ADL
#endif
      },
      {.id                 = RR_DDC,
       .base               = RCRANGE_DDC_START,
       .max                = RCRANGE_DDC_MAX,
       .desc_finder        = ddcrc_find_status_code_info,
       .finder_arg_is_modulated = true,
       // .number_finder      = ddc_error_name_to_modulated_number,
       .number_finder      = ddc_error_name_to_number,
       .base_number_finder = ddc_error_name_to_number
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

#ifdef OLD
/** This function is called by modules for specific status code ranges
 *  to register the explanation routines for their
 *  status codes.  It exists to avoid circular dependencies of includes.
 *
 *  @param  id           status code range id
 *  @param  finder_func  function to return #Status_Code_Info struct for a status code,
 *                       of type #Retcode_Description_Finder
 *  @param  finder_arg_is_modulated  if true, **finder_func** takes a modulated
 *                                   status code as an argument.  If false, it
 *                                   takes an unmodualted value.
 *
 * @remark
 * This function will be executed from module initialization functions, which are called
 * from main before the command line is parsed, so trace control not yet configured
 */
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
#endif

/** Shifts a status code in the RR_BASE range to a specified range.
 *
 * @param  rc        base status code to modulate
 * @param  range_id  range to which status code should be modulated
 *
 * @return  modulated status code
 *
 * @remark
 * It is an error to pass an already modulated status code as an argument to
 * this function.
 */
int modulate_rc(int rc, Retcode_Range_Id range_id){
   bool debug = false;
   if (debug)
      printf("(%s) rc=%d, range_id=%d\n", __func__, rc, range_id);
   assert(range_id == RR_ADL);
   // assert( abs(rc) <= RCRANGE_BASE_MAX );
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


/** Shifts a status code from the specified modulation range to the base range
 *
 *  @param  rc        status code to demodulate
 *  @param  range_id  a modulation range
 *
 * @return  demodulated status code
 *
 * @remark
 * It is an error to pass an unmodulated status code as an
 * argument to this function.
 */
int demodulate_rc(int rc, Retcode_Range_Id range_id) {
   // TODO: check that rc is in the specified modulation range
   // assert( abs(rc) > RCRANGE_BASE_MAX );
   assert(range_id == RR_ADL);
   int base = retcode_range_table[range_id].base;
   if (rc != 0) {
      if (rc < 0)
         rc = rc + base;    // rc =   -((-rc)-base);
      else
         rc = rc-base;
   }
   return rc;
}


/** Determines the modulation range for a status code.
 *
 * @param  rc     status code to check
 *
 * @return range identifier (#Retcode_Range_Id)
 */
Retcode_Range_Id get_modulation(Public_Status_Code rc) {
   int ndx = 0;
   int abs_rc = abs(rc);
   Retcode_Range_Id range_id = RR_ERRNO;  // assignment to avoid compiler warning
   for (;ndx < retcode_range_ct; ndx++) {
      if (abs_rc >= retcode_range_table[ndx].base && abs_rc <= retcode_range_table[ndx].max) {
         range_id = retcode_range_table[ndx].id;
         assert (range_id == ndx);
         break;
      }
   }
   assert(ndx < retcode_range_ct);    // fails if not found

   // printf("(%s) rc=%d, returning %d\n", __func__, rc, range_id);
   return range_id;
}


static Status_Code_Info ok_status_code_info = {0, "OK", "success"};

// N.B. Works equally well whether argument is a Global_Status_Code or a
// Public_Status_Code.  get_modulaetion() figures things out


/** Given a #Public_Status_Code, returns a pointer to the #Status_Code_Info struct.
 *  describing it.
 *
 * @param   status_code global (modulated) status code
 * @return  pointer to #Status_Code_Info for staus code, NULL if not found
 */
Status_Code_Info * find_status_code_info(Public_Status_Code status_code) {
   bool debug = false && status_code;
   // use printf() instead of DBGMSG to avoid circular includes
   if (debug)
      printf("(%s) Starting.  rc = %d\n", __func__, status_code);

   Status_Code_Info * pinfo = NULL;

   if (status_code == 0)
      pinfo = &ok_status_code_info;
   else {
      Retcode_Range_Id modulation = get_modulation(status_code);
      if (debug)
         printf("(%s) modulation=%d\n", __func__, modulation);

      Retcode_Description_Finder finder_func = retcode_range_table[modulation].desc_finder;
      assert(finder_func != NULL);
      bool finder_arg_is_modulated = retcode_range_table[modulation].finder_arg_is_modulated;
      int rawrc = (finder_arg_is_modulated) ? status_code : demodulate_rc(status_code, modulation);
      if (debug)
         printf("(%s) rawrc = %d\n", __func__, rawrc);
      pinfo = finder_func(rawrc);
   }
   if (debug) {
      printf("(%s) Done.  Returning %p\n", __func__, pinfo);
      if (pinfo)
         report_status_code_info(pinfo);
   }

   return pinfo;
}


#define GSC_WORKBUF_SIZE 300

/** Returns a description string for a #Public_Status_Code.
 *  Synthesizes a description if information for the status code cannot be found.
 *
 *  @param  psc  status code number
 *  @return string description of status code
 *
 *  @remark
 *  The value returned is valid until the next call of this function in the
 *  same thread. Caller should not free.
 */
char * psc_desc(Public_Status_Code psc) {
   static GPrivate  psc_desc_key = G_PRIVATE_INIT(g_free);
   char * workbuf = get_thread_fixed_buffer(&psc_desc_key, GSC_WORKBUF_SIZE);
   // printf("(%s) workbuf=%p\n", __func__, workbuf);
   // static char workbuf[GSC_WORKBUF_SIZE];
   // printf("(%s) status_code=%d\n", __func__, status_code);
   Status_Code_Info * pinfo = find_status_code_info(psc);
   if (pinfo) {
      snprintf(workbuf, GSC_WORKBUF_SIZE, "%s(%d): %s",
               pinfo->name, psc, pinfo->description);
   }
   else {
      snprintf(workbuf, GSC_WORKBUF_SIZE, "%d",
               psc );
   }
   return workbuf;
}

#undef GSC_WORKBUF_SIZE


/** Returns the symbolic name of a #Public_Status_Code
 *
 * @param status_code status code value
 * @return symbolic name, or "" if not found
 */

char * psc_name(Public_Status_Code status_code) {
   Status_Code_Info * pdesc = find_status_code_info(status_code);
   char * result = (pdesc) ? pdesc->name : "";
   return result;
}


/** Given a status code name, convert it to a unmodulated base status code
 *  Valid only for those status code ranges which ...
 *
 *
 * @param  status_code_name
 * @param p_error_number  where to return status code number
 * @return true if conversion successful, false if unrecognized status code
 */
bool status_name_to_unmodulated_number(
        const char * status_code_name,
        int *        p_error_number)
{
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


/** Given a status code symbolic name, convert it to #Public_Status_Code value.
 *
 * If the name is for an ADL status code, the value is modulated.
 *
 * @param  status_code_name
 * @param  p_error_number  where to return status code number
 * @return true if conversion successful, false if unrecognized status code
 */
bool
status_name_to_modulated_number(
      const char *          status_code_name,
      Public_Status_Code *  p_error_number) {
   Public_Status_Code psc = 0;
   bool found = false;

   for (int ndx = 1; ndx < retcode_range_ct; ndx++) {
      // printf("ndx=%d, id=%d, base=%d\n", ndx, retcode_range_table[ndx].id, retcode_range_table[ndx].base);
      if (retcode_range_table[ndx].number_finder) {
         found = retcode_range_table[ndx].number_finder(status_code_name, &psc);
         if (found)
            break;
      }
   }

   *p_error_number = psc;
   return found;
}


//
// Initialization and debugging
//

/** Initialize this module.
 */
void init_status_code_mgt() {
   // N.B called before command line parsed, so command line trace control not in effect
   // printf("(%s) Starting\n", __func__);
   validate_retcode_range_table();                         // uses asserts to check consistency
   // error_counts_hash = g_hash_table_new(NULL,NULL);

   // initialize_ddcrc_desc();
}


/** Display the contents of a #Status_Code_Info struct.
 *  This is a debugging function.
 *
 * @param pdesc  pointer to #Status_Code_Info struct
 */
void report_status_code_info(Status_Code_Info * pdesc) {
   printf("Status_Code_Info struct at %p\n", pdesc);
   if (pdesc) {
      printf("code:                 %d\n",     pdesc->code);
      printf("name:                 %p: %s\n", pdesc->name, pdesc->name);
      printf("description:          %p: %s\n", pdesc->description, pdesc->description);
      // printf("memoized_description: %p: %s\n", pdesc->memoized_description, pdesc->memoized_description);
   }
}
