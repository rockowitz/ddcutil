/* feature_lists.c
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

#include "feature_lists.h"

#include <glib-2.0/glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/coredefs.h"

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
feature_list_count(
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


char *
feature_list_string(
      DDCA_Feature_List * feature_list,
      char * value_prefix,
      char * sepstr)
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

      int feature_ct = feature_list_count(feature_list);
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

