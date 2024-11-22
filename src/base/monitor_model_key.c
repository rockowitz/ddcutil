/** @file monitor_model_key.c
 *
 *  Uniquely identifies a monitor model using its manufacturer id,
 *  model name, and product code, as listed in the EDID.
 */

// Copyright (C) 2018-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <ctype.h>
#include <glib-2.0/glib.h>
#include <string.h>

#include "util/debug_util.h"
#include "util/edid.h"
#include "util/glib_util.h"
#include "util/regex_util.h"

#include "core.h"
#include "rtti.h"

#include "monitor_model_key.h"

#define FIXUP_MODEL_NAME(_name) \
   for (int i=0; _name[i] && i < EDID_MODEL_NAME_FIELD_SIZE; i++) { \
      if (!isalnum(_name[i])) \
         _name[i] = '_'; \
   }


/** Returns a Monitor_Model_Key on the stack. */
Monitor_Model_Key
mmk_value(
      const char *   mfg_id,
      const char *   model_name,
      uint16_t       product_code)
{
   // verify that constants used in public ddcutil_c_types.h match those in util/edid.h
   // assert(DDCA_EDID_MFG_ID_FIELD_SIZE == EDID_MFG_ID_FIELD_SIZE);
   // assert(DDCA_EDID_MODEL_NAME_FIELD_SIZE == EDID_MODEL_NAME_FIELD_SIZE);

   assert(mfg_id && strlen(mfg_id) < EDID_MFG_ID_FIELD_SIZE);
   assert(model_name && strlen(model_name) < EDID_MODEL_NAME_FIELD_SIZE);

   Monitor_Model_Key  result;
   // memcpy(result.marker, MONITOR_MODEL_KEY_MARKER, 4);
   (void) g_strlcpy(result.mfg_id,     mfg_id,     EDID_MFG_ID_FIELD_SIZE);
   STRLCPY(result.model_name, model_name, EDID_MODEL_NAME_FIELD_SIZE);
   FIXUP_MODEL_NAME(result.model_name);
   result.product_code = product_code;
   result.defined = true;
   return result;
}


/** Returns an "undefined" Monitor_Model_Key on the stack. */
Monitor_Model_Key
mmk_undefined_value() {
   Monitor_Model_Key result;
   memset(&result, 0, sizeof(result));
   // memcpy(result.marker, MONITOR_MODEL_KEY_MARKER, 4);
   return result;
}


/** Returns a Monitor Model Key on the stack with values obtained
 *  from an EDID */
Monitor_Model_Key
mmk_value_from_edid(Parsed_Edid * edid) {
   Monitor_Model_Key result;
   // memcpy(result.marker, MONITOR_MODEL_KEY_MARKER, 4);
   /* coverity[OVERRUN] */             (void) g_strlcpy(result.mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);
   /* coverity[overrun-buffer-val] */  (void) g_strlcpy(result.mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);
   /* coverity[access_debuf_const] */  (void) g_strlcpy(result.mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);

   // STRLCPY(result.mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);
   memcpy(result.mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);
   /* coverity[OVERRUN] */ (void) g_strlcpy(result.model_name, edid->model_name, EDID_MODEL_NAME_FIELD_SIZE);
   FIXUP_MODEL_NAME(result.model_name);
   result.product_code = edid->product_code;

   result.defined = true;
   return result;
}


/** Allocates and initializes a new Monitor_Model_Key on the heap. */
Monitor_Model_Key *
mmk_new(
      const char *   mfg_id,
      const char *   model_name,
      uint16_t       product_code)
{
   assert(mfg_id && strlen(mfg_id) < EDID_MFG_ID_FIELD_SIZE);
   assert(model_name && strlen(model_name) < EDID_MODEL_NAME_FIELD_SIZE);

   Monitor_Model_Key * result = calloc(1, sizeof(Monitor_Model_Key));
   // memcpy(result->marker, MONITOR_MODEL_KEY_MARKER, 4);
   STRLCPY(result->mfg_id,     mfg_id,     EDID_MFG_ID_FIELD_SIZE);
   STRLCPY(result->model_name, model_name, EDID_MODEL_NAME_FIELD_SIZE);
   FIXUP_MODEL_NAME(result->model_name);
   result->product_code = product_code;
   result->defined = true;
   return result;
}


