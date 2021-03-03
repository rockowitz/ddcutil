/** \file ddcutil_config_file.c
 *  Processes an INI file used for ddcutil options
 */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../util/ddcutil_config_file.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <wordexp.h>

#include "util/config_file.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/xdg_util.h"


// *** TEMP ***
static char * config_fn = NULL;

char * get_config_file_name() {
   return config_fn;
}


/** Tokenize a string as per the command line
 *
 *  \param  string to tokenize
 *  \param  tokens_loc where to return the address of a
 *                     null-terminate list of tokens
 *  \return number of tokens
 */
int tokenize_init_line(char * string, char ***tokens_loc) {
   bool debug = true;
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
      char *   ddcutil_application,
      char *** tokenized_options_loc,
      char**   untokenized_option_string_loc) {
   bool debug = true;
   int token_ct = 0;
   *tokenized_options_loc = NULL;
   *untokenized_option_string_loc = NULL;
   config_fn = find_xdg_config_file("ddcutil", "ddcutilrc");
   if (!config_fn) {
      if (debug)
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
         f0printf(stderr, "Error loading configuration file: %d\n", load_rc);
         token_ct = load_rc;
      }
   }
   if (errmsgs->len > 0) {
      f0printf(stderr, "Error(s) processing configuration file %s\n", config_fn);
      for (int ndx = 0; ndx < errmsgs->len; ndx++) {
         f0printf(stderr, "   %s\n", g_ptr_array_index(errmsgs, ndx));
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
bye:
   if (debug) {
      printf("Returning untokenized options: |%s|, token_ct=%d\n",
            *untokenized_option_string_loc, token_ct);
   }
   return token_ct;

}
