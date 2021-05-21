AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/public

AM_CFLAGS = $(AM_CFLAGS_STD) 

CLEANFILES = \
*expand

clean-local:
	@echo "(src/base/Makefile) clean-local"

mostlyclean-local:
	@echo "(src/base/Makefile) mostlyclean-local"

distclean-local:
	@echo "(src/base/Makefile) distclean-local"

dist-hook:
	@echo "(src/base/Makefile) dist-hook"

# Intermediate Library
noinst_LTLIBRARIES = libbase.la

libbase_la_SOURCES =      \
base_init.c               \
build_info.c              \
core.c                    \
core_per_thread_settings.c \
ddc_errno.c               \
ddc_packets.c             \
dynamic_features.c        \
dynamic_sleep.c           \
displays.c                \
execution_stats.c         \
feature_lists.c           \
feature_metadata.c        \
feature_sets.c            \
last_io_event.c           \
linux_errno.c             \
monitor_model_key.c       \
per_thread_data.c         \
rtti.c                    \
sleep.c                   \
thread_retry_data.c       \
thread_sleep_data.c       \
tuned_sleep.c             \
status_code_mgt.c         \
vcp_version.c



# Rename to "all=local" for development 
all-local-disabled:
	@echo ""
	@echo "(src/base/Makefile:all-local) Variable values:"
	@echo "   CLEANFILES:       $(CLEANFILES)"
	@echo "   CFLAGS:           $(CFLAGS)"
	@echo "   AM_CFLAGS:        $(AM_CFLAGS)"
	@echo "   CPPFLAGS:         $(CPPFLAGS)"
	@echo "   AM_CPPFLAGS:      $(AM_CPPFLAGS)"
	@echo "   ddcutil_FLAGS:    $(ddcutil_CFLAGS)"
	@echo "   AUTOMAKE_OPTIONS: $(AUTOMAKE_OPTIONS)"
	@echo "   MAKELEVEL:        $(MAKELEVEL)"
	@echo "   MAKEFLAGS:        $(MAKEFLAGS)"
	@echo "   V:                $(V)"
	@echo "   AM_CFLAGS_STD:    $(AM_CFLAGS_STD)"
	@echo ""
