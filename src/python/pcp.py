
##############################################################################
#
# pcp.py
#
# Copyright (C) 2012 Red Hat Inc.
# Copyright (C) 2009-2012 Michael T. Werner
#
# This file is part of pcp, the python extensions for SGI's Performance
# Co-Pilot. 
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.

"""Wrapper module for libpcp - SGI's Performace Co-Pilot client API, aka PMAPI

Additional Information:

Performance Co-Pilot Web Site
http://oss.sgi.com/projects/pcp

Performance Co-Pilot Programmer's Guide
SGI Document 007-3434-005
http://techpubs.sgi.com
cf. Chapter 3. PMAPI - The Performance Metrics API

EXAMPLE

import pmapi
from pcp import *

# Create a pcp class
pm = pmContext(pmapi.PM_CONTEXT_HOST,"localhost")

# Get ids for number cpus and load metrics
(code, metric_ids) = pm.pmLookupName(("hinv.ncpu","kernel.all.load"))
# Get the description of the metrics
(code, descs) = pm.pmLookupDesc(metric_ids)
# Fetch the current value for number cpus
(code, results) = pm.pmFetch(metric_ids)
# Extract the value into a scalar value
(code, atom) = pm.pmExtractValue(results.contents.get_valfmt(0),
                                 results.contents.get_vlist(0, 0),
                                 descs[0].contents.type,
                                 pmapi.PM_TYPE_U32)
print "#cpus=",atom.ul

# Get the instance ids for kernel.all.load
inst1 = pm.pmLookupInDom(descs[1], "1 minute")
inst5 = pm.pmLookupInDom(descs[1], "5 minute")

# Loop through the metric ids
for i in xrange(results.contents.numpmid):
    # Is this the kernel.all.load id?
    if (results.contents.get_pmid(i) != metric_ids[1]):
        continue
    # Extrace the kernal.all.load instance
    for j in xrange(results.contents.get_numval(i) - 1):
        (code, atom) = pm.pmExtractValue(results.contents.get_valfmt(i),
                                         results.contents.get_vlist(i, j),
                                         descs[i].contents.type, pmapi.PM_TYPE_FLOAT)
        value = atom.f
        if results.contents.get_inst(i, j) == inst1:
            print "load average 1=",atom.f
        elif results.contents.get_inst(i, j) == inst5:
            print "load average 5=",atom.f
"""


##############################################################################
#
# imports
#

# for dereferencing timestamp in pmResult structure
import datetime

# for interfacing with libpcp - the client-side C API
import ctypes
from ctypes import *

# needed for find_library() - to load libpcp
from ctypes.util import *

# needed for mutex lock
import threading

# constants adapted from C header file <pcp/pmapi.h>
import pmapi
from pmapi import *

import time

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

libpcp_gui = loadLib( "pcp_gui" )

# libc is needed for calling free() 
libc = loadLib( "c" )


##############################################################################
#
# definition of exception classes
#

class pmErr( Exception ):

    def __str__( self ):
        errNum = self.args[0]
        try:
            errSym = pmErrSymD[ errNum ]
            errStr = libpcp.pmErrStr( errNum )
        except KeyError as e:
            errSym = errStr = ""

        if self.args[0] == PM_ERR_NAME:
            pmidA = self.args[1]
            badL = self.args[2]
            return "%s %s: %s" % (errSym, errStr, badL)
        else:
            return "%s %s" % (errSym, errStr)



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

    def __str__(self):
        tmp = datetime.date.fromtimestamp( self.tv_sec )
        return "%s.%06d" % (tmp, self.tv_usec)

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

    _atomDrefD = { PM_TYPE_32 : lambda x: x.l,
                  PM_TYPE_U32 : lambda x: x.ul,
                  PM_TYPE_64 : lambda x: x.ll,
                  PM_TYPE_U64 : lambda x: x.ull,
                  PM_TYPE_FLOAT : lambda x: x.f,
                  PM_TYPE_DOUBLE : lambda x: x.d,
                  PM_TYPE_STRING : lambda x: x.cp,
                  PM_TYPE_AGGREGATE : lambda x: None,
                  PM_TYPE_AGGREGATE_STATIC : lambda x: None,
                  PM_TYPE_NOSUPPORT : lambda x: None,
                  PM_TYPE_UNKNOWN : lambda x: None
            }


    def dref( self, type ):
        return self._atomDrefD[type]( self )   

