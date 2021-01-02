## Process this file with automake to produce Makefile.in

SUBDIRS = util usb_util base vcp i2c adl usb dynvcp ddc test app_sysenv cmdline . gobject_api swig cython cffi  sample_clients 

MOSTLYCLEANFILES =   

CLEANFILES =

DISTCLEANFILES = publc/ddcutil_macros.h


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


#
# Intermediate Libraries
#

# Convenience library containing code shared between ddcutil executable and libddcutil shared library
noinst_LTLIBRARIES = libcommon.la 

# Convenience library to collect code used only in ddcutil executable
noinst_LTLIBRARIES += libapp.la


# Public Shared Library
#

if ENABLE_SHARED_LIB_COND
lib_LTLIBRARIES = libddcutil.la
# noinst_LTLIBRARIES += libhack.la
endif

#
# Files only in ddcutil executable
#
ddcutil_SOURCES = \
app_ddcutil/main.c \
app_ddcutil/app_capabilities.c \
app_ddcutil/app_probe.c \
app_ddcutil/app_dynamic_features.c \
app_ddcutil/app_dumpload.c \
app_ddcutil/app_setvcp.c \
app_ddcutil/app_getvcp.c \
app_ddcutil/app_vcpinfo.c

if INCLUDE_TESTCASES_COND
ddcutil_SOURCES += app_testcase.c
endif

#
# Files only in libddcutil
#
libddcutil_la_SOURCES = \
   libmain/api_base.c \
   libmain/api_displays.c \
   libmain/api_metadata.c \
   libmain/api_feature_access.c \
   libmain/api_capabilities.c 

# Despite what the Autotools book says, all-local is executed aftar all, not before 
# So this ensures that the NEXT execution of all will compile build_info.c 

# removed from all-local:
# 	touch base/build_info.c

all-local:
	@echo "Executing hook all-local..."

# Never executed:
# all-hook:
#	@echo "(src/Makefile) Executing all-hook..."
#	touch base/build_info.c

# fails
# base/build_info.o: .FORCE
#	@echo "Executing phony rule for build_info.o"
#	touch base/built_info.c


#
# libcommon contains the source files that are 
# shared between ddcutil and libddcutil: 
#
libcommon_la_SOURCES =      	

libapp_la_SOURCES =

if ENABLE_SHARED_LIB_COND
include_HEADERS = \
public/ddcutil_macros.h \
public/ddcutil_status_codes.h \
public/ddcutil_types.h \
public/ddcutil_c_api.h 
endif


# Notes: 
# 1) Without -prune option, the following line fails because list of file is archived internally, get file not found errors after remove
#	find $(top_distdir) -name ".deps" -exec rm -rf {} \;
# 2) Piping to xargs fails on if there are no files, which occurs if "make dist" is invoked without anything having bee built.
#    find $(top_distdir) -name ".deps" | xargs rm
# 3) Why explicit copy of .h files?
# 4) find statements in src/usb_util/Makefile.am find nothing, but those here work.  why?


dist-hook:
	@echo "(src/Makefile) Executing hook dist-hook. top_distdir=$(top_distdir) distdir=$(distdir).."
	find .  -type f | grep  \.h$ | xargs -i cp --parents "{}" $(distdir)
	find $(distdir) -name ".deps" -prune -exec rm -rf {} \;  
	find $(distdir) -name ".libs" -prune -exec rm -rf {} \;  
	find $(distdir) -name "*.lo"  -exec rm {} \;  
	find $(distdir) -name "*.o"   -exec rm {} \;  
	rm -rf $(distdir)/swig/pyenv
	rm -rf $(distdir)/swig/__pycache__
	rm -f ${distdir}/public/ddcutil_macros.h


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


#
# Compiler flags
#

# AM_CFLAGS = -Wall -Werror
AM_CFLAGS = -Wall
# AM_CFLAGS += -Wpedantic
# AM_CFLAGS += -Werror=implicit-function-declaration
AM_CFLAGS += -Wimplicit-function-declaration


if ENABLE_CALLGRAPH_COND
  AM_CFLAGS += -fdump-rtl-expand
endif

if ASAN_COND
  # -fsanitize=address required for both compile and link,  what about other -f options?
  AM_CFLAGS += -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g
  # does this go here or as and environment variable at runtime?
  # for -fsanitize-address-use-after-scope 
  ASAN_OPTIONS =  strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1
  ASAN_OPTIONS += verbosity=1
endif

# AM_CFLAGS += $(PYTHON_CPPFLAGS)

ddcutil_CFLAGS                 = $(AM_CFLAGS)  -fPIC

# unnessary, will use AM_CFLAGS if xxx_CFLAGS undefined
# libcommon_la_CFLAGS     = $(AM_CFLAGS)
# libddcutil_la_CFLAGS    = $(AM_CFLAGS)

  
#
# Link the libraries 
#
# Be careful about library ordering. 
# A library must be listed after any libraries that depend on it
#

