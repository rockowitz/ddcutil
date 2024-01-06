/** @file ddc_serialize.c */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <jansson.h>
#include <stdbool.h>
#include <stdio.h>

#include "public/ddcutil_types.h"

#include "util/file_util.h"
#include "util/string_util.h"
#include "util/xdg_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/i2c_bus_base.h"
#include "base/monitor_model_key.h"
#include "base/rtti.h"

#include "ddc/ddc_displays.h"

#include "ddc_serialize.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_NONE;

bool display_caching_enabled = false;

GPtrArray* deserialized_displays = NULL;    // array of Display_Ref *
GPtrArray* deserialized_buses    = NULL;    // array of Display_Ref *


// #define CACHE_BUS_INFO   // not used

Display_Ref * ddc_find_deserialized_display(int busno, Byte* edidbytes) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_DDCIO, "busno = %d", busno);
   Display_Ref * result = NULL;
   if (deserialized_displays) {
      for (int ndx = 0; ndx < deserialized_displays->len; ndx++) {
         Display_Ref * cur = g_ptr_array_index(deserialized_displays, ndx);
         if (cur->io_path.io_mode == DDCA_IO_I2C    &&
             cur->io_path.path.i2c_busno == busno   &&
             cur->pedid                             &&
             memcmp(cur->pedid->bytes, edidbytes, 128) == 0 )
         {
            result = cur;
            break;
         }
      }
   }
   if (result)
      DBGTRC_RET_STRUCT(debug, DDCA_TRC_DDCIO, Display_Ref, dbgrpt_display_ref, result);
   else
      DBGTRC_DONE(debug, DDCA_TRC_DDCIO, "Not found. Returning NULL");
   return result;
}


void ddc_enable_displays_cache(bool onoff) {
   bool debug = false;
   display_caching_enabled = onoff;
   DBGMSF(debug, "Executed. onoff=%s", sbool(onoff));
}


json_t* serialize_dpath(DDCA_IO_Path iopath) {
   json_t* jpath = json_object();
   json_t* jpath_mode = json_integer(iopath.io_mode);
   json_t* jpath_busno = json_integer(iopath.path.i2c_busno);
   json_object_set_new(jpath, "io_mode", jpath_mode);
   json_object_set_new(jpath, "busno_or_hiddev", jpath_busno);
   return jpath;
}


json_t * serialize_vspec(DDCA_MCCS_Version_Spec vspec) {
   json_t* jpath = json_object();
   json_t* jpath_mode = json_integer(vspec.major);
   json_t* jpath_busno = json_integer(vspec.minor);
   json_object_set_new(jpath, "major", jpath_mode);
   json_object_set_new(jpath, "minor", jpath_busno);
   return jpath;
}


json_t * serialize_parsed_edid(Parsed_Edid * pedid) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "pedid=%p", pedid);
   json_t* jpath = json_object();

   char edid_bytes[257];
   hexstring2(
             pedid->bytes,    // bytes to convert
             128,             // number of bytes
             "",              // separator string between hex digits
             true,            // use upper case hex characters
             edid_bytes,      // buffer in which to return hex string
             257) ;           // buffer size
   DBGMSF(debug, "edid_bytes=%s", edid_bytes);

   json_t* jpath_raw = json_string(edid_bytes);
   json_object_set_new(jpath, "bytes", jpath_raw);

   json_t* jnode = json_string(pedid->edid_source);
   json_object_set_new(jpath, "edid_source", jnode);

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning %p", jpath);
   return jpath;
}


json_t * serialize_mmk(Monitor_Model_Key * mmk) {
   json_t* jpath = json_object();

   json_t* jmfg = json_string(mmk->mfg_id);
   json_object_set_new(jpath, "mfg_id", jmfg);

   json_t* jmodel = json_string(mmk->model_name);
   json_object_set_new(jpath, "model_name", jmodel);

   json_t* jproduct = json_integer(mmk->product_code);
   json_object_set_new(jpath, "product_code", jproduct);

   return jpath;
}


