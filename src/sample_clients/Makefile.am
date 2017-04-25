
AM_CPPFLAGS=     \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

# AM_CPPFLAGS += $(GLIB_CFLAGS)

AM_CFLAGS = -Wall -fPIC
AM_CFLAGS += -Werror


check_PROGRAMS =

if ENABLE_SHARED_LIB_COND
# Sample C client program for shared library:
check_PROGRAMS += \
  laclient \
  demo_global_settings \
  demo_vcpinfo \
  demo_get_set_vcp \
  demo_display_selection
endif

#
# Files only in laclient executable
#
laclient_SOURCES               = clmain.c
demo_global_settings_SOURCES   = demo_global_settings.c
demo_vcpinfo_SOURCES           = demo_vcpinfo.c
demo_display_selection_SOURCES = demo_display_selection.c
demo_get_set_vcp_SOURCES       = demo_get_set_vcp.c

LDADD                = ../libddcutil.la
# laclient_LDADD                = ../libddcutil.la
# demo_global_settings_LDADD    = ../libddcutil.la
# demo_vcpinfo_LDADD            = ../libddcutil.la
# demo_get_set_vcp_LDADD        = ../libddcutil.la
# demo_display_selection_LDADD  = ../libddcutil.la

AM_LDFLAGS                = -pie
# laclient_LDFLAGS                = -pie
# demo_global_settings_LDFLAGS    = -pie
# demo_vcpinfo_LDFLAGS            = -pie
# demo_get_set_vcp_LDFLAGS        = -pie
# demo_display_selection_LDFLAGS  = -pie

show_vars:
	@echo "src/sample_clients/Makefile"
	@echo "srcdir:      ${srcdir}"
	@echo "top_srcdir:  ${top_srcdir}"

.PHONY: show_vars


clean-local: 
	@echo "(src/util/Makefile) clean-local"

mostlyclean-local:
	@echo "(src/util/Makefile) mostlyclean-local"

distclean-local:
	@echo "(src/util/Makefile) distclean-local"


