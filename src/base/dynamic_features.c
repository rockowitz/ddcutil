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

#define _GNU_SOURCE      // for asprintf()

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ddcutil_status_codes.h"
#include "ddcutil_types.h"

#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"

#include "dynamic_features.h"

#ifdef REF
typedef struct {
   char *          mfg_id;     // [EDID_MFG_ID_FIELD_SIZE];
   char *          model_name;  // [EDID_MODEL_NAME_FIELD_SIZE];
   uint16_t        product_code;
   GHashTable *    features;     // array of DDCA_Feature_Metadata
} Dynamic_Features_Rec;



#define DDCA_FEATURE_METADATA_MARKER  "FMET"
/** Describes a VCP feature code, tailored for a specific VCP version */
typedef
struct {
   char                                  marker[4];      /**< always "FMET" */
   DDCA_Vcp_Feature_Code                 feature_code;   /**< VCP feature code */
   DDCA_MCCS_Version_Spec                vspec;          /**< MCCS version    */
   DDCA_Feature_Flags                    feature_flags;  /**< feature type description */
   DDCA_Feature_Value_Table              sl_values;      /**< valid when DDCA_SIMPLE_NC set */
   char *                                feature_name;   /**< feature name */
   char *                                feature_desc;   /**< feature description */
   // possibly add pointers to formatting functions
} DDCA_Feature_Metadata;


#endif

// copied from sample code, more to more generic location

/* Creates a string representation of DDCA_Feature_Flags bitfield.
 *
 * Arguments:
 *    flags      feature characteristics
 *
 * Returns:      string representation, caller must free
 */
char * interpret_feature_flags(DDCA_Version_Feature_Flags flags) {
   char * buffer = NULL;
   int rc = asprintf(&buffer, "%s%s%s%s%s%s%s%s%s%s%s%s%s",
       flags & DDCA_RO             ? "Read-Only, "                   : "",
       flags & DDCA_WO             ? "Write-Only, "                  : "",
       flags & DDCA_RW             ? "Read-Write, "                  : "",
       flags & DDCA_STD_CONT       ? "Continuous (standard), "       : "",
       flags & DDCA_COMPLEX_CONT   ? "Continuous (complex), "        : "",
       flags & DDCA_SIMPLE_NC      ? "Non-Continuous (simple), "     : "",
       flags & DDCA_COMPLEX_NC     ? "Non-Continuous (complex), "    : "",
       flags & DDCA_NC_CONT        ? "Non-Continuous with continuous subrange, " :"",
       flags & DDCA_WO_NC          ? "Non-Continuous (write-only), " : "",
       flags & DDCA_NORMAL_TABLE   ? "Table (readable), "            : "",
       flags & DDCA_WO_TABLE       ? "Table (write-only), "          : "",
       flags & DDCA_DEPRECATED     ? "Deprecated, "                  : "",
       flags & DDCA_SYNTHETIC      ? "Synthesized, "                 : ""
       );
   assert(rc >= 0);   // real life code would check for malloc() failure in asprintf()
   // remove final comma and blank
   if (strlen(buffer) > 0)
      buffer[strlen(buffer)-2] = '\0';

   return buffer;
}



// eventually move to a more shared location

void
dbgrpt_feature_metadata(
      DDCA_Feature_Metadata * md,
      int                     depth)
{
   int d0 = depth;
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_structure_loc("DDCA_Feature_Metadata", md, depth);
   rpt_vstring(d0, "Feature code:      0x%02x",  md->feature_code);
   rpt_vstring(d1, "MCCS version:      %d.%d",  md->vspec.major, md->vspec.minor);
   rpt_vstring(d1, "Feature name:      %s",     md->feature_name);
   rpt_vstring(d1, "Description:       %s",     md->feature_desc);
   char * s = interpret_feature_flags(md->feature_flags);
   rpt_vstring(d1, "Feature flags:     0x%04x", md->feature_flags);
   rpt_vstring(d1, "Interpreted flags: %s", s);
   free(s);
   if (md->sl_values) {
      rpt_label(d1, "SL values:");
      DDCA_Feature_Value_Entry * curval = md->sl_values;
      while(curval->value_code || curval->value_name) {
         rpt_vstring(d2, "0x%02x  - %s", curval->value_code, curval->value_name);
         curval++;
      }
   }
}



void
dbgrpt_dynamic_features_rec(
      Dynamic_Features_Rec * dfr,
      int                    depth)
{
   int d1 = depth+1;
   rpt_structure_loc("Dynamic_Features_Rec", dfr, depth);
   rpt_vstring(d1, "mfg_id:         %s", dfr->mfg_id);
   rpt_vstring(d1, "model_name:     %s", dfr->model_name);
   rpt_vstring(d1, "product_code:   %u", dfr->product_code);
   if (dfr->features) {
      rpt_vstring(d1, "features count: %d", g_hash_table_size(dfr->features));
      for (int ndx = 1; ndx < 256; ndx++) {
         DDCA_Feature_Metadata * cur_feature =
               g_hash_table_lookup(dfr->features, GINT_TO_POINTER(ndx));
         if (cur_feature)
            dbgrpt_feature_metadata(cur_feature, d1);

      }
   }
}


DDCA_Feature_Metadata *
get_dynamic_feature_metadata(
      Dynamic_Features_Rec * dfr,
      uint8_t                feature_code)
{
   DDCA_Feature_Metadata * result = NULL;
   if (dfr->features)
      result = g_hash_table_lookup(dfr->features, GINT_TO_POINTER(feature_code));
   return result;
}






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
      Error_Info *  err = errinfo_new2(DDCRC_BAD_DATA, caller, final_detail);
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
   bool debug = false;
   DBGMSF(debug, "keyword=|%s|", keyword);
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

   DBGMSF(debug, "Returning %s", SBOOL(ok));
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

   // TODO
}


void
free_dynamic_features_record(
      Dynamic_Features_Rec frec)
{
    // TODO
}



void finalize_feature(
      Dynamic_Features_Rec *  frec,
      DDCA_Feature_Metadata * cur_feature_metadata,
      GArray *                cur_feature_values,
      const char *            filename,
      GPtrArray *             errors)
{
   DDCA_Feature_Flags * pflags = &cur_feature_metadata->feature_flags;

   if (cur_feature_values) {
      // add terminating entry
      DDCA_Feature_Value_Entry last_entry;
      last_entry.value_code = 0x00;
      last_entry.value_name = NULL;
      g_array_append_val(cur_feature_values, last_entry);

      cur_feature_metadata->sl_values = (DDCA_Feature_Value_Entry*) cur_feature_values->data;
      // g_array_free(cur_feature_values, false);
      // cur_feature_values = NULL;
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

      else if ( *pflags & (DDCA_COMPLEX_CONT | DDCA_STD_CONT | DDCA_TABLE))
          ADD_ERROR(-1,  "Feature values specified for Continuous or Table feature");
   }

   if (*pflags & DDCA_NORMAL_TABLE & (*pflags & DDCA_WO))
      switch_bits(pflags, DDCA_NORMAL_TABLE, DDCA_WO_TABLE);


   g_hash_table_replace(
         frec->features,
         GINT_TO_POINTER(cur_feature_metadata->feature_code),
         cur_feature_metadata);

}


// TODO: Consider moving to string_util
typedef struct {
   char * word;
   char * rest;
} Tokenized;

Tokenized first_word(char * s) {
   // DBGMSG("Starting. s=|%s|", s);
   Tokenized result = {NULL,NULL};
   if (s) {
      while (*s == ' ')
            s++;
      if (*s) {
         char * end = s;
         while (*++end && *end != ' ');
         int wordlen = end-s;
         result.word = malloc( wordlen+1);
         memcpy(result.word, s, wordlen);
         result.word[wordlen] = '\0';

         while (*end == ' ')
            end++;
         if (*end == '\0')
            end = NULL;
         result.rest = end;
      }
   }
   // DBGMSG("Returning: result.word=|%s|, result.rest=|%s|", result.word, result.rest);
   return result;
}


