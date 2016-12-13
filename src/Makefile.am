## Process this file with automake to produce Makefile.in

SUBDIRS = . gobject_api swig 

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

CLEANFILES =
# adl/*expand               \
# adl/adl_impl/*expand      \
# adl/adl_mock_impl/*expand \
# app_ddcutil/*expand       \
# base/*expand              \
# cmdline/*expand           \
# ddc/*expand               \
# i2c/*expand               \
# libmain/*expand           \
# sample_clients/*expand    \
# test/*expand              \
# test/adl/*expand          \
#  test/ddc/*expand          \
# test/i2c/*expand          \
# usb/*expand               \
# usb_util/*expand          \
# util/*expand              \
# vcp/*expand


# todo: factor out swig 
# todo: fails if swig/.libs doesn't exist
# rm -rf swig/ddc_swig_wrap.c swig/ddc_swig.py swig/ddc_swig.pyc
# Plo files in deps directories are automatically included in generated Makefiles for some reason
# test -z `find . -name ".deps"   -type d` || rm -rf `find . -name ".deps"   -type d`
# For conditionally included files, e.g. those in usb_util, make clean does not delete 
# the .lo files.  Hence the instructions here to delete all of them
# Fails: test: too many arguments:
# test -z `find . -name "*lo" -type f` || rm `find . -name "*lo" -type f`

clean-local: 
	@echo "(src/Makefile) clean-local"
	find . -name "*plist" -type d  -exec ls -ld {} \;
	rm -rf `find . -name "*plist" -type d`
	find . -name ".libs"   -type d  -exec ls -ld {} \;
	test -z `find . -name "*expand" -type f` || rm `find . -name "*expand" -type f`
	find . -name "*.lo" -type f -exec ls -l {} \;
	find . -name "*.lo" -type f -exec rm -f {} \;
	find . -name "*.o" -type f -exec ls -l {} \;
	find . -name "*.o" -type f -exec rm -f {} \;


mostlyclean-local:
	@echo "(src/Makefile) mostlyclean-local"

distclean-local:
	@echo "(src/Makefile) distclean-local"

maintainerclean-local:
	@echo "(src/Makefile) maintainerclean-local"

#
# Execuatables 
#

bin_PROGRAMS = \
ddcutil  

check_PROGRAMS =

if ENABLE_SHARED_LIB_COND
# Sample C client program for shared library:
check_PROGRAMS += \
laclient 
endif


#
# Intermediate Libraries
#

# Convenience library containing code shared between ddcutil executable and libddcutil shared library
noinst_LTLIBRARIES = libcommon.la 


#
# Public Shared Libraries
#

if ENABLE_SHARED_LIB_COND
lib_LTLIBRARIES = libddcutil.la
endif


#
# Files only in ddcutil executable:
#
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


#
# Files only in laclient executable:
#
laclient_SOURCES = \
sample_clients/clmain.c


# files only in libddcutil:
libddcutil_la_SOURCES = \
libmain/ddcutil_c_api.c  




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
libcommon_la_SOURCES =      	\
base/base_services.c        	\
base/build_info.c           	\
base/core.c                 	\
base/ddc_errno.c            	\
base/ddc_packets.c          	\
base/displays.c             	\
base/execution_stats.c      	\
base/feature_sets.c         	\
base/linux_errno.c          	\
base/sleep.c                	\
base/status_code_mgt.c      	\
base/vcp_version.c          	\
ddc/ddc_displays.c          	\
ddc/ddc_dumpload.c          	\
ddc/ddc_edid.c              	\
ddc/ddc_multi_part_io.c     	\
ddc/ddc_output.c            	\
ddc/ddc_packet_io.c         	\
ddc/ddc_read_capabilities.c 	\
ddc/ddc_services.c          	\
ddc/ddc_strategy.c          	\
ddc/ddc_vcp.c               	\
ddc/ddc_vcp_version.c       	\
ddc/try_stats.c             	\
i2c/i2c_base_io.c          	    \
i2c/i2c_bus_core.c          	\
i2c/i2c_do_io.c    	        	\
util/data_structures.c	    	\
util/debug_util.c               \
util/device_id_util.c       	\
util/edid.c 		        	\
util/file_util.c				\
util/glib_util.c		        \
util/multi_level_map.c 		    \
util/output_sink.c 				\
util/report_util.c 				\
util/string_util.c 				\
util/subprocess_util.c			\
util/x11_util.c 				\
vcp/ddc_command_codes.c			\
vcp/parse_capabilities.c 		\
vcp/parsed_capabilities_feature.c \
vcp/vcp_feature_codes.c 		\
vcp/vcp_feature_set.c 			\
vcp/vcp_feature_values.c 	

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

if ENABLE_FAILSIM_COND
libcommon_la_SOURCES += \
  util/failsim.c        
endif


include_HEADERS = \
public/ddcutil_types.h \
public/ddcutil_c_api.h 



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


#
# C Pre-Processor Flags
#

# GLIB_CFLAGS contains output of pkgconfig --cflags glib-2.0
AM_CPPFLAGS=     \
$(GLIB_CFLAGS)   \
$(XRANDR_CFLAGS) \
$(LIBUSB_CFLAGS) \
-I$(srcdir)      \
-I$(srcdir)/public

# # temporarily hardcode PYTHON_CPPFLAGS here
# AM_CPPFLAGS += \
# $(PYTHON_CPPGLAGS)


# AM_CPPFLAGS += \
# $(HIDAPI_CFLAGS)

if HAVE_ADL_COND
AM_CPPFLAGS +=               \
-I@ADL_HEADER_DIR@  
endif


#
# Compiler flags
#

# AM_CFLAGS = -Wall -Werror
AM_CFLAGS = -Wall
if ENABLE_CALLGRAPH_COND
  AM_CFLAGS += -fdump-rtl-expand
endif

AM_CFLAGS += $(PYTHON_CPPFLAGS)

ddcutil_CFLAGS          = $(AM_CFLAGS)
laclient_CFLAGS         = $(AM_CFLAGS)
libcommon_la_CFLAGS     = $(AM_CFLAGS)
libddcutil_la_CFLAGS    = $(AM_CFLAGS)

  
#
# Link the libraries 
#
# Be careful about library ordering 
# library must be listed after any libraries that depend on it
#

# deleted $(HIDAPI_LIBS)
libddcutil_la_LIBADD = -lz
libddcutil_la_LIBADD +=  \
   libcommon.la   \
   $(X11_LIBS) \
   $(XRANDR_LIBS) \
   $(GLIB_LIBS) 

if ENABLE_USB_COND
libddcutil_la_LIBADD += \
   $(UDEV_LIBS) \
   $(LIBUSB_LIBS)
endif

# temp
libddcutil_la_LIBADD += $(PYTHON_LIBS) -lpython2.7

libddcutil_la_LDFLAGS = -export_symbols-regex '^ddc[tgs]_[^_]'
# libddcutil_la_LDFLAGS += -version-info '0:1:1'
libddcutil_la_LDFLAGS += -export-dynamic 

# needed?
libcommon_la_LDFLAGS = -export-dynamic 

#
# Link the executables
#
#
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


#needed?
ddcutil_LDFLAGS = -export-dynamic

# Fails if not linked with libxrandr - why?
laclient_LDADD = \
  libddcutil.la \
  $(XRANDR_LIBS) 


install-exec-hook:
	@echo "(install-exec-hook) Executing..." 
	@echo "  pythondir       = $(pythondir)"
	@echo "  pyexecdir       = $(pyexecdir)"


# These includes are at the Makefile.am level:
# include make_swig.am
# include make_gobject.am
