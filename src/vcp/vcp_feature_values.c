// vcp_feature_values.c

// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdlib.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/ddc_packets.h"

#include "vcp/vcp_feature_values.h"


/** Returns a descriptive name of a #DDCA_Vcp_Value_Type value
 *
 *  \param value_type
 *  \return name, "Non Table" or "Table"
 */
char * vcp_value_type_name(DDCA_Vcp_Value_Type value_type) {
   char * result = NULL;
   switch (value_type) {
   case DDCA_NON_TABLE_VCP_VALUE:
      result = "Non Table";
      break;
   case DDCA_TABLE_VCP_VALUE:
      result = "Table";
      break;
   }
   return result;
}


/** Returns the name of a #DDCA_Vcp_Value_Type value
 *
 *  \param value_type
 *  \return name, "DDCA_NON_TABLE_VCP_VALUE" or "DDCA_TABLE_VCP_VALUE"
 */
char * vcp_value_type_id(DDCA_Vcp_Value_Type  value_type) {
   char * result = NULL;
   switch(value_type) {
   case DDCA_NON_TABLE_VCP_VALUE:
      result = "DDCA_NON_TABLE_VCP_VALUE";
      break;
   case DDCA_TABLE_VCP_VALUE:
      result = "DDCA_TABLE_VCP_VALUE";
      break;
   }
   return result;
}


/** Emits a debug report of a #DDCA_Any_Vcp_Value instance
 *
 *  \param valrec pointer to instance to report
 *  \param depth  logical indentation depth
 */
void dbgrpt_single_vcp_value(
      DDCA_Any_Vcp_Value * valrec,
      int                depth)
{
   int d0 = depth;
   int d1 = depth + 1;
   int d2 = depth + 2;

   rpt_vstring(d0, "Single_Vcp_Value at %p:", valrec);
   if (valrec) {
      rpt_vstring(d1, "Opcode:          0x%02x", valrec->opcode);
      rpt_vstring(d1, "Value type:      %s (0x%02x)", vcp_value_type_id(valrec->value_type), valrec->value_type);

      if (valrec->value_type == DDCA_NON_TABLE_VCP_VALUE)
      {
         rpt_vstring(d1, "max_val:     %d - 0x%04x", VALREC_MAX_VAL(valrec), VALREC_MAX_VAL(valrec));
         rpt_vstring(d1, "cur_val:     %d - 0x%04x", VALREC_CUR_VAL(valrec), VALREC_CUR_VAL(valrec));
         rpt_vstring(d1, "mh:          0x%02x",  valrec->val.c_nc.mh);
         rpt_vstring(d1, "ml:          0x%02x",  valrec->val.c_nc.ml);
         rpt_vstring(d1, "sh:          0x%02x",  valrec->val.c_nc.sh);
         rpt_vstring(d1, "sl:          0x%02x",  valrec->val.c_nc.sl);
      }
      else {
         assert(valrec->value_type == DDCA_TABLE_VCP_VALUE);
         rpt_vstring(d1, "Bytes:");
         rpt_hex_dump(valrec->val.t.bytes, valrec->val.t.bytect, d2);
      }
   }
}


#ifdef OLD   // has been merged into dbgrpt_single_vcp_value()
void report_single_vcp_value(
      Single_Vcp_Value * valrec,
      int depth) {
   int d1 = depth+1;
   rpt_vstring(depth, "Single_Vcp_Value at %p:", valrec);
   rpt_vstring(d1, "opcode=0x%02x, value_type=%s (0x%02x)",
                   valrec->opcode, vcp_value_type_id(valrec->value_type), valrec->value_type);
   if (valrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
      rpt_vstring(d1, "mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x",
                      valrec->val.nc.mh, valrec->val.nc.ml, valrec->val.nc.sh, valrec->val.nc.sl);
      rpt_vstring(d1, "max_val=%d (0x%04x), cur_val=%d (0x%04x)",
                      valrec->val.c.max_val,
                      valrec->val.c.max_val,
                      valrec->val.c.cur_val,
                      valrec->val.c.cur_val);
   }
   else {
      assert(valrec->value_type == DDCA_TABLE_VCP_VALUE);
      rpt_hex_dump(valrec->val.t.bytes, valrec->val.t.bytect, d1);
   }
}
#endif


// must be #define, not const int, since used in buffer declaration
#define SUMMARIZE_SINGLE_VCP_VALUE_BUFFER_SIZE  101
// to expose an int rather than a define in the header file
const int summarize_single_vcp_value_buffer_size = SUMMARIZE_SINGLE_VCP_VALUE_BUFFER_SIZE;


