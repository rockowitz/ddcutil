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
noinst_LTLIBRARIES = libddc.la

libddc_la_SOURCES =         \
ddc_async.c                 \
ddc_displays.c              \
ddc_display_lock.c          \
ddc_dumpload.c              \
ddc_dynamic_features.c      \
ddc_multi_part_io.c         \
ddc_output.c                \
ddc_packet_io.c             \
ddc_read_capabilities.c     \
ddc_services.c              \
ddc_strategy.c              \
ddc_vcp.c                   \
ddc_vcp_info.c              \
ddc_vcp_version.c           \
ddc_try_stats.c    