libcommon_la_LIBADD = \
ddc/libddc.la    \
dynvcp/libdynvcp.la \
i2c/libi2c.la    \
adl/libadl.la    \
vcp/libvcp.la    \
base/libbase.la  \
util/libutil.la 

if ENABLE_USB_COND
libcommon_la_LIBADD += \
  usb_util/libusbutil.la \
  usb/libusb.la
endif

libapp_la_LIBADD = \
cmdline/libcmdline.la \
util/libutilaux.la

if ENABLE_ENVCMDS_COND
libapp_la_LIBADD  += \
app_sysenv/libappsysenv.la 
endif

if INCLUDE_TESTCASES_COND
libapp_la_LIBADD  += \
test/libtestcases.la
endif

if USE_X11_COND
  libcommon_la_LIBADD += $(LIBX11_LIBS)  $(XRANDR_LIBS) 
endif
if USE_LIBDRM_COND
  libapp_la_LIBADD += $(LIBDRM_LIBS) 
endif
# if ENABLE_YAML_COND
#   libcommon_la_LIBADD += $(YAML_LIBS)
# endif 

libcommon_la_LIBADD += \
  $(GLIB_LIBS)   \
  $(UDEV_LIBS)   \
  $(LIBUSB_LIBS)

# libddcutil_la_LIBADD = -lz
libddcutil_la_LIBADD = $(ZLIB_LIBS)

libddcutil_la_LIBADD +=  libcommon.la 

libddcutil_la_LDFLAGS = 
# Note -export-dynamic not required for failsim, don't need to append 
# libddcutil_la_LDFLAGS += -export-dynamic

libddcutil_la_LDFLAGS += -export-symbols-regex '(^DDCA_|^ddc[ags]_[^_])'
libddcutil_la_LDFLAGS += -version-info '@LT_CURRENT@:@LT_REVISION@:@LT_AGE@' 
libddcutil_la_LDFLAGS += -pie

if ASAN_COND
   libddcutil_la_LDFLAGS += -fsanitize=address
   # needed? 
   libddcutil_la_LDFLAGS += -fsanitize-address-use-after-scope -fno-omit-frame-pointer
endif

# doesn't prevent creation of .la 
# try disabling to create libddcutil.a - doesnt do it
libddcutil_la_LDFLAGS += --disable-static


#
# Link the executables
#

# ddcutil statically links libcommon, rather than using libddcutil.so
# so it needs LDADD references to external libraries as well

ddcutil_LDADD =   \
   libapp.la \
   libcommon.la 


#needed?
ddcutil_LDFLAGS =  
ddcutil_LDFLAGS += -pie
# -export-dynamic needed for failsim
ddcutil_LDFLAGS += -export-dynamic 

if ASAN_COND
   ddcutil_LDFLAGS += -fsanitize=address
   # needed? 
   ddcutil_LDFLAGS += -fsanitize-address-use-after-scope -fno-omit-frame-pointer
endif

# debug-install-hook:
#  	ls -ld $(DESTDIR)$(libdir)
# 	ls -l $(DESTDIR)$(libdir)/*la
# 	@echo "  pythondir       = $(pythondir)"
# 	@echo "  pyexecdir       = $(pyexecdir)"
# 	@echo  $(DESTDIR)$(libdir)


install-exec-hook:
	@echo "(src/Makefile) Executing install-exec-hook ..." 
	rm -f $(DESTDIR)$(libdir)/libddcutil.la  
	rm -f $(DESTDIR)$(libdir)/libddcutil.a  


#	@if [ -f $(DESTDIR)$(libdir)/libddcutil.la ] ; then \
#		rm $(DESTDIR)$(libdir)/libddcutil.la ; \
#	fi
#	@if [ -f $(DESTDIR)$(libdir)/libddcutil.a ] ; then \
#		rm $(DESTDIR)$(libdir)/libddcutil.a ; \
#	fi

#	@if [ -f $(DESTDIR)$(libdir)/libddcutil.la ] ; then \
#	  echo "(src/Makefile) install-exec-hook: Stripping dependency_libs from libddcutil.la" ; \
#	  sed -i "/dependency_libs/ s/'.*'/''/" $(DESTDIR)$(libdir)/libddcutil.la ; \
#	fi

# objdump -p $(DESTDIR)$(libdir)/libddcutil.so | sed -n -e's/^[[:space:]]*SONAME[[:space:]]*//p' |  sed -r -e's/([0-9])\.so\./\1-/; s/\.so(\.|$)//; y/_/-/; s/(.*)/\L&/'


uninstall-local:
	@echo "(src/Makefile:uninstall-local) Executing..."
	rm -f $(DESTDIR)$(libdir)/libddcutil*  
