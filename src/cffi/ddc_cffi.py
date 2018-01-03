import sys

sysver = 0
if sys.version_info[0] == 2 and sys.version_info[1] >= 7:
    sysver = 2
elif sys.version_info[0] == 3 and sys.version_info[1] >= 4:
    sysver = 3
else:
    raise Exception("Unsupported Python version: %d.%d" % (sys.version_info[0], sys.version_info[1] ) )
print("sysver: %d" % sysver)

import traceback
# from sets import Set, ImmutableSet

if sysver == 2:
    from _ddccffi2 import ffi,lib
else:
    from _ddccffi3 import ffi, lib

# copy all the constants - should be able to do this by introspection
WRITE_ONLY_TRIES    = lib.DDCA_WRITE_ONLY_TRIES
WRITE_READ_TRIES    = lib.DDCA_WRITE_READ_TRIES
MULTI_PART_TRIES    = lib.DDCA_MULTI_PART_TRIES

BUILT_WITH_ADL      = lib.DDCA_BUILT_WITH_ADL
BUILT_WITH_USB      = lib.DDCA_BUILT_WITH_USB
BUILT_WITH_FAILSIM  = lib.DDCA_BUILT_WITH_FAILSIM

NON_TABLE_VCP_VALUE = lib.DDCA_NON_TABLE_VCP_VALUE 
TABLE_VCP_VALUE     = lib.DDCA_TABLE_VCP_VALUE

NON_TABLE_VCP_VALUE_PARM = lib.DDCA_NON_TABLE_VCP_VALUE_PARM 
TABLE_VCP_VALUE_PARM     = lib.DDCA_TABLE_VCP_VALUE_PARM
UNSET_VCP_VALUE_TYPE_PARM     = lib.DDCA_UNSET_VCP_VALUE_TYPE_PARM


#
# Utilities
#

def bits_to_frozenset(bits):
  s = set()
  for i in range(16):
    v = 1 << i
    # print("v,i = %d,%d" % (v,i))
    if bits & v:
      s.add(v)
  return frozenset(s)




#
# Error handling
# 

class DDC_Exception(Exception):

    def __init__(self, rc, msg=None):
        s0 = "DDC status: %s - %s" % (rc_name(rc), rc_desc(rc))
        if msg:
          s0 = s0 + ": " + msg
        super(DDC_Exception, self).__init__(s0)
        self.status = rc


def create_ddc_exception(rc):
    # To do: test for rc values that map to standard Python exceptions

    excp = DDC_Exception(rc)     # ???
    # To do: adjust the stack
    (etype, evalue, tb) = sys.exc_info()
    print("traceback.print_tb(): ")
    traceback.print_tb(tb)
    print("traceback.print_exception():")
    traceback.print_exception(etype, evalue, tb)
    print("traceback.print_exc():")
    traceback.print_exc()

    x = traceback.extract_tb(tb)
    print(type(x))
    print(x)
    y = traceback.extract_stack(tb)
    print(type(y))
    print(y)

    print(excp)
    return excp

def check_ddca_status(rc):
    if (rc != 0):
        excp = DDC_Exception(rc)
        # (etype, evalue, tb) = sys.exc_info()    

        print(excp)
        raise excp


def from_cdata_string(cdata_string):
    s = ffi.string(cdata_string)
    if sysver == 3:
        s = s.decode("UTF-8")
    return s

#
# API functions - Build Information
#

def ddcutil_version_string():
    return from_cdata_string(lib.ddca_ddcutil_version_string())

def ddcutil_version():
  t = lib.ddca_ddcutil_version()
  return (t.major, t.minor, t.micro)

def build_options():
  return lib.ddca_build_options()

def build_options_as_set():
  bits = lib.ddca_build_options()
  return bits_to_frozenset(bits)


#
# API functions - Status Code
#

def rc_name(status_code):
    return from_cdata_string(lib.ddca_rc_name(status_code))

def rc_desc(status_code):
    return from_cdata_string( lib.ddca_rc_desc(status_code))

