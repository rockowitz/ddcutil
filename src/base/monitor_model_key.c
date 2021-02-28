/** \file monitor_model_key.c
 *  Uniquely identifies a monitor model using its manufacturer id,
 *  model name, and product code, as listed in the EDID.
 */

// Copyright (C) 2018-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <ctype.h>
#include <glib-2.0/glib.h>
#include <string.h>

#include "util/edid.h"
#include "util/glib_util.h"

#include "core.h"

#include "monitor_model_key.h"


/** Returns a Monitor_Model_Key on the stack. */
DDCA_Monitor_Model_Key
monitor_model_key_value(
      const char *   mfg_id,
      const char *   model_name,
      uint16_t       product_code)
{
   // verify that constants used in public ddcutil_c_types.h match those in util/edid.h
   // assert(DDCA_EDID_MFG_ID_FIELD_SIZE == EDID_MFG_ID_FIELD_SIZE);
   // assert(DDCA_EDID_MODEL_NAME_FIELD_SIZE == EDID_MODEL_NAME_FIELD_SIZE);

   assert(mfg_id && strlen(mfg_id) < EDID_MFG_ID_FIELD_SIZE);
   assert(model_name && strlen(model_name) < EDID_MODEL_NAME_FIELD_SIZE);

   DDCA_Monitor_Model_Key  result;
   // memcpy(result.marker, MONITOR_MODEL_KEY_MARKER, 4);
   STRLCPY(result.mfg_id,     mfg_id,     EDID_MFG_ID_FIELD_SIZE);
   STRLCPY(result.model_name, model_name, EDID_MODEL_NAME_FIELD_SIZE);
   result.product_code = product_code;
   result.defined = true;
   return result;
}


/** Returns an "undefined" Monitor_Model_Key on the stack. */
DDCA_Monitor_Model_Key
monitor_model_key_undefined_value() {
   DDCA_Monitor_Model_Key result;
   memset(&result, 0, sizeof(result));
   // memcpy(result.marker, MONITOR_MODEL_KEY_MARKER, 4);
   return result;
}


/** Returns a Monitor Model Key on the stack with values obtained
 *  from an EDID */
DDCA_Monitor_Model_Key
monitor_model_key_value_from_edid(Parsed_Edid * edid) {
   DDCA_Monitor_Model_Key result;
   // memcpy(result.marker, MONITOR_MODEL_KEY_MARKER, 4);
   STRLCPY(result.mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);
   STRLCPY(result.model_name, edid->model_name, EDID_MODEL_NAME_FIELD_SIZE);
   result.product_code = edid->product_code;
   result.defined = true;
   return result;
}


/** Allocates and initializes a new Monitor_Model_Key on the heap. */
DDCA_Monitor_Model_Key *
monitor_model_key_new(
      const char *   mfg_id,
      const char *   model_name,
      uint16_t       product_code)
{
   assert(mfg_id && strlen(mfg_id) < EDID_MFG_ID_FIELD_SIZE);
   assert(model_name && strlen(model_name) < EDID_MODEL_NAME_FIELD_SIZE);

   DDCA_Monitor_Model_Key * result = calloc(1, sizeof(DDCA_Monitor_Model_Key));
   // memcpy(result->marker, MONITOR_MODEL_KEY_MARKER, 4);
   g_strlcpy(result->mfg_id,     mfg_id,     EDID_MFG_ID_FIELD_SIZE);
   g_strlcpy(result->model_name, model_name, EDID_MODEL_NAME_FIELD_SIZE);
   result->product_code = product_code;
   result->defined = true;
   return result;
}


/** Frees a Monitor_Model_Key */
void
monitor_model_key_free(
      DDCA_Monitor_Model_Key * model_id)
{
   free(model_id);
}


/** Compares 2 Monitor_Model_Key values for equality */
bool
monitor_model_key_eq(
      DDCA_Monitor_Model_Key mmk1,
      DDCA_Monitor_Model_Key mmk2)
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
DDCA_Monitor_Model_Key *
monitor_model_key_undefined_new() {
   DDCA_Monitor_Model_Key * result = calloc(1, sizeof(DDCA_Monitor_Model_Key));
   // memcpy(result->marker, MONITOR_MODEL_KEY_MARKER, 4);
   return result;
}

// needed at API level?
DDCA_Monitor_Model_Key
monitor_model_key_assign(DDCA_Monitor_Model_Key old) {
   return old;
}

bool monitor_model_key_is_defined(DDCA_Monitor_Model_Key mmk) {
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
model_id_string(
      const char *  mfg,
      const char *  model_name,
      uint16_t      product_code)
{
   bool debug = false;
   DBGMSF(debug, "Starting. mfg=|%s|, model_name=|%s| product_code=%u",
                 mfg, model_name, product_code);

   assert(mfg);
   assert(model_name);
   char * model_name2 = strdup(model_name);
   for (int ndx = 0; ndx < strlen(model_name2); ndx++) {
      if ( !isalnum(model_name2[ndx]) )
         model_name2[ndx] = '_';
   }

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
monitor_model_string(DDCA_Monitor_Model_Key * model_id) {
   static GPrivate  dh_buf_key = G_PRIVATE_INIT(g_free);
   const int bufsz = 100;
   char * buf = get_thread_fixed_buffer(&dh_buf_key, bufsz);

   char * result = NULL;
   // perhaps use thread safe buffer so caller doesn't have to free
   if (model_id) {
      char * s  = model_id_string(
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
 */
char * mmk_repr(DDCA_Monitor_Model_Key mmk) {
   // TODO: make thread safe
   static char buf[100];
   if (!mmk.defined)
      strcpy(buf, "[Undefined]");
   else
      snprintf(buf, 100, "[%s,%s,%d]", mmk.mfg_id, mmk.model_name, mmk.product_code);
   return buf;
}

