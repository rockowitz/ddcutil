import sys

sysver = 0
if sys.version_info[0] == 2 and sys.version_info[1] >= 7:
    sysver = 2
elif sys.version_info[0] == 3 and sys.version_info[1] >= 4:
    sysver = 3
else:
    raise Exception("Unsupported Python version: %d.%d" % (sys.version_info[0], sys.version_info[1] ) )
print("sysver: %d" % sysver)



import ddc_cffi as ddccffi
import time

import util

# from ddc_cffi import ffi


def test_build_information():
    print("")
    print("Testing build information...")
    
    
    s = ddccffi.ddcutil_version_string()
    print( "ddcutil_version_string(): %s" % s)

    t = ddccffi.ddcutil_version()
    print("ddcutil_version(): %d.%d.%d" % t)

    buildopts = ddccffi.build_options()
    print("build_options: %x", buildopts)

    buildopts_as_set = ddccffi.build_options_as_set()
    print("build_options_as_set():  %s" % buildopts_as_set)


def test_status_code_management():
    pass
    
def test_mccs_version_id():
    pass
    
    
def test_retry_management():
    
    n = ddccffi.max_max_tries() 
    print( "max_max_tries(): %d" % n)

    ct = ddccffi.get_max_tries(ddccffi.WRITE_ONLY_TRIES)
    print("WRITE_ONLY_TRIES: %d" % ct)

    print("Calling set_max_tries() with valid argument")
    ddccffi.set_max_tries(ddccffi.WRITE_ONLY_TRIES, 5)

    print("Calling set_max_tries() with invalid argument")
    try:
        ddccffi.set_max_tries(ddccffi.WRITE_ONLY_TRIES, 55)
    except Exception as excp:
        print("set_max_tries threw: %s", excp)



print("")
print("Starting")

test_build_information()
test_status_code_management()
test_mccs_version_id()
test_retry_management()

def test_capabilities(dh):
    print("Testing capabilities for %s..." % dh)
    
    cap_string = dh.get_capabilities_string()
    print("capabilities string:")
    print(cap_string)

    parsed = ddccffi.Parsed_Capabilities.create_from_string(cap_string)
    print("parsed: %s" % parsed)
    
    depth=3
    parsed.report(depth)

    parsed.free() 

    
    
def test_one_feature_code(code):
    print("Getting name for feature 0x%04x... " % code)
    s = ddccffi.get_feature_name(code)
    print("Name for feature 0x%04x: %s" % (code, s))
    
def test_feature_information(): 

    print("Invariant feature information:")
    
    test_one_feature_code(0x10)
    test_one_feature_code(0x00)
    test_one_feature_code(0xff)
    try: 
       test_one_feature_code(0x0101)
    except OverflowError:
        print("Recevied expected OverflowError")
    print("")


did = ddccffi.Display_Identifier.create_by_dispno(2)
print("did_repr: %s" % did)

print("Free did:")
did.free()

did = ddccffi.Display_Identifier.create_by_dispno(2)

dref = ddccffi.Display_Ref.create_from_did(did)
print("dref: %s" % dref)

dh = ddccffi.Display_Handle.open(dref)
print("dh: %s" % dh)

test_capabilities(dh)

test_feature_information()


print("version id:")
mccs_id = dh.get_mccs_version_id()
print(mccs_id)
print(ddccffi.mccs_version_id_name(mccs_id))
print(ddccffi.mccs_version_id_desc(mccs_id))


mccs_ver = dh.get_mccs_version()
print("get_mccs_version(): ", mccs_ver)

print("get_formatted_vcp_value(x10):")
s = dh.get_formatted_vcp_value(0x10)
print(s)
print("")

print("get_vcp_value(x10):")
vcpval = dh.get_vcp_value(0x10)
print("get_vcp_value() returned: ", vcpval)
print(vcpval)

print("Setting value to 75...")
dh.set_continuous_vcp_value(0x10,75)
vcpval = dh.get_vcp_value(0x10)
print("get_vcp_value() returned: ", vcpval)
time.sleep(3)

print("Setting original value...")
dh.set_simple_nc_vcp_value(0x10, vcpval.sl)
vcpval = dh.get_vcp_value(0x10)
print("get_vcp_value() returned: ", vcpval)

dilist = ddccffi.get_display_info_list()
print("ct: %d" % len(dilist))
for ndx in range(len(dilist)):
    print(dilist[ndx])
    
di=dilist[0]
print(di.mfg_id)
print(di.model_name)
print(di.sn)
edid = di.edid_bytes
util.hex_dump(edid)
dref = di.dref
path = di.path

fvt = ddccffi.Feature_Value_Table.create_by_query(0x60, ddccffi.MCCS_V21)
# fvt = ddccffi.Feature_Value_Table.create_from_c_table(c_table)
print("fvt: %s" % fvt)
fvt.report()

fi = ddccffi.Feature_Info.create_by_vcp_version(0x60, ddccffi.MCCS_V21)
print("fi:", fi)
fi.report()

fi = ddccffi.Feature_Info.create_by_vcp_version(0x10, ddccffi.MCCS_V20)
print("fi:", fi)
fi.report()






       