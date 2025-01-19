/** @file sysfs_filter_functions.c */

// Copyright (C) 2021-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <ctype.h>
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


static const char * cardN_connector_pattern = "^card[0-9]+[-]";
static const char * cardN_pattern = "^card[0-9]+$";
static const char * D_00hh_pattern = "^[0-9]+-00[0-9a-fA-F]{2}$";
static const char * i2c_N_pattern = "^i2c-([0-9]+)$";

//
// Predicate functions for filenames and attribute values, of
// typedef Filname_Filter_Func
//

/** Tests if a value is a drm card identifier, e.g. "card1"
 *
 *  @param   value value to test
 *  @result  true/false
 */
bool predicate_cardN(const char * value) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. value = |%s|\n", __func__, value);

   bool result = compile_and_eval_regex(cardN_pattern, value);

   // bool b1 = str_starts_with(value, "card") && strlen(value) == 5;
   // if (debug)
   //    printf("(%s) str_starts_with() && strlen() returned %s\n", __func__, sbool(b1));
   // assert(b2 == b1);

   if (debug)
      printf("(%s) Returning: %s. value=|%s|\n", __func__, sbool(result), value);
   return result;
}


/** Tests if a value appears to be a DRM connector, e.g "card2-DP-1"
 *  Only the initial part of the value being tested is actually checked.
 *
 *  @param  value value to test
 *  @result true/false
 */
bool predicate_cardN_connector(const char * value) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. value=|%s|\n", __func__, value);
   bool b1 = compile_and_eval_regex(cardN_connector_pattern, value);
   if (debug)
      printf("(%s) Returning %s, value=|%s|\n", __func__, sbool( b1), value);
   return b1;
}


/** Tests if a value is an I2C bus identifier, e.g. "i2c-13"
 *
 *  @param  value value to test
 *  @result true/false
 */
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
   bool debug = false;
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
   bool debug = false;
   int result = -1;
   if (str_starts_with(value,  "/dev/")) {
      result = match_i2c_N(value+5);
   }
   if (debug)
      printf("(%s) value=|%s|, returning %d\n", __func__, value, result);
   return result;
}
#endif


/** Tests if a value looks like "3-00a7", found in /sys/bus/i2c/devices
 *
 *  @param  value value to test
 *  @result true/false
 */
bool predicate_any_D_00hh(const char * value) {
   bool debug = false;

   bool b1 = compile_and_eval_regex(D_00hh_pattern, value);

   if (debug)
      printf("(%s) value=|%s|, Returning %s\n", __func__, value, sbool( b1));
   return b1;
}


/** Tests if a value (for a class attribute) indicates a display device,
 *  e.g. the value starts with "0x03"
 *
 *  @param  value value to test
 *  @result true/false
 */
bool class_display_device_predicate(const char * value) {
   return str_starts_with(value, "0x03");
}


//
// Predicate functions for filenames and attribute values, of
// typedef Filname_Filter_Func_With_Arg
//

/** Tests if a filename has a specific value
 *
 *  @param  filename  value to test
 *  @param  val       value to test against
 *  @return true/false
 */
bool fn_equal(const char * filename, const char * val) {
   return streq(filename, val);
}


/** Tests if a filename starts with a specific value
 *
 *  @param  filename  value to test
 *  @param  val       value to test against
 *  @return true/false
 */
bool fn_starts_with(const char * filename, const char * val) {
   return str_starts_with(filename, val);
}

/** Tests if a value looks like "N-00HH", found in /sys/bus/i2c/devices
 *  where HH is a specific hex value representing a bus number
 *
 *  @param  value    value to test
 *  @param  sbusno   I2c Bus number, as hex string
 *  @result true/false
 */
// e.g. "3-00hh" where hh is bus number
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
// Predicate functions for dirname/filename pairs,
// i.e. functions of typedef Dir_Filter_Func
//

#ifdef MAYBE_FUTURE
bool dirname_starts_with(const char * dirname, const char * val) {
   bool debug = false;
   DBGMSF(debug, "dirname=%s, val_fn=%s", dirname, val);
   bool result = str_starts_with(dirname, val);
   DBGMSF(debug, "Returning %s", sbool(result));
   return result;
#endif


// for e.g. dirname = "i2c-3"
bool is_i2cN_dir(const char * dirname, const char * fn_ignored) {
   bool debug = false;
   bool result = predicate_i2c_N(dirname);
   if (debug)
      printf("(%s) dirname=%s, fn_ignored=%s, returning %s\n", __func__, dirname, fn_ignored, sbool(result));
   return result;
}


// test dirname starts with "drm_dp_aux"
bool is_drm_dp_aux_subdir(const char * dirname, const char * fn_ignored) {
   bool debug = false;
   bool result = str_starts_with(dirname, "drm_dp_aux");
   if (debug)
      printf("(%s) dirname=%s, fn_ignored=%s, returning %s\n", __func__, dirname, fn_ignored, sbool(result));
   return result;
}

// for simple_fn e.g. card0-DP-1, dirname ignored
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


/** Does dirname/simple_fn have attribute class with value
 *  display controller or docking station?
 *
 *  @param  dirname   directory name
 *  @param  simple_fn sugdirectory
 *  @return true/false
 */
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


/** Does dirname/simple_fn have attribute class with value display controller?
 *  i.e. has value x03hh
 *
 *  @param  dirname   directory name
 *  @param  simple_fn subdirectory
 *  @return true/false
 */
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


/** Tests whether the filename of a dirname/filename pair
 *  has the form card<digits>-....
 *
 *  @param  dirname   directory name (ignored)
 *  @param  simple_fn value to test
 *  @return true/false
 */
bool is_drm_connector(const char * dirname, const char * simple_fn) {
   bool debug = false;
   DBGF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);

   bool result = false;
   if (str_starts_with(simple_fn, "card")) {
      char * s0 = g_strdup( simple_fn + 4);   // work around const char *
      char * s = s0;
      while (isdigit(*s)) s++;
      if (*s == '-')
         result = true;
      free(s0);
   }

   DBGF(debug, "Done.     Returning %s", SBOOL(result));
   return result;
}

