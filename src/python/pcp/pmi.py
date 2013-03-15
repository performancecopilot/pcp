#
# Copyright (C) 2012-2013 Red Hat Inc.
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

"""Wrapper module for libpcp_import - Performace Co-Pilot Log Import API

Additional Information:

Performance Co-Pilot Web Site
http://oss.sgi.com/projects/pcp

EXAMPLE

import pcp
from pcp import pmi

# Create a new archive
log = pmi.pmiLogImport("tuesdays-log")
log.pmiSetHostname("www.abc.com")
log.pmiSetTimezone("EST-10")

# Add a metric with an instance domain
pmid = log.pmiID(60, 2, 0)
indom = log.pmiInDom(60, 2)
units = log.pmiUnits(0,0,0,0,0,0)
log.pmiAddMetric("kernel.all.load", pmid, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT, units)
log.pmiAddInstance(indom, "1 minute", 1)
log.pmiAddInstance(indom, "5 minute", 5)
log.pmiAddInstance(indom, "15 minute", 15)

# Create a record with a timestamp
log.pmiPutValue("kernel.all.load", "1 minute", "0.01")
log.pmiPutValue("kernel.all.load", "5 minute", "0.05")
log.pmiPutValue("kernel.all.load", "15 minute", "0.15")
timetuple = math.modf(time.time())
useconds = int(timetuple[0] * 1000000)
seconds = int(timetuple[1])
log.pmiWrite(seconds, useconds)
"""

from pcp import pmID, pmInDom, pmUnits, pmResult

import ctypes
from ctypes import c_int, c_char_p, POINTER

# Performance Co-Pilot PMI library (C)
libpcp_import = ctypes.CDLL(ctypes.util.find_library("pcp_import"))

##
# PMI Log Import Services

libpcp_import.pmiDump.restype = None
libpcp_import.pmiDump.argtypes = None

libpcp_import.pmiID.restype = pmID
libpcp_import.pmiID.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]

libpcp_import.pmiInDom.restype = pmInDom
libpcp_import.pmiInDom.argtypes = [ctypes.c_int, ctypes.c_int]

libpcp_import.pmiUnits.restype = pmUnits
libpcp_import.pmiUnits.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]

libpcp_import.pmiErrStr_r.restype = c_char_p
libpcp_import.pmiErrStr_r.argtypes = [c_int, c_char_p, c_int]

libpcp_import.pmiStart.restype = c_int
libpcp_import.pmiStart.argtypes = [c_char_p, c_int]

libpcp_import.pmiUseContext.restype = c_int
libpcp_import.pmiUseContext.argtypes = [c_int]

libpcp_import.pmiEnd.restype = c_int
libpcp_import.pmiEnd.argtypes = None

libpcp_import.pmiSetHostname.restype = c_int
libpcp_import.pmiSetHostname.argtypes = [c_char_p]

libpcp_import.pmiSetTimezone.restype = c_int
libpcp_import.pmiSetTimezone.argtypes = [c_char_p]

libpcp_import.pmiAddMetric.restype = c_int
libpcp_import.pmiAddMetric.argtypes = [c_char_p, pmID, c_int, pmInDom, c_int, pmUnits]

libpcp_import.pmiAddInstance.restype = c_int
libpcp_import.pmiAddInstance.argtypes = [pmInDom, c_char_p, c_int]

libpcp_import.pmiPutValue.restype = c_int
libpcp_import.pmiPutValue.argtypes = [c_char_p, c_char_p, c_char_p]

libpcp_import.pmiGetHandle.restype = c_int
libpcp_import.pmiGetHandle.argtypes = [c_char_p, c_char_p]

libpcp_import.pmiPutValueHandle.restype = c_int
libpcp_import.pmiPutValueHandle.argtypes = [c_int, c_char_p]

libpcp_import.pmiWrite.restype = c_int
libpcp_import.pmiWrite.argtypes = [c_int, c_int]

libpcp_import.pmiPutResult.restype = c_int
libpcp_import.pmiPutResult.argtypes = [POINTER(pmResult)]

#
# definition of exception classes
#

class pmiErr(Exception):

    def __str__(self):
        errNum = self.args[0]
        try:
            errSym = pmiErrSymD[errNum]
            errStr = ctypes.create_string_buffer(PMI_MAXERRMSGLEN)
            errStr = libpcp_import.pmiErrStr_r(errNum, errStr, PMI_MAXERRMSGLEN)
        except KeyError:
            errSym = errStr = ""
        return "%s %s" % (errSym, errStr)


