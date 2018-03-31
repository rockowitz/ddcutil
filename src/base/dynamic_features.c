/* dynamic_features.c
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
#include <string.h>

#include "ddcutil_status_codes.h"
#include "ddcutil_types.h"

#include "util/string_util.h"

#include "base/core.h"

#include "dynamic_features.h"


/* static */ void
add_error(
      GPtrArray *  errors,
      const char * filename,
      int          linectr,
      const char * caller,
      char *       fmt, ...)
{
      char detail[200];
      char xdetail[300];
      char * final_detail;
      va_list(args);
      va_start(args, fmt);
      vsnprintf(detail, 200, fmt, args);
      if (filename) {
         if (linectr > 0)
            snprintf(xdetail, sizeof(xdetail), "%s at line %d of file %s", detail, linectr, filename);
         else
            snprintf(xdetail, sizeof(xdetail), "%s in file %s", detail, filename);
         final_detail = xdetail;
      }
      else {
         final_detail = detail;
      }
      Error_Info *  err = errinfo_new2(DDCRC_BAD_DATA, final_detail, caller);
      g_ptr_array_add(errors, err);
      va_end(args);
}

#define ADD_ERROR(_linectr, _fmt, ...) \
   add_error(errors, filename, _linectr, __func__, _fmt, ##__VA_ARGS__)



/* static */ bool
attr_keyword(
      DDCA_Feature_Metadata * cur_feature_metadata,
      char *                  keyword)
{
   bool ok = true;
   DDCA_Feature_Flags * pflags = &cur_feature_metadata->feature_flags;
   if (streq(keyword, "RW"))
      *pflags |= DDCA_RW;
   else if (streq(keyword, "RO"))
      *pflags |= DDCA_RO;
   else if (streq(keyword, "WO"))
      *pflags |= DDCA_WO;

   else if (streq(keyword, "C"))
      *pflags |= DDCA_STD_CONT;
   else if (streq(keyword, "CCONT"))
      *pflags |= DDCA_COMPLEX_CONT;
   else if (streq(keyword, "NC"))
      *pflags |= DDCA_COMPLEX_NC;
   else if (streq(keyword, "T"))
      *pflags |= DDCA_TABLE;

   else
      ok = false;

   return ok;
}


/* static */ void
switch_bits(
      DDCA_Feature_Flags * pflags,
      uint16_t             old_bit,
      uint16_t             new_bit)
{
   *pflags &= ~old_bit;
   *pflags |= new_bit;
}


void
free_feature_metadata(
      gpointer data)
{
   DDCA_Feature_Metadata * info = (DDCA_Feature_Metadata*) data;
   assert(memcmp(info->marker, DDCA_FEATURE_METADATA_MARKER, 4) == 0);
   // DDCA_Feature_Metadata needs a marker field

   // TO IMPLEMENT
}



Error_Info *
create_monitor_dynamic_features(
      const char * mfg_id,
      const char * model_name,
      uint16_t     product_code,
      GPtrArray *  lines,
      const char * filename,     // may be NULL
      Dynamic_Features_Rec ** dynamic_featues_loc)
{
   Error_Info * master_err = NULL;
   GPtrArray * errors = g_ptr_array_new();
   Dynamic_Features_Rec * frec = calloc(1, sizeof(Dynamic_Features_Rec));
   frec->mfg_id       = strdup(mfg_id);
   frec->model_name   = strdup(model_name);
   frec->product_code = product_code;
   bool mfg_id_seen       = false;
   bool model_name_seen   = false;
   bool product_code_seen = false;
   frec->features = g_hash_table_new_full(
                              g_direct_hash,
                              g_int_equal,
                              NULL,                     // key_destroy_func
                              free_feature_metadata);   // value_destroy_func

   int     ct;
   int     linectr = 0;
   DDCA_Feature_Metadata * cur_feature_metadata = NULL;
   GArray * cur_feature_values = NULL;

   while ( linectr < lines->len ) {
      char *  line = NULL;
      char    s0[32], s1[257], s2[16];
      char *  head;
      char *  rest;

      line = g_ptr_array_index(lines,linectr);
      linectr++;

      *s0 = '\0'; *s1 = '\0'; *s2 = '\0';
      head = line;
      while (*head == ' ') head++;
      ct = sscanf(head, "%31s %256s %15s", s0, s1, s2);
      if (ct > 0 && *s0 != '*' && *s0 != '#') {
         if (ct == 1) {
            ADD_ERROR(linectr, "Invalid data \"%s\"", line);
         }
         else {
            rest = head + strlen(s0);;
            while (*rest == ' ') rest++;
            char * last = rest + strlen(rest) - 1;
            // we already parsed a second token, so don't need to worry that last becomes < head
            while (*last == ' ' || *last == '\n') {
               *last-- = '\0';
            }
            // DBGMSG("rest=|%s|", rest );

            if (streq(s0, "PRODUCT_CODE")) {
               product_code_seen = true;
               int ival;
               bool ok = str_to_int(s1, &ival);
               if (!ok) {
                   ADD_ERROR(linectr, "Invalid product_code \"%s\"", s1);
               }
               else if (ival != product_code) {
                  ADD_ERROR(linectr, "Unexpected product_code \"%s\"", s1);
               }
               // ignore for now
            }
            else if (streq(s0, "MFG_ID")) {
               mfg_id_seen = true;
               if ( !streq(s1, mfg_id) ) {
                  ADD_ERROR(linectr, "Unexpected manufacturer id \"%s\"", s1);
               }
            }
            else if (streq(s0, "MODEL")) {
               model_name_seen = true;
               if ( !streq(s1, model_name) ) {
                  ADD_ERROR(linectr, "Unexpected model name \"%s\"", s1);
               }

            }
            else if (streq(s0, "ATTRS")) {
               if (!cur_feature_metadata) {
                  ADD_ERROR(linectr, "ATTRS before FEATURE_CODE");
               }
               else {
                  // set values in cur_feature_metadata->feature_flags
                  bool ok = attr_keyword(cur_feature_metadata, s1);
                  if (!ok) {
                     ADD_ERROR(linectr, "Invalid attribute \"%s\"", s1);
                  }
                  if (ct > 2) {
                     bool ok = attr_keyword(cur_feature_metadata, s2);
                     if (!ok) {
                        ADD_ERROR(linectr, "Invalid attribute \"%s\"", s2);
                     }
                  }
               }
            }

            else if (streq(s0, "FEATURE_CODE")) {
               if (cur_feature_metadata) {
                  DDCA_Feature_Flags * pflags = &cur_feature_metadata->feature_flags;

                  if (cur_feature_values) {
                      // convert and set cur_feature_metadata->sl_values
                     // free cur_feature_values
                     DDCA_Feature_Value_Entry last_entry;
                     last_entry.value_code = 0x00;
                     last_entry.value_name = NULL;
                     g_array_append_val(cur_feature_values, last_entry);
                     cur_feature_metadata->sl_values = (DDCA_Feature_Value_Entry*) cur_feature_values->data;
                     g_array_free(cur_feature_values, false);
                     cur_feature_values = NULL;
                  }


                  if ( *pflags & (DDCA_RW | DDCA_RO | DDCA_WO) )
                     *pflags |= DDCA_RW;

                  if (cur_feature_metadata->sl_values) {
                     if (cur_feature_metadata->feature_flags & DDCA_COMPLEX_NC) {
                        if ( *pflags & DDCA_WO)
                           switch_bits(pflags, DDCA_COMPLEX_NC, DDCA_WO_NC);
                        else
                           switch_bits(pflags, DDCA_COMPLEX_NC, DDCA_SIMPLE_NC);
                     }

                     else if ( *pflags & (DDCA_COMPLEX_CONT | DDCA_TABLE))
                         ADD_ERROR(-1,  "Feature values specified for Continuous or Table feature");
                  }

                  if (*pflags & DDCA_NORMAL_TABLE & (*pflags & DDCA_WO))
                     switch_bits(pflags, DDCA_NORMAL_TABLE, DDCA_WO_TABLE);

                  if (!mfg_id_seen)
                     ADD_ERROR(-1, "Missing MFG_ID");

                  if (!model_name_seen)
                     ADD_ERROR(-1, "Missing MODEL_NAME");

                  if (!product_code_seen)
                     ADD_ERROR(-1, "Missing PRODUCT_CODE");


                  g_hash_table_replace(
                        frec->features,
                        GINT_TO_POINTER(cur_feature_metadata->feature_code),
                        cur_feature_metadata);

               }

               cur_feature_metadata = calloc(1, sizeof(DDCA_Feature_Metadata));
               memcpy(cur_feature_metadata->marker, DDCA_FEATURE_METADATA_MARKER, 4);

               if (ct != 3) {
                  ADD_ERROR(linectr, "Invalid VCP data \"%s\"", line);
               }
               else {   // found feature id and value
                  Byte feature_id;
                  bool ok = hhs_to_byte_in_buf(s1, &feature_id);
                  if (!ok) {
                     ADD_ERROR(linectr, "Invalid feature code \"%s\"", s1);
                  }
                  else {     // valid opcode
                     cur_feature_metadata->feature_code = feature_id;
                     cur_feature_metadata->feature_name = s2;   // TODO: doesn't handle blanks in name
                     cur_feature_metadata->feature_desc = NULL;   // ignore for now
                  }
               }
            }

            else if (streq(s0, "VALUE")) {
               if (ct != 3) {
                  ADD_ERROR(linectr, "Invalid feature value data \"%s\"", line);
               }
               else {   // found value code and name
                  Byte feature_value;
                  bool ok = hhs_to_byte_in_buf(s1, &feature_value);
                  if (!ok) {
                     ADD_ERROR(linectr, "Invalid feature value \"%s\"", s1);
                  }


                  else {     // valid feature value
                     if (!cur_feature_values)
                        cur_feature_values = g_array_new(false, false, sizeof(DDCA_Feature_Value_Entry));
                     DDCA_Feature_Value_Entry entry;
                     entry.value_code = feature_value;
                     entry.value_name = strdup(s2);
                     g_array_append_val(cur_feature_values, entry);
                  }
               }
            }

            else {
               ADD_ERROR(linectr, "Unexpected field \"%s\"", s0);
            }
         }    // more than 1 field on line
      }       // non-comment line
   }          // one line of file

   if (errors->len > 0) {
      master_err = errinfo_new_with_causes2(
                            DDCRC_BAD_DATA,
                            "Error parsing file",
                            (Error_Info**) &errors->pdata,
                            errors->len,
                            __func__);
      g_ptr_array_free(errors, false);
      // free constructed record
      *dynamic_featues_loc = NULL;
   }
   else {
      g_ptr_array_free(errors, false);
      *dynamic_featues_loc = frec;
   }

   return master_err;
}



