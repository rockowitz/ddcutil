## ------------------------                                 -*- Autoconf -*-
## Python file handling
## From Andrew Dalke
## Updated by James Henstridge
## ------------------------
# Copyright (C) 1999-2014 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# Modiied version of AM_PATH_PYTHON that only looks for Python 3 versions. 
# Uses separate variables so that AM_PATH_PYTHON can be used to find 
# Python 2, and AX_PATH_PYTHON3 can be use for Python 3
# 


# AX_PATH_PYTHON3([MINIMUM-VERSION], [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# ---------------------------------------------------------------------------
# Adds support for distributing Python modules and packages.  To
# install modules, copy them to $(pythondir), using the python_PYTHON
# automake variable.  To install a package with the same name as the
# automake package, install to $(pkgpythondir), or use the
# pkgpython_PYTHON automake variable.
#
# The variables $(pyexecdir) and $(pkgpyexecdir) are provided as
# locations to install python extension modules (shared libraries).
# Another macro is required to find the appropriate flags to compile
# extension modules.
#
# If your package is configured with a different prefix to python,
# users will have to add the install directory to the PYTHONPATH
# environment variable, or create a .pth file (see the python
# documentation for details).
#
# If the MINIMUM-VERSION argument is passed, AM_PATH_PYTHON will
# cause an error if the version of python installed on the system
# doesn't meet the requirement.  MINIMUM-VERSION should consist of
# numbers and dots only.
AC_DEFUN([AX_PATH_PYTHON3],
 [
  dnl Find a Python interpreter.  Python versions prior to 2.0 are not
  dnl supported. (2.0 was released on October 16, 2000).
  m4_define_default([_AM_PYTHON_INTERPRETER_LIST3],
[python3 python3.3 python3.2 python3.1 python3.0 ])

  AC_ARG_VAR([PYTHON3], [the Python 3 interpreter])

  m4_if([$1],[],[
    dnl No version check is needed.
    # Find any Python interpreter.
    if test -z "$PYTHON3"; then
      AC_PATH_PROGS([PYTHON3], _AM_PYTHON_INTERPRETER_LIST, :)
    fi
    am_display_PYTHON3=python3
  ], [
    dnl A version check is needed.
    if test -n "$PYTHON3"; then
      # If the user set $PYTHON3, use it and don't search something else.
      AC_MSG_CHECKING([whether $PYTHON3 version is >= $1])
      AM_PYTHON_CHECK_VERSION([$PYTHON3], [$1],
			      [AC_MSG_RESULT([yes])],
			      [AC_MSG_RESULT([no])
			       AC_MSG_ERROR([Python3 interpreter is too old])])
      am_display_PYTHON3=$PYTHON3
    else
      # Otherwise, try each interpreter until we find one that satisfies
      # VERSION.
      AC_CACHE_CHECK([for a Python interpreter with version >= $1],
	[am_cv_pathless_PYTHON3],[
	for am_cv_pathless_PYTHON3 in _AM_PYTHON_INTERPRETER_LIST3 none; do
	  test "$am_cv_pathless_PYTHON3" = none && break
	  AM_PYTHON_CHECK_VERSION([$am_cv_pathless_PYTHON3], [$1], [break])
	done])
      # Set $PYTHON to the absolute path of $am_cv_pathless_PYTHON3.
      if test "$am_cv_pathless_PYTHON3" = none; then
	PYTHON3=:
      else
        AC_PATH_PROG([PYTHON3], [$am_cv_pathless_PYTHON3])
      fi
      am_display_PYTHON3=$am_cv_pathless_PYTHON3
    fi
  ])

  if test "$PYTHON3" = :; then
  dnl Run any user-specified action, or abort.
    m4_default([$3], [AC_MSG_ERROR([no suitable Python3 interpreter found])])
  else

  dnl Query Python for its version number.  Getting [:3] seems to be
  dnl the best way to do this; it's what "site.py" does in the standard
  dnl library.

  AC_CACHE_CHECK([for $am_display_PYTHON3 version], [am_cv_python_version3],
    [am_cv_python_version3=`$PYTHON3 -c "import sys; sys.stdout.write(sys.version[[:3]])"`])
  AC_SUBST([PYTHON3_VERSION], [$am_cv_python_version3])

  dnl Use the values of $prefix and $exec_prefix for the corresponding
  dnl values of PYTHON_PREFIX and PYTHON_EXEC_PREFIX.  These are made
  dnl distinct variables so they can be overridden if need be.  However,
  dnl general consensus is that you shouldn't need this ability.

  AC_SUBST([PYTHON3_PREFIX], ['${prefix}'])
  AC_SUBST([PYTHON3_EXEC_PREFIX], ['${exec_prefix}'])

  dnl At times (like when building shared libraries) you may want
  dnl to know which OS platform Python thinks this is.

  AC_CACHE_CHECK([for $am_display_PYTHON3 platform], [am_cv_python3_platform],
    [am_cv_python3_platform=`$PYTHON3 -c "import sys; sys.stdout.write(sys.platform)"`])
  AC_SUBST([PYTHON3_PLATFORM], [$am_cv_python3_platform])

  # Just factor out some code duplication.
  am_python3_setup_sysconfig="\
import sys
# Prefer sysconfig over distutils.sysconfig, for better compatibility
# with python 3.x.  See automake bug#10227.
try:
    import sysconfig
except ImportError:
    can_use_sysconfig = 0
else:
    # can_use_sysconfig = 1
    can_use_sysconfig =  0
# Can't use sysconfig in CPython 2.7, since it's broken in virtualenvs:
# <https://github.com/pypa/virtualenv/issues/118>
try:
    from platform import python_implementation
    if python_implementation() == 'CPython' and sys.version[[:3]] == '2.7':
        can_use_sysconfig = 0
except ImportError:
    pass"

  dnl Set up 4 directories:

  dnl pythondir -- where to install python scripts.  This is the
  dnl   site-packages directory, not the python standard library
  dnl   directory like in previous automake betas.  This behavior
  dnl   is more consistent with lispdir.m4 for example.
  dnl Query distutils for this directory.
  AC_CACHE_CHECK([for $am_display_PYTHON3 script directory],
    [am_cv_python3_pythondir],
    [if test "x$prefix" = xNONE
     then
       am_py3_prefix=$ac_default_prefix
     else
       am_py3_prefix=$prefix
     fi
     am_cv_python3_pythondir=`$PYTHON3 -c "
$am_python3_setup_sysconfig
if can_use_sysconfig:
    sitedir = sysconfig.get_path('purelib', vars={'base':'$am_py3_prefix'})
else:
    from distutils import sysconfig
    sitedir = sysconfig.get_python_lib(0, 0, prefix='$am_py3_prefix')
sys.stdout.write(sitedir)"`
     case $am_cv_python3_pythondir in
     $am_py3_prefix*)
       am__strip_prefix=`echo "$am_py3_prefix" | sed 's|.|.|g'`
       am_cv_python3_pythondir=`echo "$am_cv_python3_pythondir" | sed "s,^$am__strip_prefix,$PYTHON3_PREFIX,"`
       ;;
     *)
       case $am_py3_prefix in
         /usr|/System*) ;;
         *)
	  am_cv_python3_pythondir=$PYTHON3_PREFIX/lib/python$PYTHON3_VERSION/site-packages
	  ;;
       esac
       ;;
     esac
    ])
  AC_SUBST([python3dir], [$am_cv_python3_pythondir])

  dnl pkgpythondir -- $PACKAGE directory under pythondir.  Was
  dnl   PYTHON_SITE_PACKAGE in previous betas, but this naming is
  dnl   more consistent with the rest of automake.

  AC_SUBST([pkgpython3dir], [\${python3dir}/$PACKAGE])

  dnl pyexecdir -- directory for installing python extension modules
  dnl   (shared libraries)
  dnl Query distutils for this directory.
  AC_CACHE_CHECK([for $am_display_PYTHON3 extension module directory],
    [am_cv_python_py3execdir],
    [if test "x$exec_prefix" = xNONE
     then
       am_py3_exec_prefix=$am_py3_prefix
     else
       am_py3_exec_prefix=$exec_prefix
     fi
     am_cv_python3_pyexecdir=`$PYTHON3 -c "
$am_python3_setup_sysconfig
if can_use_sysconfig:
    sitedir = sysconfig.get_path('platlib', vars={'platbase':'$am_py3_prefix'})
else:
    from distutils import sysconfig
    sitedir = sysconfig.get_python_lib(1, 0, prefix='$am_py3_prefix')
sys.stdout.write(sitedir)"`
     AC_MSG_NOTICE( [am_py3_cv_python3_pyexecdir = ${am_py3_cv_pyexecdir} ])
     case $am_cv_python3_pyexecdir in
     $am_py3_exec_prefix*)
       am__strip_prefix=`echo "$am_py3_exec_prefix" | sed 's|.|.|g'`
       am_cv_python3_pyexecdir=`echo "$am_cv_python3_pyexecdir" | sed "s,^$am__strip_prefix,$PYTHON3_EXEC_PREFIX,"`
       AC_MSG_NOTICE( [Case 1: am_cv_python3_pyexecdir = ${am_cv_python3_pyexecdir} ])
       am_cv_python3_pyexecdir=`echo "$am_cv_python3_pyexecdir" | sed "s,/python3/,/python$am_cv_python_version3/,"`
       AC_MSG_NOTICE( [Case 1 fixup: am_cv_python3_pyexecdir = ${am_cv_python3_pyexecdir} ])
       ;;
     *)
       case $am_py3_exec_prefix in
         /usr|/System*) ;;
         *)
	   am_cv_python3_pyexecdir=$PYTHON3_EXEC_PREFIX/lib/python$PYTHON3_VERSION/site-packages
    AC_MSG_NOTICE( [Case 2b: am_cv_python3_pyexecdir = ${am_cv_python3_pyexecdir} ])
	   ;;
       esac
       ;;
     esac
    ])
  AC_SUBST([py3execdir], [$am_cv_python3_pyexecdir])

  dnl pkgpyexecdir -- $(pyexecdir)/$(PACKAGE)

  AC_SUBST([pkgpy3execdir], [\${py3execdir}/$PACKAGE])

  dnl Run any user-specified action.
  $2
  fi

])


# AM_PYTHON_CHECK_VERSION_RANGE(PROG, VERSION, [ACTION-IF-TRUE], [ACTION-IF-FALSE])
# ---------------------------------------------------------------------------
# Run ACTION-IF-TRUE if the Python interpreter PROG has version >= VERSION.
# Run ACTION-IF-FALSE otherwise.
# This test uses sys.hexversion instead of the string equivalent (first
# word of sys.version), in order to cope with versions such as 2.2c1.
# This supports Python 2.0 or higher. (2.0 was released on October 16, 2000).
AC_DEFUN([AM_PYTHON_CHECK_VERSION_RANGE],
 [prog="import sys
# split strings by '.' and convert to numeric.  Append some zeros
# because we need at least 4 digits for the hex conversion.
# map returns an iterator in Python 3.0 and a list in 2.x
minver = list(map(int, '$2'.split('.'))) + [[0, 0, 0]]
minverhex = 0
# xrange is not present in Python 3.0 and range returns an iterator
for i in list(range(0, 4)): minverhex = (minverhex << 8) + minver[[i]]
sys.exit(sys.hexversion < minverhex)"
  AS_IF([AM_RUN_LOG([$1 -c "$prog"])], [$3], [$4])])
