#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

dnl CPPFLAGS="$CPPFLAGS -I/usr/include/glib2.0"

AC_PREREQ([2.69])
AC_INIT([dcctool], [0.1], [rockowitz@minsoft.com])
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

AC_CONFIG_SRCDIR([src/util/coredefs.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_LIBTOOL

dnl GLIB_GSETTINGS




PKG_CHECK_MODULES(GLIB,  glib-2.0 >= 2.36) 

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h langinfo.h libintl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h termios.h unistd.h wchar.h  dlfcn.h])

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



# AC_CONFIG_FILES([Makefile, src/Makefile, src/util/Makefile])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT


# AM_CXXFLAGS = GLIB_CFLAGS
