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
#include <base/parms.h>

#include <base/call_stats.h>


static DDC_Call_Stats ddc_call_stats;


static int           total_io_event_ct = 0;
static int           total_io_error_ct = 0;
static int           total_sleep_event_ct = 0;
static IO_Event_Type last_io_event;
static long          last_io_timestamp = -1;
static long          program_start_timestamp;



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

void init_call_stats() {
   init_ddc_call_stats();
      program_start_timestamp = cur_realtime_nanosec();

}


void record_io_event(const IO_Event_Type event_type, const char * location, long start_time_nanos, long end_time_nanos) {
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

#ifdef OLD
I2C_Call_Stats * new_i2c_call_stats() {
   I2C_Call_Stats * pi2c_call_stats;
   pi2c_call_stats                    = calloc(1,sizeof(I2C_Call_Stats));
   pi2c_call_stats->pread_write_stats = init_one_call_stat("read/write");
   pi2c_call_stats->popen_stats       = init_one_call_stat("open");
   pi2c_call_stats->pclose_stats      = init_one_call_stat("close");
   pi2c_call_stats->stats_active      = true;     // TODO: figure out proper way to set only if /dev/i2c* exists
   return pi2c_call_stats;
}


ADL_Call_Stats * new_adl_call_stats() {
   ADL_Call_Stats * padl_call_stats;
   padl_call_stats = calloc(1,sizeof(ADL_Call_Stats));
   padl_call_stats->pread_write_stats = init_one_call_stat("read/write");
   padl_call_stats->pother_stats      = init_one_call_stat("other");
   // padl_call_stats->stats_active   = false;     // redundant (calloc)
   return padl_call_stats;
}
#endif


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

#ifdef OLD
void report_i2c_call_stats(I2C_Call_Stats * pi2c_call_stats, int depth) {
   int d1 = depth+1;
   if (pi2c_call_stats && pi2c_call_stats->stats_active) {
      rpt_title("I2C Call Stats:", depth);
      report_one_call_stat(pi2c_call_stats->popen_stats,       d1);
      report_one_call_stat(pi2c_call_stats->pclose_stats,      d1);
      report_one_call_stat(pi2c_call_stats->pread_write_stats, d1);
   }
}


void report_adl_call_stats(ADL_Call_Stats * padl_call_stats, int depth) {
   int d1 = depth+1;
   if (padl_call_stats && padl_call_stats->stats_active) {
      rpt_title("ADL Call Stats:", depth);
      report_one_call_stat(padl_call_stats->pread_write_stats, d1);
      report_one_call_stat(padl_call_stats->pother_stats,      d1);
   }
#endif


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




long normalize_timestamp(long timestamp) {
   return timestamp - program_start_timestamp;
}


static const char * io_event_names[]    = {
      "IE_WRITE" ,
      "IE_READ",
      "IE_WRITE_READ",
      "IE_OPEN",
      "IE_CLOSE",
      "IE_OTHER" };
static const char * sleep_event_names[] = {"SE_WRITE_TO_READ", "SE_POST_OPEN", "SE_POST_WRITE"};
#define IO_EVENT_ID_CT (sizeof(io_event_names)/sizeof(char *))
#define SLEEP_EVENT_ID_CT (sizeof(sleep_event_names)/sizeof(char *))
static int io_event_cts_by_id[IO_EVENT_ID_CT];
static int sleep_event_cts_by_id[SLEEP_EVENT_ID_CT];

const char * io_event_name(IO_Event_Type event_type) {
   return io_event_names[event_type];
}

const char * sleep_event_name(Sleep_Event_Type event_type) {
   return sleep_event_names[event_type];
}


// TODO:
// maintain count of sleep events by type
// report them

// Eventually: adjust timeout based on error event frequency

void note_io_event(IO_Event_Type event_type, const char * location){
   long cur_timestamp = cur_realtime_nanosec();
   note_io_event_time(event_type, location, cur_timestamp);
}

void note_io_event_time(IO_Event_Type event_type, const char * location, long when_nanos) {

   bool debug = false;

   total_io_event_ct++;
   last_io_event = event_type;
   last_io_timestamp = normalize_timestamp(when_nanos);
   io_event_cts_by_id[event_type]++;

   if (debug)
      printf("(%s) timestamp=%11ld, event_type=%s, location=%s\n",
             __func__, last_io_timestamp, io_event_name(event_type), location);
}

void note_io_error(Global_Status_Code gsc, const char * location) {
   bool debug = false;

   total_io_error_ct++;

   if (debug)
      printf("(%s) IO error: %s\n", __func__, global_status_code_description(gsc));

}

void report_sleep_strategy_stats(int depth) {
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



void sleep_i2c(Sleep_Event_Type event_type) {
   sleep_io_mode(DDC_IO_DEVI2C, event_type);
}


void sleep_adl(Sleep_Event_Type event_type) {
   sleep_io_mode(DDC_IO_ADL, event_type);

}
void sleep_dh(Display_Handle* dh, Sleep_Event_Type event_type) {
   sleep_io_mode(dh->ddc_io_mode, event_type);
}

void sleep_io_mode(DDC_IO_Mode io_mode, Sleep_Event_Type event_type) {


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

   // Is tracing useful, given that we know the event type?
   // void sleep_millis_with_trace(int milliseconds, const char * caller_location, const char * message);

   sleep_event_cts_by_id[event_type]++;
   total_sleep_event_ct++;
   sleep_millis(sleep_time_millis);


}




