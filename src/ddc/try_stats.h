/*
 * try_stats.h
 *
 *  Created on: Jul 15, 2014
 *      Author: rock
 */

#ifndef TRY_STATS_H_
#define TRY_STATS_H_

#define MAX_STAT_NAME_LENGTH  31
#define MAX_MAX_TRIES         15

// Returns an opaque pointer to a Try_Data data structure
void * create_try_data(char * stat_name, int max_tries);

void reset_try_data(void * stats_rec);

void record_tries(void * stats_rec, int rc, int tryct);

int get_total_tries(void * stats_rec);

void report_try_data(void * stats_rec);

#endif /* TRY_STATS_H_ */
