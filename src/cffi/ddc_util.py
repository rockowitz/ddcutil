import sys

def required_pyvers(required_v2, required_v3):
    v2_minor = int(str(required_v2)[2:])
    v3_minor = int(str(required_v3)[2:])
    print("v2_minor: %d, v3_minor: %d" % ( v2_minor, v3_minor))

    sysver = 0
    if sys.version_info[0] == 2 and sys.version_info[1] >= v2_minor:
        sysver = 2
    elif sys.version_info[0] == 3 and sys.version_info[1] >= v3_minor:
        sysver = 3
    else:
        raise Exception("Unsupported Python version: %d.%d" % (sys.version_info[0], sys.version_info[1] ) )
    print("sysver: %d" % sysver)
    return sysver

    
def show_args():
    print("sys.argv:")
    for i in range(len(sys.argv)):
        print("   %s" % sys.argv[i])