/** Returns a summary of a single vcp value in a buffer provided by the caller
 *
 *  \param valrec vcp value to summarize
 *  \param buffer pointer to buffer
 *  \param bufsz  buffer size
 *  \return buffer
 */
char * summarize_single_vcp_value_r(
      DDCA_Any_Vcp_Value * valrec,
      char * buffer,
      int    bufsz)
{
   bool debug = false;
   DBGMSF(debug, "Starting.  buffer=%p, bufsz=%d", buffer, bufsz);
   if (buffer) {
      assert(bufsz >= SUMMARIZE_SINGLE_VCP_VALUE_BUFFER_SIZE);
      *buffer = '\0';
      if (valrec) {
         if (valrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
            snprintf(buffer, bufsz,
                  "opcode=0x%02x, "
                  "mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x, "
                  "max_val=%d (0x%04x), cur_val=%d (0x%04x)",
                  valrec->opcode,
                  valrec->val.c_nc.mh, valrec->val.c_nc.ml, valrec->val.c_nc.sh, valrec->val.c_nc.sl,
                  VALREC_MAX_VAL(valrec),
                  VALREC_MAX_VAL(valrec),
                  VALREC_CUR_VAL(valrec),
                  VALREC_CUR_VAL(valrec)
                  );
            // should never happen, but in case of string too long
            buffer[bufsz-1] = '\0';
         }
         else {
            assert(valrec->value_type == DDCA_TABLE_VCP_VALUE);
            snprintf(buffer, bufsz,
                     "opcode=0x%02x, value_type=Table, bytect=%d, ...",
                     valrec->opcode,
                     valrec->val.t.bytect
                     );
            // easier just to convert the whole byte array then take what can fit
            char * buf0 =    hexstring2(
                               valrec->val.t.bytes,
                               valrec->val.t.bytect,
                               NULL,                  //   sepstr,
                               true,                  //   uppercase,
                               NULL,                 // allocate buffer,
                               0);                   //    bufsz

            int space_remaining = bufsz - strlen(buffer);
            if ( strlen(buf0) < space_remaining )
               strcat(buffer, buf0);
            else {
               strncat(buffer, buf0, space_remaining-4);
               strcat(buffer, "...");
            }
            free(buf0);
         }
      }
   }
   return buffer;
}


/** Returns a summary string for a single vcp value
 *
 *  This function returns a private thread-specific buffer.
 *  The buffer is valid until the next call of this function in the same thread.
 *
 *  \param  valrec vcp value to summarize
 *  \return pointer to summary string
 */
char * summarize_single_vcp_value(DDCA_Any_Vcp_Value * valrec) {
   static GPrivate  summarize_key = G_PRIVATE_INIT(g_free);
   static GPrivate  summarize_len_key = G_PRIVATE_INIT(g_free);

   char * buf = get_thread_dynamic_buffer(&summarize_key, &summarize_len_key, SUMMARIZE_SINGLE_VCP_VALUE_BUFFER_SIZE);

   return summarize_single_vcp_value_r(valrec, buf, SUMMARIZE_SINGLE_VCP_VALUE_BUFFER_SIZE);
}


/** Frees a single vcp value instance
 *
 *  \param vcp_value pointer to instance (may be NULL)
 */
void free_single_vcp_value(DDCA_Any_Vcp_Value * vcp_value) {
   bool debug = false;

   if (vcp_value) {
      DBGMSF(debug, "Starting. vcp_value=%s", summarize_single_vcp_value(vcp_value));
      if (vcp_value->value_type == DDCA_TABLE_VCP_VALUE) {
         if (vcp_value->val.t.bytes)
            free(vcp_value->val.t.bytes);
      }
      free(vcp_value);
   }
   else
      DBGMSF(debug, "Starting. vcp_value == NULL");
   DBGMSF(debug, "Done");
}


// wrap free_single_vcp_value() in signature of GDestroyNotify()
void free_single_vcp_value_func(gpointer data) {
   free_single_vcp_value((DDCA_Any_Vcp_Value *) data);
}


   DDCA_Any_Vcp_Value *
   create_nontable_vcp_value(
         Byte feature_code,
         Byte mh,
         Byte ml,
         Byte sh,
         Byte sl)
   {
   DDCA_Any_Vcp_Value * valrec = calloc(1,sizeof(DDCA_Any_Vcp_Value));

   valrec->value_type = DDCA_NON_TABLE_VCP_VALUE;
   valrec->opcode = feature_code;
   valrec->val.c_nc.mh = mh;
   valrec->val.c_nc.ml = ml;
   valrec->val.c_nc.sh = sh;
   valrec->val.c_nc.sl = sl;
   // not needed thanks to overlay
   // valrec->val.nt.max_val = mh <<8 | ml;
   // valrec->val.nt.cur_val = sh <<8 | sl;
   return valrec;
}


