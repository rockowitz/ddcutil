#
# ddcutil configure script 
#
# Copyright 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


dnl Note on comments: 
dnl Macro names in comments are written in lower case to avoid processing as actual macros


###
### Initial Setup
###

AC_PREREQ([2.69])

m4_define([ddcutil_major_version], [0])
m4_define([ddcutil_minor_version], [7])
dnl temp micro version for development
m4_define([ddcutil_micro_version], [2])
m4_define([ddcutil_version],
          [ddcutil_major_version.ddcutil_minor_version.ddcutil_micro_version])

dnl causes VERSION to be set in config.h
AC_INIT([ddcutil], [ddcutil_version], [rockowitz@minsoft.com])

AC_ARG_VAR(DBG, [Turn on script debugging messages(0/1)])
dnl AC_MSG_NOTICE([DBG = |$DBG|])

AS_IF( [test 0$DBG -ne 0], 
   AC_MSG_NOTICE([debug messages enabled]),
   AC_MSG_NOTICE([debug messages disabled])
   )

dnl dnl uncomment the following line to enable debug messages from configure:
dnl debug_config=yes
dnl AS_IF( [ test -n "$debug_config"], 
dnl    AC_MSG_NOTICE( [debug messages enabled ] ),
dnl    AC_MSG_NOTICE( [debug messages disabled] )
dnl    )

AC_CONFIG_AUX_DIR(config)
dnl sanity check: check for a unique file in source directory: 
AC_CONFIG_SRCDIR([src/util/coredefs.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
# removed from list: package/debian/changelog
AC_CONFIG_FILES([
	Makefile 
	src/Makefile 
  src/util/Makefile
  src/usb_util/Makefile
  src/swig/Makefile 
  src/gobject_api/Makefile 
	man/Makefile 
	data/Makefile 
  docs/Makefile
  docs/doxygen/Makefile
	package/Makefile 
	package/ddcutil.spec 
  ddcutil.pc
  ],
 )
AC_CONFIG_FILES(package/upload_obsrpm, [chmod +x package/upload_obsrpm] )
AC_CONFIG_FILES(package/build_dpkg, [chmod +x package/build_dpkg] )
dnl cannot chmod on build_dpkg, upload_obsrpm, since they will not exist within dpkg build environment

dnl removed -Werror from AM_INIT_AUTOMAKE to allow compilation to proceed
AM_INIT_AUTOMAKE([1.11 -Wall -Wno-extra-portability foreign subdir-objects])
dnl AM_SILENT_RULES defined as of automake 1.11, so don't need to test for macro's existence
AM_SILENT_RULES([yes])
AM_PROG_AR

dnl explicitly initialize pkg-config in case first call to pkg_check_modules is within an if test:
PKG_PROG_PKG_CONFIG
required_packages= 

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

dnl Automatically update libtool script if it becomes out of date:
AC_SUBST([LIBTOOL_DEPS])


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
# AGE       backward compatibility (i.e. number of releases prior to current 
#           for which this release is backward compatible)
#
# Alternative comments: 
#
# Here are a set of rules to help you update your library version
# information:
#
#  1. Start with version information of `0:0:0' for each libtool library.
#  2. Update the version information only immediately before a public
#     release of your software. More frequent updates are unnecessary, and
#     only guarantee that the current interface number gets larger faster.
#  3. If the library source code has changed at all since the last update,
#     then increment revision (`c:r:a' becomes `c:r+1:a').
#  4. If any interfaces have been added, removed, or changed since the last
#     update, increment current, and set revision to 0.
#  5. If any interfaces have been added since the last public release, then
#     increment age.
#  6. If any interfaces have been removed since the last public release,
#     then set age to 0.
#

# The LT_... values are used to create the argument for the --version-info parm. 
# Note that this parm is processed differently depending on operating system.  
# For Linux, the second and third fields in the shared object name's suffix are
# taken directly from the command line, while the first is calculated as current-age.  
# For example, if LT_CURRENT=13, LT_REVISION=4, LT_AGE=4, the geneated parm 
# is --version-info "13:1:4", and the generated SO name looks like xxx.so.9.4.1

dnl Temporarily leave all fields 0 until first public release.
LT_CURRENT=0
LT_REVISION=0
LT_AGE=0
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

dnl for debian/changelog - should do it some other way
DEBIAN_RELEASE=1
DEBIAN_DISTRIBUTION=xenial
AC_SUBST(DEBIAN_RELEASE)
AC_SUBST(DEBIAN_DISTRIBUTION)


### 
### Recognize command options for configure script
### 

### Documented options

AC_MSG_NOTICE( [Checking configure command options...] )

dnl *** configure option: --with-adl-headers
AC_ARG_WITH([adl-headers],
            [ AS_HELP_STRING([--with-adl-headers=DIR], [Directory containing ADL header files]) ],
            [adl_header_dir=$withval],
            [adl_header_dir=[]])
AC_MSG_NOTICE([adl_header_dir = $adl_header_dir])
AS_IF( [test -n "$adl_header_dir"],
         AC_DEFINE( [HAVE_ADL], [1], [If defined, ADL headers are present.  Build with ADL support.])
         AC_MSG_NOTICE( [adl... enabled]  )
        ,
         AC_MSG_NOTICE( [adl... disabled] )
     )
AM_CONDITIONAL([HAVE_ADL_COND], [test -n "$adl_header_dir"] )
AC_SUBST([ADL_HEADER_DIR], [$adl_header_dir])


dnl *** configure option: --enable-lib
AC_ARG_ENABLE([lib],
              [ AS_HELP_STRING([--enable-lib=@<:@yes/no@:>@], [Build shared library and clients@<:@default=yes@:>@] )],
              [enable_lib=${enableval}],
              [enable_lib=yes] )
dnl Set flag for automake.am:
AM_CONDITIONAL([ENABLE_SHARED_LIB_COND], [test "x$enable_lib" == "xyes"] )
dnl ENABLE_SHARED_LIB_FLAG used in package/ddcutil_spec.in
AS_IF([test "x$enable_lib" == "xyes"],
      AC_MSG_NOTICE( [lib... enabled] )
      AC_SUBST(ENABLE_SHARED_LIB_FLAG, 1) 
     ,
      AC_MSG_NOTICE( [lib... disabled] )
      AC_SUBST(ENABLE_SHARED_LIB_FLAG, 0)
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


dnl TODO: Review
dnl copied from tschoonj configure.ac snippet: https://gist.github.com/tschoonj/6441999

dnl #default behavior is to install the python bindings into subfolders of $prefix
dnl #however, this may require the user to set the PYTHONPATH environment variable
dnl #in order to avoid this, invoke configure with the --enable-python-integration option

dnl AC_ARG_ENABLE([python-integration],[AS_HELP_STRING([--enable-python-integration],[install the python bindings in the interpreters site-packages folder])],[enable_python_integration=$enableval],[enable_python_integration=check])


dnl *** configure option: --enable-usb
AC_ARG_ENABLE([usb],
              [ AS_HELP_STRING( [--enable-usb=@<:@yes/no@:>@], [Support USB connected displays@<:@default=yes@:>@] )],
              [enable_usb=${enableval}],
              [enable_usb=yes] )
AM_CONDITIONAL([ENABLE_USB_COND], [test "x$enable_usb" == "xyes"] )
dnl ENABLE_USB_FLAG used in package/ddcutil.apec.in
AS_IF([test "x$enable_usb" == "xyes"],
        AC_DEFINE( [USE_USB], [1], [If defined, use usb.])
        AC_SUBST(  ENABLE_USB_FLAG, [1] )
        AC_MSG_NOTICE( [usb... enabled]  )
      ,
        AC_SUBST( ENABLE_USB_FLAG, [0] )
        AC_MSG_NOTICE( [usb... disabled] )
     )


dnl *** configure option: --enable-drm
AC_ARG_ENABLE([drm],
              [ AS_HELP_STRING( [--enable-drm=@<:@yes/no@:>@], [Use DRM in diagnostics@<:@default=yes@:>@] )],
              [enable_drm=${enableval}],
              [enable_drm=yes] )
AM_CONDITIONAL([USE_LIBDRM_COND], [test "x$enable_drm" == "xyes"] )
AS_IF([test "x$enable_drm" == "xyes"],
        AC_DEFINE([USE_LIBDRM], [1], [Use libdrm])
        AC_MSG_NOTICE( [drm... enabled]  )
      ,
        AC_MSG_NOTICE( [drm... disabled] )
     )


dnl *** configure option: --enable-doxygen
AC_ARG_ENABLE([doxygen],
              [ AS_HELP_STRING( [--enable-doxygen=@<:@no/yes@:>@], [Build API documentation using Doxygen (if it is installed)@<:@default=no@:>@] )],
              [enable_doxygen=${enableval}],
              [enable_doxygen=no] )
AM_CONDITIONAL([ENABLE_DOXYGEN_COND], [test "x$enable_doxygen" == "xyes"] )
AS_IF([test "x$enable_doxygen" == "xyes"],
        AC_MSG_NOTICE( [doxygen... enabled]  )
      ,
        AC_MSG_NOTICE( [doxygen... disabled] )
     )



### Private options

dnl *** configure option: --enable-testcases
AC_ARG_ENABLE([testcases],
              [ AS_HELP_STRING( [--enable-testcases=@<:@no/yes@:>@], [Include test cases @<:@default=no@:>@] )],
              [include_testcases=${enableval}],
              [include_testcases=no] )
AM_CONDITIONAL([INCLUDE_TESTCASES_COND], [test "x$include_testcases" == "xyes"] )
AS_IF([test "x$include_testcases" == "xyes"],
      AC_DEFINE( [INCLUDE_TESTCASES], [1], [If defined, build with test cases.]) 
      AC_MSG_NOTICE( [testcases... enabled]  )
      ,
      AC_MSG_NOTICE( [testcases... disabled] )
     )

dnl *** configure option: --enable-callgraph
AC_ARG_ENABLE([callgraph],
              [ AS_HELP_STRING( [--enable-callgraph=@<:@no/yes@:>@], [Create .expand files for static call graph@<:@default=no@:>@] )],
              [enable_callgraph=${enableval}],
              [enable_callgraph=no] )
AM_CONDITIONAL([ENABLE_CALLGRAPH_COND], [test "x$enable_callgraph" == "xyes"] )
AS_IF([test "x$enable_callgraph" == "xyes"],
      AC_MSG_NOTICE( [callgraph... enabled]  ),
      AC_MSG_NOTICE( [callgraph... disabled] )
     )

dnl GObject implementation has been mothballed.  
dnl TODO: delete or comment out this section
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

dnl *** configure option: --enable-failsim
AC_ARG_ENABLE([failsim],
              [ AS_HELP_STRING( [--enable-failsim=@<:@no/yes@:>@], [Build with failure simulation@<:@default=no@:>@] )],
              [enable_failsim=${enableval}],
              [enable_failsim=no] )
AM_CONDITIONAL([ENABLE_FAILSIM_COND], [test "x$enable_failsim" == "xyes"] )
AS_IF( [test "x$enable_failsim" == "xyes"],
         AC_DEFINE( [ENABLE_FAILSIM], [1], [If defined, enable failsim.])
         AC_MSG_NOTICE( [failsim..... enabled] )
      ,
         AC_MSG_NOTICE( [failsim..... disabled] )
     )

###
### Checks for typedefs, structures, and compiler characteristics.
###

AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_C_BIGENDIAN
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T


###
### Checks for standard library functions.
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
### Checks for header files.
###

AC_CHECK_HEADERS([fcntl.h langinfo.h libintl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h termios.h unistd.h wchar.h  dlfcn.h])
dnl i2c-dev.h is in linux-headers
dnl i2c-dev.h not found:
dnl AC_CHECK_HEADERS([i2c-dev.h])


###
### Required library tests
###

dnl Notes on pkg_check_modules:
dnl 1) appends to xxx_CFLAGS and xxx_LIBS the output of pkg-config --cflags|--lib 
dnl 2) if no action-if-false branch defined, pkg_check_modules terminates execution if not found

dnl was 2.36, can we drop this to 2.14 to allow for Debian 7.0
PKG_CHECK_MODULES(GLIB,  glib-2.0 >= 2.14) 
required_packages="$required_packages glib-2.0 >= 2.14"

dnl Not currently used
dnl PKG_CHECK_MODULES(OSINFO, libosinfo-1.0 >= 0.1, [libosinfo_found=yes], [libosinfo_found=no])

PKG_CHECK_MODULES(XRANDR, xrandr)
PKG_CHECK_MODULES(X11,    x11)
PKG_CHECK_MODULES(UDEV,   libudev, 
     [libudev_found=1],  
     [libudev_found=0
      AC_MSG_NOTICE( [The package providing libudev.h varies by Linux distribution and release.] )
      AC_MSG_NOTICE( [It may be a udev specific package, e.g. libudev-dev, libudev-devel] )
      AC_MSG_NOTICE( [or it may be part of systemd, e.g systemd-devel] )
      AC_MSG_ERROR( [libudev not found] )
     ] )
required_packages="$required_packages xrandr x11"
dnl how to handle libudev?   punt for now


###
### Optional library tests 
###

dnl TODO: use consistent pattern

### libusb 

dnl know that 1.0.8 fails, 1.0.20  works
AS_IF([test "x$enable_usb" == "xyes"],
         [ PKG_CHECK_MODULES(LIBUSB, libusb-1.0 >= 1.0.15, [libusb_found=yes] ) ],
         [ AC_MSG_NOTICE( [usb disabled, not checking for libusb] ) ]
     )

dnl Logically, these debug messages belong within the $enable_usb test, but the 
dnl nested brackests make the code hard to read.  It's LISP all over again.
AS_IF( [test 0$DBG -ne 0], 
       [
          AC_MSG_NOTICE( [LIBUSB_CFLAGS:    $LIBUSB_CFLAGS] )
          AC_MSG_NOTICE( [LIBUSB_LIBS:      $LIBUSB_LIBS] )
       ]) 

### libdrm 

AS_IF([test "x$enable_usb" == "xyes"],
         [ PKG_CHECK_MODULES(LIBDRM, libdrm >= 2.4.67, [libdrm_found=yes] ) ],
         [ AC_MSG_NOTICE( [drm disabled, not checking for libdrm] ) ]
     )

AS_IF( [test 0$DBG -ne 0], 
       [
          AC_MSG_NOTICE( [LIBDRM_CFLAGS:    $LIBDRM_CFLAGS] )
          AC_MSG_NOTICE( [LIBDRM_LIBS:      $LIBDRM_LIBS] )
       ]) 



### DOXYGEN

dnl AC_MSG_NOTICE([Start of DOXYGEN])
dnl AS_IF( [test "x$enable_doxygen" == "xyes"],
dnl         AC_MSG_NOTICE( [doxygen... enabled]  )
dnl       ,
dnl         AC_MSG_NOTICE( [doxygen... disabled] )
dnl      )

AS_IF([test "x$enable_doxygen" == "xyes"], 
      [
        dnl checks for doxygen program, sets or uses environment variable DOXYGEN
        AC_MSG_NOTICE([Checking for Doxygen...])
        FLM_PROG_TRY_DOXYGEN

        AS_IF( [test -n $DOXYGEN], 
          [
            AC_MSG_NOTICE([Calling dx_init_doxygen...])
            DX_PDF_FEATURE(ON)
            DX_HTML_FEATURE(ON)
            DX_INIT_DOXYGEN(ddcutil)
            AS_IF( [test 0$DBG -ne 0], 
               [
                  AC_MSG_NOTICE([Set by dx_init_doxygen:])
                  AC_MSG_NOTICE([  DOXYGEN:             $DOXYGEN            ])
                  AC_MSG_NOTICE([  dx_DOT_FEATURE:      $DX_DOT_FEATURE     ])
                  AC_MSG_NOTICE([  dx_FEATURE_doc       $DX_FEATURE_doc     ])
                  AC_MSG_NOTICE([  dx_DOXYGEN_FEATURE:  $DX_DOXYGEN_FEATURE ])
                  AC_MSG_NOTICE([  dx_HTML_FEATURE:     $DX_HTML_FEATURE    ])
                  AC_MSG_NOTICE([  dx_FLAG_html:        $DX_FLAG_HTML       ])
                  AC_MSG_NOTICE([  dx_PDF_FEATURE:      $DX_PDF_FEATURE     ])
                  AC_MSG_NOTICE([  DX_PROJECT:          $DX_PROJECT         ])
                  AC_MSG_NOTICE([  DX_CONFIG:           $DX_CONFIG          ])
                  AC_MSG_NOTICE([  DX_DOCDIR:           $DX_DOCDIR          ])
               ])
            AC_CONFIG_FILES( [docs/doxygen/doxyfile] )
          ]
          ,
          [
             AC_MSG_NOTICE([doxygen not found]) 
             enable_doxygen=no
          ]
        )
      ]
      ,
      [
        AC_MSG_NOTICE([doxygen disabled, not checking for Doxygen]) 
      ]
    )
dnl AC_MSG_NOTICE([enable_doxygen = ${enable_doxygen}])
dnl AM_CONDITIONAL( [HAVE_DOXYGEN], [test -n "$DOXYGEN"] )

dnl AM_CONDITIONAL( [USE_DOXYGEN], [test -n "$DOXYGEN" -a "x$enable_doxygen" == "xyes"])
AM_CONDITIONAL( [USE_DOXYGEN], [test "x$enable_doxygen" == "xyes"])

AM_COND_IF([USE_DOXYGEN], 
    AC_MSG_NOTICE([USE_DOXYGEN is set])
    ,
    AC_MSG_NOTICE([USE_DOXYGEN not set])
    )


### DOC-BASE

dnl n. doc-base is Debain specific
dnl doc-base doesn't have pc file.
AC_CHECK_PROGS( [DOCBASE_INSTALL_DOCS], [install-docs])
AS_IF( [test -n "$DOCBASE_INSTALL_DOCS"], 
          AC_MSG_NOTICE([doc-base execuable found])
          AC_CONFIG_FILES([docs/ddcutil-c-api]), 
          AC_MSG_WARN([doc-base not installed - continuing without doc-base support])
     )
AM_CONDITIONAL( [HAVE_DOCBASE], [test -n "$DOCBASE_INSTALL_DOCS"])


### GObject

# Vestigial.  GObject interface no longer being developed.

m4_ifdef([GOBJECT_INTROSPECTION_CHECK], [GOBJECT_INTROSPECTION_CHECK([1.30.0])])
AM_CONDITIONAL(HAVE_INTROSPECTION, test "x$found_introspection" = xyes)

AS_IF([test "x$enable_gobject" == "xyes"],
     [ PKG_CHECK_MODULES(GOBJECT,  gobject-2.0 >= 2.14)  ],
     )


### Python and SWIG

dnl Locate SWIG and PYTHON in case we need them

dnl Python integration is a mess.   See for example:
dnl   https://tschoonj.github.io/blog/2013/09/04/building-libtool-modules-python-bindings/
dnl   https://gist.github.com/tschoonj/6441999
dnl   https://lists.ubuntu.com/archives/ubuntu-devel/2009-February/027439.html
dnl   https://bugzilla.redhat.com/show_bug.cgi?id=533920

dnl Some of the issues: 
dnl - Python 3 is a distint language from Python 2, yet AM_PYTHON treats it as an upwardly 
dnl   compatible version Python 2. 
dnl - How to build for multiple version fo Python 2 and Python 3? 
dnl - Debian (and derivatives such as Ubuntu) have modified the directory scheme. 
dnl   Directory names end with dist-packags instead of site-packages.  
dnl   But this is incompletely implemented.   sysconfig.get_path() returns names
dnl   ending with site-package on python 3
dnl - sys.path is built using calls to site.get_site_packages().  But this function 
dnl   hardcodes the paths.  There is no reference to sysconfig.get_path() or 
dnl   distusils.sysconfig_get_python_lib().  So code that relies on the latter functions
dnl   can return a library that is not in sys.path
dnl - Where do local extensions to the distribution version of Python belong? 
dnl   In a ditrectory under /usr/local or under /usr? 
dnl - pkg-config names can vary. 
dnl   Fedora 25:  python and python2 refer to current Python2 
dnl               python3, python-3.5, python-3.5m refer to current Python 3
dnl   openSUSE 42.2:  
dnl            python3 not defined, python-3.4 refers to current Python 3 
dnl   So need to know the version number of Python 3 to construct a name
dnl   to pass to pkg-config


AS_IF( [ test "x$enable_swig" == "xyes" ], [
   AC_MSG_NOTICE( [SWIG enabled, checking for Python and SWIG...] )

   # Python 3 

	 AC_MSG_NOTICE( [Calling ax_path_python3 for Python 3...] )
	 AX_PATH_PYTHON3(3.1, [python3_found=yes], [python_found3=no]) 
	 AS_IF( [test 0$DBG -ne 0], [
      AC_MSG_NOTICE( [Set by ax_path_python3 for python3:] )
      AC_MSG_NOTICE( [  python3_found        = $python3_found] )
	    AC_MSG_NOTICE( [  PYTHON3              = $PYTHON3] )
	    AC_MSG_NOTICE( [  PYTHON3_VERSION      = $PYTHON3_VERSION] )
      AC_MSG_NOTICE( [  PYTHON3_PREFIX       = $PYTHON3_PREFIX] )
      AC_MSG_NOTICE( [  PYTHON3_EXEC_PREFIX  = $PYTHON3_EXEC_PREFIX] )
	    AC_MSG_NOTICE( [  python3dir           = $python3dir] )
	    AC_MSG_NOTICE( [  pkgpython3dir        = $pkgpython3dir] )
	    AC_MSG_NOTICE( [  py3execdir           = $py3execdir] )
	    AC_MSG_NOTICE( [  pkgpy3execdir        = $pkgpy3execdir] )
	 ])

   AS_IF( [test "x$python3_found" == "xyes" ], [
      AC_MSG_NOTICE( [Looking for Python 3 pkgconfig information...] )

      PKG_CHECK_MODULES([PY3], [python-$PYTHON3_VERSION], [
         enable_py3="yes"
         AS_IF( [test 0$DBG -ne 0], [
            AC_MSG_NOTICE( [  PY3_CFLAGS    = $PY3_CFLAGS ])
            AC_MSG_NOTICE( [  PY3_LIBS      = $PY3_LIBS   ])
         ])
         dnl PY3_CFLAGS and PY3_LIBS are precious, no need to AC_SUBST() 
         AC_SUBST(PY3_CFLAGS)
         AC_SUBST(PY3_LIBS)
      ],[
         AC_MSG_WARN([Building without Python 3 support])
         python3_found="no"
         enable_py3="no"
      ])
   ],[
      AC_MSG_NOTICE( [Skipping check for Python3 pkgconfig] )
   ])

   AS_IF( [test "x$enable_py3" == "xyes" ], [
      AX_PYTHON_ENV($PYTHON3)
      AC_MSG_NOTICE([PY3_EXTRA_LDFLAGS:  $PY3_EXTRA_LDFLAGS])   
      AC_MSG_NOTICE([PY3_EXTRA_LIBS      $PY3_EXTRA_LIBS])
      AC_MSG_NOTICE([usr_py3execdir:     $usr_py3execdir])

      dnl AC_SUBST() unnecessary, precious variables
      AC_SUBST(PY3_EXTRA_LDFLAGS)
      AC_SUBST(PY3_EXTRA_LIBS)
   ])


   # Python 2

   AC_MSG_NOTICE( [Calling am_path_python for Python 2...] )
	 AM_PATH_PYTHON(2.6, [python_found=yes], [python_found=no]) 
	 AS_IF( [test 0$DBG -ne 0], [
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
   ])

   AS_IF( [ test "x$python_found" == "xyes"], [
	   AM_PYTHON_CHECK_VERSION($PYTHON, [3.0], [
	      AC_MSG_NOTICE( [No Python 2.x found])
	      python_found = "no"
      ])
   ])

   AS_IF( [test "x$python_found" == "xyes" ], [
      AC_MSG_NOTICE( [Looking for Python2 pkgconfig information...] )

      PKG_CHECK_MODULES([PY2], [python-$PYTHON_VERSION], [
         enable_py2="yes"
         AS_IF( [test 0$DBG -ne 0], [
            AC_MSG_NOTICE( [Set by pkg_check_modules:   ] )
            AC_MSG_NOTICE( [  PY2_CFLAGS  = $PY2_CFLAGS ] )
            AC_MSG_NOTICE( [  PY2_LIBS    = $PY2_LIBS   ] )
         ])
         dnl unnecessary, precious
         AC_SUBST(PY2_CFLAGS, $PY2_CFLAGS)
         AC_SUBST(PY2_LIBS,     $PY2_LIBS)
      ],[
         AC_MSG_WARN([Building without Python 2 support])
         python2_found="no"
         enable_py2="no"
     ])
   ],[
      AC_MSG_NOTICE( [Skipping check for Python2 pkgconfig] )
   ])


   AS_IF( [test "x$enable_py2" == "xyes" ], [
      AX_PYTHON_ENV($PYTHON)
      AC_MSG_NOTICE([PY2_EXTRA_LDFLAGS:  $PY2_EXTRA_LDFLAGS])   
      AC_MSG_NOTICE([PY2_EXTRA_LIBS      $PY3_EXTRA_LIBS])
      AC_MSG_NOTICE([usr_py2execdir:     $usr_py2execdir])
      AC_MSG_NOTICE([alt_py2execdir:     $alt_py2execdir])

      dnl precious variables
      AC_SUBST(PY2_EXTRA_LDFLAGS)
      AC_SUBST(PY2_EXTRA_LIBS)
   ])


   # SWIG 

   dnl enable_py2=no
   dnl enable_py3=no
   dnl AC_MSG_NOTICE( [enable_py2:    $enable_py2" ])
   dnl AC_MSG_NOTICE( [enable_py3:    $enable_py3" ])
   AS_IF( [ test "x$enable_py2" == "xyes" -o  "x$enable_py3" == "xyes"], 
     enable_some_python="yes"
   )
   AC_MSG_NOTICE( [ enable_some_python:   $enable_some_python ])

   AS_IF( [test "x$enable_some_python" == "xyes"], [

      AC_MSG_NOTICE( [Calling ax_pkg_swig...] )

      dnl ax_pkg_swig respects SWIG_LIB, but overrides SWIG, so don't expose the possibility of overriding
      dnl AC_ARG_VAR(SWIG, [swig executable])
      dnl AC_ARG_VAR(SWIG_LIB, [linker flags for swig])
      dnl AC_MSG_NOTICE( [Before ax_pkg_swig:] )
      dnl AC_MSG_NOTICE( [  SWIG                    = $SWIG ] )
      dnl AC_MSG_NOTICE( [  SWIG_LIB                = $SWIG_LIB ])

      AX_PKG_SWIG(1.3.21, [swig_found=yes], [swig_found=no])
      AS_IF( [test 0$DBG -ne 0], [
         AC_MSG_NOTICE( [Set by ax_pkg_swig:] )
         AC_MSG_NOTICE( [  swig_found              = $swig_found] )
         AC_MSG_NOTICE( [  SWIG                    = $SWIG ] )
         AC_MSG_NOTICE( [  SWIG_LIB                = $SWIG_LIB ])
      ])

      dnl AX_SWIG_MULTI_MODULE_SUPPORT

      dnl dnl n.b. will fail if python2 not installed
      dnl dnl AX_PYTHON_DEVEL Calls AC_PATH_PROG() to find $PYTHON, fails if python not found
      dnl dnl PYTHON_SITE_PKG_EXEC set only in _alt version:
      dnl AS_IF( [test "x$python_found" == "xyes" ], [
      dnl    AC_MSG_NOTICE( [Calling ax_python_devel for informational purposes...])
      dnl    AX_PYTHON_DEVEL

      dnl    AS_IF( [test 0$DBG -ne 0], [
      dnl       AC_MSG_NOTICE( [From ax_python_devel or ac_python_devel_alt..  ] )
		  dnl       AC_MSG_NOTICE( [  PYTHON_CPPFLAGS      = $PYTHON_CPPFLAGS      ] )
		  dnl       AC_MSG_NOTICE( [  PYTHON_SITE_PKG      = $PYTHON_SITE_PKG    ] )

		  dnl       AC_MSG_NOTICE( [From ax_python_devel only ...  ] )
		  dnl       AC_MSG_NOTICE( [  PYTHON_LIBS          = $PYTHON_LIBS          ] )
		  dnl       AC_MSG_NOTICE( [  PYTHON_EXTRA_LDFLAGS = $PYTHON_EXTRA_LDFLAGS    ] )
		  dnl       AC_MSG_NOTICE( [  PYTHON_EXTRA_LIBS    = $PYTHON_EXTRA_LIBS    ] )

		  dnl       AC_MSG_NOTICE( [From ax_python_devel_alt only ...  ] )
		  dnl       AC_MSG_NOTICE( [  PYTHON_LDFLAGS       = $PYTHON_LDFLAGS       ] )
		  dnl       AC_MSG_NOTICE( [  PYTHON_SITE_PKG_EXEC = $PYTHON_SITE_PKG_EXEC    ] )
		  dnl       AC_MSG_NOTICE( [  PYTHON_CFLAGS       = $PYTHON_CFLAGS] )
		  dnl    ])
		  dnl ])

      dnl dnl n. AX_SWIG_PYTHON does AC_REQUIRE of AX_PKG_SWIG, AX_PYTHON_DEVEL
      dnl dnl AC_MSG_NOTICE( [Calling ax_swig_python...] )
      dnl AX_SWIG_PYTHON
      dnl AS_IF( [test 0$DBG -ne 0], [
      dnl    AC_MSG_NOTICE( [ Set by ax_swig_python:] )
      dnl    AC_MSG_NOTICE( [   AX_SWIG_PYTHON_CPPFLAGS = $AX_SWIG_PYTHON_CPPFLAGS ] )
      dnl    AC_MSG_NOTICE( [   AX_SWIG_PYTHON_OPT      = $AX_SWIG_PYTHON_OPT ] )
      dnl    AC_MSG_NOTICE( [   AX_SWIG_PYTHON_LIBS     = $AX_SWIG_PYTHON_LIBS ] )
      dnl ])

      AS_IF([ test "x$swig_found" == "xyes" -a "x$enable_some_python" == "xyes"],
            [ python_and_swig_found="yes"],
            [ python_and_swig_found="no"]
           )
      AC_MSG_NOTICE( [python_and_swig_found  = $python_and_swig_found ])

      AS_IF([test "x$enable_swig" == "xyes" -a  "x$python_and_swig_found" != "xyes" ] ,
            AC_MSG_ERROR( [--enable-swig requires some version of Python, its development package, and SWIG] ) 
        )

   ],[
     AC_MSG_NOTICE([Skipping SWIG check])
   ])

],
[
   AC_MSG_NOTICE( [SWIG disabled, skipping checks for Python and SWIG...])
])

AM_CONDITIONAL([ENABLE_PY2], [test x"$enable_py2" = xyes])
AM_COND_IF([ENABLE_PY2], [AC_DEFINE([ENABLE_PY2], [1], [Python2 is enabled])])

AM_CONDITIONAL([ENABLE_PY3], [test x"$enable_py3" = xyes])
AM_COND_IF([ENABLE_PY3], [AC_DEFINE([ENABLE_PY3], [1], [Python3 is enabled])])


dnl AC_MSG_NOTICE([prefix = ${prefix}])
dnl AC_MSG_NOTICE(ac_default_[prefix = ${ac_default_prefix}])
dnl AX_FIX_PYTHON_PATH($PYTHON, [pyexecdir], $pyexecdir, $prefix)

if test "x$enable_swig" = "xyes" ; then
  AC_MSG_NOTICE([Building with Python bindings])

  dnl if test "x$enable_python_integration" = xyes ; then
  dnl         pythondir=$PYTHON_SITE_PKG
  dnl         pyexecdir=$PYTHON_SITE_PKG_EXEC
  dnl fi


  dnl AC_SUBST(PYTHONDIR,    $pythondir)
  dnl AC_SUBST(PKGPYTHONDIR, $pkgpythondir)
  dnl AC_SUBST(PYEXECDIR,    $pyexecdir)
  dnl AC_SUBST(PKGPYEXECDIR, $pkgpyexecdir)

	AS_IF([ test "x$swig_found" == "xyes" -a "x$python_found" == "xyes"],
	  AC_MSG_NOTICE( [pythondir    = $pythondir]    )
	  AC_MSG_NOTICE( [pkgpythondir = $pkgpythondir] )
	  AC_MSG_NOTICE( [pyexecdir    = $pyexecdir]    )
	  AC_MSG_NOTICE( [pkgpyexecdir = $pkgpyexecdir] )
:
    AC_SUBST(PYEXECDIR,$pyexecdir)
    AC_CONFIG_FILES( [src/swig/pylocal] )
    AC_CONFIG_FILES( [src/swig/set_pylocal_exec] )

	)


  AC_ARG_VAR([PY2_EXECDIR], [Python 2 C extension directory])
  AC_ARG_VAR([PY3_EXECDIR], [Python 3 C extension directory])

  AS_IF( [test 0$DBG -ne 0], [
     AC_MSG_NOTICE( [PY2_EXECDIR    = $PY2_EXECDIR])
     AC_MSG_NOTICE( [PY3_EXECDIR    = $PY3_EXECDIR])
     AC_MSG_NOTICE( [py2execdir     = $py2execdir])
     AC_MSG_NOTICE( [py3execdir     = $py3execdir])
     AC_MSG_NOTICE( [usr_py2execdir     = $usr_py2execdir])
     AC_MSG_NOTICE( [usr_py3execdir     = $usr_py3execdir])
  ])

  AS_IF( [ test -z "$PY2_EXECDIR" ], [
     PY2_EXECDIR=${usr_py2execdir}
  ] )
  AS_IF( [ test -z "$PY3_EXECDIR" ], [
     PY3_EXECDIR=${usr_py3execdir}
  ] )

  py2execdir=$PY2_EXECDIR
  py3execdir=$PY3_EXECDIR

  AS_IF( [test 0$DBG -ne 0], [
     AC_MSG_NOTICE( [====================== Wolf 8])
     AC_MSG_NOTICE( [py2execdir     = $py2execdir])
     AC_MSG_NOTICE( [py3execdir     = $py3execdir])
  ])

  AC_SUBST(py2execdir,   $py2execdir)
  AC_SUBST(py3execdir,   $py3execdir)

fi

AM_CONDITIONAL([ENABLE_PYTHON_SWIG],[test x$enable_swig   == xyes])
dnl temporary crude flag:
AM_CONDITIONAL([HAVE_PYTHON3],      [test x$enable_py3 == xyes])


dnl AC_MSG_NOTICE([======================== required_packages:   $required_packages])
AC_SUBST(REQUIRED_PACKAGES,$required_packages)


###
### Generate output   
###

AC_OUTPUT

dnl a brief summary
AC_MSG_RESULT([
	ddcutil $VERSION
	=============

	libtool version  	${LT_CURRENT}:${LT_REVISION}:${LT_AGE}

	prefix:                 ${prefix}
	datarootdir:            ${datarootdir}
	datadir:                ${datadir}
	mandir:               	${mandir}

	adl_header_dir:         ${adl_header_dir}
	enable_lib:             ${enable_lib}
	enable_swig:            ${enable_swig}
	enable_usb:             ${enable_usb}
	enable_drm:             ${enable_drm}   
	enable_doxygen:         ${enable_doxygen}
	enable_failsim:         ${enable_failsim}

	compiler:               ${CC}
	cflags:                 ${CFLAGS}
	ldflags:                ${LDFLAGS}
])