#
# MCCS version spec
#

MCCS_VNONE =  lib.DDCA_VNONE 
MCCS_V10   =  lib.DDCA_V10 
MCCS_V20   =  lib.DDCA_V20
MCCS_V21   =  lib.DDCA_V21
MCCS_V30   =  lib.DDCA_V30
MCCS_V22   =  lib.DDCA_V22 



def mccs_version_id_name(version_id):
   return from_cdata_string(lib.ddca_mccs_version_id_name(version_id))

def mccs_version_id_desc(version_id):
   return from_cdata_string(lib.ddca_mccs_version_id_desc(version_id))




#
# Retry Management 
#

def max_max_tries():
    return lib.ddca_max_max_tries()

def get_max_tries(retry_type):
    return lib.ddca_get_max_tries(retry_type)

def set_max_tries(try_type, ct): 
    rc = lib.ddca_set_max_tries(try_type, ct)
    if rc != 0:
        # raise Exception("ddca_set_max_tries, try_type=%d, ct=%d, returned: %d" % (try_type, ct, rc))
        raise DDC_Exception(rc, "try_type=%d, ct=%d" % (try_type, ct))


#
# Feature Information
#

def get_feature_name(feature_code):
    s = ffi.string(lib.ddca_get_feature_name(feature_code))
    if sysver==3:
        s = s.decode("UTF-8")
    return s


class Feature_Value_Table(object):
    
    def __init__(self, c_table):
        self._c_table = c_table
        
    @classmethod
    def create_from_c_table(cls, c_table):
        print("(Feature_Value_Table.create_from_c_table) c_table=%s" % c_table)
        table = Feature_Value_Table(c_table)
        table.entries = {}
        ndx = 0;
        while True: 
            (k,v) = (c_table[ndx].value_code, c_table[ndx].value_name)
            if k == 0 and v == ffi.NULL:
                break
            v2 = ffi.string(v)
            if sysver==3:
                v2 = v2.decode("UTF-8")
            # print("ndx=%d, c_table=%s, value_code=%s, value_name=%s" % (ndx, c_table[ndx], k, v2) )
            table.entries[k] = v2
            ndx = ndx+1
        return table

    @classmethod
    def create_by_query(cls, feature_code, mccs_version_id):
        print("(create_by_query) feature_code = 0x%02x, mccs_version=%d" % (feature_code,mccs_version_id) )
        # TODO: validate mccs_version_id
        
        pc_table = ffi.new("DDCA_Feature_Value_Table *", init=ffi.NULL)
        rc = lib.ddca_get_simple_sl_value_table(feature_code, mccs_version_id, pc_table)
        if rc != 0:
            raise create_ddc_exception(rc)
        t = Feature_Value_Table.create_from_c_table(pc_table[0])
        # print(t)
        # return pc_table[0]
        return t
            
    def lookup(value_code):
        return self.entries[value_code]
        
    def report(self):
        print("Feature_Value_Table:")
        if len(self.entries) == 0:
            print("    No entries")
        else:
            for k in sorted(self.entries.keys()):
                v = self.entries[k]
                print("   %2d  %s" % (k,v))
        

    def __repr__(self):
        return "[Feature_Value_Table:  %d entries]" % len(self.entries)


