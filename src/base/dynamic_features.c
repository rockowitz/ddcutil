/** @file dynamic_features.c
 *
 *  Dynamic Feature Record definition, creation, destruction, and conversion
 */

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#define _GNU_SOURCE      // for asprintf()

/** \cond */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
/** \endcond */

#include "ddcutil_status_codes.h"
#include "ddcutil_types.h"

#include "util/report_util.h"
#include "util/string_util.h"

#include "core.h"
#include "monitor_model_key.h"
#include "vcp_version.h"

#include "dynamic_features.h"


//
// Generic functions that probably belong elsewhere
//

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
   int rc = asprintf(&buffer, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
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
       flags & DDCA_USER_DEFINED   ? "User-defined, "                : "",
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
// rpt_vstring(d1, "MCCS version:      %d.%d",  md->vspec.major, md->vspec.minor);
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


// TODO: modify parse_vcp_value() in app_setvcp.c to use this function.
//       Move this function to a more general location.

/** Converts a string containing a (possible) hex value to canonical form.
 *
 *  If the value starts with "x" or "X", or ends with "h" or "H", it is
 *  modified to start with "0x".
 *  Otherwise, the returned value is identical to the input value.
 *
 *  @param string_value  value to convert
 *  @return canonicalized value (caller is responsible for freeing)
 *
 *  @remark
 *  Consider converting to a function that uses a thread-specific buffer, making
 *  the returned value valid until the next call to this function on the current
 *  thread.  Would relieve the caller of responsibility for freeing the value.
 */

char * canonicalize_possible_hex_value(char * string_value) {
   assert(string_value);

   int bufsz = strlen(string_value) + 1 + 1;  // 1 for possible increased length, 1 for terminating null
   char * buf = calloc(1, bufsz);
   if (*string_value == 'X' || *string_value == 'x' ) {
      // increases string size by 1
      snprintf(buf, bufsz, "0x%s", string_value+1);
   }
   else if (*(string_value + strlen(string_value)-1) == 'H' ||
            *(string_value + strlen(string_value)-1) == 'h' )
   {
      // increases string size by 1
      int newlen = strlen(string_value)-1;
      snprintf(buf, bufsz, "0x%.*s", newlen, string_value);
   }
   else
      strcpy(buf, string_value);
   // DBGMSG("string_value=|%s|, returning |%s|", string_value, buf);
   return buf;
}

// End of generic functions


/** Thread safe function that returns a string representation of a #Dynamic_Features_Rec
 *  suitable for diagnostic messages. The returned value is valid until the
 *  next call to this function on the current thread.
 *
 *  \param  dfr  pointer to #Dynamic_Features_Rec
 *  \return string representation
 */
char * dfr_repr_t(Dynamic_Features_Rec * dfr) {
   static GPrivate  dfr_repr_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&dfr_repr_key, 100);

   if (dfr)
      g_snprintf(buf, 100, "Dynamic_Features_Rec[%s,%s,%d]",
                           dfr->mfg_id, dfr->model_name, dfr->product_code);
   else
      g_snprintf(buf, 100, "NULL");
   return buf;
}


DDCA_Feature_Metadata *
get_dynamic_feature_metadata(
      Dynamic_Features_Rec * dfr,
      uint8_t                feature_code)
{
   bool debug = false;
   DBGMSF(debug, "dfr=%s, feature_code=0x%02x", dfr_repr_t(dfr), feature_code);

   DDCA_Feature_Metadata * result = NULL;
   if (dfr->features)
      result = g_hash_table_lookup(dfr->features, GINT_TO_POINTER(feature_code));

   DBGMSF(debug, "Returning %p", result);
   return result;
}


void
free_feature_metadata(
      gpointer data)
{
   DDCA_Feature_Metadata * info = (DDCA_Feature_Metadata*) data;
   assert(memcmp(info->marker, DDCA_FEATURE_METADATA_MARKER, 4) == 0);
   // compare vs ddca_free_metadata_contents()

   free(info->feature_desc);
   free(info->feature_name);
   if (info->sl_values) {
      DDCA_Feature_Value_Entry * cur = info->sl_values;
      for ( ;cur->value_code != 0x00 || cur->value_name; cur++ ) {
         free(cur->value_name);
      }
      free(info->sl_values);      // is this right? freeing a hunk allocated by GPtrArray
   }
   info->marker[3] = 'x';
   free(info);
}


Dynamic_Features_Rec *
dfr_new(
      const char * mfg_id,
      const char * model_name,
      uint16_t     product_code,
      const char * filename)
{
   Dynamic_Features_Rec * frec = calloc(1, sizeof(Dynamic_Features_Rec));
   memcpy(frec->marker, DYNAMIC_FEATURES_REC_MARKER, 4);
   frec->mfg_id       = strdup(mfg_id);
   frec->model_name   = strdup(model_name);
   frec->product_code = product_code;
   if (filename)
      frec->filename  = strdup(filename);
   return frec;
}


