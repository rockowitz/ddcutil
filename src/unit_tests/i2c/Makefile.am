# File src/unit_tests/i2c/Makefile.am
# Unit tests for modules in src/i2c.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC

# Copy the built test executables to src/unit_test_executables/i2c
# after `make check` builds them here, so the compiled binaries land
# in a directory separate from these sources. Does not change how or
# where compilation/linking itself happens.
EXEC_OUTPUT_DIR = $(top_builddir)/src/unit_test_executables/i2c

check-local: $(check_PROGRAMS)
	@mkdir -p $(EXEC_OUTPUT_DIR)
	@for p in $(check_PROGRAMS); do cp -f $$p $(EXEC_OUTPUT_DIR)/; done

clean-local:
	@for p in $(check_PROGRAMS); do rm -f $(EXEC_OUTPUT_DIR)/$$p; done

# Unit tests link the internal libi2c/libbase/libutil convenience libraries
# directly, since these symbols are not exported from libddcutil. Run by
# "make check".
LIBI2C  = $(top_builddir)/src/i2c/libi2c.la
LIBBASE = $(top_builddir)/src/base/libbase.la
LIBUTIL = $(top_builddir)/src/util/libutil.la

# libi2c transitively pulls the sysfs, udev, drm, dbus, and acl util modules
# (bus/EDID/connector inspection, driver-name lookup), so these tests link
# their libs.  libsysfs's DPMS fallback (sysfs_dpms.c) also pulls X11/Xrandr.
LIBSYSFS = $(top_builddir)/src/sysfs/libsysfs.la
EXTRA_SYSLIBS = $(UDEV_LIBS) $(LIBDRM_LIBS) $(DBUS_LIBS) $(ACL_LIBS)
if USE_X11_COND
EXTRA_SYSLIBS += $(LIBX11_LIBS) $(XEXT_LIBS) $(XRANDR_LIBS)
endif

check_PROGRAMS = test_i2c_bus_collections test_i2c_bus_core \
  test_i2c_bus_selector test_i2c_edid test_i2c_execute test_i2c_services \
  test_i2c_strategy_dispatcher
TESTS = test_i2c_bus_collections test_i2c_bus_core \
  test_i2c_bus_selector test_i2c_edid test_i2c_execute test_i2c_services \
  test_i2c_strategy_dispatcher

test_i2c_bus_collections_SOURCES  = test_i2c_bus_collections.c
test_i2c_bus_collections_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_i2c_bus_collections_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_i2c_bus_core_SOURCES  = test_i2c_bus_core.c
test_i2c_bus_core_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_i2c_bus_core_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_i2c_bus_selector_SOURCES  = test_i2c_bus_selector.c
test_i2c_bus_selector_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_i2c_bus_selector_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_i2c_edid_SOURCES  = test_i2c_edid.c
test_i2c_edid_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_i2c_edid_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_i2c_execute_SOURCES  = test_i2c_execute.c
test_i2c_execute_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_i2c_execute_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_i2c_services_SOURCES  = test_i2c_services.c
test_i2c_services_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_i2c_services_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_i2c_strategy_dispatcher_SOURCES  = test_i2c_strategy_dispatcher.c
test_i2c_strategy_dispatcher_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_i2c_strategy_dispatcher_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)