DDCA_Any_Vcp_Value *
create_cont_vcp_value(
      Byte feature_code,
      ushort max_val,
      ushort cur_val)
{
   DDCA_Any_Vcp_Value * valrec = calloc(1,sizeof(DDCA_Any_Vcp_Value));
   valrec->value_type = DDCA_NON_TABLE_VCP_VALUE;
   valrec->opcode = feature_code;
   valrec->val.c_nc.mh = max_val >> 8;
   valrec->val.c_nc.ml = max_val & 0x0ff;
   valrec->val.c_nc.sh = cur_val >> 8;
   valrec->val.c_nc.sl = cur_val & 0xff;
   return valrec;
}


DDCA_Any_Vcp_Value *
create_table_vcp_value_by_bytes(
      Byte   feature_code,
      Byte * bytes,
      ushort bytect)
{
   DDCA_Any_Vcp_Value * valrec = calloc(1,sizeof(DDCA_Any_Vcp_Value));
   valrec->value_type = DDCA_TABLE_VCP_VALUE;
   valrec->opcode = feature_code;
   valrec->val.t.bytect = bytect;
   valrec->val.t.bytes = malloc(bytect);
   memcpy(valrec->val.t.bytes, bytes, bytect);
   return valrec;
}


DDCA_Any_Vcp_Value *
create_table_vcp_value_by_buffer(Byte feature_code, Buffer * buffer) {
   return create_table_vcp_value_by_bytes(feature_code, buffer->bytes, buffer->len);
}

DDCA_Any_Vcp_Value *
create_single_vcp_value_by_parsed_vcp_response(
      Byte feature_id,
      Parsed_Vcp_Response * presp)
{
   DDCA_Any_Vcp_Value * valrec = NULL;

   if (presp->response_type == DDCA_NON_TABLE_VCP_VALUE) {
      assert(presp->non_table_response->valid_response);
      assert(presp->non_table_response->supported_opcode);
      assert(feature_id == presp->non_table_response->vcp_code);

      valrec = create_nontable_vcp_value(
            feature_id,
            presp->non_table_response->mh,
            presp->non_table_response->ml,
            presp->non_table_response->sh,
            presp->non_table_response->sl
            );

      // assert(valrec->val.c.max_val == presp->non_table_response->max_value);
      // assert(valrec->val.c.cur_val == presp->non_table_response->cur_value);
   }
   else {
      assert(presp->response_type == DDCA_TABLE_VCP_VALUE);
      valrec = create_table_vcp_value_by_buffer(feature_id, presp->table_response);
   }
   return valrec;
}


#ifdef UNUSED
// temp for aid in conversion

Parsed_Vcp_Response * single_vcp_value_to_parsed_vcp_response(
      DDCA_Any_Vcp_Value * valrec) {
   Parsed_Vcp_Response * presp = calloc(1, sizeof(Parsed_Vcp_Response));
   presp->response_type = valrec->value_type;
   if (valrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
      presp->non_table_response = calloc(1, sizeof(Parsed_Nontable_Vcp_Response));
      // presp->non_table_response->cur_value = VALREC_CUR_VAL(valrec);
      // presp->non_table_response->max_value = VALREC_MAX_VAL(valrec);
      presp->non_table_response->mh  = valrec->val.c_nc.mh;
      presp->non_table_response->ml  = valrec->val.c_nc.ml;
      presp->non_table_response->sh  = valrec->val.c_nc.sh;
      presp->non_table_response->sl  = valrec->val.c_nc.sl;
      presp->non_table_response->supported_opcode = true;
      presp->non_table_response->valid_response = true;
      presp->non_table_response->vcp_code = valrec->opcode;
   }
   else {
      assert(valrec->value_type == DDCA_TABLE_VCP_VALUE);
      Buffer * buf2 = buffer_new(valrec->val.t.bytect, __func__);
      buffer_put(buf2, valrec->val.t.bytes, valrec->val.t.bytect);
      buffer_free(buf2,__func__);
   }
   return presp;
}
#endif


