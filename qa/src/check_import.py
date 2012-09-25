#!/usr/bin/python
#
# Copyright (C) 2012 Red Hat Inc.
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

import os
import pmapi
from pcp import *
from ctypes import *

def check_import (archive, hostname, timezone):

    pmi = pmiLogImport(archive)
    code = pmi.pmiSetHostname(hostname)
    if (code < 0):
        print "pmiSetHostname: ", pmi.pmiErrStr(code)
    code = pmi.pmiSetTimezone(timezone)
    if (code < 0):
        print "pmiSetTimezone: ", pmi.pmiErrStr(code)

    pmid = pmi.pmiID(60, 2, 0)
    indom = pmi.pmiInDom(60, 2)
    units = pmi.pmiUnits(0,0,0,0,0,0)

    # create a metric with no instances (hinv.ncpu)
    code = pmi.pmiAddMetric("hinv.ncpu", PM_ID_NULL, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, units)
    if (code < 0):
        print "pmiAddMetric: adding hinv.ncpu - ", pmi.pmiErrStr(code)
    # give it a value
    code = pmi.pmiPutValue("hinv.ncpu", "", "42")
    if (code < 0):
        print "pmiPutValue: hinv.ncpu - ", pmi.pmiErrStr(code)

    # create a metric with instances (kernel.all.load)
    code = pmi.pmiAddMetric("kernel.all.load", pmid, PM_TYPE_FLOAT, indom, PM_SEM_DISCRETE, units)
    if (code < 0):
        print "pmiAddMetric: adding kernel.all.load - ", pmi.pmiErrStr(code)
    code = pmi.pmiAddInstance(indom, "1 minute", 1)
    if (code < 0):
        print "pmiAddInstance: adding kernel.all.load[1] - ", pmi.pmiErrStr(code)
    code = pmi.pmiAddInstance(indom, "5 minute", 5)
    if (code < 0):
        print "pmiAddInstance: adding kernel.all.load[5] - ", pmi.pmiErrStr(code)
    code = pmi.pmiAddInstance(indom, "15 minute", 15)
    if (code < 0):
        print "pmiAddInstance: adding kernel.all.load[15] - ", pmi.pmiErrStr(code)

    # give them values
    code = pmi.pmiPutValue("kernel.all.load", "1 minute", "0.01")
    if (code < 0):
        print "pmiPutValue: kernel.all.load[1 minute] - ", pmi.pmiErrStr(code)
    code = pmi.pmiPutValue("kernel.all.load", "5 minute", "0.05")
    if (code < 0):
        print "pmiPutValue: kernel.all.load[5 minute] - ", pmi.pmiErrStr(code)
    code = pmi.pmiPutValue("kernel.all.load", "15 minute", "0.15")
    if (code < 0):
        print "pmiPutValue: kernel.all.load[15 minute] - ", pmi.pmiErrStr(code)

    del pmi

if __name__ == '__main__':

    if (len(sys.argv) != 2):
        print "Usage: " + sys.argv[0] + " <path>"
        sys.exit(1)

    check_import(sys.argv[1], "www.abc.com", "EST-10")

