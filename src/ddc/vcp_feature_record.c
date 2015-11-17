/*
 * vcp_feature_record.c
 *
 *  Created on: Nov 1, 2015
 *      Author: rock
 */



#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>

#include <util/data_structures.h>
#include <util/debug_util.h>

#include <base/msg_control.h>
#include <base/util.h>

#include <ddc/vcp_feature_codes.h>

#include <ddc/vcp_feature_record.h>



// Trace class for this file
// static TraceGroup TRACE_GROUP = TRC_DDC;   // currently unused, commented out to avoid warning




VCP_Feature_Record * new_VCP_Feature_Record(Byte feature_id, char * value_string_start, int value_string_len) {
   bool debug = false;
   if (debug) {
      printf("(%s) Starting. Feature: 0x%02x (%s)\n", __func__, feature_id, get_feature_name(feature_id));
      if (value_string_start)
         printf("(%s)  value string: |%.*s|\n", __func__, value_string_len, value_string_start);
      else
         printf("(%s)  value_string_start = NULL\n", __func__);
   }
   VCP_Feature_Record * vfr =
         (VCP_Feature_Record *) call_calloc(1,sizeof(VCP_Feature_Record), "new_VCP_Feature_Record");
   memcpy(vfr->marker, VCP_FEATURE_MARKER, 4);
   vfr->feature_id = feature_id;
   // relying on calloc to 0 all other fields

   if (value_string_start) {
      vfr->value_string = (char *) malloc( value_string_len+1);
      memcpy(vfr->value_string, value_string_start, value_string_len);
      vfr->value_string[value_string_len] = '\0';

      // single digit values or true integer values in string?
#ifdef OLD
      vfr->values = parse_id_list(value_string_start, value_string_len);
      if (debug)
         report_bva_array(vfr->values, "Feature values (array):");
#endif

      Byte_Value_Array bva_values = bva_create();
      bool ok1 = store_bytehex_list(value_string_start, value_string_len, bva_values, bva_appender);
      if (!ok1) {
         fprintf(stderr, "Error processing VCP feature value list into bva_values: %.*s\n", value_string_len, value_string_start);
      }
      Byte_Bit_Flags bbf_values = bbf_create();
      bool ok2 = store_bytehex_list(value_string_start, value_string_len, bbf_values, bbf_appender);
      if (!ok2) {
          fprintf(stderr, "Error processing VCP feature value list into bbf_values: %.*s\n", value_string_len, value_string_start);
       }
      if (debug) {
          printf("(%s) store_bytehex_list for bva returned %d\n", __func__, ok1);
          printf("(%s) store_bytehex_list for bbf returned %d\n", __func__, ok2);
          //printf("(%s) Comparing Byte_value_Array vs ByteBitFlags\n", __func__);
      }

#ifdef OLD
      bool compok =  bva_bbf_same_values(vfr->values, bbf_values);
      if (compok) {
         printf("(%s) Byte_Value_Array and ByteBitFlags equivalent\n", __func__);
      }
      else {
         printf("(%s) Byte_Value_Array and ByteBitFlags DO NOT MATCH\n", __func__);
         report_bva_array(vfr->values, "Byte_Value_Array contents:");
         printf("(%s) ByteBitFlags as list: %s\n", __func__, bbf_to_string(bbf_values));
      }
#endif

      bool compok =  bva_bbf_same_values(bva_values, bbf_values);
      if (compok) {
         if (debug)
            printf("(%s) Byte_Value_Array and ByteBitFlags equivalent\n", __func__);
      }
      else {
         printf("(%s) Byte_Value_Array and ByteBitFlags DO NOT MATCH\n", __func__);
         bva_report(bva_values, "Byte_Value_Array contents:");
         char buf[768];
         printf("(%s) ByteBitFlags as list: %s\n", __func__, bbf_to_string(bbf_values, buf, 768));
      }
      vfr->values = bva_values;
      if (debug)
         bva_report(vfr->values, "Feature values (array):");
      vfr->bbflags = bbf_values;
      if (debug) {
         char buf[768];
         printf("(%s) ByteBitFlags as list: %s\n", __func__, bbf_to_string(bbf_values,buf,768));
      }
   }

   return vfr;
}


void free_vcp_feature(VCP_Feature_Record * pfeat) {
   // printf("(%s) Starting. pfeat=%p\n", __func__, pfeat);
   assert(pfeat);
   assert(memcmp(pfeat->marker, VCP_FEATURE_MARKER, 4) == 0);

   if (pfeat->value_string)
      free(pfeat->value_string);

   if (pfeat->values)
      bva_free(pfeat->values);

   if (pfeat->bbflags)
      bbf_free(pfeat->bbflags);

   pfeat->marker[3] = 'x';

   call_free(pfeat, "free_vcp_feature");
   // printf("(%s) Done.\n", __func__);
}



void report_feature(VCP_Feature_Record * vfr, Version_Spec vcp_version) {
   // printf("(%s) Starting. vfr=%p\n", __func__, vfr);
   printf("  Feature: %02X (%s)\n", vfr->feature_id, get_feature_name(vfr->feature_id));
   // hex_dump((Byte*) vfr, sizeof(VCP_Feature_Record));
   // if (vfr->values)
   //    report_id_array(vfr->values, "Feature values:");
   char * buf0 = NULL;

   if (vfr->value_string) {
      printf("    Values (unparsed): %s\n", vfr->value_string);
   }

   if (vfr->values) {

      Feature_Value_Entry * feature_values = find_feature_values_for_capabilities(vfr->feature_id, vcp_version);
      // if (feature_values)
      //    printf("(%s) Feature values found for feature 0x%02x\n", __func__, vfr->feature_id);
      // else
      //    printf("(%s) Feature values NOT found for feature 0x%02x\n", __func__, vfr->feature_id);

      int ct = bva_length(vfr->values);
      if (feature_values) {
         printf("    Values (  parsed):\n");
         int ndx = 0;
         for (; ndx < ct; ndx++) {
            Byte hval = bva_get(vfr->values, ndx);
            char *  value_name = find_value_name_new(feature_values, hval);
            if (!value_name)
               value_name = "Unrecognized value!!";
            printf("       %02x: %s\n", hval, value_name);
         }
      }
      else {
         int required_size = 3 * ct;
         buf0 = malloc(required_size);
         char * bufend = buf0+required_size;

         int ndx = 0;
         char * pos = buf0;
         for (; ndx < ct; ndx++) {
            Byte hval = bva_get(vfr->values, ndx);
            snprintf(pos, bufend-pos, "%02X ", hval);
            pos = pos+3;
         }
         *(pos-1) = '\0';
         printf("    Values (  parsed): %s (interpretation unavailable)\n", buf0);
      }
   }

   // assert( streq(buf0, vfr->value_string));
   if (buf0)
      free(buf0);
}



