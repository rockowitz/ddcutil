import os
import sys
import ddc_util
from   cffi import FFI

sysver = ddc_util.required_pyvers(2.7, 3.4)

# quick and dirty
nocompile = False
if len(sys.argv) > 1 and sys.argv[1] == "--nocompile":
    nocompile = True
print("nocompile: %s" % nocompile)


ffibuilder = FFI()

base_dir    = "/shared/playproj/i2c/src/"
header_fn   = base_dir + "public/ddcutil_c_apitypes.h"
base_name   = "_ddccffi"
# module_name = "%s%s" % (base_name, sysver)
module_name = base_name


ffibuilder.set_source(module_name,
    r"""#include "../public/ddcutil_c_api.h"
     """,
     libraries=['ddcutil'],
     library_dirs=['/shared/playproj/i2c/src/.libs'],
     runtime_library_dirs=['/shared/playproj/i2c/src/.libs'])

cdef_types_fn = base_name + "_cdef_types.h"
cdef_api_fn   = base_name + "_cdef_c_api.h"

def read_file(fn):
  try: 
     with open(fn, 'r') as fn_handle:
        lines = fn_handle.read()
  except Exception as excp:
    print(excp)
    sys.exit(1)

  if (len(lines) == 0):
     printf("Empty file: %s" % cdef_types_fn)
  return lines

cdef_types_lines = read_file(cdef_types_fn)
cdef_api_lines   = read_file(cdef_api_fn)

cdef_segments = []

cdef_segments.append(cdef_types_lines)
cdef_segments.append(cdef_api_lines)

print("len(cdef_segments): %d" % len(cdef_segments))

for ndx in range(len(cdef_segments)):
   print("ndx: %d" % ndx)
   ffibuilder.cdef(cdef_segments[ndx])

if nocompile:
    ffibuilder.emit_c_code("_ddccffi.c")
    
else:
    ffibuilder.compile(verbose=True)

    # hack
    if sysver == 3:
        os.rename("_ddccffi.cpython-36m-x86_64-linux-gnu.so", "_ddccffi3.so")
    else:
        os.rename("_ddccffi.so", "_ddccffi2.so")    
    
    