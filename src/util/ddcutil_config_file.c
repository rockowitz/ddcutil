/** \file ddcutil_config_file.c
 *  Processes an INI file used for ddcutil options
 *
 *  This is not a generic utility file, but is included in
 *  the util directory to simplify its copying unmodified into
 *  the ddcui source tree.
 */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <wordexp.h>

#include "config_file.h"
#include "string_util.h"
#include "xdg_util.h"

#include "ddcutil_config_file.h"

// *** TEMP ***
#ifdef UNUSED
static char * config_fn = NULL;

char * get_config_file_name() {
   return config_fn;
}
#endif


/** Tokenize a string as per the command line
 *
 *  \param  string to tokenize
 *  \param  tokens_loc where to return the address of a
 *                     null-terminate list of tokens
 *  \return number of tokens
 */
int tokenize_init_line(char * string, char ***tokens_loc) {
   bool debug = false;
   wordexp_t p;
   int flags = WRDE_NOCMD;
   if (debug)
      flags |= WRDE_SHOWERR;
   wordexp(string, &p, flags);
   *tokens_loc = p.we_wordv;
   if (debug) {
      ntsa_show(p.we_wordv);
      printf("(%s) Returning: %ld\n", __func__, p.we_wordc);
   }
   return p.we_wordc;
}


/** Processes a ddcutil configuration file, returning the options in both an untokenized and
 *  a tokenized form.
 *
 *  \param  ddcutil_application  "ddcutil", "libddcutil", "ddcui"
 *  \param  tokenized_options_loc   where to return the address of the null terminated token list
 *  \param  untokenized_string_loc  where to return untokenized string of options obtained from init file
 *  \return number of tokens, -1 if errors
 *          It is not an error if the configuration file does not exist.
 *          In that case 0 is returned..
 */
int read_ddcutil_config_file(
      const char *   ddcutil_application,
      char *** tokenized_options_loc,
      char**   untokenized_option_string_loc) {
   bool debug = false;
   int token_ct = 0;
   *tokenized_options_loc = NULL;
   *untokenized_option_string_loc = NULL;
   char * config_fn = find_xdg_config_file("ddcutil", "ddcutilrc");
   if (!config_fn) {
      if (debug) // coverity[dead_error_condition]
         printf("(%s) Configuration file not found\n", __func__);
      token_ct = 0;
      goto bye;
   }

   GHashTable * config_hash = NULL;
   GPtrArray * errmsgs = g_ptr_array_new();
   int load_rc = load_configuration_file(config_fn, &config_hash, errmsgs, false);
   if (debug)
      fprintf(stderr, "load_configuration file() returned %d\n", load_rc);
   if (load_rc < 0) {
      if (load_rc == -ENOENT) {
         token_ct = 0;
      }
      else {
         fprintf(stderr, "Error loading configuration file: %d\n", load_rc);
         token_ct = load_rc;
      }
   }
   if (errmsgs->len > 0) {
      fprintf(stderr, "Error(s) processing configuration file %s\n", config_fn);
      for (int ndx = 0; ndx < errmsgs->len; ndx++) {
         fprintf(stderr, "   %s\n", (char*) g_ptr_array_index(errmsgs, ndx));
      }
      token_ct = -1;
   }
   if (token_ct >= 0) {
      if (debug) {
         dump_ini_hash(config_hash);
      }
      char * global_options  = get_config_value(config_hash, "global",  "options");
      char * ddcutil_options = get_config_value(config_hash, ddcutil_application, "options");

      char * combined_options = g_strdup_printf("%s %s",
                                   (global_options) ? global_options : "",
                                   (ddcutil_options) ? ddcutil_options : "");
      if (debug)
         printf("(%s) combined_options= |%s|\n", __func__, combined_options);
      *untokenized_option_string_loc = combined_options;
      char ** prefix_tokens = NULL;
      if (combined_options) {
         token_ct = tokenize_init_line(combined_options, &prefix_tokens);
         *tokenized_options_loc = prefix_tokens;
      }
   }
   free(config_fn);

bye:
   if (debug) /* coverity[DEADCODE] */  {   /* coverity[dead_error_line] */
      printf("(%s) Returning untokenized options: |%s|, token_ct = %d\n",
             __func__, *untokenized_option_string_loc, token_ct);
   }
   return token_ct;

}


int merge_command_tokens(
      int      old_argc,
      char **  old_argv,
      int      config_token_ct,
      char **  config_tokens,
      char *** new_argv_loc)
{
   bool debug = false;

   // default, assume no config file parms
   *new_argv_loc = old_argv;
   int new_argc = old_argc;

   if (config_token_ct > 0) {
      int new_ct = config_token_ct + old_argc + 1;
      /* coverity[DEADCODE] */
      if (debug)
         // coverity[dead_error_line]
         printf("(%s) config_token_ct = %d, argc=%d, new_ct=%d\n",
               __func__, config_token_ct, old_argc, new_ct);
      char ** combined = calloc(new_ct, sizeof(char *));
      combined[0] = old_argv[0];
      int new_ndx = 1;
      for (int prefix_ndx = 0; prefix_ndx < config_token_ct; prefix_ndx++, new_ndx++) {
         combined[new_ndx] = config_tokens[prefix_ndx];
      }
      for (int old_ndx = 1; old_ndx < old_argc; old_ndx++, new_ndx++) {
         combined[new_ndx] = old_argv[old_ndx];
      }
      combined[new_ndx] = NULL;
      if (debug)
         printf("(%s) Final new_ndx = %d", __func__, new_ndx);
      *new_argv_loc = combined;
      new_argc = ntsa_length(combined);
   }

   if (debug)
      /* coverity[dead_error_line] */ printf("(%s) Returning %d\n", __func__, new_argc);
   return new_argc;
}



/** Combines the options from the ddcutil configuration file with the command line arguments,
 *  returning a new list of tokens.
 *
 *  \param  old_argc  argc as passed on the command line
 *  \param  old argv  argv as passed on the command line
 *  \param  new_argv_loc  where to return the address of the combined token list
 *                        as a Null_Terminated_String_Array
 *  \param  detault_options_loc  where to return string of options obtained from ini file
 *  \return number of tokens in the combined list, -1 if errors
 *          reading the configuration file. n. it is not an error if the
 *          configuration file does not exist.  In that case 0 is returned.
 */
int full_arguments(
      const char * ddcutil_application,     // "ddcutil", "ddcui"
      int      old_argc,
      char **  old_argv,
      char *** new_argv_loc,
      char**   default_options_loc)
{
   bool debug = false;
   char **prefix_tokens = NULL;

   *new_argv_loc = old_argv;
   int new_argc = old_argc;

   int prefix_token_ct =
         read_ddcutil_config_file(ddcutil_application, &prefix_tokens, default_options_loc);
   /* coverity[dead_error_condition] */
   if (debug)
      printf("(%s) get_config_file() returned %d\n", __func__, prefix_token_ct);
   if (prefix_token_ct < 0) {
      new_argc = -1;
   }
   else if (prefix_token_ct > 0) {
      new_argc =  merge_command_tokens(
            old_argc,
            old_argv,
            prefix_token_ct,
            prefix_tokens,
            new_argv_loc);
   }
   if (prefix_tokens)
      ntsa_free(prefix_tokens, false);

   if (debug)
      printf("(%s) Returning: %d\n", __func__, new_argc);
   return new_argc;
}


