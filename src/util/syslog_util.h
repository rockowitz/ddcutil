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


#define BASIC_STD_SYSLOG(_syslog_priority, _msg) \
do { \
      char prefix[100] = {0}; \
      get_msg_decoration(prefix, 100, true); \
      syslog(_syslog_priority, "%s%s%s", prefix, _msg, (tag_output) ? " (N)" : ""  ); \
} while(0)


#define BASIC_STD_FUNC_SYSLOG(_syslog_priority, _msg) \
do { \
      char prefix[100] = {0}; \
      get_msg_decoration(prefix, 100, true); \
      syslog(_syslog_priority, "%s(%s) %s%s", prefix, __func__, _msg, (tag_output) ? " (N)" : ""  ); \
} while(0)


#define SIMPLE_STD_SYSLOG(_syslog_priority, format, ...) \
do { \
      char * body = g_strdup_printf(format, ##__VA_ARGS__); \
      char prefix[100] = {0}; \
      get_msg_decoration(prefix, 100, true); \
      syslog(_syslog_priority, "%s%s%s", prefix, body, (tag_output) ? " (N)" : ""  ); \
      free(body); \
} while(0)


#define SIMPLE_STD_FUNC_SYSLOG(_syslog_priority, _format, ...) \
do { \
      char * body = g_strdup_printf(_format, ##__VA_ARGS__); \
      char prefix[100] = {0}; \
      get_msg_decoration(prefix, 100, true); \
      syslog(_syslog_priority, "%s(%s) %s%s", prefix, __func__, body, (tag_output) ? " (N)" : ""  ); \
      free(body); \
} while(0)



#define SIMPLE_DECORATED_SYSLOGF(_debug, _syslog_priority, format, ...) \
do { \
   if (_debug) { \
      char * body = g_strdup_printf(format, ##__VA_ARGS__); \
      char prefix[100] = {0}; \
      get_msg_decoration(prefix, 100, true); \
      syslog(_syslog_priority, "%s%s%s", prefix, body, (tag_output) ? " (N)" : ""  ); \
      free(body); \
   } \
} while(0)


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
