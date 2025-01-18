AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
  $(LIBDRM_CFLAGS)   \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/public

AM_CFLAGS = -Wall
if WARNINGS_ARE_ERRORS_COND
AM_CFLAGS += -Werror
endif
# AM_CFLAGS += -Wpedantic

if ENABLE_CALLGRAPH_COND
AM_CFLAGS += -fdump-rtl-expand
endif

CLEANFILES = \
*expand

# Intermediate Library
noinst_LTLIBRARIES = libsysfs.la

libsysfs_la_SOURCES =       \
i2c_dpms.c                \
i2c_sys_drm_connector.c   \
i2c_sysfs_base.c          \
i2c_sysfs_conflicting_drivers.c \
i2c_sysfs_i2c_info.c      \
i2c_sysfs_i2c_sys_info.c  \
i2c_sysfs_top.c           \
sysfs_services.c
