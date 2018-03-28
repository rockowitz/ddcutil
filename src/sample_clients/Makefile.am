# File src/sample_clients/Makefile.am
# Makefile for sample and test programs for libddcutil

AM_CPPFLAGS=     \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC
AM_CFLAGS += -Werror

check_PROGRAMS =
if ENABLE_SHARED_LIB_COND
# Sample C client program for shared library:
check_PROGRAMS += \
  laclient \
  demo_capabilities \
  demo_display_selection \
  demo_feature_list \
  demo_get_set_vcp \
  demo_global_settings \
  demo_profile_features \
  demo_redirection \
  demo_vcpinfo
endif

laclient_SOURCES               = clmain.c
demo_capabilities_SOURCES      = demo_capabilities.c
demo_display_selection_SOURCES = demo_display_selection.c
demo_feature_list_SOURCES      = demo_feature_list.c
demo_get_set_vcp_SOURCES       = demo_get_set_vcp.c
demo_global_settings_SOURCES   = demo_global_settings.c
demo_profile_features_SOURCES  = demo_profile_features.c
demo_redirection_SOURCES       = demo_redirection.c
demo_vcpinfo_SOURCES           = demo_vcpinfo.c

LDADD       = ../libddcutil.la
AM_LDFLAGS  = -pie


clean-local: 
	@echo "(src/sample_clients/Makefile) clean-local"

mostlyclean-local:
	@echo "(src/sample_clients/Makefile) mostlyclean-local"

distclean-local:
	@echo "(src/sample_clients/Makefile) distclean-local"


# Make this a dependency of something to enable it:
show_vars:
	@echo "src/sample_clients/Makefile"
	@echo "srcdir:      ${srcdir}"
	@echo "top_srcdir:  ${top_srcdir}"

.PHONY: show_vars

