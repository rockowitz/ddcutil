generated_source_files = \
cyddc2.c \
cyddc3.c

CLEANFILES = ${generated_source_files}


if ENABLE_CYTHON

# Python extension modules, installed in $(py2execdir) or $(py3execdir)
pyexec_LTLIBRARIES = cyddc2.la
py3exec_LTLIBRARIES = cyddc3.la

# nodist_py3exec_PYTHON = 


#
# Python 2 version 
#

# Module source code, nodist because cyddc2.c is generated
nodist_cyddc2_la_SOURCES = cyddc2.c

# Flags when compiling files in cyddc2_SOURCES
cyddc2_la_CPPFLAGS = -I${top_srcdir}/src -I${top_srcdir}/src/public ${PY2_CFLAGS}
cyddc2_la_CFLAGS   = $(PYTHON_CFLAGS)

# Global and order-independent shared library and program linker config flags and options 
cyddc2_la_LDFLAGS = 
cyddc2_la_LDFLAGS += -module -shared -export_dynamic -static
cyddc2_la_LDFLAGS += version_info '@LT_CURRENT@:@LT_REVISION@:@LT_AGE@'

# Link in the core library
cyddc2_la_LIBADD = \
  ../libcommon.la \
  ../libddcutil.la 


cyddc2.c: cyddc.pyx show_vars
	@echo "=====> (src/cython/Makefile) Executing target cyddc2.c"
	cython -I${top_srcdir}/src -I{top_srcdir}/src/public -I . -2 -o cyddc2.c -a cyddc.pyx


#
# Python 3 version
#

# nodist because cyddc3.c is generated
nodist_cyddc3_la_SOURCES = cyddc3.c

# Flags when compiling files in cyddc3_SOURCES
cyddc3_la_CPPFLAGS = -I${top_srcdir}/src -I${top_srcdir}/src/public ${PY3_CFLAGS}
cyddc3_la_CFLAGS   = $(PYTHON3_CFLAGS)

# Global and order-independent shared library and program linker config flags and options 
# -module          forces libtool to generate a dynamically loadable module
# -static          do not link against shared libraries, all external references must be resolved from static libraries
# -shared          create a shared library
# -export-dynamic  add all symbols to dynamic symbol table, needed for dlopen
# -avoid-version   avoid versioning if possible (any effect on Linux?)
# -version-info
cyddc3_la_LDFLAGS = 
cyddc3_la_LDFLAGS += -module -shared -export_dynamic -static
cyddc3_la_LDFLAGS += version_info '@LT_CURRENT@:@LT_REVISION@:@LT_AGE@'

# Link in the core library
cyddc3_la_LIBADD = \
  ../libcommon.la \
  ../libddcutil.la 


cyddc3.c: cyddc.pyx show_vars
	@echo "=====> (src/cython/Makefile) Executing target cyddc.c"
	cython -I${top_srcdir}/src -I{top_srcdir}/src/public -I . -3 -o cyddc3.c -a cyddc.pyx

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
	@echo "  cyddc2_la_OBJECTS             = $(cyddc2_la_OBJECTS)"
	@echo "  cyddc2_la_DEPENDENCIES        = $(cyddc2_la_DEPENDENCIES)"
	@echo "  EXTRA_cyddc2_la_DEPENDENCIES  = $(EXTRA_cyddc2_la_DEPENDENCIES)"
	@echo "  cyddc2_la_LINK                = $(cyddc2_la_LINK)"
	@echo "  am_cyddc2_la_rpath            = $(am_cyddc2_la_rpath)"
	@echo "  cyddc2_la_OBJECTS             = $(cyddc2_la_OBJECTS)"
	@echo "  cyddc2_la_LIBADD              = $(cyddc2_la_LIBADD)"
	
	@echo "  cyddc3_la_OBJECTS             = $(cyddc3_la_OBJECTS)"
	@echo "  cyddc3_la_DEPENDENCIES        = $(cyddc3_la_DEPENDENCIES)"
	@echo "  EXTRA_cyddc3_la_DEPENDENCIES  = $(EXTRA_cyddc3_la_DEPENDENCIES)"
	@echo "  cyddc3_la_LINK                = $(cyddc3_la_LINK)"
	@echo "  am_cyddc3_la_rpath            = $(am_cyddc3_la_rpath)"
	@echo "  cyddc3_la_OBJECTS             = $(cyddc3_la_OBJECTS)"
	@echo "  cyddc3_la_LIBADD              = $(cyddc3_la_LIBADD)"
	

# ld vars:
#@echo "  PY2_EXECDIR               = $(PY2_EXECDIR)"
# @echo "  PY3_EXECDIR               = $(PY3_EXECDIR)"

.PHONY: show_vars
