""" Convenience Classes building on the base PMAPI extension module """
#
# Copyright (C) 2013-2016 Red Hat
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

from sys import stderr
from ctypes import c_int, c_uint, c_char_p, cast, POINTER
from pcp.pmapi import (pmContext, pmResult, pmValueSet, pmValue, pmDesc,
        pmErr, pmOptions, timeval)
from cpmapi import (PM_CONTEXT_HOST, PM_CONTEXT_ARCHIVE, PM_INDOM_NULL,
        PM_IN_NULL, PM_ID_NULL, PM_SEM_COUNTER, PM_ERR_EOL, PM_TYPE_DOUBLE)


class MetricCore(object):
    """
    Core metric information that can be queried from the PMAPI
    PMAPI metrics are unique by name, and MetricCores should be also
    rarely, some PMAPI metrics with different names might have identical PMIDs
    PMAPI metrics are unique by (name) and by (name,pmid) - _usually_ by (pmid)
    too.  Note that names here (and only here) are stored as byte strings for
    direct PMAPI access.  All dictionaries/caching strategies built using the
    core structure use native strings (i.e., not byte strings in python3).
    """

    def __init__(self, ctx, name, pmid):
        self.ctx = ctx
        if type(name) != type(b''):
            name = name.encode('utf-8')
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
        self._core = core   # MetricCore
        self._vset = None   # pmValueSet member
        self._values = None
        self._prevvset = None
        self._prevValues = None
        self._convType = core.desc.contents.type
        self._convUnits = None
        self._errorStatus = None
        self._netValues = None # (instance, name, value)
        self._netPrevValues = None # (instance, name, value)
        self._netConvertedValues = None # (instance, name, value)

    ##
    # core property read methods

    def _R_ctx(self):
        return self._core.ctx
    def _R_name(self):
        return self._core.name.decode()
    def _R_pmid(self):
        return self._core.pmid
    def _R_desc(self):
        return self._core.desc
    def _R_text(self):
        return self._core.text
    def _R_help(self):
        return self._core.help

    def get_vlist(self, vset, vlist_idx):
        """ Return the vlist[vlist_idx] of vset[vset_idx] """
        listptr = cast(vset.contents.vlist, POINTER(pmValue))
        return listptr[vlist_idx]

    def get_inst(self, vset, vlist_idx):
        """ Return the inst for vlist[vlist_idx] of vset[vset_idx] """
        return self.get_vlist(vset, vset_idx, vlist_idx).inst

    def computeValues(self, inValues):
        """ Extract the value for a singleton or list of instances
            as a triple (inst, name, val)
        """
        vset = inValues
        ctx = self.ctx
        instD = ctx.mcGetInstD(self.desc.contents.indom)
        valL = []
        for i in range(vset.numval):
            instval = self.get_vlist(vset, i)
            try:
                name = instD[instval.inst]
            except KeyError:
                name = ''
            outAtom = self.ctx.pmExtractValue(
                    vset.valfmt, instval, self.desc.type, self._convType)
            if self._convUnits:
                desc = (POINTER(pmDesc) * 1)()
                desc[0] = self.desc
                outAtom = self.ctx.pmConvScale(
                        self._convType, outAtom, desc, 0, self._convUnits)
            value = outAtom.dref(self._convType)
            valL.append((instval, name, value))
        return valL

    def _find_previous_instval(self, index, inst, pvset):
        """ Find a metric instance in the previous resultset """
        if index <= pvset.numval:
            pinstval = self.get_vlist(pvset, index)
            if inst == pinstval.inst:
                return pinstval
        for pi in range(pvset.numval):
            pinstval = self.get_vlist(pvset, pi)
            if inst == pinstval.inst:
                return pinstval
        return None

    def convertValues(self, values, prevValues, delta):
        """ Extract the value for a singleton or list of instances as a
            triple (inst, name, val) for COUNTER metrics with the value
            delta calculation applied (for rate conversion).
        """
        if self.desc.sem != PM_SEM_COUNTER:
            return self.computeValues(values)
        if prevValues == None:
            return None
        pvset = prevValues
        vset = values
        ctx = self.ctx
        instD = ctx.mcGetInstD(self.desc.contents.indom)
        valL = []
        for i in range(vset.numval):
            instval = self.get_vlist(vset, i)
            pinstval = self._find_previous_instval(i, instval.inst, pvset)
            if pinstval == None:
                continue
            try:
                name = instD[instval.inst]
            except KeyError:
                name = ''
            outAtom = self.ctx.pmExtractValue(vset.valfmt,
                    instval, self.desc.type, PM_TYPE_DOUBLE)
            poutAtom = self.ctx.pmExtractValue(pvset.valfmt,
                    pinstval, self.desc.type, PM_TYPE_DOUBLE)
            if self._convUnits:
                desc = (POINTER(pmDesc) * 1)()
                desc[0] = self.desc
                outAtom = self.ctx.pmConvScale(
                        PM_TYPE_DOUBLE, outAtom, desc, 0, self._convUnits)
                poutAtom = self.ctx.pmConvScale(
                        PM_TYPE_DOUBLE, poutAtom, desc, 0, self._convUnits)
            value = outAtom.dref(PM_TYPE_DOUBLE)
            pvalue = poutAtom.dref(PM_TYPE_DOUBLE)
            if (value >= pvalue):
                valL.append((instval, name, (value - pvalue) / delta))
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

    def _R_netConvValues(self):
        return self._netConvValues

    def _R_netPrevValues(self):
        if not self._prevvset:
            return None
        self._netPrevValues = self.computeValues(self._prevvset)
        return self._netPrevValues

    def _R_netValues(self):
        if not self._vset:
            return None
        self._netValues = self.computeValues(self._vset)
        return self._netValues

    def _W_values(self, values):
        self._prev = self._values
        self._values = values
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
    netConvValues = property(_R_netConvValues, None, None, None)

    def metricPrint(self):
        indomstr = self.ctx.pmInDomStr(self.desc.indom)
        print("   ", "indom:", indomstr)
        instD = self.ctx.mcGetInstD(self.desc.indom)
        for inst, name, val in self.netValues:
            print("   ", name, val)

    def metricConvert(self, delta):
        convertedList = self.convertValues(self._vset, self._prevvset, delta)
        self._netConvValues = convertedList
        return self._netConvValues


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
        self._mcCounter = 0 # number of counters in this cache
        self._mcMetrics = 0 # number of metrics in this cache

    ##
    # methods
  
    def mcGetInstD(self, indom):
        """ Query the instance : instance_list dictionary """
        return self._mcIndomD[indom]

    def _mcAdd(self, core):
        """ Update the dictionary """
        indom = core.desc.contents.indom
        if indom not in self._mcIndomD:
            if c_int(indom).value == c_int(PM_INDOM_NULL).value:
                instmap = { PM_IN_NULL : b'PM_IN_NULL' }
            else:
                if self._type == PM_CONTEXT_ARCHIVE:
                    instL, nameL = self.pmGetInDomArchive(core.desc)
                else:
                    instL, nameL = self.pmGetInDom(core.desc)
                if instL != None and nameL != None:
                    instmap = dict(zip(instL, nameL))
                else:
                    instmap = {}
            self._mcIndomD.update({indom: instmap})

        if core.desc.contents.sem == PM_SEM_COUNTER:
            self._mcCounter += 1
        self._mcMetrics += 1

        self._mcByNameD.update({core.name.decode(): core})
        self._mcByPmidD.update({core.pmid: core})

    def mcGetCoresByName(self, nameL):
        """ Update the core (metric id, description,...) list """
        coreL = []
        missD = None
        errL = None
        # lookup names in cache
        for index, name in enumerate(nameL):
            if type(name) == type(b''):
                name = name.decode()
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
                    # create core pmDesc
                    newcore = self._mcCreateCore(name, pmid)
                    # update core ref in return list
                    coreL[missD[name]] = newcore

        return coreL, errL
    
    def _mcCreateCore(self, name, pmid):
        """ Update the core description """
        newcore = MetricCore(self, name, pmid)
        try:
            newcore.desc = self.pmLookupDesc(pmid)
        except pmErr as error:
            fail = "%s: pmLookupDesc: %s" % (error.progname(), error.message())
            print >> stderr, fail
            raise SystemExit(1)

        # insert core into cache
        self._mcAdd(newcore)
        return newcore

    def mcFetchPmids(self, nameL):
        """ Update the core metric ids.  note: some names have identical pmids """
        errL = None
        nameA = (c_char_p * len(nameL))()
        for index, name in enumerate(nameL):
            if type(name) != type(b''):
                name = name.encode('utf-8')
            nameA[index] = c_char_p(name)
        try:
            pmidArray = self.pmLookupName(nameA)
            if len(pmidArray) < len(nameA):
                missing = "%d of %d metric names" % (len(pmidArray), len(nameA))
                print >> stderr, "Cannot resolve", missing
                raise SystemExit(1)
        except pmErr as error:
            fail = "%s: pmLookupName: %s" % (error.progname(), error.message())
            print >> stderr, fail
            raise SystemExit(1)

        return zip(nameL, pmidArray), errL


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
    def _R_nonCounters(self):
        return self._ctx._mcCounter != self._ctx._mcMetrics
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
    nonCounters = property(_R_nonCounters, None, None, None)
    pmidArray = property(_R_pmidArray, None, None, None)
    result = property(_R_result, _W_result, None, None)
    timestamp = property(_R_timestamp, None, None, None)
    prev = property(_R_prev, None, None, None)
    prevTimestamp = property(_R_prevTimestamp, None, None, None)

    ##
    # overloads

    def __init__(self, contextCache, inL = []):
        dict.__init__(self)
        self._ctx = contextCache
        self._pmidArray = None
        self._result = None
        self._prev = None
        self._altD = {}
        self.mgAdd(inL)

    def __setitem__(self, attr, value = []):
        if attr in self:
            raise KeyError("metric group with that key already exists")
        else:
            dict.__setitem__(self, attr, MetricGroup(self, inL = value))

    ##
    # methods

    def mgAdd(self, nameL):
        """ Create the list of Metric(s) """
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
        """ Fetch the list of Metric values.  Save the old value.  """
        try:
            self.result = self._ctx.pmFetch(self._pmidArray)
            # update the result entries in each metric
            result = self.result.contents
            for i in range(self.result.contents.numpmid):
                pmid = self.result.contents.get_pmid(i)
                vset = self.result.contents.get_vset(i)
                self._altD[pmid]._prevvset = self._altD[pmid]._vset
                self._altD[pmid]._vset = vset
        except pmErr as error:
            if error.args[0] == PM_ERR_EOL:
                raise SystemExit(0)
            fail = "%s: pmFetch: %s" % (error.progname(), error.message())
            print >> stderr, fail
            raise SystemExit(1)

    def mgDelta(self):
        """
        Sample delta - used for rate conversion calculations, which
        requires timestamps from successive samples.
        """
        if self._prev != None:
            prevTimestamp = float(self.prevTimestamp)
        else:
            prevTimestamp = 0.0
        return float(self.timestamp) - prevTimestamp


class MetricGroupPrinter(object):
    """
    Handles reporting of MetricGroups within a GroupManager.
    This object is called upon at the end of each fetch when
    new values are available.  It is also responsible for
    producing any initial (or on-going) header information
    that the tool may wish to report.
    """
    def report(self, manager):
        """ Base implementation, all tools should override """
        for group_name in manager.keys():
            group = manager[group_name]
            for metric_name in group.keys():
                group[metric_name].metricPrint()

    def convert(self, manager):
        """ Do conversion for all metrics across all groups """
        for group_name in manager.keys():
            group = manager[group_name]
            delta = group.mgDelta()
            for metric_name in group.keys():
                group[metric_name].metricConvert(delta)


class MetricGroupManager(dict, MetricCache):
    """
    Manages a dictionary of MetricGroups which can be pmFetch'ed
    inherits from MetricCache, which inherits from pmContext
    """

    ##
    # property access methods

    def _R_options(self):	# command line option object
        return self._options

    def _W_options(self, options):
        self._options = options

    def _R_default_delta(self):	# default interval unless command line set
        return self._default_delta
    def _W_default_delta(self, delta):
        self._default_delta = delta
    def _R_default_pause(self):	# default reporting delay (archives only)
        return self._default_pause
    def _W_default_pause(self, pause):
        self._default_pause = pause

    def _W_printer(self, printer): 	# helper class for reporting
        self._printer = printer
    def _R_counter(self):	# fetch iteration count, useful for printer
        return self._counter

    ##
    # property definitions

    options = property(_R_options, _W_options, None, None)
    default_delta = property(_R_default_delta, _W_default_delta, None, None)
    default_pause = property(_R_default_pause, _W_default_pause, None, None)

    printer = property(None, _W_printer, None, None)
    counter = property(_R_counter, None, None, None)

    ##
    # overloads

    def __init__(self, typed = PM_CONTEXT_HOST, target = "local:"):
        dict.__init__(self)
        MetricCache.__init__(self, typed, target)
        self._options = None
        self._default_delta = timeval(1, 0)
        self._default_pause = None
        self._printer = None
        self._counter = 0

    def __setitem__(self, attr, value = []):
        if attr in self:
            raise KeyError("metric group with that key already exists")
        else:
            dict.__setitem__(self, attr, MetricGroup(self, inL = value))

    @classmethod
    def builder(build, options, argv):
        """ Helper interface, simple PCP monitor argument parsing. """
        manager = build.fromOptions(options, argv)
        manager._default_delta = timeval(options.delta, 0)
        manager._options = options
        return manager

    ##
    # methods

    def _computeSamples(self):
        """ Calculate the number of samples we are to take.
            This is based on command line options --samples but also
            must consider --start, --finish and --interval.  If none
            of these were presented, a zero return means "infinite".
            Also consider whether the utility needs rate-conversion,
            automatically increasing the sample count to accommodate
            when counters metrics are present.
        """
        if self._options == None:
            return 0	# loop until interrupted or PM_ERR_EOL
        extra = 1       # extra sample needed if rate converting
        for group in self.keys():
            if self[group].nonCounters:
                extra = 0
        samples = self._options.pmGetOptionSamples()
        if samples != None:
            return samples + extra
        if self._options.pmGetOptionFinishOptarg() == None:
            return 0	# loop until interrupted or PM_ERR_EOL
        origin = self._options.pmGetOptionOrigin()
        finish = self._options.pmGetOptionFinish()
        delta = self._options.pmGetOptionInterval()
        if delta == None:
            delta = self._default_delta
        period = (delta.tv_sec * 1.0e6 + delta.tv_usec) / 1e6
        window = float(finish.tv_sec - origin.tv_sec)
        window += float((finish.tv_usec - origin.tv_usec) / 1e6)
        window /= period
        return int(window + 0.5) + extra    # roundup to positive number

    def _computePauseTime(self):
        """ Figure out how long to sleep between samples.
            This needs to take into account whether we were explicitly
            asked for a delay (independent of context type, --pause),
            whether this is an archive or live context, and the sampling
            --interval (including the default value, if none requested).
        """
        if self._default_pause != None:
            return self._default_pause
        if self.type == PM_CONTEXT_ARCHIVE:
            self._default_pause = timeval(0, 0)
        elif self._options != None:
            pause = self._options.pmGetOptionInterval()
            if pause != None:
                self._default_pause = pause
            else:
                self._default_pause = self._default_delta
        else:
            self._default_pause = self._default_delta
        return self._default_pause

    def checkMissingMetrics(self, nameL):
        """
        Return a list of metrics that are missing from the default context.
        This is usually only applicable when replaying archives.
        Return None if all found, else a list of missing metric names.
        """
        missing = []
        nameA = (c_char_p * len(nameL))()
        for i, n in enumerate(nameL):
            if type(n) != type(b''):
                n = n.encode('utf-8')
            nameA[i] = c_char_p(n)
        try:
            # fast path: check all in one call
            pmContext.pmLookupName(self, nameA)
        except pmErr as err:
            # failure: check all names individually
            for i, n in enumerate(nameA):
                try:
                    pmContext.pmLookupName(self, (n))
                except pmErr as err:
                    missing.append(nameL[i])
        if len(missing) == 0:
            return None 
        return missing 

    def fetch(self):
        """ Perform fetch operation on all of the groups. """
        for group in self.keys():
            self[group].mgFetch()

    def run(self):
        """ Using options specification, loop fetching and reporting,
            pausing for the requested time interval between updates.
            Transparently handles archive/live mode differences.
            Note that this can be different to the sampling interval
            in archive mode, but is usually the same as the sampling
            interval in live mode.
        """
        samples = self._computeSamples()
        timer = self._computePauseTime()
        try:
            self.fetch()
            while True:
                self._counter += 1
                if samples == 0 or self._counter <= samples:
                    self._printer.report(self)
                if self._counter == samples:
                    break
                timer.sleep()
                self.fetch()
        except SystemExit as code:
            return code
        except KeyboardInterrupt:
            pass
        return 0
