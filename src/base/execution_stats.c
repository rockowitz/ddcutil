/* execution_stats.c
 *
 * For recording and reporting the count and elapsed time of system calls.
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/glib_util.h"
#include "util/report_util.h"

#include "base/sleep.h"
#include "base/parms.h"
#include "base/ddc_errno.h"

#include "base/execution_stats.h"


//
// Typedefs
//

typedef struct {
   IO_Event_Type  id;
   const char *   name;
   const char *   desc;
   int            call_count;
   long           call_nanosec;
} IO_Event_Type_Stats;


#define STATUS_CODE_COUNTS_MARKER "SCCT"
typedef struct {
   char marker[4];
   GHashTable * error_counts_hash;
   int total_status_counts;
   char * name;
} Status_Code_Counts;


//
// Global Variables
//

static IO_Event_Type        last_io_event;
static long                 last_io_timestamp = -1;
static long                 program_start_timestamp;
static Status_Code_Counts * primary_error_code_counts;


//
// IO Event Tracking
//

IO_Event_Type_Stats io_event_stats[] = {
      {IE_WRITE,      "IE_WRITE",      "write calls",       0, 0},
      {IE_READ,       "IE_READ",       "read calls",        0, 0},
      {IE_WRITE_READ, "IE_WRITE_READ", "write/read calls",  0, 0},
      {IE_OPEN,       "IE_OPEN",       "open file calls",   0, 0},
      {IE_CLOSE,      "IE_CLOSE",      "close file calls",  0, 0},
      {IE_OTHER,      "IE_OTHER",      "other I/O calls",   0, 0},
};
#define IO_EVENT_TYPE_CT (sizeof(io_event_stats)/sizeof(IO_Event_Type_Stats))


const char * io_event_name(IO_Event_Type event_type) {
   // return io_event_names[event_type];
   return io_event_stats[event_type].name;
}


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


int total_io_event_count() {
   int total = 0;
   int ndx = 0;
   for (;ndx < IO_EVENT_TYPE_CT; ndx++)
      total += io_event_stats[ndx].call_count;
   return total;
}


long total_io_event_nanosec() {
   long total = 0;
   int ndx = 0;
   for (;ndx < IO_EVENT_TYPE_CT; ndx++)
      total += io_event_stats[ndx].call_nanosec;
   return total;
}


// No effect on program logic, but makes debug messages easier to scan
long normalize_timestamp(long timestamp) {
   return timestamp - program_start_timestamp;
}


/* Called immediately after an I2C IO call, this function updates
 * two sets of data:
 *
 * 1) Updates the total number of calls and elapsed time for
 * categories of calls.
 *
 * 2) Updates the timestamp and call type maintained for the
 * most recent I2C call.  This information is used to determine
 * the required time for the next sleep call.
 *
 * Arguments:
 *    event_type
 *    location          function name
 *    start_time_nanos
 *    end_time_nanos
 *
 * Returns:
 *    nothing
 */
void log_io_call(
        const IO_Event_Type  event_type,
        const char *         location,
        long                 start_time_nanos,
        long                 end_time_nanos)
{
   bool debug = false;
   if (debug)
      DBGMSG("event_type=%d %s", event_type, io_event_name(event_type));

   long elapsed_nanos = (end_time_nanos-start_time_nanos);
   io_event_stats[event_type].call_count++;
   io_event_stats[event_type].call_nanosec += elapsed_nanos;

   last_io_event = event_type;
   last_io_timestamp = normalize_timestamp(end_time_nanos);
}


void report_io_call_stats(int depth) {
   int d1 = depth+1;
   rpt_title("Call Stats:", depth);
   int total_ct = 0;
   long total_nanos = 0;
   int ndx = 0;
   // int max_name_length = max_event_name_length();
   // not working as variable length string specifier
   // DBGMSG("max_name_length=%d", max_name_length);
   rpt_vstring(d1, "%-40s Count Millisec  (   Nanosec)", "Type");
   for (;ndx < IO_EVENT_TYPE_CT; ndx++) {
      if (io_event_stats[ndx].call_count > 0) {
         IO_Event_Type_Stats* curstat = &io_event_stats[ndx];
         char buf[100];
         snprintf(buf, 100, "%-17s (%s)", curstat->desc, curstat->name);
         rpt_vstring(d1, "%-40s  %4d  %7ld  (%10ld)",
                     buf,
                     curstat->call_count,
                     curstat->call_nanosec / (1000*1000),
                     curstat->call_nanosec
                    );
         total_ct += curstat->call_count;
         total_nanos += curstat->call_nanosec;
      }
   }
   rpt_vstring(d1, "%-40s  %4d  %7ld  (%10ld)",
               "Totals:",
               total_ct,
               total_nanos / (1000*1000),
               total_nanos
              );
}


//
// Status Code Occurrence Tracking
//

// Design: IO errors are noted in the function that sets gsc negative,
// do not leave it to caller to set.  That way do not need to keep track
// if a called function has already set.
//
// BUT: status codes are not noted until they are modulated to Global_Status_Code

