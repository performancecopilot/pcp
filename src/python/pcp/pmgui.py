# pylint: disable=C0103
""" Wrapper module for libpcp_gui - PCP Graphical User Interface clients """
#
# Copyright (C) 2012-2013 Red Hat Inc.
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


##############################################################################
#
# imports
#

# constants adapted from C header file <pcp/pmapi.h>
from pmapi import pmErr
from cpmapi import PM_ERR_IPC

# for interfacing with libpcp - the client-side C API
from ctypes import CDLL, Structure, POINTER, cast, byref
from ctypes import c_void_p, c_char_p, c_int, c_long
from ctypes.util import find_library


##############################################################################
#
# dynamic library loads
#

LIBPCP_GUI = CDLL(find_library("pcp_gui"))
LIBC = CDLL(find_library("c"))


##############################################################################
#
# definition of structures used by C library libpcp, derived from <pcp/pmafm.h>
#

class pmRecordHost(Structure):
    """state information between the recording session and the pmlogger """
    _fields_ = [("f_config", c_void_p),
                ("fd_ipc", c_int),
                ("logfile", c_char_p),
                ("pid", c_int),
                ("status", c_int)]


##############################################################################
#
# GUI API function prototypes
#

##
# PMAPI Record-Mode Services

LIBPCP_GUI.pmRecordSetup.restype = c_long
LIBPCP_GUI.pmRecordSetup.argtypes = [c_char_p, c_char_p, c_int]

LIBPCP_GUI.pmRecordAddHost.restype = c_int
LIBPCP_GUI.pmRecordAddHost.argtypes = [
        c_char_p, c_int, POINTER(POINTER(pmRecordHost))]

LIBPCP_GUI.pmRecordControl.restype = c_int
LIBPCP_GUI.pmRecordControl.argtypes = [POINTER(pmRecordHost), c_int, c_char_p]



#LIBPCP_GUI.pmTimeConnect.restype = c_int
#LIBPCP_GUI.pmTimeConnect.argtypes = [ ]

#LIBPCP_GUI.pmTimeDisconnect.restype = c_int
#LIBPCP_GUI.pmTimeDisconnect.argtypes = [ ]

#LIBPCP_GUI.pmTimeGetPort.restype = c_int
#LIBPCP_GUI.pmTimeGetPort.argtypes = [ ]

#LIBPCP_GUI.pmTimeRecv.restype = c_int
#LIBPCP_GUI.pmTimeRecv.argtypes = [ ]

#LIBPCP_GUI.pmTimeSendAck.restype = c_int
#LIBPCP_GUI.pmTimeSendAck.argtypes = [ ]

#LIBPCP_GUI.pmTimeSendBounds.restype = c_int
#LIBPCP_GUI.pmTimeSendBounds.argtypes = [ ]

#LIBPCP_GUI.pmTimeSendMode.restype = c_int
#LIBPCP_GUI.pmTimeSendMode.argtypes = [ ]

#LIBPCP_GUI.pmTimeSendPosition.restype = c_int
#LIBPCP_GUI.pmTimeSendPosition.argtypes = [ ]

#LIBPCP_GUI.pmTimeSendTimezone.restype = c_int
#LIBPCP_GUI.pmTimeSendTimezone.argtypes = [ ]

#LIBPCP_GUI.pmTimeShowDialog.restype = c_int
#LIBPCP_GUI.pmTimeShowDialog.argtypes = [ ]

#LIBPCP_GUI.pmTimeGetStatePixmap.restype = c_int
#LIBPCP_GUI.pmTimeGetStatePixmap.argtypes = [ ]



##############################################################################
#
# class GuiClient
#
# This class wraps the GUI API library functions
#

class GuiClient(object):
    """ Provides metric recording and time control interfaces
    """

    ##
    # Record-Mode Services

    @staticmethod
    def pmRecordSetup(folio, creator, replay):
        """ GUI API - Setup an archive recording session
        File* file = pmRecordSetup("folio", "creator", 0)
        """
        file_result = LIBPCP_GUI.pmRecordSetup(
                                c_char_p(folio), c_char_p(creator), replay)
        if (file_result == 0):
            raise pmErr, file_result
        return file_result

    @staticmethod
    def pmRecordAddHost(host, isdefault, config):
        """ GUI API - Adds host to an archive recording session
        (status, recordhost) = pmRecordAddHost("host", 1, "configuration")
        """
        rhp = POINTER(pmRecordHost)()
        status = LIBPCP_GUI.pmRecordAddHost(
                                c_char_p(host), isdefault, byref(rhp))
        if status < 0:
            raise pmErr, status
        status = LIBC.fputs(c_char_p(config), c_long(rhp.contents.f_config))
        if (status < 0):
            LIBC.perror(c_char_p(""))
            raise pmErr, status
        return status, rhp

    @staticmethod
    def pmRecordControl(rhp, request, options):
        """PMAPI - Control an archive recording session
        status = pmRecordControl(0, cpmgui.PM_RCSETARG, "args")
        status = pmRecordControl(0, cpmgui.PM_REC_ON)
        status = pmRecordControl(0, cpmgui.PM_REC_OFF)
        """
        status = LIBPCP_GUI.pmRecordControl(
                                cast(rhp, POINTER(pmRecordHost)),
                                request, c_char_p(options))
        if status < 0 and status != PM_ERR_IPC:
            raise pmErr, status
        return status

    ##
    # GUI API Time Control Services
    # (Not Yet Implemented)


