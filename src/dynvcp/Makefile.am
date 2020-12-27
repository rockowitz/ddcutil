AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/public

if HAVE_ADL_COND
AM_CPPFLAGS +=  \
  -I@ADL_HEADER_DIR@  
endif

AM_CFLAGS = -Wall 
AM_CFLAGS += -Werror
# Causes PROGRAM_LOGIC_ERROR() macro to fail:
# AM_CFLAGS += -Wpedantic

if ENABLE_CALLGRAPH_COND
AM_CFLAGS += -fdump-rtl-expand
endif

CLEANFILES = \
*expand

# Intermediate Library
noinst_LTLIBRARIES = libdynvcp.la

libdynvcp_la_SOURCES =      \
dyn_feature_set.c           \
dyn_parsed_capabilities.c   \
dyn_feature_codes.c         \
dyn_feature_files.c
