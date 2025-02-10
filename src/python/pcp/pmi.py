"""Wrapper module for libpcp_import - Performace Co-Pilot Log Import API
#
# Copyright (C) 2012-2022 Red Hat.
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
        log.pmiWrite(time.time())  # sec since epoch, or datetime, or
        #log.pmiWrite(seconds, useconds)

        del log
"""

from pcp.pmapi import pmID, pmInDom, pmUnits, pmHighResResult, pmResult
from cpmi import pmiErrSymDict, PMI_MAXERRMSGLEN
from ctypes import c_int, c_uint, c_longlong, c_char_p
from ctypes import cast, create_string_buffer, POINTER, CDLL
from ctypes.util import find_library
from datetime import datetime
from math import modf

# Performance Co-Pilot PMI library (C)
LIBPCP_IMPORT = CDLL(find_library("pcp_import"))

##
# PMI Log Import Services

LIBPCP_IMPORT.pmiDump.restype = None
LIBPCP_IMPORT.pmiDump.argtypes = None

LIBPCP_IMPORT.pmiID.restype = pmID
LIBPCP_IMPORT.pmiID.argtypes = [c_int, c_int, c_int]

LIBPCP_IMPORT.pmiCluster.restype = pmID
LIBPCP_IMPORT.pmiCluster.argtypes = [c_int, c_int]

LIBPCP_IMPORT.pmiInDom.restype = pmInDom
LIBPCP_IMPORT.pmiInDom.argtypes = [c_int, c_int]

LIBPCP_IMPORT.pmiUnits.restype = pmUnits
LIBPCP_IMPORT.pmiUnits.argtypes = [
        c_int, c_int, c_int,
        c_int, c_int, c_int]

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

LIBPCP_IMPORT.pmiSetVersion.restype = c_int
LIBPCP_IMPORT.pmiSetVersion.argtypes = [c_int]

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

LIBPCP_IMPORT.pmiWrite2.restype = c_int
LIBPCP_IMPORT.pmiWrite2.argtypes = [c_longlong, c_int]

LIBPCP_IMPORT.pmiHighResWrite.restype = c_int
LIBPCP_IMPORT.pmiHighResWrite.argtypes = [c_longlong, c_int]

LIBPCP_IMPORT.pmiPutHighResResult.restype = c_int
LIBPCP_IMPORT.pmiPutHighResResult.argtypes = [POINTER(pmHighResResult)]

LIBPCP_IMPORT.pmiPutResult.restype = c_int
LIBPCP_IMPORT.pmiPutResult.argtypes = [POINTER(pmResult)]

LIBPCP_IMPORT.pmiPutMark.restype = c_int
LIBPCP_IMPORT.pmiPutMark.argtypes = None

LIBPCP_IMPORT.pmiPutText.restype = c_int
LIBPCP_IMPORT.pmiPutText.argtypes = [c_uint, c_uint, c_uint, c_char_p]

LIBPCP_IMPORT.pmiPutLabel.restype = c_int
LIBPCP_IMPORT.pmiPutLabel.argtypes = [c_uint, c_uint, c_uint, c_char_p, c_char_p]

#
# definition of exception classes
#

