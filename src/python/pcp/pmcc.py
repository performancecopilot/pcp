""" Convenience Classes building on the base PMAPI extension module """
#
# pmcc.py
#
# Copyright (C) 2013-2014 Red Hat
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

import sys
from ctypes import c_uint, c_char_p
from pcp.pmapi import pmContext, pmErr, pmResult, LIBPCP
from cpmapi import PM_CONTEXT_HOST, PM_INDOM_NULL, PM_IN_NULL, PM_ID_NULL


class MetricCore(object):
    """
    Core metric information that can be queried from the PMAPI
    PMAPI metrics are unique by name, and MetricCores should be also
    rarely, some PMAPI metrics with different names might have identical PMIDs
    PMAPI metrics are unique by (name) and by (name,pmid) - _usually_ by (pmid)
    too.
    """

    def __init__(self, ctx, name, pmid):
        self.ctx = ctx
        self.name = name
        self.pmid = pmid
        self.desc = None
        self.text = None
        self.help = None


class Metric(object):
    """
    Additional metric information, such as conversion factors and values
    several instances of Metric may share a MetricCore instance
    """

    ##
    # constructor

    def __init__(self, core):
        self._core = core
        self._values = None
        self._prevValues = None
        self._convType = core.desc.contents.type
        self._convUnits = core.desc.contents.units
        self._errorStatus = None
        self._netValues = None
        self._netPrevValues = None

    ##
    # core property read methods

    def _R_ctx(self):
        return self._core.ctx
    def _R_name(self):
        return self._core.name
    def _R_pmid(self):
        return self._core.pmid
    def _R_desc(self):
        return self._core.desc
    def _R_text(self):
        return self._core.text
    def _R_help(self):
        return self._core.help

    ## 
    # instance property read methods

    def computeValues(self, inValues):
        """ compute value """
        vset = inValues
        ctx = self.ctx
        instD = ctx.mcGetInstD(self.desc.indom)
        numval = vset.numval
        valL = []
        for i in range(numval):
            instval = vset.vlist[i].inst
            name = instD[instval]
            outAtom = self.ctx.pmExtractValue(
                     vset.valfmt, vset.vlist[i],
                     self.desc.type, self._convType)
            if self._convUnits:
                outAtom = self.ctx.pmConvScale(
                     self._convType, outAtom,
                     self.desc.units, self._convUnits)
            value = outAtom.dref(self._convType)
            valL.append((instval, name, value))
        return valL

    def _R_values(self):
        return self._values
    def _R_prevValues(self):
        return self._prevValues
    def _R_convType(self):
        return self._convType
    def _R_convUnits(self):
        return self._convUnits
    def _R_errorStatus(self):
        return self._errorStatus

    def _R_netPrevValues(self):
        if not self._prevValues:
            return None
        if type(self._netPrevValues) == type(None):
            self._netPrevValues = self.computeValues(self._prevValues)
        return self._netPrevValues
    def _R_netValues(self):
        if not self._values:
            return None
        if type(self._netValues) == type(None):
            self._netValues = self.computeValues(self._values)
        return self._netValues

    def _W_values(self, values):
        self._prev = self._values
        self._values = value
        self._netPrev = self._netValue
        self._netValue = None
    def _W_convType(self, value):
        self._convType = value
    def _W_convUnits(self, value):
        self._convUnits = value

    # interface to properties in MetricCore
    ctx = property(_R_ctx, None, None, None)
    name = property(_R_name, None, None, None)
    pmid = property(_R_pmid, None, None, None)
    desc = property(_R_desc, None, None, None)
    text = property(_R_text, None, None, None)
    help = property(_R_help, None, None, None)

    # properties specific to this instance
    values = property(_R_values, _W_values, None, None)
    prevValues = property(_R_prevValues, None, None, None)
    convType = property(_R_convType, _W_convType, None, None)
    convUnits = property(_R_convUnits, _W_convUnits, None, None)
    errorStatus = property(_R_errorStatus, None, None, None)
    netValues = property(_R_netValues, None, None, None)
    netPrevValues = property(_R_netPrevValues, None, None, None)

    def metricPrint(self):
        print self.ctx.pmIDStr(self.pmid), self.name
        indomstr = self.ctx.pmInDomStr(self.desc.indom)
        print "   ", "indom:", indomstr
        instD = self.ctx.mcGetInstD(self.desc.indom)
        for inst, name, val in self.netValues:
            print "   ", name, val


class MetricCache(pmContext):
    """
    A cache of MetricCores is kept to reduce calls into the PMAPI library
    this also slightly reduces the memory footprint of Metric instances
    that share a common MetricCore
    a cache of instance domain information is also kept, which further
    reduces calls into the PMAPI and reduces the memory footprint of
    Metric objects that share a common instance domain
    """

    ##
    # overloads
  
    def __init__(self, typed = PM_CONTEXT_HOST, target = "local:"):
        pmContext.__init__(self, typed, target)
        self._mcIndomD = {}
        self._mcByNameD = {}
        self._mcByPmidD = {}

    ##
    # methods
  
    def mcGetInstD(self, indom):
        return self._mcIndomD[indom]

    def _mcAdd(self, core):
        indom = core.desc.contents.indom
        if not self._mcIndomD.has_key(indom):
            if indom == PM_INDOM_NULL:
                instmap = { PM_IN_NULL : "PM_IN_NULL" }
            else:
                instL, nameL = self.pmGetInDom(indom)
                instmap = dict(zip(instL, nameL))
            self._mcIndomD.update({indom: instmap})

        self._mcByNameD.update({core.name: core})
        self._mcByPmidD.update({core.pmid: core})

    def mcGetCoresByName(self, nameL):
        coreL = []
        missD = None
        errL = None
        # lookup names in cache
        for index, name in enumerate(nameL):
            # lookup metric core in cache
            core = self._mcByNameD.get(name)
            if not core:
                # cache miss
                if not missD:
                    missD = {}
                missD.update({name: index})
            coreL.append(core)

        # some cache lookups missed, fetch pmids and build missing MetricCores
        if missD:
            idL, errL = self.mcFetchPmids(missD.keys())
            for name, pmid in idL:
                if pmid == PM_ID_NULL:
                    # fetch failed for the given metric name
                    if not errL:
                        errL = []
                    errL.append(name)
                else:
                    # create core
                    newcore = self._mcCreateCore(name, pmid)
                    # update core ref in return list
                    coreL[missD[name]] = newcore

        return coreL, errL
    
    def _mcCreateCore(self, name, pmid):
        newcore = MetricCore(self, name, pmid)
        try:
            newcore.desc = self.pmLookupDesc(pmid)
        except pmErr, error:
            print "pmLookupDesc: ", error

        # insert core into cache
        self._mcAdd(newcore)
        return newcore

    def mcFetchPmids(self, nameL):
        # note: some names have identical pmids
        errL = None
        nameA = (c_char_p * len(nameL))()
        for index, name in enumerate(nameL):
            nameA[index] = c_char_p(name)
        try:
            pmidArray = self.pmLookupName(nameA)
            if len(pmidArray) < len(nameA):
                print "lookup failed: got ", len(pmidArray), " of ", len(nameA)
        except pmErr, error:
            print "pmLookupName: ", error

        return zip(nameA, pmidArray), errL


class MetricGroup(dict):
    """
    Manages a group of metrics for fetching the values of
    a MetricGroup is a dictionary of Metric objects, for which data can
    be fetched from a target system using a single call to pmFetch
    the Metric objects are indexed by the metric name
    pmFetch fetches data for a list of pmIDs, so there is also a shadow
    dictionary keyed by pmID, along with a shadow list of pmIDs
    """

    ##
    # property read methods

    def _R_contextCache(self):
        return self._ctx
    def _R_pmidArray(self):
        return self._pmidArray
    def _R_timestamp(self):
        return self._result.contents.timestamp
    def _R_result(self):
        return self._result
    def _R_prevTimestamp(self):
        return self._prev.contents.timestamp
    def _R_prev(self):
        return self._prev

    ##
    # property write methods

    def _W_result(self, pmresult):
        self._prev = self._result
        self._result = pmresult

    ##
    # property definitions

    contextCache = property(_R_contextCache, None, None, None)
    pmidArray = property(_R_pmidArray, None, None, None)
    result = property(_R_result, _W_result, None, None)
    timestamp = property(_R_timestamp, None, None, None)
    prev = property(_R_prev, None, None, None)
    prevTimestamp = property(_R_prevTimestamp, None, None, None)

    ##
    # overloads

    def __init__(self, contextCache, inL = []):
        self._ctx = contextCache
        self._pmidArray = None
        self._result = None
        self._prev = None
        self._altD = {}
        dict.__init__(self)
        self.mgAdd(inL)

    ##
    # methods

    def mgAdd(self, nameL):
        coreL, errL = self._ctx.mcGetCoresByName(nameL)
        for core in coreL:
            metric = Metric(core)
            self.update({metric.name: metric})
            self._altD.update({metric.pmid: metric})
        n = len(self)
        self._pmidArray = (c_uint * n)()
        for x, key in enumerate(self.keys()):
            self._pmidArray[x] = c_uint(self[key].pmid)

    def mgFetch(self):
        # fetch the metric values
        try:
            self.result = self._ctx.pmFetch(self._pmidArray)
            # update the result entries in each metric
            result = self.result.contents
            for i in range(result.numpmid):
                pmid = result.get_pmid(i)
                vset = result.get_vset(i)
                self._altD[pmid].vset = vset
        except pmErr, error:
            print "pmFetch: ", error


class MetricGroupManager(dict, MetricCache):
    """
    Manages a dictionary of MetricGroups which can be pmFetch'ed
    inherits from MetricCache, which inherits from pmContext
    """

    ##
    # overloads

    def __init__(self, typed = PM_CONTEXT_HOST, target = "local:"):
        MetricCache.__init__(self, typed, target)
        dict.__init__(self)

    def __setitem__(self, attr, value = []):
        if self.has_key(attr):
            raise KeyError, "metric group with that key already exists"
        else:
            dict.__setitem__(self, attr, MetricGroup(self, inL = value))
    

