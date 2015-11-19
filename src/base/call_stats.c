/*  call_stats.c
 *
 *  Created on: Oct 31, 2015
 *      Author: rock
 *
 *  For recording and reporting the count and elapsed time of system calls.
 */

#include <stdio.h>
#include <stdlib.h>

#include <util/report_util.h>
#include <base/common.h>   // for sleep functions
#include <base/parms.h>

#include <base/call_stats.h>

// Forward References
void note_io_event_time(IO_Event_Type event_type, const char * location, long when_nanos);

// There are 2 collections of data structures:
//    - for recording the total number of calls and elapsed time
//    - relating to sleep interval management
//
// TODO: remove redundancy of these 2 sets of data structures that had different origins

// names for IO_Event_Type enum values
static const char * io_event_names[] = {
      "IE_WRITE" ,
      "IE_READ",
      "IE_WRITE_READ",
      "IE_OPEN",
      "IE_CLOSE",
      "IE_OTHER"};
#define IO_EVENT_ID_CT (sizeof(io_event_names)/sizeof(char *))

// names for Sleep_Event enum values
static const char * sleep_event_names[] = {
      "SE_WRITE_TO_READ",
      "SE_POST_OPEN",
      "SE_POST_WRITE"};
#define SLEEP_EVENT_ID_CT (sizeof(sleep_event_names)/sizeof(char *))

// TODO: information overlap with DDC_Call_Stats, combine
static int io_event_cts_by_id[IO_EVENT_ID_CT];
static int sleep_event_cts_by_id[SLEEP_EVENT_ID_CT];

const char * io_event_name(IO_Event_Type event_type) {
   return io_event_names[event_type];
}

const char * sleep_event_name(Sleep_Event_Type event_type) {
   return sleep_event_names[event_type];
}


static int            total_io_event_ct = 0;
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


void report_call_stats(int depth) {
   int d1 = depth+1;
   if (ddc_call_stats.stats_active) {
      rpt_title("Call Stats:", depth);
      report_one_call_stat(ddc_call_stats.pread_write_stats, d1);
      report_one_call_stat(ddc_call_stats.popen_stats,       d1);
      report_one_call_stat(ddc_call_stats.pclose_stats,      d1);
      report_one_call_stat(ddc_call_stats.pother_stats,      d1);
   }
}



// No effect on program logic, but makes debug messages easier to scan
long normalize_timestamp(long timestamp) {
   return timestamp - program_start_timestamp;
}



// TODO:
// maintain count of sleep events by type
// report them

// Eventually: adjust timeout based on error event frequency


void note_io_event_time(const IO_Event_Type event_type, const char * location, long when_nanos) {
   bool debug = false;

   total_io_event_ct++;
   last_io_event = event_type;
   last_io_timestamp = normalize_timestamp(when_nanos);
   io_event_cts_by_id[event_type]++;

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
   printf("   Total IO events:    %5d\n", total_io_event_ct);
   printf("   IO error count:     %5d\n", total_io_error_ct);
   printf("   Total sleep events: %5d\n", total_sleep_event_ct);

   int id;
   puts("");
   // printf("   IO Events by type:\n");
   printf("   IO Event type       Count\n");
   for (id=0; id < IO_EVENT_ID_CT; id++) {
      printf("   %-20s  %3d\n", io_event_names[id], io_event_cts_by_id[id]);
   }

   puts("");
   printf("   Sleep Event type    Count\n");
   for (id=0; id < SLEEP_EVENT_ID_CT; id++) {
      printf("   %-20s  %3d\n", sleep_event_names[id], sleep_event_cts_by_id[id]);
   }
}


