/** \file execution_stats.c
 *  Record execution statistics, mainly the count and elapsed time of system calls.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "ddcutil_status_codes.h"

#include "util/glib_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/sleep.h"
#include "base/parms.h"
#include "base/ddc_errno.h"
#include "base/linux_errno.h"

#include "base/execution_stats.h"


//
// Typedefs
//

typedef struct {
   IO_Event_Type  id;
   const char *   name;
   const char *   desc;
   uint64_t       call_nanosec;
   int            call_count;
} IO_Event_Type_Stats;


// struct that accumulates status code occurrence statistics
// The design allows for multiple Status_Code_Counts instances
// used for different purposes (e.g. derived status codes),
// hence the name field.
// But currently there is only 1 instance.

#define STATUS_CODE_COUNTS_MARKER "SCCT"
typedef struct {
   char marker[4];
   GHashTable * error_counts_hash;  // hash table whose key is a status code, and whose
                                    // value is the number of occurrences of that status code
   int          total_status_counts;
   char *       name;
} Status_Code_Counts;


//
// Global Variables
//

// static IO_Event_Type        last_io_event;
// static long                 last_io_timestamp = -1;
static uint64_t             program_start_timestamp;
static uint64_t             resettable_start_timestamp;
static Status_Code_Counts * primary_error_code_counts;
static Status_Code_Counts * retryable_error_code_counts;
static GMutex               status_code_counts_mutex;
static GMutex               global_stats_mutex;


static bool                 debug_status_code_counts_mutex  = false;
static bool                 debug_global_stats_mutex = false;
static bool                 debug_sleep_stats_mutex = false;


//
// IO Event Tracking
//

static
IO_Event_Type_Stats io_event_stats[] = {
      // id           name             desc           nanosec  count
      {IE_WRITE,      "IE_WRITE",      "write calls",       0, 0},
      {IE_READ,       "IE_READ",       "read calls",        0, 0},
      {IE_WRITE_READ, "IE_WRITE_READ", "write/read calls",  0, 0},
      {IE_OPEN,       "IE_OPEN",       "open file calls",   0, 0},
      {IE_CLOSE,      "IE_CLOSE",      "close file calls",  0, 0},
      {IE_OTHER,      "IE_OTHER",      "other I/O calls",   0, 0},
};
#define IO_EVENT_TYPE_CT (sizeof(io_event_stats)/sizeof(IO_Event_Type_Stats))
static GMutex io_event_stats_mutex;
static bool   debug_io_event_stats_mutex;

static
void reset_io_event_stats() {
   bool debug = false || debug_io_event_stats_mutex;
   DBGMSF(debug, "Starting");

   g_mutex_lock(&io_event_stats_mutex);
   for (int ndx = 0; ndx < IO_EVENT_TYPE_CT; ndx++) {
      io_event_stats[ndx].call_count   = 0;
      io_event_stats[ndx].call_nanosec = 0;
   }
   g_mutex_unlock(&io_event_stats_mutex);

   DBGMSF(debug, "Done");
}


/** Returns symbolic name of an event type.
 *
 * @param event_type
 * @return symbolic name
 */
const char * io_event_name(IO_Event_Type event_type) {
   // return io_event_names[event_type];
   return io_event_stats[event_type].name;
}


#ifdef UNUSED
int max_event_name_length() {
   int result = 0;
   int ndx = 0;
   for (;ndx < IO_EVENT_TYPE_CT; ndx++) {
      int curval = strlen(io_event_stats[ndx].name);
      if (curval > result)
         result = curval;
   }
   return result;
}
#endif


static int total_io_event_count() {
   int total = 0;
   int ndx = 0;
   for (;ndx < IO_EVENT_TYPE_CT; ndx++)
      total += io_event_stats[ndx].call_count;
   return total;
}

// unused
uint64_t total_io_event_nanosec() {
   uint64_t total = 0;
   int ndx = 0;
   for (;ndx < IO_EVENT_TYPE_CT; ndx++)
      total += io_event_stats[ndx].call_nanosec;
   return total;
}


// No effect on program logic, but makes debug messages easier to scan
uint64_t normalize_timestamp(uint64_t timestamp) {
   return timestamp - program_start_timestamp;
}