class pmUnits(Structure):
    if HAVE_BITFIELDS_LTOR:
        """Irix bitfields specifying scale and dimension of metric values

        Constants for specifying metric units are defined in module pmapi
        """
        _fields_ = [ ("dimSpace", c_int, 4),
                     ("dimTime", c_int, 4),
                     ("dimCount", c_int, 4),
                     ("scaleSpace", c_int, 4),
                     ("scaleTime", c_int, 4),
                     ("scaleCount", c_int, 4),
                     ("pad", c_int, 8) ]
    else:
        """Linux bitfields specifying scale and dimension of metric values

        Constants for specifying metric units are defined in module pmapi
        """
        _fields_ = [ ("pad", c_int, 8),
                     ("scaleCount", c_int, 4),
                     ("scaleTime", c_int, 4),
                     ("scaleSpace", c_int, 4),
                     ("dimCount", c_int, 4),
                     ("dimTime", c_int, 4),
                     ("dimSpace", c_int, 4) ]

    def __str__( self ):
        return libpcp.pmUnitsStr( self )


class pmValueBlock(Structure):
    if HAVE_BITFIELDS_LTOR:
        """Value block bitfields for Irix systems

        A value block holds the value of an instance of a metric
        pointed to by the pmValue structure, when that value is
        too large (> 32 bits) to fit in the pmValue structure
        """
        _fields_ = [ ("vtype", c_uint, 8),
                     ("vlen", c_uint, 24),
                     ("vbuf", c_char * 1) ]
    else:
        """Value block bitfields for Linux systems

        A value block holds the value of an instance of a metric
        pointed to by the pmValue structure, when that value is
        too large (> 32 bits) to fit in the pmValue structure
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
    only one instance value, representing the total amount of free memory on
    the target system.
    """
    _fields_ = [ ("pmid", c_uint),
                 ("numval", c_int),
                 ("valfmt", c_int),
                 ("vlist", (pmValue * 1)) ]
    def __str__(self):
        return "pmValueSet@%#lx id=%#lx numval=%d valfmt=%d" % (addressof(self), self.pmid, self.numval, self.valfmt) + (str([" %s" % str(self.vlist[i]) for i in xrange(self.numval)])) if self.valfmt == 0 else ""
                   
    def vlist_read( self ):
        return pointer( self._vlist[0] )

    vlist = property( vlist_read, None, None, None )

pmValueSetPtr = POINTER(pmValueSet)
pmValueSetPtr.pmid   = property( lambda x: x.contents.pmid,   None, None, None )
pmValueSetPtr.numval = property( lambda x: x.contents.numval, None, None, None )
pmValueSetPtr.valfmt = property( lambda x: x.contents.valfmt, None, None, None )
pmValueSetPtr.vlist  = property( lambda x: x.contents.vlist,  None, None, None )

class pmResult(Structure):
    """Structure returned by pmFetch, with a value set for each metric queried

    The vset is defined with a "fake" array bounds of 1, which can give runtime
    array bounds complaints.  The getter methods are array bounds agnostic.
    """
    _fields_ = [ ("timestamp", timeval),
                 ("numpmid", c_int),
                 # array N of pointer to pmValueSet
                 ("vset", (POINTER(pmValueSet)) * 1) ]
    def __init__(self):
        self.numpmid = n
    def __str__(self):
        return "pmResult@%#lx id#=%d " % (addressof(self), self.numpmid) + str([" %s" % str(self.vset[i].contents) for i in xrange(self.numpmid)])
    def get_pmid(self, vset_idx):
        """ Return the pmid of vset[vset_idx]
        """
        return cast(self.vset,POINTER(POINTER(pmValueSet)))[vset_idx].contents.pmid
    def get_valfmt(self, vset_idx):
        """ Return the valfmt of vset[vset_idx]
        """
        return cast(self.vset,POINTER(POINTER(pmValueSet)))[vset_idx].contents.valfmt
    def get_numval(self, vset_idx):
        """ Return the numval of vset[vset_idx]
        """
        return cast(self.vset,POINTER(POINTER(pmValueSet)))[vset_idx].contents.numval
    def get_vlist(self, vset_idx, vlist_idx):
        """ Return the vlist[vlist_idx] of vset[vset_idx]
        """
        return cast(cast(self.vset,POINTER(POINTER(pmValueSet)))[vset_idx].contents.vlist,POINTER(pmValue))[vlist_idx]
    def get_inst(self, vset_idx, vlist_idx):
        """ Return the inst for vlist[vlist_idx] of vset[vset_idx]
        """
        return cast(cast(self.vset,POINTER(POINTER(pmValueSet)))[vset_idx].contents.vlist,POINTER(pmValue))[vlist_idx].inst

pmInDom = c_uint

# class pmInDom(Structure):
#     """Structure describing a metric's instances
#     """
#     _fields_ = [ ( "indom", c_uint ),
#                  ( "num", c_int ),
#                  ( "instlist", c_void_p ),
#                  ( "namelist", c_void_p ) ]

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

def get_indom( pmdesc ):
    """Internal function to extract an indom from a pmdesc

       Allow functions requiring an indom to be passed a pmDesc* instead
    """
    class Value(Union):
        _fields_ = [ ("pval", POINTER(pmDesc)),
                     ("lval", c_uint) ]
    if type (pmdesc) == POINTER(pmDesc):
        return pmdesc.contents.indom 
    else:           # raw indom
        # Goodness, there must be a simpler way to do this
        value = Value()
        value.pval = pmdesc
        return value.lval

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
libpcp.pmNameInDom.argtypes = [ c_uint, c_uint, POINTER(c_char_p) ]


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
libpcp.pmSetMode.argtypes = [ c_int, POINTER(timeval), c_int ]

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
libpcp.pmFetch.argtypes = [ c_int, POINTER(c_uint), POINTER(POINTER(pmResult)) ]

libpcp.pmFreeResult.restype = None
libpcp.pmFreeResult.argtypes = [ POINTER(pmResult) ]

libpcp.pmStore.restype = c_int
libpcp.pmStore.argtypes = [ POINTER(pmResult) ]


##
# PMAPI Record-Mode Services

libpcp_gui.pmRecordSetup.restype = c_long
libpcp_gui.pmRecordSetup.argtypes = [ c_char_p, c_char_p, c_int]

libpcp_gui.pmRecordAddHost.restype = c_int
libpcp_gui.pmRecordAddHost.argtypes = [ c_char_p, c_int, POINTER(POINTER(pmRecordHost))]

libpcp_gui.pmRecordControl.restype = c_int
libpcp_gui.pmRecordControl.argtypes = [ POINTER(pmRecordHost), c_int, c_char_p ]

##
# PMAPI Archive-Specific Services

libpcp.pmGetArchiveLabel.restype = c_int
libpcp.pmGetArchiveLabel.argtypes = [ POINTER(pmLogLabel) ]

libpcp.pmGetArchiveEnd.restype = c_int
libpcp.pmGetArchiveEnd.argtypes = [ timeval ]

libpcp.pmGetInDomArchive.restype = c_int
libpcp.pmGetInDomArchive.argtypes = [
                  c_uint, POINTER(POINTER(c_int)), POINTER(POINTER(c_char_p)) ]

libpcp.pmLookupInDomArchive.restype = c_int
libpcp.pmLookupInDom.argtypes = [ c_uint, c_char_p ]
libpcp.pmLookupInDomArchive.argtypes = [ pmInDom, c_char_p ]

libpcp.pmNameInDomArchive.restype = c_int
libpcp.pmNameInDomArchive.argtypes = [ pmInDom, c_int ]

libpcp.pmFetchArchive.restype = c_int
libpcp.pmFetchArchive.argtypes = [ POINTER(POINTER(pmResult)) ]


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

libpcp.pmErrStr_r.restype = c_char_p
libpcp.pmErrStr_r.argtypes = [ c_int ]

libpcp.pmExtractValue.restype = c_int
libpcp.pmExtractValue.argtypes = [
           c_int, POINTER(pmValue), c_int, POINTER(pmAtomValue), c_int  ]

libpcp.pmConvScale.restype = c_int
libpcp.pmConvScale.argtypes = [
           c_int, POINTER(pmAtomValue), POINTER(pmUnits),
           POINTER(pmAtomValue), POINTER(pmUnits)  ]

libpcp.pmUnitsStr_r.restype = c_char_p
libpcp.pmUnitsStr_r.argtypes = [ POINTER(pmUnits) ]

libpcp.pmIDStr_r.restype = c_char_p
libpcp.pmIDStr_r.argtypes = [ c_uint ]

libpcp.pmInDomStr_r.restype = c_char_p
libpcp.pmInDomStr_r.argtypes = [ c_uint ]

libpcp.pmTypeStr_r.restype = c_char_p
libpcp.pmTypeStr_r.argtypes = [ c_int ]

libpcp.pmAtomStr_r.restype = c_char_p
libpcp.pmAtomStr_r.argtypes = [ POINTER(pmAtomValue), c_int ]

libpcp.pmPrintValue.restype = None
libpcp.pmPrintValue.argtypes=[c_void_p, c_int, c_int, POINTER(pmValue), c_int]

