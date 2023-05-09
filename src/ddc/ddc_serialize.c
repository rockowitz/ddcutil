// ddc_serialize.c

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <jansson.h>
// #include <json-glib-1.0/json-glib/json-glib.h>
#include <stdbool.h>

#include "public/ddcutil_types.h"
#include "util/string_util.h"
#include "base/core.h"
#include "base/displays.h"
#include "base/i2c_bus_base.h"
#include "base/monitor_model_key.h"
#include "ddc/ddc_displays.h"

#include "ddc_serialize.h"

#ifdef JSON_GLIB

char * serialize_bus_info(I2C_Bus_Info * info) {
   JsonBuilder* builder = json_builder_new();
   json_builder_begin_object(builder);
   json_builder_set_member_name(builder, "busno");                  json_builder_add_int_value(builder, info->busno);
   json_builder_set_member_name(builder, "functionality");          json_builder_add_int_value(builder, info->functionality);
   json_builder_set_member_name(builder, "driver");                 json_builder_add_string_value(builder, info->driver);
   json_builder_set_member_name(builder, "drm_connector_namer");    json_builder_add_string_value(builder, info->drm_connector_name);
   json_builder_set_member_name(builder, "drm_connector_found_by"); json_builder_add_int_value(builder, info->drm_connector_found_by);
   if (info->edid) {
   Byte checkbyte = info->edid->bytes[127];
   char* s = hexstring(&checkbyte, 1);
   json_builder_set_member_name(builder, "edid_checkbyte");         json_builder_add_string_value(builder, s);
   free(s);

   json_builder_set_member_name(builder, "edid_source");            json_builder_add_string_value(builder, info->edid->edid_source);
   }
   json_builder_end_object(builder);

   JsonGenerator* gen = json_generator_new();
   JsonNode * root = json_builder_get_root (builder);
   json_generator_set_root (gen, root);
   gchar *str = json_generator_to_data (gen, NULL);
   json_node_free (root);
   g_object_unref (gen);
   g_object_unref (builder);

   return str;
}




void serialize_io_path( JsonBuilder* builder, DDCA_IO_Path io_path) {
   bool debug = true;
   DBGMSF(debug, "Starting. io_path=%s", dpath_repr_t(&io_path));
   // json_builder_set_member_name(builder, "io_path");
   json_builder_begin_object(builder);
   json_builder_set_member_name(builder, "io_type");
   json_builder_add_int_value(builder, io_path.io_mode);
   json_builder_set_member_name(builder, "busno_or_hiddev");
   json_builder_add_int_value(builder, io_path.path.i2c_busno);
   json_builder_end_object(builder);
   DBGMSF(debug, "Done.");
}

void serialize_mccs_version(JsonBuilder* builder, DDCA_MCCS_Version_Spec vspec) {
   bool debug =true;
   DBGMSF(debug, "Starting.");
   // json_builder_set_member_name(builder, "vspec");
   json_builder_begin_object(builder);
   json_builder_set_member_name(builder, "major");
   json_builder_add_int_value(builder, vspec.major);
   json_builder_set_member_name(builder, "minor");
   json_builder_add_int_value(builder, vspec.minor);
   json_builder_end_object(builder);
   DBGMSF(debug, "Done.");
}

void serialize_parsed_edid(JsonBuilder* builder, Parsed_Edid * edid) {
   bool debug = true;
   DBGMSF(debug, "Starting.");
   json_builder_begin_object(builder);
   json_builder_set_member_name(builder, "bytes");
   char edid_bytes[257];
   json_builder_add_string_value(builder,
         hexstring2(
                   edid->bytes,      // bytes to convert
                   128,        // number of bytes
                   "",     // separator string between hex digits
                   true,  // use upper case hex characters
                   edid_bytes,     // buffer in which to return hex string
                   257) );     // buffer size
   DBGMSG("edid_bytes=%s", edid_bytes);
   json_builder_set_member_name(builder, "edid_source");
   json_builder_add_string_value(builder, edid->edid_source);
   json_builder_end_object(builder);
   DBGMSF(debug, "Done.");
}

void serialize_mmk(JsonBuilder* builder, DDCA_Monitor_Model_Key* mmk) {
   bool debug = true;
   DBGMSF(debug, "Starting.  mmk->%s", mmk_repr(*mmk));
   json_builder_begin_object(builder);
   json_builder_set_member_name(builder, "mfg_id");
   json_builder_add_string_value(builder, mmk->mfg_id);
   json_builder_set_member_name(builder, "model_name");
   json_builder_add_string_value(builder, mmk->model_name);
   json_builder_set_member_name(builder, "product_code");
   json_builder_add_int_value(builder, mmk->product_code);
   json_builder_end_object(builder);
}

#define SERIALIZE_INT(_builder, _struct_ptr, _field_name) \
   do { \
      json_builder_set_member_name(_builder, #_field_name); \
      json_builder_add_int_value(_builder, _struct_ptr->_field_name); \
   } while(0)

#define SERIALIZE_STR(_builder, _struct_ptr, _field_name) \
   do {\
      if (_struct_ptr->_field_name) { \
         json_builder_set_member_name(_builder, #_field_name); \
         json_builder_add_string_value(_builder, _struct_ptr->_field_name); \
      } \
   } while(0);


void serialize_display_ref(JsonBuilder* builder, Display_Ref* dref) {
   bool debug = true;
   DBGMSF(debug, "Starting. dref=%s", dref_repr_t(dref));
   json_builder_begin_object(builder);
   json_builder_set_member_name(builder, "io_path");
   serialize_io_path(builder, dref->io_path);
   json_builder_set_member_name(builder, "usb_bus");
   json_builder_add_int_value(builder, dref->usb_bus);
   SERIALIZE_INT(builder, dref, usb_device);
   SERIALIZE_STR(builder, dref, usb_hiddev_name);
   json_builder_set_member_name(builder, "vcp_version_xdf");
   serialize_mccs_version(builder, dref->vcp_version_xdf);
   json_builder_set_member_name(builder, "vcp_version_cmdline");
   serialize_mccs_version(builder, dref->vcp_version_cmdline);
   SERIALIZE_INT(builder, dref, flags);
   SERIALIZE_STR(builder, dref, capabilities_string);
   json_builder_set_member_name(builder, "pedid");
   serialize_parsed_edid(builder, dref->pedid);
   json_builder_set_member_name(builder, "mmid");
   serialize_mmk(builder, dref->mmid);
   SERIALIZE_INT(builder, dref, dispno);
   if (dref->dispno == -2) {
      json_builder_set_member_name(builder, "actual_display_busno");
      json_builder_add_int_value(builder, dref->actual_display->io_path.path.i2c_busno);
   }
   SERIALIZE_STR(builder, dref, driver_name);

   json_builder_end_object(builder);
   DBGMSF(debug, "Done");
}

void serialize_all_displays(JsonBuilder * builder, GPtrArray* all_displays) {
   bool debug = true;
   DBGMSF(debug, "Starting");
   // json_builder_begin_object(builder);
   json_builder_set_member_name(builder, "version");
   json_builder_add_int_value(builder, 1);
   json_builder_set_member_name(builder, "all_displays");
   json_builder_begin_array(builder);
   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
      serialize_display_ref(builder, dref);
   }
   json_builder_end_array(builder);
   DBGMSF(debug, "Done");
}


char * ddc_serialize_displays() {
   bool debug = true;
   DBGMSF(debug, "Starting");
   JsonBuilder* builder = json_builder_new();
   json_builder_begin_object(builder);

   GPtrArray* all_displays = ddc_get_all_displays();
   serialize_all_displays(builder, all_displays);
   json_builder_end_object(builder);

   JsonGenerator* gen = json_generator_new();
   json_generator_set_pretty(gen, true);
   JsonNode * root = json_builder_get_root (builder);
   json_generator_set_root (gen, root);
   gchar *str = json_generator_to_data (gen, NULL);
   json_node_free (root);
   g_object_unref (gen);
   g_object_unref (builder);
   DBGMSF(debug, "Done.  Returning: %s", str);

   return str;
}


void debug_reader(JsonReader * reader, bool show_detail, char * msg) {
   if (msg)
      DBGMSG(msg);
   DBGMSG("member_name: %s", json_reader_get_member_name(reader));

   DBGMSG("  is_array(): %s",  sbool(json_reader_is_array(reader)));
   DBGMSG("  is_object(): %s", sbool(json_reader_is_object(reader)));
   DBGMSG("  is_value(): %s", sbool(json_reader_is_value(reader)));
   int elemct = json_reader_count_elements(reader);
   DBGMSG("  found %d elements", elemct);
   int memberct = json_reader_count_members(reader);
   DBGMSG("  found %d members:", memberct);
   if (show_detail) {
   if (memberct >= 0) {
      char** members = json_reader_list_members(reader);
      int i = 0;
      while (members[i] ) {
          char * m = members[i];
          DBGMSG("members[%d] = |%s|", i, m);
          i++;
      }
   }
   }

}


void show_cur_loc(JsonParser * parser) {
   guint cur_line = json_parser_get_current_line(parser);
   guint cur_pos  = json_parser_get_current_pos(parser);
   DBGMSG("  current line:pos = %d:%d", cur_line, cur_pos);
}







// #ifdef WORK_IN_PROGRESS

Display_Ref * deserialize_dref(JsonReader* reader) {
   DBGMSG("Starting");
   debug_reader(reader, true, "Starting deserialize_dref()");
   Display_Ref * dref = NULL;

   char** members = json_reader_list_members(reader);
   for (int i = 0; members[i]; i++ ) {
       char * cur_member = members[i];
       DBGMSG("cur_member = members[%d] = |%s|", i, cur_member);

       bool ok2 = json_reader_read_member(reader, cur_member);
       if ( !ok2) {
          const GError * err = json_reader_get_error(reader);
          DBGMSG("Error reading %s: %s", cur_member, err->message);
          return NULL;
       }
       debug_reader(reader, true, "after first read_member() in loop");

#ifdef REF
typedef struct _display_ref {
char                     marker[4];
DDCA_IO_Path             io_path;
int                      usb_bus;
int                      usb_device;
char *                   usb_hiddev_name;
DDCA_MCCS_Version_Spec   vcp_version_xdf;
DDCA_MCCS_Version_Spec   vcp_version_cmdline;
Dref_Flags               flags;
char *                   capabilities_string;   // added 4/2017, private copy
Parsed_Edid *            pedid;                 // added 4/2017
DDCA_Monitor_Model_Key * mmid;                  // will be set iff pedid
int                      dispno;
void *                   detail;                // I2C_Bus_Info or Usb_Monitor_Info
Display_Async_Rec *      async_rec;
Dynamic_Features_Rec *   dfr;                   // user defined feature metadata
uint64_t                 next_i2c_io_after;     // nanosec
struct _display_ref *    actual_display;        // if dispno == -2
char *                   driver_name;           //
struct Per_Display_Data* pdd;
} Display_Ref;
#endif


       if (streq(cur_member, "io_path")) {
          json_reader_read_member(reader, "io_type");
          const int io_type = json_reader_get_int_value(reader);
          DBGMSG("io_type: %d", io_type);
          json_reader_end_member(reader);

          json_reader_read_member(reader, "busno_or_hiddev");
          const int busno_or_hiddev = json_reader_get_int_value(reader);
          DBGMSG("busno_or_hiddev: %d", busno_or_hiddev);
          // json_reader_end_member(reader);   // WHY NEED TO COMMENT OUT?
       }

       else if (streq(cur_member, "usb_bus")) {
          int usb_bus = json_reader_get_int_value(reader);
          DBGMSG("usb_bus = %d", usb_bus);
       }

       else if (streq(cur_member, "usb_device")) {
          int usb_device = json_reader_get_int_value(reader);
          DBGMSG("usb_device = %d", usb_device);
       }

       else if (streq(cur_member, "usb_hiddev_name")) {
          const char * usb_hiddev_name = json_reader_get_string_value(reader);
          DBGMSG("usb_hiddev_name = %s", usb_hiddev_name);
       }

       else if (streq(cur_member, "vcp_version_xdf")) {
          json_reader_read_member(reader, "major");
          int major = json_reader_get_int_value(reader);
          DBGMSG("vcp_version_xdf.major=%d", major);
          json_reader_end_member(reader);
          json_reader_read_member(reader, "minor");
          int minor = json_reader_get_int_value(reader);
          DBGMSG("vcp_version_xdf.minor=%d", minor);
          DBGMSG("Q: %s", json_reader_get_member_name(reader));
          json_reader_end_member(reader);  // WHY
          DBGMSG("R: %s", json_reader_get_member_name(reader));
       }

       else if (streq(cur_member, "vcp_version_cmdline")) {
          json_reader_read_member(reader, "major");
          int major = json_reader_get_int_value(reader);
          DBGMSG("vcp_version_cmdline.major=%d", major);
          json_reader_end_member(reader);
          json_reader_read_member(reader, "minor");
          int minor = json_reader_get_int_value(reader);
          DBGMSG("vcp_version_cmdline.mino=%d", minor);
          DBGMSG("member_name: %s", json_reader_get_member_name(reader) );
         json_reader_end_member(reader);  // WHY
         DBGMSG("K: %s", json_reader_get_member_name(reader));
       }


       else if (streq(cur_member, "flags")) {
          int flags = json_reader_get_int_value(reader);
           DBGMSG("flags = %d", flags);
       }


       else if (streq(cur_member, "capabilities_string")) {
          const char * capabilities = json_reader_get_string_value(reader);
           DBGMSG("capabilities = %s", capabilities);
       }

       else if (streq(cur_member, "pedid")) {
           debug_reader(reader, false, "G");
           bool ok = json_reader_read_member(reader, "bytes");

           if (!ok) {
              const GError* err = json_reader_get_error(reader);
              DBGMSG("read_member(bytes) error: %s", err->message);
           }
           debug_reader(reader, false, "after read_member(bytes)");
           const char * sbytes = json_reader_get_string_value(reader);

           int ct = 0;
           Byte * hbytes;
           DBGMSG("sbytes=%s", sbytes);
           if (!sbytes) {
              const GError* err = json_reader_get_error(reader);
                DBGMSG("get_string_value(bytes) error: %s", err->message);
           }
           else {
              ct = hhs_to_byte_array(sbytes, &hbytes);
              assert(ct == 128);
           }
           json_reader_end_member(reader);
           json_reader_read_member(reader, "edid_source");
           const char *edid_source = json_reader_get_string_value(reader);
           if (ct == 128) {
              Parsed_Edid * parsed_edid = create_parsed_edid2(hbytes, g_strdup(edid_source));
              report_parsed_edid(parsed_edid, true, 1);
           }
        }


       else if (streq(cur_member, "mmid")) {
          json_reader_read_member(reader, "mfg_id");
          const char * mfg_id = json_reader_get_string_value(reader);
          DBGMSG("mfg_id = %s", mfg_id);
          json_reader_end_member(reader);
          json_reader_read_member(reader, "model_name");
          const char * model_name = json_reader_get_string_value(reader);
          DBGMSG("model_name", model_name);
          json_reader_end_member(reader);
          json_reader_read_member(reader, "product_code");
          int product_code = json_reader_get_int_value(reader);
          DBGMSG("product_code=%d", product_code);
         // json_reader_end_member(reader);  // WHY
       }

       else if (streq(cur_member, "dispno")) {
          int dispno = json_reader_get_int_value(reader);
           DBGMSG("dispno = %d", dispno);
       }

#ifdef REF
       if (dref->dispno == -2) {
          json_builder_set_member_name(builder, "actual_display_busno");
          json_builder_add_int_value(builder, dref->actual_display->io_path.path.i2c_busno);
       }
#endif


       else if (streq(cur_member, "actual_display_busno")) {
          int actual_display_busno = json_reader_get_int_value(reader);
          DBGMSG("dispno = %d", actual_display_busno);
       }

       else if (streq(cur_member, "driver_name")) {
          const char * driver_name = json_reader_get_string_value(reader);
          DBGMSG("driver_name=%s", driver_name);
       }

       else {
          DBGMSG("Unhandled: %s", cur_member);
       }

       debug_reader(reader, false, "before final reader_end_member() in loop");
       json_reader_end_member(reader);
       debug_reader(reader, false, "after final reader_end_member() in loop");
   }
   g_strfreev(members);

   DBGMSG("Done");
   return dref;
}


GPtrArray * ddc_deserialize_displays(const char * jstring) {
   GPtrArray * all_displays = g_ptr_array_new();

   JsonParser *parser = json_parser_new ();
    GError* err = NULL;
    if (!json_parser_load_from_data (parser, jstring, -1, &err)) {
        printf("error in parsing json data %s", err->message);
        g_error_free (err);
        g_object_unref (parser);
        return NULL;
    }

    JsonReader *reader = json_reader_new (json_parser_get_root (parser));
    debug_reader(reader, false, "root level");
    show_cur_loc(parser);

    json_reader_read_member(reader, "version");
    debug_reader(reader, false, "After read_member(\"version\"");
    show_cur_loc(parser);
    DBGMSG("version: %d", json_reader_get_int_value(reader));
    json_reader_end_member(reader);

    json_reader_read_member(reader, "all_displays");
    debug_reader(reader, false, "after read_member(\"all_displays\"");
    show_cur_loc(parser);
    for (int ndx = 0; ndx <= json_reader_count_elements(reader); ndx++) {
       DBGMSG("Reading all_displays[%d]", ndx);
       bool ok = json_reader_read_element(reader, ndx);
       if (!ok) {
          const GError * err = json_reader_get_error(reader);
          DBGMSG("Error reading element %d: %s", ndx, err->message);
          json_reader_end_element(reader);
       }
       else {
          debug_reader(reader, false, "successfully read array element");
          show_cur_loc(parser);
          deserialize_dref( reader);

       }
    }
    g_object_unref (reader);
    g_object_unref (parser);
    return all_displays;
}
#endif

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


#ifdef REF
bool debug = true;

json_builder_set_member_name(builder, "edid_source");
json_builder_add_string_value(builder, edid->edid_source);
json_builder_end_object(builder);
DBGMSF(debug, "Done.");
#endif


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


char * ddc_serialize_displays() {
   bool debug = true;
   GPtrArray* all_displays = ddc_get_all_displays();
   json_t* jdisplays = json_array();
   json_t* root = json_object();
   json_object_set_new(root, "version", json_integer(1));
   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      json_t* node = serialize_one_display(g_ptr_array_index(all_displays, ndx));
      json_array_append(jdisplays, node);
   }
   json_object_set_new(root, "all_displays", jdisplays);
   char * result = json_dumps(root, JSON_INDENT(3));

   if (debug) {
      DBGMSG("Returning:");
      DBGMSG(result);
   }
   return result;
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

   Display_Ref * dref =    create_base_display_ref(io_path);

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


GPtrArray * ddc_deserialize_displays(const char * jstring) {
   bool debug = false;
   GPtrArray * restored_displays = g_ptr_array_new();

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

   json_t* disp_nodes = json_object_get(root, "all_displays");
   if (!(disp_nodes && json_is_array(disp_nodes))) {
      if (disp_nodes)
         DBGMSG("error: all_displays is not an array");
      else
         DBGMSG("member all_displays not found");
      json_decref(root);
      return NULL;
   }

   for (int dispctr = 0; dispctr < json_array_size(disp_nodes); dispctr++) {
      json_t* one_display = json_array_get(disp_nodes, dispctr);
      if (! (one_display && json_is_object(one_display))) {
         if (one_display)
            DBGMSG("all_displays[%d] not found", dispctr);
         else
            DBGMSG("all_displays[%d] is not an object", dispctr);
         json_decref(root);
         return NULL;
      }

      Display_Ref * dref = deserialize_one_display(one_display);
      g_ptr_array_add(restored_displays, dref);
}

   return restored_displays;
}

