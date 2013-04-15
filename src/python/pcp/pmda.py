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

import cpmapi
import cpmda
from pmapi import pmContext as PCP
from pmapi import pmID, pmInDom, pmUnits, pmResult

import ctypes
from ctypes import c_int, c_long, c_char_p, c_void_p
from ctypes import cast, CDLL, POINTER

## Performance Co-Pilot PMDA library (C)
LIBPCP_PMDA = ctypes.CDLL(ctypes.util.find_library("pcp_pmda"))


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
        table, metric namesspace, descriptors and help text.

        Overall strategy is to interface to the C code in
        python/pmda.c here, using a void* handle to the PMDA
        dispatch structure (allocated and managed in C code).

        In addition, several dictionaries for metric related
        strings are managed here (names, help text).
    """
    ##
    # overloads

    def __init__(self, domain, name, logfile, helpfile):
        self._metric_names = {}
        self._metric_oneline = {}
        self._metric_helptext = {}
        self._indom_oneline = {}
        self._indom_helptext = {}
        self._dispatch = cpmda.pmda_dispatch(domain, name, logfile, helpfile)


class PMDA(object):
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
        self._dispatch = MetricDispatch(domain, pmdaname, logfile, helpfile)


    ##
    # general PMDA class methods

    def add_metric(self, name, pmid, typed, sem, units, oneline = '', text = ''):
        return None

    def add_indom(self, indom, instlist, oneline = '', text = ''):
        return None

    def replace_indom(self, indom, instlist):
        return None

    def set_helpfile(self, path):
        return None

    def set_fetch(self, fetch):
        return None

    def set_instance(self, instance):
        return None

    def set_fetch_callback(self, fetch_callback):
        return None

    def set_store_callback(self, store_callback):
        return None

    def inst_lookup(self, indom, instance):
        return None

    def run(self):
        return None

    @staticmethod
    def set_user(username):
        return cpmapi.pmSetProcessIdentity(username)

    @staticmethod
    def pmid(cluster, item):
        return cpmda.pmda_pmid(cluster, item)

    @staticmethod
    def units(dim_space, dim_time, dim_count, scale_space, scale_time, scale_count):
        return cpmda.pmda_units(dim_space, dim_time, dim_count, scale_space, scale_time, scale_count)

#Needed data:
#    dispatch table
#    metrics table
#    indom table
#    pmns tree
#    metric names (dict)
#    metric oneline (dict)
#    metric helptext (dict)
#    indom helptext (dict)
#    indom oneline (dict)
#
#Needed methods:
#    __init__(name, domain)
#    set_helpdb(path)
#    clear_metrics()
#    add_indoms()
#    clear_indoms()
#    replace_indom()
#    add_timer()
#    add_pipe()
#    add_tail()
#    add_sock()
#    put_sock()
#
#    set_fetch		// separate class for dispatch?
#    set_refresh
#    set_instance
#    set_store_callback
#    set_fetch_callback
#    set_inet_socket
#    set_unix_socket
#
#Static methods:
#    pmda_pmid_name(cluster,item)
#    pmda_pmid_text(cluster,item)
#    pmda_inst_lookup(index,instance)
#    pmda_uptime(now)
#    log()
#    err()
#    