/** Called immediately after an I2C IO call, this function updates the total
 *  number of calls and elapsed time for categories of calls.
 *
 *  @param  event_type        e.g. IE_WRITE
 *  @param  location          function name
 *  @param  start_time_nanos  starting time of the event in nanoseconds
 *  @param  end_time_nanos    ending time of the event in nanoseconds
 */
void log_io_call(
        const IO_Event_Type  event_type,
        const char *         location,
        uint64_t             start_time_nanos,
        uint64_t             end_time_nanos)
{
   bool debug = false || debug_io_event_stats_mutex;

   uint64_t elapsed_nanos = (end_time_nanos-start_time_nanos);
   DBGMSF(debug, "event_type=%d %-10s, elapsed_nanos=%"PRIu64", as millis=%"PRIu64,
                  event_type, io_event_name(event_type), elapsed_nanos, elapsed_nanos/(1000*1000) );

   g_mutex_lock(&io_event_stats_mutex);

   io_event_stats[event_type].call_count++;
   io_event_stats[event_type].call_nanosec += elapsed_nanos;

   g_mutex_unlock(&io_event_stats_mutex);

   DBGMSF(debug, "Updated total nanosec = %"PRIu64", as millis=%"PRIu64,
                  io_event_stats[event_type].call_nanosec, io_event_stats[event_type].call_nanosec /(1000*1000) );
}


/** Reports the accumulated execution statistics
 *
 * @param depth logical indentation depth
 */
void report_io_call_stats(int depth) {
   int d1 = depth+1;
   rpt_title("Call Stats:", depth);
   int total_ct = 0;
   uint64_t total_nanos = 0;
   int ndx = 0;
   // int max_name_length = max_event_name_length();
   // not working as variable length string specifier
   // DBGMSG("max_name_length=%d", max_name_length);
   rpt_vstring(d1, "%-40s Count    Millisec  (      Nanosec)", "Type");
   for (;ndx < IO_EVENT_TYPE_CT; ndx++) {
      if (io_event_stats[ndx].call_count > 0) {
         IO_Event_Type_Stats* curstat = &io_event_stats[ndx];
         char buf[100];
         snprintf(buf, 100, "%-17s (%s)", curstat->desc, curstat->name);
         rpt_vstring(d1, "%-40s  %4d  %10" PRIu64 "  (%13" PRIu64 ")",
                     buf,
                     curstat->call_count,
                     curstat->call_nanosec / (1000*1000),
                     curstat->call_nanosec
                    );
         total_ct += curstat->call_count;
         total_nanos += curstat->call_nanosec;
      }
   }
   rpt_vstring(d1, "%-40s  %4d  %10"PRIu64"  (%13" PRIu64 ")",
               "Totals:",
               total_ct,
               total_nanos / (1000*1000),
               total_nanos
              );
}


//
// Status Code Occurrence Tracking
//

// Design: IO errors are noted in the function that sets psc negative,
// do not leave it to caller to set.  That way do not need to keep track
// if a called function has already set.
//
// BUT: status codes are not noted until they are modulated to Global_Status_Code

static
Status_Code_Counts * new_status_code_counts(char * name) {
   bool debug = false || debug_status_code_counts_mutex;
   DBGMSF(debug, "Starting");

   g_mutex_lock(&status_code_counts_mutex);
   Status_Code_Counts * pcounts = calloc(1,sizeof(Status_Code_Counts));
   memcpy(pcounts->marker, STATUS_CODE_COUNTS_MARKER, 4);
   pcounts->error_counts_hash =  g_hash_table_new(NULL,NULL);
   pcounts->total_status_counts = 0;
   if (name)
      pcounts->name = strdup(name);
   g_mutex_unlock(&status_code_counts_mutex);

   DBGMSF(debug, "Done");
   return pcounts;
}

static void
reset_status_code_counts_struct(Status_Code_Counts * pcounts) {
   bool debug = false || debug_status_code_counts_mutex;
   DBGMSF(debug, "Starting");
   assert(pcounts);

   g_mutex_lock(&status_code_counts_mutex);
   if (pcounts->error_counts_hash)
      g_hash_table_remove_all(pcounts->error_counts_hash);
   pcounts->total_status_counts = 0;
   g_mutex_unlock(&status_code_counts_mutex);

   DBGMSF(debug, "Done");
}



