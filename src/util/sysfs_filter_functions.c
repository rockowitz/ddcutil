// sysfs_filter_functions.c

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// #include <glib-2.0/glib.h>
#include <assert.h>
#include <glib-2.0/glib.h>
// #include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre.h>

#include "string_util.h"
#include "sysfs_util.h"

#include "sysfs_filter_functions.h"


GHashTable * pcre_hash_table = NULL;

GHashTable* get_pcre_hash_table() {
   // printf("(%s) Starting. pcre_hash = %p\n", __func__, pcre_hash);
   if (!pcre_hash_table)
      pcre_hash_table = g_hash_table_new_full(
            g_str_hash,                // GHashFunc hash_func,
            g_str_equal,               // GEqualFunc key_equal_func,
            g_free,                    // GDestroyNotify key_destroy_func,
            NULL);                     // need function to delete *pcre

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

#ifdef REGEX
static regex_t   compiled_cardN_re_buffer;
static bool      is_cardN_re_compiled = false;
// static regex_t*  compiled_cardN_re = NULL;

static regex_t * p_compiled_cardN_connector_re = NULL;

int compile_re(const char * pattern, regex_t* compiled_regex_ptr) {
   int rc = 0;
  //  regex_t*  compiled_re = calloc(1,sizeof(regex_t));
   rc = regcomp(compiled_regex_ptr, pattern, 0);
   printf("(%s) regcomp() of pattern |%s| returned %d\n", __func__, pattern, rc);

   if (rc != 0) {
        printf("(%s) Regex compilation of \"%s\" returned %d\n",
              __func__, cardN_re, rc);
        assert(false);
   }
   return 0;
}

#ifdef FUTURE
regex_t get_compiled_cardN_connector_re() {
   if (!compiled_cardN_re) {
      compiled_cardN_connector_re = compile_re(cardN_connector_re);
   }
   return compiled_cardN_re;
}
#endif

regex_t * get_compiled_cardN_re() {
   if (!is_cardN_re_compiled) {

      compile_re(cardN_pattern, &compiled_cardN_re_buffer);
      is_cardN_re_compiled = true;
   }
   regex_t * result = &compiled_cardN_re_buffer;
   printf("(%s) Returning %p\n", __func__, result);
   return result;
}

bool is_cardN_using_regex(const char * value) {
   bool result = false;

   int rc = regexec(get_compiled_cardN_re(), value, 0, NULL, 0);
   // printf("(%s) regexec() of |%s| returned %d\n", __func__, value, rc);
   result = (rc == 0) ? true : false;

   printf("(%s) returning %s, value=|%s|, regexec() returned %d\n",
         __func__, sbool(result), value, rc);
   return result;
}


bool is_cardN_connector_using_regex(const char * value) {
   // printf("(%s) p_compiled_cardN_connector_re=%p, value=|%s|\n",
   //       __func__, p_compiled_cardN_connector_re, value);
   regex_t compiled;
   p_compiled_cardN_connector_re = &compiled;
  // if (!p_compiled_cardN_connector_re) {
  //    p_compiled_cardN_connector_re = calloc(1, sizeof(regex_t));
      // REG_EXTENDED required for "+" after "[0-9]" to be recognized
      const char * pattern = "^card[0-9]+[-]";
      int rc = regcomp(p_compiled_cardN_connector_re, pattern, REG_EXTENDED);
      if (rc != 0) {
         printf("(%s) regcomp returned %d\n", __func__, rc);
         abort();
      }
   // }
   rc = regexec(p_compiled_cardN_connector_re, value, 0, NULL, 0);
   // printf("(%s) value = |%s|, regexec returned %d\n", __func__, value, rc);
   bool result = (rc == 0) ? true : false;
   printf("(%s) returning %s, value=|%s|, regexec() returned %d\n", __func__, sbool(result), value, rc);
   regfree(p_compiled_cardN_connector_re);
   // p_compiled_cardN_connector_re = NULL;
   return result;
}
#endif

#ifdef OLD
static pcre * compiled_pcre_cardN_connector = NULL;

pcre * get_compiled_pcre_cardN_connector() {
   if (compiled_pcre_cardN_connector) {
      return compiled_pcre_cardN_connector;
   }

   printf("(%s) creating compiled_pcre_cardN_connector\n", __func__);
   const char *   error = NULL;
   int erroffset = 0;
   pcre* re = NULL;

   re = pcre_compile(
         cardN_connector_pattern,
         0,         // default options
         &error,    // for error msg
         &erroffset,  // error offset
         NULL);        // use default char tables
   if (!re) {
            printf("PCRE compilation failed at offset %d: %s\n", erroffset, error);
            assert(re);
   };
   compiled_pcre_cardN_connector = re;
   return re;
}
#endif

bool eval_pcre(pcre * re, const char * value) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. re=%p, value=|%s|\n", __func__, re, value);
   int ovector[30];
   int rc = pcre_exec(
          re,                   /* the compiled pattern */
          NULL,                 /* no extra data - we didn't study the pattern */
          value,                /* the subject string */
          strlen(value),        /* the length of the subject */
          0,                    /* start at offset 0 in the subject */
          0,                    /* default options */
          ovector,              /* output vector for substring information */
          30                    /* number of elements in the output vector - multiple of 3 */
       );
   bool result = (rc >= 0) ? true : false;
   if (debug)
       printf("(%s) Returning %s. value=|%s|, pcre_exec() returned %d\n",
             __func__, sbool(result), value, rc);
   return result;
}

