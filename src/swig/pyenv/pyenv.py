import site
import sys
import distutils.sysconfig
# apparently sysconfig is not the same as distutils.sysconfig
import sysconfig
import os
from pprint import pprint 


def one_conf_var(vname):
   print( "  %s: %s" % (vname, distutils.sysconfig.get_config_var(vname)) )


pyver = sysconfig.get_python_version()
print("Python version: %s" % pyver)

print( "OS distribution:  %s" % os.uname()[3])
print( "OS architecture:  %s" % os.uname()[4])

print("sys.prefix: %s" % sys.prefix)
print("sys.exec_prefix: %s" % sys.exec_prefix)
if pyver >= '3':
   print("sys.base_prefix: %s" % sys.base_prefix)
   print("sys.base_exec_prefix: %s" % sys.base_exec_prefix)
print("")
# print("dir(sys):")
# pprint(dir(sys))
# print("")


print("site.PREFIXES:  %s " % site.PREFIXES )
print("site.ENAABLE_USER_SITE:  %s" % site.ENABLE_USER_SITE )
print("")



print("Excludes non-existent paths")
print( "sys.path:" )
for p in sys.path:
   print(p)
print("")

print("Uses it's own private code for building directory names, ")
print("Does not use sysconfig.get.get_-path() or distutils.sysconfig.get_python)_lib():")
print("Contains paths that may not exist:")
print("Any existing directory in this list are added to sys.path by site.py's main() function,")
print("which is automatically called unless the interpreter is started with the -S flag")
print( "site.getsitepackages():" )
print("If no argument, defaults to getsitepackages(prefixes=site.PREFXIES)"), 
print("where site.PREFIXES=%s" % site.PREFIXES)
# for p in site.getsitepackages():
#    print( p)
pprint(site.getsitepackages())
print("")

print( "site.getusersitepackages():  %s" % site.getusersitepackages())
# print( site.getusersitepackages())
print("")


print( "platform independent library directory - distutils.sysconfig.get_python_lib(0)")
print( distutils.sysconfig.get_python_lib(0))
print("")

print( "platform specific library directory - distutils.sysconfig.get_python_lib(1)")
print( distutils.sysconfig.get_python_lib(1))
print("")


print("")


print( "Per Python doc 28.2.2  Installation paths:" )
print( "  posix_prefix: scheme for Posix platforms like Linux or Mac OS X. " )
print( "  This is the default scheme used when Python or a component is installed." )
print("")


print("Scheme posix_local appears to be specific to Python2 on debian/ubuntu")
print("It appears that if posix_local esists, it is the default scheme, not posix_prefix")

print("sysconfig.get_scheme_names():")
pprint(sysconfig.get_scheme_names())
print("")

if "posix_local" in sysconfig.get_scheme_names():             # pyver < '3'
   schemes = ['posix_prefix', 'posix_local', None]
else:
   schemes = ['posix_prefix', None]

ct_site_posix_prefix = 0
ct_site_non_posix_prefix = 0
ct_dist_posix_prefix = 0
ct_dist_non_posix_prefix = 0

SITE_NDX = 0
DIST_NDX = 1

non_posix_prefix_counters = [0,0]
posix_prefix_counters     = [0,0]

get_path_counters = [0,0]
get_lib_counters  = [0,0]


def execute_get_path(library_id, scheme=None, platbase_value=None, expand=True):
   # print("(execute_get_path) library_id=%s, scheme=%s, platbase_value=%s, expand=%s" % (library_id, scheme, platbase_value, expand))
   funcdesc = "sysconfig.get_path()"
   if scheme:
      schemedesc = "scheme=%s" % scheme
   else:
      schemedesc = "scheme=<default>"
   if expand:
      if platbase_value:
         argdesc = "platbase=%s" % platbase_value
      else:
         argdesc = "platbase=<default>"
   else:
      argdesc = "expand=False"

   if scheme:
      if expand:
         if platbase_value:
            libname = sysconfig.get_path(library_id, scheme, vars={'platbase': platbase_value}) 
         else:
            libname = sysconfig.get_path(library_id, scheme )
      else:
         libname = sysconfig.get_path(library_id, scheme, expand=False )
   else:
      if expand:
         if platbase_value:
            libname = sysconfig.get_path(library_id, vars={'platbase': platbase_value}) 
         else:
            libname = sysconfig.get_path(library_id )
      else:
         libname = sysconfig.get_path(library_id, expand=False )

   return(libname, funcdesc, schemedesc, argdesc)


