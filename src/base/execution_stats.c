/*  execution_stats.c
 *
 *  Created on: Oct 31, 2015
 *      Author: rock
 *
 *  For recording and reporting the count and elapsed time of system calls.
 */

#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <util/report_util.h>
#include <base/common.h>   // for sleep functions
#include <base/parms.h>

#include <base/execution_stats.h>

// Forward References
void note_io_event_time(IO_Event_Type event_type, const char * location, long when_nanos);
void    init_status_counts();
int get_true_io_error_count();

// There are 2 collections of data structures:
//    - for recording the total number of calls and elapsed time
//    - relating to sleep interval management
//
// TODO: remove redundancy of these 2 sets of data structures that had different origins

#ifdef OLD
// names for IO_Event_Type enum values
static const char * io_event_names[] = {
      "IE_WRITE" ,
      "IE_READ",
      "IE_WRITE_READ",
      "IE_OPEN",
      "IE_CLOSE",
      "IE_OTHER"};
// #define IO_EVENT_ID_CT (sizeof(io_event_names) / sizeof(char *))
#endif



typedef struct {
   IO_Event_Type  id;
   const char *   name;
   const char *   desc;
   int            call_count;
   long           call_nanosec;
} IO_Event_Type_Stats;

IO_Event_Type_Stats io_event_stats[] = {
      {IE_WRITE,      "IE_WRITE",      "write calls",       0, 0},
      {IE_READ,       "IE_READ",       "read calls",        0, 0},
      {IE_WRITE_READ, "IE_WRITE_READ", "write/read calls",  0, 0},
      {IE_OPEN,       "IE_OPEN",       "open file calls",   0, 0},
      {IE_CLOSE,      "IE_CLOSE",      "close file calls",  0, 0},
      {IE_OTHER,      "IE_OTHER",      "other I/O calls",   0, 0},
};
#define IO_EVENT_TYPE_CT (sizeof(io_event_stats)/sizeof(IO_Event_Type_Stats))
// assert( IO_EVENT_ID_CT == IO_EVENT_TYPE_CT );   // TEMP, transitional

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



// names for Sleep_Event enum values
static const char * sleep_event_names[] = {
      "SE_WRITE_TO_READ",
      "SE_POST_OPEN",
      "SE_POST_WRITE"};
#define SLEEP_EVENT_ID_CT (sizeof(sleep_event_names)/sizeof(char *))

// TODO: information overlap with DDC_Call_Stats, combine
// static int io_event_cts_by_id[IO_EVENT_ID_CT];
static int sleep_event_cts_by_id[SLEEP_EVENT_ID_CT];


const char * sleep_event_name(Sleep_Event_Type event_type) {
   return sleep_event_names[event_type];
}


// static int            total_io_event_ct = 0;  // unused


static int            total_io_error_ct = 0;
static int            total_sleep_event_ct = 0;
static IO_Event_Type  last_io_event;
static long           last_io_timestamp = -1;
static long           program_start_timestamp;



typedef struct {
   long   total_call_nanosecs;
   int    total_call_ct;
   char*  stat_name;
} Single_Call_Stat;

typedef struct {
   Single_Call_Stat * pread_write_stats;
   Single_Call_Stat * popen_stats;
   Single_Call_Stat * pclose_stats;
   Single_Call_Stat * pother_stats;
   bool               stats_active;
} DDC_Call_Stats;


static DDC_Call_Stats ddc_call_stats;

Single_Call_Stat *  init_one_call_stat(char * name) {
   Single_Call_Stat * pstats = calloc(1,sizeof(Single_Call_Stat));
   pstats->total_call_nanosecs = 0;
   pstats->total_call_ct       = 0;
   pstats->stat_name           = name;
   return pstats;
}

void init_ddc_call_stats() {
   ddc_call_stats.pread_write_stats = init_one_call_stat("read/write");
   ddc_call_stats.popen_stats       = init_one_call_stat("open");
   ddc_call_stats.pclose_stats      = init_one_call_stat("close");
   ddc_call_stats.pother_stats      = init_one_call_stat("other");
   ddc_call_stats.stats_active      = true;     // TODO: figure out proper way to set only if /dev/i2c* exists
}


