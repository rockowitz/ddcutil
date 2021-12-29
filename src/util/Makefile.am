AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
  $(LIBDRM_CFLAGS)

AM_CFLAGS = $(AM_CFLAGS_STD)

CLEANFILES = \
*expand

clean-local:
	@echo "(src/util/Makefile) clean-local"

mostlyclean-local:
	@echo "(src/util/Makefile) mostlyclean-local"

distclean-local:
	@echo "(src/util/Makefile) distclean-local"

# Intermediate Libraries

noinst_LTLIBRARIES = libutil.la libutilaux.la

libutil_la_SOURCES = \
data_structures.c          \
ddcutil_config_file.c      \
debug_util.c               \
edid.c                     \
error_info.c               \
file_util.c                \
file_util_base.c           \
glib_util.c                \
glib_string_util.c         \
i2c_util.c                 \
linux_util.c               \
multi_level_map.c          \
report_util.c              \
simple_ini_file.c          \
string_util.c              \
sysfs_util.c               \
subprocess_util.c          \
timestamp.c                \
utilrpt.c                  \
xdg_util.c

if !ENABLE_TARGETBSD_COND
libutil_la_SOURCES +=      \
sysfs_filter_functions.c   \
sysfs_i2c_util.c
endif

if ENABLE_USB_COND
libutil_la_SOURCES +=      \
device_id_util.c
else
if ENABLE_ENVCMDS_COND
libutil_la_SOURCES +=      \
device_id_util.c
endif
endif

if ENABLE_UDEV_COND
libutil_la_SOURCES +=      \
udev_i2c_util.c            \
udev_usb_util.c            \
udev_util.c               
endif

if ENABLE_FAILSIM_COND
libutil_la_SOURCES += failsim.c
endif

# libutil_la_SOURCES += systemd_util.c

libutilaux_la_SOURCES = 
if USE_LIBDRM_COND
libutilaux_la_SOURCES += libdrm_util.c
endif

if USE_X11_COND
libutil_la_SOURCES += x11_util.c
endif

# if ENABLE_YAML_COND
#  libutil_la_SOURCES += libyaml_dbgutil.c
# endif



dist-hook0:
	@echo "(src/util/Makefile) dist-hook"

# Rename to "all-local" for development
all-local-disabled:
	@echo ""
	@echo "(src/util/Makefile:all-local) Variable values:"
	@echo "   CLEANFILES:       $(CLEANFILES)"
	@echo "   CFLAGS:           $(CFLAGS)"
	@echo "   AM_CFLAGS:        $(AM_CFLAGS)"
	@echo "   CPPFLAGS:         $(CPPFLAGS)"
	@echo "   AM_CPPFLAGS:      $(AM_CPPFLAGS)"
	@echo "   AUTOMAKE_OPTIONS: $(AUTOMAKE_OPTIONS)"
	@echo "   MAKELEVEL:        $(MAKELEVEL)"
	@echo "   MAKEFLAGS:        $(MAKEFLAGS)"
	@echo "   V:                $(V)"
	@echo "   AM_CFLAGS_STD:    $(AM_CFLAGS_STD)"
	@echo ""
