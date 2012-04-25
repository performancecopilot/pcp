
##############################################################################
#
# pcp.py
#
# Copyright (C) 2012 Red Hat Inc.
# Copyright 2009, Michael T. Werner
#
# This file is part of pcp, the python extensions for SGI's Performance
# Co-Pilot. Pcp is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Pcp is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
# more details. You should have received a copy of the GNU Lesser General
# Public License along with pcp. If not, see <http://www.gnu.org/licenses/>.
#

"""Wrapper module for libpcp - SGI's Performace Co-Pilot client API, aka PMAPI

Additional Information:

Performance Co-Pilot Web Site
http://oss.sgi.com/projects/pcp

Performance Co-Pilot Programmer's Guide
SGI Document 007-3434-005
http://techpubs.sgi.com
cf. Chapter 3. PMAPI - The Performance Metrics API
"""


##############################################################################
#
# imports
#

from ctypes import *

# needed for find_library()
from ctypes.util import *

# needed for mutex lock
import threading

# import constants from C header file <pcp/pmapi.h>
from pmapi import *

##############################################################################
#
# dynamic library loads
#

# helper func for platform independent loading of shared libraries
def loadLib( lib ):
    name = find_library( lib )
    try:
        handle = WinDLL( name )
    except NameError:
        pass
    handle = CDLL( name )
    return handle

# performance co-pilot pmapi library
libpcp = loadLib( "pcp" )

# libc is needed for calling free() 
libc = loadLib( "c" )


##############################################################################
#
# definition of structures used by C library libpcp, derived from <pcp/pmapi.h>
#
# This section defines the data structures for accessing and manuiplating
# metric information and values. Detailled information about these data
# structures can be found in:
#
# SGI Document: 007-3434-005
# Performance Co-Pilot Programmer's Guide
# Section 3.4 - Performance Metric Descriptions, pp. 59
# Section 3.5 - Performance Metric Values, pp. 62
#

# this hardcoded decl should be derived from <sys/time.h>
class timeval(Structure):
    _fields_ = [ ("tv_sec", c_long),
                 ("tv_usec", c_long) ]

class pmAtomValue(Union):
    """Union used for unpacking metric values according to type

    Constants for specifying metric types are defined in module pmapi
    """
    _fields_ = [ ("l", c_long),
                 ("ul", c_ulong),
                 ("ll", c_longlong),
                 ("ull", c_ulonglong),
                 ("f", c_float),
                 ("d", c_double),
                 ("cp", c_char_p),
                 ("vp", c_void_p) ]

try:

    if HAVE_BITFIELDS_LTOR:
        class pmUnits(Structure):
            """Irix bitfields specifying scale and dimentsion of metric values

            Constants for specifying metric units are defined in module pmapi
            """
            _fields_ = [ ("dimSpace", c_int, 4),
                         ("dimTime", c_int, 4),
                         ("dimCount", c_int, 4),
                         ("scaleSpace", c_int, 4),
                         ("scaleTime", c_int, 4),
                         ("scaleCount", c_int, 4),
                         ("pad", c_int, 8) ]

        class pmValueBlock(Structure):
            """Value block bitfields for Irix systems

            A value block holds the value of an instance of a metric
            pointed to by the pmValue structure, when that value is
            to large (> 32 bits) to fit in the pmValue structure
            """
            _fields_ = [ ("vtype", c_uint, 8),
                         ("vlen", c_uint, 24),
                         ("vbuf", c_char * 1) ]

except NameError:

    class pmUnits(Structure):
        """Linux bitfields specifying scale and dimentsion of metric values
            
        Constants for specifying metric units are defined in module pmapi
        """
        _fields_ = [ ("pad", c_int, 8),
                     ("scaleCount", c_int, 4),
                     ("scaleTime", c_int, 4),
                     ("scaleSpace", c_int, 4),
                     ("dimCount", c_int, 4),
                     ("dimTime", c_int, 4),
                     ("dimSpace", c_int, 4) ]

    class pmValueBlock(Structure):
        """Value block bitfields for Linux systems

        A value block holds the value of an instance of a metric
        pointed to by the pmValue structure, when that value is
        to large (> 32 bits) to fit in the pmValue structure
        """
        _fields_ = [ ("vlen", c_uint, 24),
                     ("vtype", c_uint, 8),
                     ("vbuf", c_char * 1) ]

class valueDref(Union):
    """Union in pmValue for dereferencing the value of an instance of a metric

    For small items, e.g. a 32-bit number, the union contains the actual value
    For large items, e.g. a text string, the union points to a pmValueBlock
    """
    _fields_ = [ ("pval", POINTER(pmValueBlock)),
                 ("lval", c_int) ]
    def __str__(self):
        return "value=%#lx" % (self.lval)

class pmValue(Structure):
    """Structure holding the value of a metric instance
    """
    _fields_ = [ ("inst", c_int),
                  ("value", valueDref) ]
    def __str__(self):
        return "pmValue@%#lx inst=%d " % (addressof(self), self.inst) + str(self.value)
                   
