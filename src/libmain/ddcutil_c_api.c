/* ddcutil_c_api.c
 *
 * <copyright>
 * Copyright (C) 2015-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \file
 * **ddcutil** C API implementation
 */

#include <config.h>

/** \cond */
#include <assert.h>
#include <errno.h>
#include <string.h>
/** \endcond */


#include "util/data_structures.h"
#include "util/error_info.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/udev_util.h"

#include "base/base_init.h"
#include "base/build_info.h"
#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/dynamic_features.h"
#include "base/execution_stats.h"
#include "base/feature_lists.h"
#include "base/monitor_model_key.h"
#include "base/parms.h"

#include "vcp/ddc_command_codes.h"
#include "vcp/parse_capabilities.h"
#include "vcp/parsed_capabilities_feature.h"
#include "vcp/vcp_feature_codes.h"
#include "vcp/vcp_feature_values.h"

#include "dynvcp/ddc_dynamic_features.h"
#include "dynvcp/ddc_feature_set.h"
#include "dynvcp/ddc_parsed_capabilities.h"

#include "adl/adl_shim.h"

#include "ddc/ddc_async.h"
#include "ddc/ddc_displays.h"
#include "ddc/ddc_dumpload.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_services.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"

#include "private/ddcutil_types_private.h"
#include "private/ddcutil_c_api_private.h"
#include "public/ddcutil_c_api.h"


#define WITH_DR(ddca_dref, action) \
   do { \
      if (!library_initialized) \
         return DDCRC_UNINITIALIZED; \
      DDCA_Status psc = 0; \
      Display_Ref * dref = (Display_Ref *) ddca_dref; \
      if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  { \
         psc = DDCRC_ARG; \
      } \
      else { \
         (action); \
      } \
      return psc; \
   } while(0);


#define WITH_DH(_ddca_dh_, _action_) \
   do { \
      if (!library_initialized) \
         return DDCRC_UNINITIALIZED; \
      DDCA_Status psc = 0; \
      Display_Handle * dh = (Display_Handle *) _ddca_dh_; \
      if ( !dh || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  { \
         psc = DDCRC_ARG; \
      } \
      else { \
         (_action_); \
      } \
      return psc; \
   } while(0);



static inline bool valid_display_handle(Display_Handle * dh) {
   return (dh && memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) == 0);
}

static inline bool valid_display_ref(Display_Ref * dref) {
   return (dref && memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
}


//
// Library Build Information
//


DDCA_Ddcutil_Version_Spec ddca_ddcutil_version(void) {
   static DDCA_Ddcutil_Version_Spec vspec = {255,255,255};
   static bool vspec_init = false;

   if (!vspec_init) {
      int ct = sscanf(BUILD_VERSION, "%hhu.%hhu.%hhu", &vspec.major, &vspec.minor, &vspec.micro);
      assert(ct == 3);
      vspec_init = true;
   }
   // DBGMSG("Returning: %d.%d.%d", vspec.major, vspec.minor, vspec.micro);
   return vspec;

}


/*  Returns the ddcutil version as a string in the form "major.minor.micro".
 */
const char *
ddca_ddcutil_version_string(void) {
   return BUILD_VERSION;
}


/* Indicates whether the ddcutil library was built with ADL support. .
 */
bool
ddca_built_with_adl(void) {
#ifdef HAVE_ADL
   return true;
#else
   return false;
#endif

}

/* Indicates whether the ddcutil library was built with support for USB connected monitors. .
 */
bool
ddca_built_with_usb(void) {
#ifdef USE_USB
   return true;
#else
   return false;
#endif
}

/* Indicates whether ADL successfully initialized.
 * (e.g. fglrx driver not found)
 *
 * @return true/false
 */
bool
ddca_adl_is_available(void) {
   return adlshim_is_available();
}


// Alternative to individual ddca_built_with...() functions.
// conciseness vs documentatbility
// how to document bits?   should doxygen doc be in header instead?

DDCA_Build_Option_Flags
ddca_build_options(void) {
   uint8_t result = 0x00;
#ifdef HAVE_ADL
   result |= DDCA_BUILT_WITH_ADL;
#endif
#ifdef USE_USB
         result |= DDCA_BUILT_WITH_USB;
#endif
#ifdef FAILSIM_ENABLED
         result |= DDCA_BUILT_WITH_FAILSIM;
#endif
   return result;
}


//
// Initialization
//

#ifdef NOT_NEEDED_HEADER_FILE_SEGMENT
/**
 * Initializes the ddcutil library module.
 *
 * Must be called before most other functions.
 *
 * It is not an error if this function is called more than once.
 */
// void __attribute__ ((constructor)) _ddca_init(void);
#endif


static bool library_initialized = false;

/** Initializes the ddcutil library module.
 *
 *  Normally called automatically during when the shared library is loaded.
 *
 *  It is not an error if this function is called more than once.
 */
void __attribute__ ((constructor))
_ddca_init(void) {
   // Note: Until init_msg_control() is called within init_base_services(),
   // FOUT is null, so DBGMSG() causes a segfault
   bool debug = false;
   if (!library_initialized) {
      init_base_services();
      init_ddc_services();

      // no longer needed, values are initialized on first use per-thread
      // set_output_level(DDCA_OL_NORMAL);
      // enable_report_ddc_errors(false);

      library_initialized = true;
      DBGMSF(debug, "library initialization executed");
   }
   else {
      DBGMSF(debug, "library was already initialized");
   }
}


#ifdef OBSOLETE

void
ddca_register_jmp_buf(jmp_buf* jb) {
   register_jmp_buf(jb);
}


DDCA_Global_Failure_Information *
ddca_get_global_failure_information()
{
   // return NULL if !global_failure_information.info_set_fg, or always return pointer
   // i.e. is it better if caller checks for NULL or checks info_set_fg?
   // return &global_failure_information;
   return (global_failure_information.info_set_fg)
                  ? &global_failure_information
                  : NULL;
}

#endif


//
// Error Detail
//

DDCA_Error_Detail * ddca_get_error_detail() {
   return dup_error_detail(get_thread_error_detail());
}

void ddca_free_error_detail(DDCA_Error_Detail * ddca_erec) {
   free_error_detail(ddca_erec);
}

// DDCA_Error_Detail * ddca_dup_error_detail(DDCA_Error_Detail * original) {
//     return dup_error_detail(original);
// }


//
// Status Code Management
//

char *
ddca_rc_name(DDCA_Status status_code) {
   char * result = NULL;
   Status_Code_Info * code_info = find_status_code_info(status_code);
   if (code_info)
      result = code_info->name;
   return result;
}


char *
ddca_rc_desc(DDCA_Status status_code) {
   char * result = "unknown status code";
   Status_Code_Info * code_info = find_status_code_info(status_code);
   if (code_info)
      result = code_info->description;
   return result;
}

// quick and dirty for now
// TODO: make thread safe, wrap in mutex
bool ddca_enable_error_info(bool enable) {
   bool old_value = report_freed_exceptions;
   report_freed_exceptions = enable;            // global in core.c
   return old_value;
}

//
// Output redirection
//

// Redirects output that normally would go to STDOUT
void
ddca_set_fout(FILE * fout) {
   // DBGMSG("Starting. fout=%p", fout);
   set_fout(fout);
}


void
ddca_set_fout_to_default(void) {
   set_fout_to_default();
}


// Redirects output that normally would go to STDERR
void ddca_set_ferr(FILE * ferr) {
   set_ferr(ferr);
}


void ddca_set_ferr_to_default(void) {
   set_ferr_to_default();
}


//
// Output capture - convenience functions
//

typedef struct {
   FILE * in_memory_file;
   char * in_memory_bufstart; ;
   size_t in_memory_bufsize;
   DDCA_Capture_Option_Flags flags;
} In_Memory_File_Desc;


static In_Memory_File_Desc *  get_thread_capture_buf_desc() {
   static GPrivate  in_memory_key = G_PRIVATE_INIT(g_free);

   In_Memory_File_Desc* fdesc = g_private_get(&in_memory_key);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, fdesc=%p\n", __func__, this_thread, fdesc);

   if (!fdesc) {
      fdesc = g_new0(In_Memory_File_Desc, 1);
      g_private_set(&in_memory_key, fdesc);
   }

   // printf("(%s) Returning: %p\n", __func__, fdesc);
   return fdesc;
}


void ddca_start_capture(DDCA_Capture_Option_Flags flags) {
   In_Memory_File_Desc * fdesc = get_thread_capture_buf_desc();

   if (!fdesc->in_memory_file) {
      fdesc->in_memory_file = open_memstream(&fdesc->in_memory_bufstart, &fdesc->in_memory_bufsize);
      ddca_set_fout(fdesc->in_memory_file);   // n. ddca_set_fout() is thread specific
      fdesc->flags = flags;
      if (flags & DDCA_CAPTURE_STDERR)
         ddca_set_ferr(fdesc->in_memory_file);
   }
   // printf("(%s) Done.\n", __func__);
}


char * ddca_end_capture(void) {
   In_Memory_File_Desc * fdesc = get_thread_capture_buf_desc();
   // In_Memory_File_Desc * fdesc = &in_memory_file_desc;

   char * result = "\0";
   // printf("(%s) Starting.\n", __func__);
   assert(fdesc->in_memory_file);
   if (fflush(fdesc->in_memory_file) < 0) {
      ddca_set_ferr_to_default();
      SEVEREMSG("flush() failed. errno=%d", errno);
      return strdup(result);
   }
   // n. open_memstream() maintains a null byte at end of buffer, not included in in_memory_bufsize
   result = strdup(fdesc->in_memory_bufstart);
   if (fclose(fdesc->in_memory_file) < 0) {
      ddca_set_ferr_to_default();
      SEVEREMSG("fclose() failed. errno=%d", errno);
      return result;
   }
   // free(fdesc->in_memory_file); // double free, fclose() frees in memory file
   fdesc->in_memory_file = NULL;
   ddca_set_fout_to_default();
   if (fdesc->flags & DDCA_CAPTURE_STDERR)
      ddca_set_ferr_to_default();

   // printf("(%s) Done. result=%p\n", __func__, result);
   return result;
}


#ifdef UNUSED
/** Returns the current size of the in-memory capture buffer.
 *
 *  @return number of characters in current buffer, plus 1 for
 *          terminating null
 *  @retval -1 no capture buffer on current thread
 *
 *  @remark defined and tested but does not appear useful
 */
int ddca_captured_size() {
   // printf("(%s) Starting.\n", __func__);
   In_Memory_File_Desc * fdesc = get_thread_capture_buf_desc();

   int result = -1;
   // n. open_memstream() maintains a null byte at end of buffer, not included in in_memory_bufsize
   if (fdesc->in_memory_file) {
      fflush(fdesc->in_memory_file);
      result = fdesc->in_memory_bufsize + 1;   // +1 for trailing \0
   }
   // printf("(%s) Done. result=%d\n", __func__, result);
   return result;
}
#endif


//
// Message Control
//

DDCA_Output_Level
ddca_get_output_level(void) {
   return get_output_level();
}

DDCA_Output_Level
ddca_set_output_level(DDCA_Output_Level newval) {
     return set_output_level(newval);
}

char *
ddca_output_level_name(DDCA_Output_Level val) {
   return output_level_name(val);
}


bool
ddca_enable_report_ddc_errors(bool onoff) {
   return enable_report_ddc_errors(onoff);;
}

bool
ddca_is_report_ddc_errors_enabled(void) {
   return is_report_ddc_errors_enabled();
}


//
// Global Settings
//

int
ddca_max_max_tries(void) {
   return MAX_MAX_TRIES;
}

int
ddca_get_max_tries(DDCA_Retry_Type retry_type) {
   int result = 0;
   switch(retry_type) {
      case (DDCA_WRITE_ONLY_TRIES):
         result = ddc_get_max_write_only_exchange_tries();
      break;
   case (DDCA_WRITE_READ_TRIES):
      result = ddc_get_max_write_read_exchange_tries();
      break;
   case (DDCA_MULTI_PART_TRIES):
      result = ddc_get_max_multi_part_read_tries();
      break;
   }
   return result;
}


DDCA_Status
ddca_set_max_tries(
      DDCA_Retry_Type retry_type,
      int             max_tries)
{
   DDCA_Status rc = 0;
   if (max_tries < 1 || max_tries > MAX_MAX_TRIES)
      rc = -EINVAL;
   else {
      switch(retry_type) {
      case (DDCA_WRITE_ONLY_TRIES):
         ddc_set_max_write_only_exchange_tries(max_tries);
         break;
      case (DDCA_WRITE_READ_TRIES):
         ddc_set_max_write_read_exchange_tries(max_tries);
         break;
      case (DDCA_MULTI_PART_TRIES):
         ddc_set_max_multi_part_read_tries(max_tries);
         ddc_set_max_multi_part_write_tries(max_tries);      // TODO: Separate constant
         break;
      }
   }
   return rc;
}


bool ddca_enable_verify(bool onoff) {
   return ddc_set_verify_setvcp(onoff);
}

bool ddca_is_verify_enabled() {
   return ddc_get_verify_setvcp();
}



#ifdef FUTURE

/** Gets the I2C timeout in milliseconds for the specified timeout class.
 * @param timeout_type timeout type
 * @return timeout in milliseconds
 */
int
ddca_get_timeout_millis(
      DDCA_Timeout_Type timeout_type) {
   return 0;    // *** UNIMPLEMENTED ***
}

/** Sets the I2C timeout in milliseconds for the specified timeout class
 * @param timeout_type  timeout class
 * @param millisec      timeout to set, in milliseconds
 */
void
ddca_set_timeout_millis(
      DDCA_Timeout_Type timeout_type,
      int               millisec)
{
   // *** UNIMPLEMENTED
}
#endif


//
// Tracing
//

void ddca_add_traced_function(const char * funcname) {
   add_traced_function(funcname);
}


void ddca_add_traced_file(const char * filename) {
   add_traced_file(filename);
}


void ddca_set_trace_groups(DDCA_Trace_Group trace_flags) {
   set_trace_levels(trace_flags);
}


//
// Statistics
//

// TODO: Add functions to access ddcutil's runtime error statistics

void ddca_reset_stats(void) {
   ddc_reset_stats_main();
}

// TODO: Functions that return stats in data structures
void ddca_show_stats(DDCA_Stats_Type stats_types, int depth) {
   ddc_report_stats_main( stats_types,    // stats to show
                          depth);         // logical indentation depth
}


//
// Display Identifiers
//

DDCA_Status
ddca_create_dispno_display_identifier(
      int                      dispno,
      DDCA_Display_Identifier* did_loc)
{
   Display_Identifier* did = create_dispno_display_identifier(dispno);
   *did_loc = did;
   // DBGMSG("Done.  *did_loc = %p", *did_loc);
   return 0;
}


DDCA_Status
ddca_create_busno_display_identifier(
      int busno,
      DDCA_Display_Identifier* did_loc)
{
   Display_Identifier* did = create_busno_display_identifier(busno);
   *did_loc = did;
   return 0;
}


DDCA_Status
ddca_create_adlno_display_identifier(
      int                      iAdapterIndex,
      int                      iDisplayIndex,
      DDCA_Display_Identifier* did_loc)
{
   Display_Identifier* did = create_adlno_display_identifier(iAdapterIndex, iDisplayIndex);
   *did_loc = did;
   return 0;
}


DDCA_Status
ddca_create_mfg_model_sn_display_identifier(
      const char*              mfg_id,
      const char*              model_name,
      const char*              serial_ascii,
      DDCA_Display_Identifier* did_loc)
{
   *did_loc = NULL;
   DDCA_Status rc = 0;

   // break up the invalid argument tests for clarity

   // At least 1 argument must be specified
   if (  ( !mfg_id       || strlen(mfg_id)       == 0)  &&
         ( !model_name   || strlen(model_name)   == 0)  &&
         ( !serial_ascii || strlen(serial_ascii) == 0)
      )
      rc = -EINVAL;

   // check if any arguments are too long
   else if (
        (model_name   && strlen(model_name)   >= EDID_MODEL_NAME_FIELD_SIZE)  ||
        (mfg_id       && strlen(mfg_id)       >= EDID_MFG_ID_FIELD_SIZE)      ||
        (serial_ascii && strlen(serial_ascii) >= EDID_SERIAL_ASCII_FIELD_SIZE)
      )
      rc = -EINVAL;

   else {
      *did_loc = create_mfg_model_sn_display_identifier(
                     mfg_id, model_name, serial_ascii);
   }
   return rc;
}


DDCA_Status
ddca_create_edid_display_identifier(
      const Byte *              edid,
      DDCA_Display_Identifier * did_loc)    // 128 byte EDID
{
   *did_loc = NULL;
   DDCA_Status rc = 0;
   if (edid == NULL) {
      rc = -EINVAL;
      *did_loc = NULL;
   }
   else {
      *did_loc = create_edid_display_identifier(edid);
   }
   return rc;
}


DDCA_Status
ddca_create_usb_display_identifier(
      int                      bus,
      int                      device,
      DDCA_Display_Identifier* did_loc)
{
   Display_Identifier* did = create_usb_display_identifier(bus, device);
   *did_loc = did;
   return 0;
}


DDCA_Status
ddca_create_usb_hiddev_display_identifier(
      int                      hiddev_devno,
      DDCA_Display_Identifier* did_loc)
{
   Display_Identifier* did = create_usb_hiddev_display_identifier(hiddev_devno);
   *did_loc = did;
   return 0;
}


DDCA_Status
ddca_free_display_identifier(
      DDCA_Display_Identifier did)
{
   DDCA_Status rc = 0;
   Display_Identifier * pdid = (Display_Identifier *) did;
   if (pdid) {
      if ( memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) != 0 )
         rc = DDCRC_ARG;
      else
         free_display_identifier(pdid);
   }
   return rc;
}


