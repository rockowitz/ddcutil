/** @file regex_util.c */

// Copyright (C) 2021-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <glib-2.0/glib.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug_util.h"
#include "report_util.h"
#include "regex_util.h"
#include "string_util.h"
#include "sysfs_util.h"
 



//
// Store compiled regular expressions
//

GHashTable * regex_hash_table = NULL;

// GDestroyNotify void (*GDestroyNotify) (gpointer data);
void destroy_regex(gpointer data) {
   // printf("(%s) Destroying compiled regex at %p\n", __func__, data);
   regfree( (regex_t*) data );
   free(data);                      // ???
}

GHashTable* get_regex_hash_table() {
   // printf("(%s) Starting. regex_hash_table = %p\n", __func__, regex_hash_table);
   if (!regex_hash_table)
      regex_hash_table = g_hash_table_new_full(
            g_str_hash,                // GHashFunc hash_func,
            g_str_equal,               // GEqualFunc key_equal_func,
            g_free,                    // GDestroyNotify key_destroy_func,
            destroy_regex);            // GDestroyNotify value_destroy_func

   // printf("(%s) Done. Returning regex_hash_table = %p\n", __func__, regex_hash_table);
   return regex_hash_table;
}


void dbgrpt_regex_hash_table() {
   if (regex_hash_table) {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init(&iter, regex_hash_table);
      while (g_hash_table_iter_next(&iter, &key, &value)) {
          rpt_vstring(2, "   %p->\"%s\"  :   %p", key, (char *) key, value);
      }
   }
   else
      rpt_vstring(1, "regex_hash_table not allocated");
}


void free_regex_hash_table() {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. regex_hash_table=%p\n", __func__, (void*)regex_hash_table);
   if (regex_hash_table) {
      if (debug) {
         printf("(%s) Hash table contents:\n", __func__);
         dbgrpt_regex_hash_table(regex_hash_table);
      }
      g_hash_table_destroy(regex_hash_table);
      regex_hash_table = NULL;
   }
   if (debug)
      printf("(%s) Done.\n", __func__);
}


void save_compiled_regex(const char * pattern, regex_t * compiled_re) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. pattern = |%s|, compiled_re=%p\n",
             __func__, pattern, (void*)compiled_re);
   GHashTable * regex_hash = get_regex_hash_table();
   g_hash_table_replace(regex_hash, g_strdup( pattern), compiled_re);
   if (debug)
      printf("(%s) Done.\n", __func__);
}


regex_t * get_compiled_regex(const char * pattern) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. pattern = |%s|\n", __func__, pattern);
   GHashTable * regex_hash = get_regex_hash_table();
   regex_t * result = g_hash_table_lookup(regex_hash, pattern);
   if (debug)
      printf("(%s) Returning %p. pattern = |%s|\n", __func__, (void*)result, pattern);
   return result;
}



// #ifdef FUTURE
// requires testing
bool eval_regex_with_matches(
      regex_t *     re,
      const char *  value,
      size_t        max_matches,
      regmatch_t *  pm )
{
   bool debug = true;
   if (debug)
      printf("(%s) Starting. re=%p, value=|%s|\n", __func__, (void*)re, value);
   int rc = regexec(
          re,                   /* the compiled pattern */
          value,                /* the subject string */
          max_matches,
          pm,
          0
       );
   bool result = (rc  == 0) ? true : false;
   if (debug)
       printf("(%s) Returning %s. value=|%s|, regexec() returned %d\n",
             __func__, sbool(result), value, rc);
   return result;
}
// #endif

bool eval_regex(regex_t * re, const char * value) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. re=%p, value=|%s|\n", __func__, (void*)re, value);
   int rc = regexec(
          re,                   /* the compiled pattern */
          value,                /* the subject string */
          0,
          NULL,
          0
       );
   bool result = (rc  == 0) ? true : false;
   if (debug)
       printf("(%s) Returning %s. value=|%s|, regexec() returned %d\n",
             __func__, sbool(result), value, rc);
   return result;
}


bool compile_and_eval_regex(const char * pattern, const char * value) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. pattern=|%s|, value=|%s|\n", __func__, pattern, value);
   regex_t * re = get_compiled_regex(pattern);
   // printf("(%s) forcing re = NULL\n", __func__);
   // re = NULL;
   if (!re) {
      re = calloc(1, sizeof(regex_t));
      if (debug)
         printf("(%s) Allocated regex %p, compiling...\n", __func__, (void*)re);
      int rc = regcomp(re, pattern, REG_EXTENDED);
      if (rc != 0) {
         printf("(%s) regcomp() returned %d\n", __func__, rc);
         assert(rc == 0);
      }
      save_compiled_regex(pattern, re);
   }
   bool result = eval_regex(re, value);
   if (debug)
      printf("(%s) Done. Returning %s\n", __func__, sbool(result));
   return result;
}


//#ifdef FUTURE
// to test
bool compile_and_eval_regex_with_matches(
      const char * pattern,
      const char * value,
      size_t       max_matches,
      regmatch_t * pm)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. pattern=|%s|, value=|%s|\n", __func__, pattern, value);
   regex_t * re = get_compiled_regex(pattern);
   // printf("(%s) forcing re = NULL\n", __func__);
   // re = NULL;
   if (!re) {
      re = calloc(1, sizeof(regex_t));
      if (debug)
         printf("(%s) Allocated regex %p, compiling...\n", __func__, (void*)re);
      int rc = regcomp(re, pattern, REG_EXTENDED);
      if (rc != 0) {
         printf("(%s) regcomp() returned %d\n", __func__, rc);
         assert(rc == 0);
      }
      save_compiled_regex(pattern, re);
   }
   bool result = eval_regex_with_matches(re, value, max_matches, pm);
   if (debug)
      printf("(%s) Done. Returning %s\n", __func__, sbool(result));
   return result;
}
// #endif


