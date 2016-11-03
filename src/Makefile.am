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
# ddcutil_app/.deps/*          \
# test/ddc/.deps/*      \
# test/i2c/.deps/*      \
# test/.deps/*          
# util/.deps/*          \
# adl/.deps/*           \
# adl/adl_impl/.deps/*  \
# adl/adl_mock_impl/.deps/*  

CLEANFILES =\
adl/*expand               \
adl/adl_impl/*expand      \
adl/adl_mock_impl/*expand \
app_ddcutil/*expand       \
base/*expand              \
cmdline/*expand           \
ddc/*expand               \
i2c/*expand               \
libmain/*expand           \
sample_clients/*expand    \
test/*expand              \
test/adl/*expand          \
test/ddc/*expand          \
test/i2c/*expand          \
usb/*expand               \
usb_util/*expand          \
util/*expand              \
vcp/*expand


# todo: factor out swig 
clean-local: 
	@echo "(clean-local)"
	find . -name "*plist" -type d  -exec ls -ld {} \;
	rm -rf `find . -name "*plist" -type d`
	find . -name ".libs"  -type d  -exec ls -ld {} \;
	find . -name ".libs"  -type d  -exec rm -rf {} \;
	rm -rf swig/ddct_public_wrap.c ddct_public.py ddct_public.pyc



bin_PROGRAMS = \
ddcutil  

if ENABLE_SHARED_LIB_COND
# Sample client program for shared library:
bin_PROGRAMS += \
laclient 

endif



# Convenience library containing code shared between ddcutil and libddcutil
noinst_LTLIBRARIES = libcommon.la  libsharedbase.la

if ENABLE_SHARED_LIB_COND
# Shared library
lib_LTLIBRARIES = libddcutil.la
endif

# Files only in ddcutil:
ddcutil_SOURCES = \
cmdline/cmd_parser_aux.c \
cmdline/cmd_parser_goption.c \
cmdline/parsed_cmd.c \
app_ddcutil/main.c \
app_ddcutil/app_dumpload.c \
app_ddcutil/query_sysenv.c \
app_ddcutil/app_setvcp.c \
app_ddcutil/app_getvcp.c \
app_ddcutil/testcases.c 


if INCLUDE_TESTCASES_COND
ddcutil_SOURCES += \
   test/ddc/ddc_capabilities_tests.c \
   test/ddc/ddc_vcp_tests.c \
   test/i2c/i2c_edid_tests.c \
   test/i2c/i2c_io_old.c \
   test/testcase_table.c
else
ddcutil_SOURCES += \
   app_ddcutil/testcase_mock_table.c
endif




# files only in libddcutil:
libsharedbase_la_SOURCES = \
libmain/ddct_public.c  



# files only in laclient:
laclient_SOURCES = \
sample_clients/clmain.c


# Despite what the Autotools book says, all-local is executed aftar all, not before 
# So this ensures that the NEXT execution of all will compile build_info.c 
all-local:
	@echo "Executing hook all-local..."
	touch base/build_info.c

# Never executed:
# all-hook:
#	@echo "Executing hook all-hook..."
#	touch base/build_info.c

# fails
# base/build_info.o: .FORCE
#	@echo "Executing phony rule for build_info.o"
#	touch base/built_info.c


# libcommon contains the source files that are 
# shared between ddcutil and libddcutil: 
libcommon_la_SOURCES = \
base/build_info.c \
base/base_services.c \
base/core.c \
base/ddc_errno.c \
base/ddc_packets.c \
base/displays.c \
base/execution_stats.c \
base/linux_errno.c \
base/status_code_mgt.c \
base/sleep.c \
base/vcp_version.c \
base/feature_sets.c \
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
ddc/ddc_dumpload.c \
util/data_structures.c \
util/edid.c \
util/file_util.c  \
util/glib_util.c \
util/multi_level_map.c \
util/output_sink.c \
util/device_id_util.c \
util/report_util.c \
util/string_util.c \
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

if ENABLE_USB_COND
libcommon_la_SOURCES += \
  usb/usb_base.c \
  usb/usb_edid.c \
  usb/usb_displays.c  \
  usb/usb_vcp.c  \
  usb_util/usb_hid_common.c \
  usb_util/hiddev_reports.c \
  usb_util/hiddev_util.c \
  usb_util/hidraw_util.c \
  usb_util/libusb_reports.c \
  usb_util/libusb_util.c \
  usb_util/base_hid_report_descriptor.c \
  usb_util/hid_report_descriptor.c \
  util/udev_util.c 
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
$(XRANDR_CFLAGS) \
$(LIBUSB_CFLAGS) \
-I$(top_srcdir) 


# AM_CPPFLAGS += \
# $(HIDAPI_CFLAGS)

# Compiler flags
ddcutil_CFLAGS          = $(AM_CFLAGS)
laclient_CFLAGS         = $(AM_CFLAGS)
libcommon_la_CFLAGS     = $(AM_CFLAGS)
libsharedbase_la_CFLAGS = $(AM_CFLAGS)
libddcutil_la_CFLAGS    = $(AM_CFLAGS)

  
if HAVE_ADL_COND
AM_CPPFLAGS +=               \
   -I@ADL_HEADER_DIR@  
endif


# Link the libraries 
# $(GLIB_LIBS) contains output of pkgconfig --libs glib-2.0

# Be careful about library ordering 
# library must be listed after any libraries that depend on it

# deleted $(HIDAPI_LIBS)
# Link the shared library
libsharedbase_la_LIBADD =  \
   libcommon.la   \
   $(X11_LIBS) \
   $(XRANDR_LIBS) \
   $(GLIB_LIBS) 

if ENABLE_USB_COND
libsharedbase_la_LIBADD += \
   $(UDEV_LIBS) \
   $(LIBUSB_LIBS)
endif


libddcutil_la_LIBADD = -lz
libddcutil_la_SOURCES =
# libsharedbase_la_SOURCES =
libddcutil_la_LIBADD += libsharedbase.la

libddcutil_la_LDFLAGS = -export_symbols-regex '^ddc[tg]_[^_]'
# libddcutil_la_LDFLAGS += -version-info '0:1:1'


# Link the executables

ddcutil_LDADD = \
   libcommon.la \
   $(XRANDR_LIBS) \
   $(X11_LIBS) \
   $(GLIB_LIBS)

if ENABLE_USB_COND
ddcutil_LDADD += \
   $(LIBUSB_LIBS) \
   $(UDEV_LIBS) 
endif





# Fails if not linked with libxrandr - why?
laclient_LDADD = \
  libddcutil.la \
  $(XRANDR_LIBS) 


# These includes are at the Makefile.am level:
include make_swig_am
include make_gobject_am
