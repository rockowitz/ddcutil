import os
import site
import sys
import sysconfig


def is_debian():
   result = False 
   for p in site.getsitepackages():
      if p.endswith("dist-packages"):
         result = True
         break
   return result


def fix_lib(libname): 
   old_libname = libname
   if libname.startswith("/usr/local/"):
      pfx = "/usr/local"
   else if libname.startswith("/usr/"):
      pfx = "/usr"
   else:
      pfx = None
   if pfx:
      is_debain = False 
      pkgs = site.getsitepackages()
      for p in pkgs:
         if p.endswith("dist-packages"):
            is_debian = True
            break
      if is_debian:
         pfxlib = os.path.join(pfx, "lib", "python")
         # print('pfxlib = %s' % pfxlib)

         pyver = sysconfig.get_python_version()[:3]
         # print("Python version: %s" % pyver)

         for p in pkgs:
          # TODO: handle case where package name is final suffix
          if p.startswith(pfxlib) and p.endswith("-packages"):
            libname = p
            # prefer the more specific lib/python3.5/ to lib/python3/
            if libname.contains(pyver):
               break

   print("(fix_lib) %s -> %s", (old_libname, libname) )
   return libname


pfx =  '/usr' if len(sys.argv) < 2 else sys.argv[1] 
# print( pfx)

pfxlib = os.path.join(pfx, "lib", "python")
# print('pfxlib = %s' % pfxlib)

# unused
# pyver = sysconfig.get_python_version()[:3]
# print("Python version: %s" % pyver)

result = ''
for p in site.getsitepackages():
   if p.startswith(pfxlib) and p.endswith("-packages"):
      result = p
      break
# n. no newline
sys.stdout.write(result)