/* Module initialization
 */
void init_call_stats() {
   init_ddc_call_stats();
   program_start_timestamp = cur_realtime_nanosec();
   init_status_counts();
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
void log_io_event(
        const IO_Event_Type  event_type,
        const char *         location,
        long                 start_time_nanos,
        long                 end_time_nanos)
{
   // printf("(%s) event_type=%d %s\n", __func__, event_type, io_event_name(event_type));

   Single_Call_Stat * which_stat;
   switch (event_type) {
   case(IE_WRITE):
   case(IE_READ):
   case(IE_WRITE_READ):
      which_stat = ddc_call_stats.pread_write_stats;
      break;
   case(IE_OPEN):
      which_stat = ddc_call_stats.popen_stats;
      break;
   case(IE_CLOSE):
      which_stat = ddc_call_stats.pclose_stats;
      break;
   default:
      which_stat = ddc_call_stats.pother_stats;
   }
   // printf("(%s) which_stat=%p\n", __func__, which_stat);
   which_stat->total_call_ct++;
   which_stat->total_call_nanosecs += (end_time_nanos-start_time_nanos);

   note_io_event_time(event_type, location, end_time_nanos);


   // new way:
   long elapsed_nanos = (end_time_nanos-start_time_nanos);
   io_event_stats[event_type].call_count++;
   io_event_stats[event_type].call_nanosec += elapsed_nanos;

   last_io_event = event_type;
   last_io_timestamp = normalize_timestamp(end_time_nanos);
}

void report_call_stats_old(int depth);

void report_call_stats(int depth) {
   // report_call_stats_old(depth);    // for comparing
   int d1 = depth+1;
   rpt_title("Call Stats (new):", depth);
   int total_ct = 0;
   long total_nanos = 0;
   int ndx = 0;
   // int max_name_length = max_event_name_length();
   // not working as variable length string specifier
   // printf("(%s) max_name_length=%d\n", __func__, max_name_length);
   rpt_vstring(d1, "%-40s Count Millisec (   Nanosec)", "Type");
   for (;ndx < IO_EVENT_TYPE_CT; ndx++) {
      if (io_event_stats[ndx].call_count > 0) {
         IO_Event_Type_Stats* curstat = &io_event_stats[ndx];
         char buf[100];
         snprintf(buf, 100, "%-17s (%s)", curstat->desc, curstat->name);
         rpt_vstring(d1, "%-40s  %4d  %7ld (%10ld)",
                     buf,
                     curstat->call_count,
                     curstat->call_nanosec / (1000*1000),
                     curstat->call_nanosec
                    );
         total_ct += curstat->call_count;
         total_nanos += curstat->call_nanosec;
      }
   }
   rpt_vstring(d1, "%-40s  %4d  %7ld (%10ld)",
               "Totals:",
               total_ct,
               total_nanos / (1000*1000),
               total_nanos
              );

}


void report_one_call_stat(Single_Call_Stat * pstats, int depth) {
   if (pstats) {

      rpt_vstring(depth, "Total %-10s calls:                        %7d",
                  pstats->stat_name, pstats->total_call_ct);
      rpt_vstring(depth, "Total %-10s call milliseconds (nanosec):  %7ld  (%10ld)",
                  pstats->stat_name,
                  pstats->total_call_nanosecs / (1000*1000),
                  pstats->total_call_nanosecs);
   }

}


void report_call_stats_old(int depth) {

   int d1 = depth+1;
   if (ddc_call_stats.stats_active) {
      rpt_title("Call Stats:", depth);
      report_one_call_stat(ddc_call_stats.pread_write_stats, d1);
      report_one_call_stat(ddc_call_stats.popen_stats,       d1);
      report_one_call_stat(ddc_call_stats.pclose_stats,      d1);
      report_one_call_stat(ddc_call_stats.pother_stats,      d1);
   }
}




// TODO:
// maintain count of sleep events by type
// report them

// Eventually: adjust timeout based on error event frequency


void note_io_event_time(const IO_Event_Type event_type, const char * location, long when_nanos) {
   bool debug = false;

   // total_io_event_ct++;
   last_io_event = event_type;
   last_io_timestamp = normalize_timestamp(when_nanos);
   // io_event_cts_by_id[event_type]++;

   if (debug)
      printf("(%s) timestamp=%11ld, event_type=%s, location=%s\n",
             __func__, last_io_timestamp, io_event_name(event_type), location);
}

#ifdef UNUSED
// convenience function
void note_io_event(const IO_Event_Type event_type, const char * location){
   long cur_timestamp = cur_realtime_nanosec();
   note_io_event_time(event_type, location, cur_timestamp);
}
#endif


// Design: IO errors are noted in the function that sets gsc negative,
// do not leave it to caller to set.  That way do not need to keep track
// if a called function has already set.
//
// BUT: status codes are not noted until they are modulated to Global_Status_Code


// TODO: overlaps with record_status_code_occurrence() in status_code_mgt.h
// combine function, or at least reduce to 1 call
// Or eliminate.  There's no additional information maintained here
// BUT: need to know this information to get error rates for adjusting sleep time
// also overlaps with ddcmsg in msg_control.c  can calls be combined?
// are all ddcmsg calls reflective of io errors?
// add function is_ddcmsg_gdc_an_io_error()
void note_io_error(Global_Status_Code gsc, const char * location) {
   bool debug = false;

   total_io_error_ct++;

   if (debug)
      printf("(%s) IO error: %s\n", __func__, global_status_code_description(gsc));
}


//
// Sleep Strategy
//

// Convenience functions

void call_tuned_sleep_i2c(Sleep_Event_Type event_type) {
   call_tuned_sleep(DDC_IO_DEVI2C, event_type);
}

void call_tuned_sleep_adl(Sleep_Event_Type event_type) {
   call_tuned_sleep(DDC_IO_ADL, event_type);

}

void call_tuned_sleep_dh(Display_Handle* dh, Sleep_Event_Type event_type) {
   call_tuned_sleep(dh->ddc_io_mode, event_type);
}


// TODO: Extend to take account of actual time since return from
// last system call, previous error rate, etc.

void call_tuned_sleep(DDC_IO_Mode io_mode, Sleep_Event_Type event_type) {
   int sleep_time_millis = 0;    // should be a default
   if (io_mode == DDC_IO_DEVI2C) {
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
   }
   else {    // DDC_IO_ADL
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

   printf("   Total IO events:    %5d\n", total_io_event_count());
   printf("   IO error count:     %5d\n", get_true_io_error_count());
   printf("   Total sleep events: %5d\n", total_sleep_event_ct);


   int id;
#ifdef OLD
   puts("");
   // printf("   IO Events by type:\n");
   printf("   IO Event type       Count\n");
   for (id=0; id < IO_EVENT_TYPE_CT; id++) {
      printf("   %-20s  %3d\n", io_event_stats[id].name, io_event_stats[id].call_count);
   }
#endif

   puts("");
   printf("   Sleep Event type    Count\n");
   for (id=0; id < SLEEP_EVENT_ID_CT; id++) {
      printf("   %-20s  %3d\n", sleep_event_names[id], sleep_event_cts_by_id[id]);
   }
}


// moved from status_code_mgt.c


//
// Record status code occurrence counts
//

static GHashTable * error_counts_hash = NULL;
static int total_counts = 0;


int record_status_code_occurrence(int rc, const char * caller_name) {
   bool debug = false;
   if (debug)
      printf("(%s) caller=%s, rc=%d\n", __func__, caller_name, rc);
   assert(error_counts_hash);
   total_counts++;

   // n. if key rc not found, returns NULL, which is 0
   int ct = GPOINTER_TO_INT(g_hash_table_lookup(error_counts_hash,  GINT_TO_POINTER(rc)) );
   g_hash_table_insert(error_counts_hash, GINT_TO_POINTER(rc), GINT_TO_POINTER(ct+1));
   // printf("(%s) Old count=%d\n", __func__, ct);

   // check the new value
   int newct = GPOINTER_TO_INT(g_hash_table_lookup(error_counts_hash,  GINT_TO_POINTER(rc)) );
   // printf("(%s) new count for key %d = %d\n", __func__, rc, newct);
   assert(newct == ct+1);

   return ct+1;
}


// Used by qsort in show_status_counts()
int compare( const void* a, const void* b)
{
     int int_a = * ( (int*) (a) );
     int int_b = * ( (int*) (b) );

     if ( int_a == int_b ) return 0;
     else if ( int_a < int_b ) return 1;
     else return -1;
}


void show_status_counts() {
   assert(error_counts_hash);
   unsigned int keyct;
   gpointer * keysp = g_hash_table_get_keys_as_array(error_counts_hash, &keyct);
   int summed_ct = 0;
   fprintf(stdout, "DDC packet error status codes with non-zero counts:  %s\n",
           (keyct == 0) ? "None" : "");
   if (keyct > 0) {
      qsort(keysp, keyct, sizeof(gpointer), compare);    // sort keys
      fprintf(stdout, "Count   Status Code                       Description\n");
#ifdef OLD
      Status_Code_Info default_description;
#endif
      int ndx;
      for (ndx=0; ndx<keyct; ndx++) {
         gpointer keyp = keysp[ndx];
         int key = GPOINTER_TO_INT(keyp);
         int ct  = GPOINTER_TO_INT(g_hash_table_lookup(error_counts_hash,GINT_TO_POINTER(key)));
         summed_ct += ct;
         // fprintf(stdout, "%4d    %6d\n", ct, key);

         Status_Code_Info * desc = find_global_status_code_description(key);


#ifdef OLD
         Retcode_Range_Id rc_range = get_modulation(key);
         Retcode_Description_Finder desc_finder = retcode_range_table[rc_range].desc_finder;
         Status_Code_Info * desc = NULL;
         if (desc_finder) {
            int search_key = key;
            bool value_is_modulated = retcode_range_table[rc_range].finder_arg_is_modulated;
            if (!value_is_modulated) {
               search_key = demodulate_rc(key, rc_range);
            }
            desc = desc_finder(search_key);
            if (!desc) {
               desc = &default_description;
               desc->code = key;
               desc->name = "";
               desc->description = "unrecognized status code";
            }
         }
         else {     // no finder
            desc = &default_description;
            desc->code = key;
            desc->name = "";
            desc->description = "(status code not in interpretable range)";
         }
#endif
         fprintf(stdout, "%5d   %-25s (%5d) %s\n",
              ct,
              desc->name,
              key,
              desc->description
             );
      }
   }
   printf("Total errors: %d\n", total_counts);
   assert(summed_ct == total_counts);
   g_free(keysp);
   fprintf(stdout,"\n");
}

int get_true_io_error_count() {
   assert(error_counts_hash);
     unsigned int keyct;
     gpointer * keysp = g_hash_table_get_keys_as_array(error_counts_hash, &keyct);
     int summed_ct = 0;

     int ndx;
     for (ndx=0; ndx<keyct; ndx++) {
        gpointer keyp = keysp[ndx];
        int key = GPOINTER_TO_INT(keyp);
        // TODO: filter out DDCRC_NULL_RESPONSE, perhaps others DDCRC_UNSUPPORTED
        int ct  = GPOINTER_TO_INT(g_hash_table_lookup(error_counts_hash,GINT_TO_POINTER(key)));
        summed_ct += ct;
     }
     // printf("(%s) Total errors: %d\n", __func__, total_counts);
     assert(summed_ct == total_counts);
     g_free(keysp);
     return summed_ct;
}

#ifdef FUTURE
int get_status_code_count(int rc) {
   // *** TODO ***
   return 0;
}

void reset_status_code_counts() {
   // *** TODO ***
}
#endif

void init_status_counts() {
   error_counts_hash = g_hash_table_new(NULL,NULL);

}