class pmValueSet(Structure):
    """Structure holding a metric's list of instance values

    A performance metric may contain one or more instance values, one for each
    item that the metric concerns. For example, a metric measuring filesystem
    free space would contain one instance value for each filesystem that exists
    on the target machine. Whereas, a metric measuring free memory would have
    only one instance value, representing the total ammount of free memory on
    the target system.
    """
    _fields_ = [ ("pmid", c_uint),
                 ("numval", c_int),
                 ("valfmt", c_int),
                 ("vlist", pmValue * 1) ]
    def __str__(self):
        print "pmValueSet"
        return "pmValueSet@%#lx id=%#lx numval=%d valfmt=%d" % (addressof(self), self.pmid, self.numval, self.valfmt) + (str([" %s" % str(self.vlist[i]) for i in xrange(self.numval)])) if self.valfmt == 0 else ""
                   
def define_pmResult (size):
    class pmResult (Structure):
        """Structure returned by pmFetch, with a value set for each metric queried
           Build the type so the array extent can be dynamically set
        """
        _fields_ = [ ("timestamp", timeval),
                     ("numpmid", c_int),
                     # array N of pointer to pmValueSet
                     ("vset", (POINTER(pmValueSet)) * size) ]
        def __init__(self):
            self.numpmid = size
        def __str__(self):
            return "pmResult@%#lx id#=%d " % (addressof(self), self.numpmid) + str([" %s" % str(self.vset[i]) for i in xrange(self.numpmid)])

    return pmResult

class pmInDom(Structure):
    """Structure describing a metric's instances
    """
    _fields_ = [ ( "indom", c_uint ),
                 ( "num", c_int ),
                 ( "instlist", c_void_p ),
                 ( "namelist", c_void_p ) ]

class pmDesc(Structure):
    """Structure describing a metric
    """
    _fields_ = [ ("pmid", c_uint),
                 ("type", c_int),
                 ("indom", c_uint),
                 ("sem", c_int),
                 ("units", pmUnits) ]
    def __str__(self):
        return "pmDesc@%#lx id=%#lx type=%d" % (addressof(self), self.pmid, self.type)

class pmMetricSpec(Structure):
    """Structure describing a metric's specification
    """
    _fields_ = [ ("isarch", c_int),
                 ("source", c_char_p),
                 ("metric", c_int),
                 ("ninst", c_char_p),
                 ("inst",  POINTER(c_char_p)) ]

class pmLogLabel(Structure):
    """Label Record at the start of every log file - as exported above the PMAPI
    """
    _fields_ = [ ("magic", c_int),
                 ("pid_t", c_long),
                 ("start", timeval),
                 ("hostname", c_char * PM_LOG_MAXHOSTLEN),
                 ("tz", c_char * PM_TZ_MAXLEN) ]


class pmRecordHost(Structure):
    """state information between the recording session and the pmlogger
    """
    _fields_ = [ ("f_config", c_void_p),
                 ("fd_ipc", c_int),
                 ("logfile", c_char_p),
                 ("pid", c_long),
                 ("status", c_int) ]



##############################################################################
#
# PMAPI function prototypes
#

##
# PMAPI Name Space Services

libpcp.pmGetChildren.restype = c_int
libpcp.pmGetChildren.argtypes = [ c_char_p, POINTER(POINTER(c_char_p)) ]

libpcp.pmGetChildrenStatus.restype = c_int
libpcp.pmGetChildrenStatus.argtypes = [ c_char_p, POINTER(POINTER(c_char_p)), POINTER(POINTER(c_int)) ]

libpcp.pmGetPMNSLocation.restype = c_int
libpcp.pmGetPMNSLocation.argtypes = []

libpcp.pmLoadNameSpace.restype = c_int
libpcp.pmLoadNameSpace.argtypes = [ c_char_p ]

libpcp.pmLoadASCIINameSpace.restype = c_int
libpcp.pmLoadASCIINameSpace.argtypes = [ c_char_p, c_int ]

libpcp.pmLookupName.restype = c_int
libpcp.pmLookupName.argtypes = [ c_int, (c_char_p * 1), POINTER(c_uint) ]

libpcp.pmNameAll.restype = c_int
libpcp.pmNameAll.argtypes = [ c_int, POINTER(POINTER(c_char_p)) ]

libpcp.pmNameID.restype = c_int
libpcp.pmNameID.argtypes = [ c_int, POINTER(c_char_p) ]

traverseCB_type = CFUNCTYPE( None, c_char_p )
libpcp.pmTraversePMNS.restype = c_int
libpcp.pmTraversePMNS.argtypes = [ c_char_p, traverseCB_type ]

libpcp.pmUnloadNameSpace.restype = c_int
libpcp.pmUnloadNameSpace.argtypes = [ ]


##
# PMAPI Metrics Description Services

libpcp.pmLookupDesc.restype = c_int
libpcp.pmLookupDesc.argtypes = [ c_uint, POINTER(pmDesc) ]

libpcp.pmLookupInDomText.restype = c_int
libpcp.pmLookupInDomText.argtypes = [ c_uint, c_int, POINTER(c_char_p) ]

libpcp.pmLookupText.restype = c_int
libpcp.pmLookupText.argtypes = [ c_uint, c_int, POINTER(c_char_p) ]


##
# PMAPI Instance Domain Services

libpcp.pmGetInDom.restype = c_int
libpcp.pmGetInDom.argtypes = [
                  c_uint, POINTER(POINTER(c_int)), POINTER(POINTER(c_char_p)) ]

