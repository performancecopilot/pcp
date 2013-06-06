# pylint: disable=C0103
"""Wrapper module for libpcp_pmda - Performace Co-Pilot Domain Agent API
#
# Copyright (C) 2013 Red Hat.
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
from pmapi import pmContext as PCP
from pmapi import pmID, pmInDom, pmDesc, pmUnits, pmResult

import ctypes
from ctypes import c_int, c_long, c_char_p, c_void_p
from ctypes import cast, CDLL, POINTER, Structure

## Performance Co-Pilot PMDA library (C)
LIBPCP_PMDA = ctypes.CDLL(ctypes.util.find_library("pcp_pmda"))


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
        self.i_name = name

    def __str__(self):
        return "pmdaInstid@%#lx index=%d name=%s" % (addressof(self), self.i_inst, self.i_name)

class pmdaIndom(Structure):
    """ Structure describing an instance domain within a PMDA """
    _fields_ = [("it_indom", pmInDom),
                ("it_numinst", c_int),
                ("it_set", POINTER(pmdaInstid))]

    def __init__(self, indom, insts):
        Structure.__init__(self)
        self.it_indom = indom
        self.set_instances(insts)

    def set_instances(self, insts):
        if (insts == None):
            self.it_numinst = 0
            return
        instance_count = len(insts)
        self.it_numinst = instance_count
        if (instance_count == 0):
            return
        instance_array = (pmdaInstid * instance_count)()
        for i in xrange(instance_count):
            instance_array[i].i_inst = insts[i].i_inst
            instance_array[i].i_name = insts[i].i_name
        self.it_set = instance_array

    def __str__(self):
        return "pmdaIndom@%#lx indom=%#lx num=%d" % (addressof(self), self.it_indom, self.it_numinst)

###
## PMDA Indom Cache Services
#
LIBPCP_PMDA.pmdaCacheStore.restype = c_int
LIBPCP_PMDA.pmdaCacheStore.argtypes = [pmInDom, c_int, c_char_p, c_void_p]
LIBPCP_PMDA.pmdaCacheStoreKey.restype = c_int
LIBPCP_PMDA.pmdaCacheStoreKey.argtypes = [
        pmInDom, c_int, c_char_p, c_int, c_void_p]
LIBPCP_PMDA.pmdaCacheLookup.restype = c_int
LIBPCP_PMDA.pmdaCacheLookup.argtypes = [pmInDom, c_int, c_char_p, c_void_p]
LIBPCP_PMDA.pmdaCacheLookupName.restype = c_int
LIBPCP_PMDA.pmdaCacheLookupName.argtypes = [
        pmInDom, c_char_p, POINTER(c_int), POINTER(c_void_p)]
LIBPCP_PMDA.pmdaCacheLookupKey.restype = c_int
LIBPCP_PMDA.pmdaCacheLookupKey.argtypes = [
        pmInDom, c_char_p, c_int, c_void_p, POINTER(c_char_p),
       POINTER(c_int), POINTER(c_void_p)]
LIBPCP_PMDA.pmdaCacheOp.restype = c_int
LIBPCP_PMDA.pmdaCacheOp.argtypes = [pmInDom, c_long]


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
        self.clear_indoms()
        self.clear_metrics()
        self._dispatch = cpmda.pmda_dispatch(domain, name, logfile, helpfile)

    def clear_indoms(self):
        self._indomtable = []
        self._indoms = {}
        self._indom_oneline = {}
        self._indom_helptext = {}

    def clear_metrics(self):
        self._metrictable = []
        self._metrics = {}
        self._metric_names = {}
        self._metric_oneline = {}
        self._metric_helptext = {}

    def reset_metrics(self):
        clear_metrics()
        cpmda.set_need_refresh(self._metrics)

    ##
    # general PMDA class methods

    def pmns_refresh(self):
        if (cpmda.need_refresh()):
            cpmda.pmns_refresh(self._metrics)

    def add_metric(self, name, metric, oneline = '', text = ''):
        pmid = metric.m_desc.pmid
        if (pmid in self._metric_names):
            raise KeyError, 'attempt to add_metric with an existing name'
        if (pmid in self._metrics):
            raise KeyError, 'attempt to add_metric with an existing pmid'

        self._metrictable.append(metric)
        self._metrics[pmid] = metric
        self._metric_names[pmid] = name
        self._metric_oneline[pmid] = oneline
        self._metric_helptext[pmid] = text

        cpmda.set_need_refresh(self._metrics)

    def add_indom(self, indom, oneline = '', text = ''):
        indomid = indom.it_indom
        if (indomid in self._indoms):
            raise KeyError, 'attempt to add_indom with an existing indom'
        self._indomtable.append(indom)
        self._indoms[indomid] = indom
        self._indom_oneline[indomid] = oneline
        self._indom_helptext[indomid] = text

    def replace_indom(self, indom, instlist):
        replacement = pmdaIndom(indom, instlist)
        for entry in self._indomtable:
            if (entry.it_indom == indom):
                entry = replacement
        self._indoms[indomid] = replacement

    def inst_lookup(self, indom, instance):
        """
        Lookup the (external) name associated with an (internal) instance ID
        within a specific instance domain
        """
        entry = self._indoms[indom]
        if (entry.it_indom == indom):
            for inst in entry.it_set:
                if (inst.i_inst == instance):
                    return inst.i_name
        return None


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
        print '#define %s %d' % (self._name.upper(), self._domain)

    def pmns_write(self, root):
        """
        Write out the namespace file (used during installation)
        """
        namespace = self._metric_names
        prefixes = set([namespace[key].split('.')[0] for key in namespace])
        indent = (root == 'root')
        lead = ''
        if indent:
            lead = '\t'
            print 'root {'
        for prefix in prefixes:
            print '%s%s\t%d:*:*' % (lead, prefix, self._domain)
        if indent:
            print '}'

    def run(self):
        if ('PCP_PYTHON_DOMAIN' in os.environ):
            self.domain_write()
        elif ('PCP_PYTHON_PMNS' in os.environ):
            self.pmns_refresh()
            self.pmns_write(os.environ['PCP_PYTHON_PMNS'])
        else:
            self.pmns_refresh()
            LIBPCP_PMDA.pmdaInit(self._dispatch)
            LIBPCP_PMDA.pmdaConnect(self._dispatch)
            LIBPCP_PMDA.pmdaMain(self._dispatch)

    @staticmethod
    def set_fetch(self, fetch):
        return cpmda.set_fetch(fetch)

    @staticmethod
    def set_refresh(self, refresh):
        return cpmda.set_refresh(refresh)

    @staticmethod
    def set_instance(self, instance):
        return cpmda.set_instance(instance)

    @staticmethod
    def set_fetch_callback(self, fetch_callback):
        return cpmda.set_fetch_callback(fetch_callback)

    @staticmethod
    def set_store_callback(self, store_callback):
        return cpmda.set_store_callback(store_callback)

    @staticmethod
    def set_user(username):
        return cpmapi.pmSetProcessIdentity(username)

    @staticmethod
    def pmid(cluster, item):
        return cpmda.pmda_pmid(cluster, item)

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
#    pmda_inst_lookup(index,instance)
#    
