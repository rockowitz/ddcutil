# Makefile for GObject introspection
# Adapted from that for colord by Richard Hughes

CLEANFILES = \
*expand

clean-local:
	echo "(src/gobject_api/Makefile) clean-local"

if ENABLE_SHARED_LIB_COND
if ENABLE_GOBJECT_COND

# The library that exposes the ddcutil GObject API
lib_LTLIBRARIES = libddcgobj.la

# Files implementing GObject API for introspection: 
gobject_api_sources = \
  ddcg_cont_response.c \
  ddcg_context.c \
  ddcg_display_handle.c \
  ddcg_display_identifier.c \
  ddcg_display_ref.c

# if use ddcg_gobjects.h to pull in all the .h files, get strange errors re unexpected semicolons
# gobject_api_headers = \
# gobject_api/ddcg_gobjects.h
 
gobject_api_headers = \
  ddcg_cont_response.h \
  ddcg_context.h \
  ddcg_display_handle.h \
  ddcg_display_identifier.h \
  ddcg_display_ref.h \
  ddcg_types.h

libddcgobj_la_SOURCES = $(gobject_api_sources)

# files only in goclient:
# Normally, sample code would be in a separate directory. 
# gomain.c has been moved here to consolidate the mothballed GObject introspection code.
goclient_SOURCES = \
  gomain.c

#
# C Pre-Processor Flags:
#
AM_CPPFLAGS = \
  -I..        \
  -I../public  \
  $(GOBJECT_CFLAGS) 

# Be careful about library ordering
# A library must be listed AFTER any libraries that depend on it.
# libcommon.la included because gobject code directly calls 
# dbgtrc and rpt_str, which are not exported by libddcutil
libddcgobj_la_LIBADD =  \
	../libcommon.la \
    ../libddcutil.la    \
	$(GOBJECT_LIBS) 


libddcgobj_la_LDFLAGS =
libddcgobj_la_LDFLAGS += -export_symbols-regex '^ddc[tgs]_[^_]'
# libddcgobj_la_LDFLAGS += -version-info '0:1:1'
# libddcgobj_la_LDFLAGS += -export-dynamic 


# 
# Sample client program.  
# 
check_PROGRAMS = \
goclient

# Compiler flags
goclient_CFLAGS       = $(AM_CFLAGS)

# libddcutil is in the list because goclient uses function dbgtrc() 
goclient_LDADD = \
 	libddcgobj.la \
	../libddcutil.la \
 	$(GOBJECT_LIBS)



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
   ddcutil-1.0.gir: libddcgobj.la ../libddcutil.la

   ddcutil_1_0_gir_INCLUDES = GObject-2.0
   ddcutil_1_0_gir_CFLAGS = $(AM_CPPFLAGS)
   # Colord_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0
   # Colord_1_0_gir_CFLAGS = $(AM_CPPFLAGS) -DCD_DISABLE_DEPRECATED

   # ddcutil_1_0_gir_NAMESPACE=Ddcg
   # ddcutil_1_0_gir_VERSION=1.0

   ddcutil_1_0_gir_SCANNERFLAGS = --identifier-prefix=Ddcg \
        --symbol-prefix=ddcg \
        --warn-all \
        --add-include-path=$(srcdir) 
   # Colord_1_0_gir_SCANNERFLAGS = --identifier-prefix=Cd \
   #       --symbol-prefix=cd \
   #       --warn-all \
   #       --add-include-path=$(srcdir) \
   #       --c-include="colord.h"

  
   ddcutil_1_0_gir_EXPORT_PACKAGES  = ddcutil
   ddcutil_1_0_gir_LIBS             = ../libddcutil.la libddcgobj.la
   ddcutil_1_0_gir_FILES            = $(introspection_sources)

   INTROSPECTION_GIRS += ddcutil-1.0.gir

   girdir = $(datadir)/gir-1.0
   # dist_gir_DATA=$(INTROSPECTION_GIRS)
   gir_DATA = $(INTROSPECTION_GIRS)

   typelibdir = $(libdir)/girepository-1.0
   typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

   CLEANFILES += $(gir_DATA) $(typelib_DATA)
   # CLEANFILES += $(gir_DATA) $(typelib_DATA) *.log *.trs *.test
   
endif
endif

