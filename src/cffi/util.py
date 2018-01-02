# util.py 

###  Module:  util.py 
###
###  Purpose: generic utility functions not tied to color management

import inspect
import io
import os
import os.path
import struct
import subprocess



def read_binary_file(fqfn): 
   """Reads a binary file into a bytes list. 

      Arguments:  fqfn  fully qualified file name 
   """
   with open(fqfn, 'rb') as f: 
      bytelist = f.read() 
   return bytelist 


def write_binary_file(fqfn, byteseq):
   with open(fqfn, 'wb') as f:
      f.write(byteseq)


# redundant! - not quite - different return types
def getFileBytes(fn): 
   """Reads an entire file into an array of bytes

      Arguments: 
         fn    file name 

      Raises:  
         IOError 
   """
   try: 
      f = io.FileIO(fn) 
      byteseq = f.readall() 
      f.close() 
   except Exception as err: 
      # print err
      raise
   # print "Byte count: ", len(bytes) 
   return byteseq 

def writeFile(fn, byteseq): 
   try: 
      f = io.FileIO(fn)
      f.write(byteseq)
   except Exception as err:
      print( err )
      raise




def byte2int(byte): 
   # requires a string argument of length 1: 
   result = struct.unpack('B',byte)[0] 
   return result 
   

def byteToHexString(byte): 
   """Given a single byte, returns its representation as a 2 character hex string, 
   using lower case characters.   There is no leading "0x".  
   For example, b'\FF' returns ff.
   """
   result = '%02x' % ord(byte) 
   return result 


# def byte2Number(byte): 
#    intval = struct.unpack('>i',byte)[0]       # fails 
#    return intval

def hexString(byteseq): 
   """ Given a sequence of bytes, return its representation as a hex string. 
   using lower case characters.""" 
   if isinstance(byteseq, bytearray):
      result = ''.join( '%02x' % byte for byte in byteseq ) 
   else:
      result = ''.join( '%02x' % ord(byte) for byte in byteseq ) 
   # print "(hexString) Returning: %s" % newResult
   return result 


def bytes2NumberList(byteseq): 
   """Given a list of bytes or a string whose contents is to be interpreted as a sequence of bytes, 
      returns an array where each entry is the value of a byte, interpreted as an 
      8 bit unsigned integer. 

      Arguments:  bytes  string of bytes or list of bytes
      Returns:    array of integers 
   """
   result = [] 
   for b in byteseq: 
      result.append( ord(b)  ) 
      # result += byte2Number(b) 
   return result 

   
def hexStringToByteArray(hexstring):
   """Converts hex string to byte array. 

   Arguments: hexstring 
   Returns:   array of bytes 

   Raises:    ValueError if invaid hex string
   """
   if len(hexstring) % 2 != 0:
      raise ValueError("Invalid hex string: %s" % hexstring)
   ba = []
   for i in range(0, len(hexstring), 2):
      # raises ValueError if invalid hex characters in string:
      ba.append( chr( int( hexstring[i:i+2],16) ) )
   return ba


def hexStringToByteString(hexstring):
   """Converts hex string to byte string

   Arguments: hexstring 
   Returns:   string of bytes

   Raises:    ValueError if invaid hex string
   """
   return ''.join(hexStringToByteArray(hexstring))



def isPrintableChar(ch): 
   return ord(ch) >= 32 and ord(ch) < 127 
   

def isSingleByte(val): 
   """Determines if a value is a string of length 1"""
   if type(val) == str and val.len() == 1: 
      result = True 
   else: 
      result = False
   return result 


def isByteList(val): 
   result = False
   if type(val) == list: 
      result = True 
      for b in val: 
         if type(b) != str or b.len() != 1: 
            result = False
            break
   return result 


def isUChrList(val): 
   """Is val a list of unsigned integers in the range 0..255"""
   result = False 
   if type(val) == list: 
      result = True
      for b in val: 
         if not isinstance(b, int) or b < 0 or b > 255: 
            result = False 
            break 
   return result 


def uchrListToString(val): 
   assert isUChrList(val) 
   result = "".join( chr(b) for b in val)
   return result 


def asByteString(string_or_bytelist): 
   """Takes either a string or a list of bytes, and returns a string"""
   result = string_or_bytelist 
   if type(string_or_bytelist) == str: 
      result = string_or_bytelist
   elif isByteList(string_or_bytelist): 
      result = "".join(b for b in string_or_bytelist) 
   elif isUChrList(string_or_bytelist): 
      result = "".join( chr(b) for b in string_or_bytelist)
   else: 
      raise Exception("Invalid argument type %s for value: %r" % (type(string_or_bytelist), string_or_bytelist ) ) 
   return result 


def byteArrayToString(byte_iterable): 
   """Converts an iterable of bytes to a string."""
   result = None 
   if byte_iterable: 
      result = "".join( chr(k) for k in byte_iterable ) 
   return result 


def printableString(s): 
   """Given a string, replaces non-printable characters with '.'""" 
   # debug_string("util.printableString", 's', s)
   result = ""
   for ndx in range(0,len(s)): 
      ch = s[ndx] 
      result = result + ( ch if isPrintableChar(ch) else '.' ) 
   return result 
     

def asAsciiString(string_or_bytelist): 
   """Interprets a byte string as a sequence of Ascii characters.  
      The string may be right padded with \00's 
      There must be at least 1 non-null character at the start of the string. 
      If the input value has length 0, it is not a string 

      Returns None if not an Ascii string 
   """
   s0 = asByteString(string_or_bytelist) 
   # s1 = s0.rstrip('\0')
   ct = len(s0)
   if ct == 0:
      return None 

   result = ''
   for ch in s0: 
      if not isPrintableChar(ch): 
         result = None
         break
      result = result + ch 
   return result 
   
      
def hex_dump(byteseq, title=None): 
   """Outputs a hex data table"""

   if title is not None:
      print(title)
   print("Addr   0  1  2  3  4  5  6  7   8  9  A  B  C  D  E  F  Printable")
   print("--------------------------------------------------------------------------")    
   rowct =  ( len(byteseq) + 15 ) // 16
   for rowctr in range(0,rowct): 
      start = rowctr * 16 
      rowbytes = byteseq[start:start+16] 
      # startHex = "%4x" % start 
      # rowBytesHex = hexString(rowbytes) 
      spacedHex = ''
      printable = ''
      for ndx in range(0,16):
         if ndx < len(rowbytes): 
            byte = rowbytes[ndx] 
            if isinstance(byte, str):
               spacedHex += hexString(byte)  
            else:
               spacedHex += "%02x" % byte
            spacedHex += ' '

            if isinstance(byte, int):
               ch = chr(byte)
            else: 
               ch = byte
            printable += ( ch if isPrintableChar(ch) else '.' ) 
         else:
            spacedHex += '   ' 
            printable += ' '
         if ndx == 7: 
            spacedHex += ' '
            
      print("%04x  %s|%s|" % (start, spacedHex, printable)) 


# For a nice summary of the pros and cons of various ways to execute shell commands, 
# see http://stackoverflow.com/questions/89228/calling-an-external-command-in-python


def execute_shell_command(cmd, debug=False): 
   """Execute a shell command using os.popen(). 

      Arguments:  cmd    command string to execute
                  debug  indicates whether to output debug messages
      Returns:    pair consisting of: 
                     command return code  (0 for success)
                     string containing command output 
   """
   if debug: 
      print("(util.execute_shell_command) Executing command: |%s|" % cmd)
   fh = os.popen(cmd)
   text = fh.read() 
   rc = fh.close() 
   if rc is None: 
      rc = 0
   if debug: 
      print("(util.execute_shell_command) response text: |%s|" % text ) 
      print("(util.execute_shell_command) return code: %r" % rc ) 
   return (rc,text)


def execute_command(cmd, *args, **kwargs): 
   """Execute a shell command using subprocess.check_output(). 

      Arguments:  cmd    command string to execute
                  args   positional arguments 
      Keyword Arguments: 
                  debug  indicates whether to output debug messages
      Returns:    pair consisting of: 
                     command return code  (0 for success)
                     string containing command output 
   """
   debug = False       # default 
   for k in kwargs: 
      if k == 'debug': 
         debug = kwargs[k]
      else: 
         raise Exception("Invalid keyword: %s" % k)

   debug = True
   if debug: 
      args_list = ", ".join(args) 
      print( "(util.execute_command) Executing command: |%s|, Arguments: %s" % (cmd,args_list) ) 

   args2 = [cmd] 
   for a in args: 
      args2.append(a)
   try: 
      text = subprocess.check_output(args2, stderr=subprocess.STDOUT) 
      rc = 0
   except subprocess.CalledProcessError as cpe: 
      rc = cpe.returncode
      text = cpe.output
  
   if debug: 
      print("(util.execute_command) response text: |%s|" % text ) 
      print("(util.execute_command) return code: %r" % rc ) 
   return (rc,text)


def is_process_running(process_name): 
   result = False 
   for line in os.popen('ps xa'): 
      fields = line.split() 
      if len(fields) >= 5:      # have seen a case where line had no executable name 
         # pid = fields[0] 
         process = fields[4] 
         # print "(is_process_running) process=|%s|" % process
         if process.find(process_name) >= 0:
            result = True
            break
   # dbgmsg( "process_name = %s, returning %s" % (process_name, result) )
   return result 


def uniprint(s): 
   """Safely prints a string that may contain Unicode code points > 127. 

      This function addresses the problem that Python doesn't know how to print Unicode when its 
      standard output is not a terminal, e.g. when a Python program is called inside a Bash script 
      and the output is redirected to a file.  

      Arguments:    s  string to print 
   """
   # In case the input is already UTF-8: 
   if isinstance(s, unicode): 
      s = s.encode('utf-8') 
   print( s)


def debug_string(loc, msg, stringval):
   if isinstance(stringval, unicode): 
      status = "IS"
   else: 
      status = "IS NOT"
   if len(loc) > 0: 
      loc = "(%s) " % loc 
   s = "%s%s: |%s|" % (loc, msg, stringval)
   uniprint(s) 
   s = "%sValue %s Unicode" % (loc, status) 
   print(s)


def force_unicode(s): 
   if not isinstance(s, unicode): 
      try: 
         s = unicode(s) 
      except UnicodeDecodeError: 
         print( "(util.force_unicode) converting from utf-8: |%s|" % s )
         s = s.decode('utf-8')
   return s 


def which(name, flags=os.X_OK):
   """Search PATH for executable files with the given name.
   
   On newer versions of MS-Windows, the PATHEXT environment variable will be
   set to the list of file extensions for files considered executable. This
   will normally include things like ".EXE". This fuction will also find files
   with the given name ending with any of these extensions.
   
   On MS-Windows the only flag that has any meaning is os.F_OK. Any other
   flags will be ignored.
   
   @type name: C{str}
   @param name: The name for which to search.
   
   @type flags: C{int}
   @param flags: Arguments to L{os.access}.
   
   @rtype: C{list}
   @param: A list of the full paths to files found, in the
   order in which they were found.

   Copyright (c) 2001-2004 Twisted Matrix Laboratories.
   """
   result = []
   exts = filter(None, os.environ.get('PATHEXT', '').split(os.pathsep))
   path = os.environ.get('PATH', None)
   if path is None:
      return []
   for p in os.environ.get('PATH', '').split(os.pathsep):
      p = os.path.join(p, name)
      if os.access(p, flags):
         result.append(p)
      for e in exts:
         pext = p + e
         if os.access(pext, flags):
            result.append(pext)
   return result


def examineDirTree(dirname, nameTest, dirNameTest=None): 
   """Finds all files in a directory or its subdirectories whose name satisfies the function nameTest

      Arguments:  dirname  directory name 
                  nameTest function that takes a simple file name as its single argument
                           and returns True/False  
      Returns:    array of fully qualified filenames that satisfy nameTest 
   """
   # print "(examineDir) Examining directory: |%s|" % dirname
   goodNames = [] 

   if dirNameTest and not dirNameTest(dirname):
      return goodNames

   try: 
      files = os.listdir(dirname) 
      for fn in files: 
         fqfn = dirname + '/' + fn
         if os.path.isdir(fqfn):          # need to handle follow/ignore symbolic links 
           
            subdirfns = examineDirTree(fqfn, nameTest, dirNameTest)
            goodNames += subdirfns
         elif nameTest(fn): 
            # print "   Matched:", fn
            goodNames.append(fqfn) 
         else:
            # print "   Not Matched:", fn 
            pass 
   except OSError as e: 
      # print "Error: ", sys.exc_info()
      if e.errno == 2:      # No such file or directory 
         # print "(examinedir) Directory not found: " + dirname 
         pass
      elif e.errno == 13:   # Permission denied
         print( e.message)
         pass  
      else:
         raise 
   return goodNames


def examineDir(dirname, nameTest): 
   """Finds all files in a directory whose name satisfies the function nameTest

      Arguments:  dirname  directory name 
                  nameTest function that takes a simple file name as its single argument
                           and returns True/False  
      Returns:    array of simple filenames that satisfy nameTest 
   """
   # print "(examineDir) Examining directory: |%s|" % dirname
   goodNames = [] 
   try: 
      files = os.listdir(dirname) 
      for fn in files: 
         if nameTest(fn): 
            # print "   Matched:", fn
            goodNames.append(fn) 
         else:
            # print "   Not Matched:", fn 
            pass 
   except OSError as e: 
      # print "Error: ", sys.exc_info()
      if e.errno == 2:      # No such file or directory 
         # print "(examinedir) Directory not found: " + dirname 
         pass
      elif e.errno == 13:   # Permission denied
         print( e)
         pass  
      else:
         raise 
   return goodNames 



def collectFiles(dirs, testFunc): 
   """Examines all filenames in a list of directories, and applies the function testFunc to each simple file name. 
   
      Arguments:    dirs      list of directories 
                    testFunc  predicate function 
      Returns:      array of fully qualified file names for files whose simple name passes testFunc 
   """
   allFiles = [] 
   for d in dirs: 
      fns = examineDir(d, testFunc )      # ???
      for fn in fns: 
         allFiles.append( d + '/' + fn ) 
   # print "(collectFiles) returning", allFiles
   return allFiles 


def collectFilesFromTrees(dirs, testFunc): 
   """Examines all filenames in a list of directories and their subdirectories, and applies the function testFunc to each simple file name. 
   
      Arguments:    dirs      list of directories 
                    testFunc  predicate function 
      Returns:      array of fully qualified file names for files whose simple name passes testFunc 
   """
   allFiles = [] 
   for d in dirs: 
      fqfns = examineDirTree(d, testFunc )      # ???
      for fn in fqfns: 
         allFiles.append( fn ) 
   # print "(collectFiles) returning", allFiles
   return allFiles 


def quotify(s): 
   newval = '"' + s.replace('"', '""') + '"' 
   return newval 


def caller_function_name(depth=0): 
   """Returns the name of the function that called this function.  

   Enables a function to determine it's own name without hardcoding it. 
   """
   offset = depth+1
   frame = inspect.stack()[offset] 
   # print "(util.caller_function_name) caller frame = %s" % str(frame)
   funcname = frame[3] 
   # print "(util.caller_function_name) returning %s" % funcname
   return funcname



def caller_module_name(depth=0): 
   """Returns the name of the module that contains the function that called this function.  

   Enables a function to determine it's own module name without hardcoding it. 
   """
   offset = depth+1
   frame = inspect.stack()[offset]
   # print "(util.caller_module_name) caller frame = %s" % str(frame)
   # module_fn = inspect.stack()[1][1]
   module_fn = frame[1]
   simple_fn = os.path.basename(module_fn)
   if simple_fn.endswith(".py"):
      simple_fn = simple_fn[:-3]
   # print "(util.caller_module_name) module_fn = %s" % simple_fn
   return simple_fn



def caller_module_function_name(depth=0): 
   """Returns the name of the module and function that called this function, 
   in the form "module.function"

   Enables a function to determine it's own caller without hardcoding it. 
   """
   offset = depth+1
   frame = inspect.stack()[offset]
   # print "(util.caller_module_name) caller frame = %s" % str(frame)
   # module_fn = inspect.stack()[1][1]
   module_fn = frame[1]
   funcname  = frame[3] 
   simple_fn = os.path.basename(module_fn)
   if simple_fn.endswith(".py"):
      simple_fn = simple_fn[:-3]
   result = simple_fn + '.' + funcname
   # print "(util.caller_module_name) returning: %s" % result
   return result


def dbgmsg(msg, cls=None, showModule=True):
   loc = caller_function_name(1)
   if cls is None:
      if showModule:
        loc = "%s.%s" % (caller_module_name(1),loc)   
   else: 
      if inspect.isclass(cls):
         loc = "%s.%s" % (cls.__name__,loc)
         if showModule:
            # for giggles, use cls._module__ instead of caller_module_name() 
            module_name = cls.__module__
            # print "--- " + module_name[:-3:]
            # if module_name[-3:] == ".py":
            #    module_name = module_name[:-3]
            loc = "%s.%s" % (module_name,loc)
      else: 
         assert isinstance(cls,basestring)
         if len(cls) > 0:
            loc = "%s.%s" % (cls,loc)
            if showModule: 
               loc = "%s.%s" % (caller_module_name(1),loc)
   s0 = "(%s) %s" % (loc, msg)
   print(s0)



class TraceableClass(object): 

   @classmethod
   def simple_class_name(cls): 
      # clz = cls.__class__
      # clz = cls
      # print "__base__", clz.__base__
      # print "__class__", clz.__class__
      # print "__module__", clz.__module__
      # print "__mro__", clz.__mro__
      # print "__name__", clz.__name__

      # bad: 
      # print clz.__repr__() 
      # print clz.__str__() 
      # print "mro()", clz.mro() 


      scn = cls.__name__
      # print "(TraceableClass.simple_class_name) returning %s" % scn
      return scn


   @classmethod
   def cfn(cls):
      clsname  = cls.simple_class_name() 
      funcname = caller_function_name(1) 
      result = clsname + '.' + funcname 
      # print "(TraceableClass.cfn) Returning %s" % result
      return result



