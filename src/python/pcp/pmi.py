# pylint: disable=C0103
"""Wrapper module for libpcp_import - Performace Co-Pilot Log Import API
#
# Copyright (C) 2012-2013 Red Hat.
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

# Example use of this module for creating a PCP archive:

        import math
        import time
        import pmapi
        from pcp import pmi

        # Create a new archive
        log = pmi.pmiLogImport("loadtest")
        log.pmiSetHostname("www.abc.com")
        log.pmiSetTimezone("EST-10")

        # Add a metric with an instance domain
        domain = 60  # Linux kernel
        pmid = log.pmiID(domain, 2, 0)
        indom = log.pmiInDom(domain, 2)
        units = log.pmiUnits(0, 0, 0, 0, 0, 0)
        log.pmiAddMetric("kernel.all.load", pmid, pmapi.PM_TYPE_FLOAT,
                         indom, pmapi.PM_SEM_INSTANT, units)
        log.pmiAddInstance(indom, "1 minute", 1)
        log.pmiAddInstance(indom, "5 minute", 5)
        log.pmiAddInstance(indom, "15 minute", 15)

        # Create a record with a timestamp
        log.pmiPutValue("kernel.all.load", "1 minute", "%f" % 0.01)
        log.pmiPutValue("kernel.all.load", "5 minute", "%f" % 0.05)
        log.pmiPutValue("kernel.all.load", "15 minute", "%f" % 0.15)
        timetuple = math.modf(time.time())
        useconds = int(timetuple[0] * 1000000)
        seconds = int(timetuple[1])
        log.pmiWrite(seconds, useconds)
        del log
"""

from pcp.pmapi import pmID, pmInDom, pmUnits, pmResult
from cpmi import pmiErrSymDict, PMI_MAXERRMSGLEN

import ctypes
from ctypes import cast, c_int, c_char_p, POINTER

# Performance Co-Pilot PMI library (C)
LIBPCP_IMPORT = ctypes.CDLL(ctypes.util.find_library("pcp_import"))

##
# PMI Log Import Services

LIBPCP_IMPORT.pmiDump.restype = None
LIBPCP_IMPORT.pmiDump.argtypes = None

LIBPCP_IMPORT.pmiID.restype = pmID
LIBPCP_IMPORT.pmiID.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]

LIBPCP_IMPORT.pmiInDom.restype = pmInDom
LIBPCP_IMPORT.pmiInDom.argtypes = [ctypes.c_int, ctypes.c_int]

LIBPCP_IMPORT.pmiUnits.restype = pmUnits
LIBPCP_IMPORT.pmiUnits.argtypes = [
        ctypes.c_int, ctypes.c_int, ctypes.c_int,
        ctypes.c_int, ctypes.c_int, ctypes.c_int]

LIBPCP_IMPORT.pmiErrStr_r.restype = c_char_p
LIBPCP_IMPORT.pmiErrStr_r.argtypes = [c_int, c_char_p, c_int]

LIBPCP_IMPORT.pmiStart.restype = c_int
LIBPCP_IMPORT.pmiStart.argtypes = [c_char_p, c_int]

LIBPCP_IMPORT.pmiUseContext.restype = c_int
LIBPCP_IMPORT.pmiUseContext.argtypes = [c_int]

LIBPCP_IMPORT.pmiEnd.restype = c_int
LIBPCP_IMPORT.pmiEnd.argtypes = None

LIBPCP_IMPORT.pmiSetHostname.restype = c_int
LIBPCP_IMPORT.pmiSetHostname.argtypes = [c_char_p]

LIBPCP_IMPORT.pmiSetTimezone.restype = c_int
LIBPCP_IMPORT.pmiSetTimezone.argtypes = [c_char_p]

LIBPCP_IMPORT.pmiAddMetric.restype = c_int
LIBPCP_IMPORT.pmiAddMetric.argtypes = [
        c_char_p, pmID, c_int, pmInDom, c_int, pmUnits]

LIBPCP_IMPORT.pmiAddInstance.restype = c_int
LIBPCP_IMPORT.pmiAddInstance.argtypes = [pmInDom, c_char_p, c_int]

LIBPCP_IMPORT.pmiPutValue.restype = c_int
LIBPCP_IMPORT.pmiPutValue.argtypes = [c_char_p, c_char_p, c_char_p]

LIBPCP_IMPORT.pmiGetHandle.restype = c_int
LIBPCP_IMPORT.pmiGetHandle.argtypes = [c_char_p, c_char_p]

LIBPCP_IMPORT.pmiPutValueHandle.restype = c_int
LIBPCP_IMPORT.pmiPutValueHandle.argtypes = [c_int, c_char_p]

LIBPCP_IMPORT.pmiWrite.restype = c_int
LIBPCP_IMPORT.pmiWrite.argtypes = [c_int, c_int]

LIBPCP_IMPORT.pmiPutResult.restype = c_int
LIBPCP_IMPORT.pmiPutResult.argtypes = [POINTER(pmResult)]

#
# definition of exception classes
#

class pmiErr(Exception):
    '''
    Encapsulation for PMI interface error code
    '''
    def __str__(self):
        error_code = self.args[0]
        try:
            error_symbol = pmiErrSymDict[error_code]
            error_string = ctypes.create_string_buffer(PMI_MAXERRMSGLEN)
            error_string = LIBPCP_IMPORT.pmiErrStr_r(error_code,
                                        error_string, PMI_MAXERRMSGLEN)
        except KeyError:
            error_symbol = error_string = ""
        return "%s %s" % (error_symbol, error_string)


#
# class LogImport
#
# This class wraps the PMI (Log Import) library functions
#

class pmiLogImport(object):
    """Defines a PCP Log Import archive context
       This is used to create a PCP archive from an external source
    """

    ##
    # property read methods

    def read_path(self):
        """ Property for archive path """
        return self._path
    def read_ctx(self):
        """ Property for log import context """
        return self._ctx

    ##
    # property definitions

    path = property(read_path, None, None, None)
    ctx = property(read_ctx, None, None, None)

    ##
    # overloads

    def __init__(self, path, inherit = 0):
        self._path = path        # the archive path (file name)
        self._ctx = LIBPCP_IMPORT.pmiStart(c_char_p(path), inherit)
        if self._ctx < 0:
            raise pmiErr, self._ctx

    def __del__(self):
        if LIBPCP_IMPORT:
            LIBPCP_IMPORT.pmiUseContext(self._ctx)
            LIBPCP_IMPORT.pmiEnd()
        self._ctx = -1

    ##
    # PMI Log Import Services

    def pmiSetHostname(self, hostname):
        """PMI - set the source host name for a Log Import archive """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = LIBPCP_IMPORT.pmiSetHostname(c_char_p(hostname))
        if status < 0:
            raise pmiErr, status
        return status

    def pmiSetTimezone(self, timezone):
        """PMI - set the source timezone for a Log Import archive
        """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = LIBPCP_IMPORT.pmiSetTimezone(c_char_p(timezone))
        if status < 0:
            raise pmiErr, status
        return status

    @staticmethod
    def pmiID(domain, cluster, item):
        """PMI - construct a pmID data structure (helper routine) """
        return LIBPCP_IMPORT.pmiID(domain, cluster, item)

    @staticmethod
    def pmiInDom(domain, serial):
        """PMI - construct a pmInDom data structure (helper routine) """
        return LIBPCP_IMPORT.pmiInDom(domain, serial)

    @staticmethod
    def pmiUnits(dim_space, dim_time, dim_count,
                        scale_space, scale_time, scale_count):
        # pylint: disable=R0913
        """PMI - construct a pmiUnits data structure (helper routine) """
        return LIBPCP_IMPORT.pmiUnits(dim_space, dim_time, dim_count,
                                       scale_space, scale_time, scale_count)

    def pmiAddMetric(self, name, pmid, typed, indom, sem, units):
        # pylint: disable=R0913
        """PMI - add a new metric definition to a Log Import context """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = LIBPCP_IMPORT.pmiAddMetric(c_char_p(name),
                                        pmid, typed, indom, sem, units)
        if status < 0:
            raise pmiErr, status
        return status

    def pmiAddInstance(self, indom, instance, instid):
        """PMI - add element to an instance domain in a Log Import context """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = LIBPCP_IMPORT.pmiAddInstance(indom, c_char_p(instance), instid)
        if status < 0:
            raise pmiErr, status
        return status

    def pmiPutValue(self, name, inst, value):
        """PMI - add a value for a metric-instance pair """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = LIBPCP_IMPORT.pmiPutValue(c_char_p(name),
                                        c_char_p(inst), c_char_p(value))
        if status < 0:
            raise pmiErr, status
        return status

    def pmiGetHandle(self, name, inst):
        """PMI - define a handle for a metric-instance pair """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = LIBPCP_IMPORT.pmiGetHandle(c_char_p(name), c_char_p(inst))
        if status < 0:
            raise pmiErr, status
        return status

    def pmiPutValueHandle(self, handle, value):
        """PMI - add a value for a metric-instance pair via a handle """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = LIBPCP_IMPORT.pmiPutValueHandle(handle, c_char_p(value))
        if status < 0:
            raise pmiErr, status
        return status

    def pmiWrite(self, sec, usec):
        """PMI - flush data to a Log Import archive """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = LIBPCP_IMPORT.pmiWrite(sec, usec)
        if status < 0:
            raise pmiErr, status
        return status

    def put_result(self, result):
        """PMI - add a data record to a Log Import archive """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = LIBPCP_IMPORT.pmiPutResult(cast(result, POINTER(pmResult)))
        if status < 0:
            raise pmiErr, status
        return status

    @staticmethod
    def pmiDump():
        """PMI - dump the current Log Import contexts (diagnostic) """
        LIBPCP_IMPORT.pmiDump()

    def pmiEnd(self):
        """PMI - close current context and finish a Log Import archive """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr, status
        status = LIBPCP_IMPORT.pmiEnd()
        self._ctx = -1
        if status < 0:
            raise pmiErr, status
        return status