#ifdef OLD
bool is_cardN_connector_using_pcre(const char * value) {
   printf("(%s) Starting. value=|%s|\n", __func__, value);
   pcre *   re = get_compiled_pcre_cardN_connector();
   bool result = eval_pcre(re,value);
   printf("(%s) Returning %s. value=|%s|\n", __func__, sbool(result), value);
   return result;
}



bool
starts_with_card(const char * value) {
   return str_starts_with(value, "card");
}
#endif


bool pcre_compile_and_eval(const char * pattern, const char * value) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. pattern=|%s|, value=|%s|\n", __func__, pattern, value);
   pcre * re =  get_hashed_pcre(pattern);
   // printf("(%s) forcing re = NULL\n", __func__);
   // re = NULL;
   if (!re) {
      printf("%s) Compiling...\n", __func__);
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

#ifdef REGEX
   bool b1 = is_cardN_using_regex(value);
   printf("(%s) is_cardN_using_regex() returned %s\n", __func__, sbool(b1));
   if (b1 != b2)
      printf("(%s) b1 != b2\n", __func__);
#endif

   bool result = str_starts_with(value, "card") && strlen(value) == 5;
   if (debug)
      printf("(%s) str_starts_with() && strlen() returned %s\n", __func__, sbool(result));
   assert(b2 == result);
   if (debug)
      printf("(%s) Returning: %s. value=|%s|\n", __func__, sbool(result), value);
   return result;
}


bool predicate_cardN_connector(const char * value) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. value=|%s|\n", __func__, value);
   // bool b1 = is_cardN_connector_using_pcre(value);
   bool b1 = pcre_compile_and_eval(cardN_connector_pattern, value);
#ifdef REGEX
   bool b2 = is_cardN_connector_using_regex(value);
   if (b1 != b2) {
      printf("(%s) !!!!! b1=%s, b2=%s\n", __func__, sbool( b1), sbool(b2));
      abort();
   }
#endif
   if (debug)
      printf("(%s) Returning %s, value=|%s|\n", __func__, sbool( b1), value);
   return b1;
}

#ifdef OLD
bool drm_filter(const char * name) {
   return str_starts_with(name, "card") && strlen(name) > 5;
}
#endif

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
   // bool debug = false;
   // DBGMSF(debug, "dirname=%s, simple_fn=%s", dirname, simple_fn);
   // bool result = str_starts_with(simple_fn, "card");
   bool result = predicate_cardN_connector(simple_fn);
   // DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

#ifdef OUT
// for e.g. card0-HDMI-0
bool is_sysfs_drm_connector_dir_name(const char * dirname, const char * simple_fn) {
   bool debug = false;
   if (debug)
      printf("(%s) dirname=%s, simple_fn=%s\n", __func__, dirname, simple_fn);

   bool result = predicate_cardN_connector(simple_fn);
#ifdef old
   bool result = false;
   if (str_starts_with(simple_fn, "card")) {
      char * hyphen_loc = strchr(simple_fn+4, '-');
      if (hyphen_loc) {
         // todo: test for card number
         result = true;
      }
   }
#endif

   if (debug)
      printf("(%s) Returning %s\n", __func__, sbool(result));
   return result;
}
#endif


// for e.g. card0
bool is_cardN_dir(const char * dirname, const char * simple_fn) {
   // bool debug = false;
   // DBGMSF(debug, "dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = str_starts_with(simple_fn, "card");
   // DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

bool is_drm_dir(const char * dirname, const char * simple_fn) {
   // bool debug = false;
   // DBGMSF(debug, "dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = streq(simple_fn, "drm");
   // DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

bool is_i2cN_dir(const char * dirname, const char * simple_fn) {
   // bool debug = false;
   // DBGMSF(debug, "dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = str_starts_with(simple_fn, "i2c-");
   // DBGMSF(debug, "Returning %s", sbool(result));
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



