import gi
import sys
gi.require_version("ddcutil", "1.0")
from gi.repository import ddcutil
from gi.repository import GLib
from gi.repository import GObject

dir(ddcutil)

print( "Exercise functions that require no object instance:")

print("Build information:")
vstr = ddcutil.get_ddcutil_version_string()
print("  ddcutil_get_version_string():  %s" % vstr)
vspec = ddcutil.get_ddcutil_version_spec() 
print vspec
print repr(vspec)
print("  ddcutil_get_version_spec():         %d.%d.%d" % (vspec.major, vspec.minor, vspec.micro))
vspec4 = ddcutil.get_ddcutil_version_spec4() 
print("  ddcutil.get_ddcutil_version_spec4(): %s" % vspec4)

build_options = ddcutil.get_build_options() 



print("Status code functions:")
edesc = ddcutil.rc_desc(-22)
print( "  ddcutil.rc_desc(-22): %s" % edesc)
ename = ddcutil.rc_name(-22)
print( "  ddcutil.rc_name(-22): %s" % ename)
print("  Try an invalid value...")
edesc = ddcutil.rc_desc(-222)
print( "  ddcutil.rc_desc(-222): %s" % edesc)
ename = ddcutil.rc_name(-888)
print("  ddcutil.rc_name(-888): %s" % ename)
print







ctx=ddcutil.Context.new()
print ctx
# ctx.repr()
mmt = ctx.get_max_max_tries()
print("max max tries: %s" % mmt)

mt1 = ctx.get_max_tries(ddcutil.RetryType.WRITE_READ_TRIES)
print("get_max_tries(WRITE_READ_TRIES) = %d" % mt1)

print("Calling get_max_tries() with invalid type parm:")
try:
   mt2 = ctx.get_max_tries(85)
except:
	print "Caught: ", sys.exc_info()[0] 

print("Setting max tries to valid value...")
ctx.set_max_tries(ddcutil.RetryType.WRITE_READ_TRIES, mmt)
mt2 = ctx.get_max_tries(ddcutil.RetryType.WRITE_READ_TRIES)
print("new WRIT_READ_TRIES: %d" % mt2)

print("Setting max tries to invalid value...")
try:
	ctx.set_max_tries(ddcutil.RetryType.WRITE_READ_TRIES, mmt+1)
except GLib.GError as excp:
	print "Caught: ", sys.exc_info()
	# print sys.exc_info[1]
	print excp

