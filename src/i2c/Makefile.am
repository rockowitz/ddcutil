AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/public

AM_CFLAGS = -Wall 
AM_CFLAGS += -Werror
# AM_CFLAGS += -Wpedantic

if ENABLE_CALLGRAPH_COND
AM_CFLAGS += -fdump-rtl-expand
endif

CLEANFILES = \
*expand

# Intermediate Library
noinst_LTLIBRARIES = libi2c.la

libi2c_la_SOURCES =     \
i2c_base_io.c           \
i2c_bus_core.c          \
i2c_do_io.c         
