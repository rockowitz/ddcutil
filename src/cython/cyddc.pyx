import traceback
import sys


cdef extern from "Python.h":
    ctypedef  object PyList
    ctypedef  object PyObject

    PyList PyList_New(Py_ssize_t len)

    PyList_SET_ITEM(PyObject list, Py_ssize_t index, PyObject o)


cdef extern from "stdbool.h":
    pass

cdef extern from "stdio.h":
    pass
    


cdef extern from "ddcutil_c_api.h":
    ctypedef int  integral
    ctypedef int  bint

# 
# Library build information
#

cdef extern from "ddcutil_c_api.h":
    ctypedef struct DDCA_Ddcutil_Version_Spec:
        int major
        int minor
        int micro

    const char * ddca_ddcutil_version_string() 

    DDCA_Ddcutil_Version_Spec ddca_ddcutil_version()

    ctypedef enum DDCA_Build_Option_Flags: 
        DDCA_BUILT_WITH_ADL
        DDCA_BUILT_WITH_USB
        DDCA_BUILT_WITH_FAILSIM

    int ddca_build_options()

def ddcy_ddcutil_version_string():
    return ddca_ddcutil_version_string().decode("UTF-8")

def ddcy_ddcutil_version():
    return ddca_ddcutil_version() 

def ddcutil_version2():
    v = ddca_ddcutil_version()
    return (v.major, v.minor, v.micro)

BUILT_WITH_ADL     = DDCA_BUILT_WITH_ADL
BUILT_WITH_USB     = DDCA_BUILT_WITH_USB
BUILT_WITH_FAILSIM = DDCA_BUILT_WITH_FAILSIM


def get_build_options():
    bits = ddca_build_options()
    print(bits)
    l = []
    if bits & DDCA_BUILT_WITH_ADL: 
       l.append(BUILT_WITH_ADL)
    if bits & DDCA_BUILT_WITH_USB: 
       l.append(BUILT_WITH_USB)
    if bits & DDCA_BUILT_WITH_FAILSIM: 
       l.append(BUILT_WITH_FAILSIM)
    print(l)
    s0 = set()
    if bits & DDCA_BUILT_WITH_USB: 
       s0.add(BUILT_WITH_USB)
    print(s0)
    s2 = frozenset(s0)
    s = frozenset(l)
    return s2


#
# Status Codes
#

cdef extern from "ddcutil_c_api.h":
    char * ddca_rc_name(int status_code)
    char * ddca_rc_desc(int status_code)

def rc_name(code):
    return ddca_rc_name(code).decode("UTF-8")

def rc_desc(code):
    return ddca_rc_desc(code).decode("UTF-8")


#
# Global Settings
#

cdef extern from "ddcutil_c_api.h":

    ctypedef enum DDCA_Retry_Type:
        DDCA_WRITE_ONLY_TRIES
        DDCA_WRITE_READ_TRIES
        DDCA_MULTI_PART_TRIES

    int ddca_max_max_tries() 

    void ddca_set_max_tries(int retry_type, int ct)

    int ddca_get_max_tries(int retry_type)

    void ddca_enable_verify(int onoff)

    int ddca_is_verify_enabled()

WRITE_ONLY_TRIES = DDCA_WRITE_ONLY_TRIES
WRITE_READ_TRIES = DDCA_WRITE_READ_TRIES
MULTI_PART_TRIES = DDCA_MULTI_PART_TRIES

def ddcy_get_max_max_tries():
    return ddca_max_max_tries() 

def get_max_tries(retry_type):
    return ddca_get_max_tries(retry_type)

def set_max_tries(retry_type, ct): 
    ddca_set_max_tries(retry_type, ct)

def enable_verify(onoff):
    ddca_enable_verify(onoff)

def is_verify_enabled():
    return bool(ddca_is_verify_enabled())

#
# Message Control
#

cdef extern from "ddcutil_c_api.h":

    # void ddca_set_fout(FILE * fout)
    # void ddca_set_fout_to_default()
    # void ddca_set_ferr(FILE * file)
    # void ddca_set_ferr_to_default() 

    ctypedef enum DDCA_Output_Level:
        DDCA_OL_TERSE
        DDCA_OL_NORMAL
        DDCA_OL_VERBOSE
    
    int  ddca_get_output_level()
    void ddca_set_output_level(int)
    void ddca_enable_report_ddc_errors(int truefalse)
    int  ddca_is_report_ddc_errors_enabled()

OL_TERSE   = DDCA_OL_TERSE
OL_NORMAL  = DDCA_OL_NORMAL
OL_VERBOSE = DDCA_OL_VERBOSE

def get_output_level():
    return ddca_get_output_level()

def set_output_level(ol):
    ddca_set_output_level(ol)

def enable_report_ddc_errors(truefalse):
    ddca_set_output_level(truefalse)

def is_report_ddc_errors_enabled(): 
    return bool(ddca_is_report_ddc_errors_enabled())

#
# Statistics
# 

cdef extern from "ddcutil_c_api.h":

    ctypedef enum DDCA_Stats_Type: 
        DDCA_STATS_NONE
        DDCA_STATS_TRIES
        DDCA_STATS_ERRORS
        DDCA_STATS_CALLS
        DDCA_STATS_ELAPSED
        DDCA_STATS_ALL

    void ddca_reset_stats()

    void ddca_show_stats(int stats_types, int depth)

STATS_NONE    = DDCA_STATS_NONE
STATS_TRIES   = DDCA_STATS_TRIES
STATS_ERRORS  = DDCA_STATS_ERRORS
STATS_CALLS   = DDCA_STATS_CALLS
STATS_ELAPSED = DDCA_STATS_ELAPSED
STATS_ALL     = DDCA_STATS_ALL

def reset_stats(): 
    ddca_reset_stats()

def show_stats(stats_type, depth):
    # TODO: verify that depth is integer, raise exception if not 
    ddca_show_stats(stats_type, depth)


#
# Error handling
# 



class CYDDC_Exception(Exception):

    def __init__(self, rc, msg=None):
        if msg is None:
            msg = "DDC status: %s - %s" % (rc_name(rc), rc_desc(rc))
        super(CYDDC_Exception, self).__init__(msg)
        self.status = rc


def create_ddc_exception(int rc):
    # To do: test for rc values that map to standard Python exceptions

    excp = CYDDC_Exception(rc)     # ???
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

def check_ddca_status(int rc):
    if (rc != 0):
        excp = CYDDC_Exception(rc)
        # (etype, evalue, tb) = sys.exc_info()
        

        print(excp)
        raise excp

#
# Display Access
#

cdef extern from "ddcutil_c_api.h":
    
    ctypedef void * DDCA_Display_Identifier

    ctypedef struct DDCA_Display_Ref: 
        pass

    ctypedef struct DDCA_Display_Handle:
        pass


    ctypedef struct DDCA_Adlno:
         int iAdapterIndex
         int iDisplayIndex

    ctypedef enum DDCA_IO_Mode: 
         DDCA_IO_DEVI2C
         DDCA_IO_ADL
         DDCA_IO_USB

    ctypedef union PathUnion:
         int         i2c_busno
         DDCA_Adlno  adlno
         int         hiddev_devno


    ctypedef struct DDCA_IO_Path:
         DDCA_IO_Mode  io_mode
         PathUnion pu



    ctypedef struct DDCA_Display_Info:
         char          marker[4] 
         int           dispno
         DDCA_IO_Path  path
         int           usb_bus
         int           usb_device
         const char *  mfg_id
         const char *  model_name
         const char *  sn
         # const int uint8_t * edid_bytes      # pointer to 128 bytes of edid
         DDCA_Display_Ref  dref

    ctypedef struct DDCA_Display_Info_List:
        int            ct
        DDCA_Display_Info ** info      # array whose size is determined by ct



    # ctypedef DDCA_Status int


    DDCA_Display_Info_List *  ddca_get_display_info_list()
    void ddca_free_display_info_list(DDCA_Display_Info_List * dlist)
    void ddca_report_display_info(DDCA_Display_Info * dinfo, int depth)
    void ddca_report_display_info_list(DDCA_Display_Info_List * dlist, int depth)


    int ddca_create_dispno_display_identifier(
        int dispno,
        void * pc_did)

    int ddca_free_display_identifier(void * c_did)

    char * ddca_did_repr(void * c_did)