char *
ddca_did_repr(DDCA_Display_Identifier ddca_did) {
   // DBGMSG("Starting.  ddca_did=%p", ddca_did);
   char * result = NULL;
   Display_Identifier * pdid = (Display_Identifier *) ddca_did;
   if (pdid != NULL && memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) == 0 )  {
      result = did_repr(pdid);
   }
   // DBGMSG("Done.  Returning: %p", result);
   return result;
}


//
// Display References
//

DDCA_Status
ddca_create_display_ref(
      DDCA_Display_Identifier did,
      DDCA_Display_Ref*       ddca_dref)
{
   bool debug = false;
   DBGMSF(debug, "Starting.  did=%p, ddca_dref=%p", did, ddca_dref);
   if (ddca_dref)
      DBGMSF(debug,"    *ddca_dref=%p", *ddca_dref);
   DDCA_Status rc = 0;

   if (!library_initialized) {
      rc =  DDCRC_UNINITIALIZED;
      goto bye;
   }

   ddc_ensure_displays_detected();

   Display_Identifier * pdid = (Display_Identifier *) did;
   if (!pdid || memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) != 0  || !ddca_dref)  {
     rc = -EINVAL;
   }
   else {
      Display_Ref* dref = get_display_ref_for_display_identifier(pdid, CALLOPT_ERR_MSG);
      if (debug)
         DBGMSG("get_display_ref_for_display_identifier() returned %p", dref);
      if (dref)
         *ddca_dref = dref;
      else
         rc = DDCRC_INVALID_DISPLAY;
   }

bye:
   DBGMSF(debug, "Done.  Returning: %d", rc);
   if (rc == 0)
      DBGMSF(debug,"    *ddca_dref=%p", *ddca_dref);
   return rc;
}


DDCA_Status ddca_free_display_ref(DDCA_Display_Ref ddca_dref) {
   WITH_DR(ddca_dref,
         {
            if (dref->flags & DREF_TRANSIENT)
               free_display_ref(dref);
         }
   );
}


// static char dref_work_buf[100];

char *
ddca_dref_repr(DDCA_Display_Ref ddca_dref){
   bool debug = false;
   DBGMSF(debug, "Starting.  ddca_dref = %p", ddca_dref);
   char * result = NULL;
   Display_Ref * dref = (Display_Ref *) ddca_dref;
   if (dref != NULL && memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 )  {
#ifdef TOO_MUCH_WORK
      char * dref_type_name = io_mode_name(dref->ddc_io_mode);
      switch (dref->ddc_io_mode) {
      case(DISP_ID_BUSNO):
         snprintf(dref_work_buf, 100,
                  "Display Ref Type: %s, bus=/dev/i2c-%d", dref_type_name, dref->io_path.i2c_busno);
         break;
      case(DISP_ID_ADL):
         snprintf(dref_work_buf, 100,
                  "Display Ref Type: %s, adlno=%d.%d", dref_type_name, dref->io_path.adlno.iAdapterIndex, dref->io_path.adlno.iDisplayIndex);
         break;
      }
      *repr = did_work_buf;
#endif
      // result = dref_short_name(dref);
      result = dref_repr_t(dref);
   }
   DBGMSF(debug, "Done. Returning: %s", result);
   return result;
}

void
ddca_dbgrpt_display_ref(
      DDCA_Display_Ref ddca_dref,
      int              depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting.  ddca_dref = %p, depth=%d", ddca_dref, depth);
   Display_Ref * dref = (Display_Ref *) ddca_dref;
   rpt_vstring(depth, "DDCA_Display_Ref at %p:", dref);
   dbgrpt_display_ref(dref, depth+1);
}

DDCA_Monitor_Model_Key
ddca_mmk_from_dref(
      DDCA_Display_Ref   ddca_dref)
{
   DDCA_Monitor_Model_Key result = DDCA_UNDEFINED_MONITOR_MODEL_KEY;
   Display_Ref * dref = (Display_Ref *) ddca_dref;
   if (valid_display_ref(dref) && dref->mmid)
      result = *dref->mmid;
   return result;
}



DDCA_Status
ddca_open_display(
      DDCA_Display_Ref      ddca_dref,
      DDCA_Display_Handle * p_dh)
{
   return ddca_open_display2(ddca_dref, false, p_dh);
}

DDCA_Status
ddca_open_display2(
      DDCA_Display_Ref      ddca_dref,
      bool                  wait,
      DDCA_Display_Handle * p_dh)
{
   if (!library_initialized)
      return DDCRC_UNINITIALIZED;

   ddc_ensure_displays_detected();

   DDCA_Status rc = 0;
   *p_dh = NULL;        // in case of error
   Display_Ref * dref = (Display_Ref *) ddca_dref;
   if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  {
      rc = DDCRC_ARG;
   }
   else {
     Display_Handle* dh = NULL;
     Call_Options callopts = CALLOPT_NONE;
     if (wait)
        callopts |= CALLOPT_WAIT;
     rc = ddc_open_display(dref,  callopts, &dh);
     if (rc == 0)
        *p_dh = dh;
   }
   return rc;
}


DDCA_Status
ddca_close_display(DDCA_Display_Handle ddca_dh) {
   if (!library_initialized)
      return DDCRC_UNINITIALIZED;
   DDCA_Status rc = 0;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   // if (dh == NULL || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  {
   if (!valid_display_handle(dh)) {
      rc = DDCRC_ARG;
   }
   else {
      // TODO: ddc_close_display() needs an action if failure parm,
      // should return status code
      ddc_close_display(dh);
      rc = 0;    // is this what to do?
   }
   return rc;
}


char *
ddca_dh_repr(DDCA_Display_Handle ddca_dh) {
   char * repr = NULL;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (valid_display_handle(dh))
      repr = dh_repr(dh);
   return repr;
}


DDCA_Display_Ref
ddca_display_ref_from_handle(
      DDCA_Display_Handle   ddca_dh)
{
   DDCA_Display_Ref result = NULL;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (valid_display_handle(dh))
      result = dh->dref;
   return result;
}


DDCA_Monitor_Model_Key
ddca_mmk_from_dh(
      DDCA_Display_Handle   ddca_dh)
{
   DDCA_Monitor_Model_Key result = DDCA_UNDEFINED_MONITOR_MODEL_KEY;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (valid_display_handle(dh) && dh->dref->mmid)
      result = *dh->dref->mmid;
   return result;
}



DDCA_Status
ddca_get_mccs_version_by_dh(
      DDCA_Display_Handle     ddca_dh,
      DDCA_MCCS_Version_Spec* p_spec)
{
   if (!library_initialized)
      return DDCRC_UNINITIALIZED;
   DDCA_Status rc = 0;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (dh == NULL || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  {
      rc = DDCRC_ARG;
      p_spec->major = 0;
      p_spec->minor = 0;
   }
   else {
      // need to call function, may not yet be set
      DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
      p_spec->major = vspec.major;
      p_spec->minor = vspec.minor;
      rc = 0;
   }
   return rc;
}

// not published
DDCA_Status
ddca_get_mccs_version_with_default(
      DDCA_Display_Handle     ddca_dh,
      DDCA_MCCS_Version_Spec  default_spec,
      DDCA_MCCS_Version_Spec* p_spec)
{
   DDCA_Status rc = ddca_get_mccs_version_by_dh(ddca_dh, p_spec);
   if (rc == 0 && vcp_version_eq(*p_spec, DDCA_VSPEC_UNKNOWN))
      *p_spec = default_spec;
   return rc;
}

DDCA_Status
ddca_get_mccs_version_id(
      DDCA_Display_Handle    ddca_dh,
      DDCA_MCCS_Version_Id*  p_id)
{
   DDCA_MCCS_Version_Spec vspec;
   DDCA_Status rc = ddca_get_mccs_version_by_dh(ddca_dh, &vspec);
   if (rc == 0) {
      DDCA_MCCS_Version_Id  version_id = mccs_version_spec_to_id(vspec);
      *p_id = version_id;
   }
   else {
      *p_id = DDCA_MCCS_VNONE;
   }
   return rc;
}


char *
ddca_mccs_version_id_name(DDCA_MCCS_Version_Id version_id) {
   return vcp_version_id_name(version_id);
}

#ifdef DEFINED_BUT_NOT_RELEASED
/**  Returns the descriptive name of a #DDCA_MCCS_Version_Id,
 *  e.g. "2.0".
 *
 *  @param[in]  version_id  version id value
 *  @return descriptive name (do not free)
 *
 *  @remark added to replace ddca_mccs_version_id_desc() during 0.9
 *  development, but then use of DDCA_MCCS_Version_Id deprecated
 */

char *
ddca_mccs_version_id_string(DDCA_MCCS_Version_Id version_id) {
   return format_vcp_version_id(version_id);
}
#endif

char *
ddca_mccs_version_id_desc(DDCA_MCCS_Version_Id version_id) {
   return format_vcp_version_id(version_id);
}


//
// Monitor Model Identifier
//

const DDCA_Monitor_Model_Key DDCA_UNDEFINED_MONITOR_MODEL_KEY = {{0}};

DDCA_Monitor_Model_Key
ddca_mmk(
      const char * mfg_id,
      const char * model_name,
      uint16_t     product_code)
{
   DDCA_Monitor_Model_Key result = DDCA_UNDEFINED_MONITOR_MODEL_KEY;
   if (mfg_id     && strlen(mfg_id)     < DDCA_EDID_MFG_ID_FIELD_SIZE &&
       model_name && strlen(model_name) < DDCA_EDID_MODEL_NAME_FIELD_SIZE)
   {
      result = monitor_model_key_value(mfg_id, model_name, product_code);
   }
   return result;
}

bool
ddca_mmk_eq(
      DDCA_Monitor_Model_Key mmk1,
      DDCA_Monitor_Model_Key mmk2)
{
   return monitor_model_key_eq(mmk1, mmk2);
}


bool
ddca_mmk_is_defined(
      DDCA_Monitor_Model_Key mmk)
{
   return mmk.defined;
}


DDCA_Display_Info_List *
ddca_get_display_info_list(void)
{
   DDCA_Display_Info_List * result = NULL;
   ddca_get_display_info_list2(false, &result);
   return result;
}


DDCA_Status
ddca_get_display_info_list2(
      bool                      include_invalid_displays,
      DDCA_Display_Info_List**  dlist_loc)
{
   bool debug = false;
   DBGTRC(debug, DDCA_TRC_API||DDCA_TRC_DDC, "Starting");

   ddc_ensure_displays_detected();
   GPtrArray * all_displays = ddc_get_all_displays();

   int true_ct = all_displays->len;
   if (!include_invalid_displays) {
      true_ct = 0;         // number of valid displays
      for (int ndx = 0; ndx < all_displays->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
         if (dref->dispno != -1)    // ignore invalid displays
            true_ct++;
      }
   }

   int reqd_size =   offsetof(DDCA_Display_Info_List,info) + true_ct * sizeof(DDCA_Display_Info);
   DDCA_Display_Info_List * result_list = calloc(1,reqd_size);
   result_list->ct = true_ct;
   DBGMSF(debug, "sizeof(DDCA_Display_Info) = %d, sizeof(Display_Info_List) = %d, reqd_size=%d, true_ct=%d, offsetof(DDCA_Display_Info_List,info) = %d",
           sizeof(DDCA_Display_Info), sizeof(DDCA_Display_Info_List), reqd_size, true_ct, offsetof(DDCA_Display_Info_List,info));

   int true_ctr = 0;
   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(all_displays, ndx);

      if (dref->dispno != -1 || include_invalid_displays) {
         DDCA_Display_Info * curinfo = &result_list->info[true_ctr++];
         memcpy(curinfo->marker, DDCA_DISPLAY_INFO_MARKER, 4);
         curinfo->dispno        = dref->dispno;

         curinfo->path = dref->io_path;
         if (dref->io_path.io_mode == DDCA_IO_USB) {
            curinfo->usb_bus    = dref->usb_bus;
            curinfo->usb_device = dref->usb_device;
         }

         DDCA_MCCS_Version_Spec vspec = dref->vcp_version;
         DDCA_MCCS_Version_Id version_id = DDCA_MCCS_VNONE;
         if (dref->dispno != -1) {
            // hack:
            // vcp version is unqueried to improve performance of the command line version
            // mccs_version_spec_to_id has assert error if unqueried
            if (vcp_version_eq(vspec, DDCA_VSPEC_UNQUERIED)) {
               vspec = get_vcp_version_by_display_ref(dref);
            }
            version_id = mccs_version_spec_to_id(vspec);
         }

         memcpy(curinfo->edid_bytes,    dref->pedid->bytes, 128);
         g_strlcpy(curinfo->mfg_id,     dref->pedid->mfg_id,       EDID_MFG_ID_FIELD_SIZE);
         g_strlcpy(curinfo->model_name, dref->pedid->model_name,   EDID_MODEL_NAME_FIELD_SIZE);
         g_strlcpy(curinfo->sn,         dref->pedid->serial_ascii, DDCA_EDID_SN_ASCII_FIELD_SIZE);
         curinfo->product_code  = dref->pedid->product_code;
#ifdef OLD
         curinfo->edid_bytes    = dref->pedid->bytes;
         // or should these be memcpy'd instead of just pointers, can edid go away?
         curinfo->mfg_id         = dref->pedid->mfg_id;
         curinfo->model_name     = dref->pedid->model_name;
         curinfo->sn             = dref->pedid->serial_ascii;
         curinfo->product_code   = dref->pedid->product_code;
#endif
         curinfo->vcp_version    = vspec;
         curinfo->vcp_version_id = version_id;
         curinfo->dref           = dref;

#ifdef MMID
         curinfo->mmid = monitor_model_key_value(
                                        dref->pedid->mfg_id,
                                        dref->pedid->model_name,
                                        dref->pedid->product_code);
// #ifdef OLD
         assert(streq(curinfo->mfg_id,     curinfo->mmid.mfg_id));
         assert(streq(curinfo->model_name, curinfo->mmid.model_name));
         assert(curinfo->product_code == curinfo->mmid.product_code);
// #endif
#endif
      }
   }

   if (debug || IS_TRACING_GROUP( DDCA_TRC_API||DDCA_TRC_DDC )) {
      DBGMSG("Done. Returning %p", result_list);
      ddca_report_display_info_list(result_list, 2);
   }

   *dlist_loc = result_list;
   return 0;
}


static void
ddca_free_display_info(DDCA_Display_Info * info_rec) {
   // All pointers in DDCA_Display_Info are to permanently allocated
   // data structures.  Nothing to free.
   if (info_rec && memcmp(info_rec->marker, DDCA_DISPLAY_INFO_MARKER, 4) == 0) {
      info_rec->marker[3] = 'x';
      // free(info_rec);
   }
}


void
ddca_free_display_info_list(DDCA_Display_Info_List * dlist) {
   if (dlist) {
      for (int ndx = 0; ndx < dlist->ct; ndx++) {
         ddca_free_display_info(&dlist->info[ndx]);
      }
      free(dlist);
   }
}


void
ddca_report_display_info(
      DDCA_Display_Info * dinfo,
      int                 depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dinfo=%p, depth=%d", dinfo, depth);

   assert(dinfo);
   assert(memcmp(dinfo->marker, DDCA_DISPLAY_INFO_MARKER, 4) == 0);
   int d0 = depth;
   int d1 = depth+1;
   int d2 = depth+2;
   if (dinfo->dispno > 0)
      rpt_vstring(d0, "Display number:  %d", dinfo->dispno);
   else
      rpt_label(  d0, "Invalid display - Does not support DDC");
   // rpt_vstring(d1, "IO mode:             %s", io_mode_name(dinfo->path.io_mode));
   switch(dinfo->path.io_mode) {
   case (DDCA_IO_I2C):
         rpt_vstring(d1, "I2C bus:             /dev/i2c-%d", dinfo->path.path.i2c_busno);
         break;
   case (DDCA_IO_ADL):
         rpt_vstring(d1, "ADL adapter.display: %d.%d",
                         dinfo->path.path.adlno.iAdapterIndex, dinfo->path.path.adlno.iDisplayIndex);
         break;
   case (DDCA_IO_USB):
         rpt_vstring(d1, "USB bus.device:      %d.%d",
                         dinfo->usb_bus, dinfo->usb_device);
         rpt_vstring(d1, "USB hiddev device:   /dev/usb/hiddev%d", dinfo->path.path.hiddev_devno);
         break;
   }

   rpt_vstring(d1, "Mfg Id:              %s", dinfo->mfg_id);
   rpt_vstring(d1, "Model:               %s", dinfo->model_name);
   rpt_vstring(d1, "Product code:        %u", dinfo->product_code);
   rpt_vstring(d1, "Serial number:       %s", dinfo->sn);
   // rpt_label(  d1, "Monitor Model Id:");
   // rpt_vstring(d2, "Mfg Id:           %s", dinfo->mmid.mfg_id);
   // rpt_vstring(d2, "Model name:       %s", dinfo->mmid.model_name);
   // rpt_vstring(d2, "Product code:     %d", dinfo->mmid.product_code);
   rpt_vstring(d1, "EDID:");
   rpt_hex_dump(dinfo->edid_bytes, 128, d2);
   // rpt_vstring(d1, "dref:                %p", dinfo->dref);
   rpt_vstring(d1, "VCP Version:         %s", format_vspec(dinfo->vcp_version));
// rpt_vstring(d1, "VCP Version Id:      %s", format_vcp_version_id(dinfo->vcp_version_id) );
   DBGMSF(debug, "Done");
}


void
ddca_report_display_info_list(
      DDCA_Display_Info_List * dlist,
      int                      depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting.  dlist=%p, depth=%d", dlist, depth);

   int d1 = depth+1;
   rpt_vstring(depth, "Found %d displays", dlist->ct);
   for (int ndx=0; ndx<dlist->ct; ndx++) {
      ddca_report_display_info(&dlist->info[ndx], d1);
   }
}


// deprecated
DDCA_Status
ddca_get_edid_by_dref(
      DDCA_Display_Ref ddca_dref,
      uint8_t**        p_bytes)
{
   DDCA_Status rc = 0;
   *p_bytes = NULL;

   if (!library_initialized) {
      rc = DDCRC_UNINITIALIZED;
      goto bye;
   }

   Display_Ref * dref = (Display_Ref *) ddca_dref;
   // if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  {
   if ( !valid_display_ref(dref) )  {
      rc = DDCRC_ARG;
      goto bye;
   }

   // Parsed_Edid*  edid = ddc_get_parsed_edid_by_display_ref(dref);
   Parsed_Edid * edid = dref->pedid;
   assert(edid);
   *p_bytes = edid->bytes;

bye:
   return rc;
}


#ifdef UNIMPLEMENTED
// Use ddca_get_edid_by_dref() instead
// n. edid_buffer must be >= 128 bytes

DDCA_Status
ddca_get_edid(DDCA_Display_Handle * dh, uint8_t* edid_buffer);
#endif



DDCA_Status
ddca_report_display_by_dref(DDCA_Display_Ref ddca_dref, int depth) {
   DDCA_Status rc = 0;

    if (!library_initialized) {
       rc = DDCRC_UNINITIALIZED;
       goto bye;
    }

    Display_Ref * dref = (Display_Ref *) ddca_dref;
    if ( !valid_display_ref(dref) )  {
       rc = DDCRC_ARG;
       goto bye;
    }

    ddc_report_display_by_dref(dref, depth);

bye:
   return rc;
}


//
// Feature Lists
//
// TODO: Move most functions into directory src/base
//

const DDCA_Feature_List DDCA_EMPTY_FEATURE_LIST = {{0}};


void ddca_feature_list_clear(DDCA_Feature_List* vcplist) {
   feature_list_clear(vcplist);
}


void ddca_feature_list_add(DDCA_Feature_List * vcplist, uint8_t vcp_code) {
   feature_list_add(vcplist, vcp_code);
}


bool ddca_feature_list_contains(DDCA_Feature_List * vcplist, uint8_t vcp_code) {
   return feature_list_contains(vcplist, vcp_code);
}


const char *
ddca_feature_list_id_name(
      DDCA_Feature_Subset_Id  feature_subset_id)
{
   char * result = NULL;
   switch (feature_subset_id) {
   case DDCA_SUBSET_KNOWN:
      result = "VCP_SUBSET_KNOWN";
      break;
   case DDCA_SUBSET_COLOR:
      result = "VCP_SUBSET_COLOR";
      break;
   case DDCA_SUBSET_PROFILE:
      result = "VCP_SUBSET_PROFILE";
      break;
   case DDCA_SUBSET_MFG:
      result = "VCP_SUBSET_MFG";
      break;
   case DDCA_SUBSET_UNSET:
      result = "VCP_SUBSET_NONE";
      break;
   }
   return result;
}



DDCA_Status
ddca_get_feature_list(
      DDCA_Feature_Subset_Id  feature_subset_id,
      DDCA_MCCS_Version_Spec  vspec,
      bool                    include_table_features,
      DDCA_Feature_List*      p_feature_list)   // location to fill in
{
   bool debug = false;
   DBGMSF(debug, "Starting. feature_subset_id=%d, vcp_version=%d.%d, include_table_features=%s, p_feature_list=%p",
          feature_subset_id, vspec.major, vspec.minor, bool_repr(include_table_features), p_feature_list);

   DDCA_Status ddcrc = 0;
   // Whether a feature is a table feature can vary by version, so can't
   // specify VCP_SPEC_ANY to request feature ids in any version
   if (!vcp_version_is_valid(vspec, /* allow unknown */ false)) {
      ddcrc = -EINVAL;
      ddca_feature_list_clear(p_feature_list);
      goto bye;
   }
   VCP_Feature_Subset subset = VCP_SUBSET_NONE;  // pointless initialization to avoid compile warning
   switch (feature_subset_id) {
   case DDCA_SUBSET_KNOWN:
      subset = VCP_SUBSET_KNOWN;
      break;
   case DDCA_SUBSET_COLOR:
      subset = VCP_SUBSET_COLOR;
      break;
   case DDCA_SUBSET_PROFILE:
      subset = VCP_SUBSET_PROFILE;
      break;
   case DDCA_SUBSET_MFG:
      subset = VCP_SUBSET_MFG;
      break;
   case DDCA_SUBSET_UNSET:
      subset = VCP_SUBSET_NONE;
      break;
   }
   Feature_Set_Flags flags = 0x00;
   if (!include_table_features)
      flags |= FSF_NOTABLE;
   VCP_Feature_Set fset = create_feature_set(subset, vspec, flags);
   // VCP_Feature_Set fset = create_feature_set(subset, vspec, !include_table_features);

   // TODO: function variant that takes result location as a parm, avoid memcpy
   DDCA_Feature_List result = feature_list_from_feature_set(fset);
   memcpy(p_feature_list, &result, 32);
   free_vcp_feature_set(fset);

#ifdef NO
   DBGMSG("feature_subset_id=%d, vspec=%s, returning:",
          feature_subset_id, format_vspec(vspec));
   rpt_hex_dump(result.bytes, 32, 1);
   for (int ndx = 0; ndx <= 255; ndx++) {
      uint8_t code = (uint8_t) ndx;
      if (ddca_feature_list_test(&result, code))
         printf("%02x ", code);
   }
   printf("\n");
#endif

bye:
   DBGMSF(debug, "Done. Returning: %s", psc_desc(ddcrc));
   if (debug)
      rpt_hex_dump((Byte*) p_feature_list, 32, 1);
   return ddcrc;

}


DDCA_Status
ddca_get_feature_list_by_dref(
      DDCA_Feature_Subset_Id  feature_set_id,
      DDCA_Display_Ref        ddca_dref,
      bool                    include_table_features,
      DDCA_Feature_List*      p_feature_list)
{
   WITH_DR(
         ddca_dref,
         {
#ifdef OLD
               DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_ref(ddca_dref);

               psc = ddca_get_feature_list(
                     feature_set_id,
                     vspec,               // dref->vcp_version,
                     include_table_features,
                     p_feature_list);
#endif
               bool debug = false;
               DBGMSF(debug, "Starting. feature_subset_id=%d, dref=%s, include_table_features=%s, p_feature_list=%p",
                      feature_set_id, dref_repr_t(dref), bool_repr(include_table_features), p_feature_list);

               DDCA_MCCS_Version_Spec vspec = dref->vcp_version;
               // Whether a feature is a table feature can vary by version, so can't
               // specify VCP_SPEC_ANY to request feature ids in any version
               if (!vcp_version_is_valid(vspec, /* allow unknown */ false)) {
                  psc = -EINVAL;
                  ddca_feature_list_clear(p_feature_list);
                  goto bye;
               }
               VCP_Feature_Subset subset = VCP_SUBSET_NONE;  // pointless initialization to avoid compile warning
               switch (feature_set_id) {
               case DDCA_SUBSET_KNOWN:
                  subset = VCP_SUBSET_KNOWN;
                  break;
               case DDCA_SUBSET_COLOR:
                  subset = VCP_SUBSET_COLOR;
                  break;
               case DDCA_SUBSET_PROFILE:
                  subset = VCP_SUBSET_PROFILE;
                  break;
               case DDCA_SUBSET_MFG:
                  subset = VCP_SUBSET_MFG;
                  break;
               case DDCA_SUBSET_UNSET:
                  subset = VCP_SUBSET_NONE;
                  break;
               }
               Feature_Set_Flags flags = 0x00;
               if (!include_table_features)
                  flags |= FSF_NOTABLE;
               VCP_Feature_Set fset = ddc_create_feature_set(subset, dref, flags);
               // VCP_Feature_Set fset = create_feature_set(subset, vspec, !include_table_features);

               // TODO: function variant that takes result location as a parm, avoid memcpy
               DDCA_Feature_List result = feature_list_from_feature_set(fset);
               memcpy(p_feature_list, &result, 32);
               free_vcp_feature_set(fset);

            bye:
               DBGMSF(debug, "Done. Returning: %s", psc_desc(psc));
               if (debug)
                  rpt_hex_dump((Byte*) p_feature_list, 32, 1);

         }
      );
}


DDCA_Feature_List
ddca_feature_list_or(
      DDCA_Feature_List* vcplist1,
      DDCA_Feature_List* vcplist2)
{
   return feature_list_or(vcplist1, vcplist2);
}


DDCA_Feature_List
ddca_feature_list_and(
      DDCA_Feature_List* vcplist1,
      DDCA_Feature_List* vcplist2)
{
   return feature_list_and(vcplist1, vcplist2);
}


DDCA_Feature_List
ddca_feature_list_and_not(
      DDCA_Feature_List* vcplist1,
      DDCA_Feature_List* vcplist2)
{
   return feature_list_and_not(vcplist1, vcplist2);
}


#ifdef UNPUBLISHED
// no real savings in client code
// sample use:
// int codect;
//  uint8_t feature_codes[256];
// ddca_feature_list_to_codes(&vcplist2, &codect, feature_codes);
// printf("\nFeatures in feature set COLOR:  ");
// for (int ndx = 0; ndx < codect; ndx++) {
//       printf(" 0x%02x", feature_codes[ndx]);
// }
// puts("");

/** Converts a feature list into an array of feature codes.
 *
 *  @param[in]  vcplist   pointer to feature list
 *  @param[out] p_codect  address where to return count of feature codes
 *  @param[out] vcp_codes address of 256 byte buffer to receive codes
 */

void ddca_feature_list_to_codes(
      DDCA_Feature_List* vcplist,
      int*               codect,
      uint8_t            vcp_codes[256])
{
   int ctr = 0;
   for (int ndx = 0; ndx < 256; ndx++) {
      if (ddca_feature_list_contains(vcplist, ndx)) {
         vcp_codes[ctr++] = ndx;
      }
   }
   *codect = ctr;
}
#endif


int
ddca_feature_list_count(
      DDCA_Feature_List * feature_list)
{
   return feature_list_count(feature_list);
}


char *
ddca_feature_list_string(
      DDCA_Feature_List * feature_list,
      char * value_prefix,
      char * sepstr)
{
   return feature_list_string(feature_list, value_prefix, sepstr);
}


#ifdef OLD
// or return a struct?
DDCA_Status ddca_get_feature_flags_by_vcp_version(
      DDCA_Vcp_Feature_Code         feature_code,
      DDCA_MCCS_Version_Id          mccs_version_id,
      DDCA_Version_Feature_Flags *  flags)
{
   DDCA_Status rc = 0;
   DDCA_MCCS_Version_Spec vspec = mccs_version_id_to_spec(mccs_version_id);

   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
   if (!pentry) {
      *flags = 0;
      rc = DDCRC_ARG;
   }
   else {
      DDCA_Version_Feature_Flags vflags = get_version_specific_feature_flags(pentry, vspec);
      *flags = 0;
      // TODO handle subvariants REWORK
      if (vflags & VCP2_RO)
         *flags |= DDCA_RO;
      if (vflags & VCP2_WO)
         *flags |= DDCA_WO;
      if (vflags & VCP2_RW)
         *flags |= DDCA_RW;
      if (vflags & VCP2_CONT)
         *flags |= DDCA_CONTINUOUS;
#ifdef OLD
      if (pentry->flags & VCP_TYPE_V2NC_V3T) {
         if (vspec.major < 3)
            *flags |= DDCA_SIMPLE_NC;
         else
            *flags |= DDCA_TABLE;
      }
#endif
      else if (vflags & DDCA_TABLE)
         *flags |= DDCA_TABLE;
      else if (vflags & VCP2_NC) {
         if (vspec.major < 3)
            *flags |= DDCA_SIMPLE_NC;
         else {
            // TODO: In V3, some features use combination of high and low bytes
            // for now, mark all as simple
            *flags |= DDCA_SIMPLE_NC;
            // alt: DDCT_COMPLEX_NC
         }
      }
   }
   return rc;
}
#endif


// deprecated
DDCA_Status
ddca_get_feature_info_by_vcp_version(
      DDCA_Vcp_Feature_Code       feature_code,
   // DDCT_MCCS_Version_Spec      vspec,
      DDCA_MCCS_Version_Id        mccs_version_id,
      DDCA_Version_Feature_Info** p_info)
{
   bool debug = true;
   DBGMSF(debug, "Starting. feature_code=0x%02x, mccs_version_id=%d", feature_code, mccs_version_id);

   DDCA_Status psc = 0;
   *p_info = NULL;
   // DDCA_MCCS_Version_Spec vspec = mccs_version_id_to_spec(mccs_version_id);

   // or should this be a version sensitive call?
   DDCA_Version_Feature_Info * info =  get_version_feature_info_by_version_id(
         feature_code,
         mccs_version_id,
         false,                       // with_default
         true);                       // false => version specific, true=> version sensitive
   if (!info)
      psc = DDCRC_ARG;
   else
      *p_info = info;

   DBGMSF(debug, "Returning:%d, *p_info=%p", psc, *p_info);
   return psc;

}


#ifdef NEVER_RELEASED
DDCA_Status
ddca_get_simplified_feature_info(
      DDCA_Vcp_Feature_Code         feature_code,
      DDCA_MCCS_Version_Spec        vspec,
 //   DDCA_MCCS_Version_Id          mccs_version_id,
      DDCA_Feature_Metadata *   info)
{
   DDCA_Status psc = DDCRC_ARG;
   DDCA_Version_Feature_Info * full_info =  get_version_feature_info_by_vspec(
         feature_code,
         vspec,
         false,                       // with_default
         true);                       // false => version specific, true=> version sensitive
   if (full_info) {
      info->feature_code  = feature_code;
      info->vspec         = vspec;
      info->version_id    = full_info->version_id;    // keep?
      info->feature_flags = full_info->feature_flags;

      free_version_feature_info(full_info);
      psc = 0;
   }
   return psc;
}
#endif



// UNPUBLISHED
/**
 * Gets characteristics of a VCP feature.
 *
 * VCP characteristics (C vs NC, RW vs RO, etc) can vary by MCCS version.
 *
 * @param[in]  feature_code     VCP feature code
 * @param[in]  vspec            MCCS version (may be DDCA_VSPEC_UNKNOWN)
 * @param[out] p_feature_flags  address of flag field to fill in
 * @return     status code
 * @retval     DDCRC_ARG        invalid MCCS version
 * @retval     DDCRC_UNKNOWN_FEATURE  unrecognized feature
 *
 * @since 0.9.0
 */
DDCA_Status
ddca_get_feature_flags_by_vspec(
      DDCA_Vcp_Feature_Code         feature_code,
      DDCA_MCCS_Version_Spec        vspec,
      DDCA_Feature_Flags *          feature_flags)
{
   DDCA_Status psc = DDCRC_ARG;
   if (vcp_version_is_valid(vspec, /*unknown_ok*/ true)) {
      DDCA_Version_Feature_Info * full_info =  get_version_feature_info_by_vspec(
            feature_code,
            vspec,
            false,                       // with_default
            true);                       // false => version specific, true=> version sensitive
      if (full_info) {
         *feature_flags = full_info->feature_flags;
         free_version_feature_info(full_info);
         psc = 0;
      }
      else {
         psc = DDCRC_UNKNOWN_FEATURE;
      }
   }
   return psc;
}


#ifdef NEVER_RELEASED
DDCA_Status
ddca_get_feature_flags_by_version_id(
      DDCA_Vcp_Feature_Code         feature_code,
 //   DDCA_MCCS_Version_Spec        vspec,
      DDCA_MCCS_Version_Id          mccs_version_id,
      DDCA_Feature_Flags *          feature_flags)
{
   DDCA_Status psc = DDCRC_ARG;
   DDCA_Version_Feature_Info * full_info =  get_version_feature_info_by_version_id(
         feature_code,
         mccs_version_id,
         false,                       // with_default
         true);                       // false => version specific, true=> version sensitive
   if (full_info) {
      *feature_flags = full_info->feature_flags;
      free_version_feature_info(full_info);
      psc = 0;
   }
   return psc;
}
#endif


DDCA_Status
ddca_get_feature_info_by_display(
      DDCA_Display_Handle           ddca_dh,    // needed because in rare cases feature info is MCCS version dependent
      DDCA_Vcp_Feature_Code         feature_code,
      DDCA_Version_Feature_Info **  p_info)
{
   WITH_DH(
      ddca_dh,
      {
         DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(ddca_dh);
         //DDCA_MCCS_Version_Id   version_id = mccs_version_spec_to_id(vspec);
         //psc = ddca_get_feature_info_by_vcp_version(feature_code, version_id, p_info);

         *p_info = get_version_feature_info_by_vspec(
                      feature_code,
                      vspec,
                      false,      //   with_default,
                      true);      //   version_sensitive
         if (!*p_info)
            psc = DDCRC_ARG;
      }
   );
}


// Add with_default flag?
DDCA_Status
ddca_get_feature_metadata_by_vspec(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_MCCS_Version_Spec      vspec,
      bool                        create_default_if_not_found,
      DDCA_Feature_Metadata *     info) //   change to **?
{
   // DBGMSG("vspec=%d.%d", vspec.major, vspec.minor);
   DDCA_Status psc = DDCRC_ARG;
   memset(info, 0, sizeof(DDCA_Feature_Metadata));
   memcpy(info->marker, DDCA_FEATURE_METADATA_MARKER, 4);
   DDCA_Version_Feature_Info * full_info =
         get_version_feature_info_by_vspec(
               feature_code,
               vspec,
               create_default_if_not_found,
               true);                      // false => version specific, true=> version sensitive
   if (full_info) {
      // DBGMSG("Reading full_info");
      info->feature_code  = feature_code;
 //   info->vspec         = vspec;
      info->feature_flags = full_info->feature_flags;
      if (info->feature_flags & DDCA_SIMPLE_NC)
         info->sl_values = full_info->sl_values;
      if (info->feature_flags & DDCA_SYNTHETIC) {
         // strdup so that don't have to worry about synthesized entries when free
         if (full_info->feature_name)
            info->feature_name  = strdup(full_info->feature_name);
         if (full_info->desc)
            info->feature_desc  = strdup(full_info->desc);
      }
      else {
         info->feature_name  = full_info->feature_name;
         info->feature_desc  = full_info->desc;
      }
      // DBGMSG("Reading full_info done");

      free_version_feature_info(full_info);
      psc = 0;
   }
   return psc;
}


DDCA_Status
ddca_get_feature_metadata_by_dref(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Display_Ref            ddca_dref,
      bool                        create_default_if_not_found,
      DDCA_Feature_Metadata *     info)
{
   WITH_DR(
         ddca_dref,
         {
               // DBGMSG("Starting");
               // dbgrpt_display_ref(dref, 1);

               // returns dref->vcp_version if already cached, queries monitor if not
               DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_ref(ddca_dref);

               psc = ddca_get_feature_metadata_by_vspec(
                     feature_code,
                     vspec,               // dref->vcp_version,
                     create_default_if_not_found,
                     info);
         }
      );
}


DDCA_Status
ddca_get_feature_metadata_by_dh(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Display_Handle         ddca_dh,
      bool                        create_default_if_not_found,
      DDCA_Feature_Metadata *     info)
{
   WITH_DH(
         ddca_dh,
         {
               // DBGMSG("Starting");
               // dbgrpt_display_ref(dh->dref, 1);

               // Note:  dh->dref->vcp_version may be Unqueried (255,255)
               // Query vcp version here instead of calling
               // ddca_get_feature_metadata_by_dref() because we already have
               // display handle, don't need to open display.

                DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(ddca_dh);
                // DDCA_Monitor_Model_Key* p_mmid = dh->dref->mmid;

                psc = ddca_get_feature_metadata_by_vspec(
                      feature_code,
                      vspec,               // dref->vcp_version,
                      create_default_if_not_found,
                      info);
#ifdef NO
                psc = ddca_get_feature_metadata_by_dref(
                     feature_code,
                     dh->dref,
                     create_default_if_not_found,
                     info);
#endif
         }
      );
}

// frees the contents of info, not info itself
DDCA_Status
ddca_free_feature_metadata_contents(DDCA_Feature_Metadata info) {
   if ( memcmp(info.marker, DDCA_FEATURE_METADATA_MARKER, 4) == 0) {
      if (info.feature_flags & DDCA_SYNTHETIC) {
         free(info.feature_name);
         free(info.feature_desc);
      }
      info.marker[3] = 'x';
   }
   return 0;
}


DDCA_Status
ddca_free_feature_info(
      DDCA_Version_Feature_Info * info)
{
   DDCA_Status rc = 0;
   if (info) {
      if (memcmp(info->marker, VCP_VERSION_SPECIFIC_FEATURE_INFO_MARKER, 4) != 0 )  {
        rc = DDCRC_ARG;
      }
      else {
         free_version_feature_info(info);
      }
   }
   return rc;
}

// returns pointer into permanent internal data structure, caller should not free
char *
ddca_get_feature_name(DDCA_Vcp_Feature_Code feature_code) {
   // do we want get_feature_name()'s handling of mfg specific and unrecognized codes?
   char * result = get_feature_name_by_id_only(feature_code);
   return result;
}

char *
ddca_feature_name_by_vspec(
      DDCA_Vcp_Feature_Code    feature_code,
      DDCA_MCCS_Version_Spec   vspec,
      DDCA_Monitor_Model_Key * p_mmid)  // currently ignored
{
   char * result = get_feature_name_by_id_and_vcp_version(feature_code, vspec);
   return result;
}

#ifdef NEVER_RELEASED
/** \deprecated */
char *
ddca_feature_name_by_version_id(
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_MCCS_Version_Id   mccs_version_id)
{
   DDCA_MCCS_Version_Spec vspec = mccs_version_id_to_spec(mccs_version_id);
   char * result = get_feature_name_by_id_and_vcp_version(feature_code, vspec);
   return result;
}
#endif


DDCA_Status
ddca_get_feature_name_by_dref(
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_Display_Ref       ddca_dref,
      char **                name_loc)
{
   WITH_DR(ddca_dref,
         {
               //*name_loc = ddca_feature_name_by_vspec(feature_code, dref->vcp_version, dref->mmid);
               *name_loc = get_feature_name_by_id_and_vcp_version(feature_code, dref->vcp_version);
               if (!*name_loc)
                  psc = -EINVAL;
         }
   )
}


//
// Display Inquiry
//

DDCA_Status
ddca_get_simple_sl_value_table_by_vspec(
      DDCA_Vcp_Feature_Code      feature_code,
      DDCA_MCCS_Version_Spec     vspec,
      const DDCA_Monitor_Model_Key *   p_mmid,   // currently ignored
      DDCA_Feature_Value_Entry** value_table_loc)
{
   bool debug = false;
   DDCA_Status rc = 0;
   *value_table_loc = NULL;
   DBGMSF(debug, "feature_code = 0x%02x, vspec=%d.%d",
                 feature_code, vspec.major, vspec.minor);

   if (!vcp_version_is_valid(vspec, /* unknown_ok */ true)) {
      rc = DDCRC_ARG;
      goto bye;
   }

   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
   if (!pentry) {
        *value_table_loc = NULL;
        rc = DDCRC_UNKNOWN_FEATURE;
  }
  else {
     DDCA_Version_Feature_Flags vflags = get_version_sensitive_feature_flags(pentry, vspec);
     if (!(vflags & DDCA_SIMPLE_NC)) {
        *value_table_loc = NULL;
        rc = DDCRC_INVALID_OPERATION;
     }
     else  {
        DDCA_Feature_Value_Entry * table = get_version_specific_sl_values(pentry, vspec);
        DDCA_Feature_Value_Entry * table2 = (DDCA_Feature_Value_Entry*) table;    // identical definitions
        *value_table_loc = table2;
        rc = 0;
        DDCA_Feature_Value_Entry * cur = table2;
        if (debug) {
           while (cur->value_name) {
              DBGMSG("   0x%02x - %s", cur->value_code, cur->value_name);
              cur++;
           }
        }
     }
  }

bye:
  DBGMSF(debug, "Done. *pvalue_table=%p, returning %s", *value_table_loc, psc_desc(rc));

   return rc;
}


// for now, just gets SL value table based on the vspec of the display ref,
// eventually handle dynamically assigned monitor specs
DDCA_Status
ddca_get_simple_sl_value_table_by_dref(
      DDCA_Vcp_Feature_Code      feature_code,
      DDCA_Display_Ref           ddca_dref,
      DDCA_Feature_Value_Entry** value_table_loc)
{
   WITH_DR(ddca_dref,
      {
         psc = ddca_get_simple_sl_value_table_by_vspec(
                  feature_code, dref->vcp_version, dref->mmid, value_table_loc);
      }
   )
}


DDCA_Status
ddca_get_simple_sl_value_table(
      DDCA_Vcp_Feature_Code      feature_code,
      DDCA_MCCS_Version_Id       mccs_version_id,
      DDCA_Feature_Value_Entry** value_table_loc)
{
   bool debug = false;
   DDCA_Status rc = 0;
   *value_table_loc = NULL;
   DDCA_MCCS_Version_Spec vspec = mccs_version_id_to_spec(mccs_version_id);
   DBGMSF(debug, "feature_code = 0x%02x, mccs_version_id=%d, vspec=%d.%d",
                 feature_code, mccs_version_id, vspec.major, vspec.minor);

   rc = ddca_get_simple_sl_value_table_by_vspec(
           feature_code, vspec, &DDCA_UNDEFINED_MONITOR_MODEL_KEY,  value_table_loc);

   DBGMSF(debug, "Done. *pvalue_table=%p, returning %s", *value_table_loc, psc_desc(rc));
   return rc;
}


// typedef void * Feature_Value_Table;   // temp

DDCA_Status
ddca_get_simple_nc_feature_value_name_by_table(
      DDCA_Feature_Value_Table    feature_value_table,
      uint8_t                     feature_value,
      char**                      value_name_loc)
{
   // DBGMSG("feature_value_table=%p", feature_value_table);
   // DBGMSG("*feature_value_table=%p", *feature_value_table);
   DDCA_Status rc = 0;
   DDCA_Feature_Value_Entry * feature_value_entries = feature_value_table;
   *value_name_loc = get_feature_value_name(feature_value_entries, feature_value);
   if (!*value_name_loc)
      rc = DDCRC_NOT_FOUND;               // correct handling for value not found?
   return rc;
}


DDCA_Status
ddca_get_simple_nc_feature_value_name_by_vspec(
      DDCA_Vcp_Feature_Code    feature_code,
      DDCA_MCCS_Version_Spec   vspec,    // needed because value lookup mccs version dependent
      const DDCA_Monitor_Model_Key * p_mmid,
      uint8_t                  feature_value,
      char**                   feature_name_loc)
{
   DDCA_Feature_Value_Entry * feature_value_entries = NULL;

   // this should be a function in vcp_feature_codes:
   DDCA_Status rc = ddca_get_simple_sl_value_table_by_vspec(
                      feature_code, vspec, p_mmid, &feature_value_entries);
   if (rc == 0) {
      // DBGMSG("&feature_value_entries = %p", &feature_value_entries);
      rc = ddca_get_simple_nc_feature_value_name_by_table(feature_value_entries, feature_value, feature_name_loc);
   }
   return rc;
}


// deprecated
DDCA_Status
ddca_get_simple_nc_feature_value_name_by_display(
      DDCA_Display_Handle    ddca_dh,    // needed because value lookup mccs version dependent
      DDCA_Vcp_Feature_Code  feature_code,
      uint8_t                feature_value,
      char**                 feature_name_loc)
{
   WITH_DH(ddca_dh,  {
         DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
         DDCA_Monitor_Model_Key * p_mmid = dh->dref->mmid;
         return ddca_get_simple_nc_feature_value_name_by_vspec(
                   feature_code, vspec, p_mmid, feature_value, feature_name_loc);
      }
   );
}


//
// Get and Set Feature Values
//

#ifdef OLD
// Was public, but eliminated from API due to problems in Python API caused by overlay.
// Retained for impedance matching.  Retained for historical interest.

/** Represents a single non-table VCP value */
typedef struct {
   DDCA_Vcp_Feature_Code  feature_code;
   union {
      struct {
         uint16_t   max_val;        /**< maximum value (mh, ml bytes) for continuous value */
         uint16_t   cur_val;        /**< current value (sh, sl bytes) for continuous value */
      }         c;                  /**< continuous (C) value */
      struct {
   // Ensure proper overlay of ml/mh on max_val, sl/sh on cur_val

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
      uint8_t    ml;            /**< ML byte for NC value */
      uint8_t    mh;            /**< MH byte for NC value */
      uint8_t    sl;            /**< SL byte for NC value */
      uint8_t    sh;            /**< SH byte for NC value */
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
      uint8_t    mh;
      uint8_t    ml;
      uint8_t    sh;
      uint8_t    sl;
#else
#error "Unexpected byte order value: __BYTE_ORDER__"
#endif
      }         nc;                /**< non-continuous (NC) value */
   } val;
} Non_Table_Value_Response;
#endif


DDCA_Status
ddca_get_non_table_vcp_value(
      DDCA_Display_Handle        ddca_dh,
      DDCA_Vcp_Feature_Code      feature_code,
      DDCA_Non_Table_Vcp_Value*  valrec)
{

   WITH_DH(ddca_dh,  {
       Error_Info * ddc_excp = NULL;
       Parsed_Nontable_Vcp_Response * code_info;
       ddc_excp = ddc_get_nontable_vcp_value(
                     dh,
                     feature_code,
                     &code_info);

       if (!ddc_excp) {
          valrec->mh = code_info->mh;
          valrec->ml = code_info->ml;;
          valrec->sh = code_info->sh;
          valrec->sl = code_info->sl;
          // DBGMSG("valrec:  mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x",
          //        valrec->mh, valrec->ml, valrec->sh, valrec->sl);
          free(code_info);
       }
       else {
          psc = ddc_excp->status_code;
          errinfo_free_with_report(ddc_excp, report_freed_exceptions, __func__);
          // errinfo_free(ddc_excp);
       }
    } );
}


// untested
DDCA_Status
ddca_get_table_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_Table_Vcp_Value **    table_value_loc)
{
   WITH_DH(ddca_dh,
      {
         Error_Info * ddc_excp = NULL;
         Buffer * p_table_bytes = NULL;
         ddc_excp =  ddc_get_table_vcp_value(dh, feature_code, &p_table_bytes);
         psc = (ddc_excp) ? ddc_excp->status_code : 0;
         errinfo_free(ddc_excp);
         if (psc == 0) {
            assert(p_table_bytes);  // avoid coverity warning
            int len = p_table_bytes->len;
            DDCA_Table_Vcp_Value * tv = calloc(1,sizeof(DDCA_Table_Vcp_Value));
            tv->bytect = len;
            if (len > 0) {
               tv->bytes = malloc(len);
               memcpy(tv->bytes, p_table_bytes->bytes, len);
            }
            *table_value_loc = tv;

            buffer_free(p_table_bytes, __func__);
         }
      }
   );
}


static
DDCA_Status
ddca_get_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      DDCA_Vcp_Value_Type    call_type,   // why is this needed?   look it up from dh and feature_code
      Single_Vcp_Value **    pvalrec)
{
   Error_Info * ddc_excp = NULL;

   WITH_DH(ddca_dh,
         {
               bool debug = false;
               DBGMSF(debug, "Starting. ddca_dh=%p, feature_code=0x%02x, call_type=%d, pvalrec=%p",
                      ddca_dh, feature_code, call_type, pvalrec);
               *pvalrec = NULL;
               ddc_excp = ddc_get_vcp_value(dh, feature_code, call_type, pvalrec);
               psc = (ddc_excp) ? ddc_excp->status_code : 0;
               errinfo_free(ddc_excp);
               DBGMSF(debug, "*pvalrec=%p", *pvalrec);
         }
   );
}

#ifdef OLD
static DDCA_Vcp_Value_Type_Parm
get_value_type_parm(
      DDCA_Display_Handle         ddca_dh,
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Vcp_Value_Type_Parm    default_value)
{
   bool debug = false;
   DBGMSF(debug, "Starting. ddca_dh=%p, feature_code=0x%02x, default_value=%d",
          ddca_dh, feature_code, default_value);
   DDCA_Vcp_Value_Type_Parm result = default_value;
   DDCA_MCCS_Version_Spec vspec      = get_vcp_version_by_display_handle(ddca_dh);
   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
   if (pentry) {
      DDCA_Version_Feature_Flags flags = get_version_sensitive_feature_flags(pentry, vspec);
      // Version_Feature_Flags flags = feature_info->internal_feature_flags;
      // n. will default to NON_TABLE_VCP_VALUE if not a known code
      result = (flags & DDCA_TABLE) ?  DDCA_TABLE_VCP_VALUE : DDCA_NON_TABLE_VCP_VALUE;
   }

   DBGMSF(debug, "Returning %d", result);
   return result;
}
#endif

static DDCA_Status
get_value_type(
      DDCA_Display_Handle         ddca_dh,
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Vcp_Value_Type *       p_value_type)
{
   bool debug = false;
   DBGMSF(debug, "Starting. ddca_dh=%p, feature_code=0x%02x", ddca_dh, feature_code);

   DDCA_Status ddcrc = DDCRC_NOT_FOUND;
   DDCA_MCCS_Version_Spec vspec     = get_vcp_version_by_display_handle(ddca_dh);
   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
   if (pentry) {
      DDCA_Version_Feature_Flags flags = get_version_sensitive_feature_flags(pentry, vspec);
      // Version_Feature_Flags flags = feature_info->internal_feature_flags;
      // n. will default to NON_TABLE_VCP_VALUE if not a known code
      *p_value_type = (flags & DDCA_TABLE) ?  DDCA_TABLE_VCP_VALUE : DDCA_NON_TABLE_VCP_VALUE;
      ddcrc = 0;
   }

   DBGMSF(debug, "Returning %d", ddcrc);
   return ddcrc;
}


DDCA_Status
ddca_get_any_vcp_value_using_explicit_type(
       DDCA_Display_Handle         ddca_dh,
       DDCA_Vcp_Feature_Code       feature_code,
       DDCA_Vcp_Value_Type         call_type,
       DDCA_Any_Vcp_Value **       pvalrec)
{
   bool debug = false;
   DBGMSF(debug, "Starting. ddca_dh=%p, feature_code=0x%02x, call_type=%d, pvalrec=%p",
          ddca_dh, feature_code, call_type, pvalrec);
   *pvalrec = NULL;
   DDCA_Status rc = DDCRC_ARG;

   Single_Vcp_Value *  valrec2 = NULL;
   rc = ddca_get_vcp_value(ddca_dh, feature_code, call_type, &valrec2);
   if (rc == 0) {
      DDCA_Any_Vcp_Value * valrec = single_vcp_value_to_any_vcp_value(valrec2);
      free_single_vcp_value(valrec2);
      *pvalrec = valrec;
   }

   DBGMSF(debug, "Done. Returning %s, *pvalrec=%p", psc_desc(rc), *pvalrec);
   return rc;
}


#ifdef ALT
DDCA_Status
ddca_get_any_vcp_value_using_explicit_type_new(
       DDCA_Display_Handle         ddca_dh,
       DDCA_Vcp_Feature_Code       feature_code,
       DDCA_Vcp_Value_Type_Parm    call_type,
       DDCA_Any_Vcp_Value **       pvalrec)
{
   bool debug = false;
   DBGMSF(debug, "Starting. ddca_dh=%p, feature_code=0x%02x, call_type=%d, pvalrec=%p",
          ddca_dh, feature_code, call_type, pvalrec);
   *pvalrec = NULL;
   DDCA_Status rc = DDCRC_ARG;

   if (call_type == DDCA_UNSET_VCP_VALUE_TYPE_PARM) {
      call_type = get_value_type_parm(ddca_dh, feature_code, DDCA_UNSET_VCP_VALUE_TYPE_PARM);
   }
   if (call_type != DDCA_UNSET_VCP_VALUE_TYPE_PARM) {

      Single_Vcp_Value *  valrec2 = NULL;
      rc = ddca_get_vcp_value(ddca_dh, feature_code, call_type, &valrec2);
      if (rc == 0) {
         DDCA_Any_Vcp_Value * valrec = single_vcp_value_to_any_vcp_value(valrec2);
         free(valrec2);   // n. does not free table bytes, which are now pointed to by valrec
         *pvalrec = valrec;
      }
   }
   DBGMSF(debug, "Done. Returning %s, *pvalrec=%p", psc_desc(rc), *pvalrec);
   return rc;
}
#endif


DDCA_Status
ddca_get_any_vcp_value_using_implicit_type(
       DDCA_Display_Handle         ddca_dh,
       DDCA_Vcp_Feature_Code       feature_code,
       DDCA_Any_Vcp_Value **       valrec_loc)
{

   DDCA_Vcp_Value_Type call_type;

   DDCA_Status ddcrc = get_value_type(ddca_dh, feature_code, &call_type);

   if (ddcrc == 0) {
      ddcrc = ddca_get_any_vcp_value_using_explicit_type(
                 ddca_dh,
                 call_type,
                 feature_code,
                 valrec_loc);
   }
   return ddcrc;
}


void
ddca_free_any_vcp_value(
      DDCA_Any_Vcp_Value * valrec)
{
   if (valrec->value_type == DDCA_TABLE_VCP_VALUE) {
      free(valrec->val.t.bytes);
   }
   free(valrec);
}


void
dbgrpt_any_vcp_value(
      DDCA_Any_Vcp_Value * valrec,
      int                  depth)
{
   int d1 = depth+1;
   rpt_vstring(depth, "DDCA_Any_Vcp_Value at %p:", valrec);
   rpt_vstring(d1, "opcode=0x%02x, value_type=%s (0x%02x)",
                   valrec->opcode, vcp_value_type_name(valrec->value_type), valrec->value_type);
   if (valrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
      rpt_vstring(d1, "mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x",
                      valrec->val.c_nc.mh, valrec->val.c_nc.ml,
                      valrec->val.c_nc.sh, valrec->val.c_nc.sl);
      uint16_t max_val = valrec->val.c_nc.mh << 8 | valrec->val.c_nc.ml;
      uint16_t cur_val = valrec->val.c_nc.sh << 8 | valrec->val.c_nc.sl;
      rpt_vstring(d1, "max_val=%d (0x%04x), cur_val=%d (0x%04x)",
                      max_val,
                      max_val,
                      cur_val,
                      cur_val);
   }
   else if (valrec->value_type == DDCA_TABLE_VCP_VALUE) {
      rpt_hex_dump(valrec->val.t.bytes, valrec->val.t.bytect, d1);
   }
   else {
      rpt_vstring(d1, "Unrecognized value type: %d", valrec->value_type);
   }
}