class Feature_Info(object):

    def __init__(self, c_feature_info):
        self._c_feature_info = c_feature_info
        
    @classmethod
    def create_by_vcp_version(cls, feature_code, mccs_version_id):
        print("(create_by_vcp_version) mccs_version_id=%d" % mccs_version_id)
        # TODO: validate mccs_version_id
        
        c_feature_info = ffi.new("DDCA_Version_Feature_Info **", init=ffi.NULL)
        rc = lib.ddca_get_feature_info_by_vcp_version(feature_code, mccs_version_id, c_feature_info)
        if rc != 0:
            excp = create_ddc_exception(rc)
            raise excp
        finfo = Feature_Info(c_feature_info[0])
        return finfo
        
        
    @property
    def feature_code(self):
        return self._c_feature_info.feature_code
        
    @property       
    def vspec(self):
        major = self._c_feature_info.vspec.major
        minor = self._c_feature_info.vspec.minor
        return (major, minor)
        
    @property        
    def version_id(self):
        return self._c_feature_info.version_id
        
    @property        
    def desc(self):
        return from_cdata_string( self._c_feature_info.desc )
        
    @property        
    def feature_name(self):
        return from_cdata_string( self._c_feature_info.feature_name )

    @property
    def feature_flags(self):
        return self._c_feature_info.feature_flags
        
    @property
    def feature_flags_as_set(self):
          bits = self.feature_flags
          return bits_to_frozenset(bits)
        
    @property
    def sl_values(self):
        c_fvt = self._c_feature_info.sl_values 
        if c_fvt == ffi.NULL:
            result = None
        else:
            result = Feature_Value_Table.create_from_c_table(c_fvt)
        return result
        
    def report(self):
        print("Version feature info for code 0x%02x, MCCS version: %d.%d:" %
               (self.feature_code, self.vspec[0], self.vspec[1]) )
        vdesc = mccs_version_id_desc(self.version_id)
        print("   version id:    %d - %s" % (self.version_id, vdesc) )
        print("   feature_name:  %s" % self.feature_name)
        print("   desc:          %s" % self.desc)
        print("   feature_flags: 0x%04x" % self.feature_flags)
        print("   feature_flags_as_set:  %s" % self.feature_flags_as_set)
        if self.sl_values:
            self.sl_values.report()
        
        
    def __repr__(self):
        s = "[Feature_Info: code: 0x%02x, MCCS version: %d.%d]" % (self.feature_code, self.vspec[0], self.vspec[1]) 
        return s



#
# Capabilities
#

class Parsed_Capabilities(object):
    
    def __init__(self, c_pcaps):
        super(Parsed_Capabilities,self).__init__()
        self.__dict__['_c_pcaps'] = c_pcaps
             
    @classmethod
    def create_from_string(cls, cap_string):
        print("(create_from_string) cap_string = %s" % cap_string)
        c_parsed_caps = ffi.new("DDCA_Capabilities **", init=ffi.NULL)
        print(c_parsed_caps)
        print(type(cap_string))
        print(type(c_parsed_caps))
        rc = lib.ddca_parse_capabilities_string(cap_string, c_parsed_caps)
        if rc != 0:
            excp = create_ddc_exception(rc)
            raise excp
        parsed = Parsed_Capabilities(c_parsed_caps[0])
        return parsed
        
        
    # TODO: properties for all the fields
    
    # replace with __del__()?
    def free(self):
        # returns void
        lib.ddca_free_parsed_capabilities(self._c_pcaps)

    def report(self, depth):
        # returns void
        lib.ddca_report_parsed_capabilities(self._c_pcaps, depth)   
            
    def __repr__(self):
        return "[Parsed_Capabilities: _c_pcaps=%s]" % self._c_pcaps
        
        



# 
# typedef enum {
#    DDCA_IO_DEVI2C,     /**< Use DDC to communicate with a /dev/i2c-n device */
#    DDCA_IO_ADL,        /**< Use ADL API */
#    DDCA_IO_USB         /**< Use USB reports for a USB connected monitor */
# } DDCA_IO_Mode;
# 
# typedef struct {
#    DDCA_IO_Mode io_mode;        ///< physical access mode
#    union {
#       int        i2c_busno;     ///< I2C bus number
#       DDCA_Adlno adlno;         ///< ADL iAdapterIndex/iDisplayIndex pair
#       int        hiddev_devno;  ///* USB hiddev device  number
#    };
# } DDCA_IO_Path;


IO_DEVI2C   = lib.DDCA_IO_DEVI2C
IO_ADL      = lib.DDCA_IO_ADL
IO_USB      = lib.DDCA_IO_USB

