/*  msg_control.c
 *
 *  Message management
 *
 *  Created on: Jun 15, 2014
 *      Author: rock
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "util/string_util.h"

#include "base/util.h"

#include "base/msg_control.h"


//
// Message level control
//

#ifdef OLD
static Msg_Level global_msg_level;

Msg_Level get_global_msg_level() {
   return global_msg_level;
}

void set_global_msg_level(Msg_Level newval) {
   // printf("(%s) newval=%s  \n", __func__, msgLevelName(newval) );
   global_msg_level = newval;
   // old way:
   // if (globalMsgLevel == VERBOSE) {
   //    // primitive control for now
   //    traceLevels = 0xFF;   // turn everything on
   // }
}

char * msg_level_name(Msg_Level val) {
   char * result = NULL;
   switch (val) {
      case TERSE:
         result = "Terse";
         break;
      case NORMAL:
         result = "Normal";
         break;
      case VERBOSE:
         result = "Verbose";
         break;
      default:
         PROGRAM_LOGIC_ERROR("Invalid Msg_Level value: %d", val);
   }
   return result;
}
#endif


#define SHOW_REPORTING_TITLE_START 0
#define SHOW_REPORTING_MIN_TITLE_SIZE 28


void print_simple_title_value(int    offset_start_to_title,
                              char * title,
                              int    offset_title_start_to_value,
                              char * value)
{
   printf("%.*s%-*s%s\n",
          offset_start_to_title,"",
          offset_title_start_to_value, title,
          value);
}


// New way

static Output_Level output_level;

Output_Level get_output_level() {
   return output_level;
}

void set_output_level(Output_Level newval) {
   // printf("(%s) newval=%s  \n", __func__, msgLevelName(newval) );
   output_level = newval;
   // old way:
   // if (output_level == VERBOSE) {
   //    // primitive control for now
   //    traceLevels = 0xFF;   // turn everything on
   // }
}

char * output_level_name(Output_Level val) {
   char * result = NULL;
   switch (val) {
      case OL_DEFAULT:
         result = "Default";
         break;
      case OL_PROGRAM:
         result = "Program";
         break;
      case OL_TERSE:
         result = "Terse";
         break;
      case OL_NORMAL:
         result = "Normal";
         break;
      case OL_VERBOSE:
         result = "Verbose";
         break;
      default:
         PROGRAM_LOGIC_ERROR("Invalid Output_Level value: %d", val);
   }
   // printf("(%s) val=%d 0x%02x, returning: %s\n", __func__, val, val, result);
   return result;
}

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

#ifdef REFERENCE
typedef Byte Trace_Group;
#define TRC_BASE 0x80
#define TRC_I2C  0x40
#define TRC_ADL  0x20
#define TRC_DDC  0x10
#define TRC_TOP  0x08
#endif

// same order as flags in TraceGroup
const Byte   trace_group_ids[]   = {TRC_BASE, TRC_I2C, TRC_ADL, TRC_DDC, TRC_TOP};
const char * trace_group_names[] = {"BASE",   "I2C",   "ADL",   "DDC",   "TOP"};
const int    trace_group_ct = sizeof(trace_group_names)/sizeof(char *);

Trace_Group trace_class_name_to_value(char * name) {
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

static Byte trace_levels = 0x00;

void set_trace_levels(Trace_Group trace_flags) {
   bool debug = false;
   if (debug)
      printf("(%s) trace_flags=0x%02x\n", __func__, trace_flags);
   trace_levels = trace_flags;
}

bool is_tracing(Trace_Group trace_group, const char * filename) {
   bool result =  (trace_group == 0xff) || (trace_levels & trace_group); // is traceGroup being traced?
   // printf("(%s) traceGroup = %02x, filename=%s, traceLevels=0x%02x, returning %d\n",
   //        __func__, traceGroup, filename, traceLevels, result);
   return result;
}

void show_trace_groups() {
   char buf[100] = "";
   int ndx;
   for (ndx=0; ndx< trace_group_ct; ndx++) {
      if ( trace_levels & trace_group_ids[ndx]) {
         if (strlen(buf) > 0)
            strcat(buf, ", ");
         strcat(buf, trace_group_names[ndx]);
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


void dbgmsg(
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
      snprintf(buf2, 250, "(%s) %s", funcname, buffer);
      puts(buf2);
      va_end(args);
}


void trcmsg(
        Byte         trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...)
{
   if ( is_tracing(trace_group, fn) ) {
      char buffer[200];
      char buf2[250];
      va_list(args);
      va_start(args, format);
      vsnprintf(buffer, 200, format, args);
      snprintf(buf2, 250, "(%s) %s", funcname, buffer);
      puts(buf2);
      va_end(args);
   }
}






//
// Old debug message control - to be eliminated
//

#ifdef OLD
// Adjust local_debug based on global_trace_level
bool adjust_debug_level(bool local_debug, TraceControl global_trace_level) {
   bool result = local_debug;      // if global_trace_level == RESPECT
   if (global_trace_level == NEVER)
      result = false;
   else if (global_trace_level == ALWAYS)
      result = true;
   return result;
}
#endif

//
// DDC Data Errors
//

// global variable
bool show_recoverable_errors = true;


bool is_reporting_ddc(Trace_Group traceGroup, const char * fn) {
  bool result = (is_tracing(traceGroup,fn) || show_recoverable_errors);
  return result;
}

void ddcmsg(Trace_Group traceGroup, const char * funcname, const int lineno, const char * fn, char * format, ...) {
//  if ( is_reporting_ddc(traceGroup, fn) ) {   // wrong
    if (show_recoverable_errors) {
      char buffer[200];
      va_list(args);
      va_start(args, format);
      vsnprintf(buffer, 200, format, args);
      printf("(%s) %s\n", funcname, buffer);
   }
}

void show_ddcmsg() {
   // printf("Reporting DDC data errors: %s\n", bool_repr(show_recoverable_errors));
   print_simple_title_value(SHOW_REPORTING_TITLE_START,
                              "Reporting DDC data errors: ",
                              SHOW_REPORTING_MIN_TITLE_SIZE,
                              bool_repr(show_recoverable_errors));

}


void show_reporting() {
   show_output_level();
   show_ddcmsg();
   show_trace_groups();
   puts("");
}


//
// Dead code
//



// typedef enum {MsgNoOutput, MsgNormalOutput, MsgDebugOutput} MsgOutputType;

// MsgOutputType getMsgOutputType(Byte msgGroup, MessageLevel severity) {
//    return MsgDebugOutput;    // *** TEMP ***
// }


//
//void errmsg(
//        Byte         msgGroup,
//        MessageLevel severity,
//        const char * funcname,
//        const int    lineno,
//        const char * fn,
//        char *       format,
//        ...)
//{
//   MsgOutputType outputType = getMsgOutputType(msgGroup, severity);
//   if (outputType != MsgNoOutput) {
//      char buffer[200];
//      char buf2[250];
//      char * finalBuffer = buffer;
//      va_list(args);
//      va_start(args, format);
//      vsnprintf(buffer, 200, format, args);
//
//      if (outputType != MsgNormalOutput) {
//         snprintf(buf2, 250, "(%s) %s", funcname, buffer);
//         finalBuffer = buf2;
//      }
//
//      puts(finalBuffer);
//   }
//}
//
