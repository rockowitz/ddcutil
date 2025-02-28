// drm_card_connector_util.c

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 
#include <assert.h>
#include <glib-2.0/glib.h>
#include <fcntl.h>    // for all_displays_drm2()
#include <inttypes.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // for close() used by probe_dri_device_using_drm_api

#ifdef USE_LIBDRM
// #include "drm_common.h"
#include "libdrm_util.h"
#endif

#include "coredefs_base.h"
#include "data_structures.h"
#include "debug_util.h"
#include "file_util.h"
#include "subprocess_util.h"
#include "regex_util.h"
#include "report_util.h"
#include "string_util.h"
#include "sysfs_filter_functions.h"
#include "sysfs_i2c_util.h"
#include "sysfs_util.h"
#include "timestamp.h"


#include "drm_card_connector_util.h"


int    lookup_drm_connector_type(const char * name);
char * drm_connector_type_name(Byte val);
char * drm_connector_type_title(Byte val);


// from sysfs_i2c_util.c

#ifdef UNUSED
static void
do_sysfs_drm_card_number_dir(
            const char * dirname,     // <device>/drm
            const char * simple_fn,   // card0, card1, etc.
            void *       data,
            int          depth)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. dirname=%s, simple_fn=%s\n", __func__, dirname, simple_fn);
   Bit_Set_256 * card_numbers = (Bit_Set_256*) data;
   const char * s = simple_fn+4;
   int card_number = atoi(s);
   *card_numbers = bs256_insert(*card_numbers, card_number);
   if (debug)
      printf("(%s) Done.    Added %d\n", __func__, card_number);
}


Bit_Set_32
get_sysfs_drm_card_numbers() {
   bool debug = false;
   char * dname =
 #ifdef TARGET_BSD
              "/compat/linux/sys/class/drm";
 #else
              "/sys/class/drm";
 #endif
   if (debug)
      printf("(%s) Examining %s\n", __func__, dname);
   Bit_Set_32 result = EMPTY_BIT_SET_32;
   dir_foreach(
                 dname,
                 predicate_cardN,            // filter function
                 do_sysfs_drm_card_number_dir,
                 &result,                 // accumulator
                 0);
   if (debug)
      printf("(%s) Done.    Returning DRM card numbers: %s\n", __func__, bs32_to_string_decimal(result, "", ", "));
   return result;
 }
#endif



typedef struct {
   bool has_card_connector_dir;
} Check_Card_Struct;

#ifdef REF
typedef void (*Dir_Foreach_Func)(
const char *  dirname,
const char *  fn,
void *        accumulator,
int           depth);
#endif


void dir_foreach_set_true(const char * dirname, const char * fn, void * accumulator, int depth) {
   bool debug = false;
   DBGF(debug, "dirname=%s, fn=%s, accumlator=%p, depth=%d", dirname, fn, accumulator, depth);
   Check_Card_Struct * accum = accumulator;
   DBGF(debug, "Setting accumulator->has_card_connector_dir = true");
   accum->has_card_connector_dir = true;
}


void do_one_card(const char * dirname, const char * fn, void* accumulator, int depth) {
   bool debug = false;
   char buf[PATH_MAX];
   g_snprintf(buf, sizeof(buf), "%s/%s", dirname, fn);
   DBGF(debug, "Examining dir buf=%s", buf);
   dir_foreach(buf, predicate_cardN_connector, dir_foreach_set_true, accumulator, depth);
   Check_Card_Struct * accum = accumulator;
   DBGF(debug, "Finishing with accumlator->has_card_connector_dir = %s", sbool(accum->has_card_connector_dir));
}


// n. could enhance to collect the card connector subdir names
bool card_connector_subdirs_exist(const char * adapter_dir) {
   bool debug = false;
   DBGF(debug, "Starting. adapter_dir = %s", adapter_dir);
   int lastpos= strlen(adapter_dir) - 1;
   char * delim = (adapter_dir[lastpos] == '/') ? "" : "/";
   char drm_dir[PATH_MAX];
   g_snprintf(drm_dir, PATH_MAX, "%s%sdrm", adapter_dir,delim);
   DBGF(debug, "drm_dir=%s", drm_dir);
   int d = (debug) ? 1 : -1;
   Check_Card_Struct *  accumulator = calloc(1, sizeof(Check_Card_Struct));
   dir_foreach(drm_dir, predicate_cardN, do_one_card, accumulator, d);
   bool has_card_subdir = accumulator->has_card_connector_dir;
   free(accumulator);
   DBGF(debug, "Done.    Returning %s", sbool(has_card_subdir));
   return has_card_subdir;
}


/** Check that all devices in a list of video adapter devices have drivers that implement
 *  drm by looking for card_connector_dirs in each adapter's drm directory.
 *
 *  @param  adapter_devices array of /sys directory names for video adapter devices
 *  @return true if all adapter video drivers implement drm
 */
