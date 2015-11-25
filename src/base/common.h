/*  common.h
 *
 *  Created on: Jun 5, 2014
 *      Author: rock
 *
 *  Declarations used throughout the I2C DDC application.
 */


#ifndef DDC_COMMON_H_
#define DDC_COMMON_H_

#include "base/util.h"
#include "base/displays.h"
#include "base/msg_control.h"

//
// Miscellaneous

// Maximum numbers of values on command ddc setvcp
#define MAX_SETVCP_VALUES    50


//
// Output format control
//

#ifdef OLD
typedef enum {OUTPUT_NORMAL, OUTPUT_PROG_VCP, OUTPUT_PROG_BUSINFO} Output_Format;

char * output_format_name(Output_Format format);
void   set_output_format(Output_Format format);
Output_Format get_output_format();
#endif

//
// Sleep and sleep statistics
//

void sleep_millis( int milliseconds);
void sleep_millis_with_trace(int milliseconds, const char * caller_location, const char * message);

typedef struct {
   long requested_sleep_milliseconds;
   long actual_sleep_nanos;
   int  total_sleep_calls;
} Sleep_Stats;

void init_sleep_stats();
Sleep_Stats * get_sleep_stats();
void report_sleep_stats(int depth);


//
// Error handling
//

void terminate_execution_on_error(
        Trace_Group  trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);

#define TERMINATE_EXECUTION_ON_ERROR(format, ...) \
   terminate_execution_on_error(TRACE_GROUP, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


#endif /* DDC_COMMON_H_ */
