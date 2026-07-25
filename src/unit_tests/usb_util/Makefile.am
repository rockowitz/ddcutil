# File src/unit_tests/usb_util/Makefile.am
# Unit tests for modules in src/usb_util.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC

# Copy the built test executables to src/unit_test_executables/usb_util
# after `make check` builds them here, so the compiled binaries land
# in a directory separate from these sources. Does not change how or
# where compilation/linking itself happens.
EXEC_OUTPUT_DIR = $(top_builddir)/src/unit_test_executables/usb_util

check-local: $(check_PROGRAMS)
	@mkdir -p $(EXEC_OUTPUT_DIR)
	@for p in $(check_PROGRAMS); do cp -f $$p $(EXEC_OUTPUT_DIR)/; done

clean-local:
	@for p in $(check_PROGRAMS); do rm -f $(EXEC_OUTPUT_DIR)/$$p; done

# Unit tests link the internal libusbutil/libbase/libutil convenience
# libraries directly, since these symbols are not exported from libddcutil.
# Run by "make check".
LIBUSBUTIL = $(top_builddir)/src/usb_util/libusbutil.la
LIBBASE    = $(top_builddir)/src/base/libbase.la
LIBUTIL    = $(top_builddir)/src/util/libutil.la

# libbase transitively pulls the udev/drm/dbus/acl util modules, and (via
# base modules' X11 fallbacks) X11/Xrandr.
EXTRA_SYSLIBS = $(UDEV_LIBS) $(LIBDRM_LIBS) $(DBUS_LIBS) $(ACL_LIBS) $(LIBUSB_LIBS)
if USE_X11_COND
EXTRA_SYSLIBS += $(LIBX11_LIBS) $(XEXT_LIBS) $(XRANDR_LIBS)
endif

check_PROGRAMS = test_usb_hid_common test_base_hid_report_descriptor \
  test_hid_report_descriptor test_hiddev_reports test_hiddev_util \
  test_hidraw_util test_libusb_util
TESTS = test_usb_hid_common test_base_hid_report_descriptor \
  test_hid_report_descriptor test_hiddev_reports test_hiddev_util \
  test_hidraw_util test_libusb_util

test_usb_hid_common_SOURCES  = test_usb_hid_common.c
test_usb_hid_common_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_usb_hid_common_LDADD    = $(LIBUSBUTIL) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_base_hid_report_descriptor_SOURCES  = test_base_hid_report_descriptor.c
test_base_hid_report_descriptor_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_base_hid_report_descriptor_LDADD    = $(LIBUSBUTIL) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_hid_report_descriptor_SOURCES  = test_hid_report_descriptor.c
test_hid_report_descriptor_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_hid_report_descriptor_LDADD    = $(LIBUSBUTIL) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_hiddev_reports_SOURCES  = test_hiddev_reports.c
test_hiddev_reports_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_hiddev_reports_LDADD    = $(LIBUSBUTIL) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_hiddev_util_SOURCES  = test_hiddev_util.c
test_hiddev_util_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_hiddev_util_LDADD    = $(LIBUSBUTIL) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_hidraw_util_SOURCES  = test_hidraw_util.c
test_hidraw_util_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_hidraw_util_LDADD    = $(LIBUSBUTIL) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_libusb_util_SOURCES  = test_libusb_util.c
test_libusb_util_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_libusb_util_LDADD    = $(LIBUSBUTIL) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm
