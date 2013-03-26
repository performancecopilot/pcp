# pylint: disable=C0103
""" Wrapper module for LIBPCP - the core Performace Co-Pilot API
#
# Copyright (C) 2012-2013 Red Hat
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

# Additional Information:
#   
# Performance Co-Pilot Web Site
# http://oss.sgi.com/projects/pcp
#   
# Performance Co-Pilot Programmer's Guide
# cf. Chapter 3. PMAPI - The Performance Metrics API
# 
# EXAMPLE
    
    from pcp import pmapi
    
    # Create a pcp class
    context = pmapi.pmContext(pmapi.PM_CONTEXT_HOST, "localhost")
    
    # Get ids for number cpus and load metrics
    (code, metric_ids) = context.pmLookupName(("hinv.ncpu","kernel.all.load"))
    # Get the description of the metrics
    (code, descs) = context.pmLookupDesc(metric_ids)
    # Fetch the current value for number cpus
    (code, results) = context.pmFetch(metric_ids)
    # Extract the value into a scalar value
    (code, atom) = context.pmExtractValue(
                                    results.contents.get_valfmt(0),
                                    results.contents.get_vlist(0, 0),
                                    descs[0].contents.type,
                                    pmapi.PM_TYPE_U32)
    print "#cpus=", atom.ul
    
    # Get the instance ids for kernel.all.load
    inst1 = context.pmLookupInDom(descs[1], "1 minute")
    inst5 = context.pmLookupInDom(descs[1], "5 minute")
    
    # Loop through the metric ids
    for i in xrange(results.contents.numpmid):
        # Is this the kernel.all.load id?
        if (results.contents.get_pmid(i) != metric_ids[1]):
            continue
        # Extrace the kernal.all.load instance
        for j in xrange(results.contents.get_numval(i) - 1):
            (code, atom) = context.pmExtractValue(
                                            results.contents.get_valfmt(i),
                                            results.contents.get_vlist(i, j),
                                            descs[i].contents.type,
                                            pmapi.PM_TYPE_FLOAT)
            value = atom.f
            if results.contents.get_inst(i, j) == inst1:
                print "load average 1=",atom.f
            elif results.contents.get_inst(i, j) == inst5:
                print "load average 5=",atom.f
"""


# for dereferencing timestamp in pmResult structure
import datetime

# constants adapted from C header file <pcp/pmapi.h>
import cpmapi as api

# for interfacing with LIBPCP - the client-side C API
import ctypes
from ctypes import c_char, c_int, c_uint, c_long, c_char_p, c_void_p
from ctypes import c_longlong, c_ulonglong, c_float, c_double
from ctypes import CDLL, POINTER, CFUNCTYPE, Structure, Union
from ctypes import addressof, pointer, sizeof, cast, byref
from ctypes import create_string_buffer
from ctypes.util import find_library

##############################################################################
#
# dynamic library loads
#

LIBPCP = CDLL(find_library("pcp"))
LIBC = CDLL(find_library("c"))


##############################################################################
#
# definition of exception classes
#

class pmErr(Exception):

    def __str__(self):
        errNum = self.args[0]
        try:
            errSym = api.pmErrSymDict[errNum]
            errStr = create_string_buffer(api.PM_MAXERRMSGLEN)
            errStr = LIBPCP.pmErrStr_r(errNum, errStr, api.PM_MAXERRMSGLEN)
        except KeyError:
            errSym = errStr = ""

        if self.args[0] == api.PM_ERR_NAME:
            pmidA = self.args[1]
            badL = self.args[2]
            return "%s %s: %s" % (errSym, errStr, badL)
        else:
            return "%s %s" % (errSym, errStr)


##############################################################################
#
# definition of structures used by C library LIBPCP, derived from <pcp/pmapi.h>
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

    _atomDrefD = {api.PM_TYPE_32 : lambda x: x.l,
                  api.PM_TYPE_U32 : lambda x: x.ul,
                  api.PM_TYPE_64 : lambda x: x.ll,
                  api.PM_TYPE_U64 : lambda x: x.ull,
                  api.PM_TYPE_FLOAT : lambda x: x.f,
                  api.PM_TYPE_DOUBLE : lambda x: x.d,
                  api.PM_TYPE_STRING : lambda x: x.cp,
                  api.PM_TYPE_AGGREGATE : lambda x: None,
                  api.PM_TYPE_AGGREGATE_STATIC : lambda x: None,
                  api.PM_TYPE_NOSUPPORT : lambda x: None,
                  api.PM_TYPE_UNKNOWN : lambda x: None
            }


    def dref(self, typed):
        return self._atomDrefD[typed](self)

class pmUnits(Structure):
    """
    Compiler-specific bitfields specifying scale and dimension of metric values
    Constants for specifying metric units are defined in module pmapi
    IRIX => HAVE_BITFIELDS_LTOR, gcc => not so much
    """
    if api.HAVE_BITFIELDS_LTOR:
        _fields_ = [ ("dimSpace", c_int, 4),
                     ("dimTime", c_int, 4),
                     ("dimCount", c_int, 4),
                     ("scaleSpace", c_int, 4),
                     ("scaleTime", c_int, 4),
                     ("scaleCount", c_int, 4),
                     ("pad", c_int, 8) ]
    else:
        _fields_ = [ ("pad", c_int, 8),
                     ("scaleCount", c_int, 4),
                     ("scaleTime", c_int, 4),
                     ("scaleSpace", c_int, 4),
                     ("dimCount", c_int, 4),
                     ("dimTime", c_int, 4),
                     ("dimSpace", c_int, 4) ]

    def __str__( self ):
        return LIBPCP.pmUnitsStr( self )


class pmValueBlock(Structure):
    """Value block bitfields for different compilers
       A value block holds the value of an instance of a metric
       pointed to by the pmValue structure, when that value is
       too large (> 32 bits) to fit in the pmValue structure
    """
    if api.HAVE_BITFIELDS_LTOR:   # IRIX
        _fields_ = [ ("vtype", c_uint, 8),
                     ("vlen", c_uint, 24),
                     ("vbuf", c_char * 1) ]
    else:   # Linux (gcc)
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
    """Structure holding the value of a metric instance """
    _fields_ = [ ("inst", c_int),
                  ("value", valueDref) ]
    def __str__(self):
        vstr = str(self.value)
        return "pmValue@%#lx inst=%d " % (addressof(self), self.inst) + vstr
                   
class pmValueSet(Structure):
    """Structure holding a metric's list of instance values

    A performance metric may contain one or more instance values, one for each
    item that the metric concerns. For example, a metric measuring filesystem
    free space would contain one instance value for each filesystem that exists
    on the target machine. Whereas, a metric measuring free memory would have
    only one instance value, representing the total amount of free memory on
    the target system.
    """
    _fields_ = [("pmid", c_uint),
                ("numval", c_int),
                ("valfmt", c_int),
                ("vlist", (pmValue * 1))]

    def __str__(self):
        if self.valfmt == 0:
            vals =  xrange(self.numval)
            vstr = str([" %s" % str(self.vlist[i]) for i in vals])
            vset = (addressof(self), self.pmid, self.numval, self.valfmt)
            return "pmValueSet@%#lx id=%#lx numval=%d valfmt=%d" % vset + vstr
        else:
            return ""
                   
    def vlist_read(self):
        return pointer(self._vlist[0])

    vlist = property(vlist_read, None, None, None)


pmValueSetPtr = POINTER(pmValueSet)
pmValueSetPtr.pmid   = property(lambda x: x.contents.pmid,   None, None, None)
pmValueSetPtr.numval = property(lambda x: x.contents.numval, None, None, None)
pmValueSetPtr.valfmt = property(lambda x: x.contents.valfmt, None, None, None)
pmValueSetPtr.vlist  = property(lambda x: x.contents.vlist,  None, None, None)


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
        Structure.__init__(self)
        self.numpmid = 0

    def __str__(self):
        vals = xrange(self.numpmid)
        vstr = str([" %s" % str(self.vset[i].contents) for i in vals])
        return "pmResult@%#lx id#=%d " % (addressof(self), self.numpmid) + vstr

    def get_pmid(self, vset_idx):
        """ Return the pmid of vset[vset_idx] """
        vsetptr = cast(self.vset, POINTER(POINTER(pmValueSet)))
        return vsetptr[vset_idx].contents.pmid

    def get_valfmt(self, vset_idx):
        """ Return the valfmt of vset[vset_idx] """
        vsetptr = cast(self.vset, POINTER(POINTER(pmValueSet)))
        return vsetptr[vset_idx].contents.valfmt

    def get_numval(self, vset_idx):
        """ Return the numval of vset[vset_idx] """
        vsetptr = cast(self.vset, POINTER(POINTER(pmValueSet)))
        return vsetptr[vset_idx].contents.numval

    def get_vlist(self, vset_idx, vlist_idx):
        """ Return the vlist[vlist_idx] of vset[vset_idx] """
        vsetptr = cast(self.vset, POINTER(POINTER(pmValueSet)))
        listptr = cast(vsetptr[vset_idx].contents.vlist, POINTER(pmValue))
        return listptr[vlist_idx]

    def get_inst(self, vset_idx, vlist_idx):
        """ Return the inst for vlist[vlist_idx] of vset[vset_idx] """
        return self.get_vlist(vset_idx, vlist_idx).inst

pmID = c_uint
pmInDom = c_uint

class pmDesc(Structure):
    """Structure describing a metric
    """
    _fields_ = [("pmid", c_uint),
                ("type", c_int),
                ("indom", c_uint),
                ("sem", c_int),
                ("units", pmUnits) ]
    def __str__(self):
        fields = (addressof(self), self.pmid, self.type)
        return "pmDesc@%#lx id=%#lx type=%d" % fields

def get_indom( pmdesc ):
    """Internal function to extract an indom from a pmdesc

       Allow functions requiring an indom to be passed a pmDesc* instead
    """
    class Value(Union):
        _fields_ = [ ("pval", POINTER(pmDesc)),
                     ("lval", c_uint) ]
    if type(pmdesc) == POINTER(pmDesc):
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
                 ("hostname", c_char * api.PM_LOG_MAXHOSTLEN),
                 ("tz", c_char * api.PM_TZ_MAXLEN) ]


