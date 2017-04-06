/* core.c
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

/** @file
 * Core functions and global variables.
 *
 * File core.c provides a collection of inter-dependent services at the core
 * of the **ddcutil** application.
 *
 * These include
 * - message destination redirection
 * - abnormal termination
 * - standard function call options
 * - timestamp generation
 * - message level control
 * - debug and trace messages
 */

//* \cond */
#include <assert.h>
#include <glib.h>
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/file_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/timestamp.h"

#include "base/ddc_errno.h"
#include "base/linux_errno.h"

#include "base/core.h"

//
// Externally visible variables
//

/** @defgroup output_redirection Basic Output Redirection
 */

/** Current stream for normal output.
 *
 * @remark
 * Will be NULL until init_msg_control() is called.
 * Be careful during program initialization.
 *
 * @ingroup output_redirection
 */
FILE * FOUT = NULL;

/** Current stream for error messages.
 *
 * @remark
 * Will be NULL until init_msg_control() is called.
 * Be careful during program initialization.
 *
 * @ingroup output_redirection
 */
FILE * FERR = NULL;

bool dbgtrc_show_time = false;    ///< include elapsed time in debug/trace output

#ifdef OVERKILL
#define FOUT_STACK_SIZE 8

static FILE* fout_stack[FOUT_STACK_SIZE];
static int   fout_stack_pos = -1;
#endif


//
// Global SDTOUT and STDERR redirection, for controlling message output in API
//

/** Initialize **stdout** and **stderr** redirection.
 *
 * Must be called during program initialization.
 *
 * @ingroup output_redirection
 */
void init_msg_control() {
   FOUT = stdout;
   FERR = stderr;
}


// issue: how to specify that output should be discarded vs reset to stdout?
// issue will resetting report dest cause conflicts?
// To reset to STDOUT, use constant stdout in stdio.h  - NO - screws up rpt_util
// problem:

/** Redirect output that would normally go to **stdout**.
 *
 *  @param fout pointer to output stream
 *
 * @ingroup output_redirection
 */
void set_fout(FILE * fout) {
   bool debug = false;
   DBGMSF(debug, "fout = %p", fout);
   FOUT = fout;
   rpt_change_output_dest(fout);
}

/** Redirect output that would normally go to **stdout** back to **stdout**.
 * @ingroup output_redirection
 */
void set_fout_to_default() {
   FOUT = stdout;
   rpt_change_output_dest(stdout);
}

/** Redirect output that would normally go to **stderr**..
 *
 *  @param ferr pointer to output stream
 * @ingroup output_redirection
 */
void set_ferr(FILE * ferr) {
   FERR = ferr;
}

/** Redirect output that would normally go to **stderr** back to **stderr**.
 * @ingroup output_redirection
 */
void set_ferr_to_default() {
   FERR = stderr;
}


#ifdef OVERKILL

// Functions that allow for temporarily changing the output destination.


void push_fout(FILE* new_dest) {
   assert(fout_stack_pos < FOUT_STACK_SIZE-1);
   fout_stack[++fout_stack_pos] = new_dest;
}


void pop_fout() {
   if (fout_stack_pos >= 0)
      fout_stack_pos--;
}


void reset_fout_stack() {
   fout_stack_pos = 0;
}


FILE * cur_fout() {
   // special handling for unpushed case because can't statically initialize
   // output_dest_stack[0] to stdout
   return (fout_stack_pos < 0) ? stdout : fout_stack[fout_stack_pos];
}
#endif


//
// Global error return
//

/** @defgroup abnormal_termination Abnormal Termination
 *
 */

static jmp_buf* global_abort_jmp_buf_ptr = NULL;

// It's an error handler.  Do not dynamically allocate
DDCA_Global_Failure_Information global_failure_information = {0};

/** Registers the longjmp() target with the **ddcutil** library.
 *
 * @param jb  pointer to jmp_buf
 *
 * @ingroup abnormal_termination
 */
void register_jmp_buf(jmp_buf* jb) {
   DBGMSG("setting global_abort_jmp_buf_ptr = %p", jb);
   global_abort_jmp_buf_ptr = jb;
}


/** Primary function for terminating **ddcutil** execution
 * due to an internal error.
 *
 * If a longjump jump buffer has been registered, basic error information
 * is stored in buffer global_failure_information, and longjmp() is called.
 * Otherwise, exit() is called.
 *
 *  @param funcname  function name of error
 *  @param lineno    line number of error
 *  @param fn        file name of error
 *  @param status    status code
 *
 *  @ingroup abnormal_termination
 */
/* coverity [+kill] avoid coverity memory leak warnings */
void ddc_abort(
      const char * funcname,
      const int    lineno,
      const char * fn,
      int          status)
{
   show_backtrace(2);
   DBGMSG("global_abort_jmp_buf_ptr = %p", global_abort_jmp_buf_ptr);
   if (global_abort_jmp_buf_ptr) {

      // save failure information in case it's of use at longjmp() return
      global_failure_information.info_set_fg = true;
      global_failure_information.status = status;
      g_strlcpy(global_failure_information.funcname, funcname, sizeof(global_failure_information.funcname));
      global_failure_information.lineno = lineno;
      g_strlcpy(global_failure_information.fn, fn, sizeof(global_failure_information.fn));

      longjmp(*global_abort_jmp_buf_ptr, status);
   }
   else {
      // no point setting global_failure_information, we're outta here
      f0puts("Terminating execution.\n", FERR);
      exit(EXIT_FAILURE);     // or return status?
   }
}


//
// For interpreting field of named bits
//

#ifdef OLD
static void char_buf_append(char * buffer, int bufsize, char * val_to_append) {
   assert(strlen(buffer) + strlen(val_to_append) < bufsize);
   strcat(buffer, val_to_append);
}

char * bitflags_to_string(
          int            flags_val,
          Bitname_Table  bitname_table,
          char *         sepstr,
          char *         buffer,
          int            bufsize )
{
   assert(buffer && bufsize > 1);
   buffer[0] = '\0';
   bool first = true;
   Bitname_Table_Entry * cur_entry = bitname_table;
   while (cur_entry->bitvalue) {
      // DBGMSG("Comparing flags_val=0x%08x vs cur_entry->bitvalue = 0x%08x", flags_val, cur_entry->bitvalue);

      if (flags_val & cur_entry->bitvalue) {
         if (first)
            first = false;
         else
            char_buf_append(buffer, bufsize, sepstr);
         char_buf_append(buffer, bufsize, cur_entry->bitname);
      }
      cur_entry++;
   }
   if (!flags_val && cur_entry->bitname)
      char_buf_append(buffer, bufsize, cur_entry->bitname);
   // printf("(%s) flags_val = 0x%08x, Returning |%s|\n", __func__, flags_val, buffer );
   return buffer;
}
#endif

//
// Standard call options
//

#ifdef OLD
Bitname_Table callopt_bitname_table = {
      {CALLOPT_ERR_MSG,     "CALLOPT_ERR_MSG"},
      {CALLOPT_ERR_ABORT,   "CALLOPT_ERR_ABORT"},
      {CALLOPT_RDONLY,      "CALLOPT_RDONLY"},
      {CALLOPT_WARN_FINDEX, "CALLOPT_WARN_FINDEX"},
      {CALLOPT_FORCE,       "CALLOPT_FORCE"},
//      {CALLOPT_FORCE_SLAVE, "CALLOPT_FORCE_SLAVE"},
      {CALLOPT_NONE,        "CALLOPT_NONE"},
};
#endif


Value_Name_Table callopt_bitname_table2 = {
      VN(CALLOPT_ERR_MSG),
      VN(CALLOPT_ERR_ABORT),
      VN(CALLOPT_RDONLY),
      VN(CALLOPT_WARN_FINDEX),
      VN(CALLOPT_FORCE),
      VN(CALLOPT_NONE),                // special entry
      VN_END
};


#ifdef OLD
char * interpret_call_options_r_old(Call_Options calloptions, char * buffer, int bufsize) {
   bitflags_to_string(calloptions, callopt_bitname_table, "|", buffer, bufsize);
   return buffer;
}
#endif

#ifdef OLD
/** Interprets a **Call_Options** byte as a printable string, returning the
 *  result in the buffer provided.
 *
 *  @param calloptions  **Call_Options** byte
 *  @param buffer       buffer in which to return result
 *  @param bufsize      buffer size
 *
 *  @return buffer
 *
 *  @remark
 *  If the buffer is insufficiently large, the interpretation is truncated.
 */
char * interpret_call_options_r(Call_Options calloptions, char * buffer, int bufsize) {
   // bitflags_to_string(calloptions, callopt_bitname_table, "|", buffer, bufsize);
   interpret_named_flags(calloptions, callopt_bitname_table2, "|", buffer, bufsize);
   return buffer;
}

char * interpret_call_options_r2(Call_Options callopts) {
   return vnt_interpret_flags(callopts, callopt_bitname_table2, false, "|");
}
#endif

#ifdef OLD
char * interpret_call_options_old(Call_Options calloptions) {
   static char buffer[120];
   char * result = interpret_call_options_r(calloptions, buffer, 100);
   // DBGMSG("calloptions = 0x%02x, returning %s", calloptions, result);
   return result;
}
#endif


/** Interprets a **Call_Options** byte as a printable string.
 *  The returned value is valid until the next call of this function.
 *
 *  @param calloptions  **Call_Options** byte
 *
 *  @return interpreted value
 */
#ifdef OLD
char * interpret_call_options_old(Call_Options calloptions) {
   static char buffer[120];
   char * result = interpret_call_options_r(calloptions, buffer, 100);
   // DBGMSG("calloptions = 0x%02x, returning %s", calloptions, result);
   return result;
}
#endif

char * interpret_call_options(Call_Options calloptions) {
   static char * buffer = NULL;
   if (buffer)
      free(buffer);
   buffer = vnt_interpret_flags(calloptions, callopt_bitname_table2, false, "|");
   return buffer;
}

// Local definitions and functions shared by all message control categories

#define SHOW_REPORTING_TITLE_START 0
#define SHOW_REPORTING_MIN_TITLE_SIZE 28


static void
print_simple_title_value(int    offset_start_to_title,
                         char * title,
                         int    offset_title_start_to_value,
                         char * value)
{
   f0printf(FOUT, "%.*s%-*s%s\n",
            offset_start_to_title,"",
            offset_title_start_to_value, title,
            value);
}


//
// Message level control for normal output
//

/** \defgroup msglevel Message Level Management
 *
 * Functions and variables to manage and query output level settings.
 */

static DDCA_Output_Level output_level;

/** Gets the current output level.
 *
 * @return output level
 *
 * \ingroup msglevel
 */
DDCA_Output_Level get_output_level() {
   return output_level;
}

/** Sets the output level.
 *
 * @param newval output level to set
 *
 *  \ingroup msglevel
 */
void set_output_level(DDCA_Output_Level newval) {
   // printf("(%s) newval=%s  \n", __func__, msgLevelName(newval) );
   output_level = newval;
}

/** Gets the printable name of an output level.
 *
 * @param val  output level
 * @return printable name for output level
 *
 *  \ingroup msglevel
 */
char * output_level_name(DDCA_Output_Level val) {
   char * result = NULL;
   switch (val) {
#ifdef OLD
      case OL_DEFAULT:
         result = "Default";
         break;
      case OL_PROGRAM:
         result = "Program";
         break;
#endif
      case DDCA_OL_TERSE:
         result = "Terse";
         break;
      case DDCA_OL_NORMAL:
         result = "Normal";
         break;
      case DDCA_OL_VERBOSE:
         result = "Verbose";
         break;
      // default unnecessary, case exhauts enum
      // default:
      //    PROGRAM_LOGIC_ERROR("Invalid Output_Level value: %d", val);
   }
   // printf("(%s) val=%d 0x%02x, returning: %s\n", __func__, val, val, result);
   return result;
}


/** Reports the current output level to the current FOUT device.
 *
 *  \ingroup msglevel
 */
void show_output_level() {
   // printf("Output level:           %s\n", output_level_name(output_level));
   print_simple_title_value(SHOW_REPORTING_TITLE_START,
                              "Output level: ",
                              SHOW_REPORTING_MIN_TITLE_SIZE,
                              output_level_name(output_level));
}


//
// Debug trace message control
//

/** defgroup dbgtrace Debug and Trace Messages
 *
 */

#ifdef REFERENCE
typedef Byte Trace_Group;
#define TRC_BASE 0x80
#define TRC_I2C  0x40
#define TRC_ADL  0x20
#define TRC_DDC  0x10
#define TRC_USB  0x08
#define TRC_TOP  0x04
#endif

// same order as flags in TraceGroup
#ifdef OLD
const Byte   trace_group_ids[]   = {TRC_BASE, TRC_I2C, TRC_ADL, TRC_DDC, TRC_USB, TRC_TOP};
const char * trace_group_names[] = {"BASE",   "I2C",   "ADL",   "DDC",   "USB",   "TOP"};
const int    trace_group_ct = sizeof(trace_group_names)/sizeof(char *);
#endif

// new way:

static
Value_Name_Title_Table trace_group_table = {
      VNT(TRC_BASE, "BASE"),
      VNT(TRC_I2C, "I2C"),
#ifdef HAVE_ADL
      VNT(TRC_ADL, "ADL"),
#endif
      VNT(TRC_DDC, "DDC"),
      VNT(TRC_USB, "USB"),
      VNT(TRC_TOP, "TOP"),
      VNT_END
};
const int trace_group_ct = ARRAY_SIZE(trace_group_table)-1;

#ifdef OLD
Trace_Group trace_class_name_to_value_old(char * name) {
   Trace_Group trace_group = 0x00;
   int ndx = 0;
   for (; ndx < trace_group_ct; ndx++) {
      if (strcmp(name, trace_group_names[ndx]) == 0) {
         trace_group = 0x01 << (7-ndx);
      }
   }
   // printf("(%s) name=|%s|, returning 0x%2x\n", __func__, name, traceGroup);

   return trace_group;
}
#endif

/** Given a trace group name, return its identifier.
 *  Case is ignored.
 *
 *  @param name trace group name
 *  @return trace group identifier
 *  @retval  TRC_NEVER unrecognized name
 *
 *  /ingroup dbgtrace
 */
Trace_Group trace_class_name_to_value(char * name) {
#ifdef OLD
   return (Trace_Group) vnt_id_by_title(trace_group_table,
                                        name,
                                        true,      // ignore-case
                                        TRC_NEVER);
#endif
   return (Trace_Group) vnt_find_id(trace_group_table,
                                        name,
                                        true,      // search title field
                                        true,      // ignore-case
                                        TRC_NEVER);
}


static Byte trace_levels = TRC_NEVER;   // 0x00

/** Specify the trace groups to be traced.
 *
 * @param trace_flags bit flags indicating groups to trace
 *
 * @ingroup dbgtrace
 */
void set_trace_levels(Trace_Group trace_flags) {
   bool debug = false;
   DBGMSF(debug, "trace_flags=0x%02x\n", trace_flags);

   trace_levels = trace_flags;
}

/** Checks if a group is being traced
 *
 * @param trace_group group to check
 * @param filename    file from which check is occurring (not currently used)
 *
 * @return **true** if tracing enabled, **false** if not
 *
 * @remark
 * - Multiple trace group bits can be set in **trace_group**.  If any of those
 *   group are currently being traced, the function returns **true**. That is,
 *   a given trace location in the code can be activated by multiple trace groups.
 * - If trace_group == TRC_ALWAYS (0xff), the function returns **true**.
 *   Commonly, if debugging is active in a given location, the trace_group value
 *   being checked can be set to TRC_ALWAYS, so a site can have a single debug/trace
 *   function call.
 *
 * @ingroup dbgtrace
 *
 */
bool is_tracing(Trace_Group trace_group, const char * filename) {
   bool result =  (trace_group == 0xff) || (trace_levels & trace_group); // is trace_group being traced?
   // printf("(%s) traceGroup = %02x, filename=%s, traceLevels=0x%02x, returning %d\n",
   //        __func__, traceGroup, filename, traceLevels, result);
   return result;
}

