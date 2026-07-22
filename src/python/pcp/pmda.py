"""Wrapper module for libpcp_pmda - Performace Co-Pilot Domain Agent API
#
# Copyright (C) 2013-2015,2017-2021,2025 Red Hat.
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
# See pmdasimple.py for an example use of this module.
"""
# pylint: disable=consider-using-dict-items,no-member
# pylint: disable=too-many-arguments,too-many-positional-arguments

import os
import sys

import cpmapi
import cpmda
from pcp.pmapi import pmContext as PCP
from pcp.pmapi import pmID, pmInDom, pmDesc, pmUnits, pmErr, pmLabelSet

from ctypes.util import find_library
from ctypes import CDLL, c_int, c_long, c_char_p, c_void_p
from ctypes import addressof, byref, POINTER, Structure
from typing import Any, Callable, Optional, Union

## Performance Co-Pilot PMDA library (C)
LIBPCP_PMDA = CDLL(find_library("pcp_pmda"))


###
## PMDA Indom Cache Services
#
LIBPCP_PMDA.pmdaCacheStore.restype = c_int
LIBPCP_PMDA.pmdaCacheStore.argtypes = [pmInDom, c_int, c_char_p, c_void_p]
LIBPCP_PMDA.pmdaCacheStoreKey.restype = c_int
LIBPCP_PMDA.pmdaCacheStoreKey.argtypes = [
        pmInDom, c_int, c_char_p, c_int, c_void_p]
LIBPCP_PMDA.pmdaCacheLookup.restype = c_int
LIBPCP_PMDA.pmdaCacheLookup.argtypes = [
        pmInDom, c_int, POINTER(c_char_p), POINTER(c_void_p)]
LIBPCP_PMDA.pmdaCacheLookupName.restype = c_int
LIBPCP_PMDA.pmdaCacheLookupName.argtypes = [
        pmInDom, c_char_p, POINTER(c_int), POINTER(c_void_p)]
LIBPCP_PMDA.pmdaCacheLookupKey.restype = c_int
LIBPCP_PMDA.pmdaCacheLookupKey.argtypes = [
        pmInDom, c_char_p, c_int, c_void_p, POINTER(c_char_p),
        POINTER(c_int), POINTER(c_void_p)]
LIBPCP_PMDA.pmdaCacheOp.restype = c_int
LIBPCP_PMDA.pmdaCacheOp.argtypes = [pmInDom, c_long]
LIBPCP_PMDA.pmdaCacheResize.restype = c_int
LIBPCP_PMDA.pmdaCacheResize.argtypes = [pmInDom, c_int]

LIBPCP_PMDA.pmdaAddLabels.restype = c_int
LIBPCP_PMDA.pmdaAddLabels.argtypes = [POINTER(POINTER(pmLabelSet)), c_char_p]
LIBPCP_PMDA.pmdaAddLabelFlags.restype = c_int
LIBPCP_PMDA.pmdaAddLabelFlags.argtypes = [POINTER(pmLabelSet), c_int]
LIBPCP_PMDA.pmdaAddNotes.restype = c_int
LIBPCP_PMDA.pmdaAddNotes.argtypes = [POINTER(POINTER(pmLabelSet)), c_char_p]

LIBPCP_PMDA.pmdaGetContext.restype = c_int
LIBPCP_PMDA.pmdaGetContext.argtypes = []

def pmdaAddLabels(label: Union[str, bytes]) -> Any:
    result_p = POINTER(pmLabelSet)()
    status = LIBPCP_PMDA.pmdaAddLabels(byref(result_p), label)
    if status < 0:
        raise pmErr(status)
    return result_p

def pmdaAddLabelFlags(labels: Any, flags: int) -> int:
    status = LIBPCP_PMDA.pmdaAddLabelFlags(labels, flags)
    if status < 0:
        raise pmErr(status)
    return status

