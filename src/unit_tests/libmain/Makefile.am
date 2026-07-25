# File src/unit_tests/libmain/Makefile.am
# Unit tests for modules in src/libmain.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC

# Copy the built test executables to src/unit_test_executables/libmain
# after `make check` builds them here, so the compiled binaries land
# in a directory separate from these sources. Does not change how or
# where compilation/linking itself happens.
EXEC_OUTPUT_DIR = $(top_builddir)/src/unit_test_executables/libmain

check-local: $(check_PROGRAMS)
	@mkdir -p $(EXEC_OUTPUT_DIR)
	@for p in $(check_PROGRAMS); do cp -f $$p $(EXEC_OUTPUT_DIR)/; done

clean-local:
	@for p in $(check_PROGRAMS); do rm -f $(EXEC_OUTPUT_DIR)/$$p; done

# src/libmain builds the intermediate library libsharedlib.la, which
# (together with libcommon.la) becomes libddcutil.so -- see
# libddcutil_la_LIBADD in the top-level src/Makefile.am, which this
# mirrors. Like src/unit_tests/ddc/dw/dynvcp, link the top-level libcommon
# convenience library plus the same external libraries
# ddcutil_LDADD/libcommon_la_LIBADD pull in.
LIBSHAREDLIB = $(top_builddir)/src/libmain/libsharedlib.la
LIBCOMMON    = $(top_builddir)/src/libcommon.la

EXTRA_SYSLIBS = $(ACL_LIBS) $(JANSSON_LIBS) $(UDEV_LIBS) $(LIBUSB_LIBS)
if USE_X11_COND
EXTRA_SYSLIBS += $(LIBX11_LIBS) $(XEXT_LIBS) $(XRANDR_LIBS)
endif
if USE_DBUS_COND
EXTRA_SYSLIBS += $(DBUS_LIBS)
endif
if USE_SYSTEMD_COND
EXTRA_SYSLIBS += $(SYSTEMD_LIBS)
endif
if USE_LIBDRM_COND
EXTRA_SYSLIBS += $(LIBDRM_LIBS)
endif

check_PROGRAMS = test_api_error_info_internal test_api_base test_api_metadata \
  test_api_capabilities test_api_feature_access test_api_displays
TESTS = test_api_error_info_internal test_api_base test_api_metadata \
  test_api_capabilities test_api_feature_access test_api_displays

test_api_error_info_internal_SOURCES  = test_api_error_info_internal.c
test_api_error_info_internal_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_api_error_info_internal_LDADD    = $(LIBSHAREDLIB) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_api_base_SOURCES  = test_api_base.c
test_api_base_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_api_base_LDADD    = $(LIBSHAREDLIB) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_api_metadata_SOURCES  = test_api_metadata.c
test_api_metadata_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_api_metadata_LDADD    = $(LIBSHAREDLIB) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_api_capabilities_SOURCES  = test_api_capabilities.c
test_api_capabilities_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_api_capabilities_LDADD    = $(LIBSHAREDLIB) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_api_feature_access_SOURCES  = test_api_feature_access.c
test_api_feature_access_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_api_feature_access_LDADD    = $(LIBSHAREDLIB) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_api_displays_SOURCES  = test_api_displays.c
test_api_displays_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_api_displays_LDADD    = $(LIBSHAREDLIB) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm
