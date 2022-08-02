""" Wrapper module for LIBPCP - the core Performace Co-Pilot API
#
# Copyright (C) 2012-2022 Red Hat
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
# https://pcp.io
#
# Performance Co-Pilot Programmer's Guide
# cf. Chapter 3. PMAPI - The Performance Metrics API
#
# EXAMPLE

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


    # ... or, using the fetchgroup interface:

    from pcp import pmapi
    import cpmapi as c_api

    pmfg = pmapi.fetchgroup(c_api.PM_CONTEXT_HOST, "local:")
    v = pmfg.extend_item("hinv.ncpu")
    vv = pmfg.extend_indom("kernel.all.load", c_api.PM_TYPE_FLOAT)
    vvv = pmfg.extend_event("systemd.journal.records", field="systemd.journal.field.string")
    t = pmfg.extend_timestamp()

    pmfg.fetch()
    print("time: %s" % t())
    print("number of cpus: %d" % v())
    for icode, iname, value in vv():
        print("load average %s: %f" % (iname, value()))
    for ts, line in vvv():
        print("%s : %s" % (ts, line()))
"""
# pylint: disable=missing-docstring,line-too-long,broad-except,no-member
# pylint: disable=too-many-lines,too-many-arguments,too-many-nested-blocks

import os
import sys
import time
import errno
import datetime
import json

# constants adapted from C header file <pcp/pmapi.h>
import cpmapi as c_api

# for interfacing with LIBPCP - the client-side C API
import ctypes
from ctypes import c_char, c_int, c_uint, c_long, c_char_p, c_void_p
from ctypes import c_float, c_double, c_int32, c_uint32, c_int64, c_uint64
from ctypes import CDLL, POINTER, CFUNCTYPE, Structure, Union
from ctypes import addressof, pointer, sizeof, cast, byref
from ctypes.util import find_library


##############################################################################
#
# dynamic library loads
#
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
    text_type = str
else:
    integer_types = (int, long,)
    text_type = unicode # pylint: disable=undefined-variable

def pyFileToCFile(fileObj):
    if sys.version >= '3':
        ctypes.pythonapi.PyObject_AsFileDescriptor.restype = c_int
        ctypes.pythonapi.PyObject_AsFileDescriptor.argtypes = [ctypes.py_object]
        return os.fdopen(ctypes.pythonapi.PyObject_AsFileDescriptor(fileObj), "r", closefd=False)
    else:
        ctypes.pythonapi.PyFile_AsFile.restype = c_void_p
        ctypes.pythonapi.PyFile_AsFile.argtypes = [ctypes.py_object]
        return ctypes.pythonapi.PyFile_AsFile(fileObj)


##############################################################################
#
# definition of exception classes
#

class pmErr(Exception):
    def __init__(self, *args):
        super(pmErr, self).__init__(*args)
        self.args = list(args)
        if args and isinstance(args[0], int):
            self.code = args[0]
        else:
            self.code = 0

    def __str__(self):
        errSym = None
        try:
            errSym = c_api.pmErrSymDict[self.code]
        except KeyError:
            pass
        if errSym is None:
            return self.message()
        return "%s %s" % (errSym, self.message())

    def message(self):
        errStr = ctypes.create_string_buffer(c_api.PM_MAXERRMSGLEN)
        errStr = LIBPCP.pmErrStr_r(self.code, errStr, c_api.PM_MAXERRMSGLEN)
        result = str(errStr.decode())
        for index in range(1, len(self.args)):
            result += " " + str(self.args[index])
        return result

    def progname(self):
        return str(c_char_p.in_dll(LIBPCP, "pmProgname").value.decode())

    def errno(self):
        return self.code

class pmUsageErr(Exception):
    def __init__(self, *args):
        super(pmUsageErr, self).__init__(*args)
        self.args = list(args)

    def message(self):
        for index in range(0, len(self.args)): # pylint: disable=C0200
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

# These hardcoded decls should be derived from <sys/time.h>, but no such
# ctypes facility exists.  Particularly problematic is the tv_usec field
# (which POSIX defines as having type suseconds_t) - this can be 32 bits
# on some 64 bit platforms (hence c_long is not always correct).

if c_api.PM_SIZEOF_SUSECONDS_T == 4:
    c_suseconds_t = c_int32
elif c_api.PM_SIZEOF_SUSECONDS_T == 8:
    c_suseconds_t = c_int64
else:
    raise pmErr(c_api.PM_ERR_CONV, "Unexpected suseconds_t size")

if c_api.PM_SIZEOF_TIME_T == 4:
    c_time_t = c_int32
elif c_api.PM_SIZEOF_TIME_T == 8:
    c_time_t = c_int64
else:
    raise pmErr(c_api.PM_ERR_CONV, "Unexpected time_t size")

class timeval(Structure):
    _fields_ = [("tv_sec", c_time_t),
                ("tv_usec", c_suseconds_t)]

    def __init__(self, sec=0, usec=0):
        Structure.__init__(self)
        self.tv_sec = sec
        self.tv_usec = usec

    @classmethod
    def fromInterval(cls, interval):
        """ Construct timeval from a string using pmParseInterval """
        tvp = cls()
        errmsg = c_char_p()
        if not isinstance(interval, bytes):
            interval = interval.encode('utf-8')
        status = LIBPCP.pmParseInterval(interval, byref(tvp), byref(errmsg))
        if status < 0:
            raise pmErr(status, errmsg)
        return tvp

    def __str__(self):
        return "%.3f" % c_api.pmtimevalToReal(self.tv_sec, self.tv_usec)

    def __float__(self):
        return float(c_api.pmtimevalToReal(self.tv_sec, self.tv_usec))

    def __complex__(self):
        return complex(c_api.pmtimevalToReal(self.tv_sec, self.tv_usec))

    def __index__(self):
        return int(self.tv_sec)

    def __long__(self):
        return long(self.tv_sec)

    def __int__(self):
        return int(self.tv_sec)

    def sleep(self):
        """ Delay for the amount of time specified by this timeval. """
        time.sleep(float(self))

class timespec(Structure):
    _fields_ = [("tv_sec", c_time_t),
                ("tv_nsec", c_long)]

    def __init__(self, sec=0, nsec=0):
        Structure.__init__(self)
        self.tv_sec = sec
        self.tv_nsec = nsec

    @classmethod
    def fromInterval(cls, interval):
        """ Construct timeval from a string using pmParseInterval """
        tsp = cls()
        errmsg = c_char_p()
        if not isinstance(interval, bytes):
            interval = interval.encode('utf-8')
        status = LIBPCP.pmParseHighResInterval(interval, byref(tsp), byref(errmsg))
        if status < 0:
            raise pmErr(status, errmsg)
        return tsp

    def __str__(self):
        return "%.3f" % c_api.pmtimespecToReal(self.tv_sec, self.tv_nsec)

    def __float__(self):
        return float(c_api.pmtimespecToReal(self.tv_sec, self.tv_nsec))

    def __complex__(self):
        return complex(c_api.pmtimespecToReal(self.tv_sec, self.tv_nsec))

    def __index__(self):
        return int(self.tv_sec)

    def __long__(self):
        return long(self.tv_sec)

    def __int__(self):
        return int(self.tv_sec)

    def sleep(self):
        """ Delay for the amount of time specified by this timespec. """
        time.sleep(float(self))

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
                ("tm_gmtoff", c_long),  # glibc/bsd extension
                ("tm_zone", c_char_p)]  # glibc/bsd extension

    def struct_time(self):
        # convert POSIX representations - see mktime(3) - to python:
        # https://docs.python.org/3/library/time.html#time.struct_time
        pywday = self.tm_wday - 1
        if pywday < 0:
            pywday = 6
        stlist = [self.tm_year + 1900, self.tm_mon + 1, self.tm_mday,
                  self.tm_hour, self.tm_min, self.tm_sec,
                  pywday, self.tm_yday - 1, self.tm_isdst]
        return time.struct_time(stlist)

    def __str__(self):
        # For debugging this, the timetuple is possibly more useful
        # timetuple = (self.tm_year+1900, self.tm_mon, self.tm_mday,
        #              self.tm_hour, self.tm_min, self.tm_sec,
        #              self.tm_wday, self.tm_yday, self.tm_isdst)
        # return str(timetuple)

        # For regular users manipulating struct tm, pretty-print it
        result = ctypes.create_string_buffer(32)
        second = c_api.pmMktime(self.tm_sec, self.tm_min, self.tm_hour,
                                self.tm_mday, self.tm_mon, self.tm_year,
                                self.tm_wday, self.tm_yday, self.tm_isdst,
                                long(self.tm_gmtoff), str(self.tm_zone))
        timetp = c_long(long(second))
        LIBPCP.pmCtime(byref(timetp), result)
        return str(result.value.decode()).rstrip()

class pmAtomValue(Union):
    """Union used for unpacking metric values according to type

    Constants for specifying metric types are defined in module pmapi
    """
    _fields_ = [("l", c_int32),
                ("ul", c_uint32),
                ("ll", c_int64),
                ("ull", c_uint64),
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
                  c_api.PM_TYPE_EVENT : lambda x: None,
                  c_api.PM_TYPE_HIGHRES_EVENT : lambda x: None,
                  c_api.PM_TYPE_NOSUPPORT : lambda x: None,
                  c_api.PM_TYPE_UNKNOWN : lambda x: None
                 }

    def dref(self, typed):
        value = self._atomDrefD[typed](self)
        if typed == c_api.PM_TYPE_STRING:
            try:
                value = str(value.decode('utf-8'))
            except Exception:
                value = str(value)
        return value

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

    def __int__(self):
        return c_api.pmUnits_int(self.dimSpace, self.dimTime, self.dimCount,
                                 self.scaleSpace, self.scaleTime, self.scaleCount)

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
pmValueSetPtr.pmid = property(lambda x: x.contents.pmid, None, None, None)
pmValueSetPtr.numval = property(lambda x: x.contents.numval, None, None, None)
pmValueSetPtr.valfmt = property(lambda x: x.contents.valfmt, None, None, None)
pmValueSetPtr.vlist = property(lambda x: x.contents.vlist, None, None, None)


