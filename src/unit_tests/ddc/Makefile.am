# File src/unit_tests/ddc/Makefile.am
# Unit tests for modules in src/ddc.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC

# Copy the built test executables to src/unit_test_executables/ddc
# after `make check` builds them here, so the compiled binaries land
# in a directory separate from these sources. Does not change how or
# where compilation/linking itself happens.
EXEC_OUTPUT_DIR = $(top_builddir)/src/unit_test_executables/ddc

check-local: $(check_PROGRAMS)
	@mkdir -p $(EXEC_OUTPUT_DIR)
	@for p in $(check_PROGRAMS); do cp -f $$p $(EXEC_OUTPUT_DIR)/; done

clean-local:
	@for p in $(check_PROGRAMS); do rm -f $(EXEC_OUTPUT_DIR)/$$p; done

# Unlike the other src/unit_tests/* directories, ddc/*.c files
# cross-reference each other and the rest of the ddcutil core (i2c,
# dynvcp, sysfs, vcp, base, util, and conditionally usb/dw) extensively,
# so there is no small per-directory library to link against. Instead,
# link the top-level libcommon convenience library -- the same aggregate
# the ddcutil executable itself links -- plus the same external libraries
# ddcutil_LDADD/libcommon_la_LIBADD pull in (see top-level src/Makefile.am).
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

check_PROGRAMS = test_ddc_try_data test_ddc_vcp_version \
  test_ddc_phantom_displays test_ddc_display_selection \
  test_ddc_serialize test_ddc_dumpload
TESTS = test_ddc_try_data test_ddc_vcp_version \
  test_ddc_phantom_displays test_ddc_display_selection \
  test_ddc_serialize test_ddc_dumpload

test_ddc_try_data_SOURCES  = test_ddc_try_data.c
test_ddc_try_data_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_ddc_try_data_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_ddc_vcp_version_SOURCES  = test_ddc_vcp_version.c
test_ddc_vcp_version_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_ddc_vcp_version_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_ddc_phantom_displays_SOURCES  = test_ddc_phantom_displays.c
test_ddc_phantom_displays_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_ddc_phantom_displays_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_ddc_display_selection_SOURCES  = test_ddc_display_selection.c
test_ddc_display_selection_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_ddc_display_selection_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_ddc_serialize_SOURCES  = test_ddc_serialize.c
test_ddc_serialize_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(JANSSON_CFLAGS)
test_ddc_serialize_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_ddc_dumpload_SOURCES  = test_ddc_dumpload.c
test_ddc_dumpload_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_ddc_dumpload_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm
