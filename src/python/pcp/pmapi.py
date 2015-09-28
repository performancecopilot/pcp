# pylint: disable=C0103
""" Wrapper module for LIBPCP - the core Performace Co-Pilot API """
#
# Copyright (C) 2012-2015 Red Hat
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
# http://www.pcp.io
#
# Performance Co-Pilot Programmer's Guide
# cf. Chapter 3. PMAPI - The Performance Metrics API
#
# EXAMPLE
"""
    from pcp import pmapi
    import cpmapi as c_api

    # Create a pcp class
    context = pmapi.pmContext(c_api.PM_CONTEXT_HOST, "local:")

    # Get ids for number cpus and load metrics
    metric_ids = context.pmLookupName(("hinv.ncpu","kernel.all.load"))
    # Get the description of the metrics
    descs = context.pmLookupDescs(metric_ids)
    # Fetch the current value for number cpus
    results = context.pmFetch(metric_ids)
    # Extract the value into a scalar value
    atom = context.pmExtractValue(results.contents.get_valfmt(0),
                                  results.contents.get_vlist(0, 0),
                                  descs[0].contents.type,
                                  c_api.PM_TYPE_U32)
    print "#cpus=", atom.ul

    # Get the instance ids for kernel.all.load
    inst1 = context.pmLookupInDom(descs[1], "1 minute")
    inst5 = context.pmLookupInDom(descs[1], "5 minute")

    # Loop through the metric ids
    for i in range(results.contents.numpmid):
        # Is this the kernel.all.load id?
        if (results.contents.get_pmid(i) != metric_ids[1]):
            continue
        # Extract the kernel.all.load instance
        for j in range(results.contents.get_numval(i) - 1):
            atom = context.pmExtractValue(results.contents.get_valfmt(i),
                                          results.contents.get_vlist(i, j),
                                          descs[i].contents.type,
                                          c_api.PM_TYPE_FLOAT)
            value = atom.f
            if results.contents.get_inst(i, j) == inst1:
                print "load average 1=",atom.f
            elif results.contents.get_inst(i, j) == inst5:
                print "load average 5=",atom.f
"""

# for reporting on times from pmLocaltime function
from time import mktime

# constants adapted from C header file <pcp/pmapi.h>
import cpmapi as c_api

# for interfacing with LIBPCP - the client-side C API
import ctypes
from ctypes import c_char, c_int, c_uint, c_long, c_char_p, c_void_p
from ctypes import c_longlong, c_ulonglong, c_float, c_double
from ctypes import CDLL, POINTER, CFUNCTYPE, Structure, Union
from ctypes import addressof, pointer, sizeof, cast, byref
from ctypes import create_string_buffer, memmove
from ctypes.util import find_library


##############################################################################
#
# dynamic library loads
#
import sys

LIBPCP = CDLL(find_library("pcp"))
libc_name = "c" if sys.platform != "win32" else "msvcrt"
LIBC = CDLL(find_library(libc_name))


##############################################################################
#
# python version information and compatibility
#

if sys.version >= '3':
    integer_types = (int,)
    long = int
else:
    integer_types = (int, long,)

def pyFileToCFile(fileObj):
    if sys.version >= '3':
        from os import fdopen
        ctypes.pythonapi.PyObject_AsFileDescriptor.restype = ctypes.c_int
        ctypes.pythonapi.PyObject_AsFileDescriptor.argtypes = [ctypes.py_object]
        return fdopen(ctypes.pythonapi.PyObject_AsFileDescriptor(fileObj), "r", closefd=False)
    else:
        ctypes.pythonapi.PyFile_AsFile.restype = ctypes.c_void_p
        ctypes.pythonapi.PyFile_AsFile.argtypes = [ctypes.py_object]
        return ctypes.pythonapi.PyFile_AsFile(fileObj)


##############################################################################
#
# definition of exception classes
#

class pmErr(Exception):
    def __str__(self):
        errSym = None
        try:
            errSym = c_api.pmErrSymDict[self.args[0]]
        except KeyError:
            pass
        if errSym == None:
            return self.message()
        return "%s %s" % (errSym, self.message())

    def message(self):
        errStr = create_string_buffer(c_api.PM_MAXERRMSGLEN)
        errStr = LIBPCP.pmErrStr_r(self.args[0], errStr, c_api.PM_MAXERRMSGLEN)
        for index in range(1, len(self.args)):
            errStr += b" " + str(self.args[index]).encode('utf-8')
        return str(errStr.decode())

    def progname(self):
        return str(c_char_p.in_dll(LIBPCP, "pmProgname").value.decode())

class pmUsageErr(Exception):
    def message(self):
        for index in range(0, len(self.args)):
            LIBPCP.pmprintf(str(self.args[index]).encode('utf-8'))
        return c_api.pmUsageMessage()


##############################################################################
#
# definition of structures used by libpcp, derived from <pcp/pmapi.h>
#
# This section defines the data structures for accessing and manuiplating
# metric information and values.  Detailed information about these data
# structures can be found in:
#
# Performance Co-Pilot Programmer's Guide
# Section 3.4 - Performance Metric Descriptions
# Section 3.5 - Performance Metric Values
#

# these hardcoded decls should be derived from <sys/time.h>
class timeval(Structure):
    _fields_ = [("tv_sec", c_long),
                ("tv_usec", c_long)]

    def __init__(self, sec = 0, usec = 0):
        Structure.__init__(self)
        self.tv_sec = sec
        self.tv_usec = usec

    @classmethod
    def fromInterval(builder, interval):
        """ Construct timeval from a string using pmParseInterval """
        tvp = builder()
        errmsg = c_char_p()
        if type(interval) != type(b''):
            interval = interval.encode('utf-8')
        status = LIBPCP.pmParseInterval(interval, byref(tvp), byref(errmsg))
        if status < 0:
            raise pmErr(status, errmsg)
        return tvp

    def __str__(self):
        return "%.3f" % c_api.pmtimevalToReal(self.tv_sec, self.tv_usec)

    def __float__(self):
        return float(c_api.pmtimevalToReal(self.tv_sec, self.tv_usec))

    def __long__(self):
        return long(self.tv_sec)

    def __int__(self):
        return int(self.tv_sec)

    def sleep(self):
        """ Delay for the amount of time specified by this timeval. """
        c_api.pmtimevalSleep(self.tv_sec, self.tv_usec)
        return None

class tm(Structure):
    _fields_ = [("tm_sec", c_int),
                ("tm_min", c_int),
                ("tm_hour", c_int),
                ("tm_mday", c_int),
                ("tm_mon", c_int),
                ("tm_year", c_int),
                ("tm_wday", c_int),
                ("tm_yday", c_int),
                ("tm_isdst", c_int),
                ("tm_gmtoff", c_long),	# glibc/bsd extension
                ("tm_zone", c_char_p)]	# glibc/bsd extension

    def __str__(self):
        timetuple = (self.tm_year+1900, self.tm_mon, self.tm_mday,
                     self.tm_hour, self.tm_min, self.tm_sec,
                     self.tm_wday, self.tm_yday, self.tm_isdst)
        inseconds = 0.0
        try:
            inseconds = mktime(timetuple)
        except:
            pass
        return "%s %s" % (inseconds.__str__(), timetuple)

class pmAtomValue(Union):
    """Union used for unpacking metric values according to type

    Constants for specifying metric types are defined in module pmapi
    """
    _fields_ = [("l", c_int),
                ("ul", c_uint),
                ("ll", c_longlong),
                ("ull", c_ulonglong),
                ("f", c_float),
                ("d", c_double),
                ("cp", c_char_p),
                ("vp", c_void_p)]

    _atomDrefD = {c_api.PM_TYPE_32 : lambda x: x.l,
                  c_api.PM_TYPE_U32 : lambda x: x.ul,
                  c_api.PM_TYPE_64 : lambda x: x.ll,
                  c_api.PM_TYPE_U64 : lambda x: x.ull,
                  c_api.PM_TYPE_FLOAT : lambda x: x.f,
                  c_api.PM_TYPE_DOUBLE : lambda x: x.d,
                  c_api.PM_TYPE_STRING : lambda x: x.cp,
                  c_api.PM_TYPE_AGGREGATE : lambda x: None,
                  c_api.PM_TYPE_AGGREGATE_STATIC : lambda x: None,
                  c_api.PM_TYPE_NOSUPPORT : lambda x: None,
                  c_api.PM_TYPE_UNKNOWN : lambda x: None
            }

    def dref(self, typed):
        return self._atomDrefD[typed](self)

class pmUnits(Structure):
    """
    Compiler-specific bitfields specifying scale and dimension of metric values
    Constants for specifying metric units are defined in module pmapi
    IRIX => HAVE_BITFIELDS_LTOR, gcc => not so much
    """
    if c_api.HAVE_BITFIELDS_LTOR:
        _fields_ = [("dimSpace", c_int, 4),
                    ("dimTime", c_int, 4),
                    ("dimCount", c_int, 4),
                    ("scaleSpace", c_int, 4),
                    ("scaleTime", c_int, 4),
                    ("scaleCount", c_int, 4),
                    ("pad", c_int, 8)]
    else:
        _fields_ = [("pad", c_int, 8),
                    ("scaleCount", c_int, 4),
                    ("scaleTime", c_int, 4),
                    ("scaleSpace", c_int, 4),
                    ("dimCount", c_int, 4),
                    ("dimTime", c_int, 4),
                    ("dimSpace", c_int, 4)]

    def __init__(self, dimS=0, dimT=0, dimC=0, scaleS=0, scaleT=0, scaleC=0):
        Structure.__init__(self)
        self.dimSpace = dimS
        self.dimTime = dimT
        self.dimCount = dimC
        self.scaleSpace = scaleS
        self.scaleTime = scaleT
        self.scaleCount = scaleC
        self.pad = 0

    def __str__(self):
        unitstr = ctypes.create_string_buffer(64)
        result = LIBPCP.pmUnitsStr_r(self, unitstr, 64)
        return str(result.decode())

class pmValueBlock(Structure):
    """Value block bitfields for different compilers
       A value block holds the value of an instance of a metric
       pointed to by the pmValue structure, when that value is
       too large (> 32 bits) to fit in the pmValue structure
    """
    if c_api.HAVE_BITFIELDS_LTOR:   # IRIX
        _fields_ = [("vtype", c_uint, 8),
                    ("vlen", c_uint, 24),
                    ("vbuf", c_char * 1)]
    else:   # Linux (gcc)
        _fields_ = [("vlen", c_uint, 24),
                    ("vtype", c_uint, 8),
                    ("vbuf", c_char * 1)]

class valueDref(Union):
    """Union in pmValue for dereferencing the value of an instance of a metric

    For small items, e.g. a 32-bit number, the union contains the actual value
    For large items, e.g. a text string, the union points to a pmValueBlock
    """
    _fields_ = [("pval", POINTER(pmValueBlock)),
                ("lval", c_int)]
    def __str__(self):
        return "value=%#lx" % (self.lval)

class pmValue(Structure):
    """Structure holding the value of a metric instance """
    _fields_ = [("inst", c_int),
                ("value", valueDref)]
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
            vals = range(self.numval)
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
        vals = range(self.numpmid)
        vstr = str([" %s" % str(self.vset[i].contents) for i in vals])
        return "pmResult@%#lx id#=%d " % (addressof(self), self.numpmid) + vstr

    def get_pmid(self, vset_idx):
        """ Return the pmid of vset[vset_idx] """
        vsetptr = cast(self.vset, POINTER(pmValueSetPtr))
        return vsetptr[vset_idx].contents.pmid

    def get_valfmt(self, vset_idx):
        """ Return the valfmt of vset[vset_idx] """
        vsetptr = cast(self.vset, POINTER(POINTER(pmValueSet)))
        return vsetptr[vset_idx].contents.valfmt

    def get_numval(self, vset_idx):
        """ Return the numval of vset[vset_idx] """
        vsetptr = cast(self.vset, POINTER(POINTER(pmValueSet)))
        return vsetptr[vset_idx].contents.numval

    def get_vset(self, vset_idx):
        """ Return the vset[vset_idx] """
        vsetptr = cast(self.vset, POINTER(POINTER(pmValueSet)))
        return vsetptr[vset_idx]

    def get_vlist(self, vset_idx, vlist_idx):
        """ Return the vlist[vlist_idx] of vset[vset_idx] """
        listptr = cast(self.get_vset(vset_idx).contents.vlist, POINTER(pmValue))
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