static void
reset_status_code_counts() {
   reset_status_code_counts_struct(primary_error_code_counts);
   reset_status_code_counts_struct(retryable_error_code_counts);
}



static
int log_any_status_code(Status_Code_Counts * pcounts, int rc, const char * caller_name) {
   bool debug = false || debug_status_code_counts_mutex;
   DBGMSF(debug, "caller=%s, rc=%d", caller_name, rc);
   assert(pcounts->error_counts_hash);

   if (rc == 0) {
      DBGMSG("Called with rc = 0, from function %s", caller_name);
   }

   g_mutex_lock(&status_code_counts_mutex);
   pcounts->total_status_counts++;
   // n. if key rc not found, returns NULL, which is 0
   int ct = GPOINTER_TO_INT(g_hash_table_lookup(pcounts->error_counts_hash,  GINT_TO_POINTER(rc)) );
   g_hash_table_insert(pcounts->error_counts_hash, GINT_TO_POINTER(rc), GINT_TO_POINTER(ct+1));
   // DBGMSG("Old count=%d", ct);

   // check the new value
   int newct = GPOINTER_TO_INT(g_hash_table_lookup(pcounts->error_counts_hash,  GINT_TO_POINTER(rc)) );
   g_mutex_unlock(&status_code_counts_mutex);
   // DBGMSG("new count for key %d = %d", rc, newct);
   assert(newct == ct+1);

   DBGMSF(debug, "Done");
   return ct+1;
}


/** Log a status code occurrence
 *
 * @param rc           status code
 * @param caller_name  function logging the event
 *
 * @return status code (unchanged)
 *
 * @remark returning the status code allows for assigning a status code and
 * logging it to be done in one statement
 */
Public_Status_Code
log_status_code(Public_Status_Code rc, const char * caller_name) {
   // DBGMSG("rc=%d, caller_name=%s", rc, caller_name);
   Status_Code_Counts * pcounts = primary_error_code_counts;
   // if ( ddcrc_is_derived_status_code(rc) )
   //    pcounts = secondary_status_code_counts;
   log_any_status_code(pcounts, rc, caller_name);
   return rc;
}

/** Log a status code that occurs in a retry loop
 *
 * @param rc           status code
 * @param caller_name  function logging the event
 *
 * @return status code (unchanged)
 *
 * @remark returning the status code allows for assigning a status code and
 * logging it to be done in one statement
 */
Public_Status_Code
log_retryable_status_code(Public_Status_Code rc, const char * caller_name) {
   // DBGMSG("rc=%d, caller_name=%s", rc, caller_name);
   Status_Code_Counts * pcounts = retryable_error_code_counts;
   log_any_status_code(pcounts, rc, caller_name);
   return rc;
}


// Used by qsort in show_specific_status_counts()
static
int compare( const void* a, const void* b)
{
     int int_a = * ( (int*) (a) );
     int int_b = * ( (int*) (b) );

     if ( int_a == int_b ) return 0;
     else if ( int_a < int_b ) return 1;
     else return -1;
}