char * canonicalize_feature_value(char * string_value) {
   assert(string_value);

   int bufsz = strlen(string_value) + 4;
   char * buf = calloc(1, bufsz);
   if (*string_value == 'X' || *string_value == 'x' ) {
      snprintf(buf, bufsz, "0x%s", string_value+1);
   }
   else if (*(string_value + strlen(string_value)-1) == 'H' ||
            *(string_value + strlen(string_value)-1) == 'h' )
   {
      int newlen = strlen(string_value)-1;
      snprintf(buf, bufsz, "0x%.*s", newlen, string_value);
   }
   else
      strcpy(buf, string_value);
   // DBGMSG("string_value=|%s|, returning |%s|", string_value, buf);
   return buf;
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
                              g_direct_equal,   // g_int_equal,
                              NULL,                     // key_destroy_func
                              free_feature_metadata);   // value_destroy_func

   int     linectr = 0;
   DDCA_Feature_Metadata * cur_feature_metadata = NULL;
   GArray * cur_feature_values = NULL;

   while ( linectr < lines->len ) {
      char *  line = g_ptr_array_index(lines,linectr);
      linectr++;

      Tokenized t1, t2  = {0,0};
      t1 = first_word(line);
      if (t1.word && *t1.word != '*' && *t1.word != '#') {
         t2 = first_word(t1.rest);
         if (!t2.word) {
            ADD_ERROR(linectr, "Invalid data \"%s\"", line);
         }
         else {
            if (streq(t1.word, "PRODUCT_CODE")) {
               product_code_seen = true;
               int ival;
               bool ok = str_to_int2(t2.word, &ival, 10);
               // DBGMSG("ival: %d", ival);
               if (!ok) {
                   ADD_ERROR(linectr, "Invalid product_code \"%s\"", t2.word);
               }
               else if (ival != product_code) {
                  ADD_ERROR(linectr, "Unexpected product_code \"%s\"", t2.word);
               }
            }
            else if (streq(t1.word, "MFG_ID")) {
               mfg_id_seen = true;
               if ( !streq(t2.word, mfg_id) ) {
                  ADD_ERROR(linectr, "Unexpected manufacturer id \"%s\"", t2.word);
               }
            }
            else if (streq(t1.word, "MODEL")) {
               model_name_seen = true;
               if ( !streq(t1.rest, model_name) ) {
                  ADD_ERROR(linectr, "Unexpected model name \"%s\"", t1.rest);
               }

            }
            else if (streq(t1.word, "ATTRS")) {
               if (!cur_feature_metadata) {
                  ADD_ERROR(linectr, "ATTRS before FEATURE_CODE");
               }
               else {
                  Tokenized t = first_word(t1.rest);
                  while (t.word) {
                     // set values in cur_feature_metadata->feature_flags
                     bool ok = attr_keyword(cur_feature_metadata, t.word);
                     if (!ok) {
                        ADD_ERROR(linectr, "Invalid attribute \"%s\"", t.word);
                     }
                     free(t.word);
                     t = first_word(t.rest);
                  }
               }
            }

            else if (streq(t1.word, "FEATURE_CODE")) {
               // n. cur_feature_metadata saved in frec
               if (cur_feature_metadata) {
                  finalize_feature(
                        frec,
                        cur_feature_metadata,
                        cur_feature_values,
                        filename,
                        errors);
                  if (cur_feature_values) {
                     g_array_free(cur_feature_values, false);
                     cur_feature_values = NULL;
                  }
               }

               cur_feature_metadata = calloc(1, sizeof(DDCA_Feature_Metadata));
               memcpy(cur_feature_metadata->marker, DDCA_FEATURE_METADATA_MARKER, 4);

               char * feature_code = t2.word;
               char * feature_name = t2.rest;

               if (!feature_name) {
                  ADD_ERROR(linectr, "Invalid VCP data \"%s\"", line);
               }
               else {   // found feature id and value
                  // Byte feature_id;
                  int feature_id;
                  // todo: handle xnn as well as nn ?
                  //bool ok = hhs_to_byte_in_buf(feature_code, &feature_id);
                  char * can = canonicalize_feature_value(feature_code);

                  bool ok = str_to_int2(can, &feature_id, 16);
                  free(can);
                  if (!ok) {
                     ADD_ERROR(linectr, "Invalid feature code \"%s\"", feature_code);
                  }
                  else {     // valid opcode
                     cur_feature_metadata->feature_code = feature_id;
                     cur_feature_metadata->feature_name = feature_name;
                     cur_feature_metadata->feature_desc = NULL;   // ignore for now
                  }
               }
            }

            else if (streq(t1.word, "VALUE")) {
               if (!t2.rest) {
                  ADD_ERROR(linectr, "Invalid feature value data \"%s\"", line);
               }
               else {   // found value code and name
                  int feature_value;
                   // Byte feature_value;
                  // bool ok = hhs_to_byte_in_buf(s1, &feature_value);
                  char * canonical = canonicalize_feature_value(t2.word);
                  bool ok = str_to_int2(canonical, &feature_value, 0);
                  free(canonical);
                  if (!ok) {
                     ADD_ERROR(linectr, "Invalid feature value \"%s\"", t2.word);
                  }
                  else {     // valid feature value
                     if (!cur_feature_values)
                        cur_feature_values = g_array_new(false, false, sizeof(DDCA_Feature_Value_Entry));
                     DDCA_Feature_Value_Entry entry;
                     entry.value_code = feature_value;
                     entry.value_name = strdup(t2.rest);
                     g_array_append_val(cur_feature_values, entry);
                  }
               }
            }

            else {
               ADD_ERROR(linectr, "Unexpected field \"%s\"", t1.word);
            }
         }    // more than 1 field on line
         if (t2.word)
            free(t2.word);
      }       // non-comment line
      if (t1.word)
         free(t1.word);
   }          // one line of file

   if (cur_feature_metadata) {
      finalize_feature(
            frec,
            cur_feature_metadata,
            cur_feature_values,
            filename,
            errors);
      if (cur_feature_values) {
         g_array_free(cur_feature_values, false);
         cur_feature_values = NULL;
      }
   }

   if (g_hash_table_size(frec->features) == 0)
      ADD_ERROR(-1, "No feature codes defined");

   if (!mfg_id_seen)
      ADD_ERROR(-1, "Missing MFG_ID");

   if (!model_name_seen)
      ADD_ERROR(-1, "Missing MODEL_NAME");

   if (!product_code_seen)
      ADD_ERROR(-1, "Missing PRODUCT_CODE");

   if (errors->len > 0) {
      // DBGMSG("errors->len=%d", errors->len);
      // DBGMSG("errors->pdata: %p", errors->pdata);
      // DBGMSG("&errors->pdata: %p", &errors->pdata);
      // DBGMSG("*errors->pdata: %p", *errors->pdata);
      // Error_Info * first = g_ptr_array_index(errors, 0);
      // DBGMSG("first: %p", first);
      char * detail = gaux_asprintf("Error(s) processing monitor definition file: %s", filename);
      master_err = errinfo_new_with_causes2(
                            DDCRC_BAD_DATA,
                            (Error_Info**) errors->pdata,
                            errors->len,
                            __func__,
                            detail);
      free(detail);
      DBGMSG("After errinfo_new_with_causes2()");
      g_ptr_array_free(errors, false);
      // free constructed record
      *dynamic_featues_loc = NULL;
   }
   else {
      g_ptr_array_free(errors, false);
      *dynamic_featues_loc = frec;
      dbgrpt_dynamic_features_rec(frec, 0);
   }

   return master_err;
}



