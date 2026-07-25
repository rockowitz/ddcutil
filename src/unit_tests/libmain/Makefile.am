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

check_PROGRAMS = ../../unit_test_executables/libmain/test_api_error_info_internal ../../unit_test_executables/libmain/test_api_base ../../unit_test_executables/libmain/test_api_metadata \
  ../../unit_test_executables/libmain/test_api_capabilities ../../unit_test_executables/libmain/test_api_feature_access ../../unit_test_executables/libmain/test_api_displays
TESTS = ../../unit_test_executables/libmain/test_api_error_info_internal ../../unit_test_executables/libmain/test_api_base ../../unit_test_executables/libmain/test_api_metadata \
  ../../unit_test_executables/libmain/test_api_capabilities ../../unit_test_executables/libmain/test_api_feature_access ../../unit_test_executables/libmain/test_api_displays

______unit_test_executables_libmain_test_api_error_info_internal_SOURCES  = test_api_error_info_internal.c
______unit_test_executables_libmain_test_api_error_info_internal_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_libmain_test_api_error_info_internal_LDADD    = $(LIBSHAREDLIB) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

______unit_test_executables_libmain_test_api_base_SOURCES  = test_api_base.c
______unit_test_executables_libmain_test_api_base_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_libmain_test_api_base_LDADD    = $(LIBSHAREDLIB) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

______unit_test_executables_libmain_test_api_metadata_SOURCES  = test_api_metadata.c
______unit_test_executables_libmain_test_api_metadata_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_libmain_test_api_metadata_LDADD    = $(LIBSHAREDLIB) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

______unit_test_executables_libmain_test_api_capabilities_SOURCES  = test_api_capabilities.c
______unit_test_executables_libmain_test_api_capabilities_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_libmain_test_api_capabilities_LDADD    = $(LIBSHAREDLIB) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

______unit_test_executables_libmain_test_api_feature_access_SOURCES  = test_api_feature_access.c
______unit_test_executables_libmain_test_api_feature_access_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_libmain_test_api_feature_access_LDADD    = $(LIBSHAREDLIB) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

______unit_test_executables_libmain_test_api_displays_SOURCES  = test_api_displays.c
______unit_test_executables_libmain_test_api_displays_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_libmain_test_api_displays_LDADD    = $(LIBSHAREDLIB) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm
