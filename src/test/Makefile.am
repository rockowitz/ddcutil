AM_CPPFLAGS = \
$(GLIB_CFLAGS)  \
-I$(top_srcdir) \
-I$(top_srcdir)/src \
-I$(top_srcdir)/src/public 

AM_CFLAGS = -Wall -Werror

CLEANFILES = \
*expand 


# Intermediate library
noinst_LTLIBRARIES = libtestcases.la

if INCLUDE_TESTCASES_COND
libtestcases_la_SOURCES = \
i2c/i2c_testutil.c  \
testcase_table.c \
testcases.c
else
libtestcases_la_SOURCES = \
testcase_mock_table.c
endif