DDCA_Status
ddca_get_formatted_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      char**                 formatted_value_loc)
{
   bool debug = true;
   DBGMSF(debug, "Starting. feature_code=0x%02x", feature_code);
   Error_Info * ddc_excp = NULL;
   WITH_DH(ddca_dh,
         {
               *formatted_value_loc = NULL;
               DDCA_MCCS_Version_Spec vspec      = get_vcp_version_by_display_handle(dh);
               // DDCA_MCCS_Version_Id   version_id = mccs_version_spec_to_id(vspec);

               VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
               if (!pentry) {
#ifdef ALT
               DDCA_Version_Feature_Info * feature_info =
               get_version_specific_feature_info(
                     feature_code,
                     true,                    //    with_default
                     version_id);
               assert(feature_info);
               if (!feature_info) {
#endif
                  psc = DDCRC_ARG;
               }
               else {
                  DDCA_Version_Feature_Flags flags = get_version_sensitive_feature_flags(pentry, vspec);
                  if (!(flags & DDCA_READABLE)) {
                     if (flags & DDCA_DEPRECATED)
                        *formatted_value_loc = g_strdup_printf("Feature %02x is deprecated in MCCS %d.%d\n",
                                                          feature_code, vspec.major, vspec.minor);
                     else
                        *formatted_value_loc = g_strdup_printf("Feature %02x is not readable\n", feature_code);
                     DBGMSF(debug, "%s", *formatted_value_loc);
                     psc = DDCRC_INVALID_OPERATION;
                  }
                  else {
                     // Version_Feature_Flags flags = feature_info->internal_feature_flags;
                      // n. will default to NON_TABLE_VCP_VALUE if not a known code
                      DDCA_Vcp_Value_Type call_type = (flags & DDCA_TABLE) ?  DDCA_TABLE_VCP_VALUE : DDCA_NON_TABLE_VCP_VALUE;
                      Single_Vcp_Value * pvalrec;
                      ddc_excp = ddc_get_vcp_value(dh, feature_code, call_type, &pvalrec);
                      psc = (ddc_excp) ? ddc_excp->status_code : 0;
                      errinfo_free(ddc_excp);
                      if (psc == 0) {
                         bool ok =
                         vcp_format_feature_detail(
                                pentry,
                                vspec,
                                pvalrec,
                                formatted_value_loc
                              );
                         if (!ok) {
                            psc = DDCRC_OTHER;    // ** WRONG CODE ***
                            assert(!formatted_value_loc);
                         }
                      }
                  }
               }
         }
   )
}


DDCA_Status
ddca_format_any_vcp_value(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_MCCS_Version_Spec  vspec,
      DDCA_Monitor_Model_Key * mmid,
      DDCA_Any_Vcp_Value *    anyval,
      char **                 formatted_value_loc)
{
   bool debug = false;
   DBGMSF(debug, "Starting. feature_code=0x%02x", feature_code);
   DDCA_Status psc = 0;

   *formatted_value_loc = NULL;

   // DDCA_MCCS_Version_Id   version_id = mccs_version_spec_to_id(vspec);

   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid_w_default(feature_code);
   if (!pentry) {
      psc = DDCRC_ARG;
      *formatted_value_loc = g_strdup_printf("Unrecognized feature code 0x%02x", feature_code);
      goto bye;
   }

   DDCA_Version_Feature_Flags flags = get_version_sensitive_feature_flags(pentry, vspec);
   if (!(flags & DDCA_READABLE)) {
      if (flags & DDCA_DEPRECATED)
         *formatted_value_loc = g_strdup_printf("Feature %02x is deprecated in MCCS %d.%d",
                                           feature_code, vspec.major, vspec.minor);
      else
         *formatted_value_loc = g_strdup_printf("Feature %02x is not readable", feature_code);
      DBGMSF(debug, "%s", *formatted_value_loc);
      psc = DDCRC_INVALID_OPERATION;
      goto bye;
   }

   // Version_Feature_Flags flags = feature_info->internal_feature_flags;
   // n. will default to NON_TABLE_VCP_VALUE if not a known code
   DDCA_Vcp_Value_Type call_type = (flags & DDCA_TABLE)
                                        ? DDCA_TABLE_VCP_VALUE
                                        : DDCA_NON_TABLE_VCP_VALUE;
   if (call_type != anyval->value_type) {
       *formatted_value_loc = g_strdup_printf(
             "Feature type in value does not match feature code");
       psc = DDCRC_ARG;
       goto bye;
   }

   // only copies pointer to table bytes, not the bytes
   Single_Vcp_Value * valrec = any_vcp_value_to_single_vcp_value(anyval);
   bool ok = vcp_format_feature_detail(pentry,vspec, valrec,formatted_value_loc);
   if (!ok) {
       psc = DDCRC_ARG;    // ??
       assert(!formatted_value_loc);
       *formatted_value_loc = g_strdup_printf("Unable to format value for feature 0x%02x", feature_code);
   }
   free(valrec);  // does not free any table bytes, which are in anyval

bye:
   if (pentry)
      free_synthetic_vcp_entry(pentry);   // does nothing if not synthetic

   DBGMSF(debug, "Returning: %s, formatted_value_loc -> %s", psc_desc(psc), *formatted_value_loc);
   return psc;
}


DDCA_Status
ddca_format_any_vcp_value_by_dref(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Display_Ref        ddca_dref,
      DDCA_Any_Vcp_Value *    valrec,
      char **                 formatted_value_loc)
{
   WITH_DR(ddca_dref,
         {
               return ddca_format_any_vcp_value(
                         feature_code,
                         dref->vcp_version,
                         dref->mmid,
                         valrec,
                         formatted_value_loc);
         }
   )
}


DDCA_Status
ddca_format_non_table_vcp_value(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_MCCS_Version_Spec      vspec,
      DDCA_Monitor_Model_Key *    mmid,
      DDCA_Non_Table_Vcp_Value *  valrec,
      char **                     formatted_value_loc)
{
   DDCA_Any_Vcp_Value anyval;
   anyval.opcode = feature_code;
   anyval.value_type = DDCA_NON_TABLE_VCP_VALUE;
   anyval.val.c_nc.mh = valrec->mh;
   anyval.val.c_nc.ml = valrec->ml;
   anyval.val.c_nc.sh = valrec->sh;
   anyval.val.c_nc.sl = valrec->sl;

   return ddca_format_any_vcp_value(feature_code, vspec, mmid, &anyval, formatted_value_loc);
}

DDCA_Status
ddca_format_non_table_vcp_value_by_dref(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Display_Ref            ddca_dref,
      DDCA_Non_Table_Vcp_Value *  valrec,
      char **                     formatted_value_loc)
{
   WITH_DR(ddca_dref,
         {
               return ddca_format_non_table_vcp_value(
                         feature_code,
                         dref->vcp_version,
                         dref->mmid,
                         valrec,
                         formatted_value_loc);
         }
   )
}


DDCA_Status
ddca_format_table_vcp_value(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_MCCS_Version_Spec  vspec,
      DDCA_Monitor_Model_Key * mmid,
      DDCA_Table_Vcp_Value *  table_value,
      char **                 formatted_value_loc)
{
   DDCA_Any_Vcp_Value anyval;
   anyval.opcode = feature_code;
   anyval.value_type = DDCA_TABLE_VCP_VALUE;
   anyval.val.t.bytect = table_value->bytect;
   anyval.val.t.bytes  = table_value->bytes;   // n. copying pointer, not duplicating bytes

   return ddca_format_any_vcp_value(
             feature_code, vspec, mmid, &anyval, formatted_value_loc);
}


DDCA_Status
ddca_format_table_vcp_value_by_dref(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Display_Ref        ddca_dref,
      DDCA_Table_Vcp_Value *  table_value,
      char **                 formatted_value_loc)
{
   WITH_DR(ddca_dref,
         {
               return ddca_format_table_vcp_value(
                         feature_code,
                         dref->vcp_version,
                         dref->mmid,
                         table_value,
                         formatted_value_loc);
         }
   )
}


static
DDCA_Status
set_single_vcp_value(
      DDCA_Display_Handle  ddca_dh,
      Single_Vcp_Value *   valrec,
      Single_Vcp_Value **  verified_value_loc)
{
      Error_Info * ddc_excp = NULL;
      WITH_DH(ddca_dh,  {
            ddc_excp = ddc_set_vcp_value(dh, valrec, verified_value_loc);
            psc = (ddc_excp) ? ddc_excp->status_code : 0;
            errinfo_free(ddc_excp);
         } );
}

// UNPUBLISHED
/** Sets a Continuous VCP value.
 *
 *  @param[in]  ddca_dh             display_handle
 *  @param[in]  feature_code        VCP feature code
 *  @param[in]  new_value           value to set (sign?)
 *  @param[out] verified_value_loc  if non-null, return verified value here
 *  @return status code
 *
 * @remark
 *  Verification is performed if **verified_value_loc** is non-NULL and
 *  verification has been enabled (see #ddca_enable_verify()).
 *  @remark
 *  If verification is performed, the value of the feature is read after being
 *  written. If the returned status code is either DDCRC_OK (0) or DDCRC_VERIFY,
 *  the verified value is returned in **verified_value_loc**.
 *  @remark
 *  This is essentially a convenience function, since a Continuous value can be
 *  set by passing its high and low bytes to #ddca_set_non_table_vcp_value_verify().
 */
DDCA_Status
ddca_set_continuous_vcp_value_verify(
      DDCA_Display_Handle   ddca_dh,
      DDCA_Vcp_Feature_Code feature_code,
      uint16_t              new_value,
      uint16_t *            verified_value_loc)
{
   DDCA_Status rc = 0;

   Single_Vcp_Value valrec;
   valrec.opcode = feature_code;
   valrec.value_type = DDCA_NON_TABLE_VCP_VALUE;
   valrec.val.c.cur_val = new_value;

   if (verified_value_loc) {
      Single_Vcp_Value * verified_single_value = NULL;
      rc = set_single_vcp_value(ddca_dh, &valrec, &verified_single_value);
      if (verified_single_value)
         *verified_value_loc = verified_single_value->val.c.cur_val;
   }
   else {
      rc = set_single_vcp_value(ddca_dh, &valrec, NULL);
   }

   return rc;
}

// DEPRECATED AS OF 0.9.0
/** Sets a Continuous VCP value.
 *
 *  @param[in]  ddca_dh             display_handle
 *  @param[in]  feature_code        VCP feature code
 *  @param[in]  new_value           value to set (sign?)
 *  @return status code
 *
 * @remark
 *  This is essentially a convenience function, since a Continuous value
 *  can be set by passing its high and low bytes to #ddca_set_non_table_vcp_value().
 */
DDCA_Status
ddca_set_continuous_vcp_value(
      DDCA_Display_Handle   ddca_dh,
      DDCA_Vcp_Feature_Code feature_code,
      uint16_t              new_value)
{
   return ddca_set_continuous_vcp_value_verify(ddca_dh, feature_code, new_value, NULL);
}


/** \deprecated */
DDCA_Status
ddca_set_simple_nc_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      Byte                   new_value)
{
   return ddca_set_continuous_vcp_value_verify(ddca_dh, feature_code, new_value, NULL);
}

// UNPUBLISHED
/** Sets a non-table VCP value by specifying it's high and low bytes individually.
 *  Optionally returns the values set by reading the feature code after writing.
 *
 *  \param[in]   ddca_dh             display handle
 *  \param[in]   feature_code        feature code
 *  \param[in]   hi_byte             high byte of new value
 *  \param[in]   lo_byte             low byte of new value
 *  \param[out]  p_verified_hi_byte  where to return high byte of verified value
 *  \param[out]  p_verified_lo_byte  where to return low byte of verified value
 *  \return      status code
 *
 *  \remark
 *  Either both **verified_hi_byte_loc** and **verified_lo_byte_loc** should be
 *  set, or neither. Otherwise, status code **DDCRC_ARG** is returned.
 *  \remark
 *  Verification is performed only it has been enabled (see #ddca_enable_verify()) and
 *  both **verified_hi_byte** and **verified_lo_byte** are set.
 *  \remark
 *  Verified values are returned if the status code is either 0 (success),
 *  or **DDCRC_VERIFY**, i.e. the write succeeded but verification failed.
 */
DDCA_Status
ddca_set_non_table_vcp_value_verify(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      Byte                   hi_byte,
      Byte                   lo_byte,
      Byte *                 verified_hi_byte_loc,
      Byte *                 verified_lo_byte_loc)
{
   if ( ( verified_hi_byte_loc && !verified_lo_byte_loc) ||
        (!verified_hi_byte_loc &&  verified_lo_byte_loc )
      )
      return DDCRC_ARG;

   // unwrap into 2 cases to clarify logic and avoid compiler warning
   DDCA_Status rc = 0;
   if (verified_hi_byte_loc) {
      uint16_t verified_c_value = 0;
      rc = ddca_set_continuous_vcp_value_verify(
                          ddca_dh,
                          feature_code, hi_byte << 8 | lo_byte,
                          &verified_c_value);
      *verified_hi_byte_loc = verified_c_value >> 8;
      *verified_lo_byte_loc = verified_c_value & 0xff;
   }
   else {
      rc = ddca_set_continuous_vcp_value_verify(
                          ddca_dh,
                          feature_code, hi_byte << 8 | lo_byte,
                          NULL);
   }

   return rc;
}

DDCA_Status
ddca_set_non_table_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      Byte                   hi_byte,
      Byte                   lo_byte)
{
   return ddca_set_non_table_vcp_value_verify(ddca_dh, feature_code, hi_byte, lo_byte, NULL, NULL);
}

// UNPUBLISHED
/** Sets a table VCP value.
 *  Optionally returns the value set by reading the feature code after writing.
 *
 *  \param[in]   ddca_dh             display handle
 *  \param[in]   feature_code        feature code
 *  \param[in]   new_value           value to set
 *  \param[out]  verified_value_loc  where to return verified value
 *  \return      status code
 *
 *  \remark
 *  Verification is performed only it has been enabled (see #ddca_enable_verify()) and
 *  **verified_value** is set.
 *  \remark
 *  A verified value is returned if either the status code is either 0 (success),
 *  or **DDCRC_VERIFY**, i.e. the write succeeded but verification failed.
 */
// untested
DDCA_Status
ddca_set_table_vcp_value_verify(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Table_Vcp_Value *      table_value,
      DDCA_Table_Vcp_Value **     verified_value_loc)
{
   DDCA_Status rc = 0;

    Single_Vcp_Value valrec;
    valrec.opcode = feature_code;
    valrec.value_type = DDCA_TABLE_VCP_VALUE;
    valrec.val.t.bytect = table_value->bytect;
    valrec.val.t.bytes  = table_value->bytes;  // copies pointer, not bytes

    if (verified_value_loc) {
       Single_Vcp_Value * verified_single_value = NULL;
       rc = set_single_vcp_value(ddca_dh, &valrec, &verified_single_value);
       if (verified_single_value) {
          DDCA_Table_Vcp_Value * verified_table_value = calloc(1,sizeof(DDCA_Table_Vcp_Value));
          verified_table_value->bytect = verified_single_value->val.t.bytect;
          verified_table_value->bytes  = verified_single_value->val.t.bytes;
          free(verified_single_value);  // n. does not free bytes
          *verified_value_loc = verified_table_value;
       }
    }
    else {
       rc = set_single_vcp_value(ddca_dh, &valrec, NULL);
    }

    return rc;
}

DDCA_Status
ddca_set_table_vcp_value(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Table_Vcp_Value *      table_value)
{
   return ddca_set_table_vcp_value_verify(ddca_dh, feature_code, table_value, NULL);
}

// UNPUBLISHED
/** Sets a VCP value of any type.
 *  Optionally returns the values se by reading the feature code after writing.
 *
 *  \param[in]   ddca_dh        display handle
 *  \param[in]   feature_code   feature code
 *  \param[in]   new_value      value to set
 *  \param[out]  verified_value where to return verified value
 *  \return      status code
 *
 *  \remark
 *  Verification is performed only it has been enabled (see #ddca_enable_verify()) and
 *  **verified_value** is set.
 *  \remark
 *  A verified value is returned if either the status code is either 0 (success),
 *  or **DDCRC_VERIFY**, i.e. the write succeeded but verification failed.
 */
// untested for table values
DDCA_Status
ddca_set_any_vcp_value_verify(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Any_Vcp_Value *    new_value,
      DDCA_Any_Vcp_Value **   verified_value_loc)
{
   DDCA_Status rc = 0;

   Single_Vcp_Value * valrec = any_vcp_value_to_single_vcp_value(new_value);

   if (verified_value_loc) {
      Single_Vcp_Value * verified_single_value = NULL;
      rc = set_single_vcp_value(ddca_dh, valrec, &verified_single_value);
      if (verified_single_value) {
         DDCA_Any_Vcp_Value * verified_anyval =
               single_vcp_value_to_any_vcp_value(verified_single_value);
         free_single_vcp_value(verified_single_value);
         *verified_value_loc = verified_anyval;
      }
   }
   else {
      rc = set_single_vcp_value(ddca_dh, valrec, NULL);
   }

   free(valrec);     // do not free pointer to bytes, ref was copied when creating
   return rc;
}