class IO_Path(object):
    def __init__(self, io_mode):
        print("(IO_Path.__init__) io_mode=%d" % io_mode)
        # does this get me anything I can't get from isinstance()
        self.io_mode = io_mode
        
    @classmethod
    def create_from_cdata(cls, cdata):
        print("(IO_Path.create_from_cdata) %s" % dir(cdata))
        if cdata.io_mode == IO_DEVI2C:
            instance = DEVI2C_IO_Path(cdata.i2c_busno)
        elif cdata.io_mode == IO_ADL:
            instance = DEVADL_IO_PATH(adlno.iAdapterIndex, adlno.iDisplayIndex)     # TODO: check
        else:
            instance = DEVUSB_IO_PATH(hiddev_devno)
        return instance
  
class DEVI2C_IO_Path(IO_Path):
    
    def __init__(self, i2c_busno):
        print("(DEVI2C_IO_Path) i2c_busno=%d" % i2c_busno)
        super(DEVI2C_IO_Path,self).__init__(IO_DEVI2C)
        self.busno = i2c_busno
    
    def __repr__(self):
        return "[DEVI2C_IO_Path: busno=%d]"  % self.busno
    
class ADL_IO_Path(IO_Path): 
    def __init_(self, iAdapterIndex, iDisplayIndex):
        super(ADL_IO_Path,self).__init__(IO_ADL)
        self.iAdapterIndex = iAdapterIndex
        self.iDisplayIndex = iDisplayIndex

    def __repr__(self):
        return "[ADL_IO_Path: adlno=(%d,%d)]" % (self.iAdapterIndex, self.iDisplayIndex)


class USB_IO_Path(IO_Path):
    
    def __init__(self, hiddev_devno):
        super(USB_IO_Path,self).__init__(IO_USB)
        self.hiddev_devno = hiddev_devno

    def __repr__(self):
        return "[USB_IO_Path: hiddev_devno=%d]" % self.hiddev_devno



#
# Dislay_Information
#

class Display_Info(object):
    
    def __init__(self, c_dinfo):
        super(Display_Info,self).__init__()
        self.__dict__['_c_dinfo'] = c_dinfo
        

    @property
    def dispno(self):
        return self._c_dinfo.dispno
#     @dispno.setter
#     def dispno(self, value):
#         raise AttributeError()
        
    @property
    def mfg_id(self):
        # print("model_name")
        result = ffi.string(self._c_dinfo.mfg_id)
        if sysver == 3:
            result = result.decode("UTF-8")
        return result
#     @mfg_id.setter
#     def mfg_id(self, value):
#         raise AttributeError()
    
    @property
    def model_name(self):
        # print("model_name")
        result = ffi.string(self._c_dinfo.model_name)
        if sysver == 3:
            result = result.decode("UTF-8")
        return result
    @model_name.setter
#     def model_name(self, value):
#         # print("model_name setter")
#         raise AttributeError()
      
    @property
    def sn(self):
        result = ffi.string(self._c_dinfo.sn)
        if sysver == 3:
            result = result.decode("UTF-8")
        return result
#     @sn.setter
#     def sn(self, value):
#         raise AttributeError()
        
    @property
    def edid_bytes(self):
        print("edid_bytes getter")
        return bytes( ffi.buffer(self._c_dinfo.edid_bytes, 128) )
#     @edid_bytes.setter
#     def edid_bytes(self, vale):
#         raise AttributeError("edid_bytes cannot be modified")
         
    @property
    def path(self):
        a = self._c_dinfo.path
        # print(a)
        b = IO_Path.create_from_cdata(a)
        # print(b)
        return b
#     @path.setter
#     def sn(self, value):
#         raise AttributeError()   
             
    @property
    def dref(self):
        result = (self._c_dinfo.dref)
        return result
#     @dref.setter
#     def dref(self, value):
#         raise AttributeError()
        
    @property
    def usb_bus(self):
        return self._c_dinfo.usb_bus
