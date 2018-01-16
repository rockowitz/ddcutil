generated_source_files = \
ddccffi.c ddccffi2.c ddccffi3.c

CLEANFILES = ${generated_source_files}  libddccffi2.la libddccffi3.la _ddccffi2.c _ddccffi3.c


if ENABLE_CFFI

# Python extension modules, installed in $(py2execdir) or $(py3execdir)


# nodist_py3exec_PYTHON = 


if ENABLE_PY2
pyexec_LTLIBRARIES  = _ddccffi2.la


#
# Python 2 version 
#

# Generated C file is version agnostic, but unfortunately the 
# initialization entry point name must match the .so file name, 
# and the only way to force this is use different c file names 
# in build_cffi.py
_ddccffi2.c: _ddccffi_cdef_c_api.h _ddccffi_cdef_types.h  show_vars
	@echo "=====> (src/cython/Makefile) Executing target _ddccffi2.c"
	python2 build_cffi.py --nocompile


# Module source code, nodist because ddc_cffi.c is generated
nodist__ddccffi2_la_SOURCES = _ddccffi2.c

# Flags when compiling files in ddc_cffi2_SOURCES
_ddccffi2_la_CPPFLAGS = -I${top_srcdir}/src -I${top_srcdir}/src/public ${PY2_CFLAGS}
_ddccffi2_la_CFLAGS   = $(PYTHON_CFLAGS)

# Global and order-independent shared library and program linker config flags and options 
_ddccffi2_la_LDFLAGS = 
_ddccffi2_la_LDFLAGS += -module -shared -export_dynamic -static
_ddccffi2_la_LDFLAGS += version_info '@LT_CURRENT@:@LT_REVISION@:@LT_AGE@'

# Link in the core library
_ddccffi2_la_LIBADD = \
  ../libcommon.la \
  ../libddcutil.la 

endif



if ENABLE_PY3
py3exec_LTLIBRARIES = _ddccffi3.la

#
# Python 3 version
#

# Generated C file is version agnostic, but unfortunately the 
# initialization entry point name must match the .so file name, 
# and the only way to force this is use different c file names 
# in build_cffi.py
_ddccffi3.c: _ddccffi_cdef_c_api.h _ddccffi_cdef_types.h  show_vars
	@echo "=====> (src/cython/Makefile) Executing target _ddccffi3.c"
	python3 build_cffi.py --nocompile

# nodist because ddc_cffi.c is generated
nodist__ddccffi3_la_SOURCES = _ddccffi3.c

# Flags when compiling files in ddc_cffi3_SOURCES
_ddccffi3_la_CPPFLAGS = -I${top_srcdir}/src -I${top_srcdir}/src/public ${PY3_CFLAGS}
_ddccffi3_la_CFLAGS   = $(PYTHON3_CFLAGS)

# Global and order-independent shared library and program linker config flags and options 
# -module          forces libtool to generate a dynamically loadable module
# -static          do not link against shared libraries, all external references must be resolved from static libraries
# -shared          create a shared library
# -export-dynamic  add all symbols to dynamic symbol table, needed for dlopen
# -avoid-version   avoid versioning if possible (any effect on Linux?)
# -version-info
_ddccffi3_la_LDFLAGS = 
_ddccffi3_la_LDFLAGS += -module -shared -export_dynamic -static
_ddccffi3_la_LDFLAGS += version_info '@LT_CURRENT@:@LT_REVISION@:@LT_AGE@'

# Link in the core library
_ddccffi3_la_LIBADD = \
  ../libcommon.la \
  ../libddcutil.la 


endif

endif




