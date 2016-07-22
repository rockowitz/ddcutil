## Process this file with automake to produce Makefile.in

# AM_CFLAGS = -Wall -Werror
AM_CFLAGS = -Wall
if ENABLE_CALLGRAPH_COND
  AM_CFLAGS += -fdump-rtl-expand
endif

# Is this really necessary?   Don't want to push these files to github.
# there ought to be a cleaner way
# MOSTLYCLEANFILES =    \
# base/.deps/*          \
# cmdline/.deps/*       \
# ddc/.deps/*.Po        \
# i2c/.deps/*           \
# ddctool_app/.deps/*          \
# test/ddc/.deps/*      \
# test/i2c/.deps/*      \
# test/.deps/*          
# util/.deps/*          \
# adl/.deps/*           \
# adl/adl_impl/.deps/*  \
# adl/adl_mock_impl/.deps/*  

CLEANFILES =\
adl/*expand         \
adl/adl_impl/*expand \
adl/adl_mock_impl/*expand \
app_ddctool/*expand  \
base/*expand         \
cmdline/*expand      \
ddc/*expand          \
vcp/*expand          \
i2c/*expand          \
usb/*expand          \
libmain/*expand      \
sample_clients/*expand \
test/*expand \
test/adl/*expand \
test/ddc/*expand \
test/i2c/*expand \
util/*expand 


bin_PROGRAMS = \
ddctool  

if ENABLE_SHARED_LIB_COND
# Sample client program for shared library:
bin_PROGRAMS += \
laclient 

if ENABLE_GOBJECT_COND
bin_PROGRAMS += \
goclient
endif

endif



# Convenience library containing code shared between ddctool and libddctool
noinst_LTLIBRARIES = libcommon.la

if ENABLE_SHARED_LIB_COND
# Shared library
lib_LTLIBRARIES = libddctool.la
endif

# Files only in ddctool:
ddctool_SOURCES = \
cmdline/cmd_parser_aux.c \
cmdline/cmd_parser_goption.c \
cmdline/parsed_cmd.c \
app_ddctool/main.c \
app_ddctool/app_dumpload.c \
app_ddctool/query_sysenv.c \
app_ddctool/app_setvcp.c \
app_ddctool/app_getvcp.c \
app_ddctool/testcases.c 

if ENABLE_LIBUSB_COND
ddctool_SOURCES += \
util/libusb_reports.c \
util/libusb_util.c \
util/base_hid_report_descriptor.c \
util/hid_report_descriptor.c
endif

if ENABLE_LIBUDEV_COND
ddctool_SOURCES += \
util/udev_util.c 
endif

if INCLUDE_TESTCASES_COND
ddctool_SOURCES += \
   test/ddc/ddc_capabilities_tests.c \
   test/ddc/ddc_vcp_tests.c \
   test/i2c/i2c_edid_tests.c \
   test/i2c/i2c_io_old.c \
   test/testcase_table.c
else
ddctool_SOURCES += \
   app_ddctool/testcase_mock_table.c
endif


# Files implementing GObject API for introspection: 
gobject_api_sources = \
gobject_api/ddcg_cont_response.c \
gobject_api/ddcg_context.c \
gobject_api/ddcg_display_handle.c \
gobject_api/ddcg_display_identifier.c \
gobject_api/ddcg_display_ref.c

# if use ddcg_gobjects.h to pull in all the .h files, get strange errors re unexpected semicolons
# gobject_api_headers = \
# gobject_api/ddcg_gobjects.h

gobject_api_headers = \
gobject_api/ddcg_cont_response.h \
gobject_api/ddcg_context.h \
gobject_api/ddcg_display_handle.h \
gobject_api/ddcg_display_identifier.h \
gobject_api/ddcg_display_ref.h \
gobject_api/ddcg_types.h


# files only in libddctool:
libddctool_la_SOURCES = \
libmain/ddct_public.c  

if ENABLE_GOBJECT_COND
   libddctool_la_SOURCES += $(gobject_api_sources)
endif


# files only in laclient:
laclient_SOURCES = \
sample_clients/clmain.c

#idef ENABLE_GOBJECT_COND
# files only in goclient:
goclient_SOURCES = \
sample_clients/gomain.c
#endif

# libcommon contains the source files that are 
# shared between ddctool and libddctool: 
libcommon_la_SOURCES = \
base/base_services.c \
base/core.c \
base/ddc_errno.c \
base/ddc_packets.c \
base/displays.c \
util/edid.c \
base/execution_stats.c \
base/linux_errno.c \
base/status_code_mgt.c \
base/sleep.c \
vcp/vcp_base.c \
vcp/vcp_feature_values.c \
vcp/ddc_command_codes.c \
ddc/ddc_displays.c      \
ddc/ddc_edid.c          \
ddc/ddc_multi_part_io.c \
ddc/ddc_packet_io.c \
ddc/ddc_read_capabilities.c \
ddc/ddc_output.c \
ddc/ddc_services.c \
ddc/ddc_strategy.c \
ddc/ddc_vcp.c \
vcp/parse_capabilities.c \
ddc/try_stats.c \
vcp/vcp_feature_codes.c \
vcp/parsed_capabilities_feature.c \
vcp/vcp_feature_set.c \
ddc/ddc_vcp_version.c \
i2c/i2c_base_io.c \
i2c/i2c_bus_core.c \
i2c/i2c_do_io.c \
usb/usb_base.c \
usb/usb_edid.c \
usb/usb_displays.c  \
usb/usb_vcp.c  \
ddc/ddc_dumpload.c \
util/data_structures.c \
util/file_util.c  \
util/glib_util.c \
util/hiddev_reports.c \
util/hiddev_util.c \
util/multi_level_map.c \
util/output_sink.c \
util/device_id_util.c \
util/report_util.c \
util/string_util.c \
util/usb_hid_common.c \
util/x11_util.c \
util/subprocess_util.c

# libcommon_la_SOURCES += \
# util/libusb_reports.c

if HAVE_ADL_COND
libcommon_la_SOURCES += \
  adl/adl_impl/adl_aux_intf.c \
  adl/adl_impl/adl_errors.c \
  adl/adl_impl/adl_intf.c \
  adl/adl_impl/adl_report.c \
  adl/adl_impl/adl_shim.c \
  test/adl/adl_tests.c \
  test/adl/adl_from_sample.c 
else
libcommon_la_SOURCES += \
  adl/adl_mock_impl/adl_mock_shim.c \
  adl/adl_mock_impl/adl_mock_errors.c
endif


# EXTRA_DIST = \
# util/string_util.h \
# util/subprocess_util.h

dist-hook:
	find .  -type f | grep  \.h$ | xargs -i cp --parents "{}" $(distdir)
    


# Header files

# -I/usr/src/linux-headers-4.2.0-18/include/linux 
# -I/usr/include/x86_64-linux-gnu 
# -I/usr/include/linux
# -I/shared/linsrc/i2c-tools/i2c-tools-3.1.1/include/linux 
# -I/usr/include 
# trying to get i2c-dev.h from /usr/include/linux causes errors

# GLIB_CFLAGS contains output of pkgconfig --cflags glib-2.0
AM_CPPFLAGS= \
$(GLIB_CFLAGS) \
$(GOBJECT_CFLAGS) \
$(XRANDR_CFLAGS) \
$(LIBUSB_CFLAGS) \
-I$(top_srcdir) 


# Compiler flags
ddctool_CFLAGS        = $(AM_CFLAGS)
laclient_CFLAGS       = $(AM_CFLAGS)
goclient_CFLAGS       = $(AM_CFLAGS)
libcommon_la_CFLAGS   = $(AM_CFLAGS) 
libddctool_la_CFLAGS  = $(AM_CFLAGS)

  
if HAVE_ADL_COND
AM_CPPFLAGS +=               \
   -I@ADL_HEADER_DIR@  
endif


# Link the libraries 
# $(GLIB_LIBS) contains output of pkgconfig --libs glib-2.0

# Be careful about library ordering 
# library must be listed after any libraries that depend on it

# Link the shared library
libddctool_la_LIBADD = -lz
libddctool_la_LIBADD +=  \
   libcommon.la   \
   $(GOBJECT_LIBS) \
   $(GLIB_LIBS) 

if ENABLE_LIBUDEV_COND
libddctool_la_LIBADD += \
   $(UDEV_LIBS) 
endif
         

libddctool_la_LDFLAGS = -export_symbols-regex '^ddc[tg]_[^_]'
# libddctool_la_LDFLAGS += -version-info '0:1:1'


# Link the executables

ddctool_LDADD = \
   libcommon.la \
   $(XRANDR_LIBS) \
   $(LIBUSB_LIBS) \
   $(X11_LIBS) \
   $(GLIB_LIBS)

if ENABLE_LIBUDEV_COND
ddctool_LDADD += \
   $(UDEV_LIBS) 
endif

# Fails if not linked with libudev - why?
laclient_LDADD = \
  libddctool.la \
  $(GOBJECT_LIBS)

if ENABLE_LIBUDEV_COND
laclient_LDADD += \
  $(UDEV_LIBS) 
endif

goclient_LDADD = \
  libddctool.la \
  $(GOBJECT_LIBS)



# echo "hi there"
# echo $(GLIB_LIBS)
# LIBS = $(GLIB_LIBS) 


# Comments from /usr/share/gobject-introspection-1.0/Makefile.introspection:

#
# * Input variables:
#
#   INTROSPECTION_GIRS - List of GIRS that should be generated
#   INTROSPECTION_SCANNER - Command to invoke scanner, normally set by
#      GOBJECT_INTROSPECTION_REQUIRE/CHECK() in introspection.m4
#   INTROSPECTION_SCANNER_ARGS - Additional args to pass in to the scanner
#   INTROSPECTION_SCANNER_ENV - Environment variables to set before running
#      the scanner
#   INTROSPECTION_COMPILER - Command to invoke compiler, normally set by
#      GOBJECT_INTROSPECTION_REQUIRE/CHECK() in introspection.m4
#   INTROSPECTION_COMPILER_ARGS - Additional args to pass in to the compiler
#
# * Simple tutorial
#
# Add this to configure.ac:
#   -Wno-portability to AM_INIT_AUTOMAKE
#   GOBJECT_INTROSPECTION_CHECK([0.6.7])
#
# Add this to Makefile.am where your library/program is built:
#   include $(INTROSPECTION_MAKEFILE)
#   INTROSPECTION_GIRS = YourLib-1.0.gir
#   YourLib-1.0.gir: libyourlib.la
#   YourLib_1_0_gir_NAMESPACE = YourLib
#   YourLib_1_0_gir_VERSION = 1.0
#   YourLib_1_0_gir_LIBS = libyourlib.la
#   YourLib_1_0_gir_FILES = $(libyourlib_1_0_SOURCES)
#   girdir = $(datadir)/gir-1.0
#   dist_gir_DATA = YourLib-1.0.gir
#   typelibdir = $(libdir)/girepository-1.0
#   typelib_DATA = YourLib-1.0.typelib
#   CLEANFILES = $(dist_gir_DATA) $(typelib_DATA)
#

# And later in file: 
#
# Creates a GIR by scanning C headers/sources
# $(1) - Name of the gir file (output)
#
# If output is Gtk-2.0.gir then you should name the variables like
# Gtk_2_0_gir_NAMESPACE, Gtk_2_0_gir_VERSION etc.
# Required variables:
# FILES - C sources and headers which should be scanned
#
# One of these variables are required:
# LIBS - Library where the symbol represented in the gir can be found
# PROGRAM - Program where the symbol represented in the gir can be found
#
# Optional variables
# NAMESPACE - Namespace of the gir, first letter capital,
#   rest should be lower case, for instance: 'Gtk', 'Clutter', 'ClutterGtk'.
#   If not present the namespace will be fetched from the gir filename,
#   the part before the first dash. For 'Gtk-2.0', namespace will be 'Gtk'.
# VERSION - Version of the gir, if not present, will be fetched from gir
# filename, the part after the first dash. For 'Gtk-2.0', version will be '2.0'.
# LIBTOOL - Command to invoke libtool, usually set by automake
# SCANNERFLAGS - Flags to pass in to the scanner, see g-ir-scanner(1) for a list
# CFLAGS - Flags to pass in to the parser when scanning headers
# LDFLAGS - Linker flags used by the scanner
# PACKAGES - list of pkg-config names which cflags are required to parse
#   the headers of this gir
# INCLUDES - Gir files to include without the .gir suffix, for instance
#   GLib-2.0, Gtk-2.0. This is needed for all libraries which you depend on that
#   provides introspection information.
# EXPORT_PACKAGES - list of pkg-config names that are provided by this gir.
#   By default the names in the PACKAGES variable will be used.


if HAVE_INTROSPECTION
if ENABLE_GOBJECT_COND
   -include $(INTROSPECTION_MAKEFILE)
   INTROSPECTION_GIRS =
   # INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all --verbose --identifier-prefix=Ddcg --symbol-prefix=ddcg
   # INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir) --verbose
   INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all --verbose 
   INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir) --verbose

   introspection_sources = \
      $(gobject_api_sources) \
      $(gobject_api_headers)

   # add the target:
   Ddctool-1.0.gir: libddctool.la
   # Colord-1.0.gir: libcolord.la libcolordprivate.la

   Ddctool_1_0_gir_INCLUDES = GObject-2.0
   Ddctool_1_0_gir_CFLAGS = $(AM_CPPFLAGS)
   # Colord_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0
   # Colord_1_0_gir_CFLAGS = $(AM_CPPFLAGS) -DCD_DISABLE_DEPRECATED

   # Ddctool_1_0_gir_NAMESPACE=Ddcg
   # Ddctool_1_0_gir_VERSION=1.0

   Ddctool_1_0_gir_SCANNERFLAGS = --identifier-prefix=Ddcg \
        --symbol-prefix=ddcg \
        --warn-all \
        --add-include-path=$(srcdir) 
   # Colord_1_0_gir_SCANNERFLAGS = --identifier-prefix=Cd \
   #       --symbol-prefix=cd \
   #       --warn-all \
   #       --add-include-path=$(srcdir) \
   #       --c-include="colord.h"

  
   Ddctool_1_0_gir_EXPORT_PACKAGES  = ddctool
   Ddctool_1_0_gir_LIBS             = libddctool.la
   Ddctool_1_0_gir_FILES            = $(introspection_sources)
   # Colord_1_0_gir_EXPORT_PACKAGES = colord
   # Colord_1_0_gir_LIBS            = libcolord.la libcolordprivate.la
   # Colord_1_0_gir_FILES           = $(introspection_sources)


   INTROSPECTION_GIRS += Ddctool-1.0.gir
   # INTROSPECTION_GIRS += Colord-1.0.gir


   girdir = $(datadir)/gir-1.0
   # dist_gir_DATA=$(INTROSPECTION_GIRS)
   gir_DATA = $(INTROSPECTION_GIRS)

   typelibdir = $(libdir)/girepository-1.0
   typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

   CLEANFILES += $(gir_DATA) $(typelib_DATA)
   # CLEANFILES += $(gir_DATA) $(typelib_DATA) *.log *.trs *.test
endif
endif