# cdef get_display_info_list_aux(): 
#     cdef DDCA_Display_Info_List * dlist 
#     cdef DDCA_Display_Info *      dinfo
#     cdef int ct, ndx
#     dlist = ddca_get_display_info_list()
#     ct = dlist.ct
#     pylist = PyList_New(ct)
#     for ndx in range(ct):
#         dinfo = dlist.info[ndx]
#         # how to convert a C DDCA_Display_Info struct to a Python object?
#         pyinfo = dinfo             #   will cython do the right thing?
#         PyList_SET_ITEM(pylist, ndx, pyinfo)

#     return pylist




# def create_dispno_display_identifier(int dispno): 
#       cdef DDCA_Display_Identifier did
#       rc = ddca_create_dispno_display_identifier(dispno, &did)
#       if rc != 0:
#          excp = create_ddc_exception(rc)
#          raise excp
#       return did

# def free_display_identifier(DDCA_Display_Identifier did):
#       rc = ddca_free_display_identifier(did)
#       if rc != 0:
#          excp = create_ddc_exception(rc)
#          raise excp


# def did_repr(DDCA_Display_Identifier did):
#    return ddca_did_repr(did)


#
#  Display References
#

cdef extern from "ddcutil_c_api.h":
    
    int ddca_create_display_ref(void * did, void ** ddca_dref)

    int ddca_free_display_ref(void * dref)

    char * ddca_dref_repr(void * dref)

    void ddca_dbgrpt_display_ref(void * dref, int depth)


# def get_display_ref(DDCA_Display_Identifier did): 
#     cdef void * c_dref
#     rc = ddca_create_display_ref(did, &c_dref)
#     if rc != 0:
#         excp = create_ddc_exception(rc)
#         raise excp
#     return dref

# def dref_repr(DDCA_Display_Ref dref):
#     return ddca_dref_repr(dref)

# def free_display_ref(dref): 
#     rc = ddca_free_display_ref(dref)
#     if rc != 0:
#         excp = create_ddc_exception(rc)
#         raise excp

# def report_display_ref(dref, depth):
#     ddca_dbgrpt_display_ref(dref, depth)


cdef class Display_Identifier(object):
    cdef void * c_did

    # def __init__(self, void * c_did):
    #     self.c_did = c_did

    @classmethod
    def create_by_dispno(cls, int dispno): 
        cdef void * c_did
        rc = ddca_create_dispno_display_identifier(dispno, &c_did)
        if rc != 0:
           excp = create_ddc_exception(rc)
           raise excp
        result = Display_Identifier()
        result.c_did = c_did
        return result

    def free(self):
        rc = ddca_free_display_identifier(self.c_did)
        if rc != 0:
           excp = create_ddc_exception(rc)
           raise excp

    # or ?
    def __dealloc__(self):
        rc = ddca_free_display_identifier(self.c_did)
        if rc != 0:
           excp = create_ddc_exception(rc)
           raise excp    


    def __repr__(self):
        return ddca_did_repr(self.c_did).decode("UTF-8")




cdef class Display_Ref(object):
  cdef void * c_dref

  # cdef __init__(self, c_dref):
  #   self.c_dref = c_dref

  @classmethod
  def create_from_did(cls, Display_Identifier did):
      cdef void * c_dref
      rc = ddca_create_display_ref(did.c_did, &c_dref)
      if rc != 0:
          excp = create_ddc_exception(rc)
          raise excp
      dref = Display_Ref()
      dref.c_dref = c_dref
      return dref


  def free(self):
    ddca_free_display_ref(self.c_dref)

  def __repr__(self):
    return ddca_dref_repr(self.c_dref).decode("UTF-8")

  def dbgrpt(self, int depth):
    ddca_dbgrpt_display_ref(self.c_dref, depth)

