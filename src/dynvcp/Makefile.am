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

libdynvcp_la_SOURCES =         \
ddc_feature_set.c           \
ddc_parsed_capabilities.c   \
ddc_vcp_info.c              \
ddc_dynamic_features.c
