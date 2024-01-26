# src/Makefile.am

DIST_SUBDIRS = app_ddcutil app_sysenv base cmdline ddc dynvcp i2c libmain  sample_clients test usb usb_util util vcp
# contain only header files:
EXTRA_DIST = bsd private public

SUBDIRS = util usb_util base vcp i2c usb dynvcp ddc
if INCLUDE_TESTCASES_COND
SUBDIRS += test
endif 
if ENABLE_SHARED_LIB_COND
SUBDIRS += libmain
endif
SUBDIRS += app_sysenv app_ddcutil cmdline .  sample_clients

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

# distclean-local => clean-local => mostlyclean->local

mostlyclean-local:
	@echo "(src/Makefile) mostlyclean-local"

clean-local:
	@echo "(src/Makefile) clean-local"
	find . -name "*plist" -type d  -exec ls -ld {} \;
	rm -rf `find . -name "*plist" -type d`
	find . -name ".libs"   -type d  -exec ls -ld {} \;
	test -z `find . -name "*expand" -type f` || rm `find . -name "*expand" -type f`
	find . -name "*.lo" -type f -exec rm -fv {} \;
	find . -name "*.o" -type f -exec rm -fv {} \;
	find . -name ".deps" -type d -exec ls -ld  {} \;
	find . -path "*/.deps/*" -exec ls -l {} \;    

distclean-local:
	@echo "(src/Makefile) distclean-local"
	find . -name ".libs"   -type d  -exec ls -ld {} \;
	rm -rf `find . -name "*plist" -type d`

maintainerclean-local:
	@echo "(src/Makefile) maintainerclean-local"


#
# Execuatables 
#

bin_PROGRAMS = ddcutil  
ddcutil_SOURCES = 

if ENABLE_SHARED_LIB_COND
lib_LTLIBRARIES = libddcutil.la
libddcutil_la_SOURCES =
endif

#
# Intermediate libraries built in this directory
#

# Convenience library containing code shared between ddcutil executable and libddcutil shared library
noinst_LTLIBRARIES = libcommon.la
libcommon_la_SOURCES = 


# Convenience library to collect code used only in ddcutil executable
noinst_LTLIBRARIES += libapp.la
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
# 2) Piping to xargs fails if there are no files, which occurs if "make dist" is invoked without anything having been built.
#    find $(top_distdir) -name ".deps" | xargs rm
# 3) Why explicit copy of .h files?
# 4) find statements in src/usb_util/Makefile.am find nothing, but those here work.  why?

dist-hook:
	@echo "(src/Makefile) Executing hook dist-hook. top_distdir=$(top_distdir) distdir=$(distdir).."
	find .  -type f | grep  \.h$ | xargs -i cp --parents "{}" $(distdir)
	find $(distdir) -name ".deps" -type d -prune -exec rm -rfv {} \;  
	find $(distdir) -name ".libs" -type d -prune -exec rm -rfv {} \;  
	find $(distdir) -name "*.lo"  -exec rm -v {} \;  
	find $(distdir) -name "*.o"   -exec rm -v {} \;  
	rm -rfv $(distdir)/swig/pyenv
	rm -rfv $(distdir)/swig/__pycache__
	rm -fv ${distdir}/public/ddcutil_macros.h
	find ${distdir} -name adl_archived -type d -prune -exec rm -rfv {} \;
	find ${distdir}  -name "*old"  -type d -prune -exec rm -rfv {} \;
	find ${distdir}  -name "*new"  -type d -prune -exec rm -rfv {} \;
	
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

# unused-result defined since at least gcc 5
AM_CFLAGS = -Wall -std=c11 -Werror=unused-result

# Distributing -Werror is bad programming practice.  It can result in build failure
# on an external site having a different compiler, compiler version, or configuration. 
# See comments and answers in
# https://stackoverflow.com/questions/10275554/how-to-disable-specified-warning-for-specified-file-while-using-autotools
# 10/1/2022: now enabled by WARNINGS_ARE_ERRORS_COND in individual Makefile.am
# AM_CFLAGS += -Werror
# AM_CFLAGS += -Wpedantic

# -pedantic issues warnings re code that doesn't conform to ISO C
# In particular, -m modifier on sscanf is a POSIX extension, not ISO C
# Also flags PROGRAM_LOGIC_ERROR() 
# If combined with -Werror, will cause some module compilations to fail

# AM_CFLAGS += -H
# Report header file dependencies

AM_CFLAGS += -Wimplicit-function-declaration

# Will flag glib's use of _Static_assert, which is not in the C99 standard, 
# but which is in gcc as of 4.6, also other constructs
# AM_CFLAGS += -Wc99-c11-compat

# Option added in Clang 12, if set causes warnings in WITH_VALIDATED_DH*() macros etc.
# The option does not exist in gcc.
# The "-Wno-" option form is ignored by gcc, but should be respected when compiling
# in an environment, e.g. termux, where this option is enabled by default. 
AM_CFLAGS += -Wno-compound-token-split-by-macro

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

AM_CFLAGS += -fPIC

# unnessary, will use AM_CFLAGS if xxx_CFLAGS undefined
# ddcutil_CFLAGS          = $(AM_CFLAGS)
# libcommon_la_CFLAGS     = $(AM_CFLAGS)
# libddcutil_la_CFLAGS    = $(AM_CFLAGS)

# Makefile:420: *** Recursive variable 'AM_CFLAGS' references itself (eventually).  Stop.
# export AM_CFLAGS

AM_CFLAGS_STD=$(AM_CFLAGS)
export AM_CFLAGS_STD
  