# Add show_vars to dependencies for debugging
show_vars:
	@echo "  AM_CFLAGS               = $(AM_CFLAGS)"
	@echo "  AM_CPPFLAGS             = $(AM_CPPFLAGS)"
	@echo "  AX_SWIG_PYTHON_CPPFLAGS = $(AX_SWIG_PYTHON_CPPFLAGS)"
	@echo "  AX_SWIG_PYTHON_LIBS     = $(AX_SWIG_PYTHON_LIBS)"
	@echo "  AX_SWIG_PYTHON_OPT      = $(AX_SWIG_PYTHON_OPT)"
	@echo "  PYTHON_CFLAGS           = $(PYTHON_CFLAGS)"
	@echo "  PYTHON_CPPFLAGS         = $(PYTHON_CPPFLAGS)"
	@echo "  PYTHON_EXEC_PREFIX      = $(PYTHON_EXEC_PREFIX)"
	@echo "  PYTHON_EXTRA_LDFLAGS    = $(PYTHON_EXTRA_LDFLAGS)"
	@echo "  PYTHON_EXTRA_LIBS       = $(PYTHON_EXTRA_LIBS)"

	@echo "  PY2_CFLAGS              = $(PY2_CFLAGS)"
	@echo "  PY2_LIBS                = $(PY2_LIBS)"
	@echo "  PY2_EXTRA_LDFLAGS       = $(PY2_EXTRA_LDFLAGS)"
	@echo "  PY2_EXTRA_LIBS          = $(PY2_EXTRA_LIBS)"
	@echo "  PY3_CFLAGS              = $(PY3_CFLAGS)"
	@echo "  PY3_LIBS                = $(PY3_LIBS)"
	@echo "  PY3_EXTRA_LDFLAGS       = $(PY3_EXTRA_LDFLAGS)"
	@echo "  PY3_EXTRA_LIBS          = $(PY3_EXTRA_LIBS)"
	@echo "  PYTHON_LDFLAGS          = $(PYTHON_LDFLAGS)"
	@echo "  PYTHON_LIBS             = $(PYTHON_LIBS)"
	@echo "  PYTHON_SITE_PKG         = $(PYTHON_SITE_PKG)"
	@echo "  PYTHON_SITE_PKG_EXEC    = $(PYTHON_SITE_PKG_EXEC)"
	@echo "  SWIG                    = $(SWIG) "
	@echo "  SWIG_LIB                = $(SWIG_LIB)"
	@echo "  includedir              = $(includedir)"
	@echo "  prefix                  = $(prefix)"
	@echo "  pyexecdir               = $(pyexecdir)"
	@echo "  pythondir               = $(pythondir)"
	@echo "  py2execdir              = $(py2execdir)"
	@echo "  py3execdir              = $(py3execdir)"
	@echo "  python3dir              = $(python3dir)"
	@echo "  srcdir                  = $(srcdir)"
	@echo "  top_srcdir              = $(top_srcdir)"

	@echo "  LIBS                             = $(LIBS)"
	@echo "  _ddccffi2_la_OBJECTS             = $(ddc_cffi2_la_OBJECTS)"
	@echo "  _ddccffi2_la_DEPENDENCIES        = $(ddc_cffi2_la_DEPENDENCIES)"
	@echo "  EXTRA_ddc_cffi2_la_DEPENDENCIES  = $(EXTRA_ddc_cffi2_la_DEPENDENCIES)"
	@echo "  _ddccffi2_la_LINK                = $(ddc_cffi2_la_LINK)"
	@echo "  am_ddc_cffi2_la_rpath            = $(am_ddc_cffi2_la_rpath)"
	@echo "  _ddccffi2_la_OBJECTS             = $(ddc_cffi2_la_OBJECTS)"
	@echo "  _ddccffi2_la_LIBADD              = $(ddc_cffi2_la_LIBADD)"
	
	@echo "  _ddccffi3_la_OBJECTS             = $(ddc_cffi3_la_OBJECTS)"
	@echo "  _ddccffi3_la_DEPENDENCIES        = $(ddc_cffi3_la_DEPENDENCIES)"
	@echo "  EXTRA_ddc_cffi3_la_DEPENDENCIES  = $(EXTRA_ddc_cffi3_la_DEPENDENCIES)"
	@echo "  _ddccffi3_la_LINK                = $(ddc_cffi3_la_LINK)"
	@echo "  am_ddc_cffi3_la_rpath            = $(am_ddc_cffi3_la_rpath)"
	@echo "  _ddccffi3_la_OBJECTS             = $(ddc_cffi3_la_OBJECTS)"
	@echo "  _ddccffi3_la_LIBADD              = $(ddc_cffi3_la_LIBADD)"
	

# ld vars:
#@echo "  PY2_EXECDIR               = $(PY2_EXECDIR)"
# @echo "  PY3_EXECDIR               = $(PY3_EXECDIR)"

.PHONY: show_vars