# cdef extern from "<stdint.h>":
#    ctypedef uint8_t integral

cdef extern from "ddcutil_c_api.h":

    int ddca_open_display(void * dref, void ** pdh)

    int ddca_close_display(void * dh)

    char * ddca_dh_repr(void * dh)

    int ddca_get_capabilities_string(void * dh, char ** p_caps)

   
    # ctypedef DDCA_Vcp_Feature_Code uint8_t

    ctypedef struct DDCA_Non_Table_Value:
        unsigned char     mh
        unsigned char     ml
        unsigned char     sh
        unsigned char     sl


    int ddca_get_nontable_vcp_value(void * dh, unsigned char feature_code, DDCA_Non_Table_Value * p_resp)

    ctypedef enum DDCA_Vcp_Value_Type:
       DDCA_NON_TABLE_VCP_VALUE
       DDCA_TABLE_VCP_VALUE

NON_TABLE_VCP_VALUE = DDCA_NON_TABLE_VCP_VALUE
TABLE_VCP_VALUE     = DDCA_TABLE_VCP_VALUE


cdef class Vcp_Value(object):
  cdef public unsigned char feature_code
  cdef public unsigned char feature_type   # needed?

  def __init__(self, feature_code, int feature_type):
    self.feature_code = feature_code
    self.feature_type = feature_type
 
cdef class Non_Table_Vcp_Value(Vcp_Value):
    
    def __init__(self, feature_code, mh, ml, sh, sl):
      super(Non_Table_Vcp_Value, self).__init__(feature_code, DDCA_NON_TABLE_VCP_VALUE)

      self.mh = mh
      self.ml = ml
      self.sh = sh
      self.sl = sl

    # TODO: use getattr, setattr
    def cur_val(self):
      return self.sh << 16 | self.sl


cdef class Table_Vcp_Value(Vcp_Value):

    
    def __init__(self, feature_code, bytestring  ):
      super(Table_Vcp_Value, self).__init__(feature_code, DDCA_TABLE_VCP_VALUE)
      self.bytes = bytestring



cdef class Display_Handle(object):
    cdef void * c_dh

    @classmethod
    def open(cls, Display_Ref dref): 
        cdef int rc
        cdef void * c_dh
        rc = ddca_open_display(dref.c_dref, &c_dh)
        if rc != 0:
          excp = create_ddc_exception(rc)
          raise excp
        dh = Display_Handle()
        dh.c_dh = c_dh
        return dh

    def close(self):
        cdef int rc
        rc = ddca_close_display(self.c_dh)
        if rc != 0:
          excp = create_ddc_exception(rc)
          raise excp

    def __repr__(self):
        return ddca_dh_repr(self.c_dh).decode("UTF-8")

    def get_capabilities_string(self):
      cdef char * s
      cdef int rc
      rc = ddca_get_capabilities_string(self.c_dh, &s)
      if rc != 0:
          excp = create_ddc_exception(rc)
          raise excp
      return s.decode("UTF-8")

    def get_nontable_vcp_value(self, feature_code):
        cdef DDCA_Non_Table_Value resp

        # n. fills in existing DDCA_Non_Table_Value, does not allocate
        rc = ddca_get_nontable_vcp_value(self.c_dh, feature_code, &resp)
        if rc != 0:
          excp = create_ddc_exception(rc)
          raise excp

        # Todo: create a Non_Table_Vcp_Value instance, return it
        # return  resp
        fcode = resp.feature_code
        mh = resp.mh
        ml = resp.ml
        sh = resp.sh
        sl = resp.sl
        print("fcode:  x%02x" % fcode)
        print("sl: x%02x" % sl)

        resp = Non_Table_Vcp_Value(fcode, mh, ml, sh, sl)

        # raise("unimplemented")
        return resp

        



cdef extern from "ddcutil_c_api.h":

    ctypedef struct DDCA_Capabilities: 
        char      marker[4]
        char *    unparsed_string
        # DDCA_MCCS_Version_Spec  version_spec
        int       vcp_code_ct




# cdef class Capabilities(object): 

#     @classmethod
#     def parse()