#
# class pmiLogImport
#
# This class wraps the PMI (Log Import) library functions
#

class pmiLogImport(object):
    """Defines a PCP Log Import archive context
       This is used to create a PCP archive from an external source
    """

    ##
    # property read methods

    def _R_path(self):
        return self._path
    def _R_ctx(self):
        return self._ctx

    ##
    # property definitions

    path = property(_R_path, None, None, None)
    ctx = property(_R_ctx, None, None, None)

    ##
    # overloads

    def __init__(self, path, inherit = 0):
        self._path = path	# the archive path (file name)
        self._ctx = libpcp_import.pmiStart(c_char_p(path), inherit)
        if self._ctx < 0:
            raise pmiErr, self._ctx

    def __del__(self):
        if libpcp_import:
            libpcp_import.pmiUseContext(self._ctx)
            libpcp_import.pmiEnd()
        self._ctx = -1

    ##
    # PMI Log Import Services

    def pmiSetHostname(self, hostname):
        """PMI - set the source host name for a Log Import archive
        """
        status = libpcp_import.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiSetHostname(c_char_p(hostname))
        if status < 0:
            raise pmiErr, status
        return status

    def pmiSetTimezone(self, timezone):
        """PMI - set the source timezone for a Log Import archive
        """
        status = libpcp_import.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiSetTimezone(c_char_p(timezone))
        if status < 0:
            raise pmiErr, status
        return status

    def pmiID(self, domain, cluster, item):
        """PMI - construct a pmID data structure (helper routine)
        """
        return libpcp_import.pmiID(domain, cluster, item)

    def pmiInDom(self, domain, serial):
        """PMI - construct a pmInDom data structure (helper routine)
        """
        return libpcp_import.pmiInDom(domain, serial)

    def pmiUnits(self, dimSpace, dimTime, dimCount, scaleSpace, scaleTime, scaleCount):
        """PMI - construct a pmiUnits data structure (helper routine)
        """
        return libpcp_import.pmiUnits(dimSpace, dimTime, dimCount,
                                       scaleSpace, scaleTime, scaleCount)

    def pmiAddMetric(self, name, pmid, type, indom, sem, units):
        """PMI - add a new metric definition to a Log Import context
        """
        status = libpcp_import.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiAddMetric(c_char_p(name), pmid, type, indom, sem, units)
        if status < 0:
            raise pmiErr, status
        return status

    def pmiAddInstance(self, indom, instance, instid):
        """PMI - add an element to an instance domain in a Log Import context
        """
        status = libpcp_import.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiAddInstance(indom, c_char_p(instance), instid)
        if status < 0:
            raise pmiErr, status
        return status

    def pmiPutValue(self, name, inst, value):
        """PMI - add a value for a metric-instance pair
        """
        status = libpcp_import.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiPutValue(c_char_p(name), c_char_p(inst), c_char_p(value))
        if status < 0:
            raise pmiErr, status
        return status

    def pmiGetHandle(self, name, inst):
        """PMI - define a handle for a metric-instance pair
        """
        status = libpcp_import.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiGetHandle(c_char_p(name), c_char_p(inst))
        if status < 0:
            raise pmiErr, status
        return status

    def pmiPutValueHandle(self, handle, value):
        """PMI - add a value for a metric-instance pair via a handle
        """
        status = libpcp_import.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiPutValueHandle(handle, c_char_p(value))
        if status < 0:
            raise pmiErr, status
        return status

    def pmiWrite(self, sec, usec):
        """PMI - flush data to a Log Import archive
        """
        status = libpcp_import.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiWrite(sec, usec)
        if status < 0:
            raise pmiErr, status
        return status

    def pmiPutResult(self, result):
        """PMI - add a data record to a Log Import archive
        """
        status = libpcp_import.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiPutResult(cast(result,POINTER(pmResult)))
        if status < 0:
            raise pmiErr, status
        return status

    def pmiDump(self):
        """PMI - dump the current Log Import contexts (diagnostic)
        """
        libpcp_import.pmiDump()

    def pmiEnd(self):
        """PMI - close current context and finish a Log Import archive
        """
        status = libpcp_import.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = libpcp_import.pmiEnd()
        self._ctx = -1
        if status < 0:
            raise pmiErr, status
        return status