#     @usb_bus.setter
#     def usb_bus(self, value):
#         raise AttributeError()
 
    @property
    def usb_device(self):
        return self._c_dinfo.usb_device
 
    def __setattr__(self, name, value): 
        if name == "_c_dinfo":
            raise AttributeError("can't set attribute")
        else:
            object.__setattr__(self, name, value)
           
    def __repr__(self):
        result = "[Display_Info: dispno=%d, mfg=%s]" %\
                         ( self._c_dinfo.dispno, ffi.string(self._c_dinfo.mfg_id) )
        return result
            
            

#     def __getattr__(self, item):
#         print("(__getattr__) item=%s" % (item))
#         # if item == "mfg_id":
#         #     retval = ffi.string(self._c_dinfo.mfg_id) 
#         #     if sysver == 3:
#         #         retval = retval.decode("UTF-8")
#         # if item == "sn":
#         #     retval = ffi.string(self._c_dinfo.sn)
#         #     if sysver == 3:
#         #        retval = retval.decode("UTF-8")
#         # elif item == "edid_bytes":
#         #    edidbuf = ffi.buffer(self._c_dinfo.edid_bytes, 128)
#         #    retval = bytes(edidbuf)
#         elif item == "dref":
#             retval = Display_Ref(self._c_dinfo.dref)
#         elif item == "path":
#             a = self._c_dinfo.path
#             print(a)
#             b = IO_Path.create_from_cdata(a)
#             print(b)
#             retval = b
#         else:
#             raise AttributeError("Invalid: %s" % item)
#             # retval = getattr(self,item)
#         return retval


      


def get_display_info_list():
    print("(get_display_info_list) Starting")
    dilist = lib.ddca_get_display_info_list()
    print("ct: %d" % dilist.ct)
    result = []
    for ndx in range(dilist.ct):
        di = dilist.info[ndx]
        print("di: ", dir(di))
        print( ffi.string(di.mfg_id))
        dinfo = Display_Info(di)
        result.append(dinfo)
    return result



class Display_Identifier(object):
    #  cdef void * c_did
    #  self.c_did = NULL

    # def __init__(self, void * c_did):
    #     self.c_did = c_did

    @classmethod
    def create_by_dispno(cls, dispno): 
        # c_did1 = ffi.new("void **")
        c_did2 = ffi.new("void **", init=ffi.NULL)
        # cannot instantiate ctype void of unknown size
        # c_did3 = ffi.new("void *")
        # c_did4 = ffi.new("void *", init=ffi.NULL)

        rc = lib.ddca_create_dispno_display_identifier(dispno, c_did2)
        if rc != 0:
           excp = create_ddc_exception(rc)
           raise excp
        # print(c_did2)
        # print(ffi.unpack(c_did2,1))

        c_did2u=c_did2[0]
        # print(c_did2u)

        result = Display_Identifier()
        result.c_did = c_did2u
        return result

    def free(self):
        rc = lib.ddca_free_display_identifier(self.c_did)
        if rc != 0:
           excp = create_ddc_exception(rc)
           raise excp

    # or ?
    def __dealloc__(self):
        rc = lib.ddca_free_display_identifier(self.c_did)
        if rc != 0:
           excp = create_ddc_exception(rc)
           raise excp    


    def __repr__(self):
        result = ffi.string(lib.ddca_did_repr(self.c_did))
        if sysver == 3:
            result = result.decode("UTF-8")
        return result




class Display_Ref(object):
  # cdef void * c_dref

  def __init__(self, c_dref):
      self.c_dref = c_dref

  @classmethod
  def create_from_did(cls, did):
      c_dref = ffi.new("void **", init=ffi.NULL)
      rc = lib.ddca_create_display_ref(did.c_did, c_dref)
      if rc != 0:
          excp = create_ddc_exception(rc)
          raise excp
      # dref = Display_Ref()
      # dref.c_dref = c_dref[0]
      dref = Display_Ref(c_dref[0])
      return dref


  def free(self):
    rc = lib.ddca_free_display_ref(self.c_dref)
    if rc != 0:
           excp = create_ddc_exception(rc)
           raise excp

  def __repr__(self):
        result = ffi.string(lib.ddca_dref_repr(self.c_dref)) 
        if sysver == 3:
            result = result.decode("UTF-8")
        return result

  def dbgrpt(self, depth):
    lib.ddca_report_display_ref(self.c_dref, depth)