Nontable_Vcp_Value * single_vcp_value_to_nontable_vcp_value(DDCA_Any_Vcp_Value * valrec) {
   bool debug = false;
   DBGMSF(debug, "Starting. valrec=%p", valrec);
   Nontable_Vcp_Value * non_table_response = calloc(1, sizeof(Nontable_Vcp_Value));
   assert (valrec->value_type == DDCA_NON_TABLE_VCP_VALUE);

   non_table_response->cur_value = VALREC_CUR_VAL(valrec);
   non_table_response->max_value = VALREC_MAX_VAL(valrec);
   non_table_response->mh        = valrec->val.c_nc.mh;
   non_table_response->ml        = valrec->val.c_nc.ml;
   non_table_response->sh        = valrec->val.c_nc.sh;
   non_table_response->sl        = valrec->val.c_nc.sl;
   non_table_response->vcp_code  = valrec->opcode;

   DBGMSF(debug, "Done. Returning: %p", non_table_response);
   return non_table_response;
}


#ifdef SINGLE_VCP_VALUE
/** Converts a #Single_Vcp_Value to #DDCA_Any_Vcp_Value
 *
 *  \param  valrec  pointer to #Single_Vcp_Value to convert
 *  \return newly allocated converted value
 *
 *  \remark
 *  If table type, the bytes are copied
 */
DDCA_Any_Vcp_Value * single_vcp_value_to_any_vcp_value(Single_Vcp_Value * valrec) {
   DDCA_Any_Vcp_Value * anyval = calloc(1, sizeof(DDCA_Any_Vcp_Value));
   anyval->opcode     = valrec->opcode;
   anyval->value_type = valrec->value_type;
   if (valrec->value_type ==  DDCA_NON_TABLE_VCP_VALUE) {
      anyval->val.c_nc.mh = valrec->val.nc.mh;
      anyval->val.c_nc.ml = valrec->val.nc.ml;
      anyval->val.c_nc.sh = valrec->val.nc.sh;
      anyval->val.c_nc.sl = valrec->val.nc.sl;
   }
   else {          // DDCA_TABLE_VCP_VALUE
      anyval->val.t.bytect = valrec->val.t.bytect;
      if (valrec->val.t.bytect > 0 && valrec->val.t.bytes) {
         anyval->val.t.bytes  = malloc(valrec->val.t.bytect);
         memcpy(anyval->val.t.bytes, valrec->val.t.bytes, valrec->val.t.bytect);
      }
   }

   return anyval;
}
#endif


#ifdef SINGLE_VCP_VALUE
/** Converts a #DDCA_Any_Vcp_Value to #Single_Vcp_Value
 *
 *  \param  valrec  pointer to #DDCA_Any_Vcp_Value to convert
 *  \return newly allocated converted value
 *
 *  \remark
 *  If table type, only the pointer to the bytes is copied
 */
Single_Vcp_Value * any_vcp_value_to_single_vcp_value(DDCA_Any_Vcp_Value * anyval) {
   Single_Vcp_Value * valrec = calloc(1, sizeof(Single_Vcp_Value));
   valrec->opcode     = anyval->opcode;
   valrec->value_type = anyval->value_type;
   if (valrec->value_type ==  DDCA_NON_TABLE_VCP_VALUE) {
      valrec->val.nc.mh = anyval->val.c_nc.mh;
      valrec->val.nc.ml = anyval->val.c_nc.ml;
      valrec->val.nc.sh = anyval->val.c_nc.sh;
      valrec->val.nc.sl = anyval->val.c_nc.sl;
   }
   else {          // DDCA_TABLE_VCP_VALUE
      valrec->val.t.bytect = anyval->val.t.bytect;
      valrec->val.t.bytes  = anyval->val.t.bytes;
   }
   return valrec;
}
#endif


//
// Vcp_Value_Set functions
//

Vcp_Value_Set vcp_value_set_new(int initial_size){
   GPtrArray* ga = g_ptr_array_sized_new(initial_size);
   g_ptr_array_set_free_func(ga, free_single_vcp_value_func);
   return ga;
}


void free_vcp_value_set(Vcp_Value_Set vset){
   // DBGMSG("Executing.");
   g_ptr_array_free(vset, true);
}


void vcp_value_set_add(Vcp_Value_Set vset,  DDCA_Any_Vcp_Value * pval){
   g_ptr_array_add(vset, pval);
}


int vcp_value_set_size(Vcp_Value_Set vset){
   return vset->len;
}


DDCA_Any_Vcp_Value *
vcp_value_set_get(Vcp_Value_Set vset, int ndx){
   assert(0 <= ndx && ndx < vset->len);
   return g_ptr_array_index(vset, ndx);
}


void dbgrpt_vcp_value_set(Vcp_Value_Set vset, int depth) {
   rpt_vstring(depth, "Vcp_Value_Set at %p", vset);
   rpt_vstring(depth+1, "value count: %d", vset->len);
   for(int ndx = 0; ndx<vset->len; ndx++) {
      dbgrpt_single_vcp_value( g_ptr_array_index(vset, ndx), depth+1);
   }
}

