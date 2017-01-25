

# AX_PYTHON_ENV(PYTHON_BIN, VAR_NAME) 
# ---------------------------------------------------------------------------

AC_DEFUN([AX_PYTHON_ENV], [
   AC_ARG_VAR( [PY2_EXTRA_LDFLAGS], [override additional Python 2 linker flags from disutils.sysconfig() ] )dnl
   AC_ARG_VAR( [PY2_EXTRA_LIBS],    [override additional Python 2 libraries from disutils.sysconfig()]    )dnl
   AC_ARG_VAR( [PY3_EXTRA_LDFLAGS], [override additional Python 3 linker flags from distutils.sysconfig() ] )dnl
   AC_ARG_VAR( [PY3_EXTRA_LIBS],    [override additional Python 3 libraries from distutils.sysconfig()]    )dnl

   python=$1
   dnl AC_MSG_NOTICE( [=====> Macro ax_python_env, python=$python])

   dnl AC_MSG_CHECKING(python version)
   pyver=`$python -c "import sys; print( sys.version_info.major)"`
   dnl AC_MSG_NOTICE( [ pyver = $pyver])
   AC_MSG_NOTICE( [=====> Macro ax_python_env, python=$python, pyver=$pyver])

   #
   # libraries which must be linked in when embedding
   #
   dnl AS_IF( [test  0$pyver == 2 -a -z "$PY2_EXTRA_LIBS"  -o   0$pyver == 3 -a -z "$PY3_EXTRA_LIBS"  ], [
   dnl   AC_MSG_NOTICE([Not preset])

   AC_MSG_CHECKING(Python $pyver extra libraries)
   extra_ldflags=`$python -c "import distutils.sysconfig; \
                   conf = distutils.sysconfig.get_config_var; \
                   print (conf('LIBS') + ' ' + conf('SYSLIBS'))"`
   AC_MSG_RESULT( [$extra_ldflags] )
   AS_IF( [ test 0$pyver -eq 2  ], [
      dnl AC_MSG_NOTICE( [pyver == 2] )
      AS_IF( [test -z "$PY2_EXTRA_LIBS"], [
         dnl AC_MSG_NOTICE( [Setting PY2_EXTRA_LIBS] )
         PY2_EXTRA_LIBS=$extra_ldflags
      ])
   ], [
      dnl AC_MSG_NOTICE([pyver == 3])
      AS_IF( [test -z "$PY3_EXTRA_LIBS"], [
         dnl AC_MSG_NOTICE( [Setting PY3_EXTRA_LIBS] )
         PY3_EXTRA_LIBS=$extra_ldflags
      ])
   ]) 

   dnl ],[
   dnl    AC_MSG_NOTICE([ _EXTRA_LIBS preset])
   dnl ])


   #
   # linking flags when embedding
   #
   AC_MSG_CHECKING(Python $pyver extra linking flags)
   link_flags=`$python -c "import distutils.sysconfig; \
                   conf = distutils.sysconfig.get_config_var; \
                   print (conf('LINKFORSHARED'))"`
   AC_MSG_RESULT( [$link_flags] )
   AS_IF( [ test 0$pyver -eq 2  ], [
      dnl AC_MSG_NOTICE( [pyver == 2] )
      AS_IF( [test -z "$PY2_EXTRA_LDFLAGS"], [
         dnl AC_MSG_NOTICE( [Setting PY2_EXTRA_LDFLAGS] )
         PY2_EXTRA_LDFLAGS=$link_flags
      ])
   ], [
      dnl AC_MSG_NOTICE([pyver == 3])
      AS_IF( [test -z "$PY3_EXTRA_LDFLAGS"], [
         dnl AC_MSG_NOTICE( [Setting PY3_EXTRA_LDFLAGS] )
         PY3_EXTRA_LDFLAGS=$link_flags
      ])
   ]) 


   #
   # /usr package exec directory
   #
   # should this be precious?  what should variable name be?
   AC_MSG_CHECKING(Python $pyver /usr extension directory)
   exec_dir=`$python -c  "import sys; \
        import distutils.sysconfig; \
        result = distutils.sysconfig.get_python_lib(1,0); \
        sys.stdout.write(result)"`
   AC_MSG_RESULT( [$exec_dir] )
   AS_IF( [ test 0$pyver -eq 2  ], [
      dnl AC_MSG_NOTICE( [pyver == 2] )
      AS_IF( [test -z "$usr_py2execdir"], [
         dnl AC_MSG_NOTICE( [Setting usr_py2execdir] )
         usr_py2execdir=$exec_dir
      ])
   ], [
      dnl AC_MSG_NOTICE([pyver == 3])
      AS_IF( [test -z "$usr_py3execdir"], [
         dnl AC_MSG_NOTICE( [Setting usr_py3execdir] )
         usr_py3execdir=$exec_dir
      ])
   ]) 
   vname=alt_py${pyver}execdir
   eval $vname=$exec_dir



])