Monitor_Model_Key
mmk_value_from_string(const char * sval) {
   bool debug = true;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "sval = |%s|", sval);

   static const char * mmk_pattern = "^([A-Z]{3})-(.{0,13})-([0-9]*)$";

   regmatch_t  matches[4];

   bool ok =  compile_and_eval_regex_with_matches(
         mmk_pattern,
         sval,
         4,   //       max_matches,
         matches);

   Monitor_Model_Key result = mmk_undefined_value();

   if (ok) {
      // for (int kk = 0; kk < 4; kk++) {
      //    rpt_vstring(1, "match %d, substring start=%d, end=%d", kk, matches[kk].rm_so, matches[kk].rm_eo);
      // }
      char * mfg_id         = substr(sval, matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
      char * model_name     = substr(sval, matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
      char * product_code_s = substr(sval, matches[3].rm_so, matches[3].rm_eo - matches[3].rm_so);
      FIXUP_MODEL_NAME(model_name);
      // DBGF(debug, "mfg_id=|%s|", mfg_id);
      // DBGF(debug, "model_name=|%s|", model_name);
      // DBGF(debug, "product_code_s=|%s|", product_code_s);

      uint16_t product_code;
      int ival;
      ok = str_to_int(product_code_s, &ival, 10);
      product_code = (uint16_t) ival;
      assert(ok);
      DBGF(debug, "product_code: %d", product_code);

      result = mmk_value(mfg_id, model_name, product_code);

      free(mfg_id);
      free(model_name);
      free(product_code_s);
   }

   DBGTRC_DONE(true, DDCA_TRC_NONE, "Returning: %s", mmk_repr(result));
   return result;
}


Monitor_Model_Key *
mmk_new_from_value(Monitor_Model_Key mmk) {
   bool debug = true;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "mmk=%s", mmk_repr(mmk));

   Monitor_Model_Key * result = NULL;
   if (mmk.defined) {
      result = calloc(1, sizeof(Monitor_Model_Key));
      memcpy(result, &mmk, sizeof(Monitor_Model_Key));
      Monitor_Model_Key * result2 = mmk_new(
            mmk.mfg_id,
            mmk.model_name,
            mmk.product_code);
      assert(monitor_model_key_eq(*result, *result2));
      assert(monitor_model_key_eq(mmk, *result));
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning: %p -> %s", result, mmk_repr(*result));
   return result;
}


Monitor_Model_Key *
mmk_new_from_string(const char * s) {
   bool debug = true;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "s=|s|", s);

   Monitor_Model_Key * result = NULL;
   Monitor_Model_Key mmk = mmk_value_from_string(s);
   if (mmk.defined) {
      result = mmk_new_from_value(mmk);
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning: %p", result);
   return result;
}


bool is_valid_mmk(const char * sval) {
   Monitor_Model_Key mmk = mmk_value_from_string(sval);
   return mmk.defined;
}



Monitor_Model_Key *
mmk_new_from_edid(
      Parsed_Edid * edid)
{
   Monitor_Model_Key * result = NULL;
   if (edid) {
      result = calloc(1, sizeof(Monitor_Model_Key));
      memcpy(result->mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);
      memcpy(result->model_name, edid->model_name, EDID_MODEL_NAME_FIELD_SIZE);
      FIXUP_MODEL_NAME(result->model_name);
      result->product_code = edid->product_code;
      result->defined = true;
   }
   return result;
}


/** Frees a Monitor_Model_Key */
void
mmk_free(
      Monitor_Model_Key * mmk)
{
   free(mmk);
}


/** Compares 2 Monitor_Model_Key values for equality
 *
 * \param  mmk1
 * \param  mmk2
 * \return true/false
 */
bool
monitor_model_key_eq(
      Monitor_Model_Key mmk1,
      Monitor_Model_Key mmk2)
{
   bool result = false;
   if (!mmk1.defined && !mmk2.defined) {
      result = true;
   }
   else if (mmk1.defined && mmk2.defined) {
      result =
         (mmk1.product_code == mmk2.product_code      &&
          strcmp(mmk1.mfg_id, mmk2.mfg_id) == 0       &&
          strcmp(mmk1.model_name, mmk2.model_name) == 0 );
   }
   return result;
}


#ifdef UNUSED
Monitor_Model_Key *
monitor_model_key_undefined_new() {
   Monitor_Model_Key * result = calloc(1, sizeof(Monitor_Model_Key));
   // memcpy(result->marker, MONITOR_MODEL_KEY_MARKER, 4);
   return result;
}

// needed at API level?
Monitor_Model_Key
monitor_model_key_assign(Monitor_Model_Key old) {
   return old;
}

bool monitor_model_key_is_defined(Monitor_Model_Key mmk) {
   // DDCA_Monitor_Model_Key undefined = monitor_model_key_undefined_value();
   // bool result = monitor_model_key_eq(mmk, undefined);
   return mmk.defined;
}
#endif


/** Returns a string form of a Monitor_Model_Key, suitable for use as an
 *  identifier in file names, hash keys, etc.
 *
 *  The returned value has the form MFG-MODEL-PRODUCT_CODE.
 *
 *  Non-alphanumeric characters (commonly " ") in the model name are replaced by "_".
 *
 *  \param   mfg
 *  \param   model_name
 *  \param   product_code
 *  \return  key string (caller must free or save in persistent data structure)
 */
char *
mmk_model_id_string(
      const char *  mfg,
      const char *  model_name,
      uint16_t      product_code)
{
   bool debug = false;
   DBGMSF(debug, "Starting. mfg=|%s|, model_name=|%s| product_code=%u",
                 mfg, model_name, product_code);

   assert(mfg);
   assert(model_name);
   char * model_name2 = g_strdup(model_name);
   FIXUP_MODEL_NAME(model_name2);

   char * result = g_strdup_printf("%s-%s-%u", mfg, model_name2, product_code);
   free(model_name2);
   DBGMSF(debug, "Returning: |%s|", result);
   return result;
}


/** Returns a string representation of a Monitor_Model_Key in a form
 *  suitable for file names, hash keys, etc.
 *
 *  The value returned has the same form as returned by #model_id_string.
 *
 *  \param  model_id
 *  \return string representation
 *
 *  The value returned will be valid until the next call to this function in
 *  the current thread.  Caller should not free.
 */
char *
mmk_string(Monitor_Model_Key * model_id) {
   static GPrivate  dh_buf_key = G_PRIVATE_INIT(g_free);
   const int bufsz = 100;
   char * buf = get_thread_fixed_buffer(&dh_buf_key, bufsz);

   char * result = NULL;
   // perhaps use thread safe buffer so caller doesn't have to free
   if (model_id) {
      char * s  = mmk_model_id_string(
                         model_id->mfg_id,
                         model_id->model_name,
                         model_id->product_code);
      strcpy(buf, s);
      free(s);
      result = buf;
   }

   return result;
}


/** Returns a string representation of a Monitor_Model_Key in a format
 *  suitable for debug messages.
 *
 *  \param  mmk  Monitor_Model_Key value
 *  \return string representation
 *
 *  The value returned will be valid until the next call to this function in
 *  the current thread.  Caller should not free.
 */
char * mmk_repr(Monitor_Model_Key mmk) {
   static GPrivate  dh_buf_key = G_PRIVATE_INIT(g_free);
   const int bufsz = 100;
   char * buf = get_thread_fixed_buffer(&dh_buf_key, bufsz);

   if (!mmk.defined)
      strcpy(buf, "[Undefined]");
   else
      snprintf(buf, 100, "[%s,%s,%d]", mmk.mfg_id, mmk.model_name, mmk.product_code);
   return buf;
}


void init_monitor_model_key() {
   RTTI_ADD_FUNC(mmk_value_from_string);
   RTTI_ADD_FUNC(mmk_new_from_value);
}


#undef FIXUP_MODEL_NAME
