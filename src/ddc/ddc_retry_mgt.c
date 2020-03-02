// ddc_retry_mgt.c

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>

// for syscall
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#include "util/glib_util.h"

#include "base/core.h"
#include "base/parms.h"
#include "base/thread_sleep_data.h"

#include "ddc_retry_mgt.h"

// initial default values for new threads
uint16_t initial_default_maxtries[] = {
            MAX_WRITE_ONLY_EXCHANGE_TRIES,
            MAX_WRITE_READ_EXCHANGE_TRIES,
            MAX_MULTI_EXCHANGE_TRIES };



typedef struct{
   uint16_t maxtries[3];
} Maxtries_Rec;






static GMutex        global_maxtries_mutex;

// Retrieves Thead_Tries_Data for the current thread
// Creates and initializes a new instance if not found
// static
static Maxtries_Rec * get_thread_maxtries_rec() {
   bool debug = false;
   DBGMSF(debug, "Starting.");
   static GPrivate per_thread_key = G_PRIVATE_INIT(g_free);
   Maxtries_Rec *mrec = g_private_get(&per_thread_key);
   if (!mrec) {
      mrec = g_new0(Maxtries_Rec, 1);
      int thread_id = syscall(SYS_gettid);
      DBGMSF(debug, "Created Maxtries_Rec for thread %d", thread_id);
      for (int ndx = 0; ndx < RETRY_TYPE_COUNT; ndx++) {
         mrec->maxtries[ndx] = default_maxtries.maxtries[ndx];
      }
      g_private_set(&per_thread_key, mrec);
   }
   DBGMSF(debug, "Dome.  Returning %p", mrec);
   return mrec;
}