json_t* serialize_one_display(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_DDCIO, "dref=%s", dref_repr_t(dref));
   if (debug)
      dbgrpt_display_ref(dref, 2);

   json_t * jtmp = NULL;
   json_t * jdisp = json_object();

   json_object_set_new(jdisp, "io_path", serialize_dpath(dref->io_path));
   json_object_set_new(jdisp, "usb_bus", json_integer(dref->usb_bus));
   json_object_set_new(jdisp, "usb_device", json_integer(dref->usb_device));

   json_t* jnode;
   if (dref->usb_hiddev_name) {
      jnode = json_string(dref->usb_hiddev_name);
      json_object_set_new(jdisp, "usb_hiddev_name", jnode);
   }

   json_object_set_new(jdisp, "vcp_version_xdf", serialize_vspec(dref->vcp_version_xdf));
   json_object_set_new(jdisp, "vcp_version_cmdline", serialize_vspec(dref->vcp_version_cmdline));
   json_object_set_new(jdisp, "flags", json_integer(dref->flags));

   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "capabilities_string: %s", dref->capabilities_string);
   if (dref->capabilities_string) {
      jnode = json_string(dref->capabilities_string);
      json_object_set_new(jdisp, "capabilities_string", jnode);
   }

   jtmp = serialize_parsed_edid(dref->pedid);
   json_object_set_new(jdisp, "pedid", jtmp);

   json_object_set_new(jdisp, "dispno", json_integer(dref->dispno));

   json_object_set_new(jdisp, "mmid", serialize_mmk(dref->mmid));

   if (dref->dispno == -2) {
      jtmp = serialize_dpath(dref->actual_display->io_path);
      json_object_set_new(jdisp, "actual_display_path", jtmp);
   }

   if (dref->driver_name) {
      json_object_set_new(jdisp, "driver_name", json_string(dref->driver_name));
   }

   // json_decref(jdisp);

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning %p", jdisp);
   return jdisp;
}


DDCA_IO_Path deserialize_dpath(json_t* jpath) {
   bool debug = false;
   json_t* jtmp = json_object_get(jpath, "io_mode");
   DDCA_IO_Mode mode = json_integer_value(jtmp);
   jtmp = json_object_get(jpath, "busno_or_hiddev");
   int busno = json_integer_value(jtmp);
   DDCA_IO_Path dpath;
   dpath.io_mode = mode;
   dpath.path.i2c_busno = busno;
   DBGMSF(debug, "Returning: %s", dpath_repr_t(&dpath));
   return dpath;
}


DDCA_MCCS_Version_Spec deserialize_vspec(json_t* jpath) {
   DDCA_MCCS_Version_Spec vspec = {0,0};
   json_t* jtmp = json_object_get(jpath, "major");
   vspec.major = json_integer_value(jtmp);
   jtmp = json_object_get(jpath, "minor");
   vspec.minor = json_integer_value(jtmp);
   // DBGMSG("vspec=%d.%d", vspec.major, vspec.minor);
   return vspec;
}


Parsed_Edid * deserialize_parsed_edid(json_t* jpath) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   Parsed_Edid * parsed_edid = NULL;
   json_t* jtmp = json_object_get(jpath, "bytes");
   if (!jtmp) {
      DBGMSF(debug, "bytes not found");
   }
   else {
      const char * sbytes = json_string_value(jtmp);
      if (!sbytes) {
         DBGMSF(debug, "Unable to read sbytes");
      }
      else {
         assert(strlen(sbytes) == 256);
         Byte * hbytes;
         int ct = hhs_to_byte_array(sbytes,  &hbytes);
         assert (ct == 128);

         json_t* jtmp1 = json_object_get(jpath, "edid_source");
         const char * edid_source = json_string_value(jtmp1);
         // json_decref(jtmp1);
         if (ct == 128) {
            parsed_edid = create_parsed_edid2(hbytes, edid_source);
         }
         free(hbytes);
      }
      // json_decref(jtmp);
   }
   if (parsed_edid && debug)
      report_parsed_edid(parsed_edid, true, 1);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning parsed_edid=%p", parsed_edid);
   return parsed_edid;
}


