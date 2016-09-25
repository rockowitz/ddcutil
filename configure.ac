#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])

dnl interesting but unused settings in colord; 
dnl
dnl   GLIB_GSETTINGS
dnl   # set up gtk-doc
dnl   GTK_DOC_CHECK(1.9)
dnl   AS_ALL_LINGUAS
dnl   AC_PROG_INSTALL
dnl   IT_PROG_INTLTOOL([0.35.0])
dnl   AC_PATH_PROG(XSLTPROC, xsltproc)


m4_define([ddcutil_major_version], [0])
m4_define([ddcutil_minor_version], [5])
dnl micro version 3 set 7/30/16 - development version
m4_define([ddcutil_micro_version], [9])
m4_define([ddcutil_version],
          [ddcutil_major_version.ddcutil_minor_version.ddcutil_micro_version])

dnl causes VERSION to be set in config.h
AC_INIT([ddcutil], [ddcutil_version], [rockowitz@minsoft.com])

AC_CONFIG_AUX_DIR(config)
dnl sanity check: check for a unique file in source directory: 
AC_CONFIG_SRCDIR([src/util/coredefs.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
# one entry for each Makefile.am:
# removed from list: package/debian/changelog
AC_CONFIG_FILES([
	Makefile 
	src/Makefile 
	man/Makefile 
	data/Makefile 
	package/Makefile 
	package/ddcutil.spec 
	package/upload_obsrpm
  package/build_dpkg
  ],
 )
dnl cannot chmod on build_dpkg, upload_obsrpm, since they will not exist within dpkg build environment

dnl removed -Werror 
dnl rmoved -Werror from AM_INIT_AUTOMAKE to allow compilation to proceed
AM_INIT_AUTOMAKE([1.11 -Wall -Wno-extra-portability foreign subdir-objects])
dnl AM_SILENT_RULES defined as of automake 1.11, so don't need to test for macro's existence
AM_SILENT_RULES([yes])
AM_PROG_AR


### 
### Version specification
###

# libtool versioning - applies to libddcutil
#
# See http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91 for details
#
# increment;
# CURRENT   If the API or ABI interface has changed (reset REVISION to 0)
# REVISION  If the API and ABI remains the same, but bugs are fixed.
# AGE       backward compatibility
LT_CURRENT=0
LT_REVISION=1
LT_AGE=0
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

dnl for debian/changelog - should do it some other way
DEBIAN_RELEASE=1
DEBIAN_DISTRIBUTION=wily
AC_SUBST(DEBIAN_RELEASE)
AC_SUBST(DEBIAN_DISTRIBUTION)


###
### Checks for programs.
###

AC_PROG_CC

dnl This is an obsolescent macro that checks that the C compiler supports the -c and -o options together. 
dnl Note that, since Automake 1.14, the AC_PROG_CC is rewritten to implement such checks itself, and thus 
dnl the explicit use of AM_PROG_CC_C_O should no longer be required.
dnl included in case running Automake 1.13, which is the case for SuSE 13.2
dnl but causes warning: macro 'AM_PROG_CC_C_0' not found in library on SUSE if included:
dnl AM_PROG_CC_C_0

dnl but on Suse 13.2 w automake 1.13.4, get msg that 
dnl warning: warning: compiling 'base/common.c' in subdir requires 'AM_PROG_CC_C_O' in 'configure.ac'

dnl needed for OBS
AC_PROG_CC_STDC

dnl AC_PROG_LIBTOOL, AM_PROG_LIBTOOL are deprecated names for older versions of LT_INIT
dnl adds support for --enable/disable -static/shared, -with/without-pic configure flags
LT_INIT

AC_PROG_RANLIB

m4_ifdef([GOBJECT_INTROSPECTION_CHECK], [GOBJECT_INTROSPECTION_CHECK([1.30.0])])
AM_CONDITIONAL(HAVE_INTROSPECTION, test "x$found_introspection" = xyes)

dnl temporarily disable, introspection not working
dnl AM_CONDITIONAL(HAVE_INTROSPECTION, test "x$found_introspection" = xyesxxx)


###
### Checks for libraries.
###

dnl appends to GLIB_CFLAGS and GLIB_LIBS the output of pkg-config --cflags|--lib glib-2.0
# was 2.36, can be drop this to 2.14 to allow for Debian 7.0
PKG_CHECK_MODULES(GLIB,  glib-2.0 >= 2.14) 

dnl made conditional:
dnl PKG_CHECK_MODULES(GOBJECT,  gobject-2.0 >= 2.14) 

dnl PKG_CHECK_MODULES(OSINFO, libosinfo-1.0 >= 0.1, [OSINFO_FOUND=yes], [OSINFO_FOUND=no])

PKG_CHECK_MODULES(XRANDR, xrandr)
PKG_CHECK_MODULES(X11, x11)
dnl PKG_CHECK_MODULES(UDEV, libudev)
dnl know that 1.0.8 fails, 1.0.20  works
dnl check made conditional on --enable-usb
dnl PKG_CHECK_MODULES(LIBUSB, libusb-1.0 >= 1.0.15)

dnl failing in OBS if no proper BuildRequires, even if libudev use disabled
PKG_CHECK_MODULES([UDEV], [libudev],
  [AC_DEFINE_UNQUOTED( [UDEV_IN_SYSTEMD], [0], [Use standalone UDEV])],
      [PKG_CHECK_MODULES([SYSTEMD], [systemd],
      [AC_DEFINE_UNQUOTED( [UDEV_IN_SYSTEMD], [1], [UDEV in SYSTEMD])
   ])
 ])


dnl do not use
dnl TODO: allow for either hidapi-libusb or hidapi-hidraw
dnl PKG_CHECK_MODULES(HIDAPI, hidapi-hidraw)
dnl PKG_CHECK_MODULES(HIDAPI, hidapi-libusb)

###
### Checks for header files.
###

dnl AC_MSG_NOTICE( [*** Before first header checks] )
AC_CHECK_HEADERS([fcntl.h langinfo.h libintl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h termios.h unistd.h wchar.h  dlfcn.h])
dnl is2c-dev.h is in linux-headers
dnl i2c-dev.h not found:
dnl AC_CHECK_HEADERS([i2c-dev.h])


###
### Checks for typedefs, structures, and compiler characteristics.
###

AC_CHECK_HEADER_STDBOOL
# AC_CHECK_HEADERS([glib.h])
AC_C_INLINE
AC_C_BIGENDIAN
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T


###
### Checks for library functions.
###
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([clock_gettime memset nl_langinfo stpcpy strchr strdup strerror strrchr strtol])

dnl The dlopen() function is in the C library for *BSD and in
dnl libdl on GLIBC-based systems
AC_SEARCH_LIBS([dlopen], [dl dld], [], [
  AC_MSG_ERROR([unable to find the dlopen() function])
])


###
### Other
###

dnl from online research
dnl For creating both 32 and 64 bit: 
dnl AM_ENABLE_MULTILIB
dnl also write config-ml.in script 
dnl needs system w both 32 and 64 bit libraries
dnl ./configure --build=i686=pc-linux-gnu --prefix=/arch/x86-linux/gnu \
dnl     CC=gcc -m32 -march-i586 \
dnl     CXX="g++ -m32 -march=i585 LDFlags="-m32"

dnl Automatically update libtool script if it becomes out of date:
AC_SUBST([LIBTOOL_DEPS])


### 
### Begin options for configure script
### 

dnl *** configure option: --with-adl-headers
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
dnl AC_MSG_NOTICE([HAVE_ADL = $HAVE_ADL])
AC_SUBST([ADL_HEADER_DIR], [$adl_header_dir])
dnl Makefile.am/.in variable: 
AM_CONDITIONAL([HAVE_ADL_COND], [test "x$adl_header_dir" != "x"] )

dnl AC_MSG_NOTICE([adl_header_dir2 = $adl_header_dir2])
dnl AC_DEFINE_UNQUOTED([HAVE_ADL3], [$HAVE_ADL])
dnl AC_DEFINE( [HAVE_ADL2], [1])

dnl *** configure option: --enable-testcases
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

dnl *** configure option: --enable-callgraph
AC_ARG_ENABLE([callgraph],
              [ AS_HELP_STRING( [--enable-callgraph=@<:@no/yes@:>@], [Create .expand files for static call graph@<:@default=no@:>@] )],
              [enable_callgraph=${enableval}],
              [enable_callgraph=no] )
dnl disable after testing:
dnl AC_MSG_NOTICE([enable_callgraph = $enable_callgraph])
dnl Set flag for automake.am:
AM_CONDITIONAL([ENABLE_CALLGRAPH_COND], [test "x$enable_callgraph" == "xyes"] )
AS_IF([test "x$enable_callgraph" == "xyes"],
      AC_MSG_NOTICE( [callgraph... enabled]  ),
      AC_MSG_NOTICE( [callgraph... disabled] )
     )

dnl *** configure option: --enable-lib
AC_ARG_ENABLE([lib],
              [ AS_HELP_STRING( [--enable-lib=@<:@no/yes@:>@], [Build shared library and clients@<:@default=no@:>@] )],
              [enable_lib=${enableval}],
              [enable_lib=no] )
dnl disable after testing:
dnl AC_MSG_NOTICE([enable_lib = $enable_lib])
dnl Define INCLUDE_SHARED_LIB in config.h: (not needed)
dnl AS_IF([test "x$enable_lib" == "xyes"],
dnl       AC_DEFINE_UNQUOTED( [INCLUDE_SHARED_LIB], [1], [If defined, build shared lib and clients.]),
dnl       [])
dnl Set flag for automake.am:
AM_CONDITIONAL([ENABLE_SHARED_LIB_COND], [test "x$enable_lib" == "xyes"] )
AS_IF([test "x$enable_lib" == "xyes"],
      AC_MSG_NOTICE( [lib... enabled]  ),
      AC_MSG_NOTICE( [lib... disabled] )
     )
dnl AC_SUBST([ENABLE_SHARED_LIB_FLAG], [test "x$enable_lib" == "xyes"; $?])
dnl AC_SUBST([ENABLE_SHARED_LIB_FLAG], [xy])
AS_IF([test "x$enable_lib" == "xyes"],
      AC_SUBST([ENABLE_SHARED_LIB_FLAG], [1] ),
      AC_SUBST([ENABLE_SHARED_LIB_FLAG], [0]  )
     )

dnl *** configure option: --enable-gobject-api
AC_ARG_ENABLE([gobject-api],
              [ AS_HELP_STRING( [--enable-gobject-api=@<:@no/yes@:>@], [Build gobject related files, typelib@<:@default=no@:>@] )],
              [enable_gobject=${enableval}],
              [enable_gobject=no] )
AM_CONDITIONAL([ENABLE_GOBJECT_COND], [test "x$enable_gobject" == "xyes"] )
AS_IF([test "x$enable_gobject" == "xyes"],
      AC_MSG_NOTICE( [gobject-api... enabled]  ),
      AC_MSG_NOTICE( [gobject-api... disabled] )
     )

dnl dnl *** configure option: --enable-libudev
dnl AC_ARG_ENABLE([libudev],
dnl               [ AS_HELP_STRING( [--enable-libudev=@<:@yes/no@:>@], [Support USB connected monitors@<:@default=yes@:>@] )],
dnl               [enable_libudev=${enableval}],
dnl               [enable_libudev=yes] )
dnl AM_CONDITIONAL([ENABLE_LIBUDEV_COND], [test "x$enable_libudev" == "xyes"] )
dnl AS_IF([test "x$enable_libudev" == "xyes"],
dnl       AC_SUBST([ENABLE_LIBUDEV_FLAG], [1] ),
dnl       AC_SUBST([ENABLE_LIBUDEV_FLAG], [0]  )
dnl      )
dnl dnl Define USE_LIBUDEV in config.h:
dnl AS_IF([test "x$enable_libudev" == "xyes"],
dnl       AC_DEFINE_UNQUOTED( [USE_LIBUDEV], [1], [If defined, use libudev.]),
dnl      [])
dnl AS_IF([test "x$enable_libudev" == "xyes"],
dnl       AC_MSG_NOTICE( [libudev... enabled]  ),
dnl      AC_MSG_NOTICE( [libudev... disabled] )
dnl      )

dnl *** configure option: --enable-usb
AC_ARG_ENABLE([usb],
              [ AS_HELP_STRING( [--enable-usb=@<:@yes/no@:>@], [Use usb to probe devices@<:@default=yes@:>@] )],
              [enable_usb=${enableval}],
              [enable_usb=yes] )
AM_CONDITIONAL([ENABLE_USB_COND], [test "x$enable_usb" == "xyes"] )
AS_IF([test "x$enable_usb" == "xyes"],
      AC_SUBST([ENABLE_USB_FLAG], [1] ),
      AC_SUBST([ENABLE_USB_FLAG], [0]  )
     )
dnl Define USE_USB in config.h:
AS_IF([test "x$enable_usb" == "xyes"],
      AC_DEFINE_UNQUOTED( [USE_USB], [1], [If defined, use usb.]),
      [])
AS_IF([test "x$enable_usb" == "xyes"],
      AC_MSG_NOTICE( [usb... enabled]  ),
      AC_MSG_NOTICE( [usb... disabled] )
     )


### Conditional library tests 

dnl know that 1.0.8 fails, 1.0.20  works
AS_IF([test "x$enable_usb" == "xyes"],
     [ PKG_CHECK_MODULES(LIBUSB, libusb-1.0 >= 1.0.15) ],
     [ echo usb disabled, not checking for libusb ]
     )

dnl AC_MSG_NOTICE( $LIBUSB_CFLAGS )
dnl CPPFLAGS=$LIBUSB_CFLAGS
dnl AC_MSG_NOTICE( CPPFLAGS: $CPPFLAGS )
dnl AC_CHECK_HEADERS([libusb-1.0/libusb.h])

dnl header check for libusb.h not necessary.  PKG_CHECK_MODULES for libusb-1.0 will fail 
dnl if development package not installed
dnl AC_MSG_NOTICE( [Wolf 1])
dnl AC_CHECK_HEADER fails but AC_CHECK_HEADERS succeeds.  why???
AS_IF([test "x$enable_usb" == "xyes"],
     [ 
        AC_CHECK_HEADERS( [libusb-1.0/libusb.h],
                        [libusb_h_found=yes],
                        [libusb_h_found=no],
                        )
     ],
     [ echo usb disabled, not checking for libusb.h ]
     )
AS_IF([test "x$libusb_h_found" == "xno"],
     AC_MSG_ERROR( [Header file libusb.h not found.  Install distro-specific development package for libusb-1.0 ] ) 
)


AS_IF([test "x$enable_gobject" == "xyes"],
     [ PKG_CHECK_MODULES(GOBJECT,  gobject-2.0 >= 2.14)  ],
     )



### 
### End options for configure script
###


###
### Generate output   
###

AC_OUTPUT
