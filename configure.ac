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
m4_define([ddcutil_minor_version], [6])
m4_define([ddcutil_micro_version], [1])
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
  src/swig/Makefile 
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

dnl replaced by LT_INIT
dnl AC_PROG_RANLIB

###
### GObject
###

m4_ifdef([GOBJECT_INTROSPECTION_CHECK], [GOBJECT_INTROSPECTION_CHECK([1.30.0])])
AM_CONDITIONAL(HAVE_INTROSPECTION, test "x$found_introspection" = xyes)

###
### Notes
###

dnl Notes on AC_CHECK_PROG(), AC_PROG_PATH(): 
dnl   AC_CHECK_PROG(var,prog-to-check-for, val-if-found, val-if-no-found)
dnl     checks for prog-to-check-for in PATH
dnl     does nothing if variable already set
dnl     calls AC_SUBST for variable
dnl   AC_CHECK_PATH(variable, prog-to-check-for [, value-if-not-found])
dnl     search PATH for prog-to-check-for, sets variable to entire path of prog-to-check-for if found
dnl     BUT, unlike AC_CHECK_PROG, still checks even if variable already set, so no real benefit to using


###
### Locate SWIG and PYTHON in case we need them
###

AC_MSG_NOTICE( [Calling am_path_python...] )
AM_PATH_PYTHON(2.6, [python_found=yes], [python_found=no]) 
AC_MSG_NOTICE( [Set by am_path_python:] )
AC_MSG_NOTICE( [  python_found        = $python_found] )
AC_MSG_NOTICE( [  PYTHON              = $PYTHON] )
AC_MSG_NOTICE( [  PYTHON_VERSION      = $PYTHON_VERSION] )
AC_MSG_NOTICE( [  PYTHON_PREFIX       = $PYTHON_PREFIX] )
AC_MSG_NOTICE( [  PYTHON_EXEC_PREFIX  = $PYTHON_EXEC_PREFIX] )
AC_MSG_NOTICE( [  pythondir           = $pythondir] )
AC_MSG_NOTICE( [  pkgpythondir        = $pkgpythondir] )
AC_MSG_NOTICE( [  pyexecdir           = $pyexecdir] )
AC_MSG_NOTICE( [  pkgpyexecdir        = $pkgpyexecdir] )

dnl AX_PYTHON_DEVEL Calls AC_PATH_PROG() to find $PYTHON, fails if python not found
dnl PYTHON_SITE_PKG_EXEC set only in _alt version:
AS_IF( [test "x$python_found" == "xyes" ], [
   AC_MSG_NOTICE( [Calling ax_python_devel...])
   AX_PYTHON_DEVEL

   AC_MSG_NOTICE( [From ax_python_devel or ac_python_devel_alt..  ] )
   AC_MSG_NOTICE( [  PYTHON_CPPFLAGS      = $PYTHON_CPPFLAGS      ] )
   AC_MSG_NOTICE( [  PYTHON_SITE_PKG      = $PYTHON_SITE_PKG    ] )

   AC_MSG_NOTICE( [From ax_python_devel only ...  ] )
   AC_MSG_NOTICE( [  PYTHON_LIBS          = $PYTHON_LIBS          ] )
   AC_MSG_NOTICE( [  PYTHON_EXTRA_LDFLAGS = $PYTHON_EXTRA_LDFLAGS    ] )
   AC_MSG_NOTICE( [  PYTHON_EXTRA_LIBS    = $PYTHON_EXTRA_LIBS    ] )

   AC_MSG_NOTICE( [From ax_python_devel_alt only ...  ] )
   AC_MSG_NOTICE( [  PYTHON_LDFLAGS       = $PYTHON_LDFLAGS       ] )
   AC_MSG_NOTICE( [  PYTHON_SITE_PKG_EXEC = $PYTHON_SITE_PKG_EXEC    ] )
   AC_MSG_NOTICE( [  PYTHON_CFLAGS       = $PYTHON_CFLAGS] )
])


