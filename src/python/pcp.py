#
# Copyright (C) 2012 Red Hat Inc.
# Copyright (C) 2009-2012 Michael T. Werner
#
# This file is part of the "pcp" module, the python interfaces for the
# Performance Co-Pilot toolkit.
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
#

"""Wrapper module for libpcp - Performace Co-Pilot client API, aka PMAPI

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

# needed for environment manipulation
import os

# constants adapted from C header file <pcp/pmapi.h>
import pmapi
from pmapi import *

import time

##############################################################################
#
# configuration tools
#

# Return a dict form of the key=value fields from pcp.conf
def pcp_conf():
    import shlex, string
    if ('PCP_DIR' in os.environ):
        file=os.environ['PCP_DIR']+'/etc/pcp.conf'
    else:
        file='/etc/pcp.conf'
    D={}
    for line in open(file):
        try:
            l = shlex.split(line, True)
            if (len(l) == 0): # comments
                continue
            (key,value) = string.split(l[0],'=')
            D[key]=value
            continue
        except ValueError:
            continue
    # XXX: cache D?
    return D

##############################################################################
#
# dynamic library loads
#

# helper func for platform independent loading of shared libraries
def loadLib( lib ):
    # Just in case this platform uses gcc to resolve ctypes libraries,
    # and those libraries are in some non-system directory, then
    # $LIBRARY_PATH is helpful to set.
    pc = pcp_conf()
    # XXX: don't append same path multiple times
    if ('PCP_LIB_DIR' in pc):
        try:
            os.environ['LIBRARY_PATH'] += ':' + pc['PCP_LIB_DIR']
        except KeyError:
            os.environ['LIBRARY_PATH'] = pc['PCP_LIB_DIR']

    name = find_library( lib )
    try:
        handle = WinDLL( name )
    except NameError:
        pass
    handle = CDLL( name )
    return handle

# Performance Co-Pilot PMAPI library (and friends)
libpcp = loadLib( "pcp" )
libpcp_gui = loadLib( "pcp_gui" )
libpcp_import = loadLib( "pcp_import" )

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
            errStr = ctypes.create_string_buffer(PM_MAXERRMSGLEN)
            errStr = libpcp.pmErrStr_r( errNum, errStr, PM_MAXERRMSGLEN )
        except KeyError:
            errSym = errStr = ""

        if self.args[0] == PM_ERR_NAME:
            pmidA = self.args[1]
            badL = self.args[2]
            return "%s %s: %s" % (errSym, errStr, badL)
        else:
            return "%s %s" % (errSym, errStr)

class pmiErr( Exception ):

    def __str__( self ):
        errNum = self.args[0]
        try:
            errSym = pmiErrSymD[ errNum ]
            errStr = ctypes.create_string_buffer(PMI_MAXERRMSGLEN)
            errStr = libpcp_import.pmiErrStr_r( errNum, errStr, PMI_MAXERRMSGLEN )
        except KeyError:
            errSym = errStr = ""
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
    _fields_ = [ ("l", c_int),
                 ("ul", c_uint),
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
	if self.valfmt == 0:
	    return "pmValueSet@%#lx id=%#lx numval=%d valfmt=%d" % (addressof(self), self.pmid, self.numval, self.valfmt) + (str([" %s" % str(self.vlist[i]) for i in xrange(self.numval)]))
	else:
	    return ""
                   
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

pmID = c_uint
pmInDom = c_uint

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
                 ("pid_t", c_int),
                 ("start", timeval),
                 ("hostname", c_char * PM_LOG_MAXHOSTLEN),
                 ("tz", c_char * PM_TZ_MAXLEN) ]


class pmRecordHost(Structure):
    """state information between the recording session and the pmlogger
    """
    _fields_ = [ ("f_config", c_void_p),
                 ("fd_ipc", c_int),
                 ("logfile", c_char_p),
                 ("pid", c_int),
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
# PMI Log Import Services

libpcp_import.pmiDump.restype = None
libpcp_import.pmiDump.argtypes = None

libpcp_import.pmiID.restype = pmID
libpcp_import.pmiID.argtypes = [ c_int, c_int, c_int ]

libpcp_import.pmiInDom.restype = pmInDom
libpcp_import.pmiInDom.argtypes = [ c_int, c_int ]

libpcp_import.pmiUnits.restype = pmUnits
libpcp_import.pmiUnits.argtypes = [ c_int, c_int, c_int, c_int, c_int, c_int ]

libpcp_import.pmiErrStr_r.restype = c_char_p
libpcp_import.pmiErrStr_r.argtypes = [ c_int, c_char_p, c_int ]

libpcp_import.pmiStart.restype = c_int
libpcp_import.pmiStart.argtypes = [ c_char_p, c_int ]

libpcp_import.pmiUseContext.restype = c_int
libpcp_import.pmiUseContext.argtypes = [ c_int ]

libpcp_import.pmiEnd.restype = c_int
libpcp_import.pmiEnd.argtypes = None

libpcp_import.pmiSetHostname.restype = c_int
libpcp_import.pmiSetHostname.argtypes = [ c_char_p ]

libpcp_import.pmiSetTimezone.restype = c_int
libpcp_import.pmiSetTimezone.argtypes = [ c_char_p ]

libpcp_import.pmiAddMetric.restype = c_int
libpcp_import.pmiAddMetric.argtypes = [ c_char_p, pmID, c_int, pmInDom, c_int, pmUnits ]

libpcp_import.pmiAddInstance.restype = c_int
libpcp_import.pmiAddInstance.argtypes = [ pmInDom, c_char_p, c_int ]

libpcp_import.pmiPutValue.restype = c_int
libpcp_import.pmiPutValue.argtypes = [ c_char_p, c_char_p, c_char_p ]

libpcp_import.pmiGetHandle.restype = c_int
libpcp_import.pmiGetHandle.argtypes = [ c_char_p, c_char_p ]

libpcp_import.pmiPutValueHandle.restype = c_int
libpcp_import.pmiPutValueHandle.argtypes = [ c_int, c_char_p ]

libpcp_import.pmiWrite.restype = c_int
libpcp_import.pmiWrite.argtypes = [ c_int, c_int ]

libpcp_import.pmiPutResult.restype = c_int
libpcp_import.pmiPutResult.argtypes = [ POINTER(pmResult) ]


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
libpcp.pmErrStr_r.argtypes = [ c_int, c_char_p, c_int ]

libpcp.pmExtractValue.restype = c_int
libpcp.pmExtractValue.argtypes = [
           c_int, POINTER(pmValue), c_int, POINTER(pmAtomValue), c_int  ]

libpcp.pmConvScale.restype = c_int
libpcp.pmConvScale.argtypes = [
           c_int, POINTER(pmAtomValue), POINTER(pmUnits),
           POINTER(pmAtomValue), POINTER(pmUnits)  ]

libpcp.pmUnitsStr_r.restype = c_char_p
libpcp.pmUnitsStr_r.argtypes = [ POINTER(pmUnits), c_char_p, c_int ]

libpcp.pmIDStr_r.restype = c_char_p
libpcp.pmIDStr_r.argtypes = [ c_uint, c_char_p, c_int ]

libpcp.pmInDomStr_r.restype = c_char_p
libpcp.pmInDomStr_r.argtypes = [ c_uint, c_char_p, c_int ]

libpcp.pmTypeStr_r.restype = c_char_p
libpcp.pmTypeStr_r.argtypes = [ c_int, c_char_p, c_int ]

libpcp.pmAtomStr_r.restype = c_char_p
libpcp.pmAtomStr_r.argtypes = [ POINTER(pmAtomValue), c_int, c_char_p, c_int ]

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

libpcp.pmSortInstances.restype = None
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
        self._ctx = libpcp.pmNewContext( type, target )  # the context handle
        if self._ctx < 0:
            raise pmErr, self._ctx

    def __del__(self):
        if libpcp:
            libpcp.pmDestroyContext( self.ctx )

    ##
    # PMAPI Name Space Services
    #

    def pmGetChildren( self, name ):
        """PMAPI - Return names of children of the given PMNS node NAME
        tuple names = pmGetChildren("kernel")
        """
        offspring = POINTER(c_char_p)()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmGetChildren( name, byref( offspring ) )
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
        offspring = POINTER(c_char_p)()
        childstat = POINTER(c_int)()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmGetChildrenStatus (name, byref(offspring), byref(childstat))
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
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmGetPMNSLocation( )
        if status < 0:
            raise pmErr, status
        return status

    def pmLoadNameSpace( self, filename ):
        """PMAPI - Load a local namespace
        status = pmLoadNameSpace("filename")
        """
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmLoadNameSpace( filename )
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
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        libpcp.pmLookupName.argtypes = [ c_int, (c_char_p * n), POINTER(c_uint) ]
        status = libpcp.pmLookupName( n, names, pmidA )
        if status != n:
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
        nameA_p = POINTER(c_char_p)()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmNameAll( pmid, byref(nameA_p) )
        if status < 0:
            raise pmErr, status
        nameL = map( lambda x: str( nameA_p[x] ), range( status ) )
        libc.free( nameA_p )
        return nameL

    def pmNameID( self, pmid ):
        """PMAPI - Return a metric name from a PMID
        name = pmNameID(self.metric_id)
        """
        k = c_char_p()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmNameID( pmid, byref(k) )
        if status < 0:
            raise pmErr, status
        name = k.value
        libc.free( k )
        return name

    def pmTraversePMNS( self, name, callback ):
        """PMAPI - Scan namespace, depth first, run CALLBACK at each node
        status = pmTraversePMNS("kernel", traverse_callback)
        """
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        cb = traverseCB_type( callback )
        status = libpcp.pmTraversePMNS( name, cb )
        if status < 0:
            raise pmErr, status
        return status

    def pmUnLoadNameSpace( self ):
        """PMAPI - Unloads a local PMNS, if one was previously loaded
        status = pm.pmUnLoadNameSpace("NameSpace")
        """
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmUnloadNameSpace( )
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
        if type(pmids_p) == type(int(0)) or type(pmids_p) == type(long(0)):
            n = 1
        else:
            n = len( pmids_p)

        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status

        desc = (POINTER(pmDesc) * n)()

        for i in xrange(n):
            desc[i] = cast(create_string_buffer(sizeof(pmDesc)), POINTER(pmDesc))
            if type(pmids_p) == type(int()) or type(pmids_p) == type(long()):
                   pmids = c_uint (pmids_p)
            else:
                pmids =  c_uint (pmids_p[i])

            status = libpcp.pmLookupDesc( pmids, desc[i])
            if status < 0:
                raise pmErr, status
        return status, desc

    def pmLookupInDomText( self, pmdesc, kind=PM_TEXT_ONELINE ):
        """PMAPI - Lookup the description of a metric's instance domain

        "instance" = pmLookupInDomText(pmDesc pmdesc)
        """
        buf = c_char_p()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status

        status = libpcp.pmLookupInDomText( get_indom (pmdesc), kind, byref(buf) )
        if status < 0:
            raise pmErr, status
        text = str( buf.value )
        libc.free( buf )
        return text

    def pmLookupText( self, pmid, kind=PM_TEXT_ONELINE ):
        """PMAPI - Lookup the description of a metric from its pmID
        "desc" = pmLookupText(pmid)
        """
        buf = c_char_p()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmLookupText( pmid, kind, byref(buf) )
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
        instA_p = POINTER(c_int)()
        nameA_p = POINTER(c_char_p)()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmGetInDom( get_indom (pmdescp), byref(instA_p), byref(nameA_p) )
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
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmLookupInDom( get_indom (pmdesc), name )
        if status < 0:
            raise pmErr, status
        return status

    def pmNameInDom( self, pmdesc, instval ):
        """PMAPI - Lookup the text name of an instance in an instance domain

        "string" = pmNameInDom(pmDesc pmdesc, c_uint instid)
        """
        if instval == PM_IN_NULL:
            return "PM_IN_NULL"
        name_p = c_char_p()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmNameInDom( get_indom (pmdesc), instval, byref( name_p ) )
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
        pass

    def pmDestroyContext( self, handle ):
        """PMAPI - NOOP - Destroy a PMAPI context (done in destructor)

        This is unimplemented. The context is destroyed when the pmContext
        object is destroyed.
        """
        pass

    def pmDupContext( self ):
        """PMAPI - Duplicate the current PMAPI Context

        This supports copying a pmContext object
        """
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmDupContext( )
        if status < 0:
            raise pmErr, status
        return status

    def pmUseContext( self, handle ):
        """PMAPI - NOOP - Set the PMAPI context to that identified by handle

        This is unimplemented. Context changes are handled by the individual
        methods in a pmContext class instance.
        """
        pass

    def pmWhichContext( self ):
        """PMAPI - Returns the handle of the current PMAPI context
        context = pmWhichContext()
        """
        status = libpcp.pmWhichContext( )
        if status < 0:
            raise pmErr, status
        return status

    def pmAddProfile( self, pmdesc, instL ):
        """PMAPI - add instances to list that will be collected from indom

        status = pmAddProfile(pmDesc pmdesc, c_uint instid)   
        """
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
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmAddProfile( get_indom(pmdesc), numinst, instA )
        if status < 0:
            raise pmErr, status
        return status

    def pmDelProfile( self, pmdesc, instL ):
        """PMAPI - delete instances from list to be collected from indom 

        status = pmDelProfile(pmDesc pmdesc, c_uint inst)
        status = pmDelProfile(pmDesc pmdesc, [c_uint inst])
        """
        if instL == None or len(instL) == 0:
            numinst = 0 ; instA = POINTER(c_int)()
        else:
            numinst = len( instL )
            instA = (c_int * numinst)()
            for index, value in enumerate( instL ):
                instA[index] = value
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        apmDesc = pmDesc()
        status = libpcp.pmDelProfile( get_indom (pmdesc), numinst, instA )
        if status < 0:
            raise pmErr, status
        return status

    def pmSetMode( self, mode, timeVal, delta ):
        """PMAPI - set interpolation mode for reading archive files
        code = pmSetMode (pmapi.PM_MODE_INTERP, timeval, 0)
        """
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmSetMode( mode, pointer(timeVal), delta )
        if status < 0:
            raise pmErr, status
        return status

    def pmReconnectContext( self ):
        """PMAPI - Reestablish the context connection

        Unlike the underlying PMAPI function, this method takes no parameter.
        This method simply attempts to reestablish the the context belonging
        to its pmContext instance object.
        """
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
        status = libpcp.pmGetContextHostName( self.ctx )
        if status < 0:
            raise pmErr, status
        return status

    ##
    # PMAPI Timezone Services

    def pmNewContextZone( self ):
        """PMAPI - Query and set the current reporting timezone
        """
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmNewContextZone( )
        if status < 0:
            raise pmErr, status
        return status

    def pmNewZone( self, tz ):
        """PMAPI - Create new zone handle and set reporting timezone
        """
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmNewContextZone( tz )
        if status < 0:
            raise pmErr, status
        return status

    def pmUseZone( self, tz_handle ):
        """PMAPI - Sets the current reporting timezone
        """
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmUseZone( tz_handle )
        if status < 0:
            raise pmErr, status
        return status

    def pmWhichZone( self ):
        """PMAPI - Query the current reporting timezone
        """
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
        result_p = POINTER(pmResult)()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmFetch( len(pmidA), pmidA, byref(result_p) )
        if status < 0:
            raise pmErr, status
        return status, result_p

    def pmFreeResult( self, result_p ):
        """PMAPI - Free a result previously allocated by pmFetch
        pmFreeResult(pmResult* pmresult)
        """
        libpcp.pmFreeResult( result_p )

    def pmStore( self, result ):
        """PMAPI - Set values on target source, inverse of pmFetch
        code = pmStore(pmResult* pmresult)
        """
        libpcp.pmStore.argtypes = [ (type(result)) ]
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmStore( result )
        if status < 0:
            raise pmErr, status
        return status, result

    ##
    # PMAPI Record-Mode Services

    def pmRecordSetup( self, folio, creator, replay ):
        """PMAPI - Setup an archive recording session
        File* file = pmRecordSetup("folio", "creator", 0)
        """
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        file_result = libpcp_gui.pmRecordSetup ( c_char_p(folio), c_char_p(creator), replay )
        if (file_result == 0):
            raise pmErr, file_result
        return file_result

    def pmRecordAddHost( self, host, isdefault, config ):
        """PMAPI - Adds host to an archive recording session
        (status, pmRecordHost* pmrecordhost) = pmRecordAddHost("host", 1, "configuration")
        """
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        rhp = POINTER(pmRecordHost)()
        status = libpcp_gui.pmRecordAddHost ( c_char_p(host), isdefault, byref(rhp) )
        if status < 0:
            raise pmErr, status
        status = libc.fputs (c_char_p(config), rhp.contents.f_config)
        if (status < 0):
            libc.perror(c_char_p(""))
            raise pmErr, status
        return status, rhp

    def pmRecordControl( self, rhp, request, options ):
        """PMAPI - Control an archive recording session
        status = pmRecordControl (0, pmapi.PM_RCSETARG, "args")
        status = pmRecordControl (0, pmapi.PM_REC_ON)
        status = pmRecordControl (0, pmapi.PM_REC_OFF)
        """
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp_gui.pmRecordControl ( cast(rhp,POINTER(pmRecordHost)), request, c_char_p(options) )
        if status < 0 and status != pmapi.PM_ERR_IPC:
            raise pmErr, status
        return status

    ##
    # PMAPI Archive-Specific Services

    def pmGetArchiveLabel( self, loglabel ):
        """PMAPI - Get the label record from the archive
        (status, loglabel) = pmGetArchiveLabel()
        """
        loglabel = pmLogLabel()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmGetArchiveLabel ( byref(loglabel) )
        if status < 0:
            raise pmErr, status
        return status, loglabel
    
    def pmGetArchiveEnd( self ):
        """PMAPI - Get the last recorded timestamp from the archive
        """
        tvp = POINTER(timeval)()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmGetArchiveEnd ( tvp )
        if status < 0:
            raise pmErr, status
        return status, tvp

    def pmGetInDomArchive( self, pmdescp ):
        """PMAPI - Get the instance IDs and names for an instance domain

        ((instance1, instance2...) (name1, name2...)) pmGetInDom(pmDesc pmdesc)
        """
        instA_p = POINTER(c_int)()
        nameA_p = POINTER(c_char_p)()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmGetInDomArchive( get_indom (pmdescp), byref(instA_p), byref(nameA_p) )
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
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmLookupInDomArchive(get_indom (pmdesc), name )
        if status < 0:
            raise pmErr, status
        return status

    def pmNameInDomArchive( self, pmdesc, inst ):
        """PMAPI - Lookup the text name of an instance in an instance domain

        "string" = pmNameInDomArchive(pmDesc pmdesc, c_uint instid)
        """
        name_p = c_char_p()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmNameInDomArchive(get_indom (pmdesc), inst, byref(name_p) )
        if status < 0:
            raise pmErr, status
        outName = str( name_p.value )
        libc.free( name_p )
        return outName

    def pmFetchArchive( self ):
        """PMAPI - Fetch measurements from the target source

        (status, pmResult* pmresult) = pmFetch ()
        """
        result_p = POINTER(pmResult)()
        status = libpcp.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = libpcp.pmFetchArchive(byref(result_p) )
        if status < 0:
            raise pmErr, status
        return status, result_p

    ##
    # PMAPI Time Control Services
    # (Not Yet Implemented)


    ##
    # PMAPI Ancilliary Support Services

    def pmGetConfig( self, variable ):
        """PMAPI - Return value from environment or pcp config file
        """
        x = str( libpcp.pmGetConfig( variable ) )
        return x

    def pmErrStr( self, code ):
        """PMAPI - Return value from environment or pcp config file
        """
        buffer = ctypes.create_string_buffer(PM_MAXERRMSGLEN)
        x = str( libpcp.pmErrStr_r( code, buffer, PM_MAXERRMSGLEN ) )
        return x

    def pmExtractValue( self, valfmt, vlist, intype, outtype ):
        """PMAPI - Extract a value from a pmValue struct and convert its type

        (status, pmAtomValue) = pmExtractValue(results.contents.get_valfmt(i),
        				       results.contents.get_vlist(i, 0),
                                               descs[i].contents.type,
                                               pmapi.PM_TYPE_FLOAT)
        """
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
        buffer = ctypes.create_string_buffer(64)
        x = str( libpcp.pmUnitsStr_r( units, buffer, 64 ) )
        return x

    def pmIDStr( self, pmid ):
        """PMAPI - Convert a pmID to a readable string

        pmIDStr(c_uint pmid)
        """
        buffer = ctypes.create_string_buffer(32)
        x = str( libpcp.pmIDStr_r( pmid, buffer, 32 ) )
        return x

    def pmInDomStr( self, pmdescp ):
        """PMAPI - Convert an instance domain ID  to a readable string

        "dom" =  pmGetInDom(pmDesc pmdesc)
        """
        buffer = ctypes.create_string_buffer(32)
        x = str( libpcp.pmInDomStr_r( get_indom (pmdescp), buffer, 32 ))
        return x

    def pmTypeStr( self, type ):
        """PMAPI - Convert a performance metric type to a readable string
        "type" = pmTypeStr (pmapi.PM_TYPE_FLOAT)
        """
        buffer = ctypes.create_string_buffer(32)
        x = str( libpcp.pmTypeStr_r( type, buffer, 32 ) )
        return x

    def pmAtomStr( self, atom, type ):
        """PMAPI - Convert a value atom to a readable string
        "value" = pmAtomStr (atom, pmapi.PM_TYPE_U32)
        """
        buffer = ctypes.create_string_buffer(96)
        x = str( libpcp.pmAtomStr( byref(atom), type, buffer, 96 ) )
        return x

    def pmPrintValue( self, fileObj, result, ptype, vset_idx, vlist_idx, minWidth):
        """PMAPI - Print the value of a metric
        """
        fp = ctypes.pythonapi.PyFile_AsFile( fileObj )
        libpcp.pmPrintValue (fp, c_int(result.contents.vset[vset_idx].contents.valfmt), c_int(ptype.contents.type), byref(result.contents.vset[vset_idx].contents.vlist[vlist_idx]), minWidth)

    def pmflush( self ):
        """PMAPI - flush the internal buffer shared with pmprintf
        """
        status = libpcp.pmflush( ) 
        if status < 0:
            raise pmErr, status
        return status

    def pmprintf( self, format, *args ):
        """PMAPI - append message to internal buffer for later printing
        """
        if status == 0:
            raise pmErr, status
        libpcp.pmprintf( format, *args ) 

    def pmSortInstances( self, result_p ):
        """PMAPI - sort all metric instances in result returned by pmFetch
        """
        libpcp.pmSortInstances.argtypes = [ (type(result_p)) ]
        libpcp.pmSortInstances( result_p )
        return None

    def pmParseInterval( self, str ):
        """PMAPI - parse a textual time interval into a timeval struct

        (status, timeval_ctype, "error message") = pmParseInterval ("time string")
        """
        tvp = timeval()
        errmsg = POINTER(c_char_p)()
        status = libpcp.pmParseInterval( str, byref(tvp), errmsg )
        if status < 0:
            raise pmErr, status
        return status, tvp, errmsg

    def pmParseMetricSpec( self, string, isarch, source ):
        """PMAPI - parse a textual metric specification into a struct
        (status,result,errormssg) = pmTypeStr ("kernel.all.load", 0, "localhost")
        """
        rsltp = POINTER(pmMetricSpec)()
        # errmsg = POINTER(c_char_p)         
        errmsg = c_char_p()
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
# class pmiLogImport
#
# This class wraps the PMI (Log Import) library functions
#

class pmiLogImport( object ):
    """Defines a PCP Log Import archive context
       This is used to create a PCP archive from an external source
    """

    ##
    # property read methods

    def _R_path( self ):
        return self._path
    def _R_ctx( self ):
        return self._ctx

    ##
    # property definitions

    path = property( _R_path, None, None, None )
    ctx = property( _R_ctx, None, None, None )

    ##
    # overloads

    def __init__( self, path="pcplog", inherit=0 ):
        self._path = path	# the archive path (file name)
        self._ctx = libpcp_import.pmiStart( c_char_p(path), inherit )
        if self._ctx < 0:
            raise pmiErr, self._ctx

    def __del__(self):
        if libpcp_import:
            libpcp_import.pmiUseContext( self._ctx )
            libpcp_import.pmiEnd()
        self._ctx = -1

    ##
    # PMI Log Import Services

    def pmiSetHostname( self, hostname ):
        """PMI - set the source host name for a Log Import archive
        """
        status = libpcp_import.pmiUseContext( self._ctx )
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiSetHostname( c_char_p(hostname) )
        if status < 0:
            raise pmiErr, status
        return status

    def pmiSetTimezone( self, timezone ):
        """PMI - set the source timezone for a Log Import archive
        """
        status = libpcp_import.pmiUseContext( self._ctx )
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiSetTimezone( c_char_p(timezone) )
        if status < 0:
            raise pmiErr, status
        return status

    def pmiID( self, domain, cluster, item ):
        """PMI - construct a pmID data structure (helper routine)
        """
        return libpcp_import.pmiID( domain, cluster, item )

    def pmiInDom( self, domain, serial ):
        """PMI - construct a pmInDom data structure (helper routine)
        """
        return libpcp_import.pmiInDom( domain, serial )

    def pmiUnits( self, dimSpace, dimTime, dimCount, scaleSpace, scaleTime, scaleCount ):
        """PMI - construct a pmUnits data structure (helper routine)
        """
        return libpcp_import.pmiUnits( dimSpace, dimTime, dimCount,
                                       scaleSpace, scaleTime, scaleCount )

    def pmiAddMetric( self, name, pmid, type, indom, sem, units ):
        """PMI - add a new metric definition to a Log Import context
        """
        status = libpcp_import.pmiUseContext( self._ctx )
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiAddMetric( c_char_p(name), pmid, type, indom, sem, units )
        if status < 0:
            raise pmiErr, status
        return status

    def pmiAddInstance( self, indom, instance, instid ):
        """PMI - add an element to an instance domain in a Log Import context
        """
        status = libpcp_import.pmiUseContext( self._ctx )
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiAddInstance( indom, c_char_p(instance), instid )
        if status < 0:
            raise pmiErr, status
        return status

    def pmiPutValue( self, name, inst, value ):
        """PMI - add a value for a metric-instance pair
        """
        status = libpcp_import.pmiUseContext( self._ctx )
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiPutValue( c_char_p(name), c_char_p(inst), c_char_p(value) )
        if status < 0:
            raise pmiErr, status
        return status

    def pmiGetHandle( self, name, inst ):
        """PMI - define a handle for a metric-instance pair
        """
        status = libpcp_import.pmiUseContext( self._ctx )
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiGetHandle( c_char_p(name), c_char_p(inst) )
        if status < 0:
            raise pmiErr, status
        return status

    def pmiPutValueHandle( self, handle, value ):
        """PMI - add a value for a metric-instance pair via a handle
        """
        status = libpcp_import.pmiUseContext( self._ctx )
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiPutValueHandle( handle, c_char_p(value) )
        if status < 0:
            raise pmiErr, status
        return status

    def pmiWrite( self, sec, usec ):
        """PMI - flush data to a Log Import archive
        """
        status = libpcp_import.pmiUseContext( self._ctx )
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiWrite( sec, usec )
        if status < 0:
            raise pmiErr, status
        return status

    def pmiPutResult( self, result ):
        """PMI - add a data record to a Log Import archive
        """
        status = libpcp_import.pmiUseContext( self._ctx )
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiPutResult( cast(result,POINTER(pmResult)) )
        if status < 0:
            raise pmiErr, status
        return status

    def pmiDump( self ):
        """PMI - dump the current Log Import contexts (diagnostic)
        """
        libpcp_import.pmiDump()

    def pmiEnd( self ):
        """PMI - close current context and finish a Log Import archive
        """
        status = libpcp_import.pmiUseContext( self._ctx )
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiEnd()
        self._ctx = -1
        if status < 0:
            raise pmiErr, status
        return status


