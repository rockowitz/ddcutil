# - Try to find Libddcutil
# Once done this will define
#
#  DDCUTIL_FOUND - system has DDCUtil
#  DDCUTIL_INCLUDE_DIR - the libddcutil include directory
#  DDCUTIL_LIBS - The libddcutil libraries

# Copyright (c) 2017, Dorian Vogel, <dorianvogel@gmail.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

find_package(PkgConfig)
pkg_check_modules(PC_LIBDDCUTIL QUIET ddcutil)
set(LIBDDCUTIL_DEFINITIONS ${PC_LIBDDCUTIL_CFLAGS_OTHER})

find_path(LIBDDCUTIL_INCLUDE_DIR ddcutil_c_api.h
          HINTS ${PC_LIBDDCUTIL_INCLUDEDIR} ${PC_LIBDDCUTIL_INCLUDE_DIRS})

find_library(LIBDDCUTIL_LIBRARY NAMES libddcutil.so
             HINTS ${PC_LIBDDCUTIL_LIBDIR} ${PC_LIBDDCUTIL_LIBRARY_DIRS} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBDDCUTIL_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(ddcutil  DEFAULT_MSG
                                  LIBDDCUTIL_LIBRARY LIBDDCUTIL_INCLUDE_DIR)

mark_as_advanced(LIBDDCUTIL_INCLUDE_DIR LIBDDCUTIL_LIBRARY )

set(LIBDDCUTIL_LIBRARIES ${LIBDDCUTIL_LIBRARY} )
set(LIBDDCUTIL_INCLUDE_DIRS ${LIBDDCUTIL_INCLUDE_DIR} )