def pmdaAddNotes(label: Union[str, bytes]) -> Any:
    result_p = POINTER(pmLabelSet)()
    status = LIBPCP_PMDA.pmdaAddNotes(byref(result_p), label)
    if status < 0:
        raise pmErr(status)
    return result_p

def pmdaGetContext() -> int:
    status = LIBPCP_PMDA.pmdaGetContext()
    if status < 0:
        raise pmErr(status)
    return status

##
# Definition of structures used by C library libpcp_pmda, derived from <pcp/pmda.h>
#

class pmdaMetric(Structure):
    """ Structure describing a metric definition for a PMDA """
    _fields_ = [("m_user", c_void_p),
                ("m_desc", pmDesc)]

    def __init__(self, pmid: int, typeof: int, indom: pmInDom,
                 sem: int, units: pmUnits) -> None:
        Structure.__init__(self)
        self.m_user = None
        self.m_desc.pmid = pmid
        self.m_desc.type = typeof
        self.m_desc.indom = indom
        self.m_desc.sem = sem
        self.m_desc.units = units

    def __str__(self):
        return "pmdaMetric@%#lx pmid=%#lx type=%d" % (addressof(self), self.m_desc.pmid, self.m_desc.type)

class pmdaInstid(Structure):
    """ Structure describing an instance (index/name) a PMDA """
    _fields_ = [("i_inst", c_int),
                ("i_name", c_char_p)]

    def __init__(self, instid: int, name: str) -> None:
        Structure.__init__(self)
        self.i_inst = instid
        self.i_name = name.encode('utf-8')

    def __str__(self):
        return "pmdaInstid@%#lx index=%d name=%s" % (addressof(self), self.i_inst, self.i_name)

