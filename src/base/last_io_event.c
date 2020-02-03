/** \last_io_event.c
 */

// Copyright (C) 2019-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <glib-2.0/glib.h>

#include "last_io_event.h"


static bool trace_finish_timestamps = false;

static gsize leave_event_initialized = 0;

G_LOCK_DEFINE_STATIC(timestamps_lock);

// Maintain timestamps

static GPtrArray * timestamps = NULL;


static void free_io_event_timestamp_internal(gpointer data) {
   IO_Event_Timestamp * ptr = (IO_Event_Timestamp *) data;
   assert(memcmp(ptr->marker, IO_EVENT_TIMESTAMP_MARKER, 4) == 0);
   ptr->marker[3] = 'X';
   // Do not free ptr->filename or ptr->function since these point
   // to constants in compiled code
   free(data);
}


static IO_Event_Timestamp* find_io_event_timestamp(int fd) {
   assert(timestamps);
   IO_Event_Timestamp * result = NULL;

   for (int ndx = 0; ndx < timestamps->len; ndx++) {
      IO_Event_Timestamp * cur = g_ptr_array_index(timestamps, ndx);
      if (cur->fd == fd) {
         result = cur;
         break;
      }
   }
   // DBGMSG("Returning %p", result);
   return result;
}


static void ensure_initialized() {
   if (g_once_init_enter(&leave_event_initialized)) {
      if (!timestamps) {     // first call?
         timestamps = g_ptr_array_new_full(4, free_io_event_timestamp_internal);
      }
      g_once_init_leave(&leave_event_initialized, 1);
   }
}


IO_Event_Timestamp * get_io_event_timestamp(int fd)
{
   ensure_initialized();
   G_LOCK(timestamps_lock);
   IO_Event_Timestamp * ts = find_io_event_timestamp(fd);
   if (!ts) {
      ts = calloc(1, sizeof(IO_Event_Timestamp));
      memcpy(ts->marker, IO_EVENT_TIMESTAMP_MARKER, 4);
      ts->fd = fd;
      g_ptr_array_add(timestamps, ts);
   }
   G_UNLOCK(timestamps_lock);
   assert(ts);
   return ts;
}


// IO_Event_Timestamp * new_io_event_timestamp(int fd);


void free_io_event_timestamp(int fd) {
   ensure_initialized();
   assert(timestamps);
   G_LOCK(timestamps_lock);
   IO_Event_Timestamp * ts = find_io_event_timestamp(fd);
   if (ts)
      g_ptr_array_remove(timestamps, ts);
   G_UNLOCK(timestamps_lock);
}


// *** Last IO

void record_io_finish(
      int           fd,
      uint64_t      finish_time,
      IO_Event_Type event_type,
      char *        filename,
      int           lineno,
      char *        function)
{
   bool debug = false;

   IO_Event_Timestamp * tsrec = get_io_event_timestamp(fd);
   assert(tsrec);

   uint64_t prior_nanos = 0;
   uint64_t cur_nanos = 0;
   uint64_t delta_nanos = 0;
   uint64_t delta_nanos_then_millis = 0;

   if (tsrec->finish_time != 0) {
      prior_nanos = tsrec->finish_time;
      cur_nanos = finish_time;
      delta_nanos = cur_nanos - prior_nanos;
      delta_nanos_then_millis = delta_nanos / (1000*1000);

      assert(finish_time > tsrec->finish_time);

      // DBGMSF(debug, "prior_nanos = %"PRIu64", cur_nanos = %"PRIu64", delta_nannos = %"PRIu64", delta millis from delta nanos: %"PRIu64,
      //                prior_nanos,             cur_nanos,             delta_nanos, delta_nanos_then_millis);

      // DBGMSF(debug, "prior_millis = %"PRIu64", cur_millis = %"PRIu64", delta_nullis = %"PRIu64,
      //                prior_millis,             cur_millis,             delta_millis);
      assert(cur_nanos >= prior_nanos);
      // if (cur_nanos == prior_nanos)
      //    DBGMSG("======= equal");

   }

   DBGTRC(trace_finish_timestamps | debug, DDCA_TRC_NONE,
          "fd=%d, event_type = %-10s, function = %-20s, delta: %"PRIu64" nanosec, %"PRIu64" millisec)",     // PRIU64,
                   fd, io_event_name(event_type), function, delta_nanos, delta_nanos_then_millis);

   // tsrec->fd = fd;   // unnecessary
   tsrec->event_type  = event_type;
   tsrec->filename    = filename;
   tsrec->lineno      = lineno;
   tsrec->finish_time = finish_time;
   tsrec->function    =  function;
}


// I2C_RECORD_IO_EVENT(
//       IE_OPEN,
//       ( fd = open(filename, (callopts & CALLOPT_RDONLY) ? O_RDONLY : O_RDWR) ),
//       busno,
//       fd
//       );
// if (fd >= 0)
// I2C_RECORD_IO_FINISH_NOW(busno, IE_OPEN, fd);

