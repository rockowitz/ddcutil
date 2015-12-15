#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

dnl CPPFLAGS="$CPPFLAGS -I/usr/include/glib2.0"

AC_PREREQ([2.69])

m4_define([ddctool_major_version], [0])
m4_define([ddctool_minor_version], [1])
m4_define([ddctool_micro_version], [0])
m4_define([ddctool_version],
          [ddctool_major_version.ddctool_minor_version.ddctool_micro_version])

AC_INIT([dcctool], [ddctool_version], [rockowitz@minsoft.com])
AC_CONFIG_AUX_DIR(config)
dnl removed -Werror 
AM_INIT_AUTOMAKE([-Wall -Wno-extra-portability foreign subdir-objects])
AM_PROG_AR
dnl adds support for --enable/disable -static/shared, -with/without-pic configure flags
LT_INIT

dnl from online research
dnl For creating both 32 and 64 bit: 
dnl AM_ENABLE_MULTILIB
dnl also write config-ml.in script 
dnl needs system w both 32 and 64 bit libraries
dnl ./configure --build=i686=pc-linux-gnu --prefix=/arch/x86-linux/gnu \
dnl     CC=gcc -m32 -march-i586 \
dnl     CXX="g++ -m32 -march=i585 LDFlags="-m32"


dnl to automatically update libtool script if it becomes out of date:
AC_SUBST([LIBTOOL_DEPS])

AC_CONFIG_SRCDIR([src/util/coredefs.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

dnl AC_PROG_LIBTOOL, AM_PROG_LIBTOOL are deprecated names for older versions of LT_INIT
dnl AC_PROG_LIBTOOL

dnl GLIB_GSETTINGS

dnl for future: 
dml GOBJECT_INTROSPECTION_CHECK([0.9.8])

dnl appends to GLIB_CFLAGS and GLIB_LIBS the output of pkg-config --cflags|--lib glib-2.0
PKG_CHECK_MODULES(GLIB,  glib-2.0 >= 2.36) 

dnl PKG_CHECK_MODULES(OSINFO, libosinfo-1.0 >= 0.1, [OSINFO_FOUND=yes], [OSINFO_FOUND=no])

# Checks for programs.
AC_PROG_CC

dnl This is an obsolescent macro that checks that the C compiler supports the -c and -o options together. 
dnl Note that, since Automake 1.14, the AC_PROG_CC is rewritten to implement such checks itself, and thus 
dnl the explicit use of AM_PROG_CC_C_O should no longer be required.
dnl included in case running Automake 1.13, which is the case for SuSE 13.2
dnl but causes warning: macro 'AM_PROG_CC_C_0' not found in library on SUSE if included:
dnl AM_PROG_CC_C_0

dnl but on Suse 13.2 w automake 1.13.4, get msg that 
dnl warning: warning: compiling 'base/common.c' in subdir requires 'AM_PROG_CC_C_O' in 'configure.ac'
dnl rmoved -Werror from AM_INIT_AUTOMAKE to allow compilation to proceed

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h langinfo.h libintl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h termios.h unistd.h wchar.h  dlfcn.h])
dnl i2c-dev.h not found:
dnl AC_CHECK_HEADERS([i2c-dev.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
# AC_CHECK_HEADERS([glib.h])
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([clock_gettime memset nl_langinfo stpcpy strchr strdup strerror strrchr strtol])

dnl The dlopen() function is in the C library for *BSD and in
dnl libdl on GLIBC-based systems
AC_SEARCH_LIBS([dlopen], [dl dld], [], [
  AC_MSG_ERROR([unable to find the dlopen() function])
])


AC_ARG_WITH([adl-headers],
            [ AS_HELP_STRING([--with-adl-headers=DIR], [Directory containing ADL header files]) ],
            [adl_header_dir=$withval],
            [adl_header_dir=[]])
AC_MSG_NOTICE([adl_header_dir = $adl_header_dir])

AS_IF([test "x$adl_header_dir" != "x"],
      HAVE_ADL=1,
      HAVE_ADL=0)


dnl this is the way to do it
AS_IF([test "x$adl_header_dir" != "x"],
      AC_DEFINE_UNQUOTED( [HAVE_ADL], [1], [If defined, ADL headers are present.  Build with ADL support.]),
      [])


AC_MSG_NOTICE([HAVE_ADL = $HAVE_ADL])

AC_SUBST([ADL_HEADER_DIR], [$adl_header_dir])

AM_CONDITIONAL([HAVE_ADL_COND], [test "x$adl_header_dir" != "x"] )

AC_MSG_NOTICE([adl_header_dir2 = $adl_header_dir2])

dnl AC_DEFINE_UNQUOTED([HAVE_ADL3], [$HAVE_ADL])

dnl AC_DEFINE( [HAVE_ADL2], [1])



AC_ARG_ENABLE([testcases],
              [ AS_HELP_STRING( [--enable-testcases=@<:@no/yes@:>@], [Include test cases @<:@default=no@:>@] )],
              [include_testcases=${enableval}],
              [include_testcases=no] )
dnl disable after testing:
dnl AC_MSG_NOTICE([include_testcases = $include_testcases])
dnl Set flag for automake.am:
AM_CONDITIONAL([INCLUDE_TESTCASES_COND], [test "x$include_testcases" == "xyes"] )
dnl Define INCLUDE_TESTCASES in config.h:
AS_IF([test "x$include_testcases" == "xyes"],
      AC_DEFINE_UNQUOTED( [INCLUDE_TESTCASES], [1], [If defined, build with test cases.]),
      [])
AS_IF([test "x$include_testcases" == "xyes"],
      AC_MSG_NOTICE( [testcases... enabled]  ),
      AC_MSG_NOTICE( [testcases... disabled] )
     )



AC_PROG_RANLIB


# AC_CONFIG_FILES([Makefile, src/Makefile, src/util/Makefile])
AC_CONFIG_FILES([Makefile src/Makefile man/Makefile])
AC_OUTPUT


# AM_CXXFLAGS = GLIB_CFLAGS