class pmdaIndom(Structure):
    """ Structure describing an instance domain within a PMDA """
    _fields_ = [("it_indom", pmInDom),
                ("it_numinst", c_int),
                ("it_set", POINTER(pmdaInstid))]

    def __init__(self, indom: pmInDom,
                 insts: Optional[Union[dict, list]]) -> None:
        Structure.__init__(self)
        self.it_numinst = 0
        self.it_set = None
        self.it_indom = indom
        self.load_indom(indom, insts)
        self.set_instances(indom, insts)

    def __iter__(self):
        # Generates an iterator for the cache.
        if self.it_numinst <= 0:
            LIBPCP_PMDA.pmdaCacheOp(self.it_indom,
                                    cpmda.PMDA_CACHE_WALK_REWIND)
            while 1:
                inst = LIBPCP_PMDA.pmdaCacheOp(self.it_indom,
                                               cpmda.PMDA_CACHE_WALK_NEXT)
                if inst < 0:
                    break
                name = self.inst_name_lookup(inst)
                if name:
                    yield (inst, name)
        else:
            for i in range(self.it_numinst):
                inst = self.it_set[i].i_inst
                name = self.inst_name_lookup(inst)
                if name:
                    yield (inst, name)

    def inst_name_lookup(self, instance: int) -> Optional[str]:
        if self.it_numinst <= 0:
            name = (c_char_p)()
            sts = LIBPCP_PMDA.pmdaCacheLookup(self.it_indom, instance,
                                              byref(name), None)
            if sts == cpmda.PMDA_CACHE_ACTIVE:
                return str(name.value.decode())
        elif self.it_numinst > 0:
            for inst in self.it_set:
                if inst.i_inst == instance:
                    return str(inst.i_name.decode())
        return None

    def load_indom(self, indom: pmInDom,
                   insts: Optional[Union[dict, list]]) -> None:
        if isinstance(insts, dict):
            LIBPCP_PMDA.pmdaCacheOp(indom, cpmda.PMDA_CACHE_LOAD)

    def load(self) -> None:
        if self.it_numinst == -1:
            LIBPCP_PMDA.pmdaCacheOp(self.it_indom, cpmda.PMDA_CACHE_LOAD)

    def set_list_instances(self, insts: list) -> None:
        instance_count = len(insts)
        instance_array = (pmdaInstid * instance_count)()
        for i in range(instance_count):
            instance_array[i].i_inst = insts[i].i_inst
            instance_array[i].i_name = insts[i].i_name
        self.it_set = instance_array
        self.it_numinst = instance_count
        cpmda.set_need_refresh()

    def set_dict_instances(self, indom: pmInDom, insts: dict) -> None:
        LIBPCP_PMDA.pmdaCacheOp(indom, cpmda.PMDA_CACHE_INACTIVE)
        for key in insts.keys():
            key8 = key.encode('utf-8')
            LIBPCP_PMDA.pmdaCacheStore(indom, cpmda.PMDA_CACHE_ADD, key8, byref(insts[key]))
        LIBPCP_PMDA.pmdaCacheOp(indom, cpmda.PMDA_CACHE_SAVE)
        self.it_numinst = -1

    def set_instances(self, indom: pmInDom,
                      insts: Optional[Union[dict, list]]) -> None:
        if insts is None:
            self.it_numinst = 0          # not yet known if cache indom or not
        elif isinstance(insts, dict):
            self.it_numinst = -1         # signifies cache indom (no it_set)
            self.set_dict_instances(indom, insts)
        else:
            self.it_numinst = len(insts) # signifies an old-school array indom
            self.set_list_instances(insts)

    def __str__(self):
        return "pmdaIndom@%#lx indom=%#lx num=%d" % (addressof(self), self.it_indom, self.it_numinst)

    def cache_load(self) -> None:
        if self.it_numinst <= 0:
            sts = LIBPCP_PMDA.pmdaCacheOp(self.it_indom, cpmda.PMDA_CACHE_LOAD)
            if sts < 0:
                raise pmErr(sts)
        else:
            raise pmErr(cpmapi.PM_ERR_NYI)

    def cache_mark_active(self) -> None:
        if self.it_numinst <= 0:
            LIBPCP_PMDA.pmdaCacheOp(self.it_indom, cpmda.PMDA_CACHE_ACTIVE)
        else:
            raise pmErr(cpmapi.PM_ERR_NYI)

    def cache_mark_inactive(self) -> None:
        if self.it_numinst <= 0:
            LIBPCP_PMDA.pmdaCacheOp(self.it_indom, cpmda.PMDA_CACHE_INACTIVE)
        else:
            raise pmErr(cpmapi.PM_ERR_NYI)

    def cache_resize(self, maximum: int) -> None:
        if self.it_numinst <= 0:
            sts = LIBPCP_PMDA.pmdaCacheResize(self.it_indom, maximum)
            if sts < 0:
                raise pmErr(sts)
        else:
            raise pmErr(cpmapi.PM_ERR_NYI)

class pmdaUnits(pmUnits):
    """ Wrapper class for PMDAs defining their metrics (avoids pmapi import) """
    def __init__(self, dimS: int, dimT: int, dimC: int,
                 scaleS: int, scaleT: int, scaleC: int) -> None:
        pmUnits.__init__(self, dimS, dimT, dimC, scaleS, scaleT, scaleC)


###
## Convenience Classes for PMDAs
#

