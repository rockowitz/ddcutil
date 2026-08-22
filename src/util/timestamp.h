/** @file timestamp.h
 *
 *  Timestamp management
 */

// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TIMESTAMP_H_
#define TIMESTAMP_H_

#ifdef __cplusplus
extern "C" {
#endif

/** \cond */
#include <glib-2.0/glib.h>
/** \endcond */

//
// Timestamp Generation
//
uint64_t cur_realtime_nanosec();   // Returns the current value of the realtime clock in nanoseconds
void     show_timestamp_history(); // For debugging
uint64_t elapsed_time_nanosec();   // nanoseconds since start of program, first call initializes
char *   formatted_elapsed_time0_t(uint64_t et_nanos, guint precision);
char *   formatted_elapsed_time_t(guint precision); // printable elapsed time
char *   formatted_time_t(uint64_t nanos);
char *   formatted_epoch_time_t(time_t epoch_seconds);

// Unit conversion.  Inline functions, not macros, so that the argument is
// evaluated exactly once.  The rounding below requires the value twice, and
// several callers pass an expression with a side effect, e.g.
// NANOS2MICROS(cur_realtime_nanosec() - t0).

/** Converts nanoseconds to microseconds, rounded to nearest.
 *
 *  @remark
 *  The rounding adjustment is applied after the division, not by adding half
 *  a microsecond before it.  Adding first overflows for a value within 500 of
 *  UINT64_MAX, as results from an unsigned subtraction whose operands were in
 *  the unexpected order, and reports a small number instead of an evidently
 *  wrong one.
 */
static inline uint64_t NANOS2MICROS(uint64_t _nanosec) {
   return _nanosec/1000 + ((_nanosec % 1000 >= 500) ? 1 : 0);
}

/** Converts nanoseconds to milliseconds, rounded to nearest.
 *  See #NANOS2MICROS() regarding the order of division and rounding.
 */
static inline uint64_t NANOS2MILLIS(uint64_t _nanosec) {
   return _nanosec/1000000 + ((_nanosec % 1000000 >= 500000) ? 1 : 0);
}

/** Converts microseconds to milliseconds, rounded to nearest.
 *  See #NANOS2MICROS() regarding the order of division and rounding.
 */
static inline uint64_t MICROS2MILLIS(uint64_t _microsec) {
   return _microsec/1000 + ((_microsec % 1000 >= 500) ? 1 : 0);
}

static inline uint64_t MILLIS2NANOS(uint64_t _millisec) {
   return _millisec * (uint64_t) 1000000;
}

static inline uint64_t MILLIS2MICROS(uint64_t _millisec) {
   return _millisec * (uint64_t) 1000;
}

static inline uint64_t SECS2NANOS(uint64_t _sec) {
   return _sec * (uint64_t) 1000000000;
}

static inline uint64_t SECS2MILLIS(uint64_t _sec) {
   return _sec * (uint64_t) 1000;
}

uint64_t cur_monotonic_time_nanosec();  // time since boot, excluding sleep time
uint64_t cur_boot_time_nanosec();       // time since boot, including sleep time

#ifdef __cplusplus
}
#endif

#endif /* TIMESTAMP_H_ */
