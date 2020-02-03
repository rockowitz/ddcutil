/** @file last_io_event.h
 */

// Copyright (C) 2019-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LAST_IO_EVENT_H_
#define LAST_IO_EVENT_H_

#include "public/ddcutil_types.h"

#include "execution_stats.h"

#define IO_EVENT_TIMESTAMP_MARKER "IOET"
typedef
struct {
   char          marker[4];
   uint64_t      finish_time;
   IO_Event_Type event_type;
   char *        filename;
   int           lineno;
   char *        function;
   int           fd;       // Linux file descriptor
} IO_Event_Timestamp;

IO_Event_Timestamp * get_io_event_timestamp(int fd);
// IO_Event_Timestamp * new_io_event_timestamp(int fd);
void free_io_event_timestamp(int fd);

void record_io_finish(
      int              fd,
      uint64_t         finish_time,
      IO_Event_Type    event_type,
      char *           filename,
      int              lineno,
      char *           function);

#define RECORD_IO_FINISH(_fd, _event_type, _timestamp) \
   do \
   record_io_finish(_fd, _timestamp , _event_type, (char*) __FILE__, __LINE__, (char*) __func__); \
   while(0)

#define RECORD_IO_FINISH_NOW(_fd, _event_type) \
   do \
   record_io_finish(_fd, cur_realtime_nanosec() , _event_type, (char*) __FILE__, __LINE__, (char*) __func__); \
   while(0)

// combines log_io_call() with recrod_io_finish():
#define RECORD_IO_EVENTX(_fd, _event_type, _cmd_to_time)  { \
   uint64_t _start_time = cur_realtime_nanosec(); \
   _cmd_to_time; \
   uint64_t  end_time = cur_realtime_nanosec(); \
   log_io_call(_event_type, __func__, _start_time, end_time); \
   record_io_finish(_fd, end_time, _event_type, __FILE__, __LINE__, (char *)__func__); \
}

#endif /* LAST_IO_EVENT_H_ */