DDCA_Status
ddca_set_any_vcp_value(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Any_Vcp_Value *    new_value)
{
   return ddca_set_any_vcp_value_verify(ddca_dh, feature_code, new_value, NULL);
}

//
// Monitor Capabilities
//

DDCA_Status
ddca_get_capabilities_string(
      DDCA_Display_Handle  ddca_dh,
      char**               pcaps)
{
   bool debug = false;
   Error_Info * ddc_excp = NULL;
   WITH_DH(ddca_dh,
      {
         char * p_cap_string = NULL;
         ddc_excp = get_capabilities_string(dh, &p_cap_string);
         psc = (ddc_excp) ? ddc_excp->status_code : 0;
         errinfo_free(ddc_excp);
         if (psc == 0) {
            // make copy to ensure caller does not muck around in ddcutil's
            // internal data structures
            *pcaps = strdup(p_cap_string);
            DBGMSF(debug, "*pcaps=%p", *pcaps);
         }
      }
   );
}


DDCA_Status
ddca_parse_capabilities_string(
      char *                   capabilities_string,
      DDCA_Capabilities **     p_parsed_capabilities)
{
   bool debug = false;
   DBGMSF(debug, "Starting. capabilities_string: |%s|", capabilities_string);
   DDCA_Status psc = DDCRC_OTHER;       // DDCL_BAD_DATA?
   DBGMSF(debug, "psc initialized to %s", psc_desc(psc));
   DDCA_Capabilities * result = NULL;

   // need to control messages?
   Parsed_Capabilities * pcaps = parse_capabilities_string(capabilities_string);
   if (pcaps) {
      if (debug) {
         DBGMSG("Parsing succeeded. ");
         report_parsed_capabilities(pcaps, NULL, 0);
         DBGMSG("Convert to DDCA_Capabilities...");
      }
      result = calloc(1, sizeof(DDCA_Capabilities));
      memcpy(result->marker, DDCA_CAPABILITIES_MARKER, 4);
      result->unparsed_string= strdup(capabilities_string);     // needed?
      result->version_spec = pcaps->parsed_mccs_version;
      Byte_Value_Array bva = pcaps->commands;
      if (bva) {
         result->cmd_ct = bva_length(bva);
         result->cmd_codes = malloc(result->cmd_ct);
         memcpy(result->cmd_codes, bva_bytes(bva), result->cmd_ct);
      }

      // n. needen't set vcp_code_ct if !pcaps, calloc() has done it
      if (pcaps->vcp_features) {
         result->vcp_code_ct = pcaps->vcp_features->len;
         result->vcp_codes = calloc(result->vcp_code_ct, sizeof(DDCA_Cap_Vcp));
         // DBGMSF(debug, "allocate %d bytes at %p", result->vcp_code_ct * sizeof(DDCA_Cap_Vcp), result->vcp_codes);
         for (int ndx = 0; ndx < result->vcp_code_ct; ndx++) {
            DDCA_Cap_Vcp * cur_cap_vcp = &result->vcp_codes[ndx];
            // DBGMSF(debug, "cur_cap_vcp = %p", &result->vcp_codes[ndx]);
            memcpy(cur_cap_vcp->marker, DDCA_CAP_VCP_MARKER, 4);
            Capabilities_Feature_Record * cur_cfr = g_ptr_array_index(pcaps->vcp_features, ndx);
            // DBGMSF(debug, "Capabilities_Feature_Record * cur_cfr = %p", cur_cfr);
            assert(memcmp(cur_cfr->marker, CAPABILITIES_FEATURE_MARKER, 4) == 0);
            // if (debug)
            //    show_capabilities_feature(cur_cfr, result->version_spec);
            cur_cap_vcp->feature_code = cur_cfr->feature_id;
            // DBGMSF(debug, "cur_cfr = %p, feature_code - 0x%02x", cur_cfr, cur_cfr->feature_id);

            // cur_cap_vcp->raw_values = strdup(cur_cfr->value_string);
            // TODO: get values from Byte_Bit_Flags cur_cfr->bbflags
#ifdef OLD_BVA
            Byte_Value_Array bva = cur_cfr->values;
            if (bva) {
               cur_cap_vcp->value_ct = bva_length(bva);
               cur_cap_vcp->values = bva_bytes(bva);     // makes copy of bytes
            }
#endif
            if (cur_cfr->bbflags) {
               cur_cap_vcp->value_ct = bbf_count_set(cur_cfr->bbflags);
               cur_cap_vcp->values   = calloc(1, cur_cap_vcp->value_ct);
               bbf_to_bytes(cur_cfr->bbflags, cur_cap_vcp->values, cur_cap_vcp->value_ct);
            }
         }
      }
      psc = 0;
      free_parsed_capabilities(pcaps);
   }

   *p_parsed_capabilities = result;
   DBGMSF(debug, "Done. Returning: %d", psc);
   return psc;
}


void
ddca_free_parsed_capabilities(
      DDCA_Capabilities * pcaps)
{
   bool debug = false;
   if (pcaps) {
      assert(memcmp(pcaps->marker, DDCA_CAPABILITIES_MARKER, 4) == 0);
      free(pcaps->unparsed_string);

      DBGMSF(debug, "vcp_code_ct = %d", pcaps->vcp_code_ct);
      for (int ndx = 0; ndx < pcaps->vcp_code_ct; ndx++) {
         DDCA_Cap_Vcp * cur_vcp = &pcaps->vcp_codes[ndx];
         assert(memcmp(cur_vcp->marker, DDCA_CAP_VCP_MARKER, 4) == 0);
         free(cur_vcp->values);
         cur_vcp->marker[3] = 'x';
      }

      pcaps->marker[3] = 'x';
      free(pcaps);
   }
}


void
ddca_report_parsed_capabilities(
      DDCA_Capabilities *      p_caps,
      int                      depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting");
   assert(p_caps && memcmp(p_caps->marker, DDCA_CAPABILITIES_MARKER, 4) == 0);
   // int d0 = depth;
   // quick hack since d0 no longer used
   int d1 = depth;
   int d2 = depth+1;
   int d3 = depth+2;
   int d4 = depth+3;

   DDCA_Output_Level ol = get_output_level();

   // rpt_structure_loc("DDCA_Capabilities", pcaps, depth);
   // rpt_label(  d0, "Capabilities:");
   if (ol >= DDCA_OL_VERBOSE)
      rpt_vstring(d1, "Unparsed string: %s", p_caps->unparsed_string);
   rpt_vstring(d1, "VCP version:     %d.%d", p_caps->version_spec.major, p_caps->version_spec.minor);
   if (ol >= DDCA_OL_VERBOSE) {
      rpt_label  (d1, "Command codes: ");
      for (int cmd_ndx = 0; cmd_ndx < p_caps->cmd_ct; cmd_ndx++) {
         uint8_t cur_code = p_caps->cmd_codes[cmd_ndx];
         char * cmd_name = ddc_cmd_code_name(cur_code);
         rpt_vstring(d2, "0x%02x (%s)", cur_code, cmd_name);
      }
   }
   rpt_vstring(d1, "VCP Feature codes:");
   for (int code_ndx = 0; code_ndx < p_caps->vcp_code_ct; code_ndx++) {
      DDCA_Cap_Vcp * cur_vcp = &p_caps->vcp_codes[code_ndx];
      assert( memcmp(cur_vcp->marker, DDCA_CAP_VCP_MARKER, 4) == 0);

      char * feature_name = get_feature_name_by_id_and_vcp_version(
                               cur_vcp->feature_code,
                               p_caps->version_spec);

      rpt_vstring(d2, "Feature:  0x%02x (%s)", cur_vcp->feature_code, feature_name);

      DDCA_Feature_Value_Entry * feature_value_table;
      DDCA_Status ddcrc = ddca_get_simple_sl_value_table_by_vspec(
            cur_vcp->feature_code,
            p_caps->version_spec,
            NULL,
            &feature_value_table);

      if (cur_vcp->value_ct > 0) {
         if (ol > DDCA_OL_VERBOSE)
            rpt_vstring(d3, "Unparsed values:     %s", hexstring_t(cur_vcp->values, cur_vcp->value_ct) );
         rpt_label(d3, "Values:");
         for (int ndx = 0; ndx < cur_vcp->value_ct; ndx++) {
            char * value_desc = "No lookup table";
            if (ddcrc == 0) {
               value_desc = "Unrecognized feature value";
               ddca_get_simple_nc_feature_value_name_by_table(
                          feature_value_table,
                          cur_vcp->values[ndx],
                          &value_desc);
            }
            rpt_vstring(d4, "0x%02x: %s", cur_vcp->values[ndx], value_desc);
         }
      }
   }
}


void
ddca_parse_and_report_capabilities(
      char *                    capabilities_string,
      DDCA_Monitor_Model_Key *  mmid,
      int                       depth)
{
      Parsed_Capabilities* pcaps = parse_capabilities_string(capabilities_string);
      report_parsed_capabilities(pcaps, mmid, 0);
      free_parsed_capabilities(pcaps);
}


DDCA_Feature_List
ddca_feature_list_from_capabilities(
      DDCA_Capabilities * parsed_caps)
{
   DDCA_Feature_List result = {{0}};
   for (int ndx = 0; ndx < parsed_caps->vcp_code_ct; ndx++) {
      DDCA_Cap_Vcp curVcp = parsed_caps->vcp_codes[ndx];
      ddca_feature_list_add(&result, curVcp.feature_code);
   }
   return result;
}




DDCA_Status
ddca_get_profile_related_values(
      DDCA_Display_Handle ddca_dh,
      char**              profile_values_string_loc)
{
   WITH_DH(ddca_dh,
      {
         bool debug = false;
         DBGMSF(debug, "Before dumpvcp_to_string_by_display_handle(), pprofile_values_string=%p,"
                       " *profile_values_string_loc=%p",
               profile_values_string_loc, *profile_values_string_loc);
         psc = dumpvcp_as_string(dh, profile_values_string_loc);
         DBGMSF(debug, "After dumpvcp_to_string_by_display_handle(), pprofile_values_string=%p,"
                       " *profile_values_string_loc=%p",
               profile_values_string_loc, *profile_values_string_loc);
         DBGMSF(debug, "*profile_values_string_loc = |%s|", *profile_values_string_loc);
      }
   );
}


DDCA_Status
ddca_set_profile_related_values(
      DDCA_Display_Handle  ddca_dh,
      char * profile_values_string)
{
   WITH_DH(ddca_dh,
      {
         free_thread_error_detail();
         Error_Info * ddc_excp = loadvcp_by_string(profile_values_string, dh);
         psc = (ddc_excp) ? ddc_excp->status_code : 0;
         if (ddc_excp) {
            save_thread_error_detail(error_info_to_ddca_detail(ddc_excp));
            errinfo_free(ddc_excp);
         }
      }
   );
}


//
// Reports
//

int
ddca_report_active_displays(int depth) {
   return ddc_report_displays(false, depth);
}

int
ddca_report_displays(bool include_invalid_displays, int depth) {
   return ddc_report_displays(include_invalid_displays, depth);
}


//
//  Dynamic Features (future)
//

DDCA_Status
ddca_dfr_check_by_dref(DDCA_Display_Ref * ddca_dref) {

   WITH_DR(ddca_dref,
      {
         free_thread_error_detail();
         Error_Info * ddc_excp = dfr_check_by_dref(dref);
         if (ddc_excp) {
            psc = ddc_excp->status_code;
            save_thread_error_detail(error_info_to_ddca_detail(ddc_excp));
            errinfo_free(ddc_excp);
         }
      }
   );
}

//
// Async operation - experimental
//

DDCA_Status
ddca_start_get_any_vcp_value(
      DDCA_Display_Handle         ddca_dh,
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Vcp_Value_Type         call_type,
      DDCA_Notification_Func      callback_func)
{
   bool debug = false;
   DBGMSF(debug, "Starting. ddca_dh=%p, feature_code=0x%02x, call_type=%d",
                 ddca_dh, feature_code, call_type);
   // DDCA_Status rc = DDCRC_ARG;
   Error_Info * ddc_excp = NULL;

   WITH_DH(ddca_dh,
       {
          ddc_excp = start_get_vcp_value(dh, feature_code, call_type, callback_func);
          psc = (ddc_excp) ? ddc_excp->status_code : 0;
          errinfo_free(ddc_excp);
       }
      );
}


DDCA_Status
ddca_register_callback(
      DDCA_Notification_Func func,
      uint8_t                callback_options) // type is a placeholder
{
   return DDCRC_UNIMPLEMENTED;
}

DDCA_Status
ddca_queue_get_non_table_vcp_value(
      DDCA_Display_Handle      ddca_dh,
      DDCA_Vcp_Feature_Code    feature_code)
{
   return DDCRC_UNIMPLEMENTED;
}


// CFFI
DDCA_Status
ddca_pass_callback(
      Simple_Callback_Func  func,
      int                   parm)
{
   DBGMSG("parm=%d", parm);
   int callback_rc = func(parm+2);
   DBGMSG("returning %d", callback_rc);
   return callback_rc;
}