void
dfr_free(
      Dynamic_Features_Rec * frec)
{
   if (frec) {
      assert(memcmp(frec->marker, DYNAMIC_FEATURES_REC_MARKER, 4) == 0);
      free(frec->mfg_id);
      free(frec->model_name);
      free(frec->filename);
      if (frec->features)
         g_hash_table_destroy(frec->features); // n. destroy function for values set at creation
      free(frec);
   }
}


void
dbgrpt_dfr(
      Dynamic_Features_Rec * dfr,
      int                    depth)
{
   int d1 = depth+1;
   rpt_structure_loc("Dynamic_Features_Rec", dfr, depth);
   rpt_vstring(d1, "marker:         %4s",   dfr->marker);
   rpt_vstring(d1, "mfg_id:         %s",    dfr->mfg_id);
   rpt_vstring(d1, "model_name:     %s",    dfr->model_name);
   rpt_vstring(d1, "product_code:   %u",    dfr->product_code);
   rpt_vstring(d1, "filename:       %s",    dfr->filename);
   rpt_vstring(d1, "MCCS vspec:     %d.%d", dfr->vspec.major, dfr->vspec.minor);
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


//
// Functions private to create_monitor_dynamic_featuers()
//


static
void
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



static
bool
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


static
void
switch_bits(
      DDCA_Feature_Flags * pflags,
      uint16_t             old_bit,
      uint16_t             new_bit)
{
   *pflags &= ~old_bit;
   *pflags |= new_bit;
}


static
void
finalize_feature(
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

   // For now, to revisit
  //  cur_feature_metadata->vspec = frec->vspec;

   g_hash_table_replace(
         frec->features,
         GINT_TO_POINTER(cur_feature_metadata->feature_code),
         cur_feature_metadata);
}


Error_Info *
create_monitor_dynamic_features(
      const char * mfg_id,
      const char * model_name,
      uint16_t     product_code,
      GPtrArray *  lines,
      const char * filename,     // may be NULL
      Dynamic_Features_Rec ** dynamic_features_loc)
{
   bool debug = false;
   DBGMSF(debug, "Starting. filename=%s", filename);

   Error_Info * master_err = NULL;
   GPtrArray * errors = g_ptr_array_new();
   Dynamic_Features_Rec * frec = dfr_new(mfg_id, model_name, product_code, filename);
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

      Tokenized t1 = first_word(line);
      if (t1.word && *t1.word != '*' && *t1.word != '#') {
         Tokenized t2 = first_word(t1.rest);
         if (!t2.word) {
            ADD_ERROR(linectr, "Invalid data \"%s\"", line);
         }
         else {
            if (streq(t1.word, "PRODUCT_CODE")) {
               product_code_seen = true;
               int ival;
               bool ok = str_to_int(t2.word, &ival, 10);
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
            else if (streq(t1.word, "MCCS_VERSION")) {
               // mccs_version_seen = true;   // not required for now
               // default as set by calloc() is 0.0, which is DDCA_VSPEC_UNKNOWN
               // returns DDCA_VSPEC_UKNOWN if invalid
               DDCA_MCCS_Version_Spec  vspec = parse_vspec(t1.rest);
               if (!vcp_version_is_valid(vspec, /*allow DDCA_VSPEC_UNKNOWN */ false))
                  ADD_ERROR(linectr, "Invalid MCCS version: \"%s\"", t1.rest);
               else
                  frec->vspec = vspec;
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
               cur_feature_metadata->feature_flags = DDCA_USER_DEFINED;

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
                  char * can = canonicalize_possible_hex_value(feature_code);

                  bool ok = str_to_int(can, &feature_id, 16);
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
                  char * canonical = canonicalize_possible_hex_value(t2.word);
                  bool ok = str_to_int(canonical, &feature_value, 0);
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
      //char * detail = gaux_asprintf("Error(s) processing monitor definition file: %s", filename);
      char * detail = g_strdup_printf("Error(s) processing monitor definition file: %s", filename);
      master_err = errinfo_new_with_causes2(
                            DDCRC_BAD_DATA,
                            (Error_Info**) errors->pdata,
                            errors->len,
                            __func__,
                            detail);
      free(detail);
      // DBGMSG("After errinfo_new_with_causes2()");
      g_ptr_array_free(errors, false);
      dfr_free(frec);
      *dynamic_features_loc = NULL;
   }
   else {
      g_ptr_array_free(errors, false);
      *dynamic_features_loc = frec;
      if (debug)
         dbgrpt_dfr(frec, 0);
   }

   DBGMSF(debug, "Done. *dynamic_features_loc=%p, returning %s",
                  *dynamic_features_loc, errinfo_summary(master_err));
   assert( (master_err && !*dynamic_features_loc) || (!master_err && *dynamic_features_loc));
   return master_err;
}

