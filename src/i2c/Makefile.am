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
noinst_LTLIBRARIES = libi2c.la

libi2c_la_SOURCES =       \
i2c_bus_core.c            \
i2c_bus_selector.c        \
i2c_display_lock.c        \
i2c_dpms.c                \
i2c_edid.c                \
i2c_execute.c             \
i2c_services.c            \
i2c_strategy_dispatcher.c \
i2c_sysfs.c
