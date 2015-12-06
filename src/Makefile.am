

# AM_CFLAGS = -Wall -Werror
AM_CFLAGS = -Wall 

# Is this really necessary?   Don't want to push these files to github.
# there ought to be a cleaner way
# MOSTLYCLEANFILES =    \
# base/.deps/*          \
# cmdline/.deps/*       \
# ddc/.deps/*.Po        \
# i2c/.deps/*           \
# main/.deps/*          \
# test/ddc/.deps/*      \
# test/i2c/.deps/*      \
# test/.deps/*          \
# util/.deps/*          \
# adl/.deps/*           \
# adl/adl_impl/.deps/*  \
# adl/adl_mock_impl/.deps/*  



bin_PROGRAMS = ddctool laclient
lib_LTLIBRARIES = libddctool.la

ddctool_CFLAGS = $(AM_FLAGS)
laclient_CFLAGS = $(AM_FLAGS)

laclient_SOURCES = \
clients/clmain.c

ddctool_SOURCES = \
base/common.c \
base/ddc_errno.c \
base/ddc_packets.c \
base/displays.c \
base/edid.c \
base/execution_stats.c \
base/linux_errno.c \
base/msg_control.c \
base/status_code_mgt.c \
base/util.c \
cmdline/cmd_parser_aux.c \
cmdline/cmd_parser_goption.c \
cmdline/parsed_cmd.c \
ddc/ddc_command_codes.c \
ddc/ddc_multi_part_io.c \
ddc/ddc_packet_io.c \
ddc/ddc_services.c \
ddc/ddc_strategy.c \
ddc/ddc_vcp.c \
ddc/parse_capabilities.c \
ddc/try_stats.c \
ddc/vcp_feature_codes.c \
ddc/vcp_feature_record.c \
i2c/i2c_base_io.c \
i2c/i2c_bus_core.c \
i2c/i2c_do_io.c \
main/loadvcp.c \
main/main.c \
main/testcases.c \
test/ddc/ddc_capabilities_tests.c \
test/ddc/ddc_vcp_tests.c \
test/i2c/i2c_edid_tests.c \
test/i2c/i2c_io_old.c \
test/testcase_table.c \
util/data_structures.c \
util/debug_util.c \
util/file_util.c  \
util/report_util.c \
util/string_util.c 

if HAVE_ADL_COND
ddctool_SOURCES += \
  adl/adl_impl/adl_aux_intf.c \
  adl/adl_impl/adl_errors.c \
  adl/adl_impl/adl_intf.c \
  adl/adl_impl/adl_report.c \
  adl/adl_impl/adl_shim.c \
  test/adl/adl_tests.c \
  test/adl/adl_from_sample.c 
else
ddctool_SOURCES += \
  adl/adl_mock_impl/adl_mock_shim.c \
  adl/adl_mock_impl/adl_mock_errors.c
endif


libddctool_la_SOURCES = \
libmain/ddct_public.c  \
base/common.c \
base/ddc_errno.c \
base/ddc_packets.c \
base/displays.c \
base/edid.c \
base/execution_stats.c \
base/linux_errno.c \
base/msg_control.c \
base/status_code_mgt.c \
base/util.c \
cmdline/cmd_parser_aux.c \
cmdline/cmd_parser_goption.c \
cmdline/parsed_cmd.c \
ddc/ddc_command_codes.c \
ddc/ddc_multi_part_io.c \
ddc/ddc_packet_io.c \
ddc/ddc_services.c \
ddc/ddc_strategy.c \
ddc/ddc_vcp.c \
ddc/parse_capabilities.c \
ddc/try_stats.c \
ddc/vcp_feature_codes.c \
ddc/vcp_feature_record.c \
i2c/i2c_base_io.c \
i2c/i2c_bus_core.c \
i2c/i2c_do_io.c \
main/loadvcp.c \
main/main.c \
main/testcases.c \
test/ddc/ddc_capabilities_tests.c \
test/ddc/ddc_vcp_tests.c \
test/i2c/i2c_edid_tests.c \
test/i2c/i2c_io_old.c \
test/testcase_table.c \
util/data_structures.c \
util/debug_util.c \
util/file_util.c  \
util/report_util.c \
util/string_util.c 

if HAVE_ADL_COND
libddctool_la_SOURCES += \
  adl/adl_impl/adl_aux_intf.c \
  adl/adl_impl/adl_errors.c \
  adl/adl_impl/adl_intf.c \
  adl/adl_impl/adl_report.c \
  adl/adl_impl/adl_shim.c \
  test/adl/adl_tests.c \
  test/adl/adl_from_sample.c 
else
libddctool_la_SOURCES += \
  adl/adl_mock_impl/adl_mock_shim.c \
  adl/adl_mock_impl/adl_mock_errors.c
endif



libddctool_la_LIBADD = -lz
libddctool_la_LIBADD +=                  \
   $(GLIB_LIBS)

libddctool_la_LDFLAGS = -export_symbols-regex '^ddct_[^_]'
# libddctool_la_LDFLAGS += -version-info '0:1:1'

 # -I/usr/src/linux-headers-4.2.0-18/include/linux 
 # -I/usr/include/x86_64-linux-gnu 

# -I/usr/include/linux
# -I/shared/linsrc/i2c-tools/i2c-tools-3.1.1/include/linux 
# -I/usr/include 
# -I/shared/linsrc/i2c-tools/i2c-tools-3.1.1/include/linux 
# -I"/shared/playproj/i2c/src"  

# problem: trying to get i2c-dev.h from /usr/include/linux causes errors

# GLIB_CFLAGS contains output of pkgconfig --cflags glib-2.0
AM_CPPFLAGS=$(GLIB_CFLAGS) \
-I$(top_srcdir) 


# -I/usr/include/glib-2.0  \
# -I/usr/lib64/glib-2.0/include  \
# -I/usr/lib/x86_64-linux-gnu/glib-2.0/include 



if HAVE_ADL_COND
AM_CPPFLAGS +=               \
   -I@ADL_HEADER_DIR@  
endif



# echo "hi there"
# echo $(GLIB_LIBS)

# LIBS = $(GLIB_LIBS) \
# -L/usr/lib64                      \
# -L/usr/lib64/glib-2.0             \
# -L/usr/lib/x86_64-linux-gnu

# LDADD = /usr/lib/x86_64-linux-gnu/libdl.a

# $(GLIB_LIBS) contains output of pkgconfig --libs glib-2.0
ddctool_LDADD =                  \
   $(GLIB_LIBS)

laclient_LDADD = \
   libddctool.la