class MetricDispatch(object):
    """ Helper for PMDA class which manages metric dispatch
        table, metric namespace, descriptors and help text.

        Overall strategy is to interface to the C code in
        python/pmda.c here, using a void* handle to the PMDA
        dispatch structure (allocated and managed in C code).

        In addition, several dictionaries for metric related
        strings are managed here (names, help text) - cached
        for quick lookups in callback routines.
    """

    ##
    # overloads

    def __init__(self, domain: int, name: str,
                 logfile: str, helpfile: str) -> None:
        self._indomtable = []
        self._indoms = {}
        self._indom_oneline = {}
        self._indom_helptext = {}
        self._metrictable = []
        self._metrics = {}
        self._metric_names = {}
        self._metric_names_map = {}
        self._metric_oneline = {}
        self._metric_helptext = {}
        cpmda.init_dispatch(domain, name, logfile, helpfile)

    def clear_indoms(self) -> None:
        # Notice we're using the following:
        #
        #   del list[:]
        #   dict.clear()
        #
        # instead of:
        #
        #   list = []
        #   dict = {}
        #
        # The latter creates a new list/dictionary and assigns it to
        # the variable. But, the stashed list/dictionary reference
        # down in cpmda still will point to the original
        # list/dictionary. Clearing out the existing list/dictionary
        # keeps the stashed references working the way we'd like.
        del self._indomtable[:]
        self._indoms.clear()
        self._indom_oneline.clear()
        self._indom_helptext.clear()

    def clear_metrics(self) -> None:
        # See note above in clear_indoms() about clearing
        # lists/dictionaries.
        del self._metrictable[:]
        self._metrics.clear()
        self._metric_names.clear()
        self._metric_names_map.clear()
        self._metric_oneline.clear()
        self._metric_helptext.clear()

    def reset_metrics(self) -> None:
        self.clear_metrics()
        cpmda.set_need_refresh()

    ##
    # general PMDA class methods

    def pmns_refresh(self) -> None:
        cpmda.pmns_refresh(self._metric_names)

    def connect_pmcd(self) -> None:
        cpmda.connect_pmcd()

    def add_metric(self, name: str, metric: pmdaMetric,
                   oneline: str = '', text: str = '') -> None:
        pmid = metric.m_desc.pmid
        if name in self._metric_names_map:
            raise KeyError('attempt to add_metric with an existing name=%s' % (name))
        if pmid in self._metrics:
            raise KeyError('attempt to add_metric with an existing PMID')

        self._metrictable.append(metric)
        self._metrics[pmid] = metric
        self._metric_names[pmid] = name
        self._metric_names_map[name] = pmid
        self._metric_oneline[pmid] = oneline
        self._metric_helptext[pmid] = text
        cpmda.set_need_refresh()

    def remove_metric(self, name: str, metric: pmdaMetric) -> None:
        pmid = metric.m_desc.pmid
        if name not in self._metric_names_map:
            raise KeyError('attempt to remove non-existant metric name=%s' % (name))
        if pmid not in self._metrics:
            raise KeyError('attempt to remove_metric with non-existant PMID')

        self._metrictable.remove(metric)
        self._metrics.pop(pmid)
        self._metric_names.pop(pmid)
        self._metric_names_map.pop(name)
        self._metric_oneline.pop(pmid)
        self._metric_helptext.pop(pmid)
        cpmda.set_need_refresh()

    def add_indom(self, indom: pmdaIndom,
                  oneline: str = '', text: str = '') -> None:
        indomid = indom.it_indom
        for entry in self._indomtable:
            if entry.it_indom == indomid:
                raise KeyError('attempt to add_indom with an existing ID')
        self._indomtable.append(indom)
        self._indoms[indomid] = indom
        self._indom_oneline[indomid] = oneline
        self._indom_helptext[indomid] = text

    def replace_indom(self, indom: Union[pmdaIndom, int],
                      insts: Optional[Union[dict, list]]) -> None:
        # Note that this function can take a numeric indom or a
        # pmdaIndom.
        if isinstance(indom, pmdaIndom):
            it_indom = indom.it_indom
            replacement = indom
        else:
            it_indom = indom
            replacement = pmdaIndom(it_indom, insts)
        # list indoms need to keep the table up-to-date for libpcp_pmda
        if isinstance(insts, list):
            # _indomtable is persistently shared with pmda.c
            for i, entry in enumerate(self._indomtable):
                if entry.it_indom == it_indom:
                    self._indomtable[i] = replacement # replace in place
                    break
        self._indoms[it_indom] = replacement

    def inst_lookup(self, indom: pmInDom, instance: int) -> Optional[c_void_p]:
        """
        Lookup the value associated with an (internal) instance ID
        within a specific instance domain (only valid with indoms
        of cache type - array indom will always return None).
        """
        entry = self._indoms[indom]
        if entry.it_numinst < 0:
            value = (c_void_p)()
            sts = LIBPCP_PMDA.pmdaCacheLookup(indom, instance, None, byref(value))
            if sts == cpmda.PMDA_CACHE_ACTIVE:
                return value
        return None

    def inst_name_lookup(self, indom: pmInDom, instance: int) -> Optional[str]:
        """
        Lookup the name associated with an (internal) instance ID within
        a specific instance domain.
        """
        entry = self._indoms[indom]
        return entry.inst_name_lookup(instance)

    def pmid_name_lookup(self, cluster: int, item: int) -> Optional[str]:
        """
        Lookup the name associated with a performance metric identifier.
        """
        try:
            name = self._metric_names[cpmda.pmda_pmid(cluster, item)]
        except KeyError:
            name = None
        return name


