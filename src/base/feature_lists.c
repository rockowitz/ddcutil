/** @file feature_lists.c
 */

// Copyright (C) 2018=2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "feature_lists.h"

#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/coredefs.h"
#include "base/core.h"

typedef struct {
   char * feature_list_string_buf;
   int    feature_list_buf_size;
} Thread_Feature_Lists_Data;

static Thread_Feature_Lists_Data *  get_thread_data() {
   static GPrivate per_thread_data_key = G_PRIVATE_INIT(g_free);

   Thread_Feature_Lists_Data *thread_data = g_private_get(&per_thread_data_key);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, settings=%p\n", __func__, this_thread, settings);

   if (!thread_data) {
      thread_data = g_new0(Thread_Feature_Lists_Data, 1);
      g_private_set(&per_thread_data_key, thread_data);
   }

   // printf("(%s) Returning: %p\n", __func__, thread_data);
   return thread_data;
}




void feature_list_clear(DDCA_Feature_List* vcplist) {
   memset(vcplist->bytes, 0, 32);
}


void feature_list_add(DDCA_Feature_List * vcplist, uint8_t vcp_code) {
   int flagndx   = vcp_code >> 3;
   int shiftct   = vcp_code & 0x07;
   Byte flagbit  = 0x01 << shiftct;
   // printf("(%s) val=0x%02x, flagndx=%d, shiftct=%d, flagbit=0x%02x\n",
   //        __func__, val, flagndx, shiftct, flagbit);
   vcplist->bytes[flagndx] |= flagbit;
}


bool feature_list_contains(DDCA_Feature_List * vcplist, uint8_t vcp_code) {
   int flagndx   = vcp_code >> 3;
   int shiftct   = vcp_code & 0x07;
   Byte flagbit  = 0x01 << shiftct;
   // printf("(%s) val=0x%02x, flagndx=%d, shiftct=%d, flagbit=0x%02x\n",
   //        __func__, val, flagndx, shiftct, flagbit);
   bool result = vcplist->bytes[flagndx] & flagbit;
   return result;
}


DDCA_Feature_List
feature_list_or(
      DDCA_Feature_List* vcplist1,
      DDCA_Feature_List* vcplist2)
{
   DDCA_Feature_List result;
   for (int ndx = 0; ndx < 32; ndx++) {
      result.bytes[ndx] =  vcplist1->bytes[ndx] | vcplist2->bytes[ndx];
   }
   return result;
}


DDCA_Feature_List
feature_list_and(
      DDCA_Feature_List* vcplist1,
      DDCA_Feature_List* vcplist2)
{
   DDCA_Feature_List result;
   for (int ndx = 0; ndx < 32; ndx++) {
      result.bytes[ndx] =  vcplist1->bytes[ndx] & vcplist2->bytes[ndx];
   }
   return result;
}


DDCA_Feature_List
feature_list_and_not(
      DDCA_Feature_List* vcplist1,
      DDCA_Feature_List * vcplist2)
{
   // DBGMSG("Starting. vcplist1=%p, vcplist2=%p", vcplist1, vcplist2);
   DDCA_Feature_List result;
   for (int ndx = 0; ndx < 32; ndx++) {
      result.bytes[ndx] =  vcplist1->bytes[ndx] & ~vcplist2->bytes[ndx];
   }

   // char * s = ddca_feature_list_string(&result, "0x",", ");
   // DBGMSG("Returning: %s", s);
   // free(s);
   return result;
}


int
feature_list_count_old(
      DDCA_Feature_List * feature_list)
{
   int result = 0;
   if (feature_list) {
      for (int ndx = 0; ndx < 256; ndx++) {
         if (feature_list_contains(feature_list, ndx))
            result++;
      }
   }
   return result;
}


int feature_list_count(
      DDCA_Feature_List * feature_list)
{
   // regard the array of 32 bytes as an array of 8 4-byte unsigned integers
   uint64_t * list2 = (uint64_t *) feature_list;
   unsigned int ct = 0;
   for (int ndx = 0; ndx < 4; ndx++) {
      // clever algorithm for counting number of bits per Brian Kernihgan
      uint64_t v = list2[ndx];
      for (; v; ct++) {
        v &= v - 1; // clear the least significant bit set
      }
      // DBGMSG("feature_list_count() returning: %d", ct);
   }
   assert(ct == feature_list_count_old(feature_list));
   return ct;
}


char *
feature_list_string(
      DDCA_Feature_List * feature_list,
      const char * value_prefix,
      const char * sepstr)
{
   // DBGMSG("Starting. feature_list=%p, value_prefix=|%s|, sepstr=|%s|",
   //        feature_list, value_prefix, sepstr);
   // rpt_hex_dump((Byte*)feature_list, 32, 2);

   Thread_Feature_Lists_Data * thread_data = get_thread_data();

   char * buf = NULL;

   if (feature_list) {
      if (!value_prefix)
         value_prefix = "";
      if (!sepstr)
         sepstr = "";
      int vsize = strlen(value_prefix) + 2 + strlen(sepstr);

      int feature_ct = feature_list_count_old(feature_list);
      int reqd_size = (feature_ct*vsize)+1;   // +1 for trailing null

      if (reqd_size > thread_data->feature_list_buf_size) {
         if (thread_data->feature_list_string_buf)
            free(thread_data->feature_list_string_buf);
         thread_data->feature_list_string_buf = malloc(reqd_size);
         thread_data->feature_list_buf_size = reqd_size;
      }
      buf = thread_data->feature_list_string_buf;
      buf[0] = '\0';
      // DBGMSG("feature_ct=%d, vsize=%d, buf size = %d", feature_ct, vsize, vsize*feature_ct);

      for (int ndx = 0; ndx < 256; ndx++) {
         if (feature_list_contains(feature_list, ndx))
            sprintf(buf + strlen(buf), "%s%02x%s", value_prefix, ndx, sepstr);
      }
      if (feature_ct > 0)
         buf[ strlen(buf)-strlen(sepstr)] = '\0';
   }

   // DBGMSG("Returned string length: %d", strlen(buf));
   // DBGMSG("Returning %p - %s", buf, buf);
   return buf;
}

