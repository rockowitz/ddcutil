// sysfs_filter_functions.c

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <glib-2.0/glib.h>
#include <pcre.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string_util.h"
#include "sysfs_util.h"

#include "sysfs_filter_functions.h"


//
// Store compiled regular expressions
//

GHashTable * pcre_hash_table = NULL;


GHashTable* get_pcre_hash_table() {
   // printf("(%s) Starting. pcre_hash = %p\n", __func__, pcre_hash);
   if (!pcre_hash_table)
      pcre_hash_table = g_hash_table_new_full(
            g_str_hash,                // GHashFunc hash_func,
            g_str_equal,               // GEqualFunc key_equal_func,
            g_free,                    // GDestroyNotify key_destroy_func,
            g_free);                   // GDestroyNotify value_destroy_func, is g_free sufficient?

   // printf("(%s) Done. Returning pcre_hash_table = %p\n", __func__, pcre_hash_table);
   return pcre_hash_table;
}


void free_pcre_hash_table() {
   if (pcre_hash_table)
      g_hash_table_destroy(pcre_hash_table);
}


void save_hashed_pcre(const char * pattern, pcre * re) {
   bool debug = false;
   if (debug)
      printf("(%s) Executing. pattern = |%s|, re=%p\n", __func__, pattern, re);
   GHashTable * pcre_hash = get_pcre_hash_table();
   g_hash_table_replace(pcre_hash, strdup( pattern), re);
}


pcre * get_hashed_pcre(const char * pattern) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. pattern = |%s|\n", __func__, pattern);
   GHashTable * pcre_hash = get_pcre_hash_table();
   pcre * result = g_hash_table_lookup(pcre_hash, pattern);
   if (debug)
      printf("(%s) Returning %p. pattern = |%s|\n", __func__, result, pattern);
   return result;
}


//
// Filename_Filter_Func
//

static const char * cardN_connector_pattern = "^card[0-9]+[-]";
static const char * cardN_pattern = "^card[0-9]+$";


bool eval_pcre(pcre * re, const char * value) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. re=%p, value=|%s|\n", __func__, re, value);
   static int ovecsize = 30;     // should be a multiple of 3
   int ovector[ovecsize];
   int rc = pcre_exec(
          re,                   /* the compiled pattern */
          NULL,                 /* no extra data - we didn't study the pattern */
          value,                /* the subject string */
          strlen(value),        /* the length of the subject */
          0,                    /* start at offset 0 in the subject */
          0,                    /* default options */
          ovector,              /* output vector for substring information */
          ovecsize              /* number of elements in the output vector - multiple of 3 */
       );
   bool result = (rc >= 0) ? true : false;
   if (debug)
       printf("(%s) Returning %s. value=|%s|, pcre_exec() returned %d\n",
             __func__, sbool(result), value, rc);
   return result;
}


bool pcre_compile_and_eval(const char * pattern, const char * value) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. pattern=|%s|, value=|%s|\n", __func__, pattern, value);
   pcre * re =  get_hashed_pcre(pattern);
   // printf("(%s) forcing re = NULL\n", __func__);
   // re = NULL;
   if (!re) {
      // printf("(%s) Compiling...\n", __func__);
      const char *   error = NULL;
      int erroffset = 0;

      re = pcre_compile(
            pattern,
            0,         // default options
            &error,    // for error msg
            &erroffset,  // error offset
            NULL);        // use default char tables
      if (!re) {
               printf("PCRE compilation failed at offset %d: %s\n", erroffset, error);
               assert(re);
      }
      save_hashed_pcre(pattern, re);
   }
   bool result = eval_pcre(re, value);
   if (debug)
      printf("(%s) Done. Returning %s\n", __func__, sbool(result));
   return result;
}


bool predicate_cardN(const char * value) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. value = |%s|\n", __func__, value);

   bool b2 = pcre_compile_and_eval(cardN_pattern, value);

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
   bool b1 = pcre_compile_and_eval(cardN_connector_pattern, value);
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