class PMDA(MetricDispatch):
    """ Defines a PCP performance metrics domain agent
        Used to add new metrics into the PCP toolkit.
    """

    ##
    # property read methods

    def read_name(self) -> str:
        """ Property for name of this PMDA """
        return self._name

    def read_domain(self) -> int:
        """ Property for unique domain number of this PMDA """
        return self._domain

    ##
    # property definitions

    name = property(read_name, None, None, None)
    domain = property(read_domain, None, None, None)

    ##
    # overloads

    def __init__(self, name: str, domain: int,
                 logfile: Optional[str] = None,
                 helpfile: Optional[str] = None) -> None:
        self._name = name
        self._domain = domain
        if not logfile:
            # note: logfile == "-" is special, see pmOpenLog(3).
            logfile = name + '.log'
        pmdaname = 'pmda' + name
        if not helpfile:
            helpfile = '%s/%s/help' % (PCP.pmGetConfig('PCP_PMDAS_DIR'), name)
        MetricDispatch.__init__(self, domain, pmdaname, logfile, helpfile)


    ##
    # general PMDA class methods

    def domain_probe(self) -> int:
        """
        Probe the domain to see if the PMDA could be activated
        Used by pmcheck(1) - see man page for meaning of (int)
        return codes - as part of PMDA specific scripts.
        """
        return 99  # unknown, subclasses override to use this.

    def domain_write(self) -> None:
        """
        Write out the domain.h file (used during installation)
        """
        print('#define %s %d' % (self._name.upper(), self._domain))

    def pmns_write(self, root: str) -> None:
        """
        Write out the namespace file (used during installation)
        """
        pmns = self._metric_names
        prefixes = {pmns[key].split('.')[0] for key in pmns}
        indent = root == 'root'
        lead = ''
        if indent:
            lead = '\t'
            print('root {')
        for prefix in prefixes:
            print('%s%s\t%d:*:*' % (lead, prefix, self._domain))
        if indent:
            print('}')

    def pmda_notready(self) -> None:
        """
        Tell PMCD the PMDA is not ready to process requests.
        """
        cpmda.pmda_notready()

    def pmda_ready(self) -> None:
        """
        Tell PMCD the PMDA is ready to process requests.
        """
        cpmda.pmda_ready()

    def run(self) -> None:
        """
        All the real work happens herein; we can be called in one of three
        situations, determined by environment variables.  First one is for
        pmcheck(1), next two are part of the agent Install process (where
        the domain.h and namespace files need to be created).  The fourth
        case is the real mccoy, where an agent is actually being started
        by pmcd/dbpmda and makes use of libpcp_pmda to talk PCP protocol.
        """
        if 'PCP_PYTHON_PROBE' in os.environ:
            result = self.domain_probe()
            if isinstance(result, int):
                sys.exit(int(result))
            sys.exit(2)
        elif 'PCP_PYTHON_DOMAIN' in os.environ:
            self.domain_write()
        elif 'PCP_PYTHON_PMNS' in os.environ:
            self.pmns_write(os.environ['PCP_PYTHON_PMNS'])
        else:
            self.pmns_refresh()
            cpmda.pmid_oneline_refresh(self._metric_oneline)
            cpmda.pmid_longtext_refresh(self._metric_helptext)
            cpmda.indom_oneline_refresh(self._indom_oneline)
            cpmda.indom_longtext_refresh(self._indom_helptext)
            cpmda.pmda_dispatch(self._indomtable, self._metrictable)

    @staticmethod
    def set_fetch(fetch: Callable[..., Any]) -> None:
        return cpmda.set_fetch(fetch)

    @staticmethod
    def set_label(label: Callable[..., Any]) -> None:
        return cpmda.set_label(label)

    @staticmethod
    def set_notes(notes: Callable[..., Any]) -> None:
        return cpmda.set_notes(notes)

    @staticmethod
    def set_refresh(refresh: Callable[..., Any]) -> None:
        return cpmda.set_refresh(refresh)

    @staticmethod
    def set_instance(instance: Callable[..., Any]) -> None:
        return cpmda.set_instance(instance)

    @staticmethod
    def set_fetch_callback(fetch_callback: Callable[..., Any]) -> None:
        return cpmda.set_fetch_callback(fetch_callback)

    @staticmethod
    def set_label_callback(label_callback: Callable[..., Any]) -> None:
        return cpmda.set_label_callback(label_callback)

    @staticmethod
    def set_notes_callback(notes_callback: Callable[..., Any]) -> None:
        return cpmda.set_notes_callback(notes_callback)

    @staticmethod
    def set_attribute_callback(attribute_callback: Callable[..., Any]) -> None:
        return cpmda.set_attribute_callback(attribute_callback)

    @staticmethod
    def set_store_callback(store_callback: Callable[..., Any]) -> None:
        return cpmda.set_store_callback(store_callback)

    @staticmethod
    def set_endcontext_callback(endcontext_callback: Callable[..., Any]) -> None:
        return cpmda.set_endcontext_callback(endcontext_callback)

    @staticmethod
    def set_refresh_all(refresh_all: Callable[..., Any]) -> None:
        return cpmda.set_refresh_all(refresh_all)

    @staticmethod
    def set_refresh_metrics(refresh_metrics: Callable[..., Any]) -> None:
        return cpmda.set_refresh_metrics(refresh_metrics)

    @staticmethod
    def set_notify_change() -> None:
        cpmda.set_notify_change()

    @staticmethod
    def set_user(username: str) -> int:
        if 'PCP_PYTHON_PROBE' in os.environ:
            return cpmapi.PM_ERR_NOTCONN
        return cpmapi.pmSetProcessIdentity(username)

    @staticmethod
    def pmid(cluster: int, item: int) -> pmID:
        return cpmda.pmda_pmid(cluster, item)

    @staticmethod
    def pmid_build(domain: int, cluster: int, item: int) -> pmID:
        return cpmda.pmid_build(domain, cluster, item)

    @staticmethod
    def pmid_cluster(cluster: int) -> pmID:
        return cpmda.pmid_cluster(cluster)

    @staticmethod
    def indom(serial: int) -> pmInDom:
        return cpmda.pmda_indom(serial)

    @staticmethod
    def indom_build(domain: int, serial: int) -> pmInDom:
        return cpmda.indom_build(domain, serial)

    @staticmethod
    def units(dim_space: int, dim_time: int, dim_count: int,
              scale_space: int, scale_time: int, scale_count: int) -> pmUnits:
        return cpmda.pmda_units(dim_space, dim_time, dim_count, scale_space, scale_time, scale_count)

    @staticmethod
    def uptime(now: int) -> str:
        return cpmda.pmda_uptime(now)

    @staticmethod
    def set_comm_flags(flags: int) -> int:
        return cpmda.pmda_set_comm_flags(flags)

    @staticmethod
    def log(message: str) -> int:
        return cpmda.pmda_log(message)

    @staticmethod
    def dbg(message: str) -> int:
        return cpmda.pmda_dbg(message)

    @staticmethod
    def err(message: str) -> int:
        return cpmda.pmda_err(message)