libpcp.pmLookupInDom.restype = c_int
libpcp.pmLookupInDom.argtypes = [ c_uint, c_char_p ]

libpcp.pmNameInDom.restype = c_int
libpcp.pmNameInDom.argtypes = [ c_uint, POINTER(c_char_p) ]


##
# PMAPI Context Services

libpcp.pmNewContext.restype = c_int
libpcp.pmNewContext.argtypes = [ c_int, c_char_p ]

libpcp.pmDestroyContext.restype = c_int
libpcp.pmDestroyContext.argtypes = [ c_int ]

libpcp.pmDupContext.restype = c_int
libpcp.pmDupContext.argtypes = [ ]

libpcp.pmUseContext.restype = c_int
libpcp.pmUseContext.argtypes = [ c_int ]

libpcp.pmWhichContext.restype = c_int
libpcp.pmWhichContext.argtypes = [ ]

libpcp.pmAddProfile.restype = c_int
libpcp.pmAddProfile.argtypes = [ c_uint, c_int, POINTER(c_int) ]

libpcp.pmDelProfile.restype = c_int
libpcp.pmDelProfile.argtypes = [ c_uint, c_int, POINTER(c_int) ]

libpcp.pmSetMode.restype = c_int
libpcp.pmSetMode.argtypes = [ c_int, timeval, c_int ]

libpcp.pmReconnectContext.restype = c_int
libpcp.pmReconnectContext.argtypes = [ c_int ]

libpcp.pmGetContextHostName.restype = c_char_p
libpcp.pmGetContextHostName.argtypes = [ c_int ]


##
# PMAPI Timezone Services

libpcp.pmNewContextZone.restype = c_int
libpcp.pmNewContextZone.argtypes = [ ]

libpcp.pmNewZone.restype = c_int
libpcp.pmNewZone.argtypes = [ c_char_p ]

libpcp.pmUseZone.restype = c_int
libpcp.pmUseZone.argtypes = [ c_int ]

libpcp.pmWhichZone.restype = c_int
libpcp.pmWhichZone.argtypes = [ POINTER(c_char_p) ]


##
# PMAPI Metrics Services

libpcp.pmFetch.restype = c_int
# libpcp.pmFetch.argtypes = [ c_int, POINTER(c_uint), POINTER(POINTER(pmResult)) ]

libpcp.pmFreeResult.restype = None
# libpcp.pmFreeResult.argtypes = [ POINTER(pmResult) ]

libpcp.pmStore.restype = c_int
# libpcp.pmStore.argtypes = [ POINTER(pmResult) ]


##
# PMAPI Record-Mode Services
# These entries are not in libpcp.so.3

# libpcp.pmRecordAddHost.restype = c_int
# libpcp.pmRecordAddHost.argtypes = [ c_char_p, c_int ]

# libpcp.pmRecordControl.restype = c_int
# libpcp.pmRecordControl.argtypes = [ pmRecordHost, c_int, c_char_p ]

# libpcp.pmRecordSetup.restype = c_int
# libpcp.pmRecordSetup.argtypes = [ c_char_p, c_char_p, c_int ]


##
# PMAPI Archive-Specific Services

libpcp.pmGetArchiveLabel.restype = c_int
libpcp.pmGetArchiveLabel.argtypes = [ pmLogLabel ]

libpcp.pmGetArchiveEnd.restype = c_int
libpcp.pmGetArchiveEnd.argtypes = [ timeval ]

libpcp.pmGetInDomArchive.restype = c_int
libpcp.pmGetInDomArchive.argtypes = [ pmInDom ]

libpcp.pmLookupInDomArchive.restype = c_int
libpcp.pmLookupInDomArchive.argtypes = [ pmInDom, c_char_p ]

libpcp.pmNameInDomArchive.restype = c_int
libpcp.pmNameInDomArchive.argtypes = [ pmInDom, c_int ]

libpcp.pmFetchArchive.restype = c_int
libpcp.pmFetchArchive.argtypes = [ ]


##
# PMAPI Time Control Services

#libpcp.pmCtime.restype = c_int
#libpcp.pmCtime.argtypes = [ ]

#libpcp.pmLocaltime.restype = c_int
#libpcp.pmLocaltime.argtypes = [ ]

#libpcp.pmParseTimeWindow.restype = c_int
#libpcp.pmParseTimeWindow.argtypes = [ ]

#libpcp.pmTimeConnect.restype = c_int
#libpcp.pmTimeConnect.argtypes = [ ]

#libpcp.pmTimeDisconnect.restype = c_int
#libpcp.pmTimeDisconnect.argtypes = [ ]

#libpcp.pmTimeGetPort.restype = c_int
#libpcp.pmTimeGetPort.argtypes = [ ]

#libpcp.pmTimeRecv.restype = c_int
#libpcp.pmTimeRecv.argtypes = [ ]

#libpcp.pmTimeSendAck.restype = c_int
#libpcp.pmTimeSendAck.argtypes = [ ]

#libpcp.pmTimeSendBounds.restype = c_int
#libpcp.pmTimeSendBounds.argtypes = [ ]

#libpcp.pmTimeSendMode.restype = c_int
#libpcp.pmTimeSendMode.argtypes = [ ]