# source: https://gist.github.com/inactivist/4ef7058c2132fa16759d

def __convert_struct_field( s, fields ):
    for field,fieldtype in fields:
        if fieldtype.type.kind == 'primitive':
            yield (field,getattr( s, field ))
        else:
            yield (field, convert_to_python( getattr( s, field ) ))

def convert_to_python(s):
    type=ffi.typeof(s)
    if type.kind == 'struct':
        return dict(__convert_struct_field( s, type.fields ) )
    elif type.kind == 'array':
        if type.item.kind == 'primitive':
            if type.item.cname == 'char':
                return ffi.string(s)
            else:
                return [ s[i] for i in range(type.length) ]
        else:
            return [ convert_to_python(s[i]) for i in range(type.length) ]
    elif type.kind == 'primitive':
        return int(s)

class Display_Handle(object):

    @classmethod
    def open(cls, dref): 
        pc_dh = ffi.new("void **", init=ffi.NULL)
        rc = lib.ddca_open_display(dref.c_dref, pc_dh)
        if rc != 0:
          excp = create_ddc_exception(rc)
          raise excp
        dh = Display_Handle()
        dh.c_dh = pc_dh[0]
        return dh

    def close(self):
        rc = lib.ddca_close_display(self.c_dh)
        if rc != 0:
          excp = create_ddc_exception(rc)
          raise excp

    def __repr__(self):
        result = ffi.string(lib.ddca_dh_repr(self.c_dh))
        if sysver == 3:
            result = result.decode("UTF-8")
        return result

    def get_capabilities_string(self):
      ps = ffi.new("char **", init=ffi.NULL)
      rc = lib.ddca_get_capabilities_string(self.c_dh, ps)
      if rc != 0:
          excp = create_ddc_exception(rc)
          raise excp

      # print("ps: ", ps)
      # print("unpacked: ", ffi.unpack(ps,4))

      # s=ps[0]
      # print("s: ", s)
      # s2 = ffi.string(s)
      # print("s2: ", s2)

      return ffi.string(ps[0])

    def get_mccs_version_id(self):
       pid = ffi.new("DDCA_MCCS_Version_Id *")
       # pid = ffi.new("lib.DDCA_MCCS_Version_Id *", init=ffi.NULL)
       # pid = ffi.new("int *")
       rc = lib.ddca_get_mccs_version_id(self.c_dh, pid)
       if rc != 0:
          excp = create_ddc_exception(rc)
          raise excp
       print("pid: ", pid)
       print("unpacked: ", ffi.unpack(pid,4))

       id = pid[0]
       print("id: ", id)       
       return id


    def get_mccs_version(self):
      print("(get_mccs_version) Starting.")
      pv = ffi.new("DDCA_MCCS_Version_Spec *")
      rc = lib.ddca_get_mccs_version(self.c_dh, pv)
      if rc != 0:
          raise create_ddc_exception(rc)
      print("pv: ", pv)
      print("unpacked: ", ffi.unpack(pv,4))

      ver = pv[0]
      print("ver: ", ver)   
      # print("unpacked: ", ffi.unpack(ver,2))    

      print ("ver.major: ", ver.major)

      v2 = convert_to_python(ver)
      print("v2: ", v2)
      # return as dictionary
      # return v2

      return (ver.major, ver.minor)


    def get_vcp_value(self, feature_code, feature_type=lib.DDCA_UNSET_VCP_VALUE_TYPE_PARM):
       print("(get_vcp_value) Starting")
       pvalrec = ffi.new("DDCA_Any_Vcp_Value **", init=ffi.NULL)
       rc = lib.ddca_get_any_vcp_value(self.c_dh, feature_code, feature_type, pvalrec)
       if rc != 0:
          excp = create_ddc_exception(rc)
          raise excp

       # print("pvalrec: ", pvalrec)
       # print("unpacked: ", ffi.unpack(pvalrec,8))

       valrec = pvalrec[0]
       print("valrec: ", valrec)
       # v2 = convert_to_python(valrec)
       # print("v2: ",  v2)
       
       # return valrec
       if valrec.value_type == lib.DDCA_NON_TABLE_VCP_VALUE:
           retval = Non_Table_Vcp_Value(
                        valrec.opcode, 
                        valrec.val.c_nc.mh, 
                        valrec.val.c_nc.ml, 
                        valrec.val.c_nc.sh, 
                        valrec.val.c_nc.sl )
       else:
           # bytestring = ??
           bytestring = None
           retval = Table_Vcp_Value(valrec.opcode, bytestring)
       return retval


    def get_formatted_vcp_value(self, feature_code):
       ps = ffi.new("char **", init=ffi.NULL)
       rc = lib.ddca_get_formatted_vcp_value(self.c_dh, feature_code, ps)
       if rc != 0:
          excp = create_ddc_exception(rc)
          raise excp
       return ffi.string(ps[0])
       
    def set_nc_vcp_value(self, vcp_code, sh, sl):
        rc = lib.ddca_set_raw_vcp_value(self.c_dh, vcp_code, sh, sl)
        if rc != 0:
          excp = create_ddc_exception(rc)
          raise excp
        
    def set_simple_nc_vcp_value(self, vcp_code, sl):
        self.set_nc_vcp_value(vcp_code, 0, sl)
        
    def set_continuous_vcp_value(self, vcp_code, newval):
        sh = newval >> 8
        sl = newval % 0xff
        self.set_nc_vcp_value(vcp_code, sh, sl)
        
       