Monitor_Model_Key * deserialize_mmid(json_t* jpath) {
   bool debug = false;
   const char * mfg_id = json_string_value( json_object_get(jpath, "mfg_id"));
   const char * model_name = json_string_value( json_object_get(jpath, "model_name"));
   int          product_code = json_integer_value( json_object_get(jpath, "product_code"));
   Monitor_Model_Key*  mmk = monitor_model_key_new(mfg_id, model_name, product_code);

   DBGMSF(debug, "Executed. Returning: %s", mmk_repr(*mmk) );
   return mmk;
}


Display_Ref *  deserialize_one_display(json_t* disp_node) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");
   json_t* jtmp = NULL;

   jtmp = json_object_get(disp_node, "io_path");
   DDCA_IO_Path io_path = deserialize_dpath(jtmp);

   Display_Ref * dref = create_base_display_ref(io_path);

   jtmp = json_object_get(disp_node, "usb_bus");
   dref->usb_bus = json_integer_value(jtmp);

   jtmp = json_object_get(disp_node, "usb_device");
   dref->usb_device = json_integer_value(jtmp);

   jtmp = json_object_get(disp_node, "usb_hiddev_name");
   dref->usb_hiddev_name = NULL;
   if (jtmp) {
      dref->usb_hiddev_name = g_strdup(json_string_value(jtmp));
   }

   jtmp = json_object_get(disp_node, "vcp_version_xdf");
   dref->vcp_version_xdf = deserialize_vspec(jtmp);

   jtmp = json_object_get(disp_node, "vcp_version_cmdline");
   dref->vcp_version_cmdline = deserialize_vspec(jtmp);

   jtmp = json_object_get(disp_node, "flags");
   dref->flags = json_integer_value(jtmp);

   jtmp = json_object_get(disp_node, "capabilities_string");
   dref->capabilities_string = NULL;
   if (jtmp) {
      dref->capabilities_string = g_strdup(json_string_value(jtmp));
   }

   jtmp = json_object_get(disp_node, "pedid");
   dref->pedid = deserialize_parsed_edid(jtmp);

   jtmp = json_object_get(disp_node, "mmid");
   dref->mmid = deserialize_mmid(jtmp);

   jtmp = json_object_get(disp_node, "dispno");
   dref->dispno = json_integer_value(jtmp);

   jtmp = json_object_get(disp_node, "actual_display_path");
   if (jtmp) {
      DDCA_IO_Path actual_display_path = deserialize_dpath(jtmp);
      dref->actual_display_path = calloc(1, sizeof(DDCA_IO_Path));
      memcpy(dref->actual_display_path, &actual_display_path, sizeof(DDCA_IO_Path));
   }

   jtmp = json_object_get(disp_node, "driver_name");
   if (jtmp) {
      dref->driver_name = g_strdup(json_string_value(jtmp));
   }

   DBGTRC_RET_STRUCT(debug, DDCA_TRC_NONE, Display_Ref, dbgrpt_display_ref, dref);
   return dref;
}


#ifdef CACHE_BUS_INFO
json_t* serialize_one_i2c_bus(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d, Before serialization:", businfo->busno);
   if (IS_TRACING() || debug)
      i2c_dbgrpt_bus_info(businfo, 2);

   json_t * jtmp = NULL;
   json_t * jbus = json_object();

   json_object_set_new(jbus, "busno", json_integer(businfo->busno));
   json_object_set_new(jbus, "functionality", json_integer(businfo->busno));
   if (businfo->edid) {
      jtmp = serialize_parsed_edid(businfo->edid);
      json_object_set_new(jbus, "edid", jtmp);
   }
   json_object_set_new(jbus, "flags", json_integer(businfo->flags));
   if (businfo->driver)
      json_object_set_new(jbus, "driver", json_string(businfo->driver));
   if (businfo->drm_connector_name)
      json_object_set_new(jbus, "drm_connector_name", json_string(businfo->drm_connector_name));
   json_object_set_new(jbus, "drm_connector_found_by", json_integer(businfo->drm_connector_found_by));

   // if (jtmp)
   //    json_decref(jtmp);
   // json_decref(jbus);
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning %p", jbus);
   return jbus;
}


I2C_Bus_Info * deserialize_one_i2c_bus(json_t* jbus) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "jbus=%p", jbus);
   json_t* jtmp = NULL;
   json_t* busno_node = json_object_get(jbus, "busno");
   if (!(busno_node && json_is_integer(busno_node))) {
      if (busno_node)
         SEVEREMSG("error: busno is not an integer\n");
      else
         SEVEREMSG("member busno not found");
      // json_decref(jbus);
      return NULL;
   }
   int busno = json_integer_value(busno_node);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d", busno);

   I2C_Bus_Info * businfo = i2c_new_bus_info(busno);

   businfo->functionality = json_integer_value(json_object_get(jbus, "functionality"));

   jtmp = json_object_get(jbus, "edid");
   if (jtmp)
      businfo->edid = deserialize_parsed_edid(jtmp);

   businfo->flags = json_integer_value(json_object_get(jbus, "flags"));

   jtmp = json_object_get(jbus, "driver");
   if (jtmp)
      businfo->driver = g_strdup(json_string_value(jtmp));

   jtmp = json_object_get(jbus, "drm_connector_name");
    if (jtmp)
       businfo->drm_connector_name = g_strdup(json_string_value(jtmp));

   businfo->drm_connector_found_by = json_integer_value(json_object_get(jbus, "drm_connector_found_by"));

   DBGTRC_RET_STRUCT(debug, DDCA_TRC_NONE, I2C_Bus_Info, i2c_dbgrpt_bus_info, businfo);
   return businfo;
}
#endif

typedef enum {
   serialize_mode_display,
   serialize_mode_bus
} Serialize_Mode;


const char * serialize_mode_name(Serialize_Mode mode) {
   char * result = (mode == serialize_mode_display) ? "serialize_mode_display"
                                                    : "serialize_mode_bus";
   return result;
}


char * ddc_serialize_displays_and_buses() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");

   json_t* root = json_object();
   json_object_set_new(root, "version", json_integer(1));

   GPtrArray* all_displays = ddc_get_all_display_refs();
   json_t* jdisplays = json_array();

   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
      if (dref->flags & DREF_DDC_COMMUNICATION_WORKING) {
         json_t* node = serialize_one_display(dref);
         json_array_append(jdisplays, node);
         json_decref(node);
      }
   }
   json_object_set_new(root, "all_displays", jdisplays);

#ifdef CACHE_BUS_INFO
   GPtrArray* all_buses = i2c_get_all_buses();
   json_t* jbuses = json_array();

   for (int ndx = 0; ndx < all_buses->len; ndx++) {
      json_t* node = serialize_one_i2c_bus(g_ptr_array_index(all_buses, ndx));
      json_array_append(jbuses, node);
      json_decref(node);
   }
   json_object_set_new(root, "all_buses", jbuses);
#endif
   char * result = json_dumps(root, JSON_INDENT(3));

   DBGTRC_RETURNING(debug, TRACE_GROUP, result, "");
   json_decref(root);
   return result;
}


GPtrArray * ddc_deserialize_displays_or_buses(const char * jstring, Serialize_Mode mode) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_DDCIO, "mode=%s, jstring:", serialize_mode_name(mode));
   DBGTRC_NOPREFIX(debug, DDCA_TRC_DDCIO, "%s", jstring);
   GPtrArray * restored = g_ptr_array_new();

#ifndef CACHE_BUS_INFO
   assert(mode == serialize_mode_display);