Status_Code_Counts * new_status_code_counts(char * name) {
   Status_Code_Counts * pcounts = calloc(1,sizeof(Status_Code_Counts));
   memcpy(pcounts->marker, STATUS_CODE_COUNTS_MARKER, 4);
   pcounts->error_counts_hash =  g_hash_table_new(NULL,NULL);
   pcounts->total_status_counts = 0;
   if (name)
      pcounts->name = strdup(name);
   return pcounts;
}


int log_any_status_code(Status_Code_Counts * pcounts, int rc, const char * caller_name) {
   bool debug = false;
   DBGMSF(debug, "caller=%s, rc=%d", caller_name, rc);
   assert(pcounts->error_counts_hash);
   pcounts->total_status_counts++;

   if (rc == 0) {
      DBGMSG("Called with rc = 0, from function %s", caller_name);
   }

   // n. if key rc not found, returns NULL, which is 0
   int ct = GPOINTER_TO_INT(g_hash_table_lookup(pcounts->error_counts_hash,  GINT_TO_POINTER(rc)) );
   g_hash_table_insert(pcounts->error_counts_hash, GINT_TO_POINTER(rc), GINT_TO_POINTER(ct+1));
   // DBGMSG("Old count=%d", ct);

   // check the new value
   int newct = GPOINTER_TO_INT(g_hash_table_lookup(pcounts->error_counts_hash,  GINT_TO_POINTER(rc)) );
   // DBGMSG("new count for key %d = %d", rc, newct);
   assert(newct == ct+1);

   return ct+1;
}


int log_status_code(int rc, const char * caller_name) {
   Status_Code_Counts * pcounts = primary_error_code_counts;
   // if ( ddcrc_is_derived_status_code(rc) )
   //    pcounts = secondary_status_code_counts;
   log_any_status_code(pcounts, rc, caller_name);
   return rc;
}


// Used by qsort in show_specific_status_counts()
int compare( const void* a, const void* b)
{
     int int_a = * ( (int*) (a) );
     int int_b = * ( (int*) (b) );

     if ( int_a == int_b ) return 0;
     else if ( int_a < int_b ) return 1;
     else return -1;
}