#ifdef OLD
void show_trace_groups_old() {
   const int bufsz = 200;
   char buf[bufsz];
   buf[0] = '\0';
   int ndx;
   for (ndx=0; ndx< trace_group_ct; ndx++) {
      if ( trace_levels & trace_group_ids[ndx]) {
         // buffer is sufficiently large, but make coverity happy by guarding against buffer overflow:
         if ( (strlen(buf) + 2 + strlen(trace_group_names[ndx]) ) < bufsz) {
            if (strlen(buf) > 0)
               strcat(buf, ", ");
            strcat(buf, trace_group_names[ndx]);
         }
      }
   }
   if (strlen(buf) == 0)
      strcpy(buf,"none");
   // printf("Trace groups active:      %s\n", buf);
   print_simple_title_value(SHOW_REPORTING_TITLE_START,
                              "Trace groups active: ",
                              SHOW_REPORTING_MIN_TITLE_SIZE,
                              buf);
}
#endif

#ifdef OLD
char * get_active_trace_group_names_in_buffer(char * buf, int bufsz) {
        interpret_vnt_flags_by_title(
           trace_levels,
           trace_group_table,
           ", ",
           buf,
           bufsz);
   return buf;
}

char * get_active_trace_group_names() {
   const int bufsz = 200;
   char buf[bufsz];
   return strdup(get_active_trace_group_names_in_buffer(buf, bufsz));

}
#endif

void show_trace_groups() {
   // const int bufsz = 200;
   // char buf[bufsz];
   // get_active_trace_group_names_in_buffer(buf, bufsz);
   char * buf = vnt_interpret_flags(trace_levels, trace_group_table, true /* use title */, ", ");
   // if (strlen(buf) == 0)
   //    strcpy(buf,"none");
   // printf("Trace groups active:      %s\n", buf);
   print_simple_title_value(SHOW_REPORTING_TITLE_START,
                              "Trace groups active: ",
                              SHOW_REPORTING_MIN_TITLE_SIZE,
                              (strlen(buf) == 0) ? "none" : buf);
                              // buf);
   free(buf);
}

//
// Report DDC data errors
//

// global variable - controls display of messages regarding DDC data errors
bool show_recoverable_errors = true;

// Normally wrapped in macro IS_REPORTING_DDC
bool is_reporting_ddc(Trace_Group traceGroup, const char * fn) {
  bool result = (is_tracing(traceGroup,fn) || show_recoverable_errors);
  return result;
}


/* Submits a message regarding a DDC data error for possible output.
 * Whether a message is actually output depends on whether DDC errors are
 * being shown and (currently unimplemented) the trace group for the
 * message.
 *
 * Normally, invocation of this function is wrapped in macro DDCMSG.
 */
void ddcmsg(Trace_Group  traceGroup,
            const char * funcname,
            const int    lineno,
            const char * fn,
            char *       format,
            ...)
{
//  if ( is_reporting_ddc(traceGroup, fn) ) {   // wrong
    if (show_recoverable_errors) {
      char buffer[200];
      va_list(args);
      va_start(args, format);
      vsnprintf(buffer, 200, format, args);
      f0printf(FOUT, "(%s) %s\n", funcname, buffer);
      va_end(args);
   }
}


/* Tells whether DDC data errors are reported
 *
 * Arguments:   none
 * Returns:     nothing
 */
void show_ddcmsg() {
   // printf("Reporting DDC data errors: %s\n", bool_repr(show_recoverable_errors));
   print_simple_title_value(SHOW_REPORTING_TITLE_START,
                              "Reporting DDC data errors: ",
                              SHOW_REPORTING_MIN_TITLE_SIZE,
                              bool_repr(show_recoverable_errors));
}


/* Reports output levels for:
 *   - general output level (terse, verbose, etc)
 *   - DDC data errors
 *   - trace groups
 *
 * Arguments:    none
 * Returns:      nothing
 */
