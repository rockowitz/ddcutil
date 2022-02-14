/** \file sysfs_filter_functions.c */

// Copyright (C) 2021-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <glib-2.0/glib.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "report_util.h"
#include "string_util.h"
#include "sysfs_util.h"

#include "sysfs_filter_functions.h"


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
      printf("(%s) Starting. regex_hash_table=%p\n", __func__, regex_hash_table);
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
      printf("(%s) Starting. pattern = |%s|, compiled_re=%p\n", __func__, pattern, compiled_re);
   GHashTable * regex_hash = get_regex_hash_table();
   g_hash_table_replace(regex_hash, strdup( pattern), compiled_re);
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
      printf("(%s) Returning %p. pattern = |%s|\n", __func__, result, pattern);
   return result;
}


//
// Filename_Filter_Func
//

static const char * cardN_connector_pattern = "^card[0-9]+[-]";
static const char * cardN_pattern = "^card[0-9]+$";
static const char * D_00hh_pattern = "^[0-9]+-00[0-9a-fA-F]{2}$";


bool eval_regex(regex_t * re, const char * value) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. re=%p, value=|%s|\n", __func__, re, value);
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
         printf("(%s) Allocated regex %p, compiling...\n", __func__, re);
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


bool predicate_cardN(const char * value) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. value = |%s|\n", __func__, value);

   bool b2 = compile_and_eval_regex(cardN_pattern, value);

   // bool result = str_starts_with(value, "card") && strlen(value) == 5;
   // if (debug)
   //    printf("(%s) str_starts_with() && strlen() returned %s\n", __func__, sbool(result));
   // assert(b2 == result);
   if (debug)
      printf("(%s) Returning: %s. value=|%s|\n", __func__, sbool(b2), value);
   return b2;
}


bool predicate_cardN_connector(const char * value) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. value=|%s|\n", __func__, value);
   bool b1 = compile_and_eval_regex(cardN_connector_pattern, value);
   if (debug)
      printf("(%s) Returning %s, value=|%s|\n", __func__, sbool( b1), value);
   return b1;
}


bool startswith_i2c(const char * value) {
   return str_starts_with(value, "i2c-");
}

bool class_display_device_predicate(const char * value) {
   return str_starts_with(value, "0x03");
}


bool predicate_any_D_00hh(const char * value) {
   bool debug = false;
   // if (debug)
   //    printf("(%s) Starting. value=|%s|\n", __func__, value);
   bool b1 = compile_and_eval_regex(D_00hh_pattern, value);
   if (debug)
      printf("(%s) value=|%s|, Returning %s\n", __func__, value, sbool( b1));
   return b1;
}


bool predicate_exact_D_00hh(const char * value, const char * sbusno) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. value=|%s|, sbusno=|%s|\n", __func__, value, sbusno);
   bool b1 = compile_and_eval_regex(D_00hh_pattern, value);
   if (b1) {
      // our utilities don't support extracting match groups
      char * hypos = strchr(value, '-'); // must succeed because of regex match
      char * s = substr(value, 0, (hypos-value));
      b1 = streq(s, sbusno);
      free(s);
   }
   if (debug)
      printf("(%s) Returning %s\n", __func__, sbool( b1));
   return b1;
}





//
// Dir_Filter_Func
//

#ifdef MAYBE_FUTURE
bool dirname_starts_with(const char * dirname, const char * val) {
   bool debug = false;
   DBGMSF(debug, "dirname=%s, val_fn=%s", dirname, val);
   bool result = str_starts_with(dirname, val);
   DBGMSF(debug, "Returning %s", sbool(result));
   return result;
#endif

// for e.g. i2c-3
bool is_i2cN(const char * dirname, const char * val) {
   // bool debug = false;
   // DBGMSF(debug, "dirname=%s, val_fn=%s", dirname, val);
   bool result = str_starts_with(dirname, "i2c-");
   // DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

bool is_drm_dp_aux_subdir(const char * dirname, const char * val) {
   // bool debug = false;
   // DBGMSF(debug, "dirname=%s, val=%s", dirname, val);
   bool result = str_starts_with(dirname, "drm_dp_aux");
   // DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

// for e.g. card0-DP-1
bool is_card_connector_dir(const char * dirname, const char * simple_fn) {
   bool result = predicate_cardN_connector(simple_fn);
   return result;
}

// for e.g. card0
bool is_cardN_dir(const char * dirname, const char * simple_fn) {
   bool result = str_starts_with(simple_fn, "card");
   return result;
}

bool is_drm_dir(const char * dirname, const char * simple_fn) {
   bool result = streq(simple_fn, "drm");
   return result;
}

bool is_i2cN_dir(const char * dirname, const char * simple_fn) {
   bool result = str_starts_with(simple_fn, "i2c-");
   return result;
}

// does dirname/simple_fn have attribute class with value display controller or docking station?
bool has_class_display_or_docking_station(
      const char * dirname, const char * simple_fn)
{
   // bool debug = false;
   bool result = false;
   // DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);
   char * class_val = NULL;
   int    iclass = 0;
   int    top_byte = 0;
   if ( GET_ATTR_TEXT(&class_val, dirname, simple_fn, "class") ) {
      if (str_to_int(class_val, &iclass, 16) ) {
         top_byte = iclass >> 16;
         if (top_byte == 0x03 || top_byte == 0x0a)  // display controller or docking station
            result = true;
      }
   }
   // DBGMSF(debug, "class_val = %s, top_byte = 0x%02x, result=%s",
   //               class_val, top_byte, sbool(result) );
   return result;
}