void show_specific_status_counts(Status_Code_Counts * pcounts) {
   bool debug = true;
   DBGMSF(debug, "Starting");
   if (pcounts->name)
      printf("%s:\n", pcounts->name);
   assert(pcounts->error_counts_hash);
   unsigned int keyct;

   // g_hash_table_get_keys_as_array() new in v 2.40, which is not later than the version
   // in all but the most recent distros, .e.g SUSE 13.2 is v 2.18
   // gpointer * keysp = g_hash_table_get_keys_as_array(pcounts->error_counts_hash, &keyct);
   GList * glist = g_hash_table_get_keys(pcounts->error_counts_hash);
   gpointer * keysp = g_list_to_g_array(glist, &keyct);
   if (debug) {
      DBGMSG("Keys.  keyct=%d", keyct);
      for (int ndx = 0; ndx < keyct; ndx++) {
         DBGMSG( "keysp[%d]:  %d   %p  %d",
                 ndx,  keysp[ndx], keysp[ndx], GPOINTER_TO_INT(keysp[ndx]) );
      }
   }
   int summed_ct = 0;
   // fprintf(stdout, "DDC packet error status codes with non-zero counts:  %s\n",
   fprintf(stdout, "DDC Related Errors:  %s\n",
           (keyct == 0) ? "None" : "");
   if (keyct > 0) {
      qsort(keysp, keyct, sizeof(gpointer), compare);    // sort keys
      fprintf(stdout, "Count   Status Code                          Description\n");
      int ndx;
      for (ndx=0; ndx<keyct; ndx++) {
         gpointer keyp = keysp[ndx];
         long key = GPOINTER_TO_INT(keyp);
         // DBGMSF(debug, "key:  %d   %p", key, keyp);
         if (key == 0) {
            DBGMSG("=====> Invalid status code key = %d", key);
            break;
         }
         assert( GINT_TO_POINTER(key) == keyp);

         int ct  = GPOINTER_TO_INT(g_hash_table_lookup(pcounts->error_counts_hash,keyp));
         summed_ct += ct;
         // fprintf(stdout, "%4d    %6d\n", ct, key);

         Status_Code_Info * desc = find_global_status_code_info(key);

         // or consider flags in Status_Code_Info with this information
         char * aux_msg = "";
         if (ddcrc_is_derived_status_code(key))
            aux_msg = " (derived)";
         else if (ddcrc_is_not_error(key))
            aux_msg = " (not an error)";
         fprintf(stdout, "%5d   %-28s (%5ld) %s %s\n",
              ct,
              desc->name,
              key,
              desc->description,
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


void show_all_status_counts() {
   show_specific_status_counts(primary_error_code_counts);
   // show_specific_status_counts(secondary_status_code_counts);    // not used
}


int get_true_io_error_count(Status_Code_Counts * pcounts) {
   assert(pcounts->error_counts_hash);
     unsigned int keyct;

     // g_hash_table_get_keys_as_array() new in v 2.40, which is not later than the version
     // in all but the most recent distros, .e.g SUSE 13.2 is v 2.18
     // gpointer * keysp = g_hash_table_get_keys_as_array(pcounts->error_counts_hash, &keyct);
     GList * glist = g_hash_table_get_keys(pcounts->error_counts_hash);
     gpointer * keysp = g_list_to_g_array(glist, &keyct);

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
      "SE_POST_READ"
     };
#define SLEEP_EVENT_ID_CT (sizeof(sleep_event_names)/sizeof(char *))


const char * sleep_event_name(Sleep_Event_Type event_type) {
   return sleep_event_names[event_type];
}

static int sleep_event_cts_by_id[SLEEP_EVENT_ID_CT];
static int total_sleep_event_ct = 0;
static int sleep_strategy = 0;

// TODO: create table of sleep strategy number, description

bool set_sleep_strategy(int strategy) {
   if (strategy == -1)    // if unset
      strategy = 0;       // use default strategy
   bool result = false;
   if (strategy >= 0 && strategy <= 2) {
      sleep_strategy = strategy;
      result = true;
   }
   return result;
}

int get_sleep_strategy() {
   return sleep_strategy;
}

char * sleep_strategy_desc(int sleep_strategy) {
   char * result = NULL;
   switch(sleep_strategy) {
   case (0):
      result = "Default";
      break;
   case (1):
      result = "Half sleep time";
      break;
   case (2):
      result = "Double sleep time";
      break;
   default:
      result = NULL;
   }
   return result;
}


// Convenience functions
void call_tuned_sleep_i2c(Sleep_Event_Type event_type) {
   call_tuned_sleep(DDC_IO_DEVI2C, event_type);
}
void call_tuned_sleep_adl(Sleep_Event_Type event_type) {
   call_tuned_sleep(DDC_IO_ADL, event_type);
}
void call_tuned_sleep_dh(Display_Handle* dh, Sleep_Event_Type event_type) {
   call_tuned_sleep(dh->io_mode, event_type);
}


// TODO: Extend to take account of actual time since return from
// last system call, previous error rate, etc.

void call_tuned_sleep(DDCA_IO_Mode io_mode, Sleep_Event_Type event_type) {
   int sleep_time_millis = 0;    // should be a default
   switch(io_mode) {

   case DDC_IO_DEVI2C:
      switch(event_type) {
      case (SE_WRITE_TO_READ):
            sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
            switch(sleep_strategy) {
            case (1):
               sleep_time_millis = sleep_time_millis/2;
               break;
            case (2):
               sleep_time_millis = sleep_time_millis*2;
               break;
            default:
               break;
            }
            break;
      case (SE_POST_WRITE):
            sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
            switch(sleep_strategy) {
            case (1):
               sleep_time_millis = sleep_time_millis/2;
               break;
            case (2):
               sleep_time_millis = sleep_time_millis*2;
               break;
            default:
               break;
            }
            break;
      case (SE_POST_OPEN):
            sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
            break;
      case (SE_POST_READ):
            sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
            break;
      default:
         sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
      }  // switch within DDC_IO_DEVI2C
      break;

   case DDC_IO_ADL:
      switch(event_type) {
      case (SE_WRITE_TO_READ):
            sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
            break;
      case (SE_POST_WRITE):
            sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
            break;
      case (SE_POST_OPEN):
            sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
            break;
      default:
         sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
      }
      break;

   case USB_IO:
      printf("(%s) call_tuned_sleep() called for USB_IO\n", __func__);
      break;

   }

   // TODO:
   //   get error rate (total calls, total errors), current adjustment value
   //   adjust by time since last i2c event
   // Is tracing useful, given that we know the event type?
   // void sleep_millis_with_trace(int milliseconds, const char * caller_location, const char * message);

   sleep_event_cts_by_id[event_type]++;
   total_sleep_event_ct++;
   sleep_millis(sleep_time_millis);
}


void report_sleep_strategy_stats(int depth) {
   // TODO: implement depth
   printf("Sleep Strategy Stats:\n");
   printf("   Total IO events:     %5d\n", total_io_event_count());
   printf("   IO error count:      %5d\n", get_true_io_error_count(primary_error_code_counts));
   printf("   Total sleep events:  %5d\n", total_sleep_event_ct);
   puts("");
   printf("   Sleep Event type     Count\n");
   for (int id=0; id < SLEEP_EVENT_ID_CT; id++) {
      printf("   %-20s  %4d\n", sleep_event_names[id], sleep_event_cts_by_id[id]);
   }
}


//
// Module initialization
//

void init_execution_stats() {
   primary_error_code_counts = new_status_code_counts(NULL);
   // secondary_status_code_counts = new_status_code_counts("Derived and Other Errors");
   program_start_timestamp = cur_realtime_nanosec();
}

