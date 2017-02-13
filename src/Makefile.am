## Process this file with automake to produce Makefile.in

SUBDIRS = util usb_util  . gobject_api swig 

MOSTLYCLEANFILES =   

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
	find . -name ".deps" -type d 
	find . -name ".deps" -type d -exec ls -ld  {} \;
	find . -path "*/.deps/*" -exec ls -l {} \;    



mostlyclean-local:
	@echo "(src/Makefile) mostlyclean-local"

distclean-local:
	@echo "(src/Makefile) distclean-local"
	find . -name ".libs"   -type d  -exec ls -ld {} \;

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
# Files only in ddcutil executable
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

if ENABLE_USB_COND
ddcutil_SOURCES += \
   app_ddcutil/query_usb_sysenv.c
endif

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

if USE_LIBDRM_COND
ddcutil_SOURCES += \
  util/libdrm_util.c \
  app_ddcutil/query_drm_sysenv.c
endif

#
# Files only in laclient executable
#
laclient_SOURCES = \
sample_clients/clmain.c

#
# Files only in libddcutil
#
libddcutil_la_SOURCES = \
libmain/ddcutil_c_api.c  




# Despite what the Autotools book says, all-local is executed aftar all, not before 
# So this ensures that the NEXT execution of all will compile build_info.c 

# removed from all-local:
# 	touch base/build_info.c


all-local:
	@echo "Executing hook all-local..."

# Never executed:
# all-hook:
#	@echo "Executing hook all-hook..."
#	touch base/build_info.c

# fails
# base/build_info.o: .FORCE
#	@echo "Executing phony rule for build_info.o"
#	touch base/built_info.c


#
# libcommon contains the source files that are 
# shared between ddcutil and libddcutil: 
#
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
  usb/usb_vcp.c  
endif


if ENABLE_SHARED_LIB_COND
include_HEADERS = \
public/ddcutil_types.h \
public/ddcutil_c_api.h 
endif


# Notes: 
# 1) Following line fails because list of file is archived internally, get file not found errors after remove
#	find $(top_distdir) -name ".deps" -exec rm -rf {} \;
#   Use pipe to xargs rm -rf instead 
# 2) Why explicit copy of .h files?
# 3) find statements in src/usb_util/Makefile.am find nothing, but those here work.  why?


dist-hook:
	@echo "(src/Makefile) Executing hook dist-hook.top_distdir=$(top_distdir) distdir=$(distdir).."
	find .  -type f | grep  \.h$ | xargs -i cp --parents "{}" $(distdir)
	find $(distdir) -name ".deps" | xargs rm -rf 
	find $(distdir) -name ".libs" | xargs rm -rf 
	find $(distdir) -name "*.lo"  | xargs rm 
	find $(distdir) -name "*.o"   | xargs rm
	rm -rf $(distdir)/swig/pyenv
	rm -rf $(distdir)/swig/__pycache__




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

AM_CPPFLAGS += \
$(LIBDRM_CFLAGS)


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

# AM_CFLAGS += $(PYTHON_CPPFLAGS)

ddcutil_CFLAGS          = $(AM_CFLAGS)  -fPIC
laclient_CFLAGS         = $(AM_CFLAGS)  -fPIC

# unneessary, will use AM_CFLAGS if xxx_CFLAGS undefined
# libcommon_la_CFLAGS     = $(AM_CFLAGS)
# libddcutil_la_CFLAGS    = $(AM_CFLAGS)

  
#
# Link the libraries 
#
# Be careful about library ordering. 
# A library must be listed after any libraries that depend on it
#

libcommon_la_LIBADD = \
util/libutil.la 

if ENABLE_USB_COND
libcommon_la_LIBADD += usb_util/libusbutil.la
endif


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


libddcutil_la_LDFLAGS = 
# -export-dynamic not required for failsim: 
# if ENABLE_FAILSIM_COND
# libddcutil_la_LDFLAGS += -export-dynamic
# else
libddcutil_la_LDFLAGS += -export-symbols-regex '^ddc[ags]_[^_]'
# endif
libddcutil_la_LDFLAGS += -version-info '@LT_CURRENT@:@LT_REVISION@:@LT_AGE@' 
# libddcutil_la_LDFLAGS += -init _ddca_init
libddcutil_la_LDFLAGS += -pie


#
# Link the executables
#

# ddcutil statically links libcommon, rather than using libddcutil.so
# so it needs LDADD references to external libraries as well

ddcutil_LDADD =   \
   libcommon.la   \
   $(XRANDR_LIBS) \
   $(X11_LIBS)    \
   $(GLIB_LIBS)

ddcutil_LDADD += \
   $(UDEV_LIBS)

if ENABLE_USB_COND
ddcutil_LDADD += \
   $(LIBUSB_LIBS)
endif

ddcutil_LDADD += \
  $(LIBDRM_LIBS)

#needed?
ddcutil_LDFLAGS =  
ddcutil_LDFLAGS += -pie
# -export-dynamic needed for failsim
ddcutil_LDFLAGS += -export-dynamic 

# Fails if not linked with libxrandr - why?
laclient_LDADD = \
  libddcutil.la 
#laclient_LDADD += $(XRANDR_LIBS) 

laclient_LDFLAGS = -pie

# debug-install-hook:
#  	ls -ld $(DESTDIR)$(libdir)
# 	ls -l $(DESTDIR)$(libdir)/*la
# 	@echo "  pythondir       = $(pythondir)"
# 	@echo "  pyexecdir       = $(pyexecdir)"
# 	@echo  $(DESTDIR)$(libdir)

install-exec-hook:
	@echo "(install-exec-hook) Executing..." 
	@if [ -f $(DESTDIR)$(libdir)/libddcutil.la ] ; then \
	  echo "(install-exec-hook) Stripping dependency_libs from libddcutil.la" ; \
	  sed -i "/dependency_libs/ s/'.*'/''/" $(DESTDIR)$(libdir)/libddcutil.la ; \
	fi

# objdump -p $(DESTDIR)$(libdir)/libddcutil.so | sed -n -e's/^[[:space:]]*SONAME[[:space:]]*//p' |  sed -r -e's/([0-9])\.so\./\1-/; s/\.so(\.|$)//; y/_/-/; s/(.*)/\L&/'

