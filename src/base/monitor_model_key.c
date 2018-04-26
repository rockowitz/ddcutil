/* monitor_model_key.c
 *
 * <copyright>
 * Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#include <assert.h>
#include <ctype.h>
#include <glib-2.0/glib.h>
#include <string.h>

#include "util/edid.h"

#include "core.h"

#include "monitor_model_key.h"


/** Returns a Monitor_Model_Key as a value on the stack. */
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
   g_strlcpy(result.mfg_id,     mfg_id,     EDID_MFG_ID_FIELD_SIZE);
   g_strlcpy(result.model_name, model_name, EDID_MODEL_NAME_FIELD_SIZE);
   result.product_code = product_code;
   result.defined = true;
   return result;
}


DDCA_Monitor_Model_Key
monitor_model_key_undefined_value() {
   DDCA_Monitor_Model_Key result;
   memset(&result, 0, sizeof(result));
   // memcpy(result.marker, MONITOR_MODEL_KEY_MARKER, 4);
   return result;
}

DDCA_Monitor_Model_Key
monitor_model_key_value_from_edid(Parsed_Edid * edid) {
   DDCA_Monitor_Model_Key result;
   // memcpy(result.marker, MONITOR_MODEL_KEY_MARKER, 4);
   strncpy(result.mfg_id, edid->mfg_id, EDID_MFG_ID_FIELD_SIZE);
   strncpy(result.model_name, edid->model_name, EDID_MODEL_NAME_FIELD_SIZE);
   result.product_code = edid->product_code;
   result.defined = true;
   return result;
}



/** Allocates and initializes a new Monitor_Model_Key */
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

#ifdef UNUSED
DDCA_Monitor_Model_Key *
monitor_model_key_undefined_new() {
   DDCA_Monitor_Model_Key * result = calloc(1, sizeof(DDCA_Monitor_Model_Key));
   // memcpy(result->marker, MONITOR_MODEL_KEY_MARKER, 4);
   return result;
}
#endif

void
monitor_model_key_free(
      DDCA_Monitor_Model_Key * model_id)
{
   free(model_id);
}


/** Create a feature definition key.
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


// needed at API level?
DDCA_Monitor_Model_Key
monitor_model_key_assign(DDCA_Monitor_Model_Key old) {
   return old;
}


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
bool monitor_model_key_is_defined(DDCA_Monitor_Model_Key mmk) {
   // DDCA_Monitor_Model_Key undefined = monitor_model_key_undefined_value();
   // bool result = monitor_model_key_eq(mmk, undefined);
   return mmk.defined;
}
#endif

char *
monitor_model_string(DDCA_Monitor_Model_Key * model_id) {
   // perhaps use thread safe buffer so caller doesn't have to free
   char * result = model_id_string(
                      model_id->mfg_id,
                      model_id->model_name,
                      model_id->product_code);

   return result;
}