static
void show_specific_status_counts(Status_Code_Counts * pcounts) {
   bool debug = false;
   DBGMSF(debug, "Starting");

   char * title = (pcounts->name) ? pcounts->name : "Errors";
   assert(pcounts->error_counts_hash);
   unsigned int keyct;

   // g_hash_table_get_keys_as_array() new in v 2.40, which is not later than the version
   // in all but the most recent distros, .e.g SUSE 13.2 is v 2.18
   // gpointer * keysp = g_hash_table_get_keys_as_array(pcounts->error_counts_hash, &keyct);
   GList * glist = g_hash_table_get_keys(pcounts->error_counts_hash);
   // copies the pointers in the linked list to the array, does not duplicate
   gpointer * keysp = g_list_to_g_array(glist, &keyct);
   g_list_free(glist);
   if (debug) {
      DBGMSG("Keys.  keyct=%d", keyct);
      for (int ndx = 0; ndx < keyct; ndx++) {
         DBGMSG( "keysp[%d]:  %p  %d",
                 ndx, keysp[ndx], GPOINTER_TO_INT(keysp[ndx]) );
      }
   }
   int summed_ct = 0;
   // fprintf(stdout, "DDC packet error status codes with non-zero counts:  %s\n",
   fprintf(stdout, "%s:  %s\n",
           title,
           (keyct == 0) ? "None" : "");
   if (keyct > 0) {
      qsort(keysp, keyct, sizeof(gpointer), compare);    // sort keys
      fprintf(stdout, "Count   Status Code                          Description\n");
      int ndx;
      for (ndx=0; ndx<keyct; ndx++) {
         gpointer keyp = keysp[ndx];
         long key = GPOINTER_TO_INT(keyp);                  // Public_Status_Code
         // DBGMSF(debug, "key:  %d   %p", key, keyp);
         // wrong hunch about bug
         // if (key == 0) {
         //    DBGMSG("=====> Invalid status code key = %d", key);
         //    break;
         // }
         assert( GINT_TO_POINTER(key) == keyp);

         int ct  = GPOINTER_TO_INT(g_hash_table_lookup(pcounts->error_counts_hash,keyp));
         summed_ct += ct;
         // fprintf(stdout, "%4d    %6d\n", ct, key);

         Status_Code_Info * desc = find_status_code_info(key);

         // or consider flags in Status_Code_Info with this information
         char * aux_msg = "";
         if (ddcrc_is_derived_status_code(key))
            aux_msg = " (derived)";
         else if (ddcrc_is_not_error(key))
            aux_msg = " (not an error)";
         fprintf(stdout, "%5d   %-28s (%5ld) %s %s\n",
              ct,
              (desc) ? desc->name : "",
              key,
              (desc) ? desc->description : "",
              aux_msg
             );
      }
   }
   printf("Total errors: %d\n", pcounts->total_status_counts);
   assert(summed_ct == pcounts->total_status_counts);
   g_free(keysp);
   // fprintf(stdout,"\n");
   DBGMSF(debug, "Done");
}


/** Master function to display status counts
 */
void show_all_status_counts() {
   show_specific_status_counts(primary_error_code_counts);
   // show_specific_status_counts(secondary_status_code_counts);    // not used

   rpt_nl();
   show_specific_status_counts(retryable_error_code_counts);
}


static
int get_true_io_error_count(Status_Code_Counts * pcounts) {
   assert(pcounts->error_counts_hash);
     unsigned int keyct;

     // g_hash_table_get_keys_as_array() new in v 2.40, which is not later than the version
     // in all but the most recent distros, .e.g SUSE 13.2 is v 2.18
     // gpointer * keysp = g_hash_table_get_keys_as_array(pcounts->error_counts_hash, &keyct);
     GList * glist = g_hash_table_get_keys(pcounts->error_counts_hash);
     gpointer * keysp = g_list_to_g_array(glist, &keyct);
     g_list_free(glist);

     int summed_ct = 0;

     int ndx;
     for (ndx=0; ndx<keyct; ndx++) {
        gpointer keyp = keysp[ndx];
        int key = GPOINTER_TO_INT(keyp);
        // TODO: filter out DDCRC_NULL_RESPONSE, perhaps others DDCRC_UNSUPPORTED
        int ct  = GPOINTER_TO_INT(g_hash_table_lookup(pcounts->error_counts_hash,GINT_TO_POINTER(key)));
        summed_ct += ct;
     }
     // DBGMSG("Total errors: %d", total_counts);
     assert(summed_ct == pcounts->total_status_counts);
     g_free(keysp);
     return summed_ct;
}


//
// Sleep Strategy
//

// names for Sleep_Event enum values
static const char * sleep_event_names[] = {
      "SE_WRITE_TO_READ",
      "SE_POST_OPEN",
      "SE_POST_WRITE",
      "SE_POST_READ",
      "SE_DDC_NULL",
      "SE_POST_SAVE_SETTINGS",
      "SE_PRE_EDID",
      "SE_PRE_MULTI_PART_READ",
      "SE_MULTI_PART_READ_TO_WRITE",
      "SE_OTHER",
      "SE_SPECIAL",
     };
#define SLEEP_EVENT_ID_CT (sizeof(sleep_event_names)/sizeof(char *))

int max_sleep_event_name_size() {
   int result = 0;
   for (int ndx = 0; ndx < SLEEP_EVENT_ID_CT; ndx++) {
      if (strlen(sleep_event_names[ndx]) > result)
         result = strlen(sleep_event_names[ndx]);
   }
   return result;
}