class Vcp_Value(object):
    
    def __init__(self):
        print("Vcp_Value.__init__()")
        pass
        
        
class Non_Table_Vcp_Value(Vcp_Value):
    
    def __init__(self, opcode, mh, ml, sh, sl):
        # print("Non_Table_Vcp_Value.__init__()")
        super(Non_Table_Vcp_Value,self).__init__()
        self.opcode = opcode
        self.mh = mh
        self.ml = ml
        self.sh = sh
        self.sl = sl
        # print("Non_Table_Vcp_Value.__init__() Done")
        
            
    def __getattr__(self, item):
        # print("(__getattr__) item=%s" % (item))
        if item == "cur_val":
            retval = self.sh << 8 | self.sl
        elif item == "max_val":
            retval = self.mh << 8 | self.ml
        else:
            raise AttributeError("Invalid: %s" % item)
            # retval = getattr(self,item)
        return retval

    def __setattr__(self, item, newval):
        # print("(__setattr__) item=%s, newval=%s" % (item,newval))
        if item == "curval":
            #avoid recursive call
            Vcp_Value.__setattr__(self, "sh", newval >> 8)
            Vcp_Value.__setattr__(self, "sl", newval & 0xff)
        elif item == "maxval":
            Vcp_Value.__setattr__(self, "mh", newval >> 8)
            Vcp_Value.__setattr__(self, "ml", newval & 0xff)
        else:
            # raise AttributeError("Invalid: %s" % item)
            Vcp_Value.__setattr__(self, item, newval)
        
    def __repr__(self):
        result = "[Vcp_Value: Feature 0x%02x, type=NON_TABLE, mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x, maxval=%d, curval=%d]" %\
                         ( self.opcode, self.mh, self.ml, self.sh, self.sl, self.max_val, self.cur_val)
        return result
        
        
class Table_Vcp_Value(Vcp_Value):
    
    def __init__(self, opcode, bytestring):
        self.opcode = opcode
        self.bytestring = bytestring
        
    def __repr__(self):
        result = "[Vcp_Value: Feature 0x%02x, type=TABLE, bytes=???" % (self.opcode)
        return result
                
                
