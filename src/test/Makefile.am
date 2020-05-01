AM_CPPFLAGS =   \
$(GLIB_CFLAGS)  \
-I$(top_srcdir) \
-I$(top_srcdir)/src \
-I$(top_srcdir)/src/public 

AM_CFLAGS = -Wall -Werror

if ENABLE_CALLGRAPH_COND
AM_CFLAGS += -fdump-rtl-expand
endif


CLEANFILES = \
*expand 


if INCLUDE_TESTCASES_COND
# Intermediate library
noinst_LTLIBRARIES = libtestcases.la

libtestcases_la_SOURCES = \
ddc/ddc_capabilities_tests.c \
ddc/ddc_vcp_tests.c \
i2c/i2c_testutil.c  \
i2c/i2c_edid_tests.c \
i2c/i2c_io_old.c \
testcase_table.c \
testcases.c

endif