class pmiErr(Exception):
    '''
    Encapsulation for PMI interface error code
    '''
    def __init__(self, *args):
        super(pmiErr, self).__init__(*args)
        self.args = list(args)
        if args and isinstance(args[0], int):
            self.code = args[0]
        else:
            self.code = 0

    def __str__(self):
        try:
            error_symbol = pmiErrSymDict[self.code]
            error_string = create_string_buffer(PMI_MAXERRMSGLEN)
            error_string = LIBPCP_IMPORT.pmiErrStr_r(self.code, error_string,
                                                     PMI_MAXERRMSGLEN)
        except KeyError:
            error_symbol = error_string = ""
        return "%s %s" % (error_symbol, error_string)

    def errno(self):
        return self.code


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

    def __init__(self, path, inherit=0):
        if not isinstance(path, bytes):
            path = path.encode('utf-8')
        self._path = path        # the archive path (file name)
        self._epoch = datetime.utcfromtimestamp(0)    # epoch sec
        self._ctx = LIBPCP_IMPORT.pmiStart(c_char_p(path), inherit)
        if self._ctx < 0:
            raise pmiErr(self._ctx)

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
            raise pmiErr(status)
        if not isinstance(hostname, bytes):
            hostname = hostname.encode('utf-8')
        status = LIBPCP_IMPORT.pmiSetHostname(c_char_p(hostname))
        if status < 0:
            raise pmiErr(status)
        return status

    def pmiSetTimezone(self, timezone):
        """PMI - set the source timezone for a Log Import archive
        """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        if not isinstance(timezone, bytes):
            timezone = timezone.encode('utf-8')
        status = LIBPCP_IMPORT.pmiSetTimezone(c_char_p(timezone))
        if status < 0:
            raise pmiErr(status)
        return status

    def pmiSetVersion(self, version):
        """PMI - set the output archive version (2 or 3)
        """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        if not isinstance(version, int):
            version = 2
        status = LIBPCP_IMPORT.pmiSetVersion(version)
        if status < 0:
            raise pmiErr(status)
        return status

    @staticmethod
    def pmiID(domain, cluster, item):
        """PMI - construct a pmID data structure (helper routine) """
        return LIBPCP_IMPORT.pmiID(domain, cluster, item)

    @staticmethod
    def pmiCluster(domain, cluster):
        """PMI - construct a pmID data structure (helper routine) """
        return LIBPCP_IMPORT.pmiCluster(domain, cluster)

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
            raise pmiErr(status)
        if not isinstance(name, bytes):
            name = name.encode('utf-8')
        status = LIBPCP_IMPORT.pmiAddMetric(c_char_p(name),
                                            pmid, typed, indom, sem, units)
        if status < 0:
            raise pmiErr(status)
        return status

    def pmiAddInstance(self, indom, instance, instid):
        """PMI - add element to an instance domain in a Log Import context """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        if not isinstance(instance, bytes):
            instance = instance.encode('utf-8')
        status = LIBPCP_IMPORT.pmiAddInstance(indom, c_char_p(instance), instid)
        if status < 0:
            raise pmiErr(status)
        return status

    def pmiPutValue(self, name, inst, value):
        """PMI - add a value for a metric-instance pair """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        if not isinstance(name, bytes):
            name = name.encode('utf-8')
        instance = None
        if inst is not None:
            if not isinstance(inst, bytes):
                inst = inst.encode('utf-8')
            instance = c_char_p(inst)
        if not isinstance(value, bytes):
            value = value.encode('utf-8')
        status = LIBPCP_IMPORT.pmiPutValue(c_char_p(name),
                                           instance, c_char_p(value))
        if status < 0:
            raise pmiErr(status)
        return status

    def pmiGetHandle(self, name, inst):
        """PMI - define a handle for a metric-instance pair """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        if not isinstance(name, bytes):
            name = name.encode('utf-8')
        instance = None
        if inst is not None:
            if not isinstance(inst, bytes):
                inst = inst.encode('utf-8')
            instance = c_char_p(inst)
        status = LIBPCP_IMPORT.pmiGetHandle(c_char_p(name), instance)
        if status < 0:
            raise pmiErr(status)
        return status

    def pmiPutValueHandle(self, handle, value):
        """PMI - add a value for a metric-instance pair via a handle """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        if not isinstance(value, bytes):
            value = value.encode('utf-8')
        status = LIBPCP_IMPORT.pmiPutValueHandle(handle, c_char_p(value))
        if status < 0:
            raise pmiErr(status)
        return status

    def pmiHighResWrite(self, sec, nsec):
        """PMI - flush data to a Log Import archive """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        status = LIBPCP_IMPORT.pmiHighResWrite(sec, nsec)
        if status < 0:
            raise pmiErr(status)
        return status

    def pmiWrite(self, sec, usec=None):
        """PMI - flush data to a Log Import archive """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        if sec and not usec:
            if isinstance(sec, datetime):
                sec = float((sec - self._epoch).total_seconds())
            if isinstance(sec, float):
                ts = modf(sec)
                sec = int(ts[1])
                usec = int(ts[0] * 1000000)
            else:
                usec = 0
        status = LIBPCP_IMPORT.pmiWrite2(sec, usec)
        if status < 0:
            raise pmiErr(status)
        return status

    def pmiPutMark(self):
        """PMI - write a <mark> record to a Log Import archive """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        status = LIBPCP_IMPORT.pmiPutMark()
        if status < 0:
            raise pmiErr(status)
        return status

    def put_result(self, result):
        """PMI - add a data record to a Log Import archive """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        status = LIBPCP_IMPORT.pmiPutResult(cast(result, POINTER(pmResult)))
        if status < 0:
            raise pmiErr(status)
        return status

    def put_highres_result(self, result):
        """PMI - add a data record to a Log Import archive """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        status = LIBPCP_IMPORT.pmiPutHighResResult(cast(result, POINTER(pmHighResResult)))
        if status < 0:
            raise pmiErr(status)
        return status

    def pmiPutText(self, typ, cls, ident, content):
        """PMI - add a text record to a Log Import archive """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        if not isinstance(content, bytes):
            content = content.encode('utf-8')
        status = LIBPCP_IMPORT.pmiPutText(typ, cls, ident, c_char_p(content))
        if status < 0:
            raise pmiErr(status)
        return status

    def pmiPutLabel(self, typ, ident, inst, name, content):
        # pylint: disable=R0913
        """PMI - add a label record to a Log Import archive """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        if not isinstance(name, bytes):
            name = name.encode('utf-8')
        if not isinstance(content, bytes):
            content = content.encode('utf-8')
        status = LIBPCP_IMPORT.pmiPutLabel(typ, ident, inst, c_char_p(name), c_char_p(content))
        if status < 0:
            raise pmiErr(status)
        return status

    @staticmethod
    def pmiDump():
        """PMI - dump the current Log Import contexts (diagnostic) """
        LIBPCP_IMPORT.pmiDump()

    def pmiEnd(self):
        """PMI - close current context and finish a Log Import archive """
        status = LIBPCP_IMPORT.pmiUseContext(self._ctx)
        if status < 0:
            raise pmiErr(status)
        status = LIBPCP_IMPORT.pmiEnd()
        self._ctx = -1
        if status < 0:
            raise pmiErr(status)
        return status