#endif

   json_error_t error;

   bool ok = true;
   json_t* root = json_loads(jstring, 0, &error);
   if (!root) {
          SEVEREMSG( "error: on line %d: %s\n", error.line, error.text);
          ok = false;
          goto bye;
      }
   if(!json_is_object(root))
   {
       SEVEREMSG( "error: root is not an object\n");
       // json_decref(root);
       ok = false;
       goto bye;
   }
   json_t* version_node = json_object_get(root, "version");
   if (!(version_node && json_is_integer(version_node))) {
      if (version_node)
         SEVEREMSG("error: version is not an integer\n");
      else
         SEVEREMSG("member version not found");
      // json_decref(root);
      ok = false;
      goto bye;
   }
   int version = json_integer_value(version_node);
   DBGMSF(debug, "version = %d", version);
   assert(version == 1);

   char * all = "all_displays";
#ifdef CACHE_BUS_INFO
   if (mode == serialize_mode_bus)
      all = "all_buses";
#endif

   json_t* disp_nodes = json_object_get(root, all);
   if (!(disp_nodes && json_is_array(disp_nodes))) {
      if (disp_nodes)
         SEVEREMSG("error: %s is not an array", all);
      else
         SEVEREMSG("member %s not found", all);
      // json_decref(root);
      ok = false;
      goto bye;
   }

   for (int dispctr = 0; dispctr < json_array_size(disp_nodes); dispctr++) {
      json_t* one_display_or_bus = json_array_get(disp_nodes, dispctr);
      if (! (one_display_or_bus && json_is_object(one_display_or_bus))) {
         if (one_display_or_bus)
            SEVEREMSG("%s[%d] not found", all, dispctr);
         else
            SEVEREMSG("%s[%d] is not an object", all, dispctr);
         // json_decref(root);
         ok = false;
         goto bye;
      }

#ifdef CACHE_BUS_INFO
      if (mode == serialize_mode_display) {
         Display_Ref * dref = deserialize_one_display(one_display_or_bus);
         g_ptr_array_add(restored, dref);
      }
      else {
         I2C_Bus_Info * businfo = deserialize_one_i2c_bus(one_display_or_bus);
         g_ptr_array_add(restored, businfo);
      }
#else
      Display_Ref * dref = deserialize_one_display(one_display_or_bus);
      g_ptr_array_add(restored, dref);
#endif
   }

bye:
   if (root)
      json_decref(root);

   if (!ok) {
      g_ptr_array_remove_range (restored, 0, restored->len);
   }

   DBGTRC_DONE(debug, DDCA_TRC_DDCIO, "Restored %d records.", restored->len);
   return restored;
}

#ifdef OUT
GPtrArray * ddc_deserialize_displays(const char * jstring) {
   DBGMSG("jstring: |%s|", jstring);
   return ddc_deserialize_displays_or_buses(jstring, serialize_mode_display);
}


GPtrArray * ddc_deserialize_buses(const char * jstring) {
   DBGMSG("jstring: |%s|", jstring);
   return ddc_deserialize_displays_or_buses(jstring, serialize_mode_bus);
}
#endif


/** Returns the name of the file that stores persistent display information
 *
 *  \return name of file, normally $HOME/.cache/ddcutil/displays
 */
/* caller is responsible for freeing returned value */
char * ddc_displays_cache_file_name() {
   return xdg_cache_home_file("ddcutil", DISPLAYS_CACHE_FILENAME);
}


bool ddc_store_displays_cache() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_DDCIO, "Starting");
   bool ok = false;
   if (ddc_displays_already_detected()) {
      char * json_text = ddc_serialize_displays_and_buses();
      char * fn = ddc_displays_cache_file_name();
      if (!fn) {
         SEVEREMSG("Unable to determine cisplay cache file name");
         SYSLOG2(DDCA_SYSLOG_ERROR, "Unable to determine display cache file name");
      }
      else {
         FILE * fp = NULL;
         fopen_mkdir(fn, "w", ferr(), &fp );
         if (!fp) {
            SEVEREMSG("Error opening file %s:%s", fn, strerror(errno));
            SYSLOG2(DDCA_SYSLOG_ERROR, "Error opening file %s:%s", fn, strerror(errno));
         }
         else {
            size_t bytes_written = fwrite(json_text, strlen(json_text), 1, fp);
            if (bytes_written < strlen(json_text)) {
               SEVEREMSG("Error writing file %s:%s", fn, strerror(errno));
               SYSLOG2(DDCA_SYSLOG_ERROR, "Error writing file %s:%s", fn, strerror(errno));
            }
            else {
               ok = true;
            }
            fclose(fp);
         }
         free(json_text);
         free(fn);
      }
   }
   DBGTRC_RET_BOOL(debug, DDCA_TRC_DDCIO, ok, "");
   return ok;
}


void ddc_restore_displays_cache() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_DDCIO, "");
   char * fn = ddc_displays_cache_file_name();
   if (fn && regular_file_exists(fn)) {
      DBGMSF(debug, "Found file: %s", fn);
      char * buf = read_file_single_string(fn, debug);
      // DBGMSF(debug, "buf: |%s|", buf);
      deserialized_displays = ddc_deserialize_displays_or_buses(buf, serialize_mode_display);
#ifdef CACHE_BUS_INFO
      deserialized_buses    = ddc_deserialize_displays_or_buses(buf, serialize_mode_bus);
#endif
      free(buf);
   }
   else {
      DBGMSF(debug, "File not found: %s", fn);
#ifdef CACHE_BUS_INFO
      deserialized_buses    =  g_ptr_array_new();
#endif
      deserialized_displays =  g_ptr_array_new();
   }
   free(fn);
#ifdef CACHE_BUS_INFO
   DBGTRC_DONE(debug, DDCA_TRC_DDCIO, "Restored %d Display_Ref records, %d I2C_Bus_Info records",
         deserialized_displays->len, deserialized_buses->len);
#else
   DBGTRC_DONE(debug, DDCA_TRC_DDCIO, "Restored %d Display_Ref records",
         deserialized_displays->len);
   if ( IS_DBGTRC(debug, DDCA_TRC_DDCIO)) {
      for (int ndx = 0; ndx < deserialized_displays->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(deserialized_displays, ndx);
         DBGMSG(" Display_Ref: %s", dref_repr_t(dref));
      }
   }
#endif
}


void ddc_erase_displays_cache() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_DDCIO, "");
   bool found = false;
   char * fn = ddc_displays_cache_file_name();
   if (!fn) {
      MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "Failed to obtain cache file name");
   }
   else {
      found = regular_file_exists(fn);
      if (found) {
        int rc = remove(fn);
        if (rc < 0) {
           MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "Error removing file %s: %s", fn, strerror(errno));
        }
      }
   }
   DBGTRC_DONE(debug, DDCA_TRC_DDCIO, "%s: %s", (found) ? "Removed file" : "File not found", fn);
   free(fn);
}


void init_ddc_serialize() {
   RTTI_ADD_FUNC(ddc_deserialize_displays_or_buses);
   RTTI_ADD_FUNC(ddc_serialize_displays_and_buses);
   RTTI_ADD_FUNC(ddc_erase_displays_cache);
   RTTI_ADD_FUNC(ddc_restore_displays_cache);
   RTTI_ADD_FUNC(ddc_store_displays_cache);
   RTTI_ADD_FUNC(deserialize_one_display);
   RTTI_ADD_FUNC(deserialize_parsed_edid);
   RTTI_ADD_FUNC(serialize_one_display);
   RTTI_ADD_FUNC(ddc_find_deserialized_display);
}


void terminate_ddc_serialize() {
   bool debug = false;
   DBGMSF(debug, "Starting");
   if (deserialized_buses) {
      g_ptr_array_set_free_func(deserialized_buses, (GDestroyNotify) i2c_free_bus_info);
      g_ptr_array_free(deserialized_buses,   true);
      deserialized_buses    = NULL;
   }
   if (deserialized_displays) {
      g_ptr_array_set_free_func(deserialized_displays, (GDestroyNotify) free_display_ref);
      g_ptr_array_free(deserialized_displays, true);
      deserialized_displays = NULL;
   }
   DBGMSF(debug, "Done");
}