class pmResult(Structure):
    """Structure returned by pmFetch, with a value set for each metric queried

    The vset is defined with a "fake" array bounds of 1, which can give runtime
    array bounds complaints.  The getter methods are array bounds agnostic.
    """
    _fields_ = [("timestamp", timeval),
                ("numpmid", c_int),
                # array N of pointer to pmValueSet
                ("vset", (POINTER(pmValueSet)) * 1)]
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

class pmHighResResult(Structure):
    """Structure returned by pmFetchHighRes, uses struct timespec for time
    """
    _fields_ = [("timestamp", timespec),
                ("numpmid", c_int),
                # array N of pointer to pmValueSet
                ("vset", (POINTER(pmValueSet)) * 1)]
    def __init__(self):
        Structure.__init__(self)
        self.numpmid = 0

    def __str__(self):
        vals = range(self.numpmid)
        vstr = str([" %s" % str(self.vset[i].contents) for i in vals])
        return "pmHighResResult@%#lx id#=%d " % (addressof(self), self.numpmid) + vstr

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
                ("units", pmUnits)]
    def __str__(self):
        fields = (addressof(self), self.pmid, self.type)
        return "pmDesc@%#lx id=%#lx type=%d" % fields

pmDescPtr = POINTER(pmDesc)
pmDescPtr.sem = property(lambda x: x.contents.sem, None, None, None)
pmDescPtr.type = property(lambda x: x.contents.type, None, None, None)
pmDescPtr.indom = property(lambda x: x.contents.indom, None, None, None)
pmDescPtr.units = property(lambda x: x.contents.units, None, None, None)


def get_indom(pmdesc):
    """Internal function to extract an indom from a pmdesc

       Allow functions requiring an indom to be passed a pmDesc* instead
    """
    class Value(Union):
        _fields_ = [("pval", POINTER(pmDesc)),
                    ("lval", c_uint)]
    if isinstance(pmdesc, POINTER(pmDesc)):
        return pmdesc.contents.indom
    else:           # raw indom
        # Goodness, there must be a simpler way to do this
        value = Value()
        value.pval = pmdesc
        return value.lval

class pmMetricSpec(Structure):
    """Structure describing a metric's specification """
    _fields_ = [("isarch", c_int),
                ("source", c_char_p),
                ("metric", c_char_p),
                ("ninst", c_int),
                ("inst", POINTER(c_char_p))]
    csbuffer = None     # saved source buffer (ensures correct refcount)

    def __str__(self):
        insts = list(map(lambda x: str(self.inst[x]), range(self.ninst)))
        fields = (addressof(self), self.isarch, self.source, insts)
        return "pmMetricSpec@%#lx src=%s metric=%s insts=%s" % fields

    @classmethod
    def fromString(cls, string, isarch=0, source=''):
        result = POINTER(cls)()
        errmsg = c_char_p()
        if not isinstance(source, bytes):
            source = source.encode('utf-8')
        if not isinstance(string, bytes):
            string = string.encode('utf-8')
        cstring = ctypes.create_string_buffer(string)
        csource = ctypes.create_string_buffer(source)
        status = LIBPCP.pmParseMetricSpec(cstring, isarch, csource,
                                          byref(result), byref(errmsg))
        if status < 0:
            raise pmErr(status, errmsg)
        # pmParseMetricSpec may embed a pointer to this buffer in result
        result.csbuffer = csource   # keep a reference until destruction
        return result

class pmLogLabel(Structure):
    """Label record at the start of every (v2) log file """
    _fields_ = [("magic", c_int),
                ("pid_t", c_int),
                ("start", timeval),
                ("hostname", c_char * c_api.PM_LOG_MAXHOSTLEN),
                ("tz", c_char * c_api.PM_TZ_MAXLEN)]

    def get_hostname(self):
        """ Return the hostname from the structure as native str """
        return str(self.hostname.decode())

    def get_timezone(self):
        """ Return the timezone from the structure as native str """
        return str(self.tz.decode())

class pmHighResLogLabel(Structure):
    """Label record at the start of every (v3) log file """
    _fields_ = [("magic", c_int),
                ("pid_t", c_int),
                ("start", timespec),
                ("hostname", c_char * c_api.PM_MAX_HOSTNAMELEN),
                ("timezone", c_char * c_api.PM_MAX_TIMEZONELEN),
                ("zoneinfo", c_char * c_api.PM_MAX_ZONEINFOLEN)]

    def get_hostname(self):
        """ Return the hostname from the structure as native str """
        return str(self.hostname.decode())

    def get_timezone(self):
        """ Return the timezone from the structure as native str """
        return str(self.timezone.decode())

    def get_zoneinfo(self):
        """ Return the zoneinfo from the structure as native str """
        return str(self.zoneinfo.decode())

class pmLabel(Structure):
    """Structure describing label's specification"""
    _fields_ = [("name", c_int, 16),
                ("namelen", c_int, 8),
                ("flags", c_int, 8),
                ("value", c_int, 16),
                ("valuelen", c_int, 16)]
    def __str__(self):
        return self.name + ":" + '"' + self.value + '"'

pmLabelPtr = POINTER(pmLabel)
pmLabelPtr.name = property(lambda x: x.contents.name, None, None, None)
pmLabelPtr.namelen = property(lambda x: x.contents.namelen, None, None, None)
pmLabelPtr.flags = property(lambda x: x.contents.flags, None, None, None)
pmLabelPtr.value = property(lambda x: x.contents.value, None, None, None)
pmLabelPtr.valuelen = property(lambda x: x.contents.valuelen, None, None, None)

class pmLabelSet(Structure):
    """ Structure describing label set specifications"""
    _fields_ = [("inst", c_uint),
                ("nlabels", c_int),
                ("json", c_char_p),
                ("jsonlen", c_int, 16),
                ("padding", c_int, 15),
                ("compound", c_int, 1),
                ("labels", POINTER(pmLabel)),
                ("hash", c_void_p)]
    def __str__(self):
        if self.json is None:
            return "{}"
        return self.json.decode()

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

LIBPCP.pmRegisterDerivedMetric.restype = c_int
LIBPCP.pmRegisterDerivedMetric.argtypes = [c_char_p, c_char_p, POINTER(c_char_p)]

LIBPCP.pmLoadDerivedConfig.restype = c_int
LIBPCP.pmLoadDerivedConfig.argtypes = [c_char_p]


##
# PMAPI Metrics Description Services

LIBPCP.pmLookupDesc.restype = c_int
LIBPCP.pmLookupDesc.argtypes = [c_uint, POINTER(pmDesc)]

LIBPCP.pmLookupDescs.restype = c_int
LIBPCP.pmLookupDescs.argtypes = [c_int, POINTER(c_uint), POINTER(pmDesc)]

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

LIBPCP.pmSetModeHighRes.restype = c_int
LIBPCP.pmSetModeHighRes.argtypes = [c_int, POINTER(timespec), POINTER(timespec)]

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

LIBPCP.pmFetchHighRes.restype = c_int
LIBPCP.pmFetchHighRes.argtypes = [c_int, POINTER(c_uint), POINTER(POINTER(pmHighResResult))]
# old name preserved for backwards compatibility
LIBPCP.pmHighResFetch.restype = c_int
LIBPCP.pmHighResFetch.argtypes = [c_int, POINTER(c_uint), POINTER(POINTER(pmHighResResult))]

LIBPCP.pmFreeResult.restype = None
LIBPCP.pmFreeResult.argtypes = [POINTER(pmResult)]

LIBPCP.pmFreeHighResResult.restype = None
LIBPCP.pmFreeHighResResult.argtypes = [POINTER(pmHighResResult)]

LIBPCP.pmStore.restype = c_int
LIBPCP.pmStore.argtypes = [POINTER(pmResult)]

LIBPCP.pmStoreHighRes.restype = c_int
LIBPCP.pmStoreHighRes.argtypes = [POINTER(pmHighResResult)]


##
# PMAPI Archive-Specific Services

LIBPCP.pmGetArchiveLabel.restype = c_int
LIBPCP.pmGetArchiveLabel.argtypes = [POINTER(pmLogLabel)]

LIBPCP.pmGetArchiveEnd.restype = c_int
LIBPCP.pmGetArchiveEnd.argtypes = [POINTER(timeval)]

LIBPCP.pmGetHighResArchiveLabel.restype = c_int
LIBPCP.pmGetHighResArchiveLabel.argtypes = [POINTER(pmHighResLogLabel)]

LIBPCP.pmGetHighResArchiveEnd.restype = c_int
LIBPCP.pmGetHighResArchiveEnd.argtypes = [POINTER(timespec)]

LIBPCP.pmGetInDomArchive.restype = c_int
LIBPCP.pmGetInDomArchive.argtypes = [c_uint, POINTER(POINTER(c_int)),
                                     POINTER(POINTER(c_char_p))]

LIBPCP.pmLookupInDomArchive.restype = c_int
LIBPCP.pmLookupInDom.argtypes = [c_uint, c_char_p]
LIBPCP.pmLookupInDomArchive.argtypes = [pmInDom, c_char_p]

LIBPCP.pmNameInDomArchive.restype = c_int
LIBPCP.pmNameInDomArchive.argtypes = [pmInDom, c_int]

LIBPCP.pmFetchArchive.restype = c_int
LIBPCP.pmFetchArchive.argtypes = [POINTER(POINTER(pmResult))]

LIBPCP.pmFetchHighResArchive.restype = c_int
LIBPCP.pmFetchHighResArchive.argtypes = [POINTER(POINTER(pmHighResResult))]


##
# PMAPI Ancilliary Support Services

LIBPCP.pmGetOptionalConfig.restype = c_char_p
LIBPCP.pmGetOptionalConfig.argtypes = [c_char_p]

LIBPCP.pmErrStr_r.restype = c_char_p
LIBPCP.pmErrStr_r.argtypes = [c_int, c_char_p, c_int]

LIBPCP.pmExtractValue.restype = c_int
LIBPCP.pmExtractValue.argtypes = [
    c_int, POINTER(pmValue), c_int, POINTER(pmAtomValue), c_int]

LIBPCP.pmConvScale.restype = c_int
LIBPCP.pmConvScale.argtypes = [
    c_int, POINTER(pmAtomValue), POINTER(pmUnits), POINTER(pmAtomValue),
    POINTER(pmUnits)]

LIBPCP.pmUnitsStr_r.restype = c_char_p
LIBPCP.pmUnitsStr_r.argtypes = [POINTER(pmUnits), c_char_p, c_int]

LIBPCP.pmNumberStr_r.restype = c_char_p
LIBPCP.pmNumberStr_r.argtypes = [c_double, c_char_p, c_int]

LIBPCP.pmID_build.restype = c_uint
LIBPCP.pmID_build.argtypes = [c_uint, c_uint, c_uint]

LIBPCP.pmID_domain.restype = c_uint
LIBPCP.pmID_domain.argtypes = [c_uint]

LIBPCP.pmID_cluster.restype = c_uint
LIBPCP.pmID_cluster.argtypes = [c_uint]

LIBPCP.pmID_item.restype = c_uint
LIBPCP.pmID_item.argtypes = [c_uint]

LIBPCP.pmIDStr_r.restype = c_char_p
LIBPCP.pmIDStr_r.argtypes = [c_uint, c_char_p, c_int]

LIBPCP.pmInDom_build.restype = c_uint
LIBPCP.pmInDom_build.argtypes = [c_uint, c_uint]

LIBPCP.pmInDom_domain.restype = c_uint
LIBPCP.pmInDom_domain.argtypes = [c_uint]

LIBPCP.pmInDom_serial.restype = c_uint
LIBPCP.pmInDom_serial.argtypes = [c_uint]

LIBPCP.pmInDomStr_r.restype = c_char_p
LIBPCP.pmInDomStr_r.argtypes = [c_uint, c_char_p, c_int]

LIBPCP.pmTypeStr_r.restype = c_char_p
LIBPCP.pmTypeStr_r.argtypes = [c_int, c_char_p, c_int]

LIBPCP.pmAtomStr_r.restype = c_char_p
LIBPCP.pmAtomStr_r.argtypes = [POINTER(pmAtomValue), c_int, c_char_p, c_int]

LIBPCP.pmSemStr_r.restype = c_char_p
LIBPCP.pmSemStr_r.argtypes = [c_int, c_char_p, c_int]

LIBPCP.pmPrintValue.restype = None
LIBPCP.pmPrintValue.argtypes = [c_void_p, c_int, c_int, POINTER(pmValue), c_int]

LIBPCP.pmParseHighResInterval.restype = c_int
LIBPCP.pmParseHighResInterval.argtypes = [c_char_p, POINTER(timespec),
                                          POINTER(c_char_p)]

LIBPCP.pmParseInterval.restype = c_int
LIBPCP.pmParseInterval.argtypes = [c_char_p, POINTER(timeval),
                                   POINTER(c_char_p)]

LIBPCP.pmParseMetricSpec.restype = c_int
LIBPCP.pmParseMetricSpec.argtypes = [c_char_p, c_int, c_char_p,
                                     POINTER(POINTER(pmMetricSpec)),
                                     POINTER(c_char_p)]

LIBPCP.pmflush.restype = c_int
LIBPCP.pmflush.argtypes = []

LIBPCP.pmprintf.restype = c_int
LIBPCP.pmprintf.argtypes = [c_char_p]

LIBPCP.pmSortHighResInstances.restype = None
LIBPCP.pmSortHighResInstances.argtypes = [POINTER(pmHighResResult)]

LIBPCP.pmSortInstances.restype = None
LIBPCP.pmSortInstances.argtypes = [POINTER(pmResult)]

LIBPCP.pmLookupLabels.restype = c_int
LIBPCP.pmLookupLabels.argtypes = [c_int, POINTER(POINTER(pmLabelSet))]

LIBPCP.pmGetInstancesLabels.restype = c_int
LIBPCP.pmGetInstancesLabels.argtypes = [c_int, POINTER(POINTER(pmLabelSet))]

LIBPCP.pmGetItemLabels.restype = c_int
LIBPCP.pmGetItemLabels.argtypes = [c_int, POINTER(POINTER(pmLabelSet))]

LIBPCP.pmGetClusterLabels.restype = c_int
LIBPCP.pmGetClusterLabels.argtypes = [c_int, POINTER(POINTER(pmLabelSet))]

LIBPCP.pmGetInDomLabels.restype = c_int
LIBPCP.pmGetInDomLabels.argtypes = [c_int, POINTER(POINTER(pmLabelSet))]

LIBPCP.pmGetDomainLabels.restype = c_int
LIBPCP.pmGetDomainLabels.argtypes = [c_int, POINTER(POINTER(pmLabelSet))]

LIBPCP.pmGetContextLabels.restype = c_int
LIBPCP.pmGetContextLabels.argtypes = [POINTER(POINTER(pmLabelSet))]

LIBPCP.pmMergeLabels.restype = c_int
LIBPCP.pmMergeLabels.argtypes = [POINTER(c_char_p), c_int, c_char_p, c_int]

mergeLabelSetsCB_type = CFUNCTYPE(c_int, POINTER(pmLabel), c_char_p, c_void_p)
LIBPCP.pmMergeLabelSets.restype = c_int
LIBPCP.pmMergeLabelSets.argtypes = [POINTER(POINTER(pmLabelSet)), c_int,
                                    c_char_p, c_int, mergeLabelSetsCB_type,
                                    c_void_p]

