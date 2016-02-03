/* vcp_feature_values.c
 *
 * Created on: Jan 26, 2016
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software{} you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation{} either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY{} without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program{} if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#include "vcp_feature_values.h"

#include <assert.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/report_util.h"

#include "base/ddc_packets.h"

#include "ddc/vcp_feature_values.h"


void report_single_vcp_value(Single_Vcp_Value * valrec, int depth) {
   int d1 = depth+1;
   rpt_vstring(depth, "Single_Vcp_Value at %p:", valrec);
   rpt_vstring(d1, "opcode=0x%02x, value_type=0x%02x",
                   valrec->opcode, valrec->value_type);
#ifdef OLD
   rpt_vstring(d1, "value=%d", valrec->value);
#endif
   if (valrec->value_type == NON_TABLE_VCP_CALL) {
      rpt_vstring(d1, "mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x",
                      valrec->val.nc.mh, valrec->val.nc.ml, valrec->val.nc.sh, valrec->val.nc.sl);
      rpt_vstring(d1, "max_val=%d (0x%04x), cur_val=%d (0x%04x)",
                      valrec->val.c.max_val,
                      valrec->val.c.max_val,
                      valrec->val.c.cur_val,
                      valrec->val.c.cur_val);
   }
   else {
      assert(valrec->value_type == TABLE_VCP_CALL);
      // TODO: complete
   }
}

// ignoring Buffer * since it only exists temporarily for transition
void free_single_vcp_value(Single_Vcp_Value * vcp_value) {
   if (vcp_value->value_type == TABLE_VCP_CALL) {
      if (vcp_value->val.t.bytes)
         free(vcp_value->val.t.bytes);
   }
   free(vcp_value);
}

// wrap free_single_vcp_value() in signature of GDestroyNotify()
void free_single_vcp_value_func(gpointer data) {
   free_single_vcp_value((Single_Vcp_Value *) data);
}

Single_Vcp_Value *
create_nontable_vcp_value(
      Byte feature_code,
      Byte mh,
      Byte ml,
      Byte sh,
      Byte sl)
{
   Single_Vcp_Value * valrec = calloc(1,sizeof(Single_Vcp_Value));
   valrec->value_type = NON_TABLE_VCP_CALL;
   valrec->opcode = feature_code;
#ifdef OLD
   valrec->value  = sh << 8 | sl;     // for old way
#endif
   valrec->val.nc.mh = mh;
   valrec->val.nc.ml = ml;
   valrec->val.nc.sh = sh;
   valrec->val.nc.sl = sl;
   // not needed thanks to overlay
   // valrec->val.nt.max_val = mh <<8 | ml;
   // valrec->val.nt.cur_val = sh <<8 | sl;
   return valrec;
}

Single_Vcp_Value *
create_cont_vcp_value(
      Byte feature_code,
      ushort max_val,
      ushort cur_val)
{
   Single_Vcp_Value * valrec = calloc(1,sizeof(Single_Vcp_Value));
   valrec->value_type = NON_TABLE_VCP_CALL;
   valrec->opcode = feature_code;
#ifdef OLD
   valrec->value  = cur_val;    // for old way
#endif
   // not needed thanks to overlay
   // valrec->val.nc.mh = max_val >> 8;
   // valrec->val.nc.ml = max_val & 0x0f;
   // valrec->val.nc.sh = cur_val >> 8;
   // valrec->val.nc.sl = cur_val & 0x0f;
   valrec->val.c.max_val = max_val;
   valrec->val.c.cur_val = cur_val;
   return valrec;
}

Single_Vcp_Value *
create_table_vcp_value_by_bytes(
      Byte   feature_code,
      Byte * bytes,
      ushort bytect)
{
   Single_Vcp_Value * valrec = calloc(1,sizeof(Single_Vcp_Value));
   valrec->value_type = TABLE_VCP_CALL;
   valrec->opcode = feature_code;
   valrec->val.t.bytect = bytect;
   valrec->val.t.bytes = malloc(bytect);
   memcpy(valrec->val.t.bytes, bytes, bytect);
#ifdef TRANSITIONAL
   // temp
   valrec->val.t.buffer = buffer_new(bytect, __func__);
   buffer_put(valrec->val.t.buffer, bytes, bytect);
#endif
   return valrec;
}


Single_Vcp_Value *
create_table_vcp_value_by_buffer(Byte feature_code, Buffer * buffer) {
   return create_table_vcp_value_by_bytes(feature_code, buffer->bytes, buffer->len);
}


Single_Vcp_Value *
create_single_vcp_value_by_parsed_vcp_response(
      Byte feature_id,
      Parsed_Vcp_Response * presp)
{
   Single_Vcp_Value * valrec = NULL;

   if (presp->response_type == NON_TABLE_VCP_CALL) {
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
      assert(presp->response_type == TABLE_VCP_CALL);
      valrec = create_table_vcp_value_by_buffer(feature_id, presp->table_response);
   }
   return valrec;
}

// temp for aid in conversion
Parsed_Vcp_Response * single_vcp_value_to_parsed_vcp_response(Single_Vcp_Value * valrec) {
   Parsed_Vcp_Response * presp = calloc(1, sizeof(Parsed_Vcp_Response));
   presp->response_type = valrec->value_type;
   if (valrec->value_type == NON_TABLE_VCP_CALL) {
      presp->non_table_response = calloc(1, sizeof(Parsed_Nontable_Vcp_Response));
      presp->non_table_response->cur_value = valrec->val.c.cur_val;
      presp->non_table_response->max_value = valrec->val.c.max_val;
      presp->non_table_response->mh  = valrec->val.nc.mh;
      presp->non_table_response->ml   = valrec->val.nc.ml;
      presp->non_table_response->sh  = valrec->val.nc.sh;
      presp->non_table_response->sl   = valrec->val.nc.sl;
      presp->non_table_response->supported_opcode = true;
      presp->non_table_response->valid_response = true;
      presp->non_table_response->vcp_code = valrec->opcode;

   }
   else {
      assert(valrec->value_type == TABLE_VCP_CALL);
#ifdef TRANSITIONAL
      //redundant redundancy
      presp->table_response = valrec->val.t.buffer;
#endif
      Buffer * buf2 = buffer_new(valrec->val.t.bytect, __func__);
      buffer_put(buf2, valrec->val.t.bytes, valrec->val.t.bytect);
#ifdef TRANSITIONAL
      assert(buffer_eq(buf2, presp->table_response));
#endif

   }
   return presp;
}

Nontable_Vcp_Value * single_vcp_value_to_nontable_vcp_value(Single_Vcp_Value * valrec) {
   Nontable_Vcp_Value * non_table_response = calloc(1, sizeof(Nontable_Vcp_Value));
   assert (valrec->value_type == NON_TABLE_VCP_CALL);

   non_table_response->cur_value = valrec->val.c.cur_val;
   non_table_response->max_value = valrec->val.c.max_val;
   non_table_response->mh        = valrec->val.nc.mh;
   non_table_response->ml        = valrec->val.nc.ml;
   non_table_response->sh        = valrec->val.nc.sh;
   non_table_response->sl        = valrec->val.nc.sl;
   non_table_response->vcp_code  = valrec->opcode;

   return non_table_response;
}


Vcp_Value_Set vcp_value_set_new(int initial_size){
   GPtrArray* ga = g_ptr_array_sized_new(initial_size);
   g_ptr_array_set_free_func(ga, free_single_vcp_value_func);
   return ga;
}

void free_vcp_value_set(Vcp_Value_Set vset){
   g_ptr_array_free(vset, true);
}

void vcp_value_set_add(Vcp_Value_Set vset,  Single_Vcp_Value * pval){
   g_ptr_array_add(vset, pval);
}

int vcp_value_set_size(Vcp_Value_Set vset){
   return vset->len;
}

Single_Vcp_Value * vcp_value_set_get(Vcp_Value_Set vset, int ndx){
   assert(0 <= ndx && ndx < vset->len);
   return g_ptr_array_index(vset, ndx);
}

void report_vcp_value_set(Vcp_Value_Set vset, int depth) {
   rpt_vstring(depth, "Vcp_Value_Set at %p", vset);
   rpt_vstring(depth+1, "value count: %d", vset->len);
   int ndx = 0;
   for(;ndx<vset->len; ndx++) {
      report_single_vcp_value( g_ptr_array_index(vset, ndx), depth+1);
   }


}