#
# Link convenience library libcommon.la 
#
# Be careful about library ordering. 
# A library must be listed after any libraries that depend on it
#

libcommon_la_LIBADD = \
ddc/libddc.la    \
dynvcp/libdynvcp.la \
i2c/libi2c.la    \
vcp/libvcp.la    \
cmdline/libcmdline.la \
base/libbase.la  \
util/libutil.la 

if ENABLE_USB_COND
libcommon_la_LIBADD += \
  usb_util/libusbutil.la \
  usb/libusb.la
endif

if USE_X11_COND
  libcommon_la_LIBADD += $(LIBX11_LIBS) $(XEXT_LIBS)  $(XRANDR_LIBS) 
endif

# if ENABLE_YAML_COND
#   libcommon_la_LIBADD += $(YAML_LIBS)
# endif 

libcommon_la_LIBADD += \
  $(JANSSON_LIBS) \
  $(GLIB_LIBS)    \
  $(UDEV_LIBS)    \
  $(LIBUSB_LIBS) 

# for smbus functions:
# libcommon_la_LIBADD += -li2c

#
# Link convenience library libapp
#

libapp_la_LIBADD = \
app_ddcutil/libappddcutil.la

if ENABLE_ENVCMDS_COND
libapp_la_LIBADD  += \
app_sysenv/libappsysenv.la 
endif

if INCLUDE_TESTCASES_COND
libapp_la_LIBADD  += \
test/libtestcases.la
endif

if USE_LIBDRM_COND
  libcommon_la_LIBADD += $(LIBDRM_LIBS) 
endif


#
# Link ddcutil executable
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
   ddcutil_LDADD += -lasan
endif


#
# Link libddcutil executable
#

if ENABLE_SHARED_LIB_COND
# libddcutil_la_LIBADD = -lz
libddcutil_la_LIBADD = $(ZLIB_LIBS)

libddcutil_la_LIBADD +=  libcommon.la libmain/libsharedlib.la 

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
   libddcutil_la_LIBADD += -lasan
endif

# doesn't prevent creation of .la 
# try disabling to create libddcutil.a - doesnt do it
libddcutil_la_LDFLAGS += --disable-static

endif


# debug-install-hook:
#  	ls -ld $(DESTDIR)$(libdir)
# 	ls -l $(DESTDIR)$(libdir)/*la
# 	@echo "  pythondir       = $(pythondir)"
# 	@echo "  pyexecdir       = $(pyexecdir)"
# 	@echo  $(DESTDIR)$(libdir)


install-exec-local:
	@echo "(src/Makefile) Executing install-exec-local ..." 
	@echo "(src/Makefile) install-exec-local done" 


install-exec-hook:
	@echo "(src/Makefile) Executing install-exec-hook ..." 
	rm -f $(DESTDIR)$(libdir)/libddcutil.la  

#	rm -f $(DESTDIR)$(libdir)/libddcutil.a  

#	@if [ -f $(DESTDIR)$(libdir)/libddcutil.la ] ; then \
#	  echo "(src/Makefile) install-exec-hook: Stripping dependency_libs from libddcutil.la" ; \
#	  sed -i "/dependency_libs/ s/'.*'/''/" $(DESTDIR)$(libdir)/libddcutil.la ; \
#	fi
if INSTALL_LIB_ONLY_COND
	rm -f $(DESTDIR)${bindir}/ddcutil
	rm -f ${DESTDIR}${libdir}/libddcutil.so
endif

# objdump -p $(DESTDIR)$(libdir)/libddcutil.so | sed -n -e's/^[[:space:]]*SONAME[[:space:]]*//p' |  sed -r -e's/([0-9])\.so\./\1-/; s/\.so(\.|$)//; y/_/-/; s/(.*)/\L&/'

	@echo "(src/Makefile) Completed install-exec-hook" 

install-data-local:
	@echo "(src/Makefile) Executing install-data-local ..." 
	@echo "(src/Makefile) install-data-local done" 


install-data-hook:
	@echo "(src/Makefile) Executing install-data-hook ..." 
if INSTALL_LIB_ONLY_COND
	rm -f ${DESTDIR}${includedir}/ddcutil_macros.h
	rm -f ${DESTDIR}${includedir}/ddcutil_status_codes.h
	rm -f ${DESTDIR}${includedir}/ddcutil_types.h
	rm -f ${DESTDIR}${includedir}/ddcutil_c_api.h
endif
	@echo "(src/Makefile) install-data-hook done" 


uninstall-local:
	@echo "(src/Makefile:uninstall-local) Executing..."
	rm -f $(DESTDIR)$(libdir)/libddcutil*  
	
	
# Rename to "all=local" for development 
all-local-disabled:
	@echo ""
	@echo "(src/Makefile:all-local) Variable values:"
	@echo "   CLEANFILES:       $(CLEANFILES)"
	@echo "   CFLAGS:           $(CFLAGS)"
	@echo "   AM_CFLAGS:        $(AM_CFLAGS)"
	@echo "   CPPFLAGS:         $(CPPFLAGS)"
	@echo "   AM_CPPFLAGS:      $(AM_CPPFLAGS)"
	@echo "   ddcutil_FLAGS:    $(ddcutil_CFLAGS)"
	@echo "   AUTOMAKE_OPTIONS: $(AUTOMAKE_OPTIONS)"
	@echo "   MAKELEVEL:        $(MAKELEVEL)"
	@echo "   MAKEFLAGS:        $(MAKEFLAGS)"
	@echo "   V:                $(V)"
	@echo "   AM_CFLAGS_STD:    $(AM_CFLAGS_STD)"
	@echo ""

