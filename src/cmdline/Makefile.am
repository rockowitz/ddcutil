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
noinst_LTLIBRARIES = libcmdline.la

libcmdline_la_SOURCES =      \
cmd_parser_aux.c \
cmd_parser_goption.c \
parsed_cmd.c 


