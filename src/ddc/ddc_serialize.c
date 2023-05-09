// ddc_serialize.c

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <jansson.h>
#include <stdbool.h>

#include "public/ddcutil_types.h"
#include "util/string_util.h"
#include "base/core.h"
#include "base/displays.h"
#include "base/i2c_bus_base.h"
#include "base/monitor_model_key.h"
#include "ddc/ddc_displays.h"

#include "ddc_serialize.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_NONE;

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
   json_t* jpath = json_object();

   char edid_bytes[257];
         hexstring2(
                   pedid->bytes,    // bytes to convert
                   128,             // number of bytes
                   "",              // separator string between hex digits
                   true,            // use upper case hex characters
                   edid_bytes,      // buffer in which to return hex string
                   257) ;           // buffer size
   DBGMSG("edid_bytes=%s", edid_bytes);

   json_t* jpath_raw = json_string(edid_bytes);
   json_object_set_new(jpath, "bytes", jpath_raw);

   json_t* jnode = json_string(pedid->edid_source);
   json_object_set_new(jpath, "edid_source", jnode);

   return jpath;
}


json_t * serialize_mmk(DDCA_Monitor_Model_Key * mmk) {
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
   DBGMSG("Before serialization:");
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

   DBGMSG("capabilities_string: %s", dref->capabilities_string);
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

         jtmp = json_object_get(jpath, "edid_source");
         const char * edid_source = json_string_value(jtmp);
         if (ct == 128) {
            parsed_edid = create_parsed_edid2(hbytes, g_strdup(edid_source));
         }
      }
   }
   if (parsed_edid && debug)
      report_parsed_edid(parsed_edid, true, 1);
   return parsed_edid;
}


DDCA_Monitor_Model_Key * deserialize_mmid(json_t* jpath) {
   bool debug = false;
   const char * mfg_id = json_string_value( json_object_get(jpath, "mfg_id"));
   const char * model_name = json_string_value( json_object_get(jpath, "model_name"));
   int          product_code = json_integer_value( json_object_get(jpath, "product_code"));
   DDCA_Monitor_Model_Key*  mmk = monitor_model_key_new(mfg_id, model_name, product_code);

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

#ifdef REF
#define I2C_BUS_INFO_MARKER "BINF"
/** Information about one I2C bus */
typedef
struct {
   char             marker[4];          ///< always "BINF"
   int              busno;              ///< I2C device number, i.e. N for /dev/i2c-N
   unsigned long    functionality;      ///< i2c bus functionality flags
   Parsed_Edid *    edid;               ///< parsed EDID, if slave address x50 active
   uint16_t         flags;              ///< I2C_BUS_* flags
   char *           driver;             ///< driver name
   int              open_errno;         ///< errno if open fails (!I2C_BUS_ACCESSIBLE)
   char *           drm_connector_name; ///< from /sys
   Drm_Connector_Found_By
                    drm_connector_found_by;
} I2C_Bus_Info;
#endif

json_t* serialize_one_i2c_bus(I2C_Bus_Info * businfo) {
   bool debug = true;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "Before serialization:");
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

   return jbus;
}

I2C_Bus_Info * deserialize_one_i2c_bus(json_t* jbus) {
   bool debug = true;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");
   json_t* jtmp = NULL;
   json_t* busno_node = json_object_get(jbus, "busno");
   if (!(busno_node && json_is_integer(busno_node))) {
      if (busno_node)
         fprintf(stderr, "error: busno is not an integer\n");
      else
         fprintf(stderr, "member busno not found");
      json_decref(jbus);
      return NULL;
   }
   int busno = json_integer_value(busno_node);

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

typedef enum {
   serialize_mode_display,
   serialize_mode_bus
} Serialize_Mode;


char * ddc_serialize_displays_and_buses() {
   bool debug = true;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");

   json_t* root = json_object();
   json_object_set_new(root, "version", json_integer(1));

   GPtrArray* all_displays = ddc_get_all_displays();
   json_t* jdisplays = json_array();

   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      json_t* node = serialize_one_display(g_ptr_array_index(all_displays, ndx));
      json_array_append(jdisplays, node);
   }
   json_object_set_new(root, "all_displays", jdisplays);

   GPtrArray* all_buses = i2c_get_all_buses();
   json_t* jbuses = json_array();

   for (int ndx = 0; ndx < all_buses->len; ndx++) {
      json_t* node = serialize_one_i2c_bus(g_ptr_array_index(all_buses, ndx));
      json_array_append(jbuses, node);
   }
   json_object_set_new(root, "all_buses", jbuses);

   char * result = json_dumps(root, JSON_INDENT(3));

   if (debug || IS_TRACING() ) {
      DBGMSG("Returning:");
      DBGMSG(result);
   }
   return result;
}




GPtrArray * ddc_deserialize_displays_or_buses(const char * jstring, Serialize_Mode mode) {
   bool debug = false;
   GPtrArray * restored = g_ptr_array_new();

   json_error_t error;

   json_t* root = json_loads(jstring, 0, &error);
   if (!root) {
          fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
          return NULL;
      }
   if(!json_is_object(root))
   {
       fprintf(stderr, "error: root is not an object\n");
       json_decref(root);
       return NULL;
   }
   json_t* version_node = json_object_get(root, "version");
   if (!(version_node && json_is_integer(version_node))) {
      if (version_node)
         fprintf(stderr, "error: version is not an integer\n");
      else
         fprintf(stderr, "member version not found");
      json_decref(root);
      return NULL;
   }
   int version = json_integer_value(version_node);
   DBGMSF(debug, "version = %d", version);
   assert(version == 1);

   char * all = "all_displays";
   if (mode == serialize_mode_bus)
      all = "all_buses";

   json_t* disp_nodes = json_object_get(root, all);
   if (!(disp_nodes && json_is_array(disp_nodes))) {
      if (disp_nodes)
         DBGMSG("error: %s is not an array", all);
      else
         DBGMSG("member %s not found", all);
      json_decref(root);
      return NULL;
   }

   for (int dispctr = 0; dispctr < json_array_size(disp_nodes); dispctr++) {
      json_t* one_display_or_bus = json_array_get(disp_nodes, dispctr);
      if (! (one_display_or_bus && json_is_object(one_display_or_bus))) {
         if (one_display_or_bus)
            DBGMSG("%s[%d] not found", all, dispctr);
         else
            DBGMSG("%s[%d] is not an object", all, dispctr);
         json_decref(root);
         return NULL;
      }

      if (mode == serialize_mode_display) {
         Display_Ref * dref = deserialize_one_display(one_display_or_bus);
         g_ptr_array_add(restored, dref);
      }
      else {
         I2C_Bus_Info * businfo = deserialize_one_i2c_bus(one_display_or_bus);
         g_ptr_array_add(restored, businfo);
      }
   }

   return restored;
}


GPtrArray * ddc_deserialize_displays(const char * jstring) {
   return ddc_deserialize_displays_or_buses(jstring, serialize_mode_display);
}


GPtrArray * ddc_deserialize_buses(const char * jstring) {
   return ddc_deserialize_displays_or_buses(jstring, serialize_mode_bus);
}