pmDescPtr = POINTER(pmDesc)
pmDescPtr.sem = property(lambda x: x.contents.sem, None, None, None)
pmDescPtr.type = property(lambda x: x.contents.type, None, None, None)


def get_indom(pmdesc):
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
                 ("metric", c_char_p),
                 ("ninst", c_int),
                 ("inst",  POINTER(c_char_p)) ]
    def __str__(self):
        insts = list(map(lambda x: str(self.inst[x]), range(self.ninst)))
        fields = (addressof(self), self.isarch, self.source, insts)
        return "pmMetricSpec@%#lx src=%s metric=%s insts=" % fields

    @classmethod
    def fromString(builder, string, isarch = 0, source = ''):
        result = POINTER(builder)()
        errmsg = c_char_p()
        if type(source) != type(b''):
            source = source.encode('utf-8')
        if type(string) != type(b''):
            string = string.encode('utf-8')
        status = LIBPCP.pmParseMetricSpec(string, isarch, source,
                                        byref(result), byref(errmsg))
        if status < 0:
            raise pmErr(status, errmsg)
        return result

class pmLogLabel(Structure):
    """Label record at the start of every log file
    """
    _fields_ = [ ("magic", c_int),
                 ("pid_t", c_int),
                 ("start", timeval),
                 ("hostname", c_char * c_api.PM_LOG_MAXHOSTLEN),
                 ("tz", c_char * c_api.PM_TZ_MAXLEN) ]


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

LIBPCP.pmRegisterDerived.restype = c_int
LIBPCP.pmRegisterDerived.argtypes = [c_char_p, c_char_p]

LIBPCP.pmLoadDerivedConfig.restype = c_int
LIBPCP.pmLoadDerivedConfig.argtypes = [c_char_p]

LIBPCP.pmDerivedErrStr.restype = c_char_p
LIBPCP.pmDerivedErrStr.argtypes = []

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

LIBPCP.pmGetContextHostName_r.restype = c_char_p
LIBPCP.pmGetContextHostName_r.argtypes = [c_int, c_char_p, c_int]


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

LIBPCP.pmLocaltime.restype = POINTER(tm)
LIBPCP.pmLocaltime.argtypes = [POINTER(c_long), POINTER(tm)]

LIBPCP.pmCtime.restype = c_char_p
LIBPCP.pmCtime.argtypes = [POINTER(c_long), c_char_p]


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
LIBPCP.pmGetArchiveEnd.argtypes = [POINTER(timeval)]

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


LIBPCP.pmGetOptionalConfig.restype = c_char_p
LIBPCP.pmGetOptionalConfig.argtypes = [c_char_p]

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

LIBPCP.pmNumberStr_r.restype = c_char_p
LIBPCP.pmNumberStr_r.argtypes = [c_double, c_char_p, c_int]

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
LIBPCP.pmSortInstances.argtypes = [POINTER(pmResult)]


##############################################################################
#
# class pmOptions
#
# This class wraps the PMAPI pmGetOptions functionality and can be used
# to assist with automatic construction of a PMAPI context based on the
# command line options used.
#

