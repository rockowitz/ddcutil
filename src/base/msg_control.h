/*  msg_control.h
 *
 *  Message management
 *
 *  Created on: Jun 15, 2014
 *      Author: rock
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#include <stdarg.h>
#include <stdbool.h>

#include "util/coredefs.h"
#include "base/util.h"


//
// Message level control
//

#ifdef OLD
typedef enum {TERSE, NORMAL, VERBOSE} Msg_Level;
Msg_Level get_global_msg_level();
void set_global_msg_level(Msg_Level newval);
char * msg_level_name(Msg_Level val);
#endif

// new way:
// values assigned to constants allow them to be or'd in bit flags
// ascending values in order of verbosity
typedef enum {OL_DEFAULT=0x01, OL_PROGRAM=0x02, OL_TERSE=0x04, OL_NORMAL=0x08, OL_VERBOSE=0x10} Output_Level;
Output_Level get_output_level();
void set_output_level(Output_Level newval);
char * output_level_name(Output_Level val);


//
// Debug trace message control
//

typedef Byte Trace_Group;
#define TRC_BASE 0x80
#define TRC_I2C  0x40
#define TRC_ADL  0x20
#define TRC_DDC  0x10
#define TRC_TOP  0x08

#define TRC_NEVER  0x00
#define TRC_ALWAYS 0xff

Trace_Group trace_class_name_to_value(char * name);
void set_trace_levels(Trace_Group trace_flags);
extern const char * trace_group_names[];
extern const int    trace_group_ct;

void show_trace_groups();

bool is_tracing(Trace_Group trace_group, const char * filename);
#define IS_TRACING() is_tracing(TRACE_GROUP, __FILE__)


void severemsg(
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);


void dbgmsg(
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);

void trcmsg(
        Trace_Group  trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);
#define SEVEREMSG(            format, ...) dbgmsg(             __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#define DBGMSG(               format, ...) dbgmsg(             __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#define DBGMSGF( debug_flag,     format, ...) \
   do { if (debug_flag) dbgmsg(  __func__, __LINE__, __FILE__, format, ##__VA_ARGS__); }  while(0)
#define DBGMSF( debug_flag,     format, ...) \
   do { if (debug_flag) dbgmsg(  __func__, __LINE__, __FILE__, format, ##__VA_ARGS__); }  while(0)
#define TRCMSG(               format, ...) trcmsg(TRACE_GROUP, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
// which of these are really useful?
#define TRCALWAYS(            format, ...) trcmsg(0xff,        __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#define TRCMSGTG(trace_group, format, ...) trcmsg(trace_group, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#define TRCMSGTF(trace_flag, format, ...) \
    do { if (trace_flag) trcmsg(0xff, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__); }  while(0)
// alt: trcmsg( ( (trace_flag) ? (0xff) : TRACE_GROUP ), __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


//
// Old debug message control - to be eliminated
//

#ifdef OLD
typedef enum {NEVER, RESPECT_DEBUG, ALWAYS} TraceControl;

// Utility function.  Adjust debug level in local_debug based on global_trace_level
bool adjust_debug_level(bool local_debug, TraceControl global_trace_level);
#endif


//
// DDC Data Errors
//

// Display messages for I2C error conditions that can be retried.
extern bool show_recoverable_errors;

bool is_reporting_ddc(Trace_Group trace_group, const char * fn);
#define IS_REPORTING_DDC() is_reporting_ddc(TRACE_GROUP, __FILE__)

void ddcmsg(Trace_Group trace_group, const char * funcname, const int lineno, const char * fn, char * format, ...);

#define DDCMSG(format, ...) ddcmsg(TRACE_GROUP, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


void show_reporting();


//
// Dead code
//


//typedef enum {MSGLVL_DEBUG, MSGLVL_INFO, MSGLVL_WARN, MSGLVL_ERR} MessageLevel;
//
//void errmsg(
//        Byte         msgGroup,
//        MessageLevel severity,
//        const char * funcname,
//        const int    lineno,
//        const char * fn,
//        char *       format,
//        ...);
//
/*  (block comment to avoid multi-line comment warning
  #define ERRMSG(severity, format, ... ) \
        errmsg(trace_group, severity, __func__, __LINE, __FILE__, format, ##__VA_ARGS__)
*/


#endif /* DEBUG_H_ */