#libpcp.pmTimeSendPosition.restype = c_int
#libpcp.pmTimeSendPosition.argtypes = [ ]

#libpcp.pmTimeSendTimezone.restype = c_int
#libpcp.pmTimeSendTimezone.argtypes = [ ]

#libpcp.pmTimeShowDialog.restype = c_int
#libpcp.pmTimeShowDialog.argtypes = [ ]

#libpcp.pmTimeGetStatePixmap.restype = c_int
#libpcp.pmTimeGetStatePixmap.argtypes = [ ]


##
# PMAPI Ancilliary Support Services


libpcp.pmGetConfig.restype = c_char_p
libpcp.pmGetConfig.argtypes = [ c_char_p ]

libpcp.pmErrStr.restype = c_char_p
libpcp.pmErrStr.argtypes = [ c_int ]

libpcp.pmExtractValue.restype = c_int
libpcp.pmExtractValue.argtypes = [
           c_int, POINTER(pmValue), c_int, POINTER(pmAtomValue), c_int  ]

libpcp.pmConvScale.restype = c_int
libpcp.pmConvScale.argtypes = [
           c_int, POINTER(pmAtomValue), POINTER(pmUnits),
           POINTER(pmAtomValue), POINTER(pmUnits)  ]

libpcp.pmUnitsStr.restype = c_char_p
libpcp.pmUnitsStr.argtypes = [ POINTER(pmUnits) ]

libpcp.pmIDStr.restype = c_char_p
libpcp.pmIDStr.argtypes = [ c_uint ]

libpcp.pmInDomStr.restype = c_char_p
libpcp.pmInDomStr.argtypes = [ c_uint ]

libpcp.pmTypeStr.restype = c_char_p
libpcp.pmTypeStr.argtypes = [ c_int ]

libpcp.pmAtomStr.restype = c_char_p
libpcp.pmAtomStr.argtypes = [ POINTER(pmAtomValue), c_int ]

libpcp.pmPrintValue.restype = None
libpcp.pmPrintValue.argtypes=[c_void_p, c_int, c_int, POINTER(pmValue), c_int]

libpcp.pmParseInterval.restype = c_int
libpcp.pmParseInterval.argtypes=[ c_char_p, timeval, POINTER(c_char_p)]

libpcp.pmParseMetricSpec.restype = c_int
libpcp.pmParseMetricSpec.argtypes=[ c_char_p, c_int, c_char_p, pmMetricSpec, POINTER(c_char_p)]

libpcp.pmflush.restype = c_int
libpcp.pmflush.argtypes=[ ]

libpcp.pmprintf.restype = c_int
libpcp.pmprintf.argtypes=[ c_char_p ]

libpcp.pmSortInstances.restype = c_int
# libpcp.pmSortInstances.argtypes = [ POINTER(pmResult) ]

ctypes.pythonapi.PyFile_AsFile.restype = ctypes.c_void_p
ctypes.pythonapi.PyFile_AsFile.argtypes = [ ctypes.py_object ]

# TBD
####
# pmNumberStr


def py_to_c_str_arr (py_arr, c_arr):
    if type(py_arr) != type(tuple()):
        c_arr[0] = c_char_p(py_arr)
    else:
        for i in xrange (len(py_arr)):
            c_arr[i] = c_char_p(py_arr[i])
    
def c_to_py_arr (c_arr, py_arr):
    for i in xrange (len(c_arr)):
        py_arr.append(c_arr[i])

def py_to_c_uint_arr (py_arr, c_arr):
    if type(py_arr) != type(tuple()) and type(py_arr) != type(list()):
        c_arr[0] = c_long(py_arr)
    else:
        print ('###',c_arr,py_arr[0])
        for i in xrange (len(py_arr)):
            c_arr[i] = c_uint(py_arr[i])
    
class pmErr(Exception):
    pass

##############################################################################
#
# class pmContext
#
# This class wraps the PMAPI library functions
#