/** Returns the name of sleep event type */
const char * sleep_event_name(Sleep_Event_Type event_type) {
   // ensure sleep_event_names stays in sync with Sync_Event_Type
   const int sleep_event_type_count = SE_SPECIAL+1;   // relies on values in enum assigned from 0
   assert( SLEEP_EVENT_ID_CT ==  sleep_event_type_count);
   return sleep_event_names[event_type];
}

static int sleep_event_cts_by_id[SLEEP_EVENT_ID_CT];
static int total_sleep_event_ct = 0;

static GMutex sleep_stats_mutex;


void reset_sleep_event_counts() {
   bool debug = false || debug_sleep_stats_mutex;
   DBGMSF(debug, "Starting");

   g_mutex_lock(&sleep_stats_mutex);
   for (int ndx = 0; ndx < SLEEP_EVENT_ID_CT; ndx++) {
      sleep_event_cts_by_id[ndx] = 0;
   }
   g_mutex_unlock(&sleep_stats_mutex);

   DBGMSF(debug, "Done");
}

void record_sleep_event(Sleep_Event_Type event_type) {
   // For better performance, separate mutex for each index in array
   g_mutex_lock(&sleep_stats_mutex);
   sleep_event_cts_by_id[event_type]++;
   total_sleep_event_ct++;
   g_mutex_unlock(&sleep_stats_mutex);
}


/** Reports execution statistics.
 *
 * @param depth logical indentation depth
 */
void report_execution_stats(int depth) {
   int sleep_name_field_size = max_sleep_event_name_size();
   int d1 = depth+1;
   rpt_title("IO and Sleep Events:", depth);
   rpt_vstring(d1, "Total IO events:      %5d", total_io_event_count());
   rpt_vstring(d1, "IO error count:       %5d", get_true_io_error_count(primary_error_code_counts));
   rpt_vstring(d1, "Total sleep events:   %5d", total_sleep_event_ct);
   rpt_nl();
   rpt_title("Sleep Event type      Count", d1);
   for (int id=0; id < SLEEP_EVENT_ID_CT; id++) {
      rpt_vstring(d1, "%-*s  %4d", sleep_name_field_size, sleep_event_names[id], sleep_event_cts_by_id[id]);
   }
}


//
// Module initialization
//

/** Initializes execution stats module.
 *
 * Must be called once at program startup.
 */
void init_execution_stats() {
   primary_error_code_counts   = new_status_code_counts("DDC Related Errors");
   retryable_error_code_counts = new_status_code_counts("Errors Wrapped in Retry");
   // secondary_status_code_counts = new_status_code_counts("Derived and Other Errors");
   program_start_timestamp     = cur_realtime_nanosec();
   resettable_start_timestamp  = program_start_timestamp;
   elapsed_time_nanosec();    // first call initializes, used for dbgtrc
}


/** Resets collected execution statistics */
void reset_execution_stats() {
   bool debug = false || debug_global_stats_mutex;
   DBGMSF(debug, "Starting");

   reset_sleep_event_counts();
   reset_status_code_counts();
   reset_io_event_stats();

   g_mutex_lock(&global_stats_mutex);
   resettable_start_timestamp = cur_realtime_nanosec();
   g_mutex_unlock(&global_stats_mutex);

   DBGMSF(debug, "Done");
}


/** Reports elapsed time statistics.
 *
 *  @param depth logical indentation depth
 *
 *  If a statistics reset has occurred, reports both the time
 *  since reset and total elapsed time.
 */
void report_elapsed_stats(int depth) {
   uint64_t end_nanos = cur_realtime_nanosec();
   if (program_start_timestamp != resettable_start_timestamp) {
      uint64_t cur_elapsed_nanos = end_nanos - resettable_start_timestamp;
      rpt_vstring(depth,
            "Elapsed milliseconds since last reset (nanosec):%10"PRIu64"  (%13"PRIu64")",
            cur_elapsed_nanos / (1000*1000),
            cur_elapsed_nanos);
   }

   uint64_t elapsed_nanos = end_nanos - program_start_timestamp;
   rpt_vstring(depth,
         "Total elapsed milliseconds (nanoseconds):          %10"PRIu64"  (%13"PRIu64")",
         elapsed_nanos / (1000*1000),
         elapsed_nanos);
}