##############################################################################
#
# PMAPI function prototypes
#

##
# PMAPI Name Space Services

LIBPCP.pmGetChildren.restype = c_int
LIBPCP.pmGetChildren.argtypes = [c_char_p, POINTER(POINTER(c_char_p))]

LIBPCP.pmGetChildrenStatus.restype = c_int
LIBPCP.pmGetChildrenStatus.argtypes = [
    c_char_p, POINTER(POINTER(c_char_p)), POINTER(POINTER(c_int))]

LIBPCP.pmGetPMNSLocation.restype = c_int
LIBPCP.pmGetPMNSLocation.argtypes = []

LIBPCP.pmLoadNameSpace.restype = c_int
LIBPCP.pmLoadNameSpace.argtypes = [c_char_p]

LIBPCP.pmLookupName.restype = c_int
LIBPCP.pmLookupName.argtypes = [c_int, (c_char_p * 1), POINTER(c_uint)]

LIBPCP.pmNameAll.restype = c_int
LIBPCP.pmNameAll.argtypes = [c_int, POINTER(POINTER(c_char_p))]

LIBPCP.pmNameID.restype = c_int
LIBPCP.pmNameID.argtypes = [c_int, POINTER(c_char_p)]

traverseCB_type = CFUNCTYPE(None, c_char_p)
LIBPCP.pmTraversePMNS.restype = c_int
LIBPCP.pmTraversePMNS.argtypes = [c_char_p, traverseCB_type]

LIBPCP.pmUnloadNameSpace.restype = c_int
LIBPCP.pmUnloadNameSpace.argtypes = []


##
# PMAPI Metrics Description Services

LIBPCP.pmLookupDesc.restype = c_int
LIBPCP.pmLookupDesc.argtypes = [c_uint, POINTER(pmDesc)]

LIBPCP.pmLookupInDomText.restype = c_int
LIBPCP.pmLookupInDomText.argtypes = [c_uint, c_int, POINTER(c_char_p)]

LIBPCP.pmLookupText.restype = c_int
LIBPCP.pmLookupText.argtypes = [c_uint, c_int, POINTER(c_char_p)]


##
# PMAPI Instance Domain Services

LIBPCP.pmGetInDom.restype = c_int
LIBPCP.pmGetInDom.argtypes = [
    c_uint, POINTER(POINTER(c_int)), POINTER(POINTER(c_char_p))]

LIBPCP.pmLookupInDom.restype = c_int
LIBPCP.pmLookupInDom.argtypes = [c_uint, c_char_p]

LIBPCP.pmNameInDom.restype = c_int
LIBPCP.pmNameInDom.argtypes = [c_uint, c_uint, POINTER(c_char_p)]


##
# PMAPI Context Services

LIBPCP.pmNewContext.restype = c_int
LIBPCP.pmNewContext.argtypes = [c_int, c_char_p]

LIBPCP.pmDestroyContext.restype = c_int
LIBPCP.pmDestroyContext.argtypes = [c_int]

LIBPCP.pmDupContext.restype = c_int
LIBPCP.pmDupContext.argtypes = []

LIBPCP.pmUseContext.restype = c_int
LIBPCP.pmUseContext.argtypes = [c_int]

LIBPCP.pmWhichContext.restype = c_int
LIBPCP.pmWhichContext.argtypes = []

LIBPCP.pmAddProfile.restype = c_int
LIBPCP.pmAddProfile.argtypes = [c_uint, c_int, POINTER(c_int)]

LIBPCP.pmDelProfile.restype = c_int
LIBPCP.pmDelProfile.argtypes = [c_uint, c_int, POINTER(c_int)]

LIBPCP.pmSetMode.restype = c_int
LIBPCP.pmSetMode.argtypes = [c_int, POINTER(timeval), c_int]

LIBPCP.pmReconnectContext.restype = c_int
LIBPCP.pmReconnectContext.argtypes = [c_int]

LIBPCP.pmGetContextHostName.restype = c_char_p
LIBPCP.pmGetContextHostName.argtypes = [c_int]


##
# PMAPI Timezone Services

LIBPCP.pmNewContextZone.restype = c_int
LIBPCP.pmNewContextZone.argtypes = []

LIBPCP.pmNewZone.restype = c_int
LIBPCP.pmNewZone.argtypes = [c_char_p]

LIBPCP.pmUseZone.restype = c_int
LIBPCP.pmUseZone.argtypes = [c_int]

LIBPCP.pmWhichZone.restype = c_int
LIBPCP.pmWhichZone.argtypes = [POINTER(c_char_p)]


##
# PMAPI Metrics Services

LIBPCP.pmFetch.restype = c_int
LIBPCP.pmFetch.argtypes = [c_int, POINTER(c_uint), POINTER(POINTER(pmResult))]

LIBPCP.pmFreeResult.restype = None
LIBPCP.pmFreeResult.argtypes = [POINTER(pmResult)]