libpcp.pmParseInterval.restype = c_int
libpcp.pmParseInterval.argtypes=[ c_char_p, POINTER(timeval), POINTER(c_char_p)]

libpcp.pmParseMetricSpec.restype = c_int
libpcp.pmParseMetricSpec.argtypes=[ c_char_p, c_int, c_char_p, POINTER(POINTER(pmMetricSpec)), POINTER(c_char_p)]

libpcp.pmflush.restype = c_int
libpcp.pmflush.argtypes=[ ]

libpcp.pmprintf.restype = c_int
libpcp.pmprintf.argtypes=[ c_char_p ]

libpcp.pmSortInstances.restype = c_int
# libpcp.pmSortInstances.argtypes = [ POINTER(pmResult) ]

libpcp.__pmtimevalSleep.restype = c_int
libpcp.__pmtimevalSleep.argtypes = [ POINTER(timeval) ]

ctypes.pythonapi.PyFile_AsFile.restype = ctypes.c_void_p
ctypes.pythonapi.PyFile_AsFile.argtypes = [ ctypes.py_object ]


##############################################################################
#
# class pmContext
#
# This class wraps the PMAPI library functions
#


class pmContext( object ):
    """Defines a metrics source context (e.g. host, archive, etc) to operate on

    pmContext(pmapi.PM_CONTEXT_HOST,"localhost")
    pmContext(pmapi.PM_CONTEXT_ARCHIVE,"FILENAME")

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

    def __init__( self, type=PM_CONTEXT_HOST, target="localhost" ):
        self._type = type                                # the context type
        self._target = target                            # the context target
        pmContext._pmapiLock.acquire()
        self._ctx = libpcp.pmNewContext( type, target )  # the context handle
        if self._ctx < 0:
            pmContext._pmapiLock.release()
            raise pmErr, self._ctx
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
        """PMAPI - Return names of children of the given PMNS node NAME
        tuple names = pmGetChildren("kernel")
        """
        # this method is context dependent and requires the pmapi lock
        offspring = POINTER(c_char_p)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmGetChildren( name, byref( offspring ) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        if status > 0:
            childL = map( lambda x: str( offspring[x] ), range(status) )
            libc.free( offspring )
        else:
            return None
        return childL

    def pmGetChildrenStatus( self, name ):
        """PMAPI - Return names and status of children of the given metric NAME
        (tuple names,tuple status) = pmGetChildrenStatus("kernel")
        """
        # this method is context dependent and requires the pmapi lock
        offspring = POINTER(c_char_p)()
        childstat = POINTER(c_int)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmGetChildrenStatus (name, byref(offspring), byref(childstat))
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        if status > 0:
            childL = map( lambda x: str( offspring[x] ), range(status) )
            statL = map( lambda x: int( childstat[x] ), range(status) )
            libc.free( offspring )
            libc.free( childstat )
        else:
            return None, None
        return childL, statL

    def pmGetPMNSLocation( self ):
        """PMAPI - Return the namespace location type
        loc = pmGetPMNSLocation()
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmGetPMNSLocation( )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status

    def pmLoadNameSpace( self, filename ):
        """PMAPI - Load a local namespace
        status = pmLoadNameSpace("filename")
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmLoadNameSpace( filename )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status

    def pmLookupName( self, nameA ):
        """PMAPI - Lookup pmIDs from a list of metric names nameA

        (status, c_uint pmid []) = pmidpmLookupName("MetricName") 
        (status, c_uint pmid []) = pmLookupName(("MetricName1" "MetricName2"...)) 
        """
        # this method is context dependent and requires the pmapi lock
        if type(nameA) == type(""):
            n = 1
        else:
            n = len( nameA )
        names = (c_char_p * n)()
        if type(nameA) == type(""):
            names[0] = c_char_p(nameA)
        else:
            for i in xrange (len(nameA)):
                names[i] = c_char_p(nameA[i])

        pmidA = (c_uint * n)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        libpcp.pmLookupName.argtypes = [ c_int, (c_char_p * n), POINTER(c_uint) ]
        status = libpcp.pmLookupName( n, names, pmidA )
        pmContext._pmapiLock.release()
        if status == PM_ERR_NAME:
            badL = [name for (name,pmid) in zip(nameA,pmidA) \
                                                if pmid == PM_ID_NULL]
            raise pmErr, (status, pmidA, badL )
        if status < 0:
            raise pmErr, (status, pmidA)
        return status, pmidA

    def pmNameAll( self, pmid ):
        """PMAPI - Return list of all metric names having this identical PMID
        tuple names = pmNameAll(metric_id)
        """
        # this method is context dependent and requires the pmapi lock
        nameA_p = POINTER(c_char_p)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmNameAll( pmid, byref(nameA_p) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        nameL = map( lambda x: str( nameA_p[x] ), range( status ) )
        libc.free( nameA_p )
        return nameL

    def pmNameID( self, pmid ):
        """PMAPI - Return a metric name from a PMID
        name = pmNameID(self.metric_id)
        """
        # this method is context dependent and requires the pmapi lock
        k = c_char_p()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmNameID( pmid, byref(k) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        name = k.value
        libc.free( k )
        return name

    def pmTraversePMNS( self, name, callback ):
        """PMAPI - Scan namespace, depth first, run CALLBACK at each node
        status = pmTraversePMNS("kernel", traverse_callback)
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        cb = traverseCB_type( callback )
        status = libpcp.pmTraversePMNS( name, cb )
        #status = libpcp.pmTraversePMNS( name, traverseCB_type( callback ) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status

    def pmUnLoadNameSpace( self ):
        """PMAPI - Unloads a local PMNS, if one was previously loaded
        status = pm.pmLoadNameSpace("NameSpace")
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmUnloadNameSpace( )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status

    ##
    # PMAPI Metrics Description Services

    def pmLookupDesc( self, pmids_p ):

        """PMAPI - Lookup a metric description structure from a pmID

        (status, (pmDesc* pmdesc)[]) = pmLookupDesc(c_uint pmid[N])
        (status, (pmDesc* pmdesc)[]) = pmLookupDesc(c_uint pmid)
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
                raise pmErr, status
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
            raise pmErr, status
        return status, desc

    def pmLookupInDomText( self, pmdesc, kind=PM_TEXT_ONELINE ):
        """PMAPI - Lookup the description of a metric's instance domain

        "instance" = pmLookupInDomText(pmDesc pmdesc)
        """
        # this method is context dependent and requires the pmapi lock
        buf = c_char_p()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
             
        status = libpcp.pmLookupInDomText( get_indom (pmdesc), kind, byref(buf) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        text = str( buf.value )
        libc.free( buf )
        return text

    def pmLookupText( self, pmid, kind=PM_TEXT_ONELINE ):
        """PMAPI - Lookup the description of a metric from its pmID
        "desc" = pmLookupText(pmid)
        """
        # this method is context dependent and requires the pmapi lock
        buf = c_char_p()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmLookupText( pmid, kind, byref(buf) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        text = buf.value
        libc.free( buf )
        return text

    ##
    # PMAPI Instance Domain Services

    def pmGetInDom( self, pmdescp ):
        """PMAPI - Lookup the list of instances from an instance domain PMDESCP

        ([instance1, instance2...] [name1, name2...]) pmGetInDom(pmDesc pmdesc)
        """
        # this method is context dependent and requires the pmapi lock
        instA_p = POINTER(c_int)()
        nameA_p = POINTER(c_char_p)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmGetInDom( get_indom (pmdescp), byref(instA_p), byref(nameA_p) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        if status > 0:
            instL = [] ; nameL = []
            nameL = map( lambda x: str( nameA_p[x] ), range( status ) )
            instL = map( lambda x: int( instA_p[x] ), range( status ) )
            libc.free( instA_p ) ; libc.free( nameA_p )
        else:
            instL = None ; NameL = None
        return instL, nameL

    def pmLookupInDom( self, pmdesc, name ):
        """PMAPI - Lookup the instance id with the given NAME in the indom

        c_uint instid = pmLookupInDom(pmDesc pmdesc, "Instance")   
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmLookupInDom( get_indom (pmdesc), name )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status

    def pmNameInDom( self, pmdesc, instval ):
        """PMAPI - Lookup the text name of an instance in an instance domain

        "string" = pmNameInDom(pmDesc pmdesc, c_uint instid)
        """
        # this method is context dependent and requires the pmapi lock
        #if c_uint(instval).value == PM_IN_NULL:
        if instval == PM_IN_NULL:
            return "PM_IN_NULL"
        name_p = c_char_p()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmNameInDom( get_indom (pmdesc), instval, byref( name_p ) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        outName = str( name_p.value )
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
        """PMAPI - Duplicate the current PMAPI Context

        This should be implemented to support copying a pmContext object
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmDupContext( )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
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
        context = pmWhichContext()
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        status = libpcp.pmWhichContext( )
        if status < 0:
            raise pmErr, status
        return status

    def pmAddProfile( self, pmdesc, instL ):
        """PMAPI - add instances to list that will be collected from indom

        status = pmAddProfile(pmDesc pmdesc, c_uint instid)   
        """
        # this method is context dependent and requires the pmapi lock
        if type(instL) == type(0):
            numinst = 1
            instA = (c_int * numinst)()
            instA[0] = instL
        elif instL == None or len(instL) == 0:
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
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmAddProfile( get_indom(pmdesc), numinst, instA )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status

    def pmDelProfile( self, pmdesc, instL ):
        """PMAPI - delete instances from list to be collected from indom 

        status = pmDelProfile(pmDesc pmdesc, c_uint inst)
        status = pmDelProfile(pmDesc pmdesc, [c_uint inst])
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
                raise pmErr, status
            pmContext._lastUsedContext = self
        apmDesc = pmDesc()
        status = libpcp.pmDelProfile( get_indom (pmdesc), numinst, instA )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status

    def pmSetMode( self, mode, timeVal, delta ):
        """PMAPI - set interpolation mode for reading archive files
        code = pmSetMode (pmapi.PM_MODE_INTERP, timeval, 0)
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmSetMode( mode, pointer(timeVal), delta )
        status = 0
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
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
            raise pmErr, status
        return status

    def pmGetContextHostName( self ):
        """PMAPI - Lookup the hostname for the given context

        Unlike the underlying PMAPI function, this method takes no parameter.
        This method simply returns the name of the context belonging to its
        pmContext instance object.

        "hostname" = pmGetContextHostName ()
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        status = libpcp.pmGetContextHostName( self.ctx )
        if status < 0:
            raise pmErr, status
        return status

    ##
    # PMAPI Timezone Services

    def pmNewContextZone( self ):
        """PMAPI - Query and set the current reporting timezone
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmNewContextZone( )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status

    def pmNewZone( self, tz ):
        """PMAPI - Create new zone handle and set reporting timezone
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmNewContextZone( tz )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status

    def pmUseZone( self, tz_handle ):
        """PMAPI - Sets the current reporting timezone
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmUseZone( tz_handle )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status

    def pmWhichZone( self ):
        """PMAPI - Query the current reporting timezone
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        status = libpcp.pmGetContextHostName( self.ctx )
        if status < 0:
            raise pmErr, status
        return status


    ##
    # PMAPI Metrics Services

    def pmFetch( self, pmidA ):
        """PMAPI - Fetch pmResult from the target source 

        (status, pmResult* pmresult) = pmFetch (c_uint pmid[])
        """
        # this method is context dependent and requires the pmapi lock
        result_p = POINTER(pmResult)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmFetch( len(pmidA), pmidA, byref(result_p) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status, result_p

    def pmFreeResult( self, result_p ):
        """PMAPI - Free a result previously allocated by pmFetch
        pmFreeResult(pmResult* pmresult)
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        libpcp.pmFreeResult( result_p )

    def pmStore( self, result ):
        """PMAPI - Set values on target source, inverse of pmFetch
        code = pmStore(pmResult* pmresult)
        """
        # this method is context dependent and requires the pmapi lock
        libpcp.pmStore.argtypes = [ (type(result)) ]
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmStore( result )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status, result

    ##
    # PMAPI Record-Mode Services

    def pmRecordSetup( self, folio, creator, replay ):
        """PMAPI - Setup an archive recording session
        File* file = pmRecordSetup("folio", "creator", 0)
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        file_result = libpcp_gui.pmRecordSetup ( c_char_p(folio), c_char_p(creator), replay )

        if (file_result == 0):
            raise pmErr, file_result
        pmContext._pmapiLock.release()
        return file_result

    def pmRecordAddHost( self, host, isdefault, config ):
        """PMAPI - Adds host to an archive recording session
        (status, pmRecordHost* pmrecordhost) = pmRecordAddHost("host", 1, "configuration")
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        rhp = POINTER(pmRecordHost)()
        status = libpcp_gui.pmRecordAddHost ( c_char_p(host), isdefault, byref(rhp) )
        if status < 0:
            raise pmErr, status
        status = libc.fputs (c_char_p(config), rhp.contents.f_config)
        if (status < 0):
            libc.perror(c_char_p(""))
            raise pmErr, status

        pmContext._pmapiLock.release()
        return status, rhp

    def pmRecordControl( self, rhp, request, options ):
        """PMAPI - Control an archive recording session
        status = pmRecordControl (0, pmapi.PM_RCSETARG, "args")
        status = pmRecordControl (0, pmapi.PM_REC_ON)
        status = pmRecordControl (0, pmapi.PM_REC_OFF)
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp_gui.pmRecordControl ( cast(rhp,POINTER(pmRecordHost)), request, c_char_p(options) )

        pmContext._pmapiLock.release()
        if status < 0 and status != pmapi.PM_ERR_IPC:
            raise pmErr, status
        return status

    ##
    # PMAPI Archive-Specific Services

    def pmGetArchiveLabel( self, loglabel ):
        """PMAPI - Get the label record from the archive
        (status, loglabel) = pmGetArchiveLabel()
        """
        # this method is context dependent and requires the pmapi lock
        loglabel = pmLogLabel()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmGetArchiveLabel ( byref(loglabel) )

        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status, loglabel
    

    def pmGetArchiveEnd( self ):
        """PMAPI - Get the last recorded timestamp from the archive
        """
        # this method is context dependent and requires the pmapi lock
        tvp = POINTER(timeval)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmGetArchiveEnd ( tvp )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status, tvp

    def pmGetInDomArchive( self, pmdescp ):
        """PMAPI - Get the instance IDs and names for an instance domain

        ((instance1, instance2...) (name1, name2...)) pmGetInDom(pmDesc pmdesc)
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        instA_p = POINTER(c_int)()
        nameA_p = POINTER(c_char_p)()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmGetInDomArchive( get_indom (pmdescp), byref(instA_p), byref(nameA_p) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        if status > 0:
            instL = [] ; nameL = []
            nameL = map( lambda x: str( nameA_p[x] ), range( status ) )
            instL = map( lambda x: int( instA_p[x] ), range( status ) )
            libc.free( instA_p ) ; libc.free( nameA_p )
        else:
            instL = None ; NameL = None
        return instL, nameL

    def pmLookupInDomArchive( self, pmdesc, name ):
        """PMAPI - Lookup the instance id with the given name in the indom

        c_uint instid = pmLookupInDomArchive(pmDesc pmdesc, "Instance")   
        """
        # this method is context dependent and requires the pmapi lock
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmLookupInDomArchive(get_indom (pmdesc), name )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status

    def pmNameInDomArchive( self, pmdesc, inst ):
        """PMAPI - Lookup the text name of an instance in an instance domain

        "string" = pmNameInDomArchive(pmDesc pmdesc, c_uint instid)
        """
        # this method is context dependent and requires the pmapi lock
        name_p = c_char_p()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmNameInDomArchive(get_indom (pmdesc), inst, byref(name_p) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        outName = str( name_p.value )
        libc.free( name_p )
        return outName

    def pmFetchArchive( self ):
        """PMAPI - Fetch measurements from the target source

        (status, pmResult* pmresult) = pmFetch ()
        """
        # this method is context dependent and requires the pmapi lock
        result_p = POINTER(pmResult)()
        pmContext._pmapiLock.acquire()
        if not self == pmContext._lastUsedContext: 
            status = libpcp.pmUseContext( self.ctx )
            if status < 0:
                pmContext._pmapiLock.release()
                raise pmErr, status
            pmContext._lastUsedContext = self
        status = libpcp.pmFetchArchive(byref(result_p) )
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
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
        pmContext._pmapiLock.acquire()
        x = str( libpcp.pmErrStr_r( code ) )
        pmContext._pmapiLock.release()
        return x

# int pmExtractValue(int valfmt, const pmValue *ival, int itype,
# pmAtomValue *oval, int otype)

    def pmExtractValue( self, valfmt, vlist, intype, outtype ):
        """PMAPI - Extract a value from a pmValue struct and convert its type

        (status, pmAtomValue) = pmExtractValue(results.contents.get_valfmt(i),
        				       results.contents.get_vlist(i, 0),
                                               descs[i].contents.type,
                                               pmapi.PM_TYPE_FLOAT)
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        outAtom = pmAtomValue()
        code = libpcp.pmExtractValue (valfmt, vlist, intype, byref(outAtom), outtype)
        if code < 0:
            raise pmErr (code)
        return code, outAtom


    def pmConvScale( self, inType, inAtom, desc, metric_idx, outUnits ):
        """PMAPI - Convert a value to a different scale

        (status, pmAtomValue) = pmConvScale(pmapi.PM_TYPE_FLOAT, pmAtomValue,
        				    pmDesc*, 3, pmapi.PM_SPACE_MBYTE)
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        outAtom = pmAtomValue()
        pmunits = pmUnits()
        pmunits.dimSpace = 1;
        pmunits.scaleSpace = outUnits
        status = libpcp.pmConvScale( inType, byref(inAtom),
                         byref(desc[metric_idx].contents.units), byref(outAtom),
                         byref(pmunits) )
        if status < 0:
            raise pmErr, status
        return status, outAtom

    def pmUnitsStr( self, units ):
        """PMAPI - Convert units struct to a readable string
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        pmContext._pmapiLock.acquire()
        x = str( libpcp.pmUnitsStr_r( units ) )
        pmContext._pmapiLock.release()
        return x

    def pmIDStr( self, pmid ):
        """PMAPI - Convert a pmID to a readable string

        pmIDStr(c_uint pmid)
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        pmContext._pmapiLock.acquire()
        x = str( libpcp.pmIDStr_r( pmid ) )
        pmContext._pmapiLock.release()
        return x

    def pmInDomStr( self, pmdescp ):
        """PMAPI - Convert an instance domain ID  to a readable string

        "dom" =  pmGetInDom(pmDesc pmdesc)
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        pmContext._pmapiLock.acquire()
        x = str( libpcp.pmInDomStr_r( get_indom (pmdescp) ))
        pmContext._pmapiLock.release()
        return x

    def pmTypeStr( self, type ):
        """PMAPI - Convert a performance metric type to a readable string
        "type" = pmTypeStr (pmapi.PM_TYPE_FLOAT)
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        pmContext._pmapiLock.acquire()
        x = str( libpcp.pmTypeStr_r( type ) )
        pmContext._pmapiLock.release()
        return x

    def pmAtomStr( self, atom, type ):
        """PMAPI - Convert a value atom to a readable string
        "value" = pmAtomStr (atom, pmapi.PM_TYPE_U32)
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        pmContext._pmapiLock.acquire()
        x = str( libpcp.pmAtomStr_r( byref(atom), type ) )
        pmContext._pmapiLock.release()
        return x

    def pmPrintValue( self, fileObj, result, type, vset_idx, vlist_idx, minWidth):
        """PMAPI - Print the value of a metric
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        fp = ctypes.pythonapi.PyFile_AsFile( fileObj )
        libpcp.pmPrintValue (fileObj, result.contents.vset[vset_idx].contents.valfmt, type, byref(result.contents.vset[vset_idx].contents.vlist[vlist_idx]), minWidth)

    def pmflush( self ):
        """PMAPI - flush the internal buffer shared with pmprintf
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        # this method uses a static buffer and requires an individual lock
        # use of pmflush and pmprintf require lock held for consecutive calls
        status = libpcp.pmflush( ) 
        pmContext._pmapiLock.release()
        if status < 0:
            raise pmErr, status
        return status

    def pmprintf( self, format, *args ):
        """PMAPI - append message to internal buffer for later printing
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        # this method uses a static buffer and requires an individual lock
        # use of pmflush and pmprintf require lock held for consecutive calls
        status = pmContext._pmapiLock.acquire()
        if status == 0:
            raise pmErr, status
        libpcp.pmprintf( format, *args ) 

    def pmSortInstances( self, result_p ):
        """PMAPI - sort all metric instances in result returned by pmFetch
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        libpcp.pmSortInstances.argtypes = [ (type(result_p)) ]
        status = libpcp.pmSortInstances( result_p )
        if status < 0:
            raise pmErr, status
        return status

    def pmParseInterval( self, str ):
        """PMAPI - parse a textual time interval into a timeval struct

        (status, timeval_ctype, "error message") = pmParseInterval ("time string")
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        tvp = timeval()
        errmsg = POINTER(c_char_p)()
        status = libpcp.pmParseInterval( str, byref(tvp), errmsg )
        if status < 0:
            raise pmErr, status
        return status, tvp, errmsg

    def pmParseMetricSpec( self, string, isarch, source ):
        """PMAPI - parse a textual metric specification into a struct
        """
        # this method is _not_ context dependent and requires _no_ pmapi lock
        rsltp = POINTER(pmMetricSpec)()
        # errmsg = POINTER(c_char_p)         
        errmsg = c_char_p()
        print string,isarch,source,rsltp
        status = libpcp.pmParseMetricSpec( string, isarch, source, byref(rsltp), byref(errmsg))
        if status < 0:
            raise pmErr, status
        return status, rsltp, errmsg

    def pmtimevalSleep( self, timeVal_p):
        # libpcp.__pmtimevalSleep(timeVal_p) 
        # doesn't dynamically link (leading underscore issue?)
#       libpcp.__pmtimevalSleep(timeVal_p)
        time.sleep(timeVal_p.tv_sec)


##############################################################################
#
# End of pcp.py
#
##############################################################################
