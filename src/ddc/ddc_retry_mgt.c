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

#include "ddc_retry_mgt.h"

// initial default values for new threads
uint16_t max_tries_default[] = {
            MAX_WRITE_ONLY_EXCHANGE_TRIES,
            MAX_WRITE_READ_EXCHANGE_TRIES,
            MAX_MULTI_EXCHANGE_TRIES };

char * retry_class_descriptions[] = {
      "write only",
      "write-read",
      "multi-part",
};

char * retry_class_names[] = {
      "DDCA_WRITE_ONLY_TRIES",
      "DDCA_WRITE_READ_TRIES",
      "DDCA_MULTI_PART_TRIES"
};

typedef struct{
   uint16_t maxtries[3];
} Maxtries_Rec;

static Maxtries_Rec default_maxtries = {{
      MAX_WRITE_ONLY_EXCHANGE_TRIES,
      MAX_WRITE_READ_EXCHANGE_TRIES,
      MAX_MULTI_EXCHANGE_TRIES }};


const char * ddc_retry_type_name(DDCA_Retry_Type type_id) {
   return retry_class_names[type_id];
}

const char * ddc_retry_type_description(DDCA_Retry_Type type_id) {
   return retry_class_descriptions[type_id];
}

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


void ddc_set_default_single_max_tries(DDCA_Retry_Type rcls, uint16_t new_max_tries) {
   bool debug = false;
   DBGMSF(debug, "Executing. rcls = %s, new_max_tries=%d", ddc_retry_type_name(rcls), new_max_tries);
   g_mutex_lock(&global_maxtries_mutex);
   default_maxtries.maxtries[rcls] = new_max_tries;
   g_mutex_unlock(&global_maxtries_mutex);
}

void ddc_set_default_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]) {
   bool debug = true;
   DBGMSF(debug, "Executing. new_max_tries = [%d,%d,%d]",
                 new_max_tries[0], new_max_tries[1], new_max_tries[2] );
   g_mutex_lock(&global_maxtries_mutex);
   Maxtries_Rec * mrec = get_thread_maxtries_rec();
   for (DDCA_Retry_Type type_id = 0; type_id < RETRY_TYPE_COUNT; type_id++) {
      if (new_max_tries[type_id] > 0)
         mrec->maxtries[type_id] = new_max_tries[type_id];
   }
   g_mutex_unlock(&global_maxtries_mutex);
}



void ddc_set_cur_thread_single_max_tries(DDCA_Retry_Type retry_class, uint16_t new_max_tries) {
   bool debug = false;
   DBGMSF(debug, "Executing. retry_class = %s, new_max_tries=%d",
                 ddc_retry_type_name(retry_class), new_max_tries);
   Maxtries_Rec * mrec = get_thread_maxtries_rec();
   mrec->maxtries[retry_class] = new_max_tries;
}

void ddc_set_cur_thread_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]) {
   bool debug = true;
   DBGMSF(debug, "Executing. new_max_tries = [%d,%d,%d]",
                 new_max_tries[0], new_max_tries[1], new_max_tries[2] );
   for (DDCA_Retry_Type type_id = 0; type_id < RETRY_TYPE_COUNT; type_id++) {
      if (new_max_tries[type_id] > 0)
         default_maxtries.maxtries[type_id] = new_max_tries[type_id];
   }
}


uint16_t ddc_get_cur_thread_single_max_tries(DDCA_Retry_Type type_id) {
   bool debug = false;
   Maxtries_Rec * mrec = get_thread_maxtries_rec();
   uint16_t result = mrec->maxtries[type_id];
   DBGMSF(debug, "retry type=%s, returning %d", ddc_retry_type_name(type_id), result);
   return result;
}