def compare_schemes(library_id, scheme1, scheme2):
   # print("(compare_schemes) library_id=%s, scheme1=%s, scheme2=%s" % (library_id, scheme1, scheme2))
   result = True
   libname1 = execute_get_path(library_id, scheme1, None, expand=False)[0]
   libname2 = execute_get_path(library_id, scheme2, None, expand=False)[0]
   if libname1 != libname2: 
      result = False
   else:
      for pfx in ['/usr', '/usr/local', None]:
         if pfx:
            libname1 = execute_get_path(library_id, scheme1,  pfx )[0]
            libname2 = execute_get_path(library_id, scheme2,  pfx )[0]
         else:
            libname1 = execute_get_path(library_id, scheme1, None, expand=False)[0]
            libname2 = execute_get_path(library_id, scheme2, None, expand=False)[0]

         if libname1 != libname2:
            result = False
            break 
   # print("(compare_schemes) library_id=%s, scheme1=%s, scheme2=%s, returning: %s" % (library_id, scheme1, scheme2, result))
   return result

def compare_prefixes(library_id, scheme, prefix1, prefix2):
   # print("(compare_prefixes) library_id=%s, scheme=%s, prefix1=%s, prefix2=%s" % (library_id, scheme, prefix1, prefix2))
   result = True
   if scheme:
      if prefix1:
         libname1 = execute_get_path(library_id, scheme,  platbase_value=prefix1)[0]
      else:
         libname1 = execute_get_path(library_id, scheme )[0]
      if prefix2: 
         libname2 = execute_get_path(library_id, scheme,  platbase_value = prefix2 )[0]
      else:
         libname2 = execute_get_path(library_id, scheme )[0]
   else:
      if prefix1:
         libname1 = execute_get_path(library_id, platbase_value= prefix1)[0]
      else:
         libname1 = execute_get_path(library_id)[0]
      if prefix2: 
         libname2 = execute_get_path(library_id, platbase_value=prefix2 )[0]
      else:
         libname2 = execute_get_path(library_id )[0]
   if libname1 != libname2:
      result = False

   return result

def scheme_desc(scheme_name):
   if scheme_name:
      sname = "scheme: %s" % scheme_name
   else:
      sname = "scheme: <default>"
   return sname

def prefix_desc(prefix_name):
   if prefix_name:
      sname = "prefix: %s" % prefix_name
   else:
      sname = "prefix: <default>"
   return sname


def filter_identical_schemes(library_id, schemes):
   # print("(filter_identical_schemes) library_id=%s, schemes=%s"  % (library_id, schemes))
   filtered_schemes = schemes
   scheme_ndx1 = 0
   while scheme_ndx1 < len(filtered_schemes)-1 :
      scheme_ndx2 = scheme_ndx1 + 1
      while scheme_ndx2 < len(filtered_schemes):
         sc1 = filtered_schemes[scheme_ndx1]
         sc2 = filtered_schemes[scheme_ndx2]
         if compare_schemes(library_id, sc1, sc2):
            print("library_id=%s, %s and %s are identical" % ( library_id, scheme_desc(sc1), scheme_desc(sc2) ) )
            del filtered_schemes[scheme_ndx2]
         else:
            scheme_ndx2 += 1
      scheme_ndx1 += 1
   return filtered_schemes      

def filter_identical_prefixes(library_id, scheme, prefixes):
   filtered_prefixes = prefixes
   prefix_ndx1 = 0
   while prefix_ndx1 < len(filtered_prefixes)-1 :
      prefix_ndx2 = prefix_ndx1 + 1
      while prefix_ndx2 < len(filtered_prefixes):
         pfx1 = filtered_prefixes[prefix_ndx1]
         pfx2 = filtered_prefixes[prefix_ndx2]
         if compare_prefixes(library_id, scheme, pfx1, pfx2):
            print("library_id=%s, %s, %s and %s are identical" % ( library_id, scheme_desc(scheme), prefix_desc(pfx1), prefix_desc(pfx2) ) )
            del filtered_prefixes[prefix_ndx2]
         else:
            prefix_ndx2 += 1
      prefix_ndx1 += 1
   # print("(filter_identical_prefixes) library_id=%s, scheme=%s, Returning: %s" % (library_id, scheme, filtered_prefixes))
   return filtered_prefixes 

