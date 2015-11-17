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

#include <base/call_stats.h>


Call_Stats *  init_one_call_stat(char * name) {
   Call_Stats * pstats = calloc(1,sizeof(Call_Stats));
   pstats->total_call_nanosecs = 0;
   pstats->total_call_ct       = 0;
   pstats->stat_name           = name;
   return pstats;
}


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


void report_one_call_stat(Call_Stats * pstats, int depth) {
   if (pstats) {
     rpt_vstring(depth, "Total %-10s calls:                        %7d",
            pstats->stat_name, pstats->total_call_ct);
     rpt_vstring(depth, "Total %-10s call milliseconds (nanosec):  %7ld  (%10ld)",
         pstats->stat_name,
         pstats->total_call_nanosecs / (1000*1000),
         pstats->total_call_nanosecs);
   }
}


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
}
