# File src/unit_tests/dynvcp/Makefile.am
# Unit tests for modules in src/dynvcp.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC

# Like src/unit_tests/ddc and src/unit_tests/dw, dynvcp/*.c files
# cross-reference vcp/base/ddc extensively, so there is no small
# per-directory library to link against. Link the top-level libcommon
# convenience library -- the same aggregate the ddcutil executable itself
# links -- plus the same external libraries ddcutil_LDADD/
# libcommon_la_LIBADD pull in (see top-level src/Makefile.am).
LIBCOMMON = $(top_builddir)/src/libcommon.la

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

check_PROGRAMS = test_vcp_feature_set test_dyn_feature_set \
  test_dyn_feature_codes test_dyn_feature_files test_dyn_parsed_capabilities
TESTS = test_vcp_feature_set test_dyn_feature_set \
  test_dyn_feature_codes test_dyn_feature_files test_dyn_parsed_capabilities

test_vcp_feature_set_SOURCES  = test_vcp_feature_set.c
test_vcp_feature_set_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_vcp_feature_set_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_dyn_feature_set_SOURCES  = test_dyn_feature_set.c
test_dyn_feature_set_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_dyn_feature_set_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_dyn_feature_codes_SOURCES  = test_dyn_feature_codes.c
test_dyn_feature_codes_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_dyn_feature_codes_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_dyn_feature_files_SOURCES  = test_dyn_feature_files.c
test_dyn_feature_files_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_dyn_feature_files_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_dyn_parsed_capabilities_SOURCES  = test_dyn_parsed_capabilities.c
test_dyn_parsed_capabilities_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_dyn_parsed_capabilities_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm
