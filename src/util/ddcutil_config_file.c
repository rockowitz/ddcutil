/** \file ddcutil_config_file.c
 *  Processes an INI file used for ddcutil options
 *
 *  This is not a generic utility file, but is included in
 *  the util directory to simplify its copying unmodified into
 *  the ddcui source tree.
 */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <wordexp.h>

#include "config_file.h"
#include "string_util.h"
#include "xdg_util.h"

#include "ddcutil_config_file.h"


/** Tokenize a string as per the command line
 *
 *  \param  string to tokenize
 *  \param  tokens_loc where to return the address of a
 *                     null-terminated list of tokens
 *  \return number of tokens
 */
int tokenize_options_line(char * string, char ***tokens_loc) {
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
 *  \param  ddcutil_application     "ddcutil", "libddcutil", "ddcui"
 *  \param  tokenized_options_loc   where to return the address of the null terminated token list
 *  \param  untokenized_option_loc  where to return untokenized string of options obtained from
 *                                  the configuration file
 *  \param  errmsgs                 collects error messages if non-NULL
 *  \param  config_filename_loc     where to return fully qualified name of configuration file
 *  \param  verbose                 issue error messages if true
 *  \retval >= 0                    number of tokens
 *  \retval < 0                     status code for error
 *
 *  It is not an error if the configuration file does not exist. In that case
 *  an empty tokenized option list is created and 0 is returned..
 */
int read_ddcutil_config_file(
      const char *   ddcutil_application,
      char ***       tokenized_options_loc,
      char**         untokenized_option_string_loc,
      GPtrArray *    errmsgs,
      char **        config_fn_loc,
      bool           verbose)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. ddcutil_application=%s, errmsgs=%p, verbose=%s\n",
             __func__, ddcutil_application, errmsgs, SBOOL(verbose));

   int result = 0;
   *tokenized_options_loc = NULL;
   *untokenized_option_string_loc = NULL;
   *config_fn_loc = NULL;
   char ** prefix_tokens = NULL;

   char * config_fn = find_xdg_config_file("ddcutil", "ddcutilrc");
   if (!config_fn) {
      if (debug)
         printf("(%s) Configuration file not found\n", __func__);
      result = -ENOENT;
      goto bye;
   }
   if (debug)
      printf("(%s) Found configuration file: %s\n", __func__, config_fn);
   *config_fn_loc = config_fn;

   Parsed_Ini_File * ini_file = NULL;
   int load_rc = ini_file_load(config_fn, errmsgs, false, &ini_file);
   if (debug)
      fprintf(stderr, "load_configuration file() returned %d\n", load_rc);
   if (load_rc < 0 && load_rc != -ENOENT) {
      if (verbose)
         fprintf(stderr, "Error loading configuration file: %d\n", load_rc);
   }
   if (verbose && errmsgs && errmsgs->len > 0) {
      fprintf(stderr, "Error(s) processing configuration file %s\n", config_fn);
      for (guint ndx = 0; ndx < errmsgs->len; ndx++) {
         fprintf(stderr, "   %s\n", (char*) g_ptr_array_index(errmsgs, ndx));
      }
   }

   if (load_rc == 0) {
      if (debug) {
         ini_file_dump(ini_file);
      }
      char * global_options  = ini_file_get_value(ini_file, "global",  "options");
      char * ddcutil_options = ini_file_get_value(ini_file, ddcutil_application, "options");

      char * combined_options = g_strdup_printf("%s %s",
                                   (global_options)  ? global_options  : "",
                                   (ddcutil_options) ? ddcutil_options : "");
      if (debug)
         printf("(%s) combined_options= |%s|\n", __func__, combined_options);

      if (combined_options) {
         result = tokenize_options_line(combined_options, &prefix_tokens);
      }
      *untokenized_option_string_loc = combined_options;
      *tokenized_options_loc = prefix_tokens;
   }
   else
      result = load_rc;
   ini_file_free(ini_file);

bye:
   if (debug)  {
      printf("(%s) Returning untokenized options: |%s|, *config_fn_loc=%s\n",
             __func__, *untokenized_option_string_loc, *config_fn_loc);
      printf("(%s) *tokenized_options_loc:\n", __func__);
      if (*tokenized_options_loc)
         ntsa_show(*tokenized_options_loc);
      printf("(%s) Returning: %d\n", __func__, result);
   }
   return result;
}


