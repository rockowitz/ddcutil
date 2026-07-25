# File src/unit_tests/usb/Makefile.am
# Unit tests for modules in src/usb.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC

# Copy the built test executables to src/unit_test_executables/usb
# after `make check` builds them here, so the compiled binaries land
# in a directory separate from these sources. Does not change how or
# where compilation/linking itself happens.
EXEC_OUTPUT_DIR = $(top_builddir)/src/unit_test_executables/usb

check-local: $(check_PROGRAMS)
	@mkdir -p $(EXEC_OUTPUT_DIR)
	@for p in $(check_PROGRAMS); do cp -f $$p $(EXEC_OUTPUT_DIR)/; done

clean-local:
	@for p in $(check_PROGRAMS); do rm -f $(EXEC_OUTPUT_DIR)/$$p; done

# Like src/unit_tests/ddc/dw/dynvcp, usb/*.c files cross-reference the
# rest of the ddcutil core (i2c, vcp, base, util) extensively, so there is
# no small per-directory library to link against. Link the top-level
# libcommon convenience library -- the same aggregate the ddcutil
# executable itself links -- plus the same external libraries
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

check_PROGRAMS = test_usb_base test_usb_displays test_usb_services
TESTS = test_usb_base test_usb_displays test_usb_services

test_usb_base_SOURCES  = test_usb_base.c
test_usb_base_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_usb_base_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_usb_displays_SOURCES  = test_usb_displays.c
test_usb_displays_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_usb_displays_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_usb_services_SOURCES  = test_usb_services.c
test_usb_services_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_usb_services_LDADD    = $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm
