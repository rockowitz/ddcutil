// ddc_serialize.c

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <json-glib-1.0/json-glib/json-glib.h>
#include <stdbool.h>

#include "public/ddcutil_types.h"
#include "util/string_util.h"
#include "base/core.h"
#include "base/displays.h"
#include "base/i2c_bus_base.h"
#include "base/monitor_model_key.h"
#include "ddc/ddc_displays.h"

#include "ddc_serialize.h"


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
   json_builder_add_string_value(builder, hexstring_t(edid->bytes, 128));
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
      json_builder_set_member_name(_builder, #_field_name); \
      json_builder_add_string_value(_builder, _struct_ptr->_field_name); \
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
   if (dref->driver_name)
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

#ifdef WORK_IN_PROGRESS

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
    DBGMSG("root level");
    DBGMSG("is_array(): %s",  sbool(json_reader_is_array(reader)));
    DBGMSG("is_object(): %s", sbool(json_reader_is_object(reader)));
    DBGMSG("is_value(): %s", sbool(json_reader_is_value(reader)));
    int elemct = json_reader_count_elements(reader);
    DBGMSG("found %d elements", elemct);
    int memberct = json_reader_count_members(reader);
    DBGMSG("found %d members", memberct);

    json_reader_read_member(reader, "version");
    DBGMSG("member_name: %s", json_reader_get_member_name(reader));
     DBGMSG("is_array(): %s",  sbool(json_reader_is_array(reader)));
     DBGMSG("is_object(): %s", sbool(json_reader_is_object(reader)));
     DBGMSG("is_value(): %s", sbool(json_reader_is_value(reader)));
     DBGMSG("found %d elements", json_reader_count_elements(reader));
     DBGMSG("found %d members", json_reader_count_members(reader));
     DBGMSG("version: %d", json_reader_get_string_value(reader));


    char** members = json_reader_list_members(reader);
    int i = 0;
    while (members[i] ) {
        char * m = members[i];
        DBGMSG("members[%d] = |%s|", i, m);
        if (streq(m, "version")) {
           DBGMSG("version: %d", json_reader_get_int_value(reader));
        }
        else if (streq(m, "all_displays")) {

            bool ok =  json_reader_read_member (reader, members[i]);
            DBGMSG("ok: %s", sbool(ok));
            DBGMSG("member: all_displays");
            DBGMSG("member_name: %s", json_reader_get_member_name(reader));
            DBGMSG("is_array(): %s",  sbool(json_reader_is_array(reader)));
            DBGMSG("is_object(): %s", sbool(json_reader_is_object(reader)));
            DBGMSG("is_value(): %s", sbool(json_reader_is_value(reader)));
            DBGMSG("found %d elements", json_reader_count_elements(reader));
            DBGMSG("found %d members", json_reader_count_members(reader));

            char** members2 = json_reader_list_members(reader);
            int kk = 0;
            while( members2[kk]) {
               assert(json_reader_read_member (reader, members2[kk]) );
               DBGMSG("member_name2: %s", json_reader_get_member_name(reader));
               DBGMSG("is_array(): %s",  sbool(json_reader_is_array(reader)));
               DBGMSG("is_object(): %s", sbool(json_reader_is_object(reader)));
               DBGMSG("is_value(): %s", sbool(json_reader_is_value(reader)));
               DBGMSG("found %d elements", json_reader_count_elements(reader));
               DBGMSG("found %d members", json_reader_count_members(reader));
               kk++;
            }

            assert(json_reader_read_element(reader,0) );
            const char * member_name = json_reader_get_member_name(reader);
            DBGMSG("member_name: %s", member_name);
            DBGMSG("is_array(): %s",  sbool(json_reader_is_array(reader)));
            DBGMSG("is_object(): %s", sbool(json_reader_is_object(reader)));
            DBGMSG("is_value(): %s", sbool(json_reader_is_value(reader)));
            elemct = json_reader_count_elements(reader);
            DBGMSG("found %d elements", elemct);
            memberct = json_reader_count_members(reader);
            DBGMSG("found %d members", memberct);


          //   const char * s = json_reader_get_string_value (reader);
            json_reader_end_member (reader);
            printf("parse member %s\n", members[i]);
            // printf("parse value %s\n", s);
        }
        else if (streq(m, "all_busses")) {
        }
        else {
           DBGMSG("Unexpected member: %s", m);
        }
        i++;
    }

    g_strfreev(members);
    g_object_unref (reader);
    g_object_unref (parser);
    return all_displays;

}
#endif