class pmOptions(object):
    """ Command line option parsing for short and long form arguments
        Passed into pmGetOptions, pmGetContextOptions, pmUsageMessage.
    """
    ##
    # property read methods

    def _R_mode(self):
        return self._mode
    def _R_delta(self):
        return self._delta
    def _R_need_reset(self):
        return self._need_reset
    def _W_need_reset(self, value):
        self._need_reset = value

    ##
    # property definitions

    mode = property(_R_mode, None, None, None)
    delta = property(_R_delta, None, None, None)
    need_reset = property(_R_need_reset, _W_need_reset, None, None)

    ##
    # creation and destruction

    def __init__(self, short_options = None, short_usage = None, flags = 0):
        c_api.pmResetAllOptions()
        if (short_options != None):
            c_api.pmSetShortOptions(short_options)
        if short_usage != None:
            c_api.pmSetShortUsage(short_usage)
        if flags != 0:
            c_api.pmSetOptionFlags(flags)
        else:   # good default for scripts - always evaluating log bounds
            c_api.pmSetOptionFlags(c_api.PM_OPTFLAG_BOUNDARIES)
        self._delta = 1			# default archive pmSetMode delta
        self._mode = c_api.PM_MODE_INTERP # default pmSetMode access mode
        self._need_reset = False	# flag for __del__ memory reclaim

    def __del__(self):
        if LIBPCP and self._need_reset != False:
            c_api.pmResetAllOptions()

    ##
    # general command line option access and manipulation

    def pmGetOptionFlags(self):
        return c_api.pmGetOptionFlags()

    def pmSetOptionFlags(self, flags):
        return c_api.pmSetOptionFlags(flags)

    def pmGetOptionErrors(self):
        return c_api.pmGetOptionErrors()

    def pmSetOptionErrors(self):
        errors = c_api.pmGetOptionErrors() + 1
        return c_api.pmSetOptionErrors(errors)

    def pmSetShortUsage(self, short_usage):
        return c_api.pmSetShortUsage(short_usage)

    def pmSetShortOptions(self, short_options):
        return c_api.pmSetShortOptions(short_options)

    def pmSetOptionSamples(self, count):
        """ Set sample count (converts string to integer) """
        return c_api.pmSetOptionSamples(count)

    def pmSetOptionInterval(self, interval):
        """ Set sampling interval (pmParseInterval string) """
        return c_api.pmSetOptionInterval(interval)

    def pmNonOptionsFromList(self, argv):
        return c_api.pmGetNonOptionsFromList(argv)

    def pmSetCallbackObject(self, them):
        """ When options are being parsed from within an object, the
            caller will want the "self" of the other object ("them")
            passed as the first parameter to the callback function.
        """
        return c_api.pmSetCallbackObject(them)

    def pmSetOptionCallback(self, func):
        """ Handle individual command line options, outside of the PCP
            "standard" set (or overridden).

            For every non-standard or overridden option, this callback
            will be called with the short option character (as an int)
            or zero for long-option only, and the usual getopts global
            state (optind, opterr, optopt, optarg, and index - all int
            except optarg which is a str).
        """
        return c_api.pmSetOptionCallback(func)

    def pmSetOverrideCallback(self, func):
        """ Allow a "standard" PCP option to be overridden.

            For every option parsed, this callback is called and it may
            return zero, meaning continue with processing the option in
            the standard way, or non-zero, meaning the caller wishes to
            override and interpret the option differently.
            Callback input: int, output: int
        """
        return c_api.pmSetOverrideCallback(func)

    def pmSetLongOption(self, long_opt, has_arg, short_opt, argname, message):
        """ Add long option into the set of supported long options

            Pass in the option name (str), whether it takes an argument (int),
            its short option form (int), and two usage message hints (argname
            (str) and message (str) - see pmGetOptions(3) for details).
        """
        return c_api.pmSetLongOption(long_opt, has_arg, short_opt, argname, message)

    def pmSetLongOptionHeader(self, heading):
        """ Add a new section heading into the long option usage message """
        return c_api.pmSetLongOptionHeader(heading)

    def pmSetLongOptionText(self, text):
        """ Add some descriptive text into the long option usage message """
        return c_api.pmSetLongOptionText(text)

    def pmSetLongOptionAlign(self):
        """ Add support for -A/--align into PMAPI monitor tool """
        return c_api.pmSetLongOptionAlign()

    def pmSetLongOptionArchive(self):
        """ Add support for -a/--archive into PMAPI monitor tool """
        return c_api.pmSetLongOptionArchive()

    def pmSetLongOptionDebug(self):
        """ Add support for -D/--debug into PMAPI monitor tool """
        return c_api.pmSetLongOptionDebug()

    def pmSetLongOptionGuiMode(self):
        """ Add support for -g/--guimode into PMAPI monitor tool """
        return c_api.pmSetLongOptionGuiMode()

    def pmSetLongOptionHost(self):
        """ Add support for -h/--host into PMAPI monitor tool """
        return c_api.pmSetLongOptionHost()

    def pmSetLongOptionHostsFile(self):
        """ Add support for -H/--hostsfile into PMAPI monitor tool """
        return c_api.pmSetLongOptionHostsFile()

    def pmSetLongOptionSpecLocal(self):
        """ Add support for -K/--spec-local into PMAPI monitor tool """
        return c_api.pmSetLongOptionSpecLocal()

    def pmSetLongOptionLocalPMDA(self):
        """ Add support for -L/--local-PMDA into PMAPI monitor tool """
        return c_api.pmSetLongOptionLocalPMDA()

    def pmSetLongOptionOrigin(self):
        """ Add support for -O/--origin into PMAPI monitor tool """
        return c_api.pmSetLongOptionOrigin()

    def pmSetLongOptionGuiPort(self):
        """ Add support for -p/--guiport into PMAPI monitor tool """
        return c_api.pmSetLongOptionGuiPort()

    def pmSetLongOptionStart(self):
        """ Add support for -S/--start into PMAPI monitor tool """
        return c_api.pmSetLongOptionStart()

    def pmSetLongOptionSamples(self):
        """ Add support for -s/--samples into PMAPI monitor tool """
        return c_api.pmSetLongOptionSamples()

    def pmSetLongOptionFinish(self):
        """ Add support for -T/--finish into PMAPI monitor tool """
        return c_api.pmSetLongOptionFinish()

    def pmSetLongOptionInterval(self):
        """ Add support for -t/--interval into PMAPI monitor tool """
        return c_api.pmSetLongOptionInterval()

    def pmSetLongOptionVersion(self):
        """ Add support for -V/--version into PMAPI monitor tool """
        return c_api.pmSetLongOptionVersion()

    def pmSetLongOptionTimeZone(self):
        """ Add support for -Z/--timezone into PMAPI monitor tool """
        return c_api.pmSetLongOptionTimeZone()

    def pmSetLongOptionHostZone(self):
        """ Add support for -z/--hostzone into PMAPI monitor tool """
        return c_api.pmSetLongOptionHostZone()

    def pmSetLongOptionHelp(self):
        """ Add support for -?/--help into PMAPI monitor tool """
        return c_api.pmSetLongOptionHelp()

    def pmSetLongOptionArchiveList(self):
        """ Add support for --archive-list into PMAPI monitor tool """
        return c_api.pmSetLongOptionArchiveList()

    def pmSetLongOptionArchiveFolio(self):
        """ Add support for --archive-folio into PMAPI monitor tool """
        return c_api.pmSetLongOptionArchiveFolio()

    def pmSetLongOptionHostList(self):
        """ Add support for --host-list into PMAPI monitor tool """
        return c_api.pmSetLongOptionHostList()

    def pmGetOptionContext(self):	# int (typed)
        return c_api.pmGetOptionContext()

    def pmGetOptionHosts(self):		# str list
        return c_api.pmGetOptionHosts()

    def pmGetOptionArchives(self):	# str list
        return c_api.pmGetOptionArchives()

    def pmGetOptionAlignment(self):	# timeval
        alignment = c_api.pmGetOptionAlign_optarg()
        if alignment == None:
            return None
        return timeval.fromInterval(alignment)

    def pmGetOptionStart(self):		# timeval
        sec = c_api.pmGetOptionStart_sec()
        if sec == None:
            return None
        return timeval(sec, c_api.pmGetOptionStart_usec())

    def pmGetOptionAlignOptarg(self):	# string
        return c_api.pmGetOptionAlign_optarg()

    def pmGetOptionFinishOptarg(self):	# string
        return c_api.pmGetOptionFinish_optarg()

    def pmGetOptionFinish(self):	# timeval
        sec = c_api.pmGetOptionFinish_sec()
        if sec == None:
            return None
        return timeval(sec, c_api.pmGetOptionFinish_usec())

    def pmGetOptionOrigin(self):	# timeval
        sec = c_api.pmGetOptionOrigin_sec()
        if sec == None:
            return None
        return timeval(sec, c_api.pmGetOptionOrigin_usec())

    def pmGetOptionInterval(self):	# timeval
        sec = c_api.pmGetOptionInterval_sec()
        if sec == None:
            return None
        return timeval(sec, c_api.pmGetOptionInterval_usec())

    def pmGetOptionSamples(self):	# int
        return c_api.pmGetOptionSamples()

    def pmGetOptionTimezone(self):	# str
        return c_api.pmGetOptionTimezone()

    def pmSetOptionArchiveList(self, archives):	# str
        return c_api.pmSetOptionArchiveList(archives)

    def pmSetOptionArchiveFolio(self, folio):	# str
        return c_api.pmSetOptionArchiveFolio(folio)

    def pmSetOptionHostList(self, hosts):	# str
        return c_api.pmSetOptionHostList(hosts)


##############################################################################
#
# class pmContext
#
# This class wraps the PMAPI library functions
#

class pmContext(object):
    """Defines a metrics source context (e.g. host, archive, etc) to operate on

    pmContext(c_api.PM_CONTEXT_HOST,"local:")
    pmContext(c_api.PM_CONTEXT_ARCHIVE,"FILENAME")

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
    # creation and destruction

    def __init__(self, typed = c_api.PM_CONTEXT_HOST, target = "local:"):
        self._type = typed                              # the context type
        self._target = target                            # the context target
        self._ctx = c_api.PM_ERR_NOCONTEXT                # init'd pre-connect
        if target and type(target) != type(b''):
            source = target.encode('utf-8')
        else:
            source = target
        self._ctx = LIBPCP.pmNewContext(typed, source)     # the context handle
        if self._ctx < 0:
            raise pmErr(self._ctx, [target])

    def __del__(self):
        if LIBPCP and self._ctx != c_api.PM_ERR_NOCONTEXT:
            LIBPCP.pmDestroyContext(self._ctx)

    @classmethod
    def fromOptions(builder, options, argv, typed = 0, index = 0):
        """ Helper interface, simple PCP monitor argument parsing.

            Take argv list, create a context using pmGetOptions(3)
            and standard options default values like local: etc
            based on the contents of the list.

            Caller should have already registered any options of
            interest using the option family of interfaces, i.e.
            pmSetShortOptions, pmSetLongOption*, pmSetOptionFlags,
            pmSetOptionCallback, and pmSetOptionOverrideCallback.

            When the MULTI/MIXED pmGetOptions flags are being used,
            the typed/index parameters can be used to setup several
            contexts based on the given command line parameters.
        """
        if (typed <= 0):
            options.need_reset = True
            if c_api.pmGetOptionsFromList(argv):
                raise pmUsageErr
            typed = options.pmGetOptionContext()

        if typed == c_api.PM_CONTEXT_ARCHIVE:
            archives = options.pmGetOptionArchives()
            source = archives[index]
        elif typed == c_api.PM_CONTEXT_HOST:
            hosts = options.pmGetOptionHosts()
            source = hosts[index]
        elif typed == c_api.PM_CONTEXT_LOCAL:
            source = None
        else:
            typed = c_api.PM_CONTEXT_HOST
            source = "local:"

        # core work done here - constructs the new pmContext
        context = builder(typed, source)

        # finish time windows, timezones, archive access mode
        if c_api.pmSetContextOptions(context.ctx, options.mode, options.delta):
            raise pmUsageErr

        return context

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
            raise pmErr(status)
        if type(name) != type(b''):
            name = name.encode('utf-8')
        status = LIBPCP.pmGetChildren(name, byref(offspring))
        if status < 0:
            raise pmErr(status)
        if status > 0:
            childL = list(map(lambda x: str(offspring[x].decode()), range(status)))
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
            raise pmErr(status)
        if type(name) != type(b''):
            name = name.encode('utf-8')
        status = LIBPCP.pmGetChildrenStatus(name,
                        byref(offspring), byref(childstat))
        if status < 0:
            raise pmErr(status)
        if status > 0:
            childL = list(map(lambda x: str(offspring[x].decode()), range(status)))
            statL = list(map(lambda x: int(childstat[x]), range(status)))
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
            raise pmErr(status)
        status = LIBPCP.pmGetPMNSLocation()
        if status < 0:
            raise pmErr(status)
        return status

    def pmLoadNameSpace(self, filename):
        """PMAPI - Load a local namespace
        status = pmLoadNameSpace("filename")
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        if type(filename) != type(b''):
            filename = filename.encode('utf-8')
        status = LIBPCP.pmLoadNameSpace(filename)
        if status < 0:
            raise pmErr(status)
        return status

    def pmLookupName(self, nameA, relaxed = 0):
        """PMAPI - Lookup pmIDs from a list of metric names nameA

        c_uint pmid [] = pmLookupName("MetricName")
        c_uint pmid [] = pmLookupName(("MetricName1", "MetricName2", ...))
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        if type(nameA) == type(u'') or type(nameA) == type(b''):
            n = 1
        else:
            n = len(nameA)
        names = (c_char_p * n)()
        if type(nameA) == type(u''):
            names[0] = c_char_p(nameA.encode('utf-8'))
        elif type(nameA) == type(b''):
            names[0] = c_char_p(nameA)
        else:
            for i in range(len(nameA)):
                if type(nameA[i]) == type(b''):
                    names[i] = c_char_p(nameA[i])
                else:
                    names[i] = c_char_p(nameA[i].encode('utf-8'))
        pmidA = (c_uint * n)()
        LIBPCP.pmLookupName.argtypes = [c_int, (c_char_p * n), POINTER(c_uint)]
        status = LIBPCP.pmLookupName(n, names, pmidA)
        if status < 0:
            raise pmErr(status, pmidA)
        elif relaxed == 0 and status != n:
            badL = [name for (name, pmid) in zip(nameA, pmidA) \
                                                if pmid == c_api.PM_ID_NULL]
            raise pmErr(c_api.PM_ERR_NAME, badL)
        return pmidA

    def pmNameAll(self, pmid):
        """PMAPI - Return list of all metric names having this identical PMID
        tuple names = pmNameAll(metric_id)
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        nameA_p = POINTER(c_char_p)()
        status = LIBPCP.pmNameAll(pmid, byref(nameA_p))
        if status < 0:
            raise pmErr(status)
        nameL = list(map(lambda x: str(nameA_p[x].decode()), range(status)))
        LIBC.free(nameA_p)
        return nameL

    def pmNameID(self, pmid):
        """PMAPI - Return a metric name from a PMID
        name = pmNameID(self.metric_id)
        """
        name = c_char_p()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmNameID(pmid, byref(name))
        if status < 0:
            raise pmErr(status)
        result = name.value
        LIBC.free(name)
        return str(result.decode())

    def pmTraversePMNS(self, name, callback):
        """PMAPI - Scan namespace, depth first, run CALLBACK at each node
        status = pmTraversePMNS("kernel", traverse_callback)
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = c_api.pmnsTraverse(name, callback)
        if status < 0:
            raise pmErr(status)

    def pmUnLoadNameSpace(self):
        """PMAPI - Unloads a local PMNS, if one was previously loaded
        pm.pmUnLoadNameSpace("NameSpace")
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmUnloadNameSpace()
        if status < 0:
            raise pmErr(status)

    def pmRegisterDerived(self, name, expr):
        """PMAPI - Register a derived metric name and definition
        pm.pmRegisterDerived("MetricName", "MetricName Expression")
        """
        if type(name) != type(b''):
            name = name.encode('utf-8')
        if type(expr) != type(b''):
            expr = expr.encode('utf-8')
        status = LIBPCP.pmRegisterDerived(name, expr)
        if status != 0:
            raise pmErr(status)
        status = LIBPCP.pmReconnectContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        
    def pmLoadDerivedConfig(self, fname):
        """PMAPI - Register derived metric names and definitions from a file
        pm.pmLoadDerivedConfig("FileName")
        """
        if type(fname) != type(b''):
            fname = fname.encode('utf-8')
        status = LIBPCP.pmLoadDerivedConfig(fname)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmReconnectContext(self.ctx)
        if status < 0:
            raise pmErr(status)

    @staticmethod
    def pmDerivedErrStr():
        """PMAPI - Return an error message if the pmRegisterDerived metric
        definition cannot be parsed
        pm.pmRegisterDerived()
        """
        result = LIBPCP.pmDerivedErrStr()
        return str(result.decode())

    ##
    # PMAPI Metrics Description Services

    def pmLookupDesc(self, pmid_p):

        """PMAPI - Lookup a metric description structure from a pmID

        pmDesc* pmdesc = pmLookupDesc(c_uint pmid)
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)

        descbuf = create_string_buffer(sizeof(pmDesc))
        desc = cast(descbuf, POINTER(pmDesc))
        pmid = c_uint(pmid_p)
        status = LIBPCP.pmLookupDesc(pmid, desc)
        if status < 0:
            raise pmErr(status)
        return desc

    def pmLookupDescs(self, pmids_p):

        """PMAPI - Lookup metric description structures from pmIDs

        (pmDesc* pmdesc)[] = pmLookupDesc(c_uint pmid[N])
        (pmDesc* pmdesc)[] = pmLookupDesc(c_uint pmid)
        """
        if isinstance(pmids_p, integer_types):
            n = 1
        else:
            n = len(pmids_p)

        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)

        desc = (POINTER(pmDesc) * n)()

        for i in range(n):
            descbuf = create_string_buffer(sizeof(pmDesc))
            desc[i] = cast(descbuf, POINTER(pmDesc))
            if isinstance(pmids_p, integer_types):
                pmids = c_uint(pmids_p)
            else:
                pmids = c_uint(pmids_p[i])

            status = LIBPCP.pmLookupDesc(pmids, desc[i])
            if status < 0:
                raise pmErr(status)
        return desc

    def pmLookupInDomText(self, pmdesc, kind = c_api.PM_TEXT_ONELINE):
        """PMAPI - Lookup the description of a metric's instance domain

        "instance" = pmLookupInDomText(pmDesc pmdesc)
        """
        buf = c_char_p()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)

        status = LIBPCP.pmLookupInDomText(get_indom(pmdesc), kind, byref(buf))
        if status < 0:
            raise pmErr(status)
        result = buf.value
        LIBC.free(buf)
        return str(result.decode())

    def pmLookupText(self, pmid, kind = c_api.PM_TEXT_ONELINE):
        """PMAPI - Lookup the description of a metric from its pmID
        "desc" = pmLookupText(pmid)
        """
        buf = c_char_p()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmLookupText(pmid, kind, byref(buf))
        if status < 0:
            raise pmErr(status)
        text = buf.value
        LIBC.free(buf)
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
            raise pmErr(status)
        status = LIBPCP.pmGetInDom(get_indom(pmdescp),
                                    byref(instA_p), byref(nameA_p))
        if status < 0:
            raise pmErr(status)
        if status > 0:
            nameL = list(map(lambda x: str(nameA_p[x].decode('ascii', 'ignore')), range(status)))
            instL = list(map(lambda x: int(instA_p[x]), range(status)))
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
            raise pmErr(status)
        if type(name) != type(b''):
            name = name.encode('utf-8')
        status = LIBPCP.pmLookupInDom(get_indom(pmdesc), name)
        if status < 0:
            raise pmErr(status)
        return status

    def pmNameInDom(self, pmdesc, instval):
        """PMAPI - Lookup the text name of an instance in an instance domain

        "string" = pmNameInDom(pmDesc pmdesc, c_uint instid)
        """
        if instval == c_api.PM_IN_NULL:
            return "PM_IN_NULL"
        name_p = c_char_p()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmNameInDom(get_indom(pmdesc), instval, byref(name_p))
        if status < 0:
            raise pmErr(status)
        result = name_p.value
        LIBC.free(name_p)
        return str(result.decode('ascii', 'ignore'))

    ##
    # PMAPI Context Services

    def pmNewContext(self, typed, name):
        """PMAPI - NOOP - Establish a new PMAPI context (done in constructor)

        This is unimplemented. A new context is established when a pmContext
        object is created.
        """
        pass

    def pmDestroyContext(self, handle):
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
            raise pmErr(status)
        status = LIBPCP.pmDupContext()
        if status < 0:
            raise pmErr(status)
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
            raise pmErr(status)
        return status

    def pmAddProfile(self, pmdesc, instL):
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
            numinst = len(instL)
            instA = (c_int * numinst)()
            for index, value in enumerate(instL):
                instA[index] = value
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmAddProfile(get_indom(pmdesc), numinst, instA)
        if status < 0:
            raise pmErr(status)
        return status

    def pmDelProfile(self, pmdesc, instL):
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
            raise pmErr(status)
        status = LIBPCP.pmDelProfile(get_indom(pmdesc), numinst, instA)
        if status < 0:
            raise pmErr(status)
        return status

    def pmSetMode(self, mode, timeVal, delta):
        """PMAPI - set interpolation mode for reading archive files
        code = pmSetMode(c_api.PM_MODE_INTERP, timeval, 0)
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmSetMode(mode, pointer(timeVal), delta)
        if status < 0:
            raise pmErr(status)
        return status

    def pmReconnectContext(self):
        """PMAPI - Reestablish the context connection

        Unlike the underlying PMAPI function, this method takes no parameter.
        This method simply attempts to reestablish the the context belonging
        to its pmContext instance object.
        """
        status = LIBPCP.pmReconnectContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        return status

    def pmGetContextHostName(self):
        """PMAPI - Lookup the hostname for the given context

        This method simply returns the hostname for the context belonging to
        its pmContext instance object.

        "hostname" = pmGetContextHostName()
        """
        buflen = c_api.PM_LOG_MAXHOSTLEN
        buffer = ctypes.create_string_buffer(buflen)
        result = LIBPCP.pmGetContextHostName_r(self.ctx, buffer, buflen)
        return str(result.decode())

    ##
    # PMAPI Timezone Services

    def pmNewContextZone(self):
        """PMAPI - Query and set the current reporting timezone """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmNewContextZone()
        if status < 0:
            raise pmErr(status)
        return status

    @staticmethod
    def pmNewZone(tz):
        """PMAPI - Create new zone handle and set reporting timezone """
        if type(tz) != type(b''):
            tz = tz.encode('utf-8')
        status = LIBPCP.pmNewZone(tz)
        if status < 0:
            raise pmErr(status)
        return status

    @staticmethod
    def pmUseZone(tz_handle):
        """PMAPI - Sets the current reporting timezone """
        status = LIBPCP.pmUseZone(tz_handle)
        if status < 0:
            raise pmErr(status)
        return status

    @staticmethod
    def pmWhichZone():
        """PMAPI - Query the current reporting timezone """
        tz_p = c_char_p()
        status = LIBPCP.pmWhichZone(byref(tz_p))
        if status < 0:
            raise pmErr(status)
        tz = tz_p.value
        LIBC.free(tz_p)
        return str(tz.decode())

    def pmLocaltime(self, seconds):
        """PMAPI - convert the date and time for a reporting timezone """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        result = (tm)()
        timetp = c_long(long(seconds))
        LIBPCP.pmLocaltime(byref(timetp), byref(result))
        return result

    def pmCtime(self, seconds):
        """PMAPI - format the date and time for a reporting timezone """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        result = ctypes.create_string_buffer(32)
        timetp = c_long(long(seconds))
        LIBPCP.pmCtime(byref(timetp), result)
        return str(result.value.decode())

    ##
    # PMAPI Metrics Services

    def pmFetch(self, pmidA):
        """PMAPI - Fetch pmResult from the target source

        pmResult* pmresult = pmFetch(c_uint pmid[])
        """
        result_p = POINTER(pmResult)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmFetch(len(pmidA), pmidA, byref(result_p))
        if status < 0:
            raise pmErr(status)
        return result_p

    @staticmethod
    def pmFreeResult(result_p):
        """PMAPI - Free a result previously allocated by pmFetch
        pmFreeResult(pmResult* pmresult)
        """
        LIBPCP.pmFreeResult(result_p)

    def pmStore(self, result):
        """PMAPI - Set values on target source, inverse of pmFetch
        pmresult = pmStore(pmResult* pmresult)
        """
        LIBPCP.pmStore.argtypes = [(type(result))]
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmStore(result)
        if status < 0:
            raise pmErr(status)
        return result


    ##
    # PMAPI Archive-Specific Services

    def pmGetArchiveLabel(self):
        """PMAPI - Get the label record from the archive
        loglabel = pmGetArchiveLabel()
        """
        loglabel = pmLogLabel()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmGetArchiveLabel(byref(loglabel))
        if status < 0:
            raise pmErr(status)
        return loglabel

    def pmGetArchiveEnd(self):
        """PMAPI - Get the last recorded timestamp from the archive
        """
        tvp = timeval()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmGetArchiveEnd(byref(tvp))
        if status < 0:
            raise pmErr(status)
        return tvp

    def pmGetInDomArchive(self, pmdescp):
        """PMAPI - Get the instance IDs and names for an instance domain

        ((instance1, instance2...) (name1, name2...)) pmGetInDom(pmDesc pmdesc)
        """
        instA_p = POINTER(c_int)()
        nameA_p = POINTER(c_char_p)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        indom = get_indom(pmdescp)
        status = LIBPCP.pmGetInDomArchive(indom, byref(instA_p), byref(nameA_p))
        if status < 0:
            raise pmErr(status)
        if status > 0:
            nameL = list(map(lambda x: str(nameA_p[x].decode('ascii', 'ignore')), range(status)))
            instL = list(map(lambda x: int(instA_p[x]), range(status)))
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
            raise pmErr(status)
        if type(name) != type(b''):
            name = name.encode('utf-8')
        status = LIBPCP.pmLookupInDomArchive(get_indom(pmdesc), name)
        if status < 0:
            raise pmErr(status)
        return status

    def pmNameInDomArchive(self, pmdesc, inst):
        """PMAPI - Lookup the text name of an instance in an instance domain

        "string" = pmNameInDomArchive(pmDesc pmdesc, c_uint instid)
        """
        name_p = c_char_p()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        indom = get_indom(pmdesc)
        status = LIBPCP.pmNameInDomArchive(indom, inst, byref(name_p))
        if status < 0:
            raise pmErr(status)
        result = name_p.value
        LIBC.free(name_p)
        return str(result.decode('ascii', 'ignore'))

    def pmFetchArchive(self):
        """PMAPI - Fetch measurements from the target source

        pmResult* pmresult = pmFetch()
        """
        result_p = POINTER(pmResult)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmFetchArchive(byref(result_p))
        if status < 0:
            raise pmErr(status)
        return result_p


    ##
    # PMAPI Ancilliary Support Services

    @staticmethod
    def pmGetConfig(variable):
        """PMAPI - Return value from environment or pcp config file """
        if type(variable) != type(b''):
            variable = variable.encode('utf-8')
        result = LIBPCP.pmGetOptionalConfig(variable)
        if result == None:
            return result
        return str(result.decode())

    @staticmethod
    def pmErrStr(code):
        """PMAPI - Return value from environment or pcp config file """
        errstr = ctypes.create_string_buffer(c_api.PM_MAXERRMSGLEN)
        result = LIBPCP.pmErrStr_r(code, errstr, c_api.PM_MAXERRMSGLEN)
        return str(result.decode())

    @staticmethod
    def pmExtractValue(valfmt, vlist, intype, outtype):
        """PMAPI - Extract a value from a pmValue struct and convert its type

        pmAtomValue = pmExtractValue(results.contents.get_valfmt(i),
                                     results.contents.get_vlist(i, 0),
                                     descs[i].contents.type,
                                     c_api.PM_TYPE_FLOAT)
        """
        outAtom = pmAtomValue()
        status = LIBPCP.pmExtractValue(valfmt, vlist, intype,
                                        byref(outAtom), outtype)
        if status < 0:
            raise pmErr(status)

        if outtype == c_api.PM_TYPE_STRING:
            # Get pointer to C string
            c_str = c_char_p()
            memmove(byref(c_str), addressof(outAtom) + pmAtomValue.cp.offset,
                    sizeof(c_char_p))
            # Convert to a python string and have result point to it
            outAtom.cp = outAtom.cp
            # Free the C string
            LIBC.free(c_str)
        return outAtom

    @staticmethod
    def pmConvScale(inType, inAtom, desc, metric_idx, outUnits):
        """PMAPI - Convert a value to a different scale

        pmAtomValue = pmConvScale(c_api.PM_TYPE_FLOAT, pmAtomValue,
                                            pmDesc*, 3, c_api.PM_SPACE_MBYTE)
        """
        if type(outUnits) == type(int()):
            pmunits = pmUnits()
            pmunits.dimSpace = 1
            pmunits.scaleSpace = outUnits
        else:
            pmunits = outUnits
        outAtom = pmAtomValue()
        status = LIBPCP.pmConvScale(inType, byref(inAtom),
                                    byref(desc[metric_idx].contents.units),
                                    byref(outAtom), byref(pmunits))
        if status < 0:
            raise pmErr(status)
        return outAtom

    @staticmethod
    def pmUnitsStr(units):
        """PMAPI - Convert units struct to a readable string """
        unitstr = ctypes.create_string_buffer(64)
        result = LIBPCP.pmUnitsStr_r(units, unitstr, 64)
        return str(result.decode())

    @staticmethod
    def pmNumberStr(value):
        """PMAPI - Convert double value to fixed-width string """
        numstr = ctypes.create_string_buffer(8)
        result = LIBPCP.pmNumberStr_r(value, numstr, 8)
        return str(result.decode())

    @staticmethod
    def pmIDStr(pmid):
        """PMAPI - Convert a pmID to a readable string """
        pmidstr = ctypes.create_string_buffer(32)
        result = LIBPCP.pmIDStr_r(pmid, pmidstr, 32)
        return str(result.decode())

    @staticmethod
    def pmInDomStr(pmdescp):
        """PMAPI - Convert an instance domain ID  to a readable string
        "indom" = pmGetInDom(pmDesc pmdesc)
        """
        indomstr = ctypes.create_string_buffer(32)
        result = LIBPCP.pmInDomStr_r(get_indom(pmdescp), indomstr, 32)
        return str(result.decode())

    @staticmethod
    def pmTypeStr(typed):
        """PMAPI - Convert a performance metric type to a readable string
        "type" = pmTypeStr(c_api.PM_TYPE_FLOAT)
        """
        typestr = ctypes.create_string_buffer(32)
        result = LIBPCP.pmTypeStr_r(typed, typestr, 32)
        return str(result.decode())

    @staticmethod
    def pmAtomStr(atom, typed):
        """PMAPI - Convert a value atom to a readable string
        "value" = pmAtomStr(atom, c_api.PM_TYPE_U32)
        """
        atomstr = ctypes.create_string_buffer(96)
        result = LIBPCP.pmAtomStr_r(byref(atom), typed, atomstr, 96)
        return str(result.decode())

    @staticmethod
    def pmPrintValue(fileObj, result, ptype, vset_idx, vlist_idx, min_width):
        """PMAPI - Print the value of a metric
        pmPrintValue(file, value, pmdesc, vset_index, vlist_index, min_width)
        """
        LIBPCP.pmPrintValue(pyFileToCFile(fileObj),
                c_int(result.contents.vset[vset_idx].contents.valfmt),
                c_int(ptype.contents.type),
                byref(result.contents.vset[vset_idx].contents.vlist[vlist_idx]),
                min_width)

    @staticmethod
    def pmflush():
        """PMAPI - flush the internal buffer shared with pmprintf """
        status = LIBPCP.pmflush()
        if status < 0:
            raise pmErr(status)
        return status

    @staticmethod
    def pmprintf(fmt, *args):
        """PMAPI - append message to internal buffer for later printing """
        status = LIBPCP.pmprintf(fmt, *args)
        if status < 0:
            raise pmErr(status)

    @staticmethod
    def pmSortInstances(result_p):
        """PMAPI - sort all metric instances in result returned by pmFetch """
        LIBPCP.pmSortInstances(result_p)
        return None

    @staticmethod
    def pmParseInterval(interval):
        """PMAPI - parse a textual time interval into a timeval struct
        (timeval_ctype, '') = pmParseInterval("time string")
        """
        return (timeval.fromInterval(interval), '')

    @staticmethod
    def pmParseMetricSpec(string, isarch = 0, source = ''):
        """PMAPI - parse a textual metric specification into a struct
        (result, '') = pmParseMetricSpec("hinv.ncpu", 0, "localhost")
        """
        return (pmMetricSpec.fromString(string, isarch, source), '')

    @staticmethod
    def pmParseUnitsStr(string):
        if type(string) != type(u'') and type(string) != type(b''):
            raise pmErr(c_api.PM_ERR_CONV, str(string))
        if type(string) != type(b''):
            string = string.encode('utf-8')
        result = pmUnits()
        errmsg = c_char_p()
        multiplier = c_double()
        status = LIBPCP.pmParseUnitsStr(string, byref(result), byref(multiplier), byref(errmsg))
        if status < 0:
            text = str(errmsg.value.decode())
            LIBC.free(errmsg)
            raise pmErr(status, text)
        return (result, multiplier.value)

    @staticmethod
    def pmtimevalSleep(tvp):
        """ Delay for a specified amount of time (timeval).
            Useful for implementing tools that do metric sampling.
            Single arg is timeval in tuple returned from pmParseInterval().
        """
        return tvp.sleep()

