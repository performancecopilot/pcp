# pylint: disable=C0103
"""Wrapper module for libpcp_pmda - Performace Co-Pilot Domain Agent API
#
# Copyright (C) 2013-2015 Red Hat.
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

import os

import cpmapi
import cpmda
from pcp.pmapi import pmContext as PCP
from pcp.pmapi import pmInDom, pmDesc, pmUnits, pmErr

from ctypes.util import find_library
from ctypes import CDLL, c_int, c_long, c_char_p, c_void_p, cast, byref
from ctypes import addressof, sizeof, POINTER, Structure, create_string_buffer

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


##
# Definition of structures used by C library libpcp_pmda, derived from <pcp/pmda.h>
#

class pmdaMetric(Structure):
    """ Structure describing a metric definition for a PMDA """
    _fields_ = [("m_user", c_void_p),
                ("m_desc", pmDesc)]

    def __init__(self, pmid, typeof, indom, sem, units):
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

    def __init__(self, instid, name):
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

    def __init__(self, indom, insts):
        Structure.__init__(self)
        self.it_numinst = 0
        self.it_set = None
        self.it_indom = indom
        self.set_instances(indom, insts)

    def __iter__(self):
        # Generates an iterator for the cache.
        if self.it_numinst < 0:
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

    def inst_name_lookup(self, instance):
        if self.it_numinst < 0:
            name = (c_char_p)()
            sts = LIBPCP_PMDA.pmdaCacheLookup(self.it_indom, instance,
                                              byref(name), None)
            if (sts == cpmda.PMDA_CACHE_ACTIVE):
                return str(name.value.decode())
        elif self.it_numinst > 0:
            for inst in self.it_set:
                if (inst.i_inst == instance):
                    return str(inst.i_name.decode())
        return None

    def set_list_instances(self, insts):
        instance_count = len(insts)
        if (instance_count == 0):
            return
        instance_array = (pmdaInstid * instance_count)()
        for i in range(instance_count):
            instance_array[i].i_inst = insts[i].i_inst
            instance_array[i].i_name = insts[i].i_name
        self.it_set = instance_array
        self.it_numinst = instance_count

    def set_dict_instances(self, indom, insts):
        LIBPCP_PMDA.pmdaCacheOp(indom, cpmda.PMDA_CACHE_INACTIVE)
        for key in insts.keys():
            key8 = key.encode('utf-8')
            LIBPCP_PMDA.pmdaCacheStore(indom, cpmda.PMDA_CACHE_ADD, key8, byref(insts[key]))
        LIBPCP_PMDA.pmdaCacheOp(indom, cpmda.PMDA_CACHE_SAVE)

    def set_instances(self, indom, insts):
        if (insts == None):
            self.it_numinst = 0          # not yet known if cache indom or not
        elif (isinstance(insts, dict)):
            self.it_numinst = -1         # signifies cache indom (no it_set)
            self.set_dict_instances(indom, insts)
        else:
            self.it_numinst = len(insts) # signifies an old-school array indom
            self.set_list_instances(insts)

    def __str__(self):
        return "pmdaIndom@%#lx indom=%#lx num=%d" % (addressof(self), self.it_indom, self.it_numinst)

    def cache_load(self):
        if self.it_numinst <= 0:
            sts = LIBPCP_PMDA.pmdaCacheOp(self.it_indom, cpmda.PMDA_CACHE_LOAD)
            if sts < 0:
                raise pmErr(sts)
        else:
            raise pmErr(cpmapi.PM_ERR_NYI)

    def cache_mark_active(self):
        if self.it_numinst <= 0:
            LIBPCP_PMDA.pmdaCacheOp(self.it_indom, cpmda.PMDA_CACHE_ACTIVE)
        else:
            raise pmErr(cpmapi.PM_ERR_NYI)

    def cache_mark_inactive(self):
        if self.it_numinst <= 0:
            LIBPCP_PMDA.pmdaCacheOp(self.it_indom, cpmda.PMDA_CACHE_INACTIVE)
        else:
            raise pmErr(cpmapi.PM_ERR_NYI)

    def cache_resize(self, maximum):
        if self.it_numinst <= 0:
            sts = LIBPCP_PMDA.pmdaCacheResize(self.it_indom, maximum)
            if sts < 0:
                raise pmErr(sts)
        else:
            raise pmErr(cpmapi.PM_ERR_NYI)

class pmdaUnits(pmUnits):
    """ Wrapper class for PMDAs defining their metrics (avoids pmapi import) """
    def __init__(self, dimS, dimT, dimC, scaleS, scaleT, scaleC):
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

    def __init__(self, domain, name, logfile, helpfile):
        self._indomtable = []
        self._indoms = {}
        self._indom_oneline = {}
        self._indom_helptext = {}
        self._metrictable = []
        self._metrics = {}
        self._metric_names = {}
        self._metric_oneline = {}
        self._metric_helptext = {}
        cpmda.init_dispatch(domain, name, logfile, helpfile)

    def clear_indoms(self):
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

    def clear_metrics(self):
        # See note above in clear_indoms() about clearing
        # lists/dictionaries.
        del self._metrictable[:]
        self._metrics.clear()
        self._metric_names.clear()
        self._metric_oneline.clear()
        self._metric_helptext.clear()

    def reset_metrics(self):
        self.clear_metrics()
        cpmda.set_need_refresh()

    ##
    # general PMDA class methods

    def pmns_refresh(self):
        cpmda.pmns_refresh(self._metric_names)

    def connect_pmcd(self):
        cpmda.connect_pmcd()

    def add_metric(self, name, metric, oneline = '', text = ''):
        pmid = metric.m_desc.pmid
        if (pmid in self._metric_names):
            raise KeyError('attempt to add_metric with an existing name')
        if (pmid in self._metrics):
            raise KeyError('attempt to add_metric with an existing PMID')

        self._metrictable.append(metric)
        self._metrics[pmid] = metric
        self._metric_names[pmid] = name
        self._metric_oneline[pmid] = oneline
        self._metric_helptext[pmid] = text
        cpmda.set_need_refresh()

    def add_indom(self, indom, oneline = '', text = ''):
        indomid = indom.it_indom
        for entry in self._indomtable:
            if (entry.it_indom == indomid):
                raise KeyError('attempt to add_indom with an existing ID')
        self._indomtable.append(indom)
        self._indoms[indomid] = indom
        self._indom_oneline[indomid] = oneline
        self._indom_helptext[indomid] = text

    def replace_indom(self, indom, insts):
        # Note that this function can take a numeric indom or a
        # pmdaIndom.
        if isinstance(indom, pmdaIndom):
            it_indom = indom.it_indom
            replacement = indom
        else:
            it_indom = indom
            replacement = pmdaIndom(it_indom, insts)
        # list indoms need to keep the table up-to-date for libpcp_pmda
        if (isinstance(insts, list)):
            for entry in self._indomtable:
                if (entry.it_indom == it_indom):
                    entry = replacement
                    break
        self._indoms[it_indom] = replacement

    def inst_lookup(self, indom, instance):
        """
        Lookup the value associated with an (internal) instance ID
        within a specific instance domain (only valid with indoms
        of cache type - array indom will always return None).
        """
        entry = self._indoms[indom]
        if (entry.it_numinst < 0):
            value = (c_void_p)()
            sts = LIBPCP_PMDA.pmdaCacheLookup(indom, instance, None, byref(value))
            if (sts == cpmda.PMDA_CACHE_ACTIVE):
                return value
        return None

    def inst_name_lookup(self, indom, instance):
        """
        Lookup the name associated with an (internal) instance ID within
        a specific instance domain.
        """
        entry = self._indoms[indom]
        return entry.inst_name_lookup(instance)

class PMDA(MetricDispatch):
    """ Defines a PCP performance metrics domain agent
        Used to add new metrics into the PCP toolkit.
    """

    ##
    # property read methods

    def read_name(self):
        """ Property for name of this PMDA """
        return self._name

    def read_domain(self):
        """ Property for unique domain number of this PMDA """
        return self._domain

    ##
    # property definitions

    name = property(read_name, None, None, None)
    domain = property(read_domain, None, None, None)

    ##
    # overloads

    def __init__(self, name, domain):
        self._name = name
        self._domain = domain
        logfile = name + '.log'
        pmdaname = 'pmda' + name
        helpfile = '%s/%s/help' % (PCP.pmGetConfig('PCP_PMDAS_DIR'), name)
        MetricDispatch.__init__(self, domain, pmdaname, logfile, helpfile)


    ##
    # general PMDA class methods

    def domain_write(self):
        """
        Write out the domain.h file (used during installation)
        """
        print('#define %s %d' % (self._name.upper(), self._domain))

    def pmns_write(self, root):
        """
        Write out the namespace file (used during installation)
        """
        pmns = self._metric_names
        prefixes = set([pmns[key].split('.')[0] for key in pmns])
        indent = (root == 'root')
        lead = ''
        if indent:
            lead = '\t'
            print('root {')
        for prefix in prefixes:
            print('%s%s\t%d:*:*' % (lead, prefix, self._domain))
        if indent:
            print('}')

    def run(self):
        """
        All the real work happens herein; we can be called in one of three
        situations, determined by environment variables.  First couple are
        during the agent Install process, where the domain.h and namespace
        files need to be created.  The third case is the real mccoy, where
        an agent is actually being started by pmcd/dbpmda and makes use of
        libpcp_pmda to talk PCP protocol.
        """
        if ('PCP_PYTHON_DOMAIN' in os.environ):
            self.domain_write()
        elif ('PCP_PYTHON_PMNS' in os.environ):
            self.pmns_write(os.environ['PCP_PYTHON_PMNS'])
        else:
            self.pmns_refresh()
            cpmda.pmid_oneline_refresh(self._metric_oneline)
            cpmda.pmid_longtext_refresh(self._metric_helptext)
            cpmda.indom_oneline_refresh(self._indom_oneline)
            cpmda.indom_longtext_refresh(self._indom_helptext)
            cpmda.pmda_dispatch(self._indomtable, self._metrictable)

    @staticmethod
    def set_fetch(fetch):
        return cpmda.set_fetch(fetch)

    @staticmethod
    def set_refresh(refresh):
        return cpmda.set_refresh(refresh)

    @staticmethod
    def set_instance(instance):
        return cpmda.set_instance(instance)

    @staticmethod
    def set_fetch_callback(fetch_callback):
        return cpmda.set_fetch_callback(fetch_callback)

    @staticmethod
    def set_store_callback(store_callback):
        return cpmda.set_store_callback(store_callback)

    @staticmethod
    def set_refresh_metrics(refresh_metrics):
        return cpmda.set_refresh_metrics(refresh_metrics)

    @staticmethod
    def set_user(username):
        return cpmapi.pmSetProcessIdentity(username)

    @staticmethod
    def pmid(cluster, item):
        return cpmda.pmda_pmid(cluster, item)

    @staticmethod
    def indom(serial):
        return cpmda.pmda_indom(serial)

    @staticmethod
    def units(dim_space, dim_time, dim_count, scale_space, scale_time, scale_count):
        return cpmda.pmda_units(dim_space, dim_time, dim_count, scale_space, scale_time, scale_count)

    @staticmethod
    def uptime(now):
        return cpmda.pmda_uptime(now)

    @staticmethod
    def log(message):
        return cpmda.pmda_log(message)

    @staticmethod
    def err(message):
        return cpmda.pmda_err(message)

# Other methods perl API provides:
#    add_timer()
#    add_pipe()
#    add_tail()
#    add_sock()
#    put_sock()
#    set_inet_socket
#    set_ipv6_socket
#    set_unix_socket
#    pmda_pmid_name(cluster,item)
#    pmda_pmid_text(cluster,item)
#    
