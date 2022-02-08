#
# ddcutil autotools configure script 
#

# Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
# SPDX-License-Identifier: GPL-2.0-or-later

dnl Generl notes:  
dnl - Macro names in comments are written in lower case to avoid processing as actual macros

###
### Initial Setup
###

AC_PREREQ([2.69])

m4_define([ddcutil_major_version], [1])
m4_define([ddcutil_minor_version], [2])
m4_define([ddcutil_micro_version], [3])
dnl ddcutil_version_suffix does not begin with hyphen, e.g. "dev", not "-dev"
m4_define([ddcutil_version_suffix], ["dev"])
# m4_ifelse(ddcutil_version_suffix,[], m4_define([ddcutil_version], [a]),define([ddcutil_version],[b]))
dnl mf_define( [ddcutil_version], m4_format([%s.%s.%s%s],[ddcutil_major_version],[ddcutil_minor_version],[ddcutil_micro_version],[ddcutil_version_suffix])
m4_define([ddcutil_version], [ddcutil_major_version.ddcutil_minor_version.ddcutil_micro_version])
dnl m4_ifdef('ddcutil_version_suffix' ,m4_define([ddcutil_version], [ddcutil_major_version.ddcutil_minor_version.ddcutil_micro_version]))

dnl It should be possible to define here the "fully qualified" version name, conditionally
dnl containing the version suffix, but after hours of trying to get that to work (5/2021) 
dnl I gave up.  It will be handled in the C code.

dnl process command line args, perform initializations
dnl sets among several output vars including  PACKAGE_NAME, PACKAGE_VERSION,
dnl PACKAGE_TARNAME (name of output package)
dnl causes VERSION to be set in config.h
AC_INIT([ddcutil],[ddcutil_version],[rockowitz@minsoft.com])

dnl old unused cruft
dnl AC_DEFINE( [PACKAGE_VMAJOR], [ ddcutil_major_version  ], [ddcutil major version] )
dnl AC_DEFINE( [PACKAGE_VMINOR], [ ddcutil_minor_version  ], [ddcutil minor version] )
dnl AC_DEFINE( [PACKAGE_VMICRO], [ ddcutil_micro_version  ], [ddcutil micro version] )
dnl AC_DEFINE( [PACKAGE_VSUFFIX],[ ddcutil_version_suffix ], [ddcutil version suffix] )

dnl define preprocessor symbols
dnl was AC_DEFINE_UNQUOTED, but don't need extra handling?
dnl         symbol             value set by               comment 
AC_DEFINE( [VERSION_VMAJOR], [ ddcutil_major_version  ], [ddcutil major version] )
AC_DEFINE( [VERSION_VMINOR], [ ddcutil_minor_version  ], [ddcutil minor version] )
AC_DEFINE( [VERSION_VMICRO], [ ddcutil_micro_version  ], [ddcutil micro version] )
AC_DEFINE( [VERSION_VSUFFIX],[ ddcutil_version_suffix ], [ddcutil version suffix] )
dnl AC_DEFINE( [VERSION_SUFFIX], [ ddcutil_version_suffix ], [version suffix] )

dnl substitute @PACKAGE_MAJOR@ etc. in Makefile.am with the value of the environment variable
AC_SUBST( VERSION_VMAJOR,  [ddcutil_major_version] )
AC_SUBST( VERSION_VMINOR,  [ddcutil_minor_version] )
AC_SUBST( VERSION_VMICRO,  [ddcutil_micro_version] )
AC_SUBST( VERSION_VSUFFIX, [ddcutil_version_suffix] )
dnl old: 
dnl AC_SUBST( PACKAGE_VMAJOR,  [ddcutil_major_version] )
dnl AC_SUBST( PACKAGE_VMINOR,  [ddcutil_minor_version] )
dnl AC_SUBST( PACKAGE_VMICRO,  [ddcutil_micro_version] )
dnl AC_SUBST( PACKAGE_VSUFFIX, [ddcutil_version_suffix] )

AC_ARG_VAR(DBG, [Turn on script debugging messages(0/1)])
dnl AC_MSG_NOTICE([DBG = |$DBG|])

AS_IF( [test 0$DBG -ne 0], 
   AC_MSG_NOTICE([debug messages enabled]),
   AC_MSG_NOTICE([debug messages disabled])
   )

dnl reduce clutter,  save files litmain.sh, config.guess, missing etc. here instead of top directory
AC_CONFIG_AUX_DIR(config)
dnl sanity check: verify a unique file in source directory: 
AC_CONFIG_SRCDIR([src/util/coredefs.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_FILES([
	Makefile 
	src/Makefile 
	src/util/Makefile
	src/usb_util/Makefile
	src/base/Makefile
	src/vcp/Makefile
	src/dynvcp/Makefile
	src/i2c/Makefile
	src/usb/Makefile
	src/ddc/Makefile
	src/test/Makefile
	src/cmdline/Makefile
	src/app_sysenv/Makefile
	src/sample_clients/Makefile
	man/Makefile 
	data/Makefile 
	docs/Makefile
	docs/doxygen/Makefile
	src/public/ddcutil_macros.h
  ddcutil.pc
  ],
 )
dnl AC_CONFIG_FILES(package/upload_obsrpm, [chmod +x package/upload_obsrpm] )
dnl dnl AC_CONFIG_FILES(package/build_dpkg, [chmod +x package/build_dpkg] )
dnl dnl cannot chmod on build_dpkg, upload_obsrpm, since they will not exist within dpkg build environment

dnl not working, why?
# AC_DEFINE_UNQUOTED([DDCUTIL_MAJOR_VERSION], [$ddcutil_major_version], [ddcutil major version])


dnl Automake options to be applied to every Makefile.am in the tree: 
dnl The effect is as if each option were listed in AUTOMAKE_OPTIONS
dnl removed -Werror from AM_INIT_AUTOMAKE to allow compilation to proceed
dnl n. first option in our list is the required automake version
AM_INIT_AUTOMAKE([1.13 -Wall -Wno-extra-portability foreign subdir-objects ])

dnl alternatively, add "silent-rules" to AM_INIT_AUTOMAKE
dnl Eclipse warns that as of 2.68, AM_SILENT_RULES takes 0 arguments
dnl but in 2.69 it only defaults to silen with argument "yes"
dnl with "yes" arg, silent rules is the default
AM_SILENT_RULES([yes])

AM_PROG_AR

dnl explicitly initialize pkg-config in case first call to pkg_check_modules is within an if test:
PKG_PROG_PKG_CONFIG
required_packages= 

dnl Determines C compiler to use, sets output variable cc, ac_cv_prog_cc_c89
dnl Called by other macros, but must be called explicitly at top level for proper initialization
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
dnl AC_PROG_CC_STDC

# for CENTOS 7 in OBS, EOL 6/30/2024
AC_PROG_CC_C99

dnl AC_PROG_LIBTOOL, AM_PROG_LIBTOOL are deprecated names for older versions of LT_INIT
dnl adds support for --enable/disable -static/shared, -with/without-pic configure flags
LT_INIT([disable-static])

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

dnl 7/29/19 first use, LT_CURRENT set to 1
dnl temporary note: prev was 4 0 0 
LT_CURRENT=6
LT_REVISION=0
LT_AGE=2
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)


### 
### Recognize command options for configure script
### 

###
### Documented options
###

AC_MSG_NOTICE( [Checking configure command options...] )

dnl # residual setting so that Makefile.am files don't break
adl_header_dir=""
AM_CONDITIONAL([HAVE_ADL_COND], [test -n "$adl_header_dir"] )

dnl *** configure option: --enable-lib
AC_ARG_ENABLE([lib],
              [ AS_HELP_STRING([--enable-lib=@<:@yes/no@:>@], [Build shared library and clients@<:@default=yes@:>@] )],
              [enable_lib=${enableval}],
              [enable_lib=yes] )
dnl Set flag for automake.am:
AM_CONDITIONAL([ENABLE_SHARED_LIB_COND], [test "x$enable_lib" = "xyes"] )
dnl ENABLE_SHARED_LIB_FLAG used in package/ddcutil_spec.in
AS_IF([test "x$enable_lib" = "xyes"],
      AC_MSG_NOTICE( [lib...        enabled] )
      AC_SUBST(ENABLE_SHARED_LIB_FLAG, 1)
	  AC_DEFINE( [BUILD_SHARED_LIB], [1], [If defined, buid shared library.])
     ,
      AC_MSG_NOTICE( [lib...        disabled] )
      AC_SUBST(ENABLE_SHARED_LIB_FLAG, 0)
     )

dnl *** configure option: --enable-envcmds
AC_ARG_ENABLE([envcmds],
              [ AS_HELP_STRING([--enable-envcmds=@<:@yes/no@:>@], [Include environment and usbenvironment@<:@default=yes@:>@] )],
              [enable_envcmds=${enableval}],
              [enable_envcmds=yes] )
dnl AS_IF([test "x$enable_envcmds" = "xyes"],
dnl         AC_MSG_NOTICE( [envcmds...    enabled (provisional) ]  )
dnl       ,
dnl         AC_MSG_NOTICE( [envcmds...    disabled] )
dnl      )

dnl *** configure option: --enable-udev
AC_ARG_ENABLE([udev],
              [ AS_HELP_STRING([--enable-udev=@<:@yes/no@:>@], [Use UDEV@<:@default=yes@:>@] )],
              [enable_udev=${enableval}],
              [enable_udev=yes] )
dnl AS_IF([test "x$enable_udev" = "xyes"],
dnl         AC_MSG_NOTICE( [udev...       enabled (provisional) ]  )
dnl       ,
dnl         AC_MSG_NOTICE( [udev...       disabled] )
dnl      )

dnl *** configure option: --enable-usb
AC_ARG_ENABLE([usb],
              [ AS_HELP_STRING( [--enable-usb=@<:@yes/no@:>@], [Support USB connected displays@<:@default=yes@:>@] )],
              [enable_usb=${enableval}],
              [enable_usb=yes] )
AS_IF([test "x$enable_usb" = "xyes"],
        AC_MSG_NOTICE( [usb...        enabled (provisional) ]  )
      ,
        AC_MSG_NOTICE( [usb...        disabled] )
     )

dnl *** configure option: --enable-drm
AC_ARG_ENABLE([drm],
              [ AS_HELP_STRING( [--enable-drm=@<:@yes/no@:>@], [Use DRM in diagnostics@<:@default=yes@:>@] )],
              [enable_drm=${enableval}],
              [enable_drm=yes] )
AS_IF([test "x$enable_drm" = "xyes"],
        AC_MSG_NOTICE( [drm...        enabled (provisional) ]  )
      ,
        AC_MSG_NOTICE( [drm...        disabled] )
     )
             
dnl *** configure option: --enable-x11
AC_ARG_ENABLE([x11],
              [ AS_HELP_STRING( [--enable-x11=@<:@yes/no@:>@], [Use X11 in diagnostics@<:@default=yes@:>@] )],
              [enable_x11=${enableval}],
              [enable_x11=yes] )
dnl AS_IF([test "x$enable_x11" = "xyes"],
dnl       AC_MSG_NOTICE( [x11...      enabled (provisional)]  ),
dnl       AC_MSG_NOTICE( [x11...      disabled] )
dnl      )              


dnl *** configure option: --enable-asan
AC_ARG_ENABLE([asan],
              [ AS_HELP_STRING( [--enable-asan=@<:@no/yes@:>@], [Build for asan (address sanitizer)@<:@default=no@:>@] )],
              [enable_asan=${enableval}],
              [enable_asan=no] )
AM_CONDITIONAL([ASAN_COND], [test "x$enable_asan" = "xyes"] )
AS_IF( [test "x$enable_asan" = "xyes"],
         AC_DEFINE( [WITH_ASAN], [1], [If defined, building for Asan instrumentation.])
         AC_MSG_NOTICE( [asan..........enabled] )
      ,
         AC_MSG_NOTICE( [asan..........disabled] )
     )


dnl *** configure option: --enable-targetbsd
AC_ARG_ENABLE([targetbsd],
              [ AS_HELP_STRING([--enable-targetbsd=@<:@no/yes@:>@], [Build for BSD@<:@default=no@:>@ (Developer-only)] )],
              [enable_targetbsd=${enableval}],
              [enable_targetbsd=no] )


dnl *** configure option: --enable-doxygen
AC_ARG_ENABLE([doxygen],
              [ AS_HELP_STRING( [--enable-doxygen=@<:@no/yes@:>@], [Build API documentation using Doxygen (if it is installed)@<:@default=no@:>@ (Developer-only)] )],
              [enable_doxygen=${enableval}],
              [enable_doxygen=no] )
AM_CONDITIONAL([ENABLE_DOXYGEN_COND], [test "x$enable_doxygen" = "xyes"] )
AS_IF([test "x$enable_doxygen" = "xyes"],
        AC_MSG_NOTICE( [doxygen...    enabled]  )
      ,
        AC_MSG_NOTICE( [doxygen...    disabled] )
     )

###
### Resolve choices for public options
###

AC_MSG_NOTICE( [Resolving options...] ) 
dnl --enable-targetbsd => --disable-envcmds, --disable-usb, --disable-udev

AS_IF([test "x$enable_targetbsd" = "xyes" -a "x$enable_envcmds" = "xyes" ],
       [ AC_MSG_NOTICE( [--enable-targetbsd forces --disable-envcmds] )
        enable_envcmds=no
       ]
   ) 

AS_IF([test "x$enable_targetbsd" = "xyes" -a "x$enable_udev" = "xyes" ],
       [ AC_MSG_NOTICE( [--enable-targetbsd forces --disable-udev] )
        enable_udev=no
       ]
   ) 

AS_IF([test "x$enable_targetbsd" = "xyes" -a "x$enable_usb" = "xyes" ],
       [ AC_MSG_NOTICE( [--enable-targetbsd forces --disable-usb] )
        enable_usb=no
       ]
   ) 

AS_IF([test "x$enable_udev" != "xyes" -a "x$enable_usb" = "xyes" ],
       [ AC_MSG_NOTICE( [--disable-udev forces --disable-usb] )
        enable_usb=no
       ]
   ) 


# --disable-envcmds => --disable-drm, --disable-x11

AS_IF([test "x$enable_envcmds" != "xyes" -a "x$enable_drm" = "xyes" ],
       [ AC_MSG_NOTICE( [--disable-envcmds forces --disable-drm] )
        enable_drm=no
       ]
   ) 

AS_IF([test "x$enable_envcmds" != "xyes" -a "x$enable_x11" = "xyes" ],
       [ AC_MSG_NOTICE( [--disable-envcmds forces --disable-x11] )
        enable_x11=no
       ]
   ) 


###
### Report resolved options and set conditionals, substitutions, and defines
###

AM_CONDITIONAL([ENABLE_TARGETBSD_COND], [test "x$enable_targetbsd" = "xyes"] )
AS_IF([test "x$enable_targetbsd" = "xyes"],
      AC_MSG_NOTICE( [targetbsd...  enabled] )
      AC_DEFINE( [TARGET_BSD], [1], [If defined, building for BSD.])
      AC_SUBST(ENABLE_TARGETBSD_FLAG, 1) 
     ,
      AC_MSG_NOTICE( [targetbsd...  disabled] )
      AC_SUBST(ENABLE_TARGETBSD_FLAG, 0)
      AC_DEFINE( [TARGET_LINUX], [1], [If defined, building for Linux.])
     )

AM_CONDITIONAL([ENABLE_ENVCMDS_COND], [test "x$enable_envcmds" = "xyes"] )
AS_IF([test "x$enable_envcmds" = "xyes"],
      AC_MSG_NOTICE( [envcmds...    enabled] )
      AC_DEFINE( [ENABLE_ENVCMDS], [1], [If defined, enable environment commands.])
      AC_SUBST(ENABLE_ENVCMDS_FLAG, 1) 
     ,
      AC_MSG_NOTICE( [envcmds...    disabled] )
      AC_SUBST(ENABLE_ENVCMDS_FLAG, 0)
     )

AM_CONDITIONAL([ENABLE_UDEV_COND], [test "x$enable_udev" = "xyes"] )
AS_IF([test "x$enable_udev" = "xyes"],
      AC_MSG_NOTICE( [udev...       enabled] )
      AC_DEFINE( [ENABLE_UDEV], [1], [If defined, use UDEV.])
      AC_SUBST(ENABLE_UDEV_FLAG, 1) 
     ,
      AC_MSG_NOTICE( [udev...       disabled] )
      AC_SUBST(ENABLE_UDEV_FLAG, 0)
     )

AM_CONDITIONAL([ENABLE_USB_COND], [test "x$enable_usb" = "xyes"] )
AS_IF([test "x$enable_usb" = "xyes"],
        AC_DEFINE( [USE_USB], [1], [If defined, use usb.])
        AC_SUBST(  ENABLE_USB_FLAG, [1] )
        AC_MSG_NOTICE( [usb...        enabled]  )
      ,
        AC_SUBST( ENABLE_USB_FLAG, [0] )
        AC_MSG_NOTICE( [usb...        disabled] )
     )

AS_IF([test "x$enable_x11" = "xyes"],
        AC_MSG_NOTICE( [x11...        enabled]  )
      ,
        AC_MSG_NOTICE( [x11...        disabled] )
     )


### Private options

dnl dnl *** configure option: --enable-yaml
dnl AC_ARG_ENABLE([yaml],
dnl               [ AS_HELP_STRING( [--enable-yaml=@<:@no/yes@:>@], [Enable YAML for external file parsing @<:@default=no@:>@] )],
dnl               [enable_yaml=${enableval}],
dnl               [enable_yaml=no] )
dnl AM_CONDITIONAL([ENABLE_YAML_COND], [test "x$enable_yaml" = "xyes"] )
dnl AS_IF([test "x$enable_yaml" = "xyes"],
dnl       AC_DEFINE( [ENABLE_YAML], [1], [Enable YAML for parsing.]) 
dnl       AC_MSG_NOTICE( [yaml... enabled]  )
dnl       ,
dnl       AC_MSG_NOTICE( [yaml... disabled] )
dnl     )


dnl *** configure option: --enable-testcases
AC_ARG_ENABLE([testcases],
              [ AS_HELP_STRING( [--enable-testcases=@<:@no/yes@:>@], [Include test cases @<:@default=no@:>@] (Developer-only) )],
              [include_testcases=${enableval}],
              [include_testcases=no] )
AM_CONDITIONAL([INCLUDE_TESTCASES_COND], [test "x$include_testcases" = "xyes"] )
AS_IF([test "x$include_testcases" = "xyes"],
      AC_DEFINE( [INCLUDE_TESTCASES], [1], [If defined, build with test cases.]) 
      AC_MSG_NOTICE( [testcases...  enabled]  )
      ,
      AC_MSG_NOTICE( [testcases...  disabled] )
     )

dnl *** configure option: --enable-callgraph
AC_ARG_ENABLE([callgraph],
              [ AS_HELP_STRING( [--enable-callgraph=@<:@no/yes@:>@], [Create .expand files for static call graph@<:@default=no@:>@] (Developer-only) )],
              [enable_callgraph=${enableval}],
              [enable_callgraph=no] )
AM_CONDITIONAL([ENABLE_CALLGRAPH_COND], [test "x$enable_callgraph" = "xyes"] )
AS_IF([test "x$enable_callgraph" = "xyes"],
      AC_MSG_NOTICE( [callgraph...  enabled]  ),
      AC_MSG_NOTICE( [callgraph...  disabled] )
     )

dnl *** configure option: --enable-failsim
AC_ARG_ENABLE([failsim],
              [ AS_HELP_STRING( [--enable-failsim=@<:@no/yes@:>@], [Build with failure simulation@<:@default=no@:>@ (Developer-only)] )],
              [enable_failsim=${enableval}],
              [enable_failsim=no] )
AM_CONDITIONAL([ENABLE_FAILSIM_COND], [test "x$enable_failsim" = "xyes"] )
AS_IF( [test "x$enable_failsim" = "xyes"],
         AC_DEFINE( [ENABLE_FAILSIM], [1], [If defined, enable failsim.])
         AC_MSG_NOTICE( [failsim.....  enabled] )
      ,
         AC_MSG_NOTICE( [failsim.....  disabled] )
     )

dnl *** configure option: --enable-force-suse
AC_ARG_ENABLE([force-suse],
              [ AS_HELP_STRING( [--enable-force-suse=@<:@no/yes@:>@], [Force SUSE target directories@<:@default=no@:>@ (Developer-only)]  )],
              [enable_force_suse=${enableval}],
              [enable_force_suse=no] )
AM_CONDITIONAL([ENABLE_FORCE_SUSE_COND], [test "x$enable_force_suse" = "xyes"] )
AS_IF( [test "x$enable_force_suse" = "xyes"],
         AC_DEFINE( [ENABLE_FORCE_SUSE], [1], [If defined, force SUSE target directories.])
         AC_MSG_NOTICE( [force-suse....enabled] )
      ,
         AC_MSG_NOTICE( [force-suse....disabled] )
     )


dnl Note to self: use autoheader to regenerate config.h.in whenever new defines added 


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

AC_CHECK_HEADERS([fcntl.h langinfo.h libintl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h termios.h unistd.h wchar.h  dlfcn.h execinfo.h])
dnl i2c-dev.h is in linux-headers
dnl i2c-dev.h not found:
dnl AC_CHECK_HEADERS([i2c-dev.h])

dnl libi2c and libi2c-dev have no .pc files.  Check for header file instead.
AC_CHECK_HEADER( [i2c/smbus.h],
                    AC_MSG_NOTICE( [header file i2c/smbus.h found.] ), 
                    AC_MSG_ERROR( [libi2c development package (e.g. libi2c-dev, name varies by distribution) >= 4.0 required.] )
					)

dnl on openSUSE, libkmod.h is found in a subdirectory
AC_CHECK_HEADER( [kmod/libkmod.h],
                    AC_DEFINE([LIBKMOD_H_SUBDIR_KMOD], [1], [libkmod.h location is include/kmod])
                    AC_MSG_NOTICE( [header file libkmod.h found in subdirectory include/kmod]  )
               ,
	            AC_MSG_NOTICE( [header file libkmod.h not in subdirectory include/kmod]    )

               )


###
### Required library tests
###

dnl Notes on pkg_check_modules:
dnl 1) appends to xxx_CFLAGS and xxx_LIBS the output of pkg-config --cflags|--lib 
dnl 2) if no action-if-false branch defined, pkg_check_modules terminates execution if not found

dnl was 2.36, can we drop this to 2.14 to allow for Debian 7.0
dnl 9/2017: need >= 2.32 for g_thread_...() functions
PKG_CHECK_MODULES(GLIB,  glib-2.0 >= 2.32) 
required_packages="$required_packages glib-2.0 >= 2.32"

PKG_CHECK_MODULES(KMOD, libkmod)
required_packages="$required_packages kmod"

AS_IF( [test "x$enable_udev" = "xyes"],
  [   PKG_CHECK_MODULES(UDEV,   libudev, 
       [ libudev_found=1],  
       [ libudev_found=0
           AC_MSG_NOTICE( [The package providing libudev.h varies by Linux distribution and release.] )
           AC_MSG_NOTICE( [It may be a udev specific package, e.g. libudev-dev, libudev-devel] )
           AC_MSG_NOTICE( [or it may be part of systemd, e.g systemd-devel] )
           AC_MSG_ERROR( [libudev not found] )
       ] )
       ]
     ,
     )
required_packages="$required_packages xrandr x11"
dnl how to handle libudev?   punt for now


###
### Optional library tests 
###

dnl TODO: use consistent pattern

### libusb 

dnl know that 1.0.8 fails, 1.0.20  works
AS_IF([test "x$enable_usb" = "xyes"],
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

AS_IF([test "x$enable_drm" = "xyes"],
         [ PKG_CHECK_MODULES(LIBDRM, libdrm >= 2.4.67, 
             [libdrm_found=yes], 
             [libdrm_found=no
              AC_MSG_WARN( [libdrm >= 2.4.67 not found. Forcing --disable-drm])
              enable_drm=no
             ]
           ) 
         ],
         [ AC_MSG_NOTICE( [drm disabled, not checking for libdrm] ) ]
     )

AM_CONDITIONAL([USE_LIBDRM_COND], [test "x$enable_drm" = "xyes"] )
AS_IF([test "x$enable_drm" = "xyes"],
        AC_DEFINE([USE_LIBDRM], [1], [Use libdrm])
        AC_MSG_NOTICE( [drm...        enabled]  )
      ,
        AC_MSG_NOTICE( [drm...        disabled] )
     )

AS_IF( [test 0$DBG -ne 0], 
       [
          AC_MSG_NOTICE( [LIBDRM_CFLAGS:    $LIBDRM_CFLAGS] )
          AC_MSG_NOTICE( [LIBDRM_LIBS:      $LIBDRM_LIBS] )
       ]) 


 
dnl ### libyaml 
dnl dnl PKG_CHECK_MODULES(YAML, yaml-0.1)
dnl 
dnl AS_IF([test "x$enable_yaml" = "xyes"],
dnl          [ PKG_CHECK_MODULES(YAML, yaml-0.1, 
dnl              [libyaml_found=yes], 
dnl              [libyaml_found=no
dnl               AC_MSG_WARN( [yaml-0.1  not found. Forcing --disable-yaml])
dnl               enable_yaml=no
dnl              ]
dnl            ) 
dnl          ],
dnl          [ AC_MSG_NOTICE( [yaml disabled, not checking for libyaml] ) ]
dnl      )
dnl 
dnl AM_CONDITIONAL([ENABLE_YAML_COND], [test "x$enable_yaml" = "xyes"] )
dnl AS_IF([test "x$enable_yaml" = "xyes"],
dnl         AC_DEFINE([ENABLE_YAML], [1], [Use yaml])
dnl         AC_MSG_NOTICE( [yaml... enabled]  )
dnl       ,
dnl         AC_MSG_NOTICE( [yaml... disabled] )
dnl      )
dnl 
dnl AS_IF( [test 0$DBG -ne 0], 
dnl        [
dnl           AC_MSG_NOTICE( [YAML_CFLAGS:    $YAML_CFLAGS] )
dnl           AC_MSG_NOTICE( [YAML_LIBS:      $YAML_LIBS] )
dnl        ]) 
 
### X11 

dnl PKG_CHECK_MODULES(XRANDR, xrandr)
dnl PKG_CHECK_MODULES(X11,    x11)

dnl Duplicate of next section?

dnl TODO: disable if enable-envcmds is false?

dnl Force disable of options if environment commands disabled

AS_IF([test "x$enable_x11" = "xyes"],
         [ PKG_CHECK_MODULES(LIBX11, x11)
           PKG_CHECK_MODULES(XRANDR, xrandr)
         ]) 

AM_CONDITIONAL([USE_X11_COND], [test "x$enable_x11" = "xyes"] )
AS_IF([test "x$enable_x11" = "xyes"],
        AC_DEFINE([USE_X11], [1], [Use X11])
        AC_MSG_NOTICE( [x11...        enabled]  )
      ,
        AC_MSG_NOTICE( [x11...        disabled] )
     )

dnl AS_IF([test "x$enable_x11" = "xyes"],
dnl          [ PKG_CHECK_MODULES(XRANDR, xrandr)  
dnl         ])


dnl Note cflags and libs, but don't need to use in makefiles 
AS_IF( [test 0$DBG -ne 0], 
       [
          AC_MSG_NOTICE( [LIBX11_CFLAGS:    $LIBX11_CFLAGS] )
          AC_MSG_NOTICE( [LIBX11_LIBS:      $LIBX11_LIBS] )
       ]) 



### DOXYGEN

dnl AC_MSG_NOTICE([Start of DOXYGEN])
dnl AS_IF( [test "x$enable_doxygen" = "xyes"],
dnl         AC_MSG_NOTICE( [doxygen... enabled]  )
dnl       ,
dnl         AC_MSG_NOTICE( [doxygen... disabled] )
dnl      )

AS_IF([test "x$enable_doxygen" = "xyes"], 
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

dnl AM_CONDITIONAL( [USE_DOXYGEN], [test -n "$DOXYGEN" -a "x$enable_doxygen" = "xyes"])
AM_CONDITIONAL( [USE_DOXYGEN], [test "x$enable_doxygen" = "xyes"])

AM_COND_IF([USE_DOXYGEN], 
    AC_MSG_NOTICE([USE_DOXYGEN is set])
    ,
    AC_MSG_NOTICE([USE_DOXYGEN not set])
    )


### DOC-BASE

dnl n. doc-base is Debian specific
dnl doc-base does not have pc file.
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

AS_IF([test "x$enable_gobject" = "xyes"],
     [ PKG_CHECK_MODULES(GOBJECT,  gobject-2.0 >= 2.14)  ],
     )

### Library

AS_IF([test "x$enable_lib" = "xyes"],
     [ PKG_CHECK_MODULES(ZLIB,  zlib)  ],
     )



AS_IF( [test "x$enable_asan" = "xyes"], [
   required_packages="asan $required_packages"
   # has no effect on $CFLAGS
   CFLAGS="$CFLAGS -fsanitize=address"
   CFLAGS="$CFLAGS -fsanitize-address-use-after-scope -fno-omit-frame-pointer"
   ])


dnl AC_MSG_NOTICE([======================== required_packages:   $required_packages])
AC_SUBST(REQUIRED_PACKAGES,$required_packages)


### Opsys variability

dnl Fails: if building in OBS, SUSE is in /proc/version, even when building for Fedora
dnl TODO: do not modify if explicitly set
dnl AC_MSG_NOTICE([Original docdir:   ${docdir}])
dnl AS_IF( [ grep SUSE /proc/version ], [
dnl 	AC_MSG_NOTICE( [IS SUSE]) 
dnl         docdir=\${datarootdir}/doc/packages/\${PACKAGE_TARNAME}
dnl 	], [
dnl	AC_MSG_NOTICE( [NOT SUSE] )
dnl 	] )


dnl AC_MSG_NOTICE([======> Tests for SUSE target:])

dnl test using  [ grep SUSE /proc/version ] 
dnl always tests true on OBS, i.e reports the OS on which the build system is running, not the target os

dnl test using  [ ls -d /usr/share/doc/packages ]
dnl always tests true on OBS 

dnl test using  [test "x$enable_force_suse" = "xyes"], i.e. enable-force_suse parm passed to configure
dnl works

dnl test using  [ grep suse /etc/os-release ]
dnl successfully detects target OS when run on OBS

dnl test using  [lsb-release -i | grep SUSE ]
dnl on OBS, command not found


AC_MSG_CHECKING( [target OS using /etc/os-release] )
AS_IF( [ grep suse /etc/os-release > /dev/null], [
   AC_MSG_RESULT( [target is SUSE]) 
   docdir=\${datarootdir}/doc/packages/\${PACKAGE_TARNAME}
   AC_MSG_NOTICE( [..Forcing docdir to ${docdir}] )
   ], [
   AC_MSG_RESULT( [target is not SUSE ] )
   ] )



###
### Generate output   
###

dnl called once at end of configure.ac, generates and runs config.status
AC_OUTPUT

dnl a brief summary
AC_MSG_RESULT([
	ddcutil $VERSION
	version suffix:   ddcutil_version_suffix
	=============

	libtool version  	${LT_CURRENT}:${LT_REVISION}:${LT_AGE}

	prefix:                 ${prefix}
	exec_prefix:		${exec_prefix}
	libexecdir:		${libexecdir}
	bindir:                 ${bindir}
	libdir:			${libdir}
	datarootdir:            ${datarootdir}
	datadir:                ${datadir}
	docdir:                 ${docdir}
	mandir:               	${mandir}
	includedir:		${includedir}
	pkgconfigdir:           ${pkgconfigdir}
	required_packages:      ${required_packages}

	enable_lib:             ${enable_lib}
	enable_envcmds          ${enable_envcmds}
	enable_udev             ${enable_udev}
	enable_usb:             ${enable_usb}
	enable_drm:             ${enable_drm}   
	enable_x11:             ${enable_x11}   
	enable_asan:            ${enable_asan}

	Developer-only options:
	enable_targetbsd:       ${enable_targetbsd}
	enable_doxygen:         ${enable_doxygen}
	enable_failsim:         ${enable_failsim}
	include_testcases:      ${include_testcases}

	compiler:               ${CC}
	CFLAGS:                 ${CFLAGS}
	CPPFLAGS:               ${CPPFLAGS}
	LDFLAGS:                ${LDFLAGS}
])
dnl cat config.h
