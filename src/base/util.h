/* util.h
 *
 * Utility functions not specific to the current application.
 *
 * This header file has no dependencies on any application specific
 * header file.
 */

#ifndef I2CPROBE_UTIL
#define I2CPROBE_UTIL

#include <stdbool.h>
#include <stdlib.h>


//
// General
//

// For defining boolean "exit if failure" function parameters, allowing
// functions to be called with more comprehensible parameter values than
// "true" and "false".
typedef bool Failure_Action;
static const Failure_Action EXIT_IF_FAILURE = true;
static const Failure_Action RETURN_ERROR_IF_FAILURE = false;


//
// Timing functions
//

// Returns the current value of the realtime clock in nanoseconds:
long cur_realtime_nanosec();
// For debugging:
void report_timestamp_history();


//
// Error handling
//

void report_ioctl_error(
      int         errnum,
      const char* funcname,
      int         lineno,
      char*       filename,
      bool        fatal);


void report_ioctl_error2(
      int   errnum,
      int   fh,
      int   request,
      void* data,
      const char* funcname,   // const to avoid warning msg on references at compile time
      int   lineno,
      char* filename,
      bool fatal);


// reports a program logic error and terminates execution
void program_logic_error(
      const char * funcname,
      const int    lineno,
      const char * fn,
      char *       format,
      ...);

#define PROGRAM_LOGIC_ERROR(format, ...) \
   program_logic_error(__func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


#endif