class pmContext( object ):
    """Defines a metrics source context (e.g. host, archive, etc) to operate on

    This object defines a PMAPI context, and its methods wrap calls to PMAPI
    library functions. Detailled information about those C library functions
    can be found in the following document.

    SGI Document: 007-3434-005
    Performance Co-Pilot Programmer's Guide
    Section 3.7 - PMAPI Procedural Interface, pp. 67

    Detailed information about the underlying data structures can be found
    in the same document.

    Section 3.4 - Performance Metric Descriptions, pp. 59
    Section 3.5 - Performance Metric Values, pp. 62
    """

    ##
    # class attributes

    # Many PMAPI function operate on global state or static memory.
    # A global lock will be used for now.
    # Some finer grained locking of specific functions should be done later.
    # Also, the context should hold the lock instead of individual methods, so
    # that several methods might run concurrently while the context holds the
    # the pmapi lock.

    _pmapiLock = threading.Lock()

    # Many calls into the PMAPI execute within preestabished context, which 
    # defines a metric source - a target host or a metrics archive file. The
    # trouble is: the PMAPI shared library keeps track of the current context
    # in a single global state. That global context state must be changed
    # to operate on metrics from a different source. The last used context is
    # tracked in the following class attribute, to reduce the number of
    # context change calls made to the PMAPI

    _lastUsedContext = None

    ##
    # property read methods

    def _R_type( self ):
        return self._type
    def _R_target( self ):
        return self._target
    def _R_ctx( self ):
        return self._ctx

    ##
    # property definitions

    type   = property( _R_type, None, None, None )
    target = property( _R_target, None, None, None )
    ctx    = property( _R_ctx, None, None, None )

    ##
    # overloads

    def __init__( self, type=PM_CONTEXT_HOST, target="127.0.0.1" ):
        self._type = type                                # the context type
        self._target = target                            # the context target
        pmContext._pmapiLock.acquire()
        self._ctx = libpcp.pmNewContext( type, target )  # the context handle
        if self._ctx < 0:
            pmContext._pmapiLock.release()
            raise (pmErr, self._ctx)
        pmContext._lastUsedContext = self
        pmContext._pmapiLock.release()

    def __del__(self):
        pmContext._pmapiLock.acquire()
        if libpcp:
            libpcp.pmDestroyContext( self.ctx )
        pmContext._lastUsedContext = None
        pmContext._pmapiLock.release()

    ##
    # PMAPI Name Space Services
    #

    def pmGetChildren( self, name ):
        """PMAPI - Lookup names of children of the given PMNS node
        """
        # this method is context dependent and requires the pmapi lock
        offspring = POINTER(c_char_p)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmGetChildren( name, byref( offspring ) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        if status > 0:
            childL = map( lambda x: str( offspring[x] ), range(status) )
            libc.free( offspring )
        else:
            return None
        return childL

    def pmGetChildrenStatus( self, name ):
        """PMAPI - Lookup names and status of children of the given metric
        """
        # this method is context dependent and requires the pmapi lock
        offspring = POINTER(c_char_p)()
        childstat = POINTER(c_int)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmGetChildren(name, byref(offspring), byref(childstat))
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        if status > 0:
            childL = map( lambda x: str( offspring[x] ), range(status) )
            statL = map( lambda x: int( childstat[x] ), range(status) )
            libc.free( offspring )
            libc.free( childstat )
        else:
            return None, None
        return childL, statL

    def pmGetPMNSLocation( self ):
        """PMAPI - Lookup the namespace location type
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmGetPMNSLocation( )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmLoadNameSpace( self, filename ):
        """PMAPI - Load a local namespace
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmLoadNameSpace( filename )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmLoadASCIINameSpace( self, filename, dupok ):
        """PMAPI - Load an ASCII formatted local namespace
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmLoadASCIINameSpace( filename, dupok )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmLookupName( self, nameA ):
        """PMAPI - Lookup pmIDs from a list of metric names
        """
        # this method is context dependent and requires the pmapi lock
        if type(nameA) == type(""):
            n = 1
        else:
            n = len( nameA )
        names = (c_char_p * n)()
        py_to_c_str_arr (nameA, names)

        pmidA = (c_uint * n)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        libpcp.pmLookupName.argtypes = [ c_int, (c_char_p * n), POINTER(c_uint) ]
        status = libpcp.pmLookupName( n, names, pmidA )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status, pmidA

    def pmNameAll( self, pmid ):
        """PMAPI - Lookup list of all metric names having this identical pmid
        """
        # this method is context dependent and requires the pmapi lock
        nameA_p = POINTER(c_char_p)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmNameAll( pmid, byref(nameA_p) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        nameL = map( lambda x: str( nameA_p[x] ), range( status ) )
        libc.free( nameA_p )
        return nameL

    def pmNameID( self, pmid ):
        """PMAPI - Lookup a metric name from a pmID
        """
        # this method is context dependent and requires the pmapi lock
        k = c_char_p()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmNameID( pmid, byref(k) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        name = str( k )
        libc.free( k )
        return name

    def pmTraversePMNS( self, name, callback ):
        """PMAPI - Scan namespace, depth first, run callback at each node
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        cb = traverseCB_type( callback )
        status = libpcp.pmTraversePMNS( name, cb )
        #status = libpcp.pmTraversePMNS( name, traverseCB_type( callback ) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmUnLoadNameSpace( self ):
        """PMAPI - Unloads a local PMNS, if one was previously loaded
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.UnloadNameSpace( )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    ##
    # PMAPI Metrics Description Services

    def pmLookupDesc( self, pmids_p ):
        """PMAPI - Lookup a metric description structure from a pmID
        """
        # this method is context dependent and requires the pmapi lock
        if type(pmids_p) == type(0):
            n = 1
        else:
            n = len( pmids_p)

        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        
        desc = (POINTER(pmDesc) * n)()

        for i in xrange(n):
            desc[i] = cast(create_string_buffer(sizeof(pmDesc)), POINTER(pmDesc))
            if type(pmids_p) == type(int()) or type(pmids_p) == type(long()):
                pmids = c_uint (pmids_p)
            else:
                pmids =  c_uint (pmids_p[i])

            status = libpcp.pmLookupDesc( pmids, desc[i])
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status, desc

    def pmLookupInDomText( self, indom, kind=PM_TEXT_ONELINE ):
        """PMAPI - Lookup the description of a metric's instance domain
        """
        # this method is context dependent and requires the pmapi lock
        buf = c_char_p()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmLookupInDomText( indom, kind, byref(buf) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        text = str( buf )
        libc.free( buf )
        return text

    def pmLookupText( self, pmid, kind=PM_TEXT_ONELINE ):
        """PMAPI - Lookup the description of a metric from its pmID
        """
        # this method is context dependent and requires the pmapi lock
        buf = c_char_p()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmLookupText( pmid, kind, byref(buf) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        text = str( buf )
        libc.free( buf )
        return text

    ##
    # PMAPI Instance Domain Services

    def pmGetInDom( self, indom ):
        """PMAPI - Lookup the list of instances from an instance domain
        """
        # this method is context dependent and requires the pmapi lock
        instA_p = POINTER(c_int)()
        nameA_p = POINTER(c_char_p)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmGetInDom( indom, byref(instA_p), byref(nameA_p) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        if status > 0:
            instL = [] ; nameL = []
            nameL = map( lambda x: str( nameA_p[x] ), range( status ) )
            instL = map( lambda x: int( instA_p[x] ), range( status ) )
            libc.free( instA_p ) ; libc.free( nameA_p )
        else:
            instL = None ; NameL = None
        return instL, nameL

    def pmLookupInDom( self, indom, name ):
        """PMAPI - Lookup the instance id with the given name in the indom
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmLookupInDom( indom, name )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmNameInDom( self, indom, instval ):
        """PMAPI - Lookup the text name of an instance in an instance domain
        """
        # this method is context dependent and requires the pmapi lock
        name_p = c_char_p()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmNameInDom( indom, instval, byref( name_p ) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        outName = str( name_p )
        libc.free( name_p )
        return outName

    ##
    # PMAPI Context Services

    def pmNewContext( self, type, name ):
        """PMAPI - NOOP - Establish a new PMAPI context (done in constructor)

        This is unimplemented. A new context is established when a pmContext
        object is created.
        """
        # this method is context dependent and requires the pmapi lock
        pass

    def pmDestroyContext( self, handle ):
        """PMAPI - NOOP - Destroy a PMAPI context (done in destructor)

        This is unimplemented. The context is destroyed when the pmContext
        object is destroyed.
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        pass

    def pmDupContext( self ):
        """PMAPI - TBD - Duplicate the current PMAPI Context
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmDupContext( )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmUseContext( self, handle ):
        """PMAPI - NOOP - Set the PMAPI context to that identified by handle

        This is unimplemented. Context changes are handled by the individual
        methods in a pmContext class instance.
        """
        # this method is context dependent and requires the pmapi lock
        pass

    def pmWhichContext( self ):
        """PMAPI - Returns the handle of the current PMAPI context
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        status = libpcp.pmWhichContext( )
        if status < 0:
            raise (pmErr, status)
        return status

    def pmAddProfile( self, indom, instL ):
        """PMAPI - add instances to list that will be collected from indom
        """
        # this method is context dependent and requires the pmapi lock
        if instL == None or len(instL) == 0:
            numinst = 0 ; instA = POINTER(c_int)()
        else:
            numinst = len( instL )
            instA = (c_int * numinst)()
            for index, value in enumerate( instL ):
                instA[index] = value
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmAddProfile( indom, numinst, instA )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmDelProfile( self, indom, instL ):
        """PMAPI - delete instances from list to be collected from indom
        """
        # this method is context dependent and requires the pmapi lock
        if instL == None or len(instL) == 0:
            numinst = 0 ; instA = POINTER(c_int)()
        else:
            numinst = len( instL )
            instA = (c_int * numinst)()
            for index, value in enumerate( instL ):
                instA[index] = value
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmDelProfile( indom, numinst, instA )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmSetMode( self, timeVal, delta ):
        """PMAPI - TBD - set interpolation mode for reading archive files
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmSetMode( byref(timeVal), delta )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmReconnectContext( self ):
        """PMAPI - Reestablish the context connection

        Unlike the underlying PMAPI function, this method takes no parameter.
        This method simply attempts to reestablish the the context belonging
        to its pmContext instance object.
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        status = libpcp.pmReconnectContext( self.ctx )
        if status < 0:
            raise (pmErr, status)
        return status

    def pmGetContextHostName( self ):
        """PMAPI - Lookup the hostname for the given context

        Unlike the underlying PMAPI function, this method takes no parameter.
        This method simply returns the name of the context belonging to its
        pmContext instance object.
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        status = libpcp.pmGetContextHostName( self.ctx )
        if status < 0:
            raise (pmErr, status)
        return status

    ##
    # PMAPI Timezone Services

    def pmNewContextZone( self ):
        """PMAPI - TBD - Query and set the current reporting timezone
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmNewContextZone( )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmNewZone( self, tz ):
        """PMAPI - TBD - Create new zone handle and set reporting timezone
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmNewContextZone( tz )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmUseZone( self, tz_handle ):
        """PMAPI - TBD - Sets the current reporting timezone
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmUseZone( tz_handle )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmWhichZone( self ):
        """PMAPI - TBD - Query the current reporting timezone
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        status = libpcp.pmGetContextHostName( self.ctx )
        if status < 0:
            raise (pmErr, status)
        return status


    ##
    # PMAPI Metrics Services

    def pmFetch( self, pmids ):
        """PMAPI - Fetch measurements from the target source
        """
        # this method is context dependent and requires the pmapi lock
        n = len(pmids)
        pmResult = define_pmResult(n)
        libpcp.pmFetch.argtypes = [ c_int, POINTER(c_uint), POINTER(POINTER(pmResult)) ]
        result_p = POINTER(pmResult)()
        libpcp.pmFetch.argtypes = [ c_int, POINTER(c_uint), POINTER(POINTER(pmResult)) ]
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmFetch( n, pmids, byref(result_p) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status, result_p

    def pmFreeResult( self, result_p ):
        """PMAPI - Free a result previously allocated by pmFetch
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        libpcp.pmFreeResult.argtypes = [ POINTER(type(result_p)) ]
        libpcp.pmFreeResult( result_p )

    def pmStore( self, result ):
        """PMAPI - TBD - Set values on target source, inverse of pmFetch
        """
        # this method is context dependent and requires the pmapi lock
        libpcp.pmStore.argtypes = [ POINTER(type(pmResult)) ]
        result_p = POINTER(type(result))()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmStore( byref(result_p) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status, result_p

    ##
    # PMAPI Record-Mode Services

    # def pmRecordAddHost( self, host, isdefault ):
    #     """PMAPI - TBD - Adds host to an archive recording session
    #     """
    #     # this method is context dependent and requires the pmapi lock
    #     result_p = POINTER(pmResult)()
    #     rhp = POINTER(pmRecordHost)()
    #     pmContext._pmapiLock.acquire()
    #     if not self == pmContext._lastUsedContext: 
    #         status = libpcp.pmUseContext( self.ctx )
    #         if status < 0:
    #             pmContext._pmapiLock.release()
    #             raise (pmErr, status)
    #         pmContext._lastUsedContext = self
    #     status = libpcp.pmRecordAddHost( host, isdefault, byref(rhp) )
    #     pmContext._pmapiLock.release()
    #     if status < 0:
    #         raise (pmErr, status)
    #     return status, result_p, rhp

    # def pmRecordControl( self, rhp, request, options ):
    #     """PMAPI - TBD - Control an archive recording session
    #     """
    #     # this method is context dependent and requires the pmapi lock
    #     pmContext._pmapiLock.acquire()
    #     if not self == pmContext._lastUsedContext: 
    #         status = libpcp.pmUseContext( self.ctx )
    #         if status < 0:
    #             pmContext._pmapiLock.release()
    #             raise (pmErr, status)
    #         pmContext._lastUsedContext = self
    #     status = libpcp.pmRecordControl( byref(rhp), request, options )
    #     pmContext._pmapiLock.release()
    #     if status < 0:
    #         raise (pmErr, status)
    #     return status

    # def pmRecordSetup( self, folio, creator, replay ):
    #     """PMAPI - TBD - Setup an archive recording sesion
    #     """
    #     # this method is context dependent and requires the pmapi lock
    #     pmContext._pmapiLock.acquire()
    #     if not self == pmContext._lastUsedContext: 
    #         status = libpcp.pmUseContext( self.ctx )
    #         if status < 0:
    #             pmContext._pmapiLock.release()
    #             raise (pmErr, status)
    #         pmContext._lastUsedContext = self
    #     status = libpcp.pmRecordControl( folio, creator, replay )
    #     pmContext._pmapiLock.release()
    #     if status < 0:
    #         raise (pmErr, status)
    #     return status

    ##
    # PMAPI Archive-Specific Services

    def pmGetArchiveLabel( self, loglabel ):
        """PMAPI - TBD - Get the label record from the archive
        """
        # this method is context dependent and requires the pmapi lock
        loglabel = POINTER(pmLogLabel)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmGetArchiveLabel ( loglabel )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status, loglabel
        

    def pmGetArchiveEnd( self ):
        """PMAPI - TBD - Get the last recorded timestamp from the archive
        """
        # this method is context dependent and requires the pmapi lock
        tvp = POINTER(timeval)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmGetArchiveEnd ( tvp )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status, tvp

    def pmGetInDomArchive( self, indom ):
        """PMAPI - TBD - Get the instance IDs and names for an instance domain
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        instlist = POINTER(c_int)()
        namelist = POINTER(c_int)()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmGetInDomArchive(indom, instlist, namelist )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status, instlist, namelist

    def pmLookupInDomArchive( self, indom, name ):
        """PMAPI - TBD - Get the instance ID for name and instance domain
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmLookupInDomArchive(indom, name )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmNameInDomArchive( self, indom, inst ):
        """PMAPI - TBD - Get the name for the given indom and inst ID
        """
        # this method is context dependent and requires the pmapi lock
        name_p = POINTER(c_char_p)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmNameInDomArchive(indom, inst, name_p )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status, name_p

    def pmFetchArchive( self ):
        """PMAPI - TBD - Fetch the next record from an archive log
        """
        # this method is context dependent and requires the pmapi lock
        result_p = POINTER(pmResult)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise (pmErr, status)
            pmContext._lastUsedContext = self
        status = libpcp.pmFetchArchive(result_p )
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status, result_p

    ##
    # PMAPI Time Control Services


    ##
    # PMAPI Ancilliary Support Services

    def pmGetConfig( self, variable ):
        """PMAPI - Return value from environment or pcp config file
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        x = str( libpcp.pmGetConfig( variable ) )
        return x

    def pmErrStr( self, code ):
        """PMAPI - Return value from environment or pcp config file
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        # this method uses a static buffer and requires an individual lock
        pmContext._pmapiLock.acquire()
        x = str( libpcp.pmErrStr( code ) )
        pmContext._pmapiLock.release()
        return x

    def pmExtractValue( self, value, desc, metric_idx, vlist_idx, outType):
        """PMAPI - Extract a value from a pmValue struct and convert its type
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        outAtom = pmAtomValue()
        for i in xrange(value.contents.numpmid):
            if (value.contents.vset[i].contents.pmid != metric_idx):
                continue
            code = libpcp.pmExtractValue (
                value.contents.vset[i].contents.valfmt,
                value.contents.vset[i].contents.vlist[vlist_idx],
                desc[i].contents.type,
                byref(outAtom),
                outType)
            if code < 0:
                raise (status)
            return code, outAtom


    def pmConvScale( self, inType, inAtom, inUnits, outUnits ):
        """PMAPI - Convert a value to a different scale
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        outAtom = pmAtomValue()
        status = libpcp.pmConvScale( inType, byref(inAtom),
                         byref(inUnits), byref(outAtom),
                         byref(outUnits) )
        if status < 0:
            raise (status)
        return outAtom

    def pmUnitsStr( self, units ):
        """PMAPI - Convert units struct to a readable string
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        # this method uses a static buffer and requires an individual lock
        pmContext._pmapiLock.acquire()
        x = str( libpcp.pmUnitsStr( units ) )
        pmContext._pmapiLock.release()
        return x

    def pmIDStr( self, pmid ):
        """PMAPI - Convert a pmID to a readable string
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        # this method uses a static buffer and requires an individual lock
        pmContext._pmapiLock.acquire()
        x = str( libpcp.pmIDStr( pmid ) )
        pmContext._pmapiLock.release()
        return x

    def pmInDomStr( self, indom ):
        """PMAPI - Convert an instance domain ID  to a readable string
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        # this method uses a static buffer and requires an individual lock
        pmContext._pmapiLock.acquire()
        x = str( libpcp.pmInDomStr( indom ) )
        pmContext._pmapiLock.release()
        return x

    def pmTypeStr( self, type ):
        """PMAPI - Convert a performance metric type to a readable string
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        # this method uses a static buffer and requires an individual lock
        pmContext._pmapiLock.acquire()
        x = str( libpcp.pmTypeStr( type ) )
        pmContext._pmapiLock.release()
        return x

    def pmAtomStr( self, atom, type ):
        """PMAPI - Convert a value atom to a readable string
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        # this method uses a static buffer and requires an individual lock
        pmContext._pmapiLock.acquire()
        x = str( libpcp.pmAtomStr( byref(atom), type ) )
        pmContext._pmapiLock.release()
        return x

    def pmNumberStr( self, value ):
        """PMAPI - NOOP - Convert a number to a string (not needed for python)
        """
        pass

    def pmPrintValue( self, fileObj, valfmt, type, value, minWidth ):
        """PMAPI - TBD - Print the value of a metric
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        fp = ctypes.pythonapi.PyFile_AsFile( fileObj )
        libpcp.pmPrintValue( fp, valfmt, type, value, minWidth )

    def pmflush( self ):
        """PMAPI - TBD - flush the internal buffer shared with pmprintf
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        # this method uses a static buffer and requires an individual lock
        # use of pmflush and pmprintf require lock held for consecutive calls
        status = libpcp.pmflush( ) 
        pmContext._pmapiLock.release()
        if status < 0:
            raise (pmErr, status)
        return status

    def pmprintf( self, format, *args ):
        """PMAPI - TBD - append message to internal buffer for later printing
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        # this method uses a static buffer and requires an individual lock
        # use of pmflush and pmprintf require lock held for consecutive calls
        status = pmContext._pmapiLock.acquire( blocking = 0 )
        if status == 0:
            raise (pmErr, status)
        libpcp.pmprintf( format, *args ) 

    def pmSortInstances( self, result_p ):
        """PMAPI - TBD - sort all metric instances in result returned by pmFetch
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        libpcp.pmSortInstances.argtypes = [ POINTER(type(result_p)) ]
        status = libpcp.pmSortInstances( result_p )
        if status < 0:
            raise (pmErr, status)
        return status

    def pmParseInterval( self, str, rslt ):
        """PMAPI - TBD - parse a textual time interval into a timeval struct
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        errmsg = POINTER(c_char_p)()
        status = libpcp.pmParseInterval( str, byref(rslt), errmsg )
        if status < 0:
            raise (pmErr, status)
        return status, errmsg

    def pmParseMetricSpec( self, string, isarch, source ):
        """PMAPI - TBD - parse a textual metric specification into a struct
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        rsltp = POINTER(pmMetricSpec)()
        errmsg = POINTER(c_char_p)         
        status = libpcp.pmParseMetricSpec( string, isarch, source, rsltp, errmsg)
        if status < 0:
            raise (pmErr, status)
        return status, errmsg



##############################################################################
#
# End of pcp.py
#
##############################################################################
