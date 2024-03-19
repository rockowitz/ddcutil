/** @file sysfs_filter_functions.c */

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

#include "sysfs_filter_functions.h"


//
// Filename_Filter_Func
//

static const char * cardN_connector_pattern = "^card[0-9]+[-]";
static const char * cardN_pattern = "^card[0-9]+$";
static const char * D_00hh_pattern = "^[0-9]+-00[0-9a-fA-F]{2}$";
static const char * i2c_N_pattern = "^i2c-([0-9]+)$";


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


bool predicate_i2c_N(const char * value) {
   bool debug = false;
   bool b1 = compile_and_eval_regex(i2c_N_pattern, value);
   if (debug)
      printf("(%s) value=|%s|, returning %s\n", __func__, value, sbool( b1));
   return b1;
}


#ifdef FUTURE
// untested
int match_i2c_N(const char * value) {
   bool debug = true;
   regmatch_t matchpos;
   int result = -1;
   if (compile_and_eval_regex(i2c_N_pattern, value, &matchpos)) {
      // pattern match ensures that the character after the end of the match is NULL,
      // and that atoi always succeeds
      result = atoi(value + matchpos.rm_so);
   }
   if (debug)
      printf("(%s) value=|%s|, returning %d\n", __func__, value, result);
   return result;
}


int match_dev_i2c_N(const char * value) {
   bool debug = true;
   int result = -1;
   if (str_starts_with(value,  "/dev/")) {
      result = match_i2c_N(value+5);
   }
   if (debug)
      printf("(%s) value=|%s|, returning %d\n", __func__, value, result);
   return result;
}
#endif

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
bool is_i2cN_dir(const char * dirname, const char * fn_ignored) {
   bool debug = false;
   bool result = predicate_i2c_N(dirname);
   if (debug)
      printf("(%s) dirname=%s, fn_ignored=%s, returning %s\n", __func__, dirname, fn_ignored, sbool(result));
   return result;
}


bool is_drm_dp_aux_subdir(const char * dirname, const char * fn_ignored) {
   bool debug = false;
   bool result = str_starts_with(dirname, "drm_dp_aux");
   if (debug)
      printf("(%s) dirname=%s, fn_ignored=%s, returning %s\n", __func__, dirname, fn_ignored, sbool(result));
   return result;
}

// for e.g. card0-DP-1
bool is_card_connector_dir(const char * dirname, const char * simple_fn) {
   bool debug = false;
   DBGF(debug, "Starting. dirname=|%s|, simple_fn=|%s|", dirname, simple_fn);
   bool result = false;
   if (simple_fn)
      result = predicate_cardN_connector(simple_fn);
   DBGF(debug, "Done.     Returning: %s", sbool(result));
   return result;
}

// for e.g. card0
bool is_cardN_dir(const char * dirname, const char * simple_fn) {
   bool result = predicate_cardN(simple_fn);
// bool result = str_starts_with(simple_fn, "card");
   return result;
}


bool is_drm_dir(const char * dirname, const char * simple_fn) {
   bool result = streq(simple_fn, "drm");
   return result;
}

// does dirname/simple_fn have attribute class with value display controller or docking station?
bool has_class_display_or_docking_station(
      const char * dirname, const char * simple_fn)
{
   bool debug = false;
   bool result = false;
   if (debug)
      printf("(%s) Starting. dirname=%s, simple_fn=%s\n", __func__, dirname, simple_fn);
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
   if (debug)
      printf("(%s) class_val = %s, top_byte = 0x%02x, result=%s\n", __func__, class_val, top_byte, sbool(result) );
   free(class_val);
   return result;
}


// does dirname/simple_fn have attribute class with value display controller?
bool has_class_display(
      const char * dirname, const char * simple_fn)
{
   bool debug = false;
   bool result = false;
   if (debug)
      printf("(%s) Starting. dirname=%s, simple_fn=%s\n", __func__, dirname, simple_fn);
   char * class_val = NULL;
   int    iclass = 0;
   int    top_byte = 0;
   if ( GET_ATTR_TEXT(&class_val, dirname, simple_fn, "class") ) {
      if (str_to_int(class_val, &iclass, 16) ) {
         top_byte = iclass >> 16;
         if (top_byte == 0x03 )  // display controller
            result = true;
      }
   }
   if (debug)
      printf("(%s) class_val = %s, top_byte = 0x%02x, result=%s\n", __func__, class_val, top_byte, sbool(result) );
   free(class_val);
   return result;
}