def whereami(): 
   print( "(util.whereami)" )
   print( inspect.stack()[1]  )
   print( inspect.currentframe() ) 



def var_len_or_none(var_name): 
   """Given a variable that that has a len() function (typically a collection), 
   returns a string reporting the variable's length or "None" if the value of the 
   variable is None.  For example: 

        len(my_var)=42 
        my_var is None 

   This function is intended to simplify coding of debugging statements. 

   Arguments: 
      var_name   variable name as a string 

   Returns:      string reporting variable value
   """
   frame = inspect.currentframe() 
   caller_locals = frame.f_back.f_locals
   del frame

   s1 = "'%s is None' if %s is None else 'len(%s)=" % (var_name, var_name, var_name)
   s2 = r"%d' % "
   s3 = "len(%s)" % var_name
   expr = s1 + s2 + s3
   # print "expr:", expr
   result =  eval(expr, {}, caller_locals)
   # print "(util.var_len_or_none) var_name='%s', returning: '%s'"  % (var_name, result) 
   return result



def compare_dictionaries(dict1, dict2, name1="Dictionary 1", name2="Dictionary 2", include_keys=None, exclude_keys=None): 
   """Debugging utility that reports the differences between two dictionaries

      Arguments: 
         dict1         first dictionary 
         dict2         second dictionary 
         name1         name of first dictionary (default='Dctionary 1')
         name2         name of second dictionary (default='Dictionary 2')
         include_keys  limit comparison to these keys
         exclude_keys  do not compare these keys

      Returns: 
         True if all key values match, False if not
   """

   print ("Comparing %s vs %s" % (name1, name2))
   # pprint(exclude_keys)
   keys1 = set(dict1.keys() )
   keys2 = set(dict2.keys() ) 
   if include_keys: 
      onlykeys = set(include_keys) 
      keys1 = keys1 & onlykeys
      keys2 = keys2 & onlykeys
   if exclude_keys: 
      excludes = set(exclude_keys) 
      # pprint(excludes)
      keys1 = keys1 - excludes
      keys2 = keys2 - excludes 
   # pprint(keys1)
   # pprint(keys2)
   both  = keys1 & keys2
   only1 = keys1 - keys2
   only2 = keys2 - keys1
   differences_found = False

   if len(only1) > 0: 
      differences_found = True
      print( "Keys only in %s: " % name1 )
      for k in sorted(list(only1)):
         print( "   ", k )

   if len(only2) > 0: 
      differences_found = True
      print( "Keys only in %s: " % name2 )
      for k in sorted(list(only2)):  
         print( "   ", k )

   for k in sorted(list(both)): 
      v1 = dict1[k] 
      v2 = dict2[k]

      if v1 != v2: 
         differences_found = True
         print( "Values for key %s do not match:" % k  ) 
         print( "  %s: %s" % (name1,v1) )
         print( "  %s: %s" % (name2,v2) ) 


   if differences_found: 
      print( "Dictionaries do not match!" )
   else:
      print( "Dictionaries match" )

   return not differences_found


def prompt_ok_to_proceed(debug=True): 
   result=None
   while result is None: 
      try: 
         response = raw_input( "Ok to proceed?: ")
      except KeyboardInterrupt:
         print                   # so system prompt starts on new line
         sys.exit()
      if response.upper() in ['YES', 'Y', 'OK']:
         result = True
      elif response.upper() in ['NO', 'N']:
         result = False
      else: 
         print( "Invalid response" )
         
   if debug:
      print( "(%s) Returning: %s" % (caller_module_function_name(), result) )
   return result 



### 
### Tests 
###

def test_conversions(): 
   print( "(test_conversions) Executing" )
   assert byteToHexString( b'\x00' ) == '00'
   assert byteToHexString( b'\x01' ) == '01'
   # assert byteToHexString( b'255'  ) == 'ff'
   assert byteToHexString( b'\xFF' ) == 'ff'


   assert hexString( '' )   == ''
   assert hexString( b'\x00')   == '00'
   assert hexString( b'\x01\x02')   == '0102'
   assert hexString( b'\x01\xFE')   == '01fe'


if __name__ == "__main__":
   test_conversions()