void show_reporting() {
   show_output_level();
   show_ddcmsg();
   show_trace_groups();
   // f0puts("", FOUT);
}


//
// Issue messages of various types
//

// n. used within macro LOADFUNC of adl_intf.c


// NB CANNOT MAP TO dbgtrc - writes to stderr, not stdout


void severemsg(
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...)
{
      char buffer[200];
      char buf2[250];
      va_list(args);
      va_start(args, format);
      vsnprintf(buffer, 200, format, args);
      snprintf(buf2, 250, "(%s) %s\n", funcname, buffer);
      // fputs(buf2, stderr);
      f0puts(buf2, FERR);
      va_end(args);
}



#ifdef OLD
// normally wrapped in one of the DBSMGS macros
void dbgmsg(
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...)
{
   // char buffer[200];
   // char buf2[250];
   static char * buffer = NULL;
   static int    bufsz  = 200;     // initial value
   static char * buf2   = NULL;

   if (!buffer) {      // first call
      buffer = calloc(bufsz,    sizeof(char*));
      buf2   = calloc(bufsz+50, sizeof(char*));
   }
   va_list(args);
   va_start(args, format);
   int ct = vsnprintf(buffer, bufsz, format, args);
   if (ct >= bufsz) {
      // printf("(dbgmsg) Reallocting buffer, new size = %d\n", ct+1);
      // buffer too small, reallocate and try again
      free(buffer);
      free(buf2);
      bufsz = ct+1;
      buffer = calloc(bufsz, sizeof(char*));
      buf2   = calloc(bufsz+50, sizeof(char*));
      va_list(args);
      va_start(args, format);
      ct = vsnprintf(buffer, bufsz, format, args);
      assert(ct < bufsz);
   }
   snprintf(buf2, bufsz+50, "(%s) %s", funcname, buffer);
   puts(buf2);
   va_end(args);
}
#endif


/* Core function for emitting debug or trace messages.
 * Normally wrapped in a DBGMSG or TRCMSG macro to
 * simplify calling.
 *
 * Arguments:
 *    trace_group   trace group of caller, to determine whether to output msg
 *                  0xff to always output
 *    funcname      function name in message
 *    lineno        line number in message
 *    fn            file name
 *    format        format string for message
 *    ...           format arguments
 *
 * Returns:         nothing
 */
void dbgtrc(
        Byte         trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...)
{
   static char * buffer = NULL;
   static int    bufsz  = 200;     // initial value
   static char * buf2   = NULL;

   if (!buffer) {      // first call
      buffer = calloc(bufsz,    sizeof(char));
      buf2   = calloc(bufsz+60, sizeof(char));
   }

   if ( is_tracing(trace_group, fn) ) {
      va_list(args);
      va_start(args, format);
      int ct = vsnprintf(buffer, bufsz, format, args);
      va_end(args);
      if (ct >= bufsz) {   // if buffer too small, reallocate
         // printf("(dbgtrc) Reallocating buffer, new size = %d\n", ct+1);
         // buffer too small, reallocate and try again
         free(buffer);
         free(buf2);
         // va_end(args);

         bufsz = ct+1;
         buffer = calloc(bufsz, sizeof(char));
         buf2   = calloc(bufsz+50, sizeof(char));
         va_list(args);
         va_start(args, format);
         ct = vsnprintf(buffer, bufsz, format, args);
         assert(ct < bufsz);
         va_end(args);
      }

      if (dbgtrc_show_time)
         snprintf(buf2, bufsz+60, "[%s](%s) %s\n", formatted_elapsed_time(), funcname, buffer);
      else
         snprintf(buf2, bufsz+60, "(%s) %s\n", funcname, buffer);
      // puts(buf2);        // automatic terminating null
      f0puts(buf2, FOUT);    // no automatic terminating null
      // va_end(args);
   }
}


//
// Standardized handling of exceptional conditions, including
// error messages and possible program termination.
//

/* Report an IOCTL error and possibly terminate execution.
 *
 * Arguments:
 *    errnum         errno value
 *    funcname       function name of error
 *    lineno         line number of error
 *    filename       file name of error
 *    fatal          if true, terminate execution
 *
 *  Returns:         nothing
 */