/** Merges the tokenized command string passed to the program with tokens
 *  obtained from the configuration file.
 *
 *  \param   old_argc        original argument count
 *  \param   old_argv        original argument list
 *  \param   config_token_ct number of tokens to insert
 *  \param   config_tokens   list of tokens
 *  \param   new_argv_loc    where to return address of merged argument list
 *  \return  length of merged argument list
 */
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
      if (debug)
         printf("(%s) config_token_ct = %d, argc=%d, new_ct=%d\n",
               __func__, config_token_ct, old_argc, new_ct);
      char ** combined = calloc(new_ct, sizeof(char *));
      combined[0] = old_argv[0];   // command
      int new_ndx = 1;
      for (int prefix_ndx = 0; prefix_ndx < config_token_ct; prefix_ndx++, new_ndx++) {
         combined[new_ndx] = config_tokens[prefix_ndx];
      }
      for (int old_ndx = 1; old_ndx < old_argc; old_ndx++, new_ndx++) {
         combined[new_ndx] = old_argv[old_ndx];
      }
      combined[new_ndx] = NULL;
      if (debug)
         printf("(%s) Final new_ndx = %d\n", __func__, new_ndx);
      *new_argv_loc = combined;
      new_argc = new_ct - 1;
      assert(new_argc == ntsa_length(combined));
   }

   if (debug) {
       printf("(%s) Returning %d, *new_argv_loc=%p\n", __func__, new_argc, *new_argv_loc);
       printf("(%s) tokens:\n", __func__);
       ntsa_show(*new_argv_loc);
   }

   return new_argc;
}


/** Reads and tokenizes the appropriate options entries in the config file,
 *  then combines the tokenized options from the ddcutil configuration file
 *  with the command line arguments, returning a new list of tokens.
 *
 *  \param  old_argc  argc as passed on the command line
 *  \param  old argv  argv as passed on the command line
 *  \param  new_argv_loc  where to return the address of the combined token list
 *                        as a Null_Terminated_String_Array
 *  \param  detault_options_loc  where to return string of options obtained from ini file
 *  \return number of tokens in the combined list, -1 if errors
 *          reading or parsing the configuration file. n. it is not an error if the
 *          configuration file does not exist.  In that case 0 is returned.
 */
int read_parse_and_merge_config_file(
      const char * ddcutil_application,     // "ddcutil", "ddcui"
      int          old_argc,
      char **      old_argv,
      char ***     new_argv_loc,
      char**       untokenized_cmd_prefix_loc,
      char**       configure_fn_loc,
      GPtrArray *  errmsgs)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. ddcutil_application=%s, errmsgs=%p\n",
             __func__, ddcutil_application, errmsgs);
   char **cmd_prefix_tokens = NULL;

   *new_argv_loc = old_argv;
   int result = 0;
   // int new_argc = old_argc;

   int read_config_rc =
         read_ddcutil_config_file(
               ddcutil_application,
               &cmd_prefix_tokens,
               untokenized_cmd_prefix_loc,
               errmsgs,
               configure_fn_loc,
               debug);   // verbose
#ifdef NO
   if (read_config_rc == 0  || read_config_rc == -EBADMSG)
      assert(*configure_fn_loc && *untokenized_cmd_prefix_loc && cmd_prefix_tokens);
   else if (read_config_rc == -ENOENT)
      assert(!*configure_fn_loc && !*untokenized_cmd_prefix_loc && !cmd_prefix_tokens);
   else   // error reading config file that exists
      assert(*configure_fn_loc && !*untokenized_cmd_prefix_loc && !*cmd_prefix_tokens);
#endif

   int prefix_token_ct = (cmd_prefix_tokens) ? ntsa_length(cmd_prefix_tokens) : 0;
   if (debug)
      printf("(%s) get_config_file() returned %d, configure_fn: %s\n",
             __func__, read_config_rc, *configure_fn_loc);

   if (read_config_rc == -ENOENT) {
      result = 0;
   }
   else if (read_config_rc < 0) {
      result = -1;
   }
   else if (prefix_token_ct > 0) {
      int new_argc =  merge_command_tokens(
            old_argc,
            old_argv,
            prefix_token_ct,
            cmd_prefix_tokens,
            new_argv_loc);
      assert(new_argc == ntsa_length(*new_argv_loc));
   }
   if (cmd_prefix_tokens)
      ntsa_free(cmd_prefix_tokens, false);

   if (debug) {
       printf("(%s) Returning %d, *new_argv_loc=%p\n", __func__, result, *new_argv_loc);
       printf("(%s) tokens:\n", __func__);
       ntsa_show(*new_argv_loc);
   }

   return result;
}

