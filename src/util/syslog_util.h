/** @file syslog_util.h */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SYSLOG_UTIL_H_
#define SYSLOG_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <syslog.h>

#include "msg_util.h"

#ifdef UNUSED
#define SIMPLE_SYSLOG(_syslog_priority, format, ...) \
do { \
         char * body = g_strdup_printf(format, ##__VA_ARGS__); \
         syslog(_syslog_priority, PRItid" (%s) %s", (intmax_t) tid(), __func__, body); \
         free(body); \
} while(0)
#endif

#ifdef UNUSED
#define SIMPLE_SYSLOGF(_debug, _syslog_priority, format, ...) \
do { \
      if (_debug) { \
         char * body = g_strdup_printf(format, ##__VA_ARGS__); \
         syslog(_syslog_priority, PRItid" (%s) %s", (intmax_t) tid(), __func__, body); \
         free(body); \
      } \
} while(0)
#endif

#ifdef COMPARE_VS_CORE
#define DECORATED_SYSLOG(_log_level, _msg) \
do { \
   { \
      char prefix[100]; \
      get_msg_decoration(prefix, 100, /*dest_syslog*/ true); \
      syslog(_log_level, "%s%s ", prefix, _msg); \
   } \
} while (0)
#endif

/** Macros for writing messages to the system log.
 *
 *  For debugging purposes messages, messages may have a final tag field to
 *  identify their origin.when viewing the log. Whether the tag is appended
 *  depends on the value of global #tag_output.
 *
 *  Naming convention:
 *     BASIC_   non-variadic
 *     SIMPLE_  variadic
 */

/** Variant that does not include the name of the current function,
 *  non-variadic message.
 *
 *  Format: [elapsed time][thread id] (func name) message (tag)
 *
 *  @param _syslog_priority
 *  @param _msg
 *
 *  Whether the tag field is appended depends on the value
 *  of global #tag_output.
 */
#define BASIC_STD_SYSLOG(_syslog_priority, _msg) \
do { \
      char prefix[100] = {0}; \
      get_msg_decoration(prefix, 100, true); \
      syslog(_syslog_priority, "%s%s%s", prefix, _msg, (tag_output) ? " (N)" : ""  ); \
} while(0)


#define BARE_STD_SYSLOG(_syslog_priority, _msg) \
do { \
      char prefix[100] = {0}; \
      get_msg_decoration(prefix, 100, true); \
      int prefix_len = strlen(prefix); \
      syslog(_syslog_priority, "%*s%s%s", prefix_len, "", _msg, (tag_output) ? " (N)" : ""  ); \
} while(0)



/** Variant that includes the name of the current function,
 *  non-variadic message.
 *
 *  Format: [elapsed time][thread id] (func name) message (tag)
 *
 *  @param _syslog_priority
 *  @param _msg
 *
 *  Whether the tag field is included depends on the value
 *  of global ##tag_output.
 */
#define BASIC_STD_FUNC_SYSLOG(_syslog_priority, _msg) \
do { \
      char prefix[100] = {0}; \
      get_msg_decoration(prefix, 100, true); \
      char * funcname_field = g_strdup_printf("%-*s", funcname_field_size, __func__); \
      syslog(_syslog_priority, "%s(%s) %s%s", prefix, funcname_field, _msg, (tag_output) ? " (N)" : ""  ); \
      g_free(funcname_field); \
} while(0)


/** Variant that includes explicit function name
 *  non-variadic message.
 *
 *  Format: [elapsed time][thread id] (func name) message (tag)
 *
 *  @param _syslog_priority
 *  @param _msg
 *
 *  Whether the tag field is included depends on the value
 *  of global ##tag_output.
 */
#define BASIC_STD_SYSLOGX(_syslog_priority, _func, _msg) \
do { \
      char prefix[100] = {0}; \
      get_msg_decoration(prefix, 100, true); \
      char * funcname_field = g_strdup_printf("%-*s", funcname_field_size, _func); \
      syslog(_syslog_priority, "%s(%s) %s%s", prefix, funcname_field, _msg, (tag_output) ? " (N)" : ""  ); \
      g_free(funcname_field); \
} while(0)


/** Variant that does not include the name of the current function,
 *  message has variadic form.
 *
 *  Format: [elapsed time][thread id] msssage (tg)
 *
 *  @param _syslog_priority
 *  @param _format
 *  @param ...
 *
 *  Whether the tag field is included depends on the value
 *  of global ##tag_output.
 */
#define SIMPLE_STD_SYSLOG(_syslog_priority, _format, ...) \
do { \
      char * body = g_strdup_printf(_format, ##__VA_ARGS__); \
      char prefix[100] = {0}; \
      get_msg_decoration(prefix, 100, true); \
      syslog(_syslog_priority, "%s%s%s", prefix, body, (tag_output) ? " (N)" : ""  ); \
      free(body); \
} while(0)

/** Variant that does not include the name of the current function,
 *  message has variadic form.
 *  Whether output actually occurs depends on the value of the _debug.
 *
 *  Format: [elapsed time][thread id] msssage (tag)
 *
 *  @param _debug
 *  @param _syslog_priority
 *  @param _format
 *  @param ...
 *
 *  Whether the tag field is included depends on the value
 *  of global ##tag_output.
 */

#define SIMPLE_STD_SYSLOGF(_debug, _syslog_priority, _format, ...) \
do { \
   if (_debug) { \
      char * body = g_strdup_printf(_format, ##__VA_ARGS__); \
      char prefix[100] = {0}; \
      get_msg_decoration(prefix, 100, true); \
      syslog(_syslog_priority, "%s%s%s", prefix, body, (tag_output) ? " (N)" : ""  ); \
      free(body); \
   } \
} while(0)

/** Variant that includes the name of the current function,
 *  message has variadic form.
 *
 *  Format: [elapsed time][thread id] (function name) msssage (tag)
 *
 *  @param _syslog_priority
 *  @param _format
 *  @param ...
 *
 *  Whether the tag field is included depends on the value
 *  of global ##tag_output.
 */

#define SIMPLE_STD_FUNC_SYSLOG(_syslog_priority, _format, ...) \
do { \
      char * body = g_strdup_printf(_format, ##__VA_ARGS__); \
      char prefix[100] = {0}; \
      get_msg_decoration(prefix, 100, true); \
      syslog(_syslog_priority, "%s(%s) %s%s", prefix, __func__, body, (tag_output) ? " (N)" : ""  ); \
      free(body); \
} while(0)


/** Variant that does not include the name of the current function,
 *  message has variadic form.
 *  Whether output actually occurs depends on the value of the _debug.arg.
 *  whether [elapsed time][thread id} are included depends on value of
 *  function rpt_get_ornamaentation_enabled().  This variant is useful
 *  when incorporating report output.
 *
 *  Format: [elapsed time][thread id] msssage (tag)
 *
 *  @oaram  debug
 *  @param _syslog_priority
 *  @param _format
 *  @param ...
 *
 *  Whether the tag field is included depends on the value
 *  of global ##tag_output.
 */
#define SIMPLE_REPORT_SYSLOGF(_debug, _syslog_priority, format, ...) \
do { \
   if (_debug) { \
      char * body = g_strdup_printf(format, ##__VA_ARGS__); \
      char prefix[100] = {0}; \
      if (rpt_get_ornamentation_enabled() ) { \
         get_msg_decoration(prefix, 100, true); \
      } \
      syslog(_syslog_priority, "%s%s%s", prefix, body, (tag_output) ? " (N)" : ""  ); \
      free(body); \
   } \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* SYSLOG_UTIL_H_ */