void report_ioctl_error(
      int   errnum,
      const char* funcname,   // const to avoid warning msg on references at compile time
      int   lineno,
      char* filename,
      bool fatal) {
   int errsv = errno;
   // fprintf(stderr, "(report_ioctl_error)\n");
   f0printf(FERR, "ioctl error in function %s at line %d in file %s: errno=%s\n",
           funcname, lineno, filename, linux_errno_desc(errnum) );
   // fprintf(stderr, "  %s\n", strerror(errnum));  // linux_errno_desc now calls strerror
   // will contain at least sterror(errnum), possibly more:
   // not worth the linkage issues:
   // fprintf(stderr, "  %s\n", explain_errno_ioctl(errnum, filedes, request, data));
   if (fatal) {
      ddc_abort(funcname, lineno, filename, DDCL_INTERNAL_ERROR);
      // exit(EXIT_FAILURE);
   }
   errno = errsv;
}


#ifdef UNUSED
// variant that can use libexplain, unused
void report_ioctl_error2(
      int   errnum,
      int   fh,
      int   request,
      void* data,
      const char* funcname,   // const to avoid warning msg on references at compile time
      int   lineno,
      char* filename,
      bool fatal)
{
   int errsv = errno;
   // fprintf(stderr, "(report_ioctl_error2)\n");
   report_ioctl_error(errno, funcname, lineno, filename, false /* non-fatal */ );
#ifdef USE_LIBEXPLAIN
   // fprintf(stderr, "(report_ioctl_error2) within USE_LIBEXPLAIN\n");
   fprintf(stderr, "%s\n", explain_ioctl(fh, request, data));
#endif
   if (fatal)
      exit(EXIT_FAILURE);
   errno = errsv;
}
#endif



/** Called when a condition that should be impossible has been detected.
 * Issues messages to **stderr** and terminates execution.
 *
 * This function is normally invoked using macro PROGRAM_LOGIC_ERROR()
 *
 *  @param  funcname    function name
 *  @param  lineno      line number in source file
 *  @param  fn          source file name
 *  @param  format      format string, as in printf()
 *  @param  ...         one or more substitution values for the format string
 *
 * @ingroup output_redirection
 */
void program_logic_error(
      const char * funcname,
      const int    lineno,
      const char * fn,
      char *       format,
      ...)
{
  // assemble the error message
  char buffer[200];
  va_list(args);
  va_start(args, format);
  vsnprintf(buffer, 200, format, args);

  // assemble the location message:
  char buf2[250];
  snprintf(buf2, 250, "Program logic error in function %s at line %d in file %s:\n",
                      funcname, lineno, fn);

  // don't combine into 1 line, might be very long.  just output 2 lines:
  f0puts(buf2,   FERR);
  f0puts(buffer, FERR);
  f0puts("\n",   FERR);

  // fputs("Terminating execution.\n", stderr);
  ddc_abort(funcname, lineno, fn, DDCL_INTERNAL_ERROR);
  // exit(EXIT_FAILURE);
}


/** This function is called to terminate execution on a fatal error.
 *
 *  It is normally wrapped in macro TERMINATE_EXECUTION_ON_ERROR(format,...)
 *
 *  @param  trace_group trace group for function where error occurred
 *  @param  funcname    function name
 *  @param  lineno      line number
 *  @param  fn          file name
 *  @param  format      printf() style format string
 *  @param  ...         arguments for format string
 *
 *  @ingroup output_redirection
 */
void terminate_execution_on_error(
        Trace_Group   trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...)
{
   char buffer[200];
   char buf2[250];
   char * finalBuffer = buffer;
   va_list(args);
   va_start(args, format);
   vsnprintf(buffer, 200, format, args);

   if ( is_tracing(trace_group, fn) ) {
      snprintf(buf2, 250, "(%s) %s", funcname, buffer);
      finalBuffer = buf2;
   }

   f0puts(finalBuffer, FERR);
   f0puts("\n", FERR);

   ddc_abort(funcname, lineno, fn, DDCL_INTERNAL_ERROR);
}

