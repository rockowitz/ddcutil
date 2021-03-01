// ddcutil_config_file.c

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <wordexp.h>

#include <stddef.h>
 
#include "public/ddcutil_status_codes.h"

#include "util/string_util.h"
#include "base/core.h"
#include "base/config_file.h"
#include "base/ddcutil_config_file.h"


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
      DBGMSG("Returning: %d", p.we_wordc);
   }
   return p.we_wordc;
}


/** Combines the options from a configuration with the command line arguments,
 *  returning a new list of tokens.
 *
 *  \param  old_argc  argc as passed on the command line
 *  \param  old argv  argv as passed on the command line
 *  \param  new_argv_loc  where to return the address of the combined token list
 *  \param  detault_options_loc  where to return string of options obtained from init file
 *  \return number of tokens in the combined list, -1 if errors
 *          reading the configuration file. n. it is not an error if the
 *          configuration file does not exist.  In that case 0 is returned.
 */
int get_config_file(char * application, char *** tokens_loc, char** default_options_loc) {
   bool debug = false;
   // char * cmd_prefix = read_configuration_file();
   int token_ct = 0;
   *tokens_loc = NULL;
   *default_options_loc = NULL;
   Error_Info * errs = load_configuration_file(false);
   if (errs) {
      if (errs->status_code == DDCRC_NOT_FOUND) {
         token_ct = 0;
      }
      else {
         // rpt_vstring(0,"Error(s) reading configuration file");
         errinfo_report_details(errs, 0);
         token_ct = -1;
      }
      errinfo_free(errs);
   }
   else {
      // dbgrpt_ini_hash(0);
      char * global_options  = get_config_value("global",  "options");
      char * ddcutil_options = get_config_value(application, "options");

      char * cmd_prefix = g_strdup_printf("%s %s",
                                   (global_options) ? global_options : "",
                                   (ddcutil_options) ? ddcutil_options : "");
      DBGMSF(debug, "cmd_prefix= |%s|", cmd_prefix);
      *default_options_loc = cmd_prefix;
      char ** prefix_tokens = NULL;
      if (cmd_prefix) {
         token_ct = tokenize_init_line(cmd_prefix, &prefix_tokens);
         *tokens_loc = prefix_tokens;
      }
   }
   return token_ct;

}
