AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/public


AM_CFLAGS = -Wall 
AM_CFLAGS += -Werror

if ENABLE_CALLGRAPH_COND
AM_CFLAGS += -fdump-rtl-expand
endif

CLEANFILES = \
*expand

# Intermediate Library
noinst_LTLIBRARIES = libappcommon.la

libappcommon_la_SOURCES =     \
   app_dynamic_features.c

@ alt:
# if ENABLE_DYNAMIC_FEATURES
# libappcommon_la_SOURCES += \
#    app_dynamic_features.c 
# endif