for libid in ['purelib', 'platlib']:
   print("*** %s:" % libid)
   print("Checking for identical schemes...")
   filtered_schemes = filter_identical_schemes(libid, schemes)
   print("   Filtered schemes: %s" % filtered_schemes)

   prefixes_for_scheme = {}
   for scheme in filtered_schemes:
      print("Checking for identical prefixes for scheme: %s" % scheme)
      pfxs = ['/usr', '/usr/local', None]
      filtered_prefixes = filter_identical_prefixes(libid, scheme, pfxs)
      prefixes_for_scheme[scheme] = filtered_prefixes

   print("")
   print("prefixes_for_scheme:")
   pprint(prefixes_for_scheme)

   template = "  %-45s %-20s %-25s %s"
   print("Filtered %s libraries:" % libid)
   for sc in filtered_schemes:
      # print("sc: %s" % sc)
      cur_prefixes = prefixes_for_scheme[sc]
      #  pprint( cur_prefixes )     
      for pfx in prefixes_for_scheme[sc]:
         #print("pfx: %s" % pfx)
         (libname, funcdesc, schemedesc, argdesc) = execute_get_path(libid, sc, pfx, expand=True)
         print(template % (funcdesc, schemedesc, argdesc, libname))
   print("")

print("")
print("Getting libraries using sysconfig.get_python_lib()")
template = "  %-45s %-20s %-25s %s"

schemedesc = ""

for libnum in [0,1]:
   print("libnum = %d" % libnum)
   for prefix in [None, '/usr', '/usr/local']:
     if prefix:
        libname  =  distutils.sysconfig.get_python_lib(libnum, 0, prefix=prefix) 
        argdesc = "prefix=%s" % prefix
     else:
        libname  =  distutils.sysconfig.get_python_lib(libnum, 0)
        argdesc = "prefix=<default>"
     schemedesc = ""
     funcdesc = "distutils.sysconfig.get_python_lib()"
     print( template % (funcdesc, schemedesc, argdesc, libname))




#---


print("")
print("")

print("Getting platform specific library:")
template = "  %-45s %-20s %-25s %s"
for scheme in schemes:
   funcdesc = "sysconfig.get_path()"
   argdesc  = "expand=False"
   counters = posix_prefix_counters if scheme == "posix_prefix" else non_posix_prefix_counters
   if scheme:
      libname  = sysconfig.get_path('platlib', scheme, expand=False) 
      schemedesc = "scheme=%s" % scheme
   else:
      libname  = sysconfig.get_path('platlib', expand=False) 
      schemedesc = "scheme=<default>"
   print(template % (funcdesc, schemedesc, argdesc, libname))
   ndx = SITE_NDX if libname.endswith("site-packages") else DIST_NDX
   counters[ndx] += 1
   fcounters = get_path_counters
   fcounters[ndx] += 1

   if scheme:
      libname  = sysconfig.get_path('platlib', scheme) 
      schemedesc = "scheme=%s" % scheme
   else:
      libname  = sysconfig.get_path('platlib') 
      schemedesc = "scheme=<default>"
   funcdesc = "sysconfig.get_path()"
   argdesc  = "<defaults>"
   print(template % (funcdesc, schemedesc, argdesc, libname))
   ndx = SITE_NDX if libname.endswith("site-packages") else DIST_NDX
   counters[ndx] += 1
   fcounters = get_path_counters
   fcounters[ndx] += 1



   for prefix in [None, '/usr', '/usr/local']:
     if scheme:
        if prefix:
           libname  = sysconfig.get_path('platlib', scheme, vars={'platbase': prefix}) 
        else:
           libname  = sysconfig.get_path('platlib', scheme )
        schemedesc = "scheme=%s" % scheme
     else:
        if prefix:
           libname  = sysconfig.get_path('platlib', vars={'platbase': prefix}) 
        else:
           libname  = sysconfig.get_path('platlib') 
        schemedesc = "scheme=<default>"
     funcdesc = "sysconfig.get_path()"
     argdesc  = "prefix=%s" % prefix
     print( template % (funcdesc, schemedesc, argdesc, libname))
     counters = posix_prefix_counters if scheme == "posix_prefix" else non_posix_prefix_counters
     ndx = SITE_NDX if libname.endswith("site-packages") else DIST_NDX
     counters[ndx] += 1
     fcounters = get_path_counters
     fcounters[ndx] += 1

     if prefix:
        libname  =  distutils.sysconfig.get_python_lib(1, 0, prefix=prefix) 
     else:
        libname  =  distutils.sysconfig.get_python_lib(1, 0)
     schemedesc = ""
     funcdesc = "distutils.sysconfig.get_python_lib()"
     print( template % (funcdesc, schemedesc, argdesc, libname))
     counters = non_posix_prefix_counters
     ndx = SITE_NDX if libname.endswith("site-packages") else DIST_NDX
     counters[ndx] += 1
     fcounters = get_lib_counters
     fcounters[ndx] += 1


print("")
print("                                   site-packages    dist-packages")
print("sysconfig.get_path( scheme='posix_prefix')   %2d               %2d" % (posix_prefix_counters[0],      posix_prefix_counters[1] )  )
print("other                                        %2d               %2d" % (non_posix_prefix_counters[0],  non_posix_prefix_counters[1]) )
print("")
print("                          site-packages    dist-packages")
print("sysconfig.get_path()                %2d               %2d" % (get_path_counters[0],  get_path_counters[1]) )
print("distutils.sysconfix.get_lib()       %2d               %2d" % (get_lib_counters[0],   get_lib_counters[1] )  )
print("")

print( "platform independent include directory - distutils.sysconfig.get_python_inc(0)")
print( distutils.sysconfig.get_python_inc(0))
print("")

print( "platform specific include directory - distutils.sysconfig.get_python_inc(1)")
print( distutils.sysconfig.get_python_inc(1))
print("")


print("sysconfig.getpath_names():")
pprint(sysconfig.get_path_names())
for sname in sysconfig.get_path_names():
   print("  %s:  %s" % (sname, sysconfig.get_path(sname)) )



print("")
c = distutils.sysconfig.get_config_vars()
print( "distutils.sysconfig.get_config_vars(): ")
vlist =  [
   'BINDIR',
   'CC',
   'CFLAGS', 
   'CFLAGSFORSHARED',
   'CONFIG_ARGS',
   'CONFIGFILES',
   'CONFIGURE_CPPFLAGS',
   'CONFINCLUDEDIR',
   'CPPFLAGS', 
   'datarootdir',
   'DESTDIRS',
   'DESTLIB',
   'DESTSHARED',
   'DIST', 
   'DISTDIRS', 
   'DLINCDIR',
   'exec_prefix',
   'EXTRAPATDIR',
   'INCLUDEDIR',
   'INCLUDEPY',
   'INSTALL',
   'INSTaLL_DATA',
   'INSTALL_SHARED', 
   'INSTSONAME',
   'LDFLAGS',
   'LDLIBRARY',
   'LDLIBRARYDIR',
   'LDSHARED',
   'LIBC',
   'LIBDEST',
   'LIBDIR',
   'LIBOBJDIR',
   'LIBOBJS',
   'LIBRARY',
   'LIBS', 
   'LINKFORSHARED', 
   'MACHDESTLIB',
   'MULTIARCH',
   'OPT', 
   'PLATDIR',
   'PLATMACPATH',
   'PREFIX',
   'PY_CFLAGS', 
   'PY_CORE_CFLAGS',
   'PY_CPPFLAGS',
   'Py_ENABLE_SHARED', 
   'PY_LDFLAGS',
   'PYTHON',
   'PYTHON_HEADERS',
   'PYTHONPATH',
   'SCRIPTDIR',
   'SHLIBS',
   'SITEPATH', 
   'srcdir',
   'SRCDIRS',
   'SUBDIRS',
   'SYSLIBS', 
   'VERSION',
   'VPATH',
   ]
# for v in vlist: 
#    one_conf_var(v)

vlist2 = [
   'CPPFLAGS', 
   'CFLAGS',
   'CFLAGSFORSHARED',
   'exec_prefix',
   'INCLUDEDIR',
   'INCLUDEPY',
   'INSTSONAME',
   'LDFLAGS',
   'LDLIBRARY',
   'LDSHARED',
   'LIBS',
   'LINKFORSHARED',
   'platbase',
   'PREFIX',
   'PY_CFLAGS',
   'PY_CORE_CFLAGS',
   'PY_CPPFLAGS',
   'PY_LDFLAGS',
   'SHLIBS',
   'SITEPATH',
   'SYSLIBS',
  ]
for v in vlist2: 
   one_conf_var(v)


# kall = c.keys()
# kall.sort()
# for k in kall:
#    print k


