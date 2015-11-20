/*
 * status_code_mgt.c
 *
 *  Created on: Nov 3, 2015
 *      Author: rock
 */

#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>

#include <base/ddc_errno.h>
#include <base/linux_errno.h>

#include <base/status_code_mgt.h>



/*

Notes on status code management.

Status codes in the DCC application have multiple sources:

1) Linux system calls.  

In general, status codes returned by functions use positive values or 0 to indicate 
something about a successful call, such as the number of bytes read.  Negative values
indicate that something bad occurred; in that case the special variable errno is the 
"error code".  

Errno values, listed in errno.h, are positive numbers ranging from 1 to apparently 
less than 200. 

2) ADL functions return status codes, listed in file ...
The value of these codes ranges from 4 to -nn. 
0 indicates normal success.  Positive values appear to be "ok, but".  Not clear 
when these values occur or what to do about them.  Negative values indicate errors, 
some of which may reflect programming errors. 

3) DDC specific status codes.    

 Linux system calls.  Generally special variable errno is set if an error occurs.

 ADL

Status codes specific to this application.

Problem:   Linux and ADL error numbers conflict.
DCC error numbers can be assigned to a range out of conflict.

Solution.

Mulitplexing.

 */




typedef struct {
   Retcode_Range_Id            id;
   int                         base;
   int                         max;
   Retcode_Description_Finder  desc_finder;
   bool                        finder_arg_is_modulated;
} Retcode_Range_Table_Entry;


// order must be kept consistent with Retcode_Range_Id
// For explainers in files that are included by this file, the explainer
// can be filled in statically.  For other files, register_retcode_desc_finder()
// is called by the initializer function in those files.
Retcode_Range_Table_Entry retcode_range_table[] = {
      {RR_BASE,   RCRANGE_BASE_START,   RCRANGE_BASE_MAX,  NULL,  false },     // should this be entry in table?
      {RR_ERRNO,  RCRANGE_ERRNO_START,  RCRANGE_ERRNO_MAX, NULL,  false },
      {RR_ADL,    RCRANGE_ADL_START,    RCRANGE_ADL_MAX,   NULL,  false },
      {RR_DDC,    RCRANGE_DDC_START,    RCRANGE_DDC_MAX,   ddcrc_find_status_code_info,  true },
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
        bool                       finder_arg_is_modulated) {
   // printf("(%s) registering callback description finder for range id %d\n", __func__, id);
   retcode_range_table[id].desc_finder = finder_func;
   retcode_range_table[id].finder_arg_is_modulated = finder_arg_is_modulated;
}


int modulate_rc(int rc, Retcode_Range_Id range_id){
   // printf("(%s) rc=%d, range_id=%d\n", __func__, rc, range_id);
   assert( abs(rc) <= RCRANGE_BASE_MAX );
   int base = retcode_range_table[range_id].base;
   if (rc != 0) {
      if (rc < 0)
         rc = -base + rc;
      else
         base = base+rc;
   }
   // printf("(%s) Returning: %d\n",  __func__, rc);
   return rc;
}

int demodulate_rc(int rc, Retcode_Range_Id range_id) {
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
             ? gsc = modulate_rc(rc, RR_ERRNO)
             : rc;
   return gsc;
}


static Status_Code_Info ok_status_code_info = {0, "OK", "success"};


Status_Code_Info * find_global_status_code_description(Global_Status_Code rc) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting.  rc = %d\n", __func__, rc);

   Status_Code_Info * pinfo = NULL;

   if (rc == 0)
      pinfo = &ok_status_code_info;
   else {
   Retcode_Range_Id modulation = get_modulation(rc);

   Retcode_Description_Finder finder_func = retcode_range_table[modulation].desc_finder;
   assert(finder_func != NULL);
   bool finder_arg_is_modulated = retcode_range_table[modulation].finder_arg_is_modulated;
   int rawrc = (finder_arg_is_modulated) ? rc : demodulate_rc(rc, modulation);
   pinfo = finder_func(rawrc);
   }
   if (debug) {
      printf("(%s) Done.  Returning %p\n", __func__, pinfo);
      if (pinfo)
         report_status_code_info(pinfo);
   }

   return pinfo;
}

#define WORKBUF_SIZE 300
static char workbuf[WORKBUF_SIZE];


// Returns status code description:
char * gsc_desc(Global_Status_Code status_code) { // must be freed after use
   Status_Code_Info * pdesc = find_global_status_code_description(status_code);
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

#undef WOKBUF_SIZE


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