dnl AC_MSG_NOTICE( [----------------------])
dnl dnl swig is not known to pkg-config, so either it exists in PATH or it doesn't exist
dnl AC_ARG_VAR(SWIG2, [swig executable])
dnl AC_MSG_NOTICE( [before ac_check_prog: SWIG2: $SWIG2] )
dnl AC_CHECK_PROG( [SWIG2], [swig], [swig] )
dnl AC_MSG_NOTICE( [after ac_check_prog: SWIG2 = $SWIG2] )
dnl AS_IF( [test "x$SWIG2" != "x"],
dnl           AC_MSG_NOTICE( [swig found] ),
dnl           AC_MSG_NOTICE( [swig NOT found] )
dnl     )

dnl aclocal doesn't find AX_PKG_SWIG if inside AS_IF, so execute it whether or not we need it
AC_MSG_NOTICE( [Calling ax_pkg_swig...] )
dnl ax_pkg_swig respects SWIG_LIB, but overrides SWIG, so don't expose the possibility of overriding
dnl AC_ARG_VAR(SWIG, [swig executable])
dnl AC_ARG_VAR(SWIG_LIB, [linker flags for swig])
dnl AC_MSG_NOTICE( [Before ax_pkg_swig:] )
dnl AC_MSG_NOTICE( [  SWIG                    = $SWIG ] )
dnl AC_MSG_NOTICE( [  SWIG_LIB                = $SWIG_LIB ])

AX_PKG_SWIG(1.3.21, [swig_found=yes], [swig_found=no])
AC_MSG_NOTICE( [Set by ax_pkg_swig:] )
AC_MSG_NOTICE( [  swig_found              = $swig_found] )
AC_MSG_NOTICE( [  SWIG                    = $SWIG ] )
AC_MSG_NOTICE( [  SWIG_LIB                = $SWIG_LIB ])

dnl AC_MSG_NOTICE( [Calling AX .. SWIG .. MULTI .. MODULE .. SUPPORT...] )
dnl AX_SWIG_MULTI_MODULE_SUPPORT
dnl AC_MSG_NOTICE( [Set by AX..SWIG..MULTI..MODULE..SUPPORT: ] )
dnl AC_MSG_NOTICE( [  AX_SWIG_PYTHON_LIBS     = $AX_SWIG_PYTHON_LIBS ])

dnl n. AX_SWIG_PYTHON does AC_REQUIRE of AX_PKG_SWIG, AX_PYTHON_DEVEL
AC_MSG_NOTICE( [Calling ax_swig_python...] )
AX_SWIG_PYTHON
AC_MSG_NOTICE( [ Set by ax_swig_python:] )
AC_MSG_NOTICE( [   AX_SWIG_PYTHON_CPPFLAGS = $AX_SWIG_PYTHON_CPPFLAGS ] )
AC_MSG_NOTICE( [   AX_SWIG_PYTHON_OPT      = $AX_SWIG_PYTHON_OPT ] )
AC_MSG_NOTICE( [   AX_SWIG_PYTHON_LIBS     = $AX_SWIG_PYTHON_LIBS ] )


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

dnl Commented out because no longer using hidapi
dnl TODO: allow for either hidapi-libusb or hidapi-hidraw
dnl PKG_CHECK_MODULES(HIDAPI, hidapi-hidraw)
dnl PKG_CHECK_MODULES(HIDAPI, hidapi-libusb)


###
### Checks for header files.
###

AC_CHECK_HEADERS([fcntl.h langinfo.h libintl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h termios.h unistd.h wchar.h  dlfcn.h])
dnl i2c-dev.h is in linux-headers
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


dnl Were we building SWIG support for more than Python, there would need to be 2 configuration option, 
dnl one to control SWIG and one to enable the Python bindings.   
dnl Since that day is far off, we have just 1 option to enable SWIG for Python

dnl *** configure option: --enable-swig
AC_ARG_ENABLE([swig],
              [ AS_HELP_STRING( [--enable-swig=@<:@no/yes@:>@], [Build SWIG interface@<:@default=no@:>@] )],
              [enable_swig=${enableval}],
              [enable_swig=no] )
dnl Set flag for automake.am:
AM_CONDITIONAL([ENABLE_SWIG_COND], [test "x$enable_swig" == "xyes"] )
AS_IF([test "x$enable_swig" == "xyes"],
      AC_MSG_NOTICE( [swig... enabled]  ),
      AC_MSG_NOTICE( [swig... disabled] )
     )
dnl AC_SUBST([ENABLE_SWIG_FLAG], [test "x$enable_swig" == "xyes"; $?])
dnl AC_SUBST([ENABLE_SWIG_FLAG], [xy])
AS_IF([test "x$enable_swig" == "xyes"],
      AC_SUBST([ENABLE_SWIG_FLAG], [1] ),
      AC_SUBST([ENABLE_SWIG_FLAG], [0]  )
     )

dnl Alternative, copied from somewhere

#default behavior is to install the python bindings into subfolders of $prefix
#however, this may require the user to set the PYTHONPATH environment variable
#in order to avoid this, invoke configure with the --enable-python-integration option

AC_ARG_ENABLE([python-integration],[AS_HELP_STRING([--enable-python-integration],[install the python bindings in the interpreters site-packages folder])],[enable_python_integration=$enableval],[enable_python_integration=check])



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

dnl *** configure option: --enable-failsim
AC_ARG_ENABLE([failsim],
              [ AS_HELP_STRING( [--enable-failsim=@<:@no/yes@:>@], [Build with failure simulation@<:@default=no@:>@] )],
              [enable_failsim=${enableval}],
              [enable_failsim=no] )
AM_CONDITIONAL([ENABLE_FAILSIM_COND], [test "x$enable_failsim" == "xyes"] )
dnl Defines $(ENABLE_FAILSIM_FLAG) in Makefile.in 
AS_IF([test "x$enable_failsim" == "xyes"],
      [ AC_SUBST([ENABLE_FAILSIM_FLAG], [1] )
        AC_DEFINE( [ENABLE_FAILSIM], [1], [If defined, enable failsim.])
        AC_MSG_NOTICE( [failsim..... enabled] )
      ],
      [ AC_SUBST([ENABLE_FAILSIM_FLAG], [0]  )
        AC_MSG_NOTICE( [failsim..... disabled] )
      ]
     )
dnl Define ENABLE_FAILSIM in config.h:
AS_IF([test "x$enable_failsim" == "xyes"],
     AC_DEFINE_UNQUOTED( [ENABLE_FAILSIM], [1], [If defined, enable failsim.]),
     [])
dnl AS_IF([test "x$enable_failsim" == "xyes"],
dnl      AC_MSG_NOTICE( [failsim... enabled]  ),
dnl       AC_MSG_NOTICE( [failsim... disabled] )
dnl     )


###
### Conditional library tests 
###

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


AS_IF([ test "x$swig_found" == "xyes" -a "x$python_found" == "xyes"],
      [ python_and_swig_found="yes"],
      [ python_and_swig_found="no"]
     )
AC_MSG_NOTICE( [python_and_swig_found  = $python_and_swig_found ])

AS_IF([test "x$enable_swig" == "xyes" -a  "x$python_and_swig_found" != "xyes" ] ,
      AC_MSG_ERROR( [--enable-swig requires that both Python and SWIG be installed] ) 
     )


if test "x$enable_swig" = "xyes" ; then
  AC_MSG_NOTICE([Building with Python bindings])

  dnl if test "x$enable_python_integration" = xyes ; then
  dnl         pythondir=$PYTHON_SITE_PKG
  dnl         pyexecdir=$PYTHON_SITE_PKG_EXEC
  dnl fi


  dnl AC_SUBST(PYTHONDIR,$pythondir)
  dnl AC_SUBST(PKGPYTHONDIR,$pkgpythondir)
  dnl AC_SUBST(PYEXECDIR,$pyexecdir)
  dnl AC_SUBST(PKGPYEXECDIR,$pkgpyexecdir)

  AC_MSG_NOTICE( [pythondir=$pythondir])
  AC_MSG_NOTICE( [pkgpythondir=$pkgpythondir])
  AC_MSG_NOTICE( [pyexecdir=$pyexecdir])
  AC_MSG_NOTICE( [pkgpyexecdir=$pkgpyexecdir])

fi

AM_CONDITIONAL([ENABLE_PYTHON],[test x$enable_swig = xyes])


### 
### End options for configure script
###


###
### Generate output   
###

AC_OUTPUT

dnl m4_include([m4/ax_pkg_swig.m4])