LIBPCP.pmStore.restype = c_int
LIBPCP.pmStore.argtypes = [POINTER(pmResult)]


##
# PMAPI Archive-Specific Services

LIBPCP.pmGetArchiveLabel.restype = c_int
LIBPCP.pmGetArchiveLabel.argtypes = [POINTER(pmLogLabel)]

LIBPCP.pmGetArchiveEnd.restype = c_int
LIBPCP.pmGetArchiveEnd.argtypes = [timeval]

LIBPCP.pmGetInDomArchive.restype = c_int
LIBPCP.pmGetInDomArchive.argtypes = [
    c_uint, POINTER(POINTER(c_int)), POINTER(POINTER(c_char_p)) ]

LIBPCP.pmLookupInDomArchive.restype = c_int
LIBPCP.pmLookupInDom.argtypes = [c_uint, c_char_p]
LIBPCP.pmLookupInDomArchive.argtypes = [pmInDom, c_char_p]

LIBPCP.pmNameInDomArchive.restype = c_int
LIBPCP.pmNameInDomArchive.argtypes = [pmInDom, c_int]

LIBPCP.pmFetchArchive.restype = c_int
LIBPCP.pmFetchArchive.argtypes = [POINTER(POINTER(pmResult))]


##
# PMAPI Ancilliary Support Services


LIBPCP.pmGetConfig.restype = c_char_p
LIBPCP.pmGetConfig.argtypes = [c_char_p]

LIBPCP.pmErrStr_r.restype = c_char_p
LIBPCP.pmErrStr_r.argtypes = [c_int, c_char_p, c_int]

LIBPCP.pmExtractValue.restype = c_int
LIBPCP.pmExtractValue.argtypes = [
    c_int, POINTER(pmValue), c_int, POINTER(pmAtomValue), c_int  ]

LIBPCP.pmConvScale.restype = c_int
LIBPCP.pmConvScale.argtypes = [
    c_int, POINTER(pmAtomValue), POINTER(pmUnits), POINTER(pmAtomValue),
    POINTER(pmUnits)]

LIBPCP.pmUnitsStr_r.restype = c_char_p
LIBPCP.pmUnitsStr_r.argtypes = [POINTER(pmUnits), c_char_p, c_int]

LIBPCP.pmIDStr_r.restype = c_char_p
LIBPCP.pmIDStr_r.argtypes = [c_uint, c_char_p, c_int]

LIBPCP.pmInDomStr_r.restype = c_char_p
LIBPCP.pmInDomStr_r.argtypes = [c_uint, c_char_p, c_int]

LIBPCP.pmTypeStr_r.restype = c_char_p
LIBPCP.pmTypeStr_r.argtypes = [c_int, c_char_p, c_int]

LIBPCP.pmAtomStr_r.restype = c_char_p
LIBPCP.pmAtomStr_r.argtypes = [POINTER(pmAtomValue), c_int, c_char_p, c_int]

LIBPCP.pmPrintValue.restype = None
LIBPCP.pmPrintValue.argtypes = [c_void_p, c_int, c_int, POINTER(pmValue), c_int]

LIBPCP.pmParseInterval.restype = c_int
LIBPCP.pmParseInterval.argtypes = [c_char_p, POINTER(timeval),
    POINTER(c_char_p)]

LIBPCP.pmParseMetricSpec.restype = c_int
LIBPCP.pmParseMetricSpec.argtypes = [c_char_p, c_int, c_char_p,
    POINTER(POINTER(pmMetricSpec)), POINTER(c_char_p)]

LIBPCP.pmflush.restype = c_int
LIBPCP.pmflush.argtypes = []

LIBPCP.pmprintf.restype = c_int
LIBPCP.pmprintf.argtypes = [c_char_p]

LIBPCP.pmSortInstances.restype = None
# LIBPCP.pmSortInstances.argtypes = [POINTER(pmResult)]

ctypes.pythonapi.PyFile_AsFile.restype = ctypes.c_void_p
ctypes.pythonapi.PyFile_AsFile.argtypes = [ctypes.py_object]


##############################################################################
#
# class pmContext
#
# This class wraps the PMAPI library functions
#


class pmContext(object):
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

    def _R_type(self):
        return self._type
    def _R_target(self):
        return self._target
    def _R_ctx(self):
        return self._ctx

    ##
    # property definitions

    type   = property(_R_type, None, None, None)
    target = property(_R_target, None, None, None)
    ctx    = property(_R_ctx, None, None, None)

    ##
    # overloads

    def __init__(self, typed = api.PM_CONTEXT_HOST, target = "localhost"):
        self._type = typed                              # the context type
        self._target = target                            # the context target
        self._ctx = LIBPCP.pmNewContext(typed, target)    # the context handle
        if self._ctx < 0:
            raise pmErr, self._ctx

    def __del__(self):
        if LIBPCP:
            LIBPCP.pmDestroyContext(self.ctx)

    ##
    # PMAPI Name Space Services
    #

    def pmGetChildren(self, name):
        """PMAPI - Return names of children of the given PMNS node NAME
        tuple names = pmGetChildren("kernel")
        """
        offspring = POINTER(c_char_p)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmGetChildren(name, byref( offspring))
        if status < 0:
            raise pmErr, status
        if status > 0:
            childL = map(lambda x: str(offspring[x]), range(status))
            LIBC.free(offspring)
        else:
            return None
        return childL

    def pmGetChildrenStatus(self, name):
        """PMAPI - Return names and status of children of the given metric NAME
        (tuple names,tuple status) = pmGetChildrenStatus("kernel")
        """
        offspring = POINTER(c_char_p)()
        childstat = POINTER(c_int)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmGetChildrenStatus(name,
                        byref(offspring), byref(childstat))
        if status < 0:
            raise pmErr, status
        if status > 0:
            childL = map(lambda x: str(offspring[x]), range(status))
            statL = map(lambda x: int(childstat[x]), range(status))
            LIBC.free(offspring)
            LIBC.free(childstat)
        else:
            return None, None
        return childL, statL

    def pmGetPMNSLocation(self):
        """PMAPI - Return the namespace location type
        loc = pmGetPMNSLocation()
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmGetPMNSLocation()
        if status < 0:
            raise pmErr, status
        return status

    def pmLoadNameSpace(self, filename):
        """PMAPI - Load a local namespace
        status = pmLoadNameSpace("filename")
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmLoadNameSpace(filename)
        if status < 0:
            raise pmErr, status
        return status

    def pmLookupName(self, nameA):
        """PMAPI - Lookup pmIDs from a list of metric names nameA

        (status, c_uint pmid []) = pmidpmLookupName("MetricName") 
        (status, c_uint pmid []) = pmLookupName(("MetricName1" "MetricName2"...)) 
        """
        if type(nameA) == type(""):
            n = 1
        else:
            n = len(nameA)
        names = (c_char_p * n)()
        if type(nameA) == type(""):
            names[0] = c_char_p(nameA)
        else:
            for i in xrange(len(nameA)):
                names[i] = c_char_p(nameA[i])

        pmidA = (c_uint * n)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        LIBPCP.pmLookupName.argtypes = [c_int, (c_char_p * n), POINTER(c_uint)]
        status = LIBPCP.pmLookupName(n, names, pmidA)
        if status != n:
            badL = [name for (name, pmid) in zip(nameA, pmidA) \
                                                if pmid == api.PM_ID_NULL]
            raise pmErr, (status, pmidA, badL)
        if status < 0:
            raise pmErr, (status, pmidA)
        return status, pmidA

    def pmNameAll(self, pmid):
        """PMAPI - Return list of all metric names having this identical PMID
        tuple names = pmNameAll(metric_id)
        """
        nameA_p = POINTER(c_char_p)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmNameAll(pmid, byref(nameA_p))
        if status < 0:
            raise pmErr, status
        nameL = map(lambda x: str(nameA_p[x]), range(status))
        LIBC.free( nameA_p )
        return nameL

    def pmNameID(self, pmid):
        """PMAPI - Return a metric name from a PMID
        name = pmNameID(self.metric_id)
        """
        k = c_char_p()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmNameID( pmid, byref(k) )
        if status < 0:
            raise pmErr, status
        name = k.value
        LIBC.free( k )
        return name

    def pmTraversePMNS(self, name, callback):
        """PMAPI - Scan namespace, depth first, run CALLBACK at each node
        status = pmTraversePMNS("kernel", traverse_callback)
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        cb = traverseCB_type(callback)
        status = LIBPCP.pmTraversePMNS(name, cb)
        if status < 0:
            raise pmErr, status
        return status

    def pmUnLoadNameSpace(self):
        """PMAPI - Unloads a local PMNS, if one was previously loaded
        status = pm.pmUnLoadNameSpace("NameSpace")
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmUnloadNameSpace()
        if status < 0:
            raise pmErr, status
        return status

    ##
    # PMAPI Metrics Description Services

    def pmLookupDesc(self, pmids_p):

        """PMAPI - Lookup a metric description structure from a pmID

        (status, (pmDesc* pmdesc)[]) = pmLookupDesc(c_uint pmid[N])
        (status, (pmDesc* pmdesc)[]) = pmLookupDesc(c_uint pmid)
        """
        if type(pmids_p) == type(int(0)) or type(pmids_p) == type(long(0)):
            n = 1
        else:
            n = len(pmids_p)

        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status

        desc = (POINTER(pmDesc) * n)()

        for i in xrange(n):
            descbuf = create_string_buffer(sizeof(pmDesc))
            desc[i] = cast(descbuf, POINTER(pmDesc))
            if type(pmids_p) == type(int()) or type(pmids_p) == type(long()):
                pmids = c_uint(pmids_p)
            else:
                pmids = c_uint(pmids_p[i])

            status = LIBPCP.pmLookupDesc(pmids, desc[i])
            if status < 0:
                raise pmErr, status
        return status, desc

    def pmLookupInDomText(self, pmdesc, kind = api.PM_TEXT_ONELINE):
        """PMAPI - Lookup the description of a metric's instance domain

        "instance" = pmLookupInDomText(pmDesc pmdesc)
        """
        buf = c_char_p()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status

        status = LIBPCP.pmLookupInDomText(get_indom(pmdesc), kind, byref(buf))
        if status < 0:
            raise pmErr, status
        text = str( buf.value )
        LIBC.free( buf )
        return text

    def pmLookupText(self, pmid, kind = api.PM_TEXT_ONELINE):
        """PMAPI - Lookup the description of a metric from its pmID
        "desc" = pmLookupText(pmid)
        """
        buf = c_char_p()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmLookupText(pmid, kind, byref(buf))
        if status < 0:
            raise pmErr, status
        text = buf.value
        LIBC.free( buf )
        return text

    ##
    # PMAPI Instance Domain Services

    def pmGetInDom(self, pmdescp):
        """PMAPI - Lookup the list of instances from an instance domain PMDESCP

        ([instance1, instance2...] [name1, name2...]) pmGetInDom(pmDesc pmdesc)
        """
        instA_p = POINTER(c_int)()
        nameA_p = POINTER(c_char_p)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmGetInDom(get_indom(pmdescp),
                                    byref(instA_p), byref(nameA_p))
        if status < 0:
            raise pmErr, status
        if status > 0:
            nameL = map(lambda x: str(nameA_p[x]), range(status))
            instL = map(lambda x: int(instA_p[x]), range(status))
            LIBC.free(instA_p)
            LIBC.free(nameA_p)
        else:
            instL = None
            nameL = None
        return instL, nameL

    def pmLookupInDom(self, pmdesc, name):
        """PMAPI - Lookup the instance id with the given NAME in the indom

        c_uint instid = pmLookupInDom(pmDesc pmdesc, "Instance")   
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmLookupInDom(get_indom(pmdesc), name)
        if status < 0:
            raise pmErr, status
        return status

    def pmNameInDom(self, pmdesc, instval):
        """PMAPI - Lookup the text name of an instance in an instance domain

        "string" = pmNameInDom(pmDesc pmdesc, c_uint instid)
        """
        if instval == api.PM_IN_NULL:
            return "PM_IN_NULL"
        name_p = c_char_p()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmNameInDom(get_indom(pmdesc), instval, byref(name_p))
        if status < 0:
            raise pmErr, status
        outName = str(name_p.value)
        LIBC.free(name_p)
        return outName

    ##
    # PMAPI Context Services

    def pmNewContext(self, typed, name):
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

    def pmDupContext(self):
        """PMAPI - Duplicate the current PMAPI Context

        This supports copying a pmContext object
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmDupContext()
        if status < 0:
            raise pmErr, status
        return status

    def pmUseContext(self, handle):
        """PMAPI - NOOP - Set the PMAPI context to that identified by handle

        This is unimplemented. Context changes are handled by the individual
        methods in a pmContext class instance.
        """
        pass

    @staticmethod
    def pmWhichContext():
        """PMAPI - Returns the handle of the current PMAPI context
        context = pmWhichContext()
        """
        status = LIBPCP.pmWhichContext()
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
            numinst = 0
            instA = POINTER(c_int)()
        else:
            numinst = len( instL )
            instA = (c_int * numinst)()
            for index, value in enumerate( instL ):
                instA[index] = value
        status = LIBPCP.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmAddProfile( get_indom(pmdesc), numinst, instA )
        if status < 0:
            raise pmErr, status
        return status

    def pmDelProfile( self, pmdesc, instL ):
        """PMAPI - delete instances from list to be collected from indom 

        status = pmDelProfile(pmDesc pmdesc, c_uint inst)
        status = pmDelProfile(pmDesc pmdesc, [c_uint inst])
        """
        if instL == None or len(instL) == 0:
            numinst = 0
            instA = POINTER(c_int)()
        else:
            numinst = len(instL)
            instA = (c_int * numinst)()
            for index, value in enumerate(instL):
                instA[index] = value
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmDelProfile(get_indom(pmdesc), numinst, instA)
        if status < 0:
            raise pmErr, status
        return status

    def pmSetMode( self, mode, timeVal, delta ):
        """PMAPI - set interpolation mode for reading archive files
        code = pmSetMode(pmapi.PM_MODE_INTERP, timeval, 0)
        """
        status = LIBPCP.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmSetMode( mode, pointer(timeVal), delta )
        if status < 0:
            raise pmErr, status
        return status

    def pmReconnectContext( self ):
        """PMAPI - Reestablish the context connection

        Unlike the underlying PMAPI function, this method takes no parameter.
        This method simply attempts to reestablish the the context belonging
        to its pmContext instance object.
        """
        status = LIBPCP.pmReconnectContext( self.ctx )
        if status < 0:
            raise pmErr, status
        return status

    def pmGetContextHostName( self ):
        """PMAPI - Lookup the hostname for the given context

        Unlike the underlying PMAPI function, this method takes no parameter.
        This method simply returns the name of the context belonging to its
        pmContext instance object.

        "hostname" = pmGetContextHostName()
        """
        status = LIBPCP.pmGetContextHostName( self.ctx )
        if status < 0:
            raise pmErr, status
        return status

    ##
    # PMAPI Timezone Services

    def pmNewContextZone( self ):
        """PMAPI - Query and set the current reporting timezone
        """
        status = LIBPCP.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmNewContextZone( )
        if status < 0:
            raise pmErr, status
        return status

    def pmNewZone( self, tz ):
        """PMAPI - Create new zone handle and set reporting timezone
        """
        status = LIBPCP.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmNewContextZone( tz )
        if status < 0:
            raise pmErr, status
        return status

    def pmUseZone( self, tz_handle ):
        """PMAPI - Sets the current reporting timezone
        """
        status = LIBPCP.pmUseContext( self.ctx )
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmUseZone( tz_handle )
        if status < 0:
            raise pmErr, status
        return status

    def pmWhichZone( self ):
        """PMAPI - Query the current reporting timezone
        """
        status = LIBPCP.pmGetContextHostName( self.ctx )
        if status < 0:
            raise pmErr, status
        return status


    ##
    # PMAPI Metrics Services

    def pmFetch(self, pmidA):
        """PMAPI - Fetch pmResult from the target source 

        (status, pmResult* pmresult) = pmFetch(c_uint pmid[])
        """
        result_p = POINTER(pmResult)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmFetch(len(pmidA), pmidA, byref(result_p))
        if status < 0:
            raise pmErr, status
        return status, result_p

    @staticmethod
    def pmFreeResult(result_p):
        """PMAPI - Free a result previously allocated by pmFetch
        pmFreeResult(pmResult* pmresult)
        """
        LIBPCP.pmFreeResult(result_p)

    def pmStore(self, result):
        """PMAPI - Set values on target source, inverse of pmFetch
        code = pmStore(pmResult* pmresult)
        """
        LIBPCP.pmStore.argtypes = [(type(result))]
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmStore(result)
        if status < 0:
            raise pmErr, status
        return status, result


    ##
    # PMAPI Archive-Specific Services

    def pmGetArchiveLabel(self, loglabel):
        """PMAPI - Get the label record from the archive
        (status, loglabel) = pmGetArchiveLabel()
        """
        loglabel = pmLogLabel()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmGetArchiveLabel(byref(loglabel))
        if status < 0:
            raise pmErr, status
        return status, loglabel
    
    def pmGetArchiveEnd(self):
        """PMAPI - Get the last recorded timestamp from the archive
        """
        tvp = POINTER(timeval)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmGetArchiveEnd(tvp)
        if status < 0:
            raise pmErr, status
        return status, tvp

    def pmGetInDomArchive(self, pmdescp):
        """PMAPI - Get the instance IDs and names for an instance domain

        ((instance1, instance2...) (name1, name2...)) pmGetInDom(pmDesc pmdesc)
        """
        instA_p = POINTER(c_int)()
        nameA_p = POINTER(c_char_p)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        indom = get_indom(pmdescp)
        status = LIBPCP.pmGetInDomArchive(indom, byref(instA_p), byref(nameA_p))
        if status < 0:
            raise pmErr, status
        if status > 0:
            nameL = map(lambda x: str(nameA_p[x]), range(status))
            instL = map(lambda x: int(instA_p[x]), range(status))
            LIBC.free(instA_p)
            LIBC.free(nameA_p)
        else:
            instL = None
            nameL = None
        return instL, nameL

    def pmLookupInDomArchive(self, pmdesc, name):
        """PMAPI - Lookup the instance id with the given name in the indom

        c_uint instid = pmLookupInDomArchive(pmDesc pmdesc, "Instance")   
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmLookupInDomArchive(get_indom(pmdesc), name)
        if status < 0:
            raise pmErr, status
        return status

    def pmNameInDomArchive(self, pmdesc, inst):
        """PMAPI - Lookup the text name of an instance in an instance domain

        "string" = pmNameInDomArchive(pmDesc pmdesc, c_uint instid)
        """
        name_p = c_char_p()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        indom = get_indom(pmdesc)
        status = LIBPCP.pmNameInDomArchive(indom, inst, byref(name_p))
        if status < 0:
            raise pmErr, status
        outName = str(name_p.value)
        LIBC.free(name_p)
        return outName

    def pmFetchArchive(self):
        """PMAPI - Fetch measurements from the target source

        (status, pmResult* pmresult) = pmFetch()
        """
        result_p = POINTER(pmResult)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr, status
        status = LIBPCP.pmFetchArchive(byref(result_p))
        if status < 0:
            raise pmErr, status
        return status, result_p


    ##
    # PMAPI Ancilliary Support Services

    @staticmethod
    def pmGetConfig(variable):
        """PMAPI - Return value from environment or pcp config file """
        return str(LIBPCP.pmGetConfig(variable))

    @staticmethod
    def pmErrStr(code):
        """PMAPI - Return value from environment or pcp config file """
        errstr = ctypes.create_string_buffer(api.PM_MAXERRMSGLEN)
        return str(LIBPCP.pmErrStr_r(code, errstr, api.PM_MAXERRMSGLEN))

    @staticmethod
    def pmExtractValue(valfmt, vlist, intype, outtype):
        """PMAPI - Extract a value from a pmValue struct and convert its type

        (status, pmAtomValue) = pmExtractValue(results.contents.get_valfmt(i),
        				       results.contents.get_vlist(i, 0),
                                               descs[i].contents.type,
                                               pmapi.PM_TYPE_FLOAT)
        """
        outAtom = pmAtomValue()
        status = LIBPCP.pmExtractValue(valfmt, vlist, intype,
                                        byref(outAtom), outtype)
        if status < 0:
            raise pmErr, status
        return status, outAtom

    @staticmethod
    def pmConvScale(inType, inAtom, desc, metric_idx, outUnits):
        """PMAPI - Convert a value to a different scale

        (status, pmAtomValue) = pmConvScale(pmapi.PM_TYPE_FLOAT, pmAtomValue,
        				    pmDesc*, 3, pmapi.PM_SPACE_MBYTE)
        """
        outAtom = pmAtomValue()
        pmunits = pmUnits()
        pmunits.dimSpace = 1
        pmunits.scaleSpace = outUnits
        status = LIBPCP.pmConvScale(inType, byref(inAtom),
                         byref(desc[metric_idx].contents.units), byref(outAtom),
                         byref(pmunits))
        if status < 0:
            raise pmErr, status
        return status, outAtom

    @staticmethod
    def pmUnitsStr(units):
        """PMAPI - Convert units struct to a readable string """
        unitstr = ctypes.create_string_buffer(64)
        return str(LIBPCP.pmUnitsStr_r(units, unitstr, 64))

    @staticmethod
    def pmIDStr(pmid):
        """PMAPI - Convert a pmID to a readable string """
        pmidstr = ctypes.create_string_buffer(32)
        return str(LIBPCP.pmIDStr_r(pmid, pmidstr, 32))

    @staticmethod
    def pmInDomStr(pmdescp):
        """PMAPI - Convert an instance domain ID  to a readable string
        "indom" =  pmGetInDom(pmDesc pmdesc)
        """
        indomstr = ctypes.create_string_buffer(32)
        return str(LIBPCP.pmInDomStr_r(get_indom(pmdescp), indomstr, 32))

    @staticmethod
    def pmTypeStr(typed):
        """PMAPI - Convert a performance metric type to a readable string
        "type" = pmTypeStr(pmapi.PM_TYPE_FLOAT)
        """
        typestr = ctypes.create_string_buffer(32)
        return str( LIBPCP.pmTypeStr_r(typed, typestr, 32))

    @staticmethod
    def pmAtomStr(atom, typed):
        """PMAPI - Convert a value atom to a readable string
        "value" = pmAtomStr(atom, pmapi.PM_TYPE_U32)
        """
        atomstr = ctypes.create_string_buffer(96)
        return str(LIBPCP.pmAtomStr(byref(atom), typed, atomstr, 96))

    @staticmethod
    def pmPrintValue(fileObj, result, ptype, vset_idx, vlist_idx, minWidth):
        """PMAPI - Print the value of a metric """
        LIBPCP.pmPrintValue(ctypes.pythonapi.PyFile_AsFile(fileObj),
                c_int(result.contents.vset[vset_idx].contents.valfmt),
                c_int(ptype.contents.type),
                byref(result.contents.vset[vset_idx].contents.vlist[vlist_idx]),
                minWidth)

    @staticmethod
    def pmflush():
        """PMAPI - flush the internal buffer shared with pmprintf """
        status = LIBPCP.pmflush()
        if status < 0:
            raise pmErr, status
        return status

    @staticmethod
    def pmprintf(fmt, *args):
        """PMAPI - append message to internal buffer for later printing """
        status = LIBPCP.pmprintf(fmt, *args)
        if status < 0:
            raise pmErr, status

    @staticmethod
    def pmSortInstances(result_p):
        """PMAPI - sort all metric instances in result returned by pmFetch """
        LIBPCP.pmSortInstances.argtypes = [(type(result_p))]
        LIBPCP.pmSortInstances(result_p)
        return None

    @staticmethod
    def pmParseInterval(interval):
        """PMAPI - parse a textual time interval into a timeval struct
        (status, timeval_ctype, "error message") = pmParseInterval("time string")
        """
        tvp = timeval()
        errmsg = POINTER(c_char_p)()
        status = LIBPCP.pmParseInterval(interval, byref(tvp), errmsg)
        if status < 0:
            raise pmErr, status
        return status, tvp, errmsg

    @staticmethod
    def pmParseMetricSpec(string, isarch, source):
        """PMAPI - parse a textual metric specification into a struct
        (status,result,errormssg) = pmTypeStr("kernel.all.load", 0, "localhost")
        """
        rsltp = POINTER(pmMetricSpec)()
        errmsg = c_char_p()
        status = LIBPCP.pmParseMetricSpec(string, isarch, source,
                                        byref(rsltp), byref(errmsg))
        if status < 0:
            raise pmErr, status
        return status, rsltp, errmsg