bool check_video_adapters_list_implements_drm(GPtrArray * adapter_devices) {
   bool debug = false;
   assert(adapter_devices);
   uint64_t t0, t1;
   if (debug)
      t0 = cur_realtime_nanosec();
   DBGF(debug, "adapter_devices->len=%d at %p", adapter_devices->len, adapter_devices);
   bool result = true;
   for (int ndx = 0; ndx < adapter_devices->len; ndx++) {
      // char * subdir_name = NULL;
      char * adapter_dir = g_ptr_array_index(adapter_devices, ndx);
      bool has_card_subdir = card_connector_subdirs_exist(adapter_dir);
      // DBGF(debug, "Examined.  has_card_subdir = %s", sbool(has_card_subdir));
      if (!has_card_subdir) {
         result = false;
         break;
      }
   }
   if (debug) {
     t1 = cur_realtime_nanosec();
     DBG("elapsed: %jd microsec",  NANOS2MICROS(t1-t0));
   }
   DBGF(debug, "Done.     Returning %s", sbool(result));
   return result;
}


/** Checks that all video adapters on the system have drivers that implement drm
 *  by checking that card connector directories drm/cardN/cardN-xxx exist.
 *
 *  @return true if all video adapters have drivers implementing drm, false if not
 *
 *  The degenerate case of no video adapters returns false.
 *
 */
bool check_all_video_adapters_implement_drm() {
   bool debug = false;
   DBGF(debug, "Starting");

   uint64_t t0 = cur_realtime_nanosec();
   // DBGF(debug, "t0=%"PRIu64, t0);
   GPtrArray * devices = get_video_adapter_devices();
   uint64_t t1 = cur_realtime_nanosec();
   // DBGF(debug, "t1=%"PRIu64, t1);
   // DBGF(debug, "t1-t0=%"PRIu64, t1-t0);
   DBGF(debug, "get_video_adapter_devices() took %jd microseconds", NANOS2MICROS(t1-t0));

   bool all_drm = check_video_adapters_list_implements_drm(devices);
   uint64_t t2 = cur_realtime_nanosec();
   // DBGF(debug, "t2=%"PRIu64, t2);
   // DBGF(debug, "t2-t1=%"PRIu64, t2-t1);
   DBGF(debug, "check_video_adapters_list_implements_drm() took %jd microseconds", NANOS2MICROS(t2-t1));
   g_ptr_array_free(devices, true);

   // DBGF(debug, "t2-t0=%"PRIu64, t2-t0);
   DBGF(debug, "Done.  Returning %s.  elapsed=%jd microsec", sbool(all_drm), NANOS2MICROS(t2-t0));
   return all_drm;
}


#ifdef TO_FIX
/** Checks if a display has a DRM driver by looking for
 *  card connector subdirs of drm in the adapter directory.
 *
 *  @param busno   I2C bus number
 *  @return true/false
 */
 bool is_drm_display_by_busno(int busno) {
   bool debug = false;
   DBGF(debug, "Starting. busno = %d", busno);
   bool result = false;
   char i2cdir[40];
   g_snprintf(i2cdir, 40, "/sys/bus/i2c/devices/i2c-%d",busno);
   char * real_i2cdir = NULL;
   GET_ATTR_REALPATH(&real_i2cdir, i2cdir);
   DBGF(debug, "real_i2cdir = %s", real_i2cdir);
   assert(real_i2cdir);
   int d = (debug)  ? 1 : -1;
   char * adapter_dir = find_adapter(real_i2cdir, d);  // in i2c/i2c_sysfs.c, need to fix
   assert(adapter_dir);
   result = card_connector_subdirs_exist(adapter_dir);
   free(real_i2cdir);
   free(adapter_dir);
   DBGF(debug, "Done.    Returning: %s", sbool(result));
   return result;
}
#endif


 char * get_drm_connector_type_name(int connector_type) {
#ifdef USE_LIBDRM
    return drm_connector_type_name(connector_type);
#else
    return NULL;
#endif
 }

 int get_drm_connector_type(const char * name) {
#ifdef USE_LIBDRM
    return lookup_drm_connector_type(name);
#else
    return -1;
#endif
 }


char * dci_repr(Drm_Connector_Identifier dci) {
   char * buf = g_strdup_printf("[dci:cardno=%d,connector_id=%d,connector_type=%d=%s,connector_type_id=%d]",
         dci.cardno, dci.connector_id,
         dci.connector_type, get_drm_connector_type_name(dci.connector_type),
         dci.connector_type_id);
   return buf;
}


/** Thread safe function that returns a brief string representation of a #Drm_Connector_Identifier.
 *  The returned value is valid until the next call to this function on the current thread.
 *
 *
 *  \param  dpath  pointer to ##DDCA_IO_Path
 *  \return string representation of #DDCA_IO_Path
 */
char * dci_repr_t(Drm_Connector_Identifier dci) {
   static GPrivate  dci_repr_key = G_PRIVATE_INIT(g_free);

   char * repr = dci_repr(dci);
   char * buf = get_thread_fixed_buffer(&dci_repr_key, 100);
   g_snprintf(buf, 100, "%s", repr);
   free(repr);

   return buf;
}


bool dci_eq(Drm_Connector_Identifier dci1, Drm_Connector_Identifier dci2) {
   bool result = false;
   if (dci1.connector_id > 0 && dci1.connector_id == dci2.connector_id) {
      result = true;
   }
   else
      result = dci1.cardno            == dci2.cardno &&
               dci1.connector_type    == dci2.connector_type &&
               dci1.connector_type_id == dci2.connector_type_id;
   return result;
}


/** Compares 2 Drm_Connector_Identifier values. */

int dci_cmp(Drm_Connector_Identifier dci1, Drm_Connector_Identifier dci2) {
   int result = 0;
   if (dci1.cardno < dci2.cardno)
      result = -1;
   else if (dci1.cardno > dci2.cardno)
      result = 1;
   else {
      if (dci1.connector_type < dci2.connector_type)
         result = -1;
      else if (dci1.connector_type > dci2.connector_type)
         result = 1;
      else {
         if (dci1.connector_type_id < dci2.connector_type_id)
            result = -1;
         else if (dci1.connector_type_id > dci2.connector_type_id)
            result = 1;
         else
            result = 0;
      }
   }
   return result;
}


/** Compare drm connector names so that e.g. card1-DP-10 comes
 *  after card1-DP-2, not before.
 */
int sys_drm_connector_name_cmp0(const char * s1, const char * s2) {
   int result = 0;

   // do something "reasonable" for pathological cases
   if (!s1 && s2)
      result = -1;
   else if (!s1 && !s2)
      result = 0;
   else if (s1 && !s2)
      result = 1;

   else {      // normal case
      Drm_Connector_Identifier dci1 = parse_sys_drm_connector_name(s1);
      Drm_Connector_Identifier dci2 = parse_sys_drm_connector_name(s2);
      result = dci_cmp(dci1, dci2);
   }

   return result;
}


/** QSort style comparison function for sorting drm connector names.
 */
int sys_drm_connector_name_cmp(gconstpointer connector_name1, gconstpointer connector_name2) {
   bool debug = false;

   int result = 0;
   char * s1 = (connector_name1) ? *(char**)connector_name1 : NULL;
   char * s2 = (connector_name2) ? *(char**)connector_name2 : NULL;
   DBGF(debug, "s1=%p->%s, s2=%p->%s", s1, s1, s2, s2);

   result = sys_drm_connector_name_cmp0(s1, s2);

   DBGF(debug, "Returning: %d", result);
   return result;
}


Drm_Connector_Identifier parse_sys_drm_connector_name(const char * drm_connector) {
   bool debug = false;
   DBGF(debug, "Starting. drm_connector = |%s|", drm_connector);
   Drm_Connector_Identifier result = {-1,-1,-1,-1};
   static const char * drm_connector_pattern = "^card([0-9])[-](.*)[-]([0-9]+)";

   regmatch_t  matches[4];

   bool ok =  compile_and_eval_regex_with_matches(
         drm_connector_pattern,
         drm_connector,
         4,   //       max_matches,
         matches);

   if (ok) {
      // for (int kk = 0; kk < 4; kk++) {
      //    rpt_vstring(1, "match %d, substring start=%d, end=%d", kk, matches[kk].rm_so, matches[kk].rm_eo);
      // }
      char * cardno_s = substr(drm_connector, matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
      char * connector_type = substr(drm_connector, matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
      char * connector_type_id_s = substr(drm_connector, matches[3].rm_so, matches[3].rm_eo - matches[3].rm_so);
      // DBGF(debug, "cardno_s=|%s|", cardno_s);
      // DBGF(debug, "connector_type=|%s|", connector_type);
      // DBGF(debug, "connector_type_id_s=|%s|", connector_type_id_s);

      ok = str_to_int(cardno_s, &result.cardno, 10);
      assert(ok);
      ok = str_to_int(connector_type_id_s, &result.connector_type_id, 10);
      assert(ok);
      // DBGF(debug, "result.cardno: %d", result.cardno);
      // DBGF(debug, "connector_type_id: %d", result.connector_type_id);

#ifdef USE_LIBDRM
      result.connector_type = lookup_drm_connector_type(connector_type);
#else
      result.connector_type = -1;
#endif

      free(connector_type);
      free(cardno_s);
      free(connector_type_id_s);
   }

   if (debug) {
      char * s = dci_repr(result);
      DBG("Done.     Returning: %s", s);
      free(s);
   }
   return result;
}

