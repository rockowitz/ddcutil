/** \f core_per_thread_settings.h
 *
 * Maintains certain output settings on a per-thread basis.
 * These are:
 *   fout - normally stdout
 *   feer  - normally stderr
 *   output level (OL_NORMAL etc.)
 *
 *   These are maintained on per-thread basis because they are changeable on
 *   an API thread, and the change should not affect other threads.
 *   However, output level can be set on the ddcutil command line, and should
 *   apply to all threads.  Hence the ability to set a value for all threads.
 *
 *   There is no way to modify fout and ferr on the ddcutil command line, but
 *   they are handled similarly to output level, with default levels for all
 *   threads.  Also fout and ferr can be modified by api initialization.
 *
 *   Additionally, struct Thread_Output_Settings maintains the DDCA_Error_Detail
 *   chain for the thread.  This is always initialized to NULL, so requires no
 *   special initialization handling.
 * */

// Copyright (C)2014 -2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_PER_THREAD_SETTINGS_H_
#define CORE_PER_THREAD_SETTINGS_H_

#include <inttypes.h>
#include <stdio.h>

#include "ddcutil_types.h"


typedef struct {
   FILE *              fout;
   FILE *              ferr;
   DDCA_Output_Level   output_level;
   // bool             report_ddc_errors;  // unused, ddc error reporting left as global
   DDCA_Error_Detail * error_detail;
   intmax_t            tid;
} Thread_Output_Settings;

Thread_Output_Settings *  get_thread_settings();  // get settings for the current thread

//
// Output redirection
//
// Note: FILE * externs FOUT and FERR were eliminated when output redirection
// was made thread specific.  Use fout() and ferr().
// Note these are used within functions that are part of the shared library.

void set_fout(FILE * fout);
void set_ferr(FILE * ferr);
void set_fout_to_default();
void set_ferr_to_default();
FILE * fout();
FILE * ferr();

//
// Message level control
//

DDCA_Output_Level get_output_level();
DDCA_Output_Level set_output_level(DDCA_Output_Level newval);
void set_default_thread_output_level(DDCA_Output_Level ol);
// const  adding "const" would require api change to ddca_output_level_name()
char *            output_level_name(DDCA_Output_Level val);


#endif /* CORE_PER_THREAD_SETTINGS_H_ */