LIBPCP.pmFreeLabelSets.restype = None
LIBPCP.pmFreeLabelSets.argtypes = [POINTER(pmLabelSet), c_int]

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

    def __init__(self, short_options=None, short_usage=None, flags=0):
        c_api.pmResetAllOptions()
        if short_options is not None:
            c_api.pmSetShortOptions(short_options)
        if short_usage is not None:
            c_api.pmSetShortUsage(short_usage)
        if flags != 0:
            c_api.pmSetOptionFlags(flags)
        else:   # good default for scripts - always evaluating log bounds
            c_api.pmSetOptionFlags(c_api.PM_OPTFLAG_BOUNDARIES)
        self._delta = 1         # default archive pmSetMode delta
        self._mode = c_api.PM_MODE_INTERP # default pmSetMode access mode
        self._need_reset = False    # flag for __del__ memory reclaim

    def __del__(self):
        if LIBPCP is not None and self._need_reset:
            c_api.pmResetAllOptions()

    @staticmethod
    def daemonize():
        """ Cross-platform --daemonize (re-parent to init) option helper """
        return c_api.pmServerStart()

    ##
    # general command line option access and manipulation

    def pmGetOptionFlags(self):
        return c_api.pmGetOptionFlags()

    def pmSetOptionContext(self, context):
        return c_api.pmSetOptionContext(context)

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
        """ Set sampling interval (pmParseHighResInterval string) """
        return c_api.pmSetOptionInterval(interval)

    def pmGetOperands(self):
        """ After a pmGetOptions(3) call has been made this method
            returns a list of any remaining parameters which were
            not parsed as command line options, aka "operands".
            http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap03.html#tag_03_254
        """
        return c_api.pmGetOperands()

    # Deprecated, use pmGetOperands() above instead
    def pmGetNonOptionsFromList(self, argv):
        return c_api.pmGetNonOptionsFromList(argv)

    # Deprecated, use pmGetOperands() above instead
    def pmNonOptionsFromList(self, argv):
        return c_api.pmGetNonOptionsFromList(argv)

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
            its short option form (str), and two usage message hints (argname
            (str) and message (str) - see pmGetOptions(3) for details).
        """
        if short_opt is None:
            short_opt = ''
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
        """ Unimplemented """
        return None

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
        """ Unimplemented """
        return None

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

    def pmSetLongOptionContainer(self):
        """ Add support for --container into PMAPI monitor tool """
        return c_api.pmSetLongOptionContainer()

    def pmSetLongOptionHostList(self):
        """ Add support for --host-list into PMAPI monitor tool """
        return c_api.pmSetLongOptionHostList()

    def pmGetOptionContext(self):   # int (typed)
        return c_api.pmGetOptionContext()

    def pmGetOptionHosts(self):     # str list
        return c_api.pmGetOptionHosts()

    def pmGetOptionArchives(self):  # str list
        return c_api.pmGetOptionArchives()

    def pmGetOptionAlignment(self): # timeval
        alignment = c_api.pmGetOptionAlign_optarg()
        if alignment is None:
            return None
        return timeval.fromInterval(alignment)

    def pmGetOptionHighResAlignment(self): # timespec
        alignment = c_api.pmGetOptionAlign_optarg()
        if alignment is None:
            return None
        return timespec.fromInterval(alignment)

    def pmGetOptionStart(self):     # timeval
        sec = c_api.pmGetOptionStart_sec()
        if sec is None:
            return None
        return timeval(sec, c_api.pmGetOptionStart_usec())

    def pmGetOptionHighResStart(self):  # timespec
        sec = c_api.pmGetOptionStart_sec()
        if sec is None:
            return None
        return timespec(sec, c_api.pmGetOptionStart_nsec())

    def pmGetOptionAlignOptarg(self):   # string
        return c_api.pmGetOptionAlign_optarg()

    def pmGetOptionFinishOptarg(self):  # string
        return c_api.pmGetOptionFinish_optarg()

    def pmGetOptionFinish(self):    # timeval
        sec = c_api.pmGetOptionFinish_sec()
        if sec is None:
            return None
        return timeval(sec, c_api.pmGetOptionFinish_usec())

    def pmGetOptionHighResFinish(self): # timespec
        sec = c_api.pmGetOptionFinish_sec()
        if sec is None:
            return None
        return timespec(sec, c_api.pmGetOptionFinish_nsec())

    def pmGetOptionOrigin(self):        # timeval
        sec = c_api.pmGetOptionOrigin_sec()
        if sec is None:
            return None
        return timeval(sec, c_api.pmGetOptionOrigin_usec())

    def pmGetOptionHighResOrigin(self): # timespec
        sec = c_api.pmGetOptionOrigin_sec()
        if sec is None:
            return None
        return timespec(sec, c_api.pmGetOptionOrigin_nsec())

    def pmGetOptionInterval(self):      # timeval
        sec = c_api.pmGetOptionInterval_sec()
        if sec is None:
            return None
        return timeval(sec, c_api.pmGetOptionInterval_usec())

    def pmGetOptionHighResInterval(self): # timespec
        sec = c_api.pmGetOptionInterval_sec()
        if sec is None:
            return None
        return timespec(sec, c_api.pmGetOptionInterval_nsec())

    def pmGetOptionSamples(self):   # int
        return c_api.pmGetOptionSamples()

    def pmGetOptionHostZone(self):  # boolean
        if c_api.pmGetOptionHostZone() == 0:
            return False
        return True

    def pmGetOptionTimezone(self):  # str
        return c_api.pmGetOptionTimezone()

    def pmGetOptionContainer(self): # str
        return c_api.pmGetOptionContainer()

    def pmGetOptionLocalPMDA(self):        # boolean
        if c_api.pmGetOptionLocalPMDA() == 0:
            return False
        return True

    def pmSetOptionArchive(self, archive):  # str
        return c_api.pmSetOptionArchive(archive)

    def pmSetOptionArchiveList(self, archives): # str
        return c_api.pmSetOptionArchiveList(archives)

    def pmSetOptionArchiveFolio(self, folio):   # str
        return c_api.pmSetOptionArchiveFolio(folio)

    def pmSetOptionContainer(self, container):  # str
        return c_api.pmSetOptionContainer(container)

    def pmSetOptionHost(self, host):    # str
        return c_api.pmSetOptionHost(host)

    def pmSetOptionHostList(self, hosts):   # str
        return c_api.pmSetOptionHostList(hosts)

    def pmSetOptionSpecLocal(self, spec):        # str
        return c_api.pmSetOptionSpecLocal(spec)

    def pmSetOptionLocalPMDA(self):
        return c_api.pmSetOptionLocalPMDA()


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

    type = property(_R_type, None, None, None)
    target = property(_R_target, None, None, None)
    ctx = property(_R_ctx, None, None, None)

    ##
    # creation and destruction

    def __init__(self, typed=c_api.PM_CONTEXT_HOST, target="local:"):
        self._type = typed                              # the context type
        self._target = target                            # the context target
        self._ctx = c_api.PM_ERR_NOCONTEXT                # init'd pre-connect
        if target and not isinstance(target, bytes):
            source = target.encode('utf-8')
        else:
            source = target
        self._ctx = LIBPCP.pmNewContext(typed, source)     # the context handle
        if self._ctx < 0:
            raise pmErr(self._ctx, [target])

    def __del__(self):
        if LIBPCP is not None and self._ctx != c_api.PM_ERR_NOCONTEXT:
            LIBPCP.pmDestroyContext(self._ctx)

    @classmethod
    def fromOptions(cls, options, argv, typed=0, index=0):
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
        if typed is None or typed <= 0:
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
        elif options.pmGetOptionOrigin() is not None:
            archives = options.pmGetOptionArchives()
            source = archives[index]
            typed = c_api.PM_CONTEXT_ARCHIVE
        else:
            typed = c_api.PM_CONTEXT_HOST
            source = "local:"

        # core work done here - constructs the new pmContext
        context = cls(typed, source)

        # finish time windows, timezones, archive access mode
        if c_api.pmSetContextOptions(context.ctx, options.mode, options.delta):
            raise pmUsageErr

        return context

    @staticmethod
    def set_connect_options(options, source, speclocal):
        """ Helper to set connection options and to get context/source for pmfg. """
        context = None

        if c_api.pmGetOptionArchives():
            context = c_api.PM_CONTEXT_ARCHIVE
            options.pmSetOptionContext(c_api.PM_CONTEXT_ARCHIVE)
            source = options.pmGetOptionArchives()[0]
        elif c_api.pmGetOptionHosts():
            context = c_api.PM_CONTEXT_HOST
            options.pmSetOptionContext(c_api.PM_CONTEXT_HOST)
            source = options.pmGetOptionHosts()[0]
        elif c_api.pmGetOptionLocalPMDA():
            context = c_api.PM_CONTEXT_LOCAL
            options.pmSetOptionContext(c_api.PM_CONTEXT_LOCAL)
            source = None

        if not context:
            if '/' in source:
                context = c_api.PM_CONTEXT_ARCHIVE
                options.pmSetOptionArchive(source)
                options.pmSetOptionContext(c_api.PM_CONTEXT_ARCHIVE)
            elif source != '@':
                context = c_api.PM_CONTEXT_HOST
                options.pmSetOptionHost(source)
                options.pmSetOptionContext(c_api.PM_CONTEXT_HOST)
            else:
                context = c_api.PM_CONTEXT_LOCAL
                options.pmSetOptionLocalPMDA()
                options.pmSetOptionContext(c_api.PM_CONTEXT_LOCAL)
                source = None

        if context == c_api.PM_CONTEXT_LOCAL and speclocal:
            for spec in filter(None, speclocal.split(";")):
                options.pmSetOptionSpecLocal(spec)

        flags = options.pmGetOptionFlags()
        options.pmSetOptionFlags(flags | c_api.PM_OPTFLAG_DONE)
        c_api.pmEndOptions()

        return context, source

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
        if not isinstance(name, bytes):
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
        if not isinstance(name, bytes):
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
        if not isinstance(filename, bytes):
            filename = filename.encode('utf-8')
        status = LIBPCP.pmLoadNameSpace(filename)
        if status < 0:
            raise pmErr(status)
        return status

    def pmLookupName(self, nameA, relaxed=0):
        """PMAPI - Lookup pmIDs from a list of metric names nameA

        c_uint pmid [] = pmLookupName("MetricName")
        c_uint pmid [] = pmLookupName(("MetricName1", "MetricName2", ...))
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        if isinstance(nameA, (str, bytes)):
            n = 1
        else:
            n = len(nameA)
        names = (c_char_p * n)()
        if isinstance(nameA, str):
            names[0] = c_char_p(nameA.encode('utf-8'))
        elif isinstance(nameA, bytes):
            names[0] = c_char_p(nameA)
        else:
            for i, name in enumerate(nameA):
                if isinstance(nameA[i], bytes):
                    names[i] = c_char_p(name)
                else:
                    names[i] = c_char_p(name.encode('utf-8'))
        pmidA = (c_uint * n)()
        LIBPCP.pmLookupName.argtypes = [c_int, (c_char_p * n), POINTER(c_uint)]
        status = LIBPCP.pmLookupName(n, names, pmidA)
        if status < 0:
            raise pmErr(status, pmidA)
        if relaxed == 0 and status != n:
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
        if not isinstance(name, bytes):
            name = name.encode('utf-8')
        if not isinstance(expr, bytes):
            expr = expr.encode('utf-8')
        errmsg = c_char_p()
        result = LIBPCP.pmRegisterDerivedMetric(name, expr, byref(errmsg))
        if result != 0:
            text = str(errmsg.value.decode())
            LIBC.free(errmsg)
            raise pmErr(c_api.PM_ERR_CONV, text)
        status = LIBPCP.pmReconnectContext(self.ctx)
        if status < 0:
            raise pmErr(status)

    def pmLoadDerivedConfig(self, fname):
        """PMAPI - Register derived metric names and definitions from a file
        pm.pmLoadDerivedConfig("FileName")
        """
        if not isinstance(fname, bytes):
            fname = fname.encode('utf-8')
        status = LIBPCP.pmLoadDerivedConfig(fname)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmReconnectContext(self.ctx)
        if status < 0:
            raise pmErr(status)

    # Deprecated, no longer needed, py wrapper uses pmRegisterDerivedMetric(3)
    # and the exception encodes a more complete error message as a result.
    @staticmethod
    def pmDerivedErrStr():
        """PMAPI - Return an error message if the pmRegisterDerived(3) metric
        definition cannot be parsed
        pm.pmRegisterDerived()
        """
        result = LIBPCP.pmDerivedErrStr()
        if result is not None:
            return str(result.decode())
        return None

    ##
    # PMAPI Metrics Description Services

    def pmLookupDesc(self, pmid_p):

        """PMAPI - Lookup a metric description structure from a pmID

        pmDesc* pmdesc = pmLookupDesc(c_uint pmid)
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)

        descbuf = ctypes.create_string_buffer(sizeof(pmDesc))
        desc = cast(descbuf, POINTER(pmDesc))
        pmid = c_uint(pmid_p)
        status = LIBPCP.pmLookupDesc(pmid, desc)
        if status < 0:
            raise pmErr(status)
        return desc

    def pmLookupDescs(self, pmids_p):

        """PMAPI - Lookup metric description structures from pmIDs

        (pmDesc* pmdesc)[] = pmLookupDescs(c_uint pmid[N])
        (pmDesc* pmdesc)[] = pmLookupDescs(c_uint pmid)
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)

        if isinstance(pmids_p, integer_types):
            pmids = (c_uint * 1)()
            pmids[0] = pmids_p
        else:
            pmids = (c_uint * len(pmids_p))()
            for i, pmid in enumerate(pmids_p):
                pmids[i] = c_uint(pmid)

        descsbuf = ctypes.create_string_buffer(len(pmids) * sizeof(pmDesc))
        desclist = cast(descsbuf, POINTER(pmDesc))
        status = LIBPCP.pmLookupDescs(len(pmids), pmids, desclist)
        if status < 0:
            raise pmErr(status)

        # API back-compat - for hysterical raisins, return an array of pointers
        descs = (POINTER(pmDesc) * len(pmids))()
        for i in range(len(pmids)):
            descs[i] = pointer(desclist[i])
        return descs

    def pmLookupInDomText(self, pmdesc, kind=c_api.PM_TEXT_ONELINE):
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

    def pmLookupText(self, pmid, kind=c_api.PM_TEXT_ONELINE):
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

    def pmGetInDom(self, pmdescp=None, indom=None):
        """PMAPI - Lookup the list of instances from an instance domain PMDESCP or indom
        ([instance1, instance2...] [name1, name2...]) pmGetInDom(pmDesc pmdesc)
        """
        if pmdescp is None and indom is None:
            raise pmErr(c_api.PM_ERR_GENERIC, "invalid arguments")

        instA_p = POINTER(c_int)()
        nameA_p = POINTER(c_char_p)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        if indom is None:
            indom = get_indom(pmdescp)
        status = LIBPCP.pmGetInDom(indom, byref(instA_p), byref(nameA_p))
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

    def pmGetInDomDict(self, indom):
        """ helper to return dict of inst:name for an indom """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        retD = {}
        instL, nameL = self.pmGetInDom(indom=indom)
        if instL is None or nameL is None:
            return retD
        n = len(instL)
        for i in range(n):
            retD.update({instL[i]: nameL[i]})
        return retD

    def pmLookupInDom(self, pmdesc, name):
        """PMAPI - Lookup the instance id with the given NAME in the indom

        c_uint instid = pmLookupInDom(pmDesc pmdesc, "Instance")
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        if not isinstance(name, bytes):
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

    def pmDestroyContext(self, handle):
        """PMAPI - NOOP - Destroy a PMAPI context (done in destructor)

        This is unimplemented. The context is destroyed when the pmContext
        object is destroyed.
        """

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
        if isinstance(instL, int):
            numinst = 1
            instA = (c_int * numinst)()
            instA[0] = instL
        elif instL is None or not instL:
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
        if instL is None or not instL:
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

    def pmSetModeHighRes(self, mode, origin, interval):
        """PMAPI - set interpolation mode for reading archive files
        code = pmSetModeHighRes(c_api.PM_MODE_INTERP, timespec, timespec)
        """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        when = None
        if origin is not None:
            when = pointer(origin)
        delta = None
        if interval is not None and interval != 0:
            delta = pointer(interval)
        status = LIBPCP.pmSetModeHighRes(mode, when, delta)
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
        when = None
        if timeVal is not None:
            when = pointer(timeVal)
        status = LIBPCP.pmSetMode(mode, when, delta)
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
        if not isinstance(tz, bytes):
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

    def pmFetchHighRes(self, pmidA):
        """PMAPI - Fetch pmHighResResult from the target source

        pmHighResResult* pmresult = pmFetchHighRes(c_uint pmid[])
        """
        result_p = POINTER(pmHighResResult)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmFetchHighRes(len(pmidA), pmidA, byref(result_p))
        if status < 0:
            raise pmErr(status)
        return result_p

    def pmHighResFetch(self, pmidA):
        """PMAPI - Fetch pmHighResResult from the target source (deprecated)

        pmHighResResult* pmresult = pmHighResFetch(c_uint pmid[])
        """
        result_p = POINTER(pmHighResResult)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmFetchHighRes(len(pmidA), pmidA, byref(result_p))
        if status < 0:
            raise pmErr(status)
        return result_p

    @staticmethod
    def pmFreeResult(result_p):
        """PMAPI - Free a result previously allocated by pmFetch
        pmFreeResult(pmResult* pmresult)
        """
        LIBPCP.pmFreeResult(result_p)

    @staticmethod
    def pmFreeHighResResult(result_p):
        """PMAPI - Free a result previously allocated by pmFetch
        pmFreeHighResResult(pmHighResResult* pmresult)
        """
        LIBPCP.pmFreeHighResResult(result_p)

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

    def pmStoreHighRes(self, result):
        """PMAPI - Set values on target source, inverse of pmFetchHighRes
        pmresult = pmStoreHighRes(pmHighResResult* pmresult)
        """
        LIBPCP.pmStoreHighRes.argtypes = [(type(result))]
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmStoreHighRes(result)
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

    def pmGetHighResArchiveLabel(self):
        """PMAPI - Get the label record from the archive
        loglabel = pmGetHighResArchiveLabel()
        """
        loglabel = pmHighResLogLabel()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmGetHighResArchiveLabel(byref(loglabel))
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

    def pmGetHighResArchiveEnd(self):
        """PMAPI - Get the last recorded timestamp from the archive
        """
        spec = timespec()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmGetHighResArchiveEnd(byref(spec))
        if status < 0:
            raise pmErr(status)
        return spec

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
        if not isinstance(name, bytes):
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
        """PMAPI - Fetch raw measurements from the target source

        pmResult* pmresult = pmFetchArchive()
        """
        result_p = POINTER(pmResult)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmFetchArchive(byref(result_p))
        if status < 0:
            raise pmErr(status)
        return result_p

    def pmFetchHighResArchive(self):
        """PMAPI - Fetch raw measurements from the target source

        pmHighResResult* pmresult = pmFetchHighResArchive()
        """
        result_p = POINTER(pmHighResResult)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmFetchHighResArchive(byref(result_p))
        if status < 0:
            raise pmErr(status)
        return result_p

    def pmlabelset_to_dict(self, lset, flags=0xff):
        """ return a dict of a pmLabelSet, i.e. {name: value, ...}
            flags arg is currently ignored
        """
        ret = {}
        if lset is not None and lset.jsonlen > 0 and lset.json is not None:
            # no filtering, return a dict of all labels in the set
            ret = json.loads(lset.json.decode())
        return ret

    def pmLookupLabels(self, pmid):
        """PMAPI - Get all labels for a single metric, excluding instance
           level labels (use pmGetInstancesLabels for those).
           Return dict of {type: {name: value, ...}, ...}
        """
        ret = {}
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        desc = self.pmLookupDesc(pmid)

        lset = self.pmGetContextLabels()
        if lset:
            ret.update({c_api.PM_LABEL_CONTEXT: lset})

        lset = self.pmGetDomainLabels(LIBPCP.pmID_domain(pmid))
        if lset:
            ret.update({c_api.PM_LABEL_DOMAIN: lset})

        if desc.indom != c_api.PM_INDOM_NULL:
            lset = self.pmGetInDomLabels(desc.indom)
            if lset:
                ret.update({c_api.PM_LABEL_INDOM: lset})

        lset = self.pmGetClusterLabels(pmid)
        if lset:
            ret.update({c_api.PM_LABEL_CLUSTER: lset})

        lset = self.pmGetItemLabels(pmid)
        if lset:
            ret.update({c_api.PM_LABEL_ITEM: lset})

        return ret

    def pmGetInstancesLabels(self, indom):
        """PMAPI - Get instance level labels for all instances in indom
           return a dict {instid: {name: value, ...}, ...}
        """
        instlabelsD = {}
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        if indom == c_api.PM_INDOM_NULL:
            return instlabelsD
        result_p = POINTER(pmLabelSet)()
        status = LIBPCP.pmGetInstancesLabels(indom, byref(result_p))
        if status < 0:
            raise pmErr(status)
        for i in range(status):
            lset = result_p[i]
            if lset.json is not None:
                instlabelsD.update({lset.inst: json.loads(lset.json.decode())})
        if status > 0:
            LIBPCP.pmFreeLabelSets(result_p, status)

        return instlabelsD

    def pmGetItemLabels(self, pmid):
        """PMAPI - Get labels of a given metric identifier
           On success, this returns a dict of the labels in a single pmLabelSet
        """
        result_p = POINTER(pmLabelSet)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmGetItemLabels(pmid, byref(result_p))
        if status < 0:
            raise pmErr(status)
        if status == 0:
            return {}
        ret = self.pmlabelset_to_dict(result_p[0])
        self.pmFreeLabelSets(result_p, 1)
        return ret

    def pmGetClusterLabels(self, pmid):
        """PMAPI - Get labels of a given metric cluster
           On success, this returns a dict of the labels in a single pmLabelSet
        """
        result_p = POINTER(pmLabelSet)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmGetClusterLabels(pmid, byref(result_p))
        if status < 0:
            raise pmErr(status)
        if status == 0:
            return {}
        ret = self.pmlabelset_to_dict(result_p[0])
        self.pmFreeLabelSets(result_p, 1)
        return ret

    def pmGetInDomLabels(self, indom):
        """PMAPI - Get labels of a given instance domain
           On success, this returns a dict of the labels in a single pmLabelSet
        """
        indomLabelsD = {}
        result_p = POINTER(pmLabelSet)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        if indom == c_api.PM_INDOM_NULL:
            return indomLabelsD
        status = LIBPCP.pmGetInDomLabels(indom, byref(result_p))
        if status < 0:
            raise pmErr(status)
        if status > 0:
            indomLabelsD = self.pmlabelset_to_dict(result_p[0])
            self.pmFreeLabelSets(result_p, 1)
        return indomLabelsD

    def pmGetDomainLabels(self, domain):
        """PMAPI - Get labels of a given performance domain
           On success, this returns a dict of the labels in a single pmLabelSet
        """
        result_p = POINTER(pmLabelSet)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmGetDomainLabels(domain, byref(result_p))
        if status < 0:
            raise pmErr(status)
        if status == 0:
            return {}
        ret = self.pmlabelset_to_dict(result_p[0])
        self.pmFreeLabelSets(result_p, 1)
        return ret

    def pmGetContextLabels(self):
        """PMAPI - Get labels of the current context
           On success, this returns a dict of the labels in a single pmLabelSet
        """
        result_p = POINTER(pmLabelSet)()
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)
        status = LIBPCP.pmGetContextLabels(byref(result_p))
        if status < 0:
            raise pmErr(status)
        if status == 0:
            return {}
        ret = self.pmlabelset_to_dict(result_p[0])
        self.pmFreeLabelSets(result_p, 1)
        return ret

    ##
    # PMAPI Ancilliary Support Services

    @staticmethod
    def pmMergeLabels(labels):
        """PMAPI - Merges string labels into a string
        """
        if not isinstance(labels, list):
            labelsL = [labels]
        else:
            labelsL = labels
        result_p = ctypes.create_string_buffer(c_api.PM_MAXLABELJSONLEN)
        arg_arr = (c_char_p * len(labelsL))()
        for i, label in enumerate(labelsL):
            arg_arr[i] = c_char_p(label.encode('utf-8'))
        status = LIBPCP.pmMergeLabels(arg_arr, len(arg_arr),
                                      result_p, len(result_p))
        if status < 0:
            raise pmErr(status)
        result = result_p.value
        return str(result.decode('ascii', 'ignore'))

    @staticmethod
    def pmMergeLabelSets(labelSets, callback=None, arg=None):
        """PMAPI - Merges list of pmLabelSets based on labelSets hierarchy into
           a string. Each list element may also be either a list or None.
        """
        result_p = ctypes.create_string_buffer(c_api.PM_MAXLABELJSONLEN)
        if callback is None:
            callback = lambda x, y, z: 1
        cb_func = mergeLabelSetsCB_type(callback)

        if labelSets is None:
            result_p.value = b"{}"
        else:
            if not isinstance(labelSets, list):
                labelSetsL = [labelSets]
            else:
                labelSetsL = []
                for l in labelSets:
                    if isinstance(l, list):
                        labelSetsL.extend(l)
                    elif l is not None:
                        labelSetsL.append(l)

            arg_arr = (POINTER(pmLabelSet) * len(labelSetsL))()
            for i, labelset in enumerate(labelSetsL):
                arg_arr[i] = cast(byref(labelset), POINTER(pmLabelSet))
            arg = cast(c_char_p(arg), c_void_p)
            status = LIBPCP.pmMergeLabelSets(arg_arr, len(labelSetsL), result_p,
                                             len(result_p), cb_func, arg)
            if status < 0:
                raise pmErr(status)

        result = result_p.value
        return str(result.decode('ascii', 'ignore'))

    @staticmethod
    def pmFreeLabelSets(labelSets, nsets=1):
        """PMAPI - Free the pmLabelSets memory. The labelsets argument is
           an array of nsets pmLabelSet structures.
        """
        for i in range(nsets):
            arg = cast(byref(labelSets[i]), POINTER(pmLabelSet))
            if arg:
                LIBPCP.pmFreeLabelSets(arg, 1)

    @staticmethod
    def pmGetConfig(variable):
        """PMAPI - Return single value from environment or pcp config file """
        if not isinstance(variable, bytes):
            variable = variable.encode('utf-8')
        result = LIBPCP.pmGetOptionalConfig(variable)
        if result is None:
            return result
        return str(result.decode())

    @staticmethod
    def pmErrStr(code):
        """PMAPI - Convert an error code to a readable string  """
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
        return outAtom

    @staticmethod
    def pmConvScale(inType, inAtom, desc, metric_idx, outUnits):
        """PMAPI - Convert a value to a different scale

        pmAtomValue = pmConvScale(c_api.PM_TYPE_FLOAT, pmAtomValue,
                                            pmDesc*, 3, c_api.PM_SPACE_MBYTE)
        """
        if isinstance(outUnits, int):
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
    def pmID_build(d, c, i):
        """PMAPI - build a pmID from domain, cluster, item"""
        result = LIBPCP.pmID_build(d, c, i)
        return result

    @staticmethod
    def pmID_domain(pmid):
        """PMAPI - return domain number from pmID"""
        result = LIBPCP.pmID_domain(pmid)
        return result

    @staticmethod
    def pmID_cluster(pmid):
        """PMAPI - return cluster number from pmID"""
        result = LIBPCP.pmID_cluster(pmid)
        return result

    @staticmethod
    def pmID_item(pmid):
        """PMAPI - return item number from pmID"""
        result = LIBPCP.pmID_item(pmid)
        return result

    @staticmethod
    def pmIDStr(pmid):
        """PMAPI - Convert a pmID to a readable string """
        pmidstr = ctypes.create_string_buffer(32)
        result = LIBPCP.pmIDStr_r(pmid, pmidstr, 32)
        return str(result.decode())

    @staticmethod
    def pmInDom_build(d, s):
        """PMAPI - build an indom from domain and serial"""
        result = LIBPCP.pmInDom_build(d, s)
        return result

    @staticmethod
    def pmInDom_domain(indom):
        """PMAPI - return domain number from indom"""
        result = LIBPCP.pmInDom_domain(indom)
        return result

    @staticmethod
    def pmInDom_serial(indom):
        """PMAPI - return serial number from indom"""
        result = LIBPCP.pmInDom_serial(indom)
        return result

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
    def pmSemStr(sem):
        """PMAPI - Convert a performance metric semantic to a readable string
        "string" = pmSemStr(c_api.PM_SEM_COUNTER)
        """
        semstr = ctypes.create_string_buffer(32)
        result = LIBPCP.pmSemStr_r(sem, semstr, 32)
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
    def pmSortHighResInstances(result_p):
        """PMAPI - sort all metric instances in result returned by pmFetchHighRes """
        LIBPCP.pmSortHighResInstances(result_p)

    @staticmethod
    def pmSortInstances(result_p):
        """PMAPI - sort all metric instances in result returned by pmFetch """
        LIBPCP.pmSortInstances(result_p)

    @staticmethod
    def pmParseHighResInterval(interval):
        """PMAPI - parse a textual time interval into a timespec struct
        (timespec_ctype, '') = pmParseHighResInterval("time string")
        """
        return (timespec.fromInterval(interval), '')

    @staticmethod
    def pmParseInterval(interval):
        """PMAPI - parse a textual time interval into a timeval struct
        (timeval_ctype, '') = pmParseInterval("time string")
        """
        return (timeval.fromInterval(interval), '')

    @staticmethod
    def pmParseMetricSpec(string, isarch=0, source=''):
        """PMAPI - parse a textual metric specification into a struct
        (result, '') = pmParseMetricSpec("hinv.ncpu", 0, "localhost")
        """
        return (pmMetricSpec.fromString(string, isarch, source), '')

    @staticmethod
    def pmParseUnitsStr(string):
        if not isinstance(string, (bytes, text_type)):
            raise pmErr(c_api.PM_ERR_CONV, str(string))
        if not isinstance(string, bytes):
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
    def pmtimespecSleep(spec):
        """ Delay for a specified amount of time (timespec).
            Useful for implementing tools that do metric sampling.
            Single arg is timespec in tuple returned from pmParseHighResInterval().
        """
        return spec.sleep()

    @staticmethod
    def pmtimevalSleep(tvp):
        """ Delay for a specified amount of time (timeval).
            Useful for implementing tools that do metric sampling.
            Single arg is timeval in tuple returned from pmParseInterval().
        """
        return tvp.sleep()

    @staticmethod
    def pmProgname():
        return str(c_char_p.in_dll(LIBPCP, "pmProgname").value.decode())

    @staticmethod
    def pmDebug(flags):
        if c_int.in_dll(LIBPCP, "pmDebug").value & flags:
            return True
        return False

    ##
    # PMAPI Python Utility Support Services

    @staticmethod
    def get_current_tz(options=None, set_dst=-1):
        """ Get current timezone offset string using POSIX convention """
        if set_dst >= 0:
            dst = 1 if set_dst else 0
        elif options:
            tvp = options.pmGetOptionOrigin()
            dst = time.localtime(tvp).tm_isdst
        else:
            dst = time.localtime().tm_isdst
        offset = time.altzone if dst else time.timezone
        timezone = time.tzname[dst]
        if offset:
            offset_hr = int(offset / 3600.0)
            offset_min = int(offset % 3600 / 60)
            if offset >= 0:
                timezone += "+"
            timezone += str(offset_hr)
            if offset_min:
                timezone += ":" + str(offset_min)
        return timezone

    @staticmethod
    def posix_tz_to_utc_offset(timezone):
        """ Convert POSIX timezone offset string to human readable UTC offset """
        if not timezone or not True in [c in timezone for c in ['+', '-']]:
            return "UTC+0"
        offset = timezone.split("+")[1] if "+" in timezone else timezone.split("-")[1]
        sign = "+" if "-" in timezone else "-"
        return "UTC" + sign + str(offset)

    @staticmethod
    def set_timezone(options):
        """ Set timezone for a Python tool """
        if options.pmGetOptionTimezone():
            os.environ['TZ'] = options.pmGetOptionTimezone()
            time.tzset()
            pmContext.pmNewZone(options.pmGetOptionTimezone())
        elif options.pmGetOptionHostZone():
            os.environ['TZ'] = pmContext.pmWhichZone()
            time.tzset()
        else:
            timezone = pmContext.get_current_tz(options)
            os.environ['TZ'] = timezone
            time.tzset()
            pmContext.pmNewZone(timezone)

    @staticmethod
    def datetime_to_secs(value, precision=c_api.PM_TIME_SEC):
        """ Convert datetime value to seconds of given precision """
        tdt = value - datetime.datetime.fromtimestamp(0)
        if precision == c_api.PM_TIME_SEC:
            tst = (tdt.microseconds + (tdt.seconds + tdt.days * 24.0 * 3600.0) * 10.0**6) / 10.0**6
        elif precision == c_api.PM_TIME_MSEC:
            tst = (tdt.microseconds + (tdt.seconds + tdt.days * 24.0 * 3600.0) * 10.0**6) / 10.0**3
        elif precision == c_api.PM_TIME_USEC:
            tst = (tdt.microseconds + (tdt.seconds + tdt.days * 24.0 * 3600.0) * 10.0**6) / 1.0
        elif precision == c_api.PM_TIME_NSEC:
            tst = (tdt.microseconds + (tdt.seconds + tdt.days * 24.0 * 3600.0) * 10.0**6) * 10.0**3
        else:
            raise ValueError("Unsupported precision requested")
        return tst

    @staticmethod
    def get_mode_step(archive, interpol, interval):
        """ Get mode and step for pmSetMode """
        if not interpol or archive:
            mode = c_api.PM_MODE_FORW
            step = None
        else:
            mode = c_api.PM_MODE_INTERP
            step = timespec(interval.tv_sec, interval.tv_usec * 1000)
        return mode, step

    def prepare_execute(self, options, archive, interpol, interval):
        """ Common execution preparation """
        status = LIBPCP.pmUseContext(self.ctx)
        if status < 0:
            raise pmErr(status)

        self.set_timezone(options)

        if self.type == c_api.PM_CONTEXT_ARCHIVE:
            mode, step = pmContext.get_mode_step(archive, interpol, interval)
            self.pmSetModeHighRes(mode, options.pmGetOptionHighResOrigin(), step)

# ----- fetchgroup API

LIBPCP.pmCreateFetchGroup.restype = c_int
LIBPCP.pmCreateFetchGroup.argtypes = [POINTER(c_void_p), c_int, c_char_p]
LIBPCP.pmGetFetchGroupContext.restype = c_int
LIBPCP.pmGetFetchGroupContext.argtypes = [c_void_p]
LIBPCP.pmClearFetchGroup.restype = c_int
LIBPCP.pmClearFetchGroup.argtypes = [c_void_p]
LIBPCP.pmDestroyFetchGroup.restype = c_int
LIBPCP.pmDestroyFetchGroup.argtypes = [c_void_p]
LIBPCP.pmExtendFetchGroup_item.restype = c_int
LIBPCP.pmExtendFetchGroup_item.argtypes = [c_void_p, c_char_p, c_char_p, c_char_p,
                                           POINTER(pmAtomValue), c_int, POINTER(c_int)]
LIBPCP.pmExtendFetchGroup_indom.restype = c_int
LIBPCP.pmExtendFetchGroup_indom.argtypes = [c_void_p, c_char_p, c_char_p,
                                            POINTER(c_int),
                                            POINTER(c_char_p),
                                            POINTER(pmAtomValue),
                                            c_int,
                                            POINTER(c_int),
                                            c_uint,
                                            POINTER(c_uint),
                                            POINTER(c_int)]
LIBPCP.pmExtendFetchGroup_event.restype = c_int
LIBPCP.pmExtendFetchGroup_event.argtypes = [c_void_p, c_char_p, c_char_p, c_char_p, c_char_p,
                                            POINTER(timespec),
                                            POINTER(pmAtomValue),
                                            c_int,
                                            POINTER(c_int),
                                            c_uint,
                                            POINTER(c_uint),
                                            POINTER(c_int)]
LIBPCP.pmExtendFetchGroup_timeval.restype = c_int
LIBPCP.pmExtendFetchGroup_timeval.argtypes = [c_void_p, POINTER(timeval)]
LIBPCP.pmExtendFetchGroup_timespec.restype = c_int
LIBPCP.pmExtendFetchGroup_timespec.argtypes = [c_void_p, POINTER(timespec)]
LIBPCP.pmFetchGroup.restype = c_int
LIBPCP.pmFetchGroup.argtypes = [c_void_p]


class fetchgroup(object):
    """Defines a PMAPI fetchgroup.

    This class wraps the pmFetchGroup set of C PMAPI functions (q.v.)
    in an object-oriented manner.

    Each instance of this class represents one fetchgroup, in which
    interest in several metrics (individual or indoms) is registered.
    Each registration results in an function-like object that may be
    called to decode that metric's value(s).  Errors are signalled
    with exceptions rather than result integers.  Strings are all
    UTF-8 encoded.
    """

    class fetchgroup_item(object):
        """
        An internal class to receive value/status for a single item.
        It may be called as if it were a function object to decode
        the embedded pmAtomValue, which was set at the most recent
        .fetch() call.
        """

        def __init__(self, pmtype):
            """Allocate a single instance to receive a fetchgroup item."""
            self.sts = c_int(c_api.PM_ERR_VALUE)
            self.pmtype = pmtype
            self.value = pmAtomValue()

        def __call__(self):
            """Retrieve a converted value of a fetchgroup item, if available."""
            if self.sts.value < 0:
                raise pmErr(self.sts.value)
            return self.value.dref(self.pmtype)

    class fetchgroup_timespec(object):
        """
        An internal class to receive value for a single timespec.
        It may be called as if it were a function object to decode
        the timespec, which was set at the most recent
        .fetch() call, into a datetime object.
        """

        def __init__(self, ctx):
            """Allocate a single instance to receive a fetchgroup timespec."""
            self.value = timespec()
            self.ctx = ctx

        def __call__(self):
            """
            Retrieve a converted value of a timespec, if available.  Use
            pmLocaltime() to convert to a datetime object.
            """
            ts = self.ctx.pmLocaltime(self.value.tv_sec)
            us = int(self.value.tv_nsec) // 1000
            dt = datetime.datetime(ts.tm_year+1900, ts.tm_mon+1, ts.tm_mday,
                                   ts.tm_hour, ts.tm_min, ts.tm_sec, us, None)
            return dt

    class fetchgroup_timeval(object):
        """
        An internal class to receive value for a single timeval.
        It may be called as if it were a function object to decode
        the timeval, which was set at the most recent
        .fetch() call, into a datetime object.
        """

        def __init__(self, ctx):
            """Allocate a single instance to receive a fetchgroup timeval."""
            self.value = timeval()
            self.ctx = ctx

        def __call__(self):
            """
            Retrieve a converted value of a timeval, if available.  Use
            pmLocaltime() to convert to a datetime object.
            """
            ts = self.ctx.pmLocaltime(self.value.tv_sec)
            us = int(self.value.tv_usec)
            dt = datetime.datetime(ts.tm_year+1900, ts.tm_mon+1, ts.tm_mday,
                                   ts.tm_hour, ts.tm_min, ts.tm_sec, us, None)
            return dt

    # create the backward compatibility alias
    fetchgroup_timestamp = fetchgroup_timeval

    class fetchgroup_indom(object):
        """
        An internal class to receive value/status for an indom of
        items.  It may be called as if it were a function object to
        create an list of tuples containing instance-code/-name/value
        information.  Each value is a function object that decodes
        the embedded pmAtomValue, which was set at the most recent
        fetch() call.
        """

        def __init__(self, pmtype, num):
            """Allocate a single instance to receive a fetchgroup item."""
            stss_t = c_int * num
            values_t = pmAtomValue * num
            icodes_t = c_uint * num
            inames_t = c_char_p * num
            self.sts = c_int()
            self.stss = stss_t()
            self.pmtype = pmtype
            self.values = values_t()
            self.icodes = icodes_t()
            self.inames = inames_t()
            self.num = c_uint()

        def __call__(self):
            """Retrieve a converted value of a fetchgroup item, if available."""
            vv = []
            if self.sts.value < 0:
                raise pmErr(self.sts.value)
            for i in range(self.num.value):
                def decode_one(self, i):
                    if self.stss[i] < 0:
                        raise pmErr(self.stss[i])
                    return self.values[i].dref(self.pmtype)
                vv.append((self.icodes[i],
                           self.inames[i].decode('utf-8') if self.inames[i] else None,
                           # nested lambda for proper i capture
                           # pylint: disable=cell-var-from-loop
                           (lambda i: (lambda: decode_one(self, i)))(i)))
            return vv


    class fetchgroup_event(object):
        """
        An internal class to receive value/status for an
        event record field.  It may be called as if it were a function
        object to create an list of tuples containing timestamp/value
        information.  Each value is a function object that decodes
        the embedded pmAtomValue, which was set at the most recent
        fetch() call.
        """

        def __init__(self, pmtype, num, ctx):
            """Allocate a single instance to receive a fetchgroup item."""
            stss_t = c_int * num
            values_t = pmAtomValue * num
            timespec_t = timespec * num
            self.sts = c_int()
            self.stss = stss_t()
            self.pmtype = pmtype
            self.times = timespec_t()
            self.values = values_t()
            self.num = c_uint()
            self.ctx = ctx

        def __call__(self):
            """Retrieve a converted value of a fetchgroup item, if available."""
            vv = []
            if self.sts.value < 0:
                raise pmErr(self.sts.value)
            # print ([self.values[i].dref(self.pmtype) for i in range(self.num.value)])
            for i in range(self.num.value):
                def decode_one(self, i):
                    if self.stss[i] < 0:
                        raise pmErr(self.stss[i])
                    return self.values[i].dref(self.pmtype)

                ts = self.ctx.pmLocaltime(self.times[i].tv_sec)
                us = int(self.times[i].tv_nsec) // 1000
                dt = datetime.datetime(ts.tm_year+1900, ts.tm_mon+1, ts.tm_mday,
                                       ts.tm_hour, ts.tm_min, ts.tm_sec, us, None)
                # nested lambda for proper i capture
                # pylint: disable=cell-var-from-loop
                vv.append((dt,
                           (lambda i: (lambda: decode_one(self, i)))(i)))
            return vv


    class pmContext_borrowed(pmContext): # lgtm[py/missing-call-to-init] lgtm[py/missing-call-to-delete]
        """
        An internal class for accessing the private PMAPI context
        belonging to a fetchgroup.  It works just like a pmContext,
        except it overrides the constructor/destructor to reflect
        the "borrowed" state of the context.
        """
        def __init__(self, ctx, typed, target):
            """Override pmContext ctor to eschew pmNewContext."""
            # pylint: disable=W0231
            self._ctx = ctx
            self._type = typed
            self._target = target
            # NB: don't call pmContext.__init__!

        def __del__(self):
            """Override pmContext ctor to eschew pmDestroyContext."""
            if c_api is not None and self._ctx != c_api.PM_ERR_NOCONTEXT:
                self._ctx = c_api.PM_ERR_NOCONTEXT

    def __init__(self, typed=c_api.PM_CONTEXT_HOST, target="local:"):
        """Create a fetchgroup from a pmContext."""
        self.pmfg = c_void_p()
        self.items = []
        if typed == c_api.PM_CONTEXT_LOCAL and target is None:
            target = "" # Ignored
        sts = LIBPCP.pmCreateFetchGroup(byref(self.pmfg), typed, target.encode('utf-8'))
        if sts < 0:
            raise pmErr(sts)
        self.ctx = fetchgroup.pmContext_borrowed(LIBPCP.pmGetFetchGroupContext(self.pmfg),
                                                 typed, target)

    def __del__(self):
        """Destroy the fetchgroup.  Drop references to fetchgroup_* items."""
        if LIBPCP is not None and self.pmfg.value is not None:
            sts = LIBPCP.pmDestroyFetchGroup(self.pmfg)
            if sts < 0:
                raise pmErr(sts)
        del self.items[:]

    def get_context(self):
        """
        Return the private pmContext used by the fetchgroup.
        WARNING: mutation of this context by other PMAPI functions
        may disrupt fetchgroup functionality.
        """
        return self.ctx

    def extend_item(self, metric=None, mtype=None, scale=None, instance=None):
        # pylint: disable=C0330
        """Extend the fetchgroup with a single metric.  Infer type if
        necessary.  Convert scale/rate if appropriate/requested.
        Requires a specified instance if metric has an instance
        domain.
        """
        if metric is None:
            raise pmErr(-errno.EINVAL)

        if mtype is None:
            # a special service to dynamically-typed python
            pmids = self.ctx.pmLookupName(metric)
            descs = self.ctx.pmLookupDescs(pmids)
            mtype = descs[0].type
        v = fetchgroup.fetchgroup_item(mtype)
        sts = LIBPCP.pmExtendFetchGroup_item(self.pmfg,
                      c_char_p(metric.encode('utf-8') if metric else None),
                      c_char_p(instance.encode('utf-8') if instance else None),
                      c_char_p(scale.encode('utf-8') if scale else None),
                      pointer(v.value), c_int(mtype),
                      pointer(v.sts))
        if sts < 0:
            raise pmErr(sts)
        self.items.append(v) # keep registered pmAtomValue/etc. alive
        return v

    def extend_indom(self, metric=None, mtype=None, scale=None, maxnum=100):
        # pylint: disable=C0330
        """Extend the fetchgroup with up to @maxnum instances of a metric.
        (Metrics without instances are also accepted.)  Infer type if
        necessary.  Convert scale/rate if appropriate/requested.
        """
        if metric is None or maxnum < 0:
            raise pmErr(-errno.EINVAL)
        if mtype is None:
            # a special service to dynamically-typed python
            pmids = self.ctx.pmLookupName(metric)
            descs = self.ctx.pmLookupDescs(pmids)
            mtype = descs[0].type
        vv = fetchgroup.fetchgroup_indom(mtype, maxnum)
        sts = LIBPCP.pmExtendFetchGroup_indom(self.pmfg,
                      c_char_p(metric.encode('utf-8') if metric else None),
                      c_char_p(scale.encode('utf-8') if scale else None),
                      cast(pointer(vv.icodes), POINTER(c_int)),
                      cast(pointer(vv.inames), POINTER(c_char_p)),
                      cast(pointer(vv.values), POINTER(pmAtomValue)),
                      c_int(mtype),
                      cast(pointer(vv.stss), POINTER(c_int)),
                      c_uint(maxnum), pointer(vv.num), pointer(vv.sts))
        if sts < 0:
            raise pmErr(sts)
        self.items.append(vv) # keep registered pmAtomValue/etc. alive
        return vv

    def extend_timespec(self):
        """Extend the fetchgroup with a timespec query. """
        v = fetchgroup.fetchgroup_timespec(self.ctx)
        sts = LIBPCP.pmExtendFetchGroup_timespec(self.pmfg, pointer(v.value))
        if sts < 0:
            raise pmErr(sts)
        self.items.append(v) # keep registered timespec alive
        return v

    def extend_timeval(self):
        """Extend the fetchgroup with a timeval query. """
        v = fetchgroup.fetchgroup_timeval(self.ctx)
        sts = LIBPCP.pmExtendFetchGroup_timeval(self.pmfg, pointer(v.value))
        if sts < 0:
            raise pmErr(sts)
        self.items.append(v) # keep registered timeval alive
        return v

    # backward compatibility alias
    extend_timestamp = extend_timeval

    def extend_event(self, metric=None, field=None, ftype=None, scale=None, instance=None, maxnum=100):
        # pylint: disable=C0330
        """Extend the fetchgroup with up to @maxnum instances of the given
        field of the given event metric's records.  Infer type if necessary.
        Convert scale if appropriate/requested.
        """
        if metric is None or maxnum < 0:
            raise pmErr(-errno.EINVAL)
        if ftype is None:
            # a special service to dynamically-typed python
            pmids = self.ctx.pmLookupName(field)
            descs = self.ctx.pmLookupDescs(pmids)
            ftype = descs[0].type
        vv = fetchgroup.fetchgroup_event(ftype, maxnum, self.ctx)
        sts = LIBPCP.pmExtendFetchGroup_event(self.pmfg,
                      c_char_p(metric.encode('utf-8') if metric else None),
                      c_char_p(instance.encode('utf-8') if instance else None),
                      c_char_p(field.encode('utf-8') if field else None),
                      c_char_p(scale.encode('utf-8') if scale else None),
                      cast(pointer(vv.times), POINTER(timespec)),
                      cast(pointer(vv.values), POINTER(pmAtomValue)),
                      c_int(ftype),
                      cast(pointer(vv.stss), POINTER(c_int)),
                      c_uint(maxnum), pointer(vv.num), pointer(vv.sts))
        if sts < 0:
            raise pmErr(sts)
        self.items.append(vv) # keep registered pmAtomValue/etc. alive
        return vv

    def fetch(self):
        """Fetch all the metrics in this fetchgroup and update all values."""
        sts = LIBPCP.pmFetchGroup(self.pmfg)
        if sts < 0:
            raise pmErr(sts)
        return sts  # propogate any pmFetch(3) state flags to caller

    def clear(self):
        """Clear all the metrics in this fetchgroup ready to start again."""
        sts = LIBPCP.pmClearFetchGroup(self.pmfg)
        del self.items[:]
        if sts < 0:
            raise pmErr(sts)
        self.